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

#include "nes/mappers/mapper_test_support.h"

#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/runloop.h"
#include "nes/emulator_impl.h"

namespace kiwi {
namespace nes {
namespace testing {
namespace {

constexpr size_t kINESHeaderSize = 0x10;
constexpr size_t k8K = 0x2000;

void SetPRGByte(Bytes* rom, size_t bank, Address address, Byte value) {
  ASSERT_TRUE(rom);
  const size_t index = kINESHeaderSize + bank * k8K + (address & 0x1fff);
  ASSERT_LT(index, rom->size());
  (*rom)[index] = value;
}

}  // namespace

class Mapper245Test : public MapperTest {
 protected:
  scoped_refptr<Cartridge> LoadCustomROM(const Bytes& rom) {
    auto cartridge = base::MakeRefCounted<Cartridge>(
        static_cast<EmulatorImpl*>(emulator_.get()));
    const Cartridge::LoadResult result = cartridge->Load(rom);
    EXPECT_TRUE(result.success);
    if (!result.success)
      return nullptr;

    cartridge->mapper()->set_mirroring_changed_callback(base::BindRepeating(
        [](int* count) { ++*count; }, base::Unretained(&mirroring_changes_)));
    cartridge->mapper()->set_irq_callback(base::BindRepeating(
        [](int* count) { ++*count; }, base::Unretained(&irq_count_)));
    cartridge->mapper()->Reset();
    return cartridge;
  }
};

TEST_F(Mapper245Test, SelectsBothProgramROMHalves) {
  ASSERT_TRUE(Mapper::IsMapperSupported(245));
  Bytes rom = MakeTestROM(245, 64, 0, 0x02);
  for (size_t bank = 0; bank < 128; ++bank)
    SetPRGByte(&rom, bank, 0x8123, static_cast<Byte>(bank));
  auto cartridge = LoadCustomROM(rom);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteMMC3Register(mapper, 6, 3);
  WriteMMC3Register(mapper, 7, 4);
  EXPECT_EQ(mapper->ReadPRG(0x8123), 3);
  EXPECT_EQ(mapper->ReadPRG(0xa123), 4);
  EXPECT_EQ(mapper->ReadPRG(0xc123), 62);
  EXPECT_EQ(mapper->ReadPRG(0xe123), 63);

  WriteMMC3Register(mapper, 0, 0x02);
  EXPECT_EQ(mapper->ReadPRG(0x8123), 67);
  EXPECT_EQ(mapper->ReadPRG(0xa123), 68);
  EXPECT_EQ(mapper->ReadPRG(0xc123), 126);
  EXPECT_EQ(mapper->ReadPRG(0xe123), 127);

  mapper->WritePRG(0x8000, 0x46);
  mapper->WritePRG(0x8001, 5);
  EXPECT_EQ(mapper->ReadPRG(0x8123), 126);
  EXPECT_EQ(mapper->ReadPRG(0xc123), 69);
}

TEST_F(Mapper245Test, KeepsCharacterRAMUnbanked) {
  auto cartridge = LoadMapper(245, 64, 0, 0x02);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteCHR(0x0123, 0x45);
  mapper->WriteCHR(0x1123, 0x67);
  WriteMMC3Register(mapper, 0, 0xfe);
  WriteMMC3Register(mapper, 1, 0xfc);
  mapper->WritePRG(0x8000, 0x80);
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x45);
  EXPECT_EQ(mapper->ReadCHR(0x1123), 0x67);
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x1123), 0x1123u);
}

TEST_F(Mapper245Test, PreservesMMC3IRQAndState) {
  Bytes rom = MakeTestROM(245, 64, 0, 0x02);
  SetPRGByte(&rom, 66, 0x8123, 0x66);
  auto cartridge = LoadCustomROM(rom);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteMMC3Register(mapper, 0, 0x02);
  WriteMMC3Register(mapper, 6, 0x02);
  mapper->WriteCHR(0x0123, 0x5a);
  mapper->WritePRG(0xc000, 1);
  mapper->WritePRG(0xc001, 0);
  mapper->WritePRG(0xe001, 0);
  const Bytes state = SerializeMapper(mapper);

  WriteMMC3Register(mapper, 0, 0);
  mapper->WriteCHR(0x0123, 0xa5);
  mapper->WritePRG(0xe000, 0);
  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8123), 0x66);
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x5a);
  mapper->ScanlineIRQ(0, true);
  mapper->ScanlineIRQ(1, true);
  EXPECT_EQ(irq_count_, 1);
}

TEST_F(Mapper245Test, CorrectsKnownBadHeaderMetadata) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::filesystem::path rom_path =
      std::filesystem::path(rom_directory) /
      "Ying Xiong Yuan Yi Jing Chuan Qi (China) (Unl).nes";
  auto cartridge = base::MakeRefCounted<Cartridge>(
      static_cast<EmulatorImpl*>(emulator_.get()));
  const Cartridge::LoadResult result =
      cartridge->Load(base::FilePath::FromUTF8Unsafe(rom_path.string()));
  ASSERT_TRUE(result.success) << rom_path;
  ASSERT_TRUE(cartridge->GetRomData());
  EXPECT_EQ(static_cast<uint32_t>(cartridge->GetRomData()->crc), 0xdfad3f66u);
  EXPECT_EQ(static_cast<uint32_t>(cartridge->GetRomData()->mapper), 4u);
  EXPECT_EQ(cartridge->GetRomData()->name_table_mirroring,
            NametableMirroring::kHorizontal);
  EXPECT_TRUE(cartridge->GetRomData()->has_battery);
  EXPECT_EQ(cartridge->GetRomData()->prg_nvram_size, k8K);
}

TEST_F(Mapper245Test, RendersKnownBadHeaderROMFor600Frames) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::filesystem::path rom_path =
      std::filesystem::path(rom_directory) /
      "Ying Xiong Yuan Yi Jing Chuan Qi (China) (Unl).nes";
  bool loaded = false;
  emulator_->LoadFromFile(
      base::FilePath::FromUTF8Unsafe(rom_path.string()),
      base::BindOnce([](bool* loaded, bool success) { *loaded = success; },
                     base::Unretained(&loaded)));
  ASSERT_TRUE(loaded) << rom_path;
  ASSERT_TRUE(emulator_->GetRomData());
  ASSERT_EQ(static_cast<uint32_t>(emulator_->GetRomData()->mapper), 4u);

  // This known bad dump needs one soft reset after its cold-boot protection
  // reaches KIL, matching the behavior documented by MAME.
  emulator_->Run();
  for (int frame = 0; frame < 18; ++frame)
    emulator_->RunOneFrame();
  emulator_->Pause();
  bool reached_kil = false;
  for (int cycle = 0; cycle < 100000; ++cycle) {
    if (static_cast<EmulatorImpl*>(emulator_.get())
            ->GetCPUContext()
            .registers.PC == 0x09fc) {
      reached_kil = true;
      break;
    }
    emulator_->Step();
  }
  ASSERT_TRUE(reached_kil);
  base::RunLoop reset_loop;
  emulator_->Reset(
      base::BindOnce([](base::RepeatingClosure quit) { quit.Run(); },
                     reset_loop.QuitClosure()));
  reset_loop.Run();
  emulator_->Run();

  Colors previous_pixels;
  bool rendered = false;
  bool frame_changed = false;
  size_t final_unique_colors = 0;
  for (int frame = 0; frame < 600; ++frame) {
    emulator_->RunOneFrame();
    const Colors& pixels = emulator_->GetLastFrame();
    ASSERT_EQ(pixels.size(), 256u * 240u) << rom_path;
    final_unique_colors = std::set<Color>(pixels.begin(), pixels.end()).size();
    rendered = rendered || final_unique_colors > 1;
    if (!previous_pixels.empty() && pixels != previous_pixels)
      frame_changed = true;
    previous_pixels = pixels;
  }

  EXPECT_TRUE(rendered) << rom_path;
  EXPECT_TRUE(frame_changed) << rom_path;
  EXPECT_GT(final_unique_colors, 1u) << rom_path;
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
