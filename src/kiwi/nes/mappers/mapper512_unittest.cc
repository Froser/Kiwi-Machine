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
#include "nes/emulator_impl.h"
#include "nes/ppu_bus.h"

namespace kiwi {
namespace nes {
namespace testing {
namespace {

constexpr size_t k1K = 0x0400;
constexpr size_t k8K = 0x2000;

Bytes MakeMapper512ROM() {
  return MakeTestROM(512, 16, 16, 0x02, 0, 0, 0x70);
}

}  // namespace

class Mapper512Test : public MapperTest {
 protected:
  scoped_refptr<Cartridge> LoadCustomROM(const Bytes& rom) {
    auto cartridge = base::MakeRefCounted<Cartridge>(
        static_cast<EmulatorImpl*>(emulator_.get()));
    const Cartridge::LoadResult result = cartridge->Load(rom);
    EXPECT_TRUE(result.success);
    if (!result.success)
      return nullptr;

    EXPECT_EQ(static_cast<uint32_t>(cartridge->GetRomData()->mapper), 512u);
    if (static_cast<uint32_t>(cartridge->GetRomData()->mapper) != 512u)
      return nullptr;

    cartridge->mapper()->set_mirroring_changed_callback(base::BindRepeating(
        [](int* count) { ++*count; }, base::Unretained(&mirroring_changes_)));
    cartridge->mapper()->set_irq_callback(base::BindRepeating(
        [](int* count) { ++*count; }, base::Unretained(&irq_count_)));
    cartridge->mapper()->Reset();
    return cartridge;
  }

  scoped_refptr<Cartridge> LoadMapper512() {
    return LoadCustomROM(MakeMapper512ROM());
  }
};

TEST_F(Mapper512Test, ParsesTwelveBitNES20MapperAndNVRAMMetadata) {
  EXPECT_TRUE(Mapper::IsMapperSupported(512));
  auto cartridge = base::MakeRefCounted<Cartridge>(
      static_cast<EmulatorImpl*>(emulator_.get()));
  ASSERT_TRUE(cartridge->Load(MakeMapper512ROM()).success);
  ASSERT_TRUE(cartridge->GetRomData());

  EXPECT_EQ(static_cast<uint32_t>(cartridge->GetRomData()->mapper), 512u);
  EXPECT_TRUE(cartridge->GetRomData()->is_nes_20);
  EXPECT_TRUE(cartridge->GetRomData()->has_battery);
  EXPECT_EQ(cartridge->GetRomData()->prg_ram_size, 0u);
  EXPECT_EQ(cartridge->GetRomData()->prg_nvram_size, k8K);
}

TEST_F(Mapper512Test, CorrectsLegacyChuugokuHeaderByPayloadCRC) {
  Bytes rom = MakeTestROM(116, 16, 16, 0x08);
  ASSERT_GE(rom.size(), 4u);
  rom[rom.size() - 4] = 0x62;
  rom[rom.size() - 3] = 0x32;
  rom[rom.size() - 2] = 0xa1;
  rom[rom.size() - 1] = 0x48;

  auto cartridge = base::MakeRefCounted<Cartridge>(
      static_cast<EmulatorImpl*>(emulator_.get()));
  const Cartridge::LoadResult result = cartridge->Load(rom);
  ASSERT_TRUE(result.success);
  ASSERT_TRUE(cartridge->GetRomData());
  EXPECT_EQ(result.crc32, 0x037006f7u);
  EXPECT_EQ(static_cast<uint32_t>(cartridge->GetRomData()->mapper), 512u);
  EXPECT_EQ(cartridge->GetRomData()->name_table_mirroring,
            NametableMirroring::kHorizontal);
  EXPECT_TRUE(cartridge->GetRomData()->has_battery);
  EXPECT_EQ(cartridge->GetRomData()->prg_ram_size, 0u);
  EXPECT_EQ(cartridge->GetRomData()->prg_nvram_size, k8K);
}

TEST_F(Mapper512Test, PreservesMMC3BankingMirroringIRQAndWRAM) {
  auto cartridge = LoadMapper512();
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteMMC3Register(mapper, 6, 3);
  WriteMMC3Register(mapper, 7, 4);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(3 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xa123), TestPRGByte(4 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(30 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xe123), TestPRGByte(31 * k8K + 0x0123));

  WriteMMC3Register(mapper, 2, 9);
  EXPECT_EQ(mapper->ReadCHR(0x1155), TestCHRByte(9 * k1K + 0x0155));

  mapper->WritePRG(0xa000, 0);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
  mapper->WritePRG(0xa000, 1);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);

  mapper->WritePRG(0xa001, 0);
  mapper->WriteExtendedRAM(0x6123, 0x5a);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x5a);

  mapper->WritePRG(0xc000, 1);
  mapper->WritePRG(0xc001, 0);
  mapper->WritePRG(0xe001, 0);
  mapper->ScanlineIRQ(0, true);
  mapper->ScanlineIRQ(1, true);
  EXPECT_EQ(irq_count_, 1);
}

TEST_F(Mapper512Test, SwitchesIndependentCharacterAndNametableRAM) {
  auto cartridge = LoadMapper512();
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  PPUBus ppu_bus;
  ppu_bus.SetMapper(mapper);

  WriteMMC3Register(mapper, 2, 9);
  const Byte chr_rom_byte = TestCHRByte(9 * k1K + 0x0123);
  EXPECT_EQ(ppu_bus.Read(0x1123), chr_rom_byte);

  ppu_bus.Write(0x2001, 0x11);
  ppu_bus.Write(0x2801, 0x22);

  mapper->WriteExtendedRAM(0x4100, 2);
  ppu_bus.UpdateMirroring();
  ppu_bus.Write(0x0123, 0x5a);
  EXPECT_EQ(ppu_bus.Read(0x0123), 0x5a);
  EXPECT_EQ(ppu_bus.Read(0x1123), 0x5a);
  EXPECT_EQ(ppu_bus.Read(0x2001), 0x11);
  EXPECT_EQ(ppu_bus.Read(0x2801), 0x22);

  mapper->WriteExtendedRAM(0x4100, 1);
  ppu_bus.UpdateMirroring();
  EXPECT_EQ(ppu_bus.Read(0x1123), chr_rom_byte);
  ppu_bus.Write(0x2001, 0x31);
  ppu_bus.Write(0x2401, 0x32);
  ppu_bus.Write(0x2801, 0x33);
  ppu_bus.Write(0x2c01, 0x34);
  EXPECT_EQ(ppu_bus.Read(0x2001), 0x31);
  EXPECT_EQ(ppu_bus.Read(0x2401), 0x32);
  EXPECT_EQ(ppu_bus.Read(0x2801), 0x33);
  EXPECT_EQ(ppu_bus.Read(0x2c01), 0x34);

  mapper->WriteExtendedRAM(0x4100, 0);
  ppu_bus.UpdateMirroring();
  EXPECT_EQ(ppu_bus.Read(0x2001), 0x11);
  EXPECT_EQ(ppu_bus.Read(0x2801), 0x22);

  mapper->WriteExtendedRAM(0x4100, 3);
  ppu_bus.UpdateMirroring();
  EXPECT_EQ(ppu_bus.Read(0x0123), 0x5a);
  mapper->WriteExtendedRAM(0x4100, 1);
  ppu_bus.UpdateMirroring();
  EXPECT_EQ(ppu_bus.Read(0x2001), 0x31);
  EXPECT_EQ(ppu_bus.Read(0x2c01), 0x34);
}

TEST_F(Mapper512Test, DecodesMirroredModeRegister) {
  auto cartridge = LoadMapper512();
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x4100, 2);
  mapper->WriteCHR(0x0123, 0x5a);
  EXPECT_EQ(mapper->ReadCHR(0x1123), 0x5a);

  mapper->WriteExtendedRAM(0x4200, 0);
  EXPECT_EQ(mapper->ReadCHR(0x1123), 0x5a);

  mapper->WriteExtendedRAM(0x5f7f, 0);
  EXPECT_EQ(mapper->ReadCHR(0x1123), TestCHRByte(4 * k1K + 0x0123));
}

TEST_F(Mapper512Test, PersistsModeRAMMMC3AndResetState) {
  auto cartridge = LoadMapper512();
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteMMC3Register(mapper, 6, 3);
  mapper->WritePRG(0xa000, 0);
  mapper->WriteExtendedRAM(0x4100, 2);
  mapper->WriteCHR(0x0123, 0x5a);
  mapper->WriteExtendedRAM(0x6123, 0x6b);
  mapper->WriteExtendedRAM(0x4100, 1);
  mapper->WriteCHR(0x2456, 0x7c);
  mapper->WriteExtendedRAM(0x4100, 2);
  const Bytes state = SerializeMapper(mapper);

  mapper->WriteExtendedRAM(0x4100, 0);
  WriteMMC3Register(mapper, 6, 7);
  mapper->WriteExtendedRAM(0x6123, 0xa6);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x5a);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x6b);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(3 * k8K + 0x0123));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
  mapper->WriteExtendedRAM(0x4100, 1);
  EXPECT_EQ(mapper->ReadCHR(0x2456), 0x7c);

  mapper->Reset();
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  EXPECT_EQ(mapper->ReadCHR(0x0123), TestCHRByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x6b);
}

TEST_F(Mapper512Test, RendersChuugokuTaiteiFor600Frames) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::string expected_rom = "Chuugoku Taitei (Asia) (Unl).nes";
  size_t verified_roms = 0;
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(rom_directory)) {
    if (!entry.is_regular_file() ||
        entry.path().filename().string() != expected_rom) {
      continue;
    }

    bool loaded = false;
    emulator_->LoadFromFile(
        base::FilePath::FromUTF8Unsafe(entry.path().string()),
        base::BindOnce([](bool* loaded, bool success) { *loaded = success; },
                       base::Unretained(&loaded)));
    ASSERT_TRUE(loaded) << entry.path();
    ASSERT_TRUE(emulator_->GetRomData());
    EXPECT_EQ(static_cast<uint32_t>(emulator_->GetRomData()->crc), 0x037006f7u);
    EXPECT_EQ(static_cast<uint32_t>(emulator_->GetRomData()->mapper), 512u);

    emulator_->Run();
    Colors previous_pixels;
    bool rendered = false;
    bool frame_changed = false;
    size_t final_unique_colors = 0;
    for (int frame = 0; frame < 600; ++frame) {
      emulator_->RunOneFrame();
      const Colors& pixels = emulator_->GetLastFrame();
      ASSERT_EQ(pixels.size(), 256u * 240u) << entry.path();
      final_unique_colors =
          std::set<Color>(pixels.begin(), pixels.end()).size();
      rendered = rendered || final_unique_colors > 1;
      if (!previous_pixels.empty() && pixels != previous_pixels)
        frame_changed = true;
      previous_pixels = pixels;
    }

    EXPECT_TRUE(rendered) << entry.path();
    EXPECT_TRUE(frame_changed) << entry.path();
    EXPECT_GT(final_unique_colors, 1u) << entry.path();
    ++verified_roms;
  }

  EXPECT_EQ(verified_roms, 1u);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
