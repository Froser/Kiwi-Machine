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

#include <array>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "nes/emulator_impl.h"

namespace kiwi {
namespace nes {
namespace testing {
namespace {

constexpr size_t kINESHeaderSize = 0x10;
constexpr size_t k8K = 0x2000;
constexpr size_t k2K = 0x0800;

void SetPRGByte(Bytes* rom, size_t bank, Address address, Byte value) {
  ASSERT_TRUE(rom);
  const size_t index = kINESHeaderSize + bank * k8K + (address & 0x1fff);
  ASSERT_LT(index, rom->size());
  (*rom)[index] = value;
}

}  // namespace

class Mapper246Test : public MapperTest {
 protected:
  scoped_refptr<Cartridge> LoadCustomROM(const Bytes& rom) {
    auto cartridge = base::MakeRefCounted<Cartridge>(
        static_cast<EmulatorImpl*>(emulator_.get()));
    const Cartridge::LoadResult result = cartridge->Load(rom);
    EXPECT_TRUE(result.success);
    if (!result.success)
      return nullptr;
    cartridge->mapper()->Reset();
    return cartridge;
  }
};

TEST_F(Mapper246Test, SelectsFourProgramAndCharacterBanks) {
  ASSERT_TRUE(Mapper::IsMapperSupported(246));
  auto cartridge = LoadMapper(246, 32, 64);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  for (Byte slot = 0; slot < 4; ++slot) {
    mapper->WriteExtendedRAM(static_cast<Address>(0x6000 + slot), slot + 1);
    mapper->WriteExtendedRAM(static_cast<Address>(0x6004 + slot), slot + 4);
  }

  for (Byte slot = 0; slot < 4; ++slot) {
    const Address address = static_cast<Address>(0x8000 + slot * k8K + 0x0123);
    EXPECT_EQ(mapper->ReadPRG(address), TestPRGByte((slot + 1) * k8K + 0x0123));
    const Address chr_address = static_cast<Address>(slot * k2K + 0x0456);
    EXPECT_EQ(mapper->ReadCHR(chr_address),
              TestCHRByte((slot + 4) * k2K + 0x0456));
  }
}

TEST_F(Mapper246Test, ForcesProgramA17ForResetVectorReads) {
  Bytes rom = MakeTestROM(246, 32, 64);
  constexpr std::array<Address, 4> kForcedAddresses = {0xffe4, 0xffec, 0xfff4,
                                                       0xfffc};
  for (Address address : kForcedAddresses) {
    SetPRGByte(&rom, 0, address, 0xa0);
    SetPRGByte(&rom, 16, address, 0xb0);
  }
  SetPRGByte(&rom, 0, 0xffe3, 0xa3);
  SetPRGByte(&rom, 16, 0xffe3, 0xb3);
  auto cartridge = LoadCustomROM(rom);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x6003, 0);
  EXPECT_EQ(mapper->ReadPRG(0xffe3), 0xa3);
  for (Address address : kForcedAddresses)
    EXPECT_EQ(mapper->ReadPRG(address), 0xb0);
}

TEST_F(Mapper246Test, PersistsBanksAndMapsOnlyTwoKiBOfProgramRAM) {
  auto cartridge = LoadMapper(246, 32, 64, 0x02);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x6000, 2);
  mapper->WriteExtendedRAM(0x6004, 3);
  mapper->WriteExtendedRAM(0x6800, 0xa5);
  mapper->WriteExtendedRAM(0x6fff, 0x5a);
  mapper->WriteExtendedRAM(0x7000, 0xff);
  const Bytes state = SerializeMapper(mapper);

  mapper->WriteExtendedRAM(0x6000, 1);
  mapper->WriteExtendedRAM(0x6004, 1);
  mapper->WriteExtendedRAM(0x6800, 0);
  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(2 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(3 * k2K + 0x0456));
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6800), 0xa5);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6fff), 0x5a);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x7000), 0x70);

  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(0x0456));
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6800), 0xa5);
}

TEST_F(Mapper246Test, RendersFengShenBang) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::string expected_rom = "Feng Shen Bang (Asia) (Unl).nes";
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
    ASSERT_EQ(emulator_->GetRomData()->mapper, 246) << entry.path();

    emulator_->Run();
    bool rendered = false;
    for (int frame = 0; frame < 600; ++frame) {
      emulator_->RunOneFrame();
      const Colors& pixels = emulator_->GetLastFrame();
      ASSERT_EQ(pixels.size(), 256u * 240u) << entry.path();
      const std::set<Color> unique_colors(pixels.begin(), pixels.end());
      if (unique_colors.size() > 1)
        rendered = true;
    }
    EXPECT_TRUE(rendered) << entry.path();
    ++verified_roms;
  }

  EXPECT_EQ(verified_roms, 1u);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
