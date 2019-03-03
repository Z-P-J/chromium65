// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_debug_recording_helper.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_message_loop.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_sample_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

#if defined(OS_WIN)
#define IntToStringType base::IntToString16
#else
#define IntToStringType base::IntToString
#endif

namespace media {

namespace {

// The filename extension the mock should return in GetFileNameExtension().
const base::FilePath::CharType kFileNameExtension[] = FILE_PATH_LITERAL("wav");

}  // namespace

// Mock class for the audio file writer that the helper wraps.
class MockAudioDebugFileWriter : public AudioDebugFileWriter {
 public:
  MockAudioDebugFileWriter(const AudioParameters& params)
      : AudioDebugFileWriter(params), reference_data_(nullptr) {}
  ~MockAudioDebugFileWriter() override = default;

  MOCK_METHOD1(DoStart, void(bool));
  void Start(base::File file) { DoStart(file.IsValid()); }
  MOCK_METHOD0(Stop, void());

  // Functions with move-only types as arguments can't be mocked directly, so
  // we pass on to DoWrite(). Also, we can verify the data this way.
  MOCK_METHOD1(DoWrite, void(AudioBus*));
  void Write(std::unique_ptr<AudioBus> data) override {
    CHECK(reference_data_);
    EXPECT_EQ(reference_data_->channels(), data->channels());
    EXPECT_EQ(reference_data_->frames(), data->frames());
    for (int i = 0; i < data->channels(); ++i) {
      float* data_ptr = data->channel(i);
      float* ref_data_ptr = reference_data_->channel(i);
      for (int j = 0; j < data->frames(); ++j, ++data_ptr, ++ref_data_ptr)
        EXPECT_EQ(*ref_data_ptr, *data_ptr);
    }
    DoWrite(data.get());
  }

  MOCK_METHOD0(WillWrite, bool());
  MOCK_METHOD0(GetFileNameExtension, const base::FilePath::CharType*());

  // Set reference data to compare against. Must be called before Write() is
  // called.
  void SetReferenceData(AudioBus* reference_data) {
    EXPECT_EQ(params_.channels(), reference_data->channels());
    EXPECT_EQ(params_.frames_per_buffer(), reference_data->frames());
    reference_data_ = reference_data;
  }

 private:
  AudioBus* reference_data_;

  DISALLOW_COPY_AND_ASSIGN(MockAudioDebugFileWriter);
};

// Sub-class of the helper that overrides the CreateAudioDebugFileWriter
// function to create the above mock instead.
class AudioDebugRecordingHelperUnderTest : public AudioDebugRecordingHelper {
 public:
  AudioDebugRecordingHelperUnderTest(
      const AudioParameters& params,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      AudioDebugRecordingHelper::CreateFileCallback create_file_callback,
      base::OnceClosure on_destruction_closure)
      : AudioDebugRecordingHelper(params,
                                  std::move(task_runner),
                                  std::move(create_file_callback),
                                  std::move(on_destruction_closure)) {}
  ~AudioDebugRecordingHelperUnderTest() override = default;

 private:
  // Creates the mock writer. After the mock writer is returned, we always
  // expect GetFileNameExtension() and Start() to be called on it by the helper.
  std::unique_ptr<AudioDebugFileWriter> CreateAudioDebugFileWriter(
      const AudioParameters& params) override {
    MockAudioDebugFileWriter* writer = new MockAudioDebugFileWriter(params);
    EXPECT_CALL(*writer, GetFileNameExtension())
        .WillOnce(Return(kFileNameExtension));
    EXPECT_CALL(*writer, DoStart(true));
    return base::WrapUnique<AudioDebugFileWriter>(writer);
  }

  DISALLOW_COPY_AND_ASSIGN(AudioDebugRecordingHelperUnderTest);
};

// The test fixture.
class AudioDebugRecordingHelperTest : public ::testing::Test {
 public:
  AudioDebugRecordingHelperTest() {}

  ~AudioDebugRecordingHelperTest() override = default;

  // Helper function that creates a recording helper.
  std::unique_ptr<AudioDebugRecordingHelper> CreateRecordingHelper(
      const AudioParameters& params,
      base::OnceClosure on_destruction_closure) {
    return std::make_unique<AudioDebugRecordingHelperUnderTest>(
        params, scoped_task_environment_.GetMainThreadTaskRunner(),
        base::BindRepeating(&AudioDebugRecordingHelperTest::CreateFile,
                            base::Unretained(this)),
        std::move(on_destruction_closure));
  }

  // Helper function that unsets the mock writer pointer after disabling.
  void DisableDebugRecording(AudioDebugRecordingHelper* recording_helper,
                             const base::FilePath& file_path) {
    recording_helper->DisableDebugRecording();
    EXPECT_TRUE(
        base::DeleteFile(file_path.AddExtension(kFileNameExtension), false));
  }

  MOCK_METHOD0(OnAudioDebugRecordingHelperDestruction, void());

  // Callback function for creating a file, passed in constructor and run on
  // enable. Creating actual files to be able to test writer->DoWrite mock.
  MOCK_METHOD1(DoCreateFile, void(const base::FilePath& file_name));
  void CreateFile(const base::FilePath& file_name,
                  base::OnceCallback<void(base::File)> reply_callback) {
    base::File debug_file(base::File(
        file_name, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE));
    std::move(reply_callback).Run(std::move(debug_file));
    DoCreateFile(file_name);
  }

 protected:
  // The test task environment.
  base::test::ScopedTaskEnvironment scoped_task_environment_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioDebugRecordingHelperTest);
};

// Creates a helper with an on destruction closure, and verifies that it's
// run.
TEST_F(AudioDebugRecordingHelperTest, TestDestructionClosure) {
  const AudioParameters params;
  std::unique_ptr<AudioDebugRecordingHelper> recording_helper =
      CreateRecordingHelper(
          params, base::BindOnce(&AudioDebugRecordingHelperTest::
                                     OnAudioDebugRecordingHelperDestruction,
                                 base::Unretained(this)));

  EXPECT_CALL(*this, OnAudioDebugRecordingHelperDestruction());
}

// Verifies that disable can be called without being enabled.
TEST_F(AudioDebugRecordingHelperTest, OnlyDisable) {
  const AudioParameters params;
  std::unique_ptr<AudioDebugRecordingHelper> recording_helper =
      CreateRecordingHelper(params, base::OnceClosure());

  recording_helper->DisableDebugRecording();
}

TEST_F(AudioDebugRecordingHelperTest, EnableDisable) {
  const AudioParameters params;
  std::unique_ptr<AudioDebugRecordingHelper> recording_helper =
      CreateRecordingHelper(params, base::OnceClosure());

  base::FilePath file_path;
  EXPECT_TRUE(base::CreateTemporaryFile(&file_path));
  EXPECT_CALL(*this, DoCreateFile(file_path.AddExtension(kFileNameExtension)));
  recording_helper->EnableDebugRecording(file_path);
  EXPECT_CALL(*static_cast<MockAudioDebugFileWriter*>(
                  recording_helper->debug_writer_.get()),
              Stop());
  DisableDebugRecording(recording_helper.get(), file_path);

  EXPECT_TRUE(base::CreateTemporaryFile(&file_path));
  EXPECT_CALL(*this, DoCreateFile(file_path.AddExtension(kFileNameExtension)));
  recording_helper->EnableDebugRecording(file_path);
  EXPECT_CALL(*static_cast<MockAudioDebugFileWriter*>(
                  recording_helper->debug_writer_.get()),
              Stop());
  DisableDebugRecording(recording_helper.get(), file_path);
}

TEST_F(AudioDebugRecordingHelperTest, OnData) {
  // Only channel layout and frames per buffer is used in the file writer and
  // AudioBus, the other parameters are ignored.
  const int number_of_frames = 100;
  const AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                               ChannelLayout::CHANNEL_LAYOUT_STEREO, 0, 0,
                               number_of_frames);

  // Setup some data.
  const int number_of_samples = number_of_frames * params.channels();
  const float step = std::numeric_limits<int16_t>::max() / number_of_frames;
  std::unique_ptr<float[]> source_data(new float[number_of_samples]);
  for (float i = 0; i < number_of_samples; ++i)
    source_data[i] = i * step;
  std::unique_ptr<AudioBus> audio_bus = AudioBus::Create(params);
  audio_bus->FromInterleaved<Float32SampleTypeTraits>(source_data.get(),
                                                      number_of_frames);

  std::unique_ptr<AudioDebugRecordingHelper> recording_helper =
      CreateRecordingHelper(params, base::OnceClosure());

  // Should not do anything.
  recording_helper->OnData(audio_bus.get());

  base::FilePath file_path;
  EXPECT_TRUE(base::CreateTemporaryFile(&file_path));
  EXPECT_CALL(*this, DoCreateFile(file_path.AddExtension(kFileNameExtension)));
  recording_helper->EnableDebugRecording(file_path);
  MockAudioDebugFileWriter* mock_audio_file_writer =
      static_cast<MockAudioDebugFileWriter*>(
          recording_helper->debug_writer_.get());
  mock_audio_file_writer->SetReferenceData(audio_bus.get());

  EXPECT_CALL(*mock_audio_file_writer, DoWrite(_));
  recording_helper->OnData(audio_bus.get());
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*mock_audio_file_writer, Stop());
  DisableDebugRecording(recording_helper.get(), file_path);

  // Make sure we clear the loop before enabling again.
  base::RunLoop().RunUntilIdle();

  // Enable again, this time with two OnData() calls, one OnData() call
  // without running the message loop until after disabling, and one call after
  // disabling.
  EXPECT_TRUE(base::CreateTemporaryFile(&file_path));
  EXPECT_CALL(*this, DoCreateFile(file_path.AddExtension(kFileNameExtension)));
  recording_helper->EnableDebugRecording(file_path);
  mock_audio_file_writer = static_cast<MockAudioDebugFileWriter*>(
      recording_helper->debug_writer_.get());
  mock_audio_file_writer->SetReferenceData(audio_bus.get());

  EXPECT_CALL(*mock_audio_file_writer, DoWrite(_)).Times(2);
  recording_helper->OnData(audio_bus.get());
  recording_helper->OnData(audio_bus.get());
  base::RunLoop().RunUntilIdle();

  // This call should not yield a DoWrite() call on the mock, since the message
  // loop isn't run until after disabling. WillWrite() is expected since
  // recording is enabled.
  recording_helper->OnData(audio_bus.get());

  EXPECT_CALL(*mock_audio_file_writer, Stop());
  DisableDebugRecording(recording_helper.get(), file_path);

  // This call should not yield a DoWrite() call on the mock either.
  recording_helper->OnData(audio_bus.get());
  base::RunLoop().RunUntilIdle();
}

}  // namespace media
