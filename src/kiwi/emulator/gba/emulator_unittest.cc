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

#include "emulator/gba/emulator.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_executor.h"
#include "emulator/gba/io_devices.h"
#include "third_party/googletest-release-1.12.1/googletest/include/gtest/gtest.h"

namespace kiwi {
namespace gba {
namespace testing {
namespace {

constexpr size_t kBIOSSize = 0x4000;
constexpr size_t kSRAMSize = 0x8000;

void WriteWord(Bytes& data, size_t offset, uint32_t value) {
  for (size_t byte = 0; byte < sizeof(value); ++byte)
    data[offset + byte] = static_cast<Byte>(value >> (byte * 8));
}

Bytes MakeTestROM() {
  Bytes rom(0xc0);
  // Infinite ARM branch at the cartridge entry point.
  WriteWord(rom, 0, 0xeafffffe);
  constexpr char kTitle[] = "KIWI TEST";
  constexpr char kGameCode[] = "KTSE";
  std::copy(kTitle, kTitle + sizeof(kTitle) - 1, rom.begin() + 0xa0);
  std::copy(kGameCode, kGameCode + sizeof(kGameCode) - 1, rom.begin() + 0xac);
  // Fixed-value byte required by the GBA cartridge header.
  rom[0xb2] = 0x96;
  return rom;
}

Bytes MakeSaveWritingTestROM() {
  Bytes rom = MakeTestROM();
  // Write 0x3c to SRAM address 0x0e000123, then loop forever.
  WriteWord(rom, 0, 0xea000006);
  WriteWord(rom, 0x20, 0xe59f0008);
  WriteWord(rom, 0x24, 0xe3a0103c);
  WriteWord(rom, 0x28, 0xe5c01000);
  WriteWord(rom, 0x2c, 0xeafffffe);
  WriteWord(rom, 0x30, 0x0e000123);
  return rom;
}

Bytes MakeTestBIOS(size_t size) {
  Bytes bios(size);
  for (size_t interrupt = 0; interrupt < 7; ++interrupt) {
    size_t opcode = interrupt * sizeof(uint32_t);
    if (opcode + 3 < bios.size())
      bios[opcode + 3] = 0xea;
  }
  return bios;
}

class TestInputDevice : public IODevices::InputDevice {
 public:
  bool IsKeyDown(int controller_id, ControllerButton button) override {
    last_controller_id = controller_id;
    ++read_count;
    return button == ControllerButton::kA || button == ControllerButton::kR;
  }

  int last_controller_id = -1;
  int read_count = 0;
};

class TestRenderDevice : public IODevices::RenderDevice {
 public:
  bool NeedRender() override { return true; }

  void Render(const FrameData& frame) override {
    ++render_count;
    last_width = frame.width;
    last_height = frame.height;
    last_frame_number = frame.frame_number;
    if (frame.pixels)
      last_frame = *frame.pixels;
  }

  int render_count = 0;
  int last_width = 0;
  int last_height = 0;
  uint64_t last_frame_number = 0;
  Buffer last_frame;
};

class TestAudioDevice : public IODevices::AudioDevice {
 public:
  void OnSampleArrived(Sample* samples, size_t count) override {
    ++callback_count;
    sample_count += count;
    last_samples.assign(samples, samples + count);
  }

  int callback_count = 0;
  size_t sample_count = 0;
  std::vector<Sample> last_samples;
};

class TestLuminanceDevice : public IODevices::LuminanceDevice {
 public:
  uint8_t ReadLuminance() override { return 0x80; }
};

class TestRumbleDevice : public IODevices::RumbleDevice {
 public:
  void SetRumble(bool enabled) override { last_enabled = enabled; }

  bool last_enabled = false;
};

class TestRTCDevice : public IODevices::RTCDevice {
 public:
  int64_t GetUnixTimeSeconds() override { return 123456789; }
};

class EmulatorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    task_executor_ = std::make_unique<base::SingleThreadTaskExecutor>();
    emulator_ = CreateEmulatorForTesting();
    emulator_->PowerOn();

    auto io_devices = std::make_unique<IODevices>();
    io_devices->set_input_device(&input_device_);
    io_devices->add_render_device(&render_device_);
    io_devices->set_audio_device(&audio_device_);
    emulator_->SetIODevices(std::move(io_devices));
  }

  void TearDown() override { emulator_->PowerOff(); }

  bool LoadROM(bool run) {
    bool loaded = false;
    Emulator::LoadCallback callback = base::BindOnce(
        [](bool* result, bool success) { *result = success; }, &loaded);
    if (run) {
      emulator_->LoadAndRun(MakeTestROM(), std::move(callback));
    } else {
      emulator_->LoadFromBinary(MakeTestROM(), std::move(callback));
    }
    return loaded;
  }

  std::optional<Emulator::SaveDataSnapshot> ExportSaveData(bool dirty_only) {
    std::optional<Emulator::SaveDataSnapshot> snapshot;
    emulator_->ExportSaveData(
        dirty_only, base::BindOnce(
                        [](std::optional<Emulator::SaveDataSnapshot>* output,
                           std::optional<Emulator::SaveDataSnapshot> value) {
                          *output = std::move(value);
                        },
                        &snapshot));
    return snapshot;
  }

  bool AcknowledgeSaveData(uint32_t rom_crc32, uint64_t generation) {
    bool acknowledged = false;
    emulator_->AcknowledgeSaveDataSaved(
        rom_crc32, generation,
        base::BindOnce([](bool* result, bool success) { *result = success; },
                       &acknowledged));
    return acknowledged;
  }

  std::unique_ptr<base::SingleThreadTaskExecutor> task_executor_;
  TestInputDevice input_device_;
  TestRenderDevice render_device_;
  TestAudioDevice audio_device_;
  scoped_refptr<Emulator> emulator_;
};

TEST_F(EmulatorTest, LoadsRunsAndPresentsFrame) {
  ASSERT_TRUE(LoadROM(true));
  EXPECT_EQ(emulator_->GetRunningState(), Emulator::RunningState::kRunning);

  const RomData* rom_data = emulator_->GetRomData();
  ASSERT_NE(rom_data, nullptr);
  EXPECT_EQ(rom_data->title, "KIWI TEST");
  EXPECT_EQ(rom_data->game_code, "AGB-KTSE");
  EXPECT_EQ(rom_data->rom_size, MakeTestROM().size());
  EXPECT_NE(rom_data->crc32, 0u);
  EXPECT_EQ(rom_data->save_type, SaveType::kAuto);
  EXPECT_EQ(rom_data->save_size, 0u);

  emulator_->RunOneFrame();

  EXPECT_GT(input_device_.read_count, 0);
  EXPECT_EQ(input_device_.last_controller_id, 0);
  EXPECT_EQ(render_device_.render_count, 1);
  EXPECT_EQ(render_device_.last_width, kFrameWidth);
  EXPECT_EQ(render_device_.last_height, kFrameHeight);
  EXPECT_GT(render_device_.last_frame_number, 0u);
  ASSERT_EQ(render_device_.last_frame.size(),
            static_cast<size_t>(kFrameWidth * kFrameHeight));
  EXPECT_TRUE(std::all_of(
      render_device_.last_frame.begin(), render_device_.last_frame.end(),
      [](Color color) { return (color & 0xff000000) == 0xff000000; }));
  EXPECT_EQ(emulator_->GetLastFrame(), render_device_.last_frame);
  EXPECT_GT(audio_device_.callback_count, 0);
  EXPECT_GT(audio_device_.sample_count, 0u);
  EXPECT_EQ(audio_device_.sample_count % 2, 0u);
}

TEST_F(EmulatorTest, SavesAndLoadsState) {
  ASSERT_TRUE(LoadROM(true));
  emulator_->RunOneFrame();

  Bytes state;
  emulator_->SaveState(base::BindOnce(
      [](Bytes* output, Bytes value) { *output = std::move(value); }, &state));
  ASSERT_FALSE(state.empty());

  emulator_->RunOneFrame();
  emulator_->Pause();

  bool loaded = false;
  emulator_->LoadState(
      state,
      base::BindOnce([](bool* result, bool success) { *result = success; },
                     &loaded));
  EXPECT_TRUE(loaded);
  EXPECT_EQ(emulator_->GetRunningState(), Emulator::RunningState::kPaused);

  bool rejected = true;
  emulator_->LoadState(
      Bytes{0},
      base::BindOnce([](bool* result, bool success) { *result = success; },
                     &rejected));
  EXPECT_FALSE(rejected);
}

TEST_F(EmulatorTest, RejectsInvalidROMWithoutReplacingLoadedROM) {
  ASSERT_TRUE(LoadROM(true));

  bool loaded = true;
  emulator_->LoadFromBinary(
      Bytes{0xea},
      base::BindOnce([](bool* result, bool success) { *result = success; },
                     &loaded));
  EXPECT_FALSE(loaded);
  EXPECT_EQ(emulator_->GetRunningState(), Emulator::RunningState::kRunning);

  emulator_->RunOneFrame();
  EXPECT_EQ(render_device_.render_count, 1);
}

TEST_F(EmulatorTest, UnloadStopsEmulatorAndClearsState) {
  ASSERT_TRUE(LoadROM(true));

  bool unloaded = false;
  emulator_->Unload(
      base::BindOnce([](bool* result) { *result = true; }, &unloaded));
  EXPECT_TRUE(unloaded);
  EXPECT_EQ(emulator_->GetRunningState(), Emulator::RunningState::kStopped);

  Bytes state{1};
  emulator_->SaveState(base::BindOnce(
      [](Bytes* output, Bytes value) { *output = std::move(value); }, &state));
  EXPECT_TRUE(state.empty());
}

TEST_F(EmulatorTest, ClampsVolume) {
  emulator_->SetVolume(2.f);
  EXPECT_FLOAT_EQ(emulator_->GetVolume(), 1.f);
  emulator_->SetVolume(-1.f);
  EXPECT_FLOAT_EQ(emulator_->GetVolume(), 0.f);
}

TEST_F(EmulatorTest, ValidatesAndLoadsBIOS) {
  bool loaded = true;
  emulator_->LoadBIOSFromBinary(
      MakeTestBIOS(7 * sizeof(uint32_t)),
      base::BindOnce([](bool* result, bool success) { *result = success; },
                     &loaded));
  EXPECT_FALSE(loaded);
  EXPECT_FALSE(emulator_->HasBIOS());

  emulator_->LoadBIOSFromBinary(
      MakeTestBIOS(kBIOSSize),
      base::BindOnce([](bool* result, bool success) { *result = success; },
                     &loaded));
  EXPECT_TRUE(loaded);
  EXPECT_TRUE(emulator_->HasBIOS());
}

TEST_F(EmulatorTest, ImportsExportsAndAcknowledgesSaveData) {
  Bytes initial_save(kSRAMSize, 0xff);
  initial_save[0x123] = 0x11;
  bool loaded = false;
  emulator_->LoadAndRunWithSaveData(
      MakeSaveWritingTestROM(), initial_save,
      base::BindOnce([](bool* result, bool success) { *result = success; },
                     &loaded),
      Emulator::LoadOptions(true, SaveType::kSRAM));
  ASSERT_TRUE(loaded);

  const RomData* rom_data = emulator_->GetRomData();
  ASSERT_NE(rom_data, nullptr);
  EXPECT_EQ(rom_data->save_type, SaveType::kSRAM);
  EXPECT_EQ(rom_data->save_size, kSRAMSize);

  std::optional<Emulator::SaveDataSnapshot> initial_snapshot =
      ExportSaveData(false);
  ASSERT_TRUE(initial_snapshot);
  EXPECT_EQ(initial_snapshot->rom_crc32, rom_data->crc32);
  EXPECT_EQ(initial_snapshot->data, initial_save);
  EXPECT_EQ(initial_snapshot->generation, 0u);
  EXPECT_FALSE(ExportSaveData(true));

  Bytes state;
  emulator_->SaveState(base::BindOnce(
      [](Bytes* output, Bytes value) { *output = std::move(value); }, &state));
  ASSERT_FALSE(state.empty());

  emulator_->RunOneFrame();

  std::optional<Emulator::SaveDataSnapshot> dirty_snapshot =
      ExportSaveData(true);
  ASSERT_TRUE(dirty_snapshot);
  ASSERT_EQ(dirty_snapshot->data.size(), kSRAMSize);
  EXPECT_EQ(dirty_snapshot->data[0x123], 0x3c);
  EXPECT_EQ(dirty_snapshot->generation, 1u);

  EXPECT_FALSE(AcknowledgeSaveData(dirty_snapshot->rom_crc32 + 1,
                                   dirty_snapshot->generation));
  EXPECT_FALSE(AcknowledgeSaveData(dirty_snapshot->rom_crc32,
                                   dirty_snapshot->generation + 1));
  EXPECT_TRUE(AcknowledgeSaveData(dirty_snapshot->rom_crc32,
                                  dirty_snapshot->generation));
  EXPECT_FALSE(ExportSaveData(true));

  emulator_->Pause();
  bool state_loaded = false;
  emulator_->LoadState(
      state,
      base::BindOnce([](bool* result, bool success) { *result = success; },
                     &state_loaded));
  ASSERT_TRUE(state_loaded);

  std::optional<Emulator::SaveDataSnapshot> restored_snapshot =
      ExportSaveData(true);
  ASSERT_TRUE(restored_snapshot);
  EXPECT_EQ(restored_snapshot->data, initial_save);
  EXPECT_EQ(restored_snapshot->generation, 2u);
  EXPECT_TRUE(AcknowledgeSaveData(restored_snapshot->rom_crc32,
                                  restored_snapshot->generation));
  EXPECT_FALSE(ExportSaveData(true));
}

TEST_F(EmulatorTest, RejectsInvalidSaveDataWithoutReplacingROM) {
  ASSERT_TRUE(LoadROM(false));
  const RomData* previous_rom = emulator_->GetRomData();

  bool loaded = true;
  emulator_->LoadAndRunWithSaveData(
      MakeTestROM(), Bytes(1),
      base::BindOnce([](bool* result, bool success) { *result = success; },
                     &loaded),
      Emulator::LoadOptions(true, SaveType::kSRAM));

  EXPECT_FALSE(loaded);
  EXPECT_EQ(emulator_->GetRomData(), previous_rom);
}

TEST(IODevicesTest, StoresDevicesAndProvidesMotionDefaults) {
  IODevices devices;
  IODevices::MotionDevice motion;
  TestInputDevice input;
  TestRenderDevice renderer;
  TestAudioDevice audio;
  TestLuminanceDevice luminance;
  TestRumbleDevice rumble;
  TestRTCDevice rtc;

  EXPECT_EQ(motion.ReadTiltX(), 0);
  EXPECT_EQ(motion.ReadTiltY(), 0);
  EXPECT_EQ(motion.ReadGyroZ(), 0);
  motion.Sample();
  luminance.Sample();
  rtc.Sample();

  devices.set_input_device(&input);
  devices.add_render_device(&renderer);
  devices.set_audio_device(&audio);
  devices.set_motion_device(&motion);
  devices.set_luminance_device(&luminance);
  devices.set_rumble_device(&rumble);
  devices.set_rtc_device(&rtc);

  EXPECT_EQ(devices.input_device(), &input);
  EXPECT_EQ(devices.render_devices().count(&renderer), 1u);
  EXPECT_EQ(devices.audio_device(), &audio);
  EXPECT_EQ(devices.motion_device(), &motion);
  EXPECT_EQ(devices.luminance_device(), &luminance);
  EXPECT_EQ(devices.rumble_device(), &rumble);
  EXPECT_EQ(devices.rtc_device(), &rtc);

  devices.remove_render_device(&renderer);
  EXPECT_TRUE(devices.render_devices().empty());
}

}  // namespace
}  // namespace testing
}  // namespace gba
}  // namespace kiwi
