// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper_test_support.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "nes/ppu_bus.h"
#include "third_party/nes_apu/Blip_Buffer.h"

namespace kiwi {
namespace nes {
namespace testing {
namespace {

constexpr size_t k8K = 0x2000;
constexpr size_t k1K = 0x0400;

void WriteAudioRAM(Mapper* mapper, Byte address, Byte value) {
  mapper->WritePRG(0xf800, address);
  mapper->WriteExtendedRAM(0x4800, value);
}

Byte ReadAudioRAM(Mapper* mapper, Byte address) {
  mapper->WritePRG(0xf800, address);
  return mapper->ReadExtendedRAM(0x4800);
}

template <size_t N>
bool ContainsCRC(const uint32_t (&values)[N], uint32_t crc) {
  return std::find(std::begin(values), std::end(values), crc) !=
         std::end(values);
}

void VerifyKnownROMConfiguration(const RomData& rom) {
  constexpr uint32_t kNamco175[] = {
      0x0c47946d,
      0x808606f0,
      0x81b7f1a8,
      0xc247cc80,
  };
  constexpr uint32_t kNamco340[] = {
      0x1dc0f740, 0x2447e03b, 0x429103c9, 0x46fd7843,
      0x6ec51de5, 0xadffd64f, 0xbd523011, 0xd323b806,
  };
  constexpr uint32_t kN163Submapper3[] = {
      0x098c672a,
      0x5746a461,
      0x684b292f,
      0x96773f32,
  };
  constexpr uint32_t kN163Submapper4[] = {0x2565786d};
  constexpr uint32_t kN163Submapper5[] = {
      0x35d8c961, 0x369da42d, 0xc811dc7a, 0xe64b8975, 0xef7996bf,
  };
  constexpr uint32_t kN163AudioOnlyNVRAM[] = {
      0x0c1792da, 0x10c8f2fa, 0x47c2020b, 0xace56f39, 0xb5ff71ab, 0xbc11e61a,
  };
  constexpr uint32_t kN163ExternalNVRAM[] = {
      0x098c672a, 0x369da42d, 0x96773f32, 0xcf23290f, 0xe64b8975,
  };

  const uint32_t crc = static_cast<uint32_t>(rom.crc);
  if (ContainsCRC(kNamco175, crc)) {
    EXPECT_EQ(rom.mapper, 210);
    EXPECT_EQ(rom.submapper, 1);
  } else if (ContainsCRC(kNamco340, crc)) {
    EXPECT_EQ(rom.mapper, 210);
    EXPECT_EQ(rom.submapper, 2);
  } else {
    EXPECT_EQ(rom.mapper, 19);
    Byte submapper = 2;
    if (ContainsCRC(kN163Submapper3, crc))
      submapper = 3;
    else if (ContainsCRC(kN163Submapper4, crc))
      submapper = 4;
    else if (ContainsCRC(kN163Submapper5, crc))
      submapper = 5;
    EXPECT_EQ(rom.submapper, submapper);
  }

  size_t nvram_size = 0;
  if (ContainsCRC(kN163AudioOnlyNVRAM, crc))
    nvram_size = 0x0080;
  else if (ContainsCRC(kN163ExternalNVRAM, crc))
    nvram_size = 0x2080;
  else if (crc == 0xc247cc80)
    nvram_size = 0x0800;
  EXPECT_EQ(rom.prg_nvram_size, nvram_size);
}

}  // namespace

class Mapper019Test : public MapperTest {};

TEST_F(Mapper019Test, MapsAllPRGAndCHRWindows) {
  auto cartridge = LoadMapper(19, 32, 32, 0, 3);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xe000, 3);
  mapper->WritePRG(0xe800, 4);
  mapper->WritePRG(0xf000, 5);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xa000), TestPRGByte(4 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(5 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xe000), TestPRGByte(63 * k8K));

  for (Byte window = 0; window < 8; ++window)
    mapper->WritePRG(0x8000 + window * 0x0800, 0x10 + window);
  for (size_t window = 0; window < 8; ++window) {
    const Address address = static_cast<Address>(window * k1K + 0x123);
    EXPECT_EQ(mapper->ReadCHR(address),
              TestCHRByte((0x10 + window) * k1K + 0x123));
    EXPECT_EQ(mapper->GetAbsoluteCHRAddress(address),
              (0x10 + window) * k1K + 0x123);
  }
}

TEST_F(Mapper019Test, RoutesPatternAndNametableWindowsToCHROrCIRAM) {
  auto cartridge = LoadMapper(19, 32, 32, 0, 3);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  PPUBus ppu_bus;
  ppu_bus.SetMapper(mapper);

  ASSERT_TRUE(mapper->UsesCustomPPUMemoryMapping());
  mapper->WritePRG(0x8000, 0xe0);
  ppu_bus.Write(0x0123, 0x5a);
  EXPECT_EQ(ppu_bus.Read(0x0123), 0x5a);

  mapper->WritePRG(0x8800, 0xe1);
  ppu_bus.Write(0x0456, 0xa5);
  EXPECT_EQ(ppu_bus.Read(0x0456), 0xa5);

  mapper->WritePRG(0xe800, 0x40);
  EXPECT_EQ(ppu_bus.Read(0x0123), TestCHRByte(0xe0 * k1K + 0x0123));

  mapper->WritePRG(0xc000, 0xe1);
  ppu_bus.Write(0x2001, 0x34);
  EXPECT_EQ(ppu_bus.Read(0x2001), 0x34);
  mapper->WritePRG(0xc800, 3);
  EXPECT_EQ(ppu_bus.Read(0x2456), TestCHRByte(3 * k1K + 0x56));
}

TEST_F(Mapper019Test, CountsUpIRQAndStopsAtTerminalValue) {
  auto cartridge = LoadMapper(19, 16, 16, 0, 3);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_TRUE(mapper->NeedsM2CycleIRQ());
  mapper->WriteExtendedRAM(0x5000, 0xfe);
  mapper->WriteExtendedRAM(0x5800, 0xff);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x5000), 0xff);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x5800), 0xff);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
}

TEST_F(Mapper019Test, AutoIncrementStopsAtLastAudioRAMByte) {
  auto cartridge = LoadMapper(19, 16, 16, 0, 3);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xf800, 0xfe);
  mapper->WriteExtendedRAM(0x4800, 0x12);
  mapper->WriteExtendedRAM(0x4800, 0x34);
  mapper->WriteExtendedRAM(0x4800, 0x56);
  EXPECT_EQ(ReadAudioRAM(mapper, 0x7e), 0x12);
  EXPECT_EQ(ReadAudioRAM(mapper, 0x7f), 0x56);
}

TEST_F(Mapper019Test, PersistsExternalAndInternalBatteryRAM) {
  auto cartridge = LoadMapper(19, 32, 32, 0x02, 3, 0, 0x70);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  ASSERT_EQ(mapper->GetPRGNVRAMSize(), 0x2080u);

  mapper->WritePRG(0xf800, 0x40);
  mapper->WriteExtendedRAM(0x6123, 0x5a);
  WriteAudioRAM(mapper, 5, 0xa5);
  auto snapshot = mapper->ExportPRGNVRAM(false);
  ASSERT_TRUE(snapshot);
  EXPECT_EQ(snapshot->data.size(), 0x2080u);

  auto restored_cartridge = LoadMapper(19, 32, 32, 0x02, 3, 0, 0x70);
  ASSERT_TRUE(restored_cartridge);
  Mapper* restored = restored_cartridge->mapper();
  ASSERT_TRUE(restored->ImportPRGNVRAM(snapshot->data));
  EXPECT_EQ(restored->ReadExtendedRAM(0x6123), 0x5a);
  EXPECT_EQ(ReadAudioRAM(restored, 5), 0xa5);
}

TEST_F(Mapper019Test, ImplementsNamco175RAMAndNamco340Mirroring) {
  auto n175_cartridge = LoadMapper(210, 32, 16, 0x02, 1, 0, 0x50);
  ASSERT_TRUE(n175_cartridge);
  Mapper* n175 = n175_cartridge->mapper();
  EXPECT_FALSE(n175->NeedsM2CycleIRQ());
  n175->WriteExtendedRAM(0x6123, 0x11);
  EXPECT_EQ(n175->ReadExtendedRAM(0x6123), 0);
  n175->WritePRG(0xc000, 1);
  n175->WriteExtendedRAM(0x6123, 0x5a);
  EXPECT_EQ(n175->ReadExtendedRAM(0x6923), 0x5a);

  auto n340_cartridge = LoadMapper(210, 32, 16, 0, 2);
  ASSERT_TRUE(n340_cartridge);
  Mapper* n340 = n340_cartridge->mapper();
  constexpr NametableMirroring kExpectedMirroring[] = {
      NametableMirroring::kOneScreenLower,
      NametableMirroring::kVertical,
      NametableMirroring::kOneScreenHigher,
      NametableMirroring::kHorizontal,
  };
  for (Byte mode = 0; mode < std::size(kExpectedMirroring); ++mode) {
    n340->WritePRG(0xe000, mode << 6);
    EXPECT_EQ(n340->GetNametableMirroring(), kExpectedMirroring[mode]);
  }
}

TEST_F(Mapper019Test, ProducesExpansionAudioForAudioSubmappers) {
  auto cartridge = LoadMapper(19, 16, 16, 0, 3);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  Blip_Buffer buffer;
  ASSERT_EQ(buffer.sample_rate(44100, 50), nullptr);
  buffer.clock_rate(1789773);
  mapper->SetExpansionAudioOutput(&buffer);
  mapper->SetExpansionAudioVolume(1.f);

  WriteAudioRAM(mapper, 0x00, 0xff);
  WriteAudioRAM(mapper, 0x78, 0xff);
  WriteAudioRAM(mapper, 0x7a, 0xff);
  WriteAudioRAM(mapper, 0x7c, 0x03);
  WriteAudioRAM(mapper, 0x7e, 0x00);
  WriteAudioRAM(mapper, 0x7f, 0x0f);
  mapper->EndExpansionAudioFrame(30000);
  buffer.end_frame(30000);

  std::vector<blip_sample_t> samples(buffer.samples_avail());
  ASSERT_GT(samples.size(), 0u);
  buffer.read_samples(samples.data(), samples.size());
  EXPECT_TRUE(std::any_of(samples.begin(), samples.end(),
                          [](blip_sample_t sample) { return sample != 0; }));
}

TEST_F(Mapper019Test, RestoresMapperAndAudioState) {
  auto cartridge = LoadMapper(19, 32, 32, 0, 3);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xe000, 3);
  mapper->WritePRG(0x8000, 7);
  mapper->WritePRG(0xc000, 5);
  mapper->WriteExtendedRAM(0x5000, 0xfe);
  mapper->WriteExtendedRAM(0x5800, 0xff);
  WriteAudioRAM(mapper, 9, 0x6d);
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0xe000, 1);
  mapper->WritePRG(0x8000, 2);
  mapper->WritePRG(0xc000, 4);
  mapper->WriteExtendedRAM(0x5000, 0);
  mapper->WriteExtendedRAM(0x5800, 0);
  WriteAudioRAM(mapper, 9, 0x22);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k8K));
  EXPECT_EQ(mapper->ReadCHR(0x0123), TestCHRByte(7 * k1K + 0x123));
  EXPECT_EQ(ReadAudioRAM(mapper, 9), 0x6d);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
}

TEST_F(Mapper019Test, RendersAllAcceptanceROMs) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  size_t verified_roms = 0;
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(rom_directory)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".nes")
      continue;

    std::array<Byte, 16> header{};
    std::ifstream stream(entry.path(), std::ios::binary);
    stream.read(reinterpret_cast<char*>(header.data()), header.size());
    if (stream.gcount() != header.size() || header[0] != 'N' ||
        header[1] != 'E' || header[2] != 'S' || header[3] != 0x1a) {
      continue;
    }

    const Byte mapper_id =
        static_cast<Byte>((header[6] >> 4) | (header[7] & 0xf0));
    if (mapper_id != 19)
      continue;

    bool loaded = false;
    emulator_->LoadFromFile(
        base::FilePath::FromUTF8Unsafe(entry.path().string()),
        base::BindOnce([](bool* loaded, bool success) { *loaded = success; },
                       base::Unretained(&loaded)));
    ASSERT_TRUE(loaded) << entry.path();
    ASSERT_TRUE(emulator_->GetRomData());
    VerifyKnownROMConfiguration(*emulator_->GetRomData());

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

  EXPECT_EQ(verified_roms, 32u);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
