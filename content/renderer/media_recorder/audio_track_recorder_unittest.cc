// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media_recorder/audio_track_recorder.h"

#include <stdint.h>

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "content/renderer/media/media_stream_audio_source.h"
#include "media/audio/simple_sources.h"
#include "media/base/audio_sample_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebHeap.h"
#include "third_party/opus/src/include/opus.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::TestWithParam;
using ::testing::ValuesIn;
using base::TimeTicks;

namespace {

const int kDefaultBitsPerSample = 16;
const int kDefaultSampleRate = 48000;
// The |frames_per_buffer| field of AudioParameters is not used by ATR.
const int kIgnoreFramesPerBuffer = 1;

// The following parameters replicate those in audio_track_recorder.cc, see this
// file for explanations.
const int kMediaStreamAudioTrackBufferDurationMs = 10;
const int kOpusBufferDurationMs = 60;
const int kRatioInputToOutputFrames =
    kOpusBufferDurationMs / kMediaStreamAudioTrackBufferDurationMs;

const int kFramesPerBuffer = kOpusBufferDurationMs * kDefaultSampleRate / 1000;

}  // namespace

namespace content {

ACTION_P(RunClosure, closure) {
  closure.Run();
}

struct ATRTestParams {
  const media::AudioParameters::Format input_format;
  const media::ChannelLayout channel_layout;
  const int sample_rate;
  const int bits_per_sample;
  const AudioTrackRecorder::CodecId codec;
};

const ATRTestParams kATRTestParams[] = {
    // Equivalent to default settings:
    {media::AudioParameters::AUDIO_PCM_LOW_LATENCY, /* input format */
     media::CHANNEL_LAYOUT_STEREO,                  /* channel layout */
     kDefaultSampleRate,                            /* sample rate */
     kDefaultBitsPerSample,                         /* bits per sample */
     AudioTrackRecorder::CodecId::OPUS},            /* codec for encoding */
    // Change to mono:
    {media::AudioParameters::AUDIO_PCM_LOW_LATENCY, media::CHANNEL_LAYOUT_MONO,
     kDefaultSampleRate, kDefaultBitsPerSample,
     AudioTrackRecorder::CodecId::OPUS},
    // Different sampling rate as well:
    {media::AudioParameters::AUDIO_PCM_LOW_LATENCY, media::CHANNEL_LAYOUT_MONO,
     24000, kDefaultBitsPerSample, AudioTrackRecorder::CodecId::OPUS},
    {media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
     media::CHANNEL_LAYOUT_STEREO, 8000, kDefaultBitsPerSample,
     AudioTrackRecorder::CodecId::OPUS},
    // Using a non-default Opus sampling rate (48, 24, 16, 12, or 8 kHz).
    {media::AudioParameters::AUDIO_PCM_LOW_LATENCY, media::CHANNEL_LAYOUT_MONO,
     22050, kDefaultBitsPerSample, AudioTrackRecorder::CodecId::OPUS},
    {media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
     media::CHANNEL_LAYOUT_STEREO, 44100, kDefaultBitsPerSample,
     AudioTrackRecorder::CodecId::OPUS},
    {media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
     media::CHANNEL_LAYOUT_STEREO, 96000, kDefaultBitsPerSample,
     AudioTrackRecorder::CodecId::OPUS},
    {media::AudioParameters::AUDIO_PCM_LOW_LATENCY, media::CHANNEL_LAYOUT_MONO,
     kDefaultSampleRate, kDefaultBitsPerSample,
     AudioTrackRecorder::CodecId::PCM},
    {media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
     media::CHANNEL_LAYOUT_STEREO, kDefaultSampleRate, kDefaultBitsPerSample,
     AudioTrackRecorder::CodecId::PCM},
};

class AudioTrackRecorderTest : public TestWithParam<ATRTestParams> {
 public:
  // Initialize |first_params_| based on test parameters, and |second_params_|
  // to always be the same thing.
  AudioTrackRecorderTest()
      : codec_(GetParam().codec),
        first_params_(GetParam().input_format,
                      GetParam().channel_layout,
                      GetParam().sample_rate,
                      GetParam().bits_per_sample,
                      kIgnoreFramesPerBuffer),
        second_params_(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                       media::CHANNEL_LAYOUT_STEREO,
                       kDefaultSampleRate,
                       kDefaultBitsPerSample,
                       kIgnoreFramesPerBuffer),
        first_source_(first_params_.channels(),     /* # channels */
                      440,                          /* frequency */
                      first_params_.sample_rate()), /* sample rate */
        second_source_(second_params_.channels(),
                       440,
                       second_params_.sample_rate()),
        opus_decoder_(nullptr),
        first_source_cache_pos_(0) {
    ResetDecoder(first_params_);
    PrepareBlinkTrack();
    audio_track_recorder_.reset(new AudioTrackRecorder(
        codec_, blink_track_,
        base::Bind(&AudioTrackRecorderTest::OnEncodedAudio,
                   base::Unretained(this)),
        0 /* bits_per_second */));
  }

  ~AudioTrackRecorderTest() {
    opus_decoder_destroy(opus_decoder_);
    opus_decoder_ = nullptr;
    blink_track_.Reset();
    blink::WebHeap::CollectAllGarbageForTesting();
    audio_track_recorder_.reset();
    // Let the message loop run to finish destroying the recorder properly.
    base::RunLoop().RunUntilIdle();
  }

  void ResetDecoder(const media::AudioParameters& params) {
    if (opus_decoder_) {
      opus_decoder_destroy(opus_decoder_);
      opus_decoder_ = nullptr;
    }

    int error;
    opus_decoder_ =
        opus_decoder_create(kDefaultSampleRate, params.channels(), &error);
    EXPECT_TRUE(error == OPUS_OK && opus_decoder_);

    opus_buffer_.reset(new float[kFramesPerBuffer * params.channels()]);
  }

  std::unique_ptr<media::AudioBus> GetFirstSourceAudioBus() {
    std::unique_ptr<media::AudioBus> bus(media::AudioBus::Create(
        first_params_.channels(), first_params_.sample_rate() *
                                      kMediaStreamAudioTrackBufferDurationMs /
                                      base::Time::kMillisecondsPerSecond));
    first_source_.OnMoreData(base::TimeDelta(), base::TimeTicks::Now(), 0,
                             bus.get());

    // Save the samples that we read into the first_source_cache_.
    std::unique_ptr<media::AudioBus> cache_bus(
        media::AudioBus::Create(bus->channels(), bus->frames()));
    bus->CopyTo(cache_bus.get());

    int current_size = first_source_cache_.size();
    first_source_cache_.resize(current_size +
                               cache_bus->frames() * cache_bus->channels());
    cache_bus->ToInterleaved<media::Float32SampleTypeTraits>(
        cache_bus->frames(), &first_source_cache_[current_size]);

    return bus;
  }
  std::unique_ptr<media::AudioBus> GetSecondSourceAudioBus() {
    std::unique_ptr<media::AudioBus> bus(media::AudioBus::Create(
        second_params_.channels(), second_params_.sample_rate() *
                                       kMediaStreamAudioTrackBufferDurationMs /
                                       base::Time::kMillisecondsPerSecond));
    second_source_.OnMoreData(base::TimeDelta(), base::TimeTicks::Now(), 0,
                              bus.get());
    return bus;
  }

  MOCK_METHOD3(DoOnEncodedAudio,
               void(const media::AudioParameters& params,
                    std::string encoded_data,
                    TimeTicks timestamp));

  void OnEncodedAudio(const media::AudioParameters& params,
                      std::unique_ptr<std::string> encoded_data,
                      TimeTicks timestamp) {
    EXPECT_TRUE(!encoded_data->empty());

    if (codec_ == AudioTrackRecorder::CodecId::OPUS) {
      // Decode |encoded_data| and check we get the expected number of frames
      // per buffer.
      EXPECT_EQ(
          kDefaultSampleRate * kOpusBufferDurationMs / 1000,
          opus_decode_float(opus_decoder_,
                            reinterpret_cast<uint8_t*>(
                                base::string_as_array(encoded_data.get())),
                            encoded_data->size(), opus_buffer_.get(),
                            kFramesPerBuffer, 0));
    } else if (codec_ == AudioTrackRecorder::CodecId::PCM) {
      // Manually confirm that we're getting the same data out as what we
      // generated from the sine wave.
      for (size_t b = 0; b + 3 < encoded_data->size() &&
                         first_source_cache_pos_ < first_source_cache_.size();
           b += sizeof(first_source_cache_[0]), ++first_source_cache_pos_) {
        float sample;
        memcpy(&sample, &(*encoded_data)[b], 4);
        ASSERT_FLOAT_EQ(sample, first_source_cache_[first_source_cache_pos_])
            << "(Sample " << first_source_cache_pos_ << ")";
      }
    }

    DoOnEncodedAudio(params, *encoded_data, timestamp);
  }

  const base::test::ScopedTaskEnvironment scoped_task_environment_;

  // ATR and WebMediaStreamTrack for fooling it.
  std::unique_ptr<AudioTrackRecorder> audio_track_recorder_;
  blink::WebMediaStreamTrack blink_track_;

  // The codec we'll use for compression the audio.
  const AudioTrackRecorder::CodecId codec_;

  // Two different sets of AudioParameters for testing re-init of ATR.
  const media::AudioParameters first_params_;
  const media::AudioParameters second_params_;

  // AudioSources for creating AudioBuses.
  media::SineWaveAudioSource first_source_;
  media::SineWaveAudioSource second_source_;

  // Decoder for verifying data was properly encoded.
  OpusDecoder* opus_decoder_;
  std::unique_ptr<float[]> opus_buffer_;

  // Save the data we generate from the first source so that we might compare it
  // later if we happen to be using the PCM encoder.
  std::vector<float> first_source_cache_;
  size_t first_source_cache_pos_;

 private:
  // Prepares a blink track of a given MediaStreamType and attaches the native
  // track, which can be used to capture audio data and pass it to the producer.
  // Adapted from media::WebRTCLocalAudioSourceProviderTest.
  void PrepareBlinkTrack() {
    blink::WebMediaStreamSource audio_source;
    audio_source.Initialize(blink::WebString::FromUTF8("dummy_source_id"),
                            blink::WebMediaStreamSource::kTypeAudio,
                            blink::WebString::FromUTF8("dummy_source_name"),
                            false /* remote */);
    audio_source.SetExtraData(new MediaStreamAudioSource(true));
    blink_track_.Initialize(blink::WebString::FromUTF8("audio_track"),
                            audio_source);
    CHECK(MediaStreamAudioSource::From(audio_source)
              ->ConnectToTrack(blink_track_));
  }

  DISALLOW_COPY_AND_ASSIGN(AudioTrackRecorderTest);
};

TEST_P(AudioTrackRecorderTest, OnDataOpus) {
  if (codec_ != AudioTrackRecorder::CodecId::OPUS)
    return;

  InSequence s;
  base::RunLoop run_loop;
  base::Closure quit_closure = run_loop.QuitClosure();

  // Give ATR initial audio parameters.
  audio_track_recorder_->OnSetFormat(first_params_);

  // TODO(ajose): consider adding WillOnce(SaveArg...) and inspecting, as done
  // in VTR unittests. http://crbug.com/548856
  EXPECT_CALL(*this, DoOnEncodedAudio(_, _, _)).Times(1);
  audio_track_recorder_->OnData(*GetFirstSourceAudioBus(), TimeTicks::Now());
  for (int i = 0; i < kRatioInputToOutputFrames - 1; ++i)
    audio_track_recorder_->OnData(*GetFirstSourceAudioBus(), TimeTicks::Now());

  EXPECT_CALL(*this, DoOnEncodedAudio(_, _, _))
      .Times(1)
      // Only reset the decoder once we've heard back:
      .WillOnce(RunClosure(base::Bind(&AudioTrackRecorderTest::ResetDecoder,
                                      base::Unretained(this), second_params_)));
  audio_track_recorder_->OnData(*GetFirstSourceAudioBus(), TimeTicks::Now());
  for (int i = 0; i < kRatioInputToOutputFrames - 1; ++i)
    audio_track_recorder_->OnData(*GetFirstSourceAudioBus(), TimeTicks::Now());

  // If the amount of samples/10ms buffer is not an integer (e.g. 22050Hz) we
  // need an extra OnData() to account for the round-off error.
  if (GetParam().sample_rate % 100)
    audio_track_recorder_->OnData(*GetFirstSourceAudioBus(), TimeTicks::Now());

  // Give ATR new audio parameters.
  audio_track_recorder_->OnSetFormat(second_params_);

  // Send audio with different params.
  EXPECT_CALL(*this, DoOnEncodedAudio(_, _, _))
      .Times(1)
      .WillOnce(RunClosure(quit_closure));
  audio_track_recorder_->OnData(*GetSecondSourceAudioBus(), TimeTicks::Now());
  for (int i = 0; i < kRatioInputToOutputFrames - 1; ++i)
    audio_track_recorder_->OnData(*GetSecondSourceAudioBus(), TimeTicks::Now());

  run_loop.Run();
  Mock::VerifyAndClearExpectations(this);
}

TEST_P(AudioTrackRecorderTest, OnDataPcm) {
  if (codec_ != AudioTrackRecorder::CodecId::PCM)
    return;

  InSequence s;
  base::RunLoop run_loop;
  base::Closure quit_closure = run_loop.QuitClosure();

  audio_track_recorder_->OnSetFormat(first_params_);

  EXPECT_CALL(*this, DoOnEncodedAudio(_, _, _)).Times(5);
  EXPECT_CALL(*this, DoOnEncodedAudio(_, _, _))
      .WillOnce(RunClosure(quit_closure));

  audio_track_recorder_->OnData(*GetFirstSourceAudioBus(), TimeTicks::Now());
  for (int i = 0; i < kRatioInputToOutputFrames - 1; ++i)
    audio_track_recorder_->OnData(*GetFirstSourceAudioBus(), TimeTicks::Now());

  run_loop.Run();
  Mock::VerifyAndClearExpectations(this);
}

TEST_P(AudioTrackRecorderTest, PauseResume) {
  if (codec_ != AudioTrackRecorder::CodecId::OPUS)
    return;

  InSequence s;
  base::RunLoop run_loop;
  base::Closure quit_closure = run_loop.QuitClosure();

  // Give ATR initial audio parameters.
  audio_track_recorder_->OnSetFormat(first_params_);

  audio_track_recorder_->Pause();
  EXPECT_CALL(*this, DoOnEncodedAudio(_, _, _)).Times(0);
  audio_track_recorder_->OnData(*GetFirstSourceAudioBus(), TimeTicks::Now());
  for (int i = 0; i < kRatioInputToOutputFrames - 1; ++i)
    audio_track_recorder_->OnData(*GetFirstSourceAudioBus(), TimeTicks::Now());

  audio_track_recorder_->Resume();
  EXPECT_CALL(*this, DoOnEncodedAudio(_, _, _))
      .Times(1)
      .WillOnce(RunClosure(quit_closure));
  audio_track_recorder_->OnData(*GetFirstSourceAudioBus(), TimeTicks::Now());
  for (int i = 0; i < kRatioInputToOutputFrames - 1; ++i)
    audio_track_recorder_->OnData(*GetFirstSourceAudioBus(), TimeTicks::Now());

  if (GetParam().sample_rate % 100)
    audio_track_recorder_->OnData(*GetFirstSourceAudioBus(), TimeTicks::Now());

  run_loop.Run();
  Mock::VerifyAndClearExpectations(this);
}

INSTANTIATE_TEST_CASE_P(, AudioTrackRecorderTest, ValuesIn(kATRTestParams));
}  // namespace content
