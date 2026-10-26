// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

#include "models/audio_base.h"

#include <array>

#include "third_party/googletest-release-1.12.1/googletest/include/gtest/gtest.h"

namespace {

class TestAudioBase : public AudioBase {
 public:
  TestAudioBase(size_t buffer_size, size_t buffer_count)
      : AudioBase(buffer_size, buffer_count) {}

  using AudioBase::ReadSamples;
  using AudioBase::WriteSamples;

  int lock_count() const { return lock_count_; }
  int unlock_count() const { return unlock_count_; }

 protected:
  bool IsOutputActive() const override { return true; }
  void LockOutput() override { ++lock_count_; }
  void UnlockOutput() override { ++unlock_count_; }

 private:
  int lock_count_ = 0;
  int unlock_count_ = 0;
};

TEST(AudioBaseTest, PublishesOnlyCompleteBuffers) {
  TestAudioBase audio(4, 2);
  const std::array<kiwi::emu::Sample, 2> first = {1, 2};
  const std::array<kiwi::emu::Sample, 2> second = {3, 4};
  std::array<kiwi::emu::Sample, 4> output = {};

  audio.WriteSamples(first.data(), first.size());
  EXPECT_EQ(audio.ReadSamples(output.data(), output.size()), 0u);

  audio.WriteSamples(second.data(), second.size());
  EXPECT_EQ(audio.ReadSamples(output.data(), 3), 3u);
  EXPECT_EQ(output[0], 1);
  EXPECT_EQ(output[1], 2);
  EXPECT_EQ(output[2], 3);
  EXPECT_EQ(audio.ReadSamples(output.data() + 3, 1), 1u);
  EXPECT_EQ(output[3], 4);
}

TEST(AudioBaseTest, DropsNewestBufferWhenRingIsFull) {
  TestAudioBase audio(2, 2);
  const std::array<kiwi::emu::Sample, 6> input = {1, 2, 3, 4, 5, 6};
  std::array<kiwi::emu::Sample, 6> output = {};

  audio.WriteSamples(input.data(), input.size());
  EXPECT_EQ(audio.ReadSamples(output.data(), output.size()), 4u);
  EXPECT_EQ(output[0], 1);
  EXPECT_EQ(output[1], 2);
  EXPECT_EQ(output[2], 3);
  EXPECT_EQ(output[3], 4);
}

TEST(AudioBaseTest, PreservesInterleavedStereoAcrossBuffers) {
  TestAudioBase audio(4, 2);
  const std::array<kiwi::emu::Sample, 8> input = {
      10, 11, 20, 21, 30, 31, 40, 41,
  };
  std::array<kiwi::emu::Sample, 8> output = {};

  audio.WriteSamples(input.data(), input.size());
  EXPECT_EQ(audio.ReadSamples(output.data(), 6), 6u);
  EXPECT_EQ(audio.ReadSamples(output.data() + 6, 2), 2u);
  EXPECT_EQ(output, input);
}

TEST(AudioBaseTest, LocksActiveOutputWhileResetting) {
  TestAudioBase audio(4, 2);
  audio.Reset();

  EXPECT_EQ(audio.lock_count(), 1);
  EXPECT_EQ(audio.unlock_count(), 1);
}

}  // namespace
