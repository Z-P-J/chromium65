// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/audio_pump.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "remoting/codec/audio_encoder.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/protocol/audio_source.h"
#include "remoting/protocol/audio_stub.h"
#include "remoting/protocol/fake_audio_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace protocol {

namespace {

// Creates a dummy packet with 1k data
std::unique_ptr<AudioPacket> MakeAudioPacket(int channel_count = 2) {
  std::unique_ptr<AudioPacket> packet(new AudioPacket);
  packet->add_data()->resize(1024);
  packet->set_encoding(AudioPacket::ENCODING_RAW);
  packet->set_sampling_rate(AudioPacket::SAMPLING_RATE_44100);
  packet->set_bytes_per_sample(AudioPacket::BYTES_PER_SAMPLE_2);
  packet->set_channels(static_cast<AudioPacket::Channels>(channel_count));
  return packet;
}

}  // namespace

class FakeAudioEncoder : public AudioEncoder {
 public:
  FakeAudioEncoder() = default;
  ~FakeAudioEncoder() override = default;

  std::unique_ptr<AudioPacket> Encode(
      std::unique_ptr<AudioPacket> packet) override {
    EXPECT_TRUE(!!packet);
    EXPECT_EQ(packet->encoding(), AudioPacket::ENCODING_RAW);
    EXPECT_EQ(packet->sampling_rate(), AudioPacket::SAMPLING_RATE_44100);
    EXPECT_EQ(packet->bytes_per_sample(), AudioPacket::BYTES_PER_SAMPLE_2);
    EXPECT_LE(packet->channels(), AudioPacket::CHANNELS_STEREO);
    return packet;
  }
  int GetBitrate() override { return 160000; }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeAudioEncoder);
};

class AudioPumpTest : public testing::Test, public protocol::AudioStub {
 public:
  AudioPumpTest() = default;

  void SetUp() override;
  void TearDown() override;

  // protocol::AudioStub interface.
  void ProcessAudioPacket(std::unique_ptr<AudioPacket> audio_packet,
                          const base::Closure& done) override;

 protected:
  base::MessageLoop message_loop_;

  // |source_| and |encoder_| are owned by the |pump_|.
  FakeAudioSource* source_;
  FakeAudioEncoder* encoder_;

  std::unique_ptr<AudioPump> pump_;

  std::vector<std::unique_ptr<AudioPacket>> sent_packets_;
  std::vector<base::Closure> done_closures_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioPumpTest);
};

void AudioPumpTest::SetUp() {
  source_ = new FakeAudioSource();
  encoder_ = new FakeAudioEncoder();
  pump_.reset(new AudioPump(message_loop_.task_runner(),
                            base::WrapUnique(source_),
                            base::WrapUnique(encoder_), this));
}

void AudioPumpTest::TearDown() {
  pump_.reset();

  // Let the message loop run to finish destroying the capturer.
  base::RunLoop().RunUntilIdle();
}

void AudioPumpTest::ProcessAudioPacket(
    std::unique_ptr<AudioPacket> audio_packet,
    const base::Closure& done) {
  sent_packets_.push_back(std::move(audio_packet));
  done_closures_.push_back(done);
}

// Verify that the pump pauses pumping when the network is congested.
TEST_F(AudioPumpTest, BufferSizeLimit) {
  // Run message loop to let the pump start the capturer.
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(source_->callback().is_null());

  // Try sending 100 packets, 1k each. The pump should stop pumping and start
  // dropping the data at some point.
  for (size_t i = 0; i < 100; ++i) {
    source_->callback().Run(MakeAudioPacket());
    base::RunLoop().RunUntilIdle();
  }

  size_t num_sent_packets = sent_packets_.size();
  EXPECT_LT(num_sent_packets, 100U);
  EXPECT_GT(num_sent_packets, 0U);

  // Call done closure for the first packet. This should allow one more packet
  // to be sent below.
  done_closures_.front().Run();
  base::RunLoop().RunUntilIdle();

  // Verify that the pump continues to send captured audio.
  source_->callback().Run(MakeAudioPacket());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(num_sent_packets + 1, sent_packets_.size());
}

TEST_F(AudioPumpTest, DownmixAudioPacket) {
  // Run message loop to let the pump start the capturer.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(source_->callback());

  // Generate several audio packets with different channel counts.
  static const int kChannels[] = {
    AudioPacket::CHANNELS_7_1,
    AudioPacket::CHANNELS_6_1,
    AudioPacket::CHANNELS_5_1,
    AudioPacket::CHANNELS_STEREO,
    AudioPacket::CHANNELS_MONO,
    AudioPacket::CHANNELS_7_1,
    AudioPacket::CHANNELS_7_1,
    AudioPacket::CHANNELS_7_1,
    AudioPacket::CHANNELS_7_1,
    AudioPacket::CHANNELS_6_1,
    AudioPacket::CHANNELS_6_1,
    AudioPacket::CHANNELS_6_1,
    AudioPacket::CHANNELS_6_1,
    AudioPacket::CHANNELS_5_1,
    AudioPacket::CHANNELS_5_1,
    AudioPacket::CHANNELS_5_1,
    AudioPacket::CHANNELS_5_1,
    AudioPacket::CHANNELS_STEREO,
    AudioPacket::CHANNELS_STEREO,
    AudioPacket::CHANNELS_STEREO,
    AudioPacket::CHANNELS_STEREO,
    AudioPacket::CHANNELS_MONO,
    AudioPacket::CHANNELS_MONO,
    AudioPacket::CHANNELS_MONO,
    AudioPacket::CHANNELS_MONO,
  };

  for (size_t i = 0; i < arraysize(kChannels); i++) {
    source_->callback().Run(MakeAudioPacket(kChannels[i]));
    // Run message loop to let the pump processes the audio packet and send it
    // to the encoder.
    base::RunLoop().RunUntilIdle();
    // Call done closure to allow one more packet to be sent.
    ASSERT_EQ(done_closures_.size(), 1U);
    done_closures_.front().Run();
    done_closures_.pop_back();
    base::RunLoop().RunUntilIdle();
  }

  ASSERT_EQ(sent_packets_.size(), arraysize(kChannels));
}

}  // namespace protocol
}  // namespace remoting
