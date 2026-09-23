// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper_test_support.h"

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
constexpr size_t k32K = 0x8000;
constexpr size_t k4K = 0x1000;

void SetPRGByte(Bytes* rom, size_t bank, Address address, Byte value) {
  ASSERT_TRUE(rom);
  const size_t index = kINESHeaderSize + bank * k32K + (address & 0x7fff);
  ASSERT_LT(index, rom->size());
  (*rom)[index] = value;
}

}  // namespace

class Mapper034Test : public MapperTest {
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

TEST_F(Mapper034Test, CreatesBNROMForCHRRAMImages) {
  ASSERT_TRUE(Mapper::IsMapperSupported(34));
  auto cartridge = LoadMapper(34, 8, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(k32K - 1));

  mapper->WriteCHR(0x0456, 0x5a);
  EXPECT_EQ(mapper->ReadCHR(0x0456), 0x5a);
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0456), 0x0456u);
}

TEST_F(Mapper034Test, AppliesBNROMBusConflictsAndRestoresState) {
  Bytes rom = MakeTestROM(34, 8, 0);
  SetPRGByte(&rom, 0, 0x80f1, 0x01);
  SetPRGByte(&rom, 1, 0x80ff, 0xff);
  auto cartridge = LoadCustomROM(rom);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x80f1, 0x03);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(k32K + 0x0123));
  mapper->WriteCHR(0x0123, 0x6d);
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0x80ff, 0x03);
  mapper->WriteCHR(0x0123, 0xa5);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(3 * k32K + 0x0123));

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(k32K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x6d);

  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x6d);
}

TEST_F(Mapper034Test, CreatesNINA001ForCHRROMImagesAndMapsRegisters) {
  auto cartridge = LoadMapper(34, 4, 8, 0x01);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  ASSERT_TRUE(mapper->HasPRGRAM());
  mapper->WriteExtendedRAM(0x6000, 0x5a);
  mapper->WriteExtendedRAM(0x7ffd, 0x01);
  mapper->WriteExtendedRAM(0x7ffe, 0x03);
  mapper->WriteExtendedRAM(0x7fff, 0x07);

  EXPECT_EQ(mapper->ReadExtendedRAM(0x6000), 0x5a);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x7ffd), 0x01);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x7ffe), 0x03);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x7fff), 0x07);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(k32K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(3 * k4K + 0x0456));
  EXPECT_EQ(mapper->ReadCHR(0x1456), TestCHRByte(7 * k4K + 0x0456));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0456), 3u * k4K + 0x0456);
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x1456), 7u * k4K + 0x0456);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);

  mapper->WritePRG(0x8000, 0xff);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(k32K + 0x0123));
}

TEST_F(Mapper034Test, ResetsAndRestoresNINA001BanksAndWorkRAM) {
  auto cartridge = LoadMapper(34, 4, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x6000, 0x5a);
  mapper->WriteExtendedRAM(0x7ffd, 0x01);
  mapper->WriteExtendedRAM(0x7ffe, 0x02);
  mapper->WriteExtendedRAM(0x7fff, 0x05);
  const Bytes state = SerializeMapper(mapper);

  mapper->WriteExtendedRAM(0x6000, 0xa5);
  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(0x0456));
  EXPECT_EQ(mapper->ReadCHR(0x1456), TestCHRByte(0x0456));
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6000), 0xa5);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(k32K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(2 * k4K + 0x0456));
  EXPECT_EQ(mapper->ReadCHR(0x1456), TestCHRByte(5 * k4K + 0x0456));
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6000), 0x5a);
}

TEST_F(Mapper034Test, RendersAllAcceptanceROMs) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::set<std::string> expected_roms = {
      "Deadly Towers (USA).nes",
      "Impossible Mission II (USA) (Unl).nes",
      "Mashou (Japan).nes",
  };

  size_t verified_roms = 0;
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(rom_directory)) {
    if (!entry.is_regular_file() ||
        !expected_roms.contains(entry.path().filename().string())) {
      continue;
    }

    bool loaded = false;
    emulator_->LoadFromFile(
        base::FilePath::FromUTF8Unsafe(entry.path().string()),
        base::BindOnce([](bool* loaded, bool success) { *loaded = success; },
                       base::Unretained(&loaded)));
    ASSERT_TRUE(loaded) << entry.path();
    ASSERT_TRUE(emulator_->GetRomData());
    ASSERT_EQ(emulator_->GetRomData()->mapper, 34) << entry.path();

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

  EXPECT_EQ(verified_roms, expected_roms.size());
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
