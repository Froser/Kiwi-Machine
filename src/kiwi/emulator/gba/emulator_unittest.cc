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

void WriteHalfWord(Bytes& data, size_t offset, uint16_t value) {
  data[offset] = static_cast<Byte>(value);
  data[offset + 1] = static_cast<Byte>(value >> 8);
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

Bytes MakeDebugTestROM() {
  Bytes rom = MakeTestROM();
  WriteWord(rom, 0, 0xea000006);     // b 0x08000020
  WriteWord(rom, 0x20, 0xe3a00001);  // mov r0, #1
  WriteWord(rom, 0x24, 0xe3a01002);  // mov r1, #2
  WriteWord(rom, 0x28, 0xeafffffe);  // b .
  return rom;
}

Bytes MakeThumbDebugTestROM() {
  Bytes rom = MakeTestROM();
  WriteWord(rom, 0, 0xea000006);     // b 0x08000020
  WriteWord(rom, 0x20, 0xe28f0001);  // add r0, pc, #1
  WriteWord(rom, 0x24, 0xe12fff10);  // bx r0
  WriteHalfWord(rom, 0x28, 0x2107);  // movs r1, #7
  WriteHalfWord(rom, 0x2a, 0xe7fe);  // b .
  return rom;
}

Bytes MakeWatchpointTestROM() {
  Bytes rom = MakeTestROM();
  WriteWord(rom, 0, 0xea000006);     // b 0x08000020
  WriteWord(rom, 0x20, 0xe59f0008);  // ldr r0, [pc, #8]
  WriteWord(rom, 0x24, 0xe3a0103c);  // mov r1, #0x3c
  WriteWord(rom, 0x28, 0xe5c01000);  // strb r1, [r0]
  WriteWord(rom, 0x2c, 0xeafffffe);  // b .
  WriteWord(rom, 0x30, 0x02000010);
  return rom;
}

Bytes MakeCallStackTestROM() {
  Bytes rom = MakeTestROM();
  WriteWord(rom, 0, 0xea000006);     // b 0x08000020
  WriteWord(rom, 0x20, 0xeb000002);  // bl 0x08000030
  WriteWord(rom, 0x24, 0xeafffffe);  // b .
  WriteWord(rom, 0x30, 0xe3a00007);  // mov r0, #7
  WriteWord(rom, 0x34, 0xe12fff1e);  // bx lr
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

  bool LoadROM(bool run) { return LoadROM(MakeTestROM(), run); }

  bool LoadROM(const Bytes& rom, bool run) {
    bool loaded = false;
    Emulator::LoadCallback callback = base::BindOnce(
        [](bool* result, bool success) { *result = success; }, &loaded);
    if (run) {
      emulator_->LoadAndRun(rom, std::move(callback));
    } else {
      emulator_->LoadFromBinary(rom, std::move(callback));
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

TEST_F(EmulatorTest, CapturesArmStateAndStepsOneInstruction) {
  ASSERT_TRUE(LoadROM(MakeDebugTestROM(), true));

  std::optional<Emulator::DebugSnapshot> rejected;
  emulator_->GetDebugSnapshot(
      4, base::BindOnce(
             [](std::optional<Emulator::DebugSnapshot>* output,
                std::optional<Emulator::DebugSnapshot> snapshot) {
               *output = std::move(snapshot);
             },
             &rejected));
  EXPECT_FALSE(rejected);

  emulator_->Pause();
  std::optional<Emulator::DebugSnapshot> before;
  emulator_->GetDebugSnapshot(
      4, base::BindOnce(
             [](std::optional<Emulator::DebugSnapshot>* output,
                std::optional<Emulator::DebugSnapshot> snapshot) {
               *output = std::move(snapshot);
             },
             &before));
  ASSERT_TRUE(before);
  EXPECT_EQ(before->execution_mode, Emulator::DebugExecutionMode::kArm);
  EXPECT_EQ(before->instruction_address, 0x08000000u);
  EXPECT_EQ(before->registers[0], 0u);
  ASSERT_EQ(before->instructions.size(), 4u);
  EXPECT_EQ(before->instructions[0].address, 0x08000000u);
  EXPECT_EQ(before->instructions[0].opcode, 0xea000006u);
  EXPECT_EQ(before->instructions[0].width, 4u);
  EXPECT_NE(before->instructions[0].text.find("b"), std::string::npos);

  std::optional<Emulator::DebugSnapshot> after;
  emulator_->StepAndGetDebugSnapshot(
      4, base::BindOnce(
             [](std::optional<Emulator::DebugSnapshot>* output,
                std::optional<Emulator::DebugSnapshot> snapshot) {
               *output = std::move(snapshot);
             },
             &after));
  ASSERT_TRUE(after);
  EXPECT_EQ(after->instruction_address, 0x08000020u);
  EXPECT_EQ(after->instructions[0].opcode, 0xe3a00001u);
  EXPECT_NE(after->instructions[0].text.find("mov"), std::string::npos);

  emulator_->StepAndGetDebugSnapshot(
      4, base::BindOnce(
             [](std::optional<Emulator::DebugSnapshot>* output,
                std::optional<Emulator::DebugSnapshot> snapshot) {
               *output = std::move(snapshot);
             },
             &after));
  ASSERT_TRUE(after);
  EXPECT_EQ(after->registers[0], 1u);
  EXPECT_EQ(after->instruction_address, 0x08000024u);
  EXPECT_EQ(after->instructions[0].opcode, 0xe3a01002u);
}

TEST_F(EmulatorTest, DisassemblesThumbAfterModeSwitch) {
  ASSERT_TRUE(LoadROM(MakeThumbDebugTestROM(), true));
  emulator_->Pause();

  std::optional<Emulator::DebugSnapshot> snapshot;
  auto capture = [](std::optional<Emulator::DebugSnapshot>* output,
                    std::optional<Emulator::DebugSnapshot> value) {
    *output = std::move(value);
  };
  emulator_->StepAndGetDebugSnapshot(4, base::BindOnce(capture, &snapshot));
  ASSERT_TRUE(snapshot);
  EXPECT_EQ(snapshot->instruction_address, 0x08000020u);

  emulator_->StepAndGetDebugSnapshot(4, base::BindOnce(capture, &snapshot));
  ASSERT_TRUE(snapshot);
  EXPECT_EQ(snapshot->instruction_address, 0x08000024u);

  emulator_->StepAndGetDebugSnapshot(4, base::BindOnce(capture, &snapshot));
  ASSERT_TRUE(snapshot);
  EXPECT_EQ(snapshot->execution_mode, Emulator::DebugExecutionMode::kThumb);
  EXPECT_NE(snapshot->cpsr & (1u << 5), 0u);
  EXPECT_EQ(snapshot->instruction_address, 0x08000028u);
  ASSERT_FALSE(snapshot->instructions.empty());
  EXPECT_EQ(snapshot->instructions[0].opcode, 0x2107u);
  EXPECT_EQ(snapshot->instructions[0].width, 2u);
  EXPECT_NE(snapshot->instructions[0].text.find("mov"), std::string::npos);

  emulator_->StepAndGetDebugSnapshot(4, base::BindOnce(capture, &snapshot));
  ASSERT_TRUE(snapshot);
  EXPECT_EQ(snapshot->registers[1], 7u);
  EXPECT_EQ(snapshot->instruction_address, 0x0800002au);
}

TEST_F(EmulatorTest, CapturesMemoryGraphicsAudioAndMetrics) {
  ASSERT_TRUE(LoadROM(true));
  emulator_->RunOneFrame();
  emulator_->Pause();

  std::optional<Emulator::DebugMemorySnapshot> memory;
  emulator_->GetDebugMemorySnapshot(
      0x08000000, 32,
      base::BindOnce(
          [](std::optional<Emulator::DebugMemorySnapshot>* output,
             std::optional<Emulator::DebugMemorySnapshot> value) {
            *output = std::move(value);
          },
          &memory));
  ASSERT_TRUE(memory);
  EXPECT_EQ(memory->address, 0x08000000u);
  ASSERT_EQ(memory->data.size(), 32u);
  EXPECT_EQ(memory->data[3], 0xeau);
  EXPECT_FALSE(memory->regions.empty());
  EXPECT_FALSE(memory->io_registers.empty());

  std::optional<Emulator::DebugGraphicsSnapshot> graphics;
  emulator_->GetDebugGraphicsSnapshot(base::BindOnce(
      [](std::optional<Emulator::DebugGraphicsSnapshot>* output,
         std::optional<Emulator::DebugGraphicsSnapshot> value) {
        *output = std::move(value);
      },
      &graphics));
  ASSERT_TRUE(graphics);
  EXPECT_EQ(graphics->vram.size(), 0x18000u);
  EXPECT_EQ(graphics->palette.size(), 512u);
  EXPECT_EQ(graphics->sprites.size(), 128u);
  EXPECT_EQ(graphics->layers.size(), 8u);

  bool changed = false;
  emulator_->SetDebugVideoLayerEnabled(
      0, false,
      base::BindOnce([](bool* output, bool success) { *output = success; },
                     &changed));
  ASSERT_TRUE(changed);
  emulator_->GetDebugGraphicsSnapshot(base::BindOnce(
      [](std::optional<Emulator::DebugGraphicsSnapshot>* output,
         std::optional<Emulator::DebugGraphicsSnapshot> value) {
        *output = std::move(value);
      },
      &graphics));
  ASSERT_TRUE(graphics);
  EXPECT_FALSE(graphics->layers[0].enabled);

  std::optional<Emulator::DebugAudioSnapshot> audio;
  emulator_->GetDebugAudioSnapshot(base::BindOnce(
      [](std::optional<Emulator::DebugAudioSnapshot>* output,
         std::optional<Emulator::DebugAudioSnapshot> value) {
        *output = std::move(value);
      },
      &audio));
  ASSERT_TRUE(audio);
  EXPECT_EQ(audio->channels.size(), 6u);
  changed = false;
  emulator_->SetDebugAudioChannelEnabled(
      5, false,
      base::BindOnce([](bool* output, bool success) { *output = success; },
                     &changed));
  ASSERT_TRUE(changed);
  emulator_->GetDebugAudioSnapshot(base::BindOnce(
      [](std::optional<Emulator::DebugAudioSnapshot>* output,
         std::optional<Emulator::DebugAudioSnapshot> value) {
        *output = std::move(value);
      },
      &audio));
  ASSERT_TRUE(audio);
  EXPECT_FALSE(audio->channels[5].enabled);

  const Emulator::DebugMetrics metrics = emulator_->GetDebugMetrics();
  EXPECT_GT(metrics.frame_count, 0u);
  EXPECT_GT(metrics.last_frame_duration_us, 0u);
  EXPECT_GT(metrics.last_audio_sample_count, 0u);
}

TEST_F(EmulatorTest, BreakpointStopsBeforeTargetInstruction) {
  ASSERT_TRUE(LoadROM(MakeDebugTestROM(), true));
  emulator_->Pause();

  bool added = false;
  emulator_->AddDebugBreakpoint(
      0x08000020,
      base::BindOnce([](bool* output, bool success) { *output = success; },
                     &added));
  ASSERT_TRUE(added);

  emulator_->Run();
  emulator_->RunOneFrame();
  EXPECT_EQ(emulator_->GetRunningState(), Emulator::RunningState::kPaused);

  std::optional<Emulator::DebugSnapshot> snapshot;
  emulator_->GetDebugSnapshot(
      4, base::BindOnce(
             [](std::optional<Emulator::DebugSnapshot>* output,
                std::optional<Emulator::DebugSnapshot> value) {
               *output = std::move(value);
             },
             &snapshot));
  ASSERT_TRUE(snapshot);
  EXPECT_EQ(snapshot->instruction_address, 0x08000020u);
  EXPECT_EQ(snapshot->registers[0], 0u);
  EXPECT_EQ(snapshot->last_break.reason,
            Emulator::DebugBreakReason::kBreakpoint);
  ASSERT_EQ(snapshot->points.size(), 1u);
  EXPECT_EQ(snapshot->points[0].address, 0x08000020u);
}

TEST_F(EmulatorTest, WatchpointCapturesMemoryWrite) {
  ASSERT_TRUE(LoadROM(MakeWatchpointTestROM(), true));
  emulator_->Pause();

  bool added = false;
  emulator_->AddDebugWatchpoint(
      0x02000010, Emulator::DebugWatchpointType::kWrite,
      base::BindOnce([](bool* output, bool success) { *output = success; },
                     &added));
  ASSERT_TRUE(added);

  emulator_->Run();
  emulator_->RunOneFrame();
  EXPECT_EQ(emulator_->GetRunningState(), Emulator::RunningState::kPaused);

  std::optional<Emulator::DebugSnapshot> snapshot;
  emulator_->GetDebugSnapshot(
      4, base::BindOnce(
             [](std::optional<Emulator::DebugSnapshot>* output,
                std::optional<Emulator::DebugSnapshot> value) {
               *output = std::move(value);
             },
             &snapshot));
  ASSERT_TRUE(snapshot);
  EXPECT_EQ(snapshot->last_break.reason,
            Emulator::DebugBreakReason::kWatchpoint);
  EXPECT_EQ(snapshot->last_break.address, 0x02000010u);
  EXPECT_EQ(snapshot->last_break.new_value & 0xff, 0x3cu);

  std::optional<Emulator::DebugMemorySnapshot> memory;
  emulator_->GetDebugMemorySnapshot(
      0x02000010, 16,
      base::BindOnce(
          [](std::optional<Emulator::DebugMemorySnapshot>* output,
             std::optional<Emulator::DebugMemorySnapshot> value) {
            *output = std::move(value);
          },
          &memory));
  ASSERT_TRUE(memory);
  EXPECT_EQ(memory->data[0], 0x3cu);
}

TEST_F(EmulatorTest, TracksArmCallStack) {
  ASSERT_TRUE(LoadROM(MakeCallStackTestROM(), true));
  emulator_->Pause();

  bool enabled = false;
  emulator_->SetDebugStackTraceEnabled(
      true,
      base::BindOnce([](bool* output, bool success) { *output = success; },
                     &enabled));
  ASSERT_TRUE(enabled);

  std::optional<Emulator::DebugSnapshot> snapshot;
  emulator_->StepAndGetDebugSnapshot(
      4, base::BindOnce(
             [](std::optional<Emulator::DebugSnapshot>* output,
                std::optional<Emulator::DebugSnapshot> value) {
               *output = std::move(value);
             },
             &snapshot));
  ASSERT_TRUE(snapshot);
  EXPECT_TRUE(snapshot->stack_trace_enabled);
  ASSERT_EQ(snapshot->stack_frames.size(), 1u);
  EXPECT_EQ(snapshot->stack_frames[0].call_address, 0x08000020u);
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
