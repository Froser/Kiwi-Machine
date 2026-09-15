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
#include <fstream>
#include <set>
#include <string>

#include "nes/emulator_impl.h"

namespace kiwi {
namespace nes {
namespace testing {
namespace {

struct MapperNVRAMCase {
  Byte mapper;
  Byte prg_banks;
  Byte chr_banks;
  size_t expected_size;
};

constexpr size_t k8K = 0x2000;
constexpr size_t k64K = 0x10000;
constexpr std::array<Byte, 6> kBatteryMapperIds{{0, 1, 4, 5, 10, 74}};

void EnableMMC5SRAMWrites(Mapper* mapper, Byte bank = 0) {
  mapper->WriteExtendedRAM(0x5102, 0x02);
  mapper->WriteExtendedRAM(0x5103, 0x01);
  mapper->WriteExtendedRAM(0x5113, bank);
}

void SelectWritablePRGNVRAM(Mapper* mapper, Byte mapper_id) {
  if (mapper_id == 5) {
    EnableMMC5SRAMWrites(mapper);
  }
}

class MapperNVRAMCompatibilityTest
    : public MapperTest,
      public ::testing::WithParamInterface<MapperNVRAMCase> {};

TEST_P(MapperNVRAMCompatibilityTest, RoundTripsBatteryBackedPRGRAM) {
  const MapperNVRAMCase& test = GetParam();
  auto cartridge =
      LoadMapper(test.mapper, test.prg_banks, test.chr_banks, 0x02);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  SelectWritablePRGNVRAM(mapper, test.mapper);

  EXPECT_TRUE(mapper->HasBatteryBackedRAM());
  EXPECT_EQ(mapper->GetPRGNVRAMSize(), test.expected_size);
  EXPECT_FALSE(mapper->ExportPRGNVRAM(true));

  constexpr Address kTestAddress = 0x6123;
  constexpr Byte kTestValue = 0x5a;
  mapper->WriteExtendedRAM(kTestAddress, kTestValue);
  EXPECT_EQ(mapper->ReadExtendedRAM(kTestAddress), kTestValue);

  std::optional<Mapper::PRGNVRAMSnapshot> snapshot =
      mapper->ExportPRGNVRAM(true);
  ASSERT_TRUE(snapshot);
  EXPECT_EQ(snapshot->data.size(), test.expected_size);
  EXPECT_EQ(snapshot->data[kTestAddress - 0x6000], kTestValue);

  mapper->AcknowledgePRGNVRAMSaved(snapshot->generation);
  EXPECT_FALSE(mapper->IsPRGNVRAMDirty());

  auto restored_cartridge =
      LoadMapper(test.mapper, test.prg_banks, test.chr_banks, 0x02);
  ASSERT_TRUE(restored_cartridge);
  Mapper* restored_mapper = restored_cartridge->mapper();
  ASSERT_TRUE(restored_mapper->ImportPRGNVRAM(snapshot->data));
  SelectWritablePRGNVRAM(restored_mapper, test.mapper);
  EXPECT_EQ(restored_mapper->ReadExtendedRAM(kTestAddress), kTestValue);
  EXPECT_FALSE(restored_mapper->IsPRGNVRAMDirty());
}

INSTANTIATE_TEST_SUITE_P(
    SupportedBatteryMappers,
    MapperNVRAMCompatibilityTest,
    ::testing::Values(MapperNVRAMCase{0, 2, 1, k8K},
                      MapperNVRAMCase{1, 8, 1, k8K},
                      MapperNVRAMCase{4, 8, 8, k8K},
                      MapperNVRAMCase{5, 16, 8, k64K},
                      MapperNVRAMCase{10, 8, 16, k8K},
                      MapperNVRAMCase{74, 8, 2, k8K}),
    [](const ::testing::TestParamInfo<MapperNVRAMCase>& info) {
      return "Mapper" + std::to_string(info.param.mapper);
    });

class RealROMNVRAMAcceptanceTest : public MapperTest {};

TEST_F(RealROMNVRAMAcceptanceTest, RoundTripsSupportedBatteryMappers) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::set<Byte> expected_mappers(kBatteryMapperIds.begin(),
                                        kBatteryMapperIds.end());
  std::set<Byte> verified_mappers;
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(rom_directory)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".nes") {
      continue;
    }

    std::array<Byte, 16> header{};
    std::ifstream stream(entry.path(), std::ios::binary);
    stream.read(reinterpret_cast<char*>(header.data()), header.size());
    if (stream.gcount() != header.size() || header[0] != 'N' ||
        header[1] != 'E' || header[2] != 'S' || header[3] != 0x1a ||
        (header[6] & 0x02) == 0 || (header[6] & 0x04) != 0) {
      continue;
    }

    const Byte mapper_id =
        static_cast<Byte>((header[6] >> 4) | (header[7] & 0xf0));
    if (!expected_mappers.contains(mapper_id) ||
        verified_mappers.contains(mapper_id)) {
      continue;
    }

    auto cartridge = base::MakeRefCounted<Cartridge>(
        static_cast<EmulatorImpl*>(emulator_.get()));
    const Cartridge::LoadResult result =
        cartridge->Load(base::FilePath::FromUTF8Unsafe(entry.path().string()));
    if (!result.success) {
      continue;
    }
    cartridge->Reset();

    Mapper* mapper = cartridge->mapper();
    ASSERT_TRUE(mapper->HasBatteryBackedRAM()) << entry.path();
    ASSERT_GT(mapper->GetPRGNVRAMSize(), 0u) << entry.path();
    SelectWritablePRGNVRAM(mapper, mapper_id);

    constexpr Address kTestAddress = 0x6123;
    const Byte test_value =
        static_cast<Byte>(mapper->ReadExtendedRAM(kTestAddress) ^ 0xff);
    mapper->WriteExtendedRAM(kTestAddress, test_value);
    std::optional<Mapper::PRGNVRAMSnapshot> snapshot =
        mapper->ExportPRGNVRAM(true);
    ASSERT_TRUE(snapshot) << entry.path();
    EXPECT_EQ(mapper->ReadExtendedRAM(kTestAddress), test_value)
        << entry.path();

    auto restored_cartridge = base::MakeRefCounted<Cartridge>(
        static_cast<EmulatorImpl*>(emulator_.get()));
    ASSERT_TRUE(
        restored_cartridge
            ->Load(base::FilePath::FromUTF8Unsafe(entry.path().string()))
            .success)
        << entry.path();
    restored_cartridge->Reset();
    Mapper* restored_mapper = restored_cartridge->mapper();
    ASSERT_TRUE(restored_mapper->ImportPRGNVRAM(snapshot->data))
        << entry.path();
    SelectWritablePRGNVRAM(restored_mapper, mapper_id);
    EXPECT_EQ(restored_mapper->ReadExtendedRAM(kTestAddress), test_value)
        << entry.path();
    verified_mappers.insert(mapper_id);
  }

  EXPECT_EQ(verified_mappers, expected_mappers);
}

}  // namespace
}  // namespace testing
}  // namespace nes
}  // namespace kiwi
