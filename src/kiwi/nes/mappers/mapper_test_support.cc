// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper_test_support.h"

#include "base/functional/bind.h"
#include "nes/emulator_impl.h"

namespace kiwi {
namespace nes {
namespace testing {
namespace {

constexpr size_t kINESHeaderSize = 16;
constexpr size_t kPRGROMBankSize = 16 * 1024;
constexpr size_t kCHRROMBankSize = 8 * 1024;

}  // namespace

Byte TestPRGByte(size_t index) {
  return static_cast<Byte>(((index >> 10) * 29 + (index & 0xff)) & 0xff);
}

Byte TestCHRByte(size_t index) {
  return static_cast<Byte>((0x80 + (index >> 10) * 17 + (index & 0xff)) & 0xff);
}

Bytes MakeTestROM(Byte mapper,
                  Byte prg_banks,
                  Byte chr_banks,
                  Byte flags6,
                  Byte submapper) {
  const size_t prg_size = static_cast<size_t>(prg_banks) * kPRGROMBankSize;
  const size_t chr_size = static_cast<size_t>(chr_banks) * kCHRROMBankSize;
  Bytes rom(kINESHeaderSize + prg_size + chr_size);

  rom[0] = 'N';
  rom[1] = 'E';
  rom[2] = 'S';
  rom[3] = 0x1a;
  rom[4] = prg_banks;
  rom[5] = chr_banks;
  rom[6] = static_cast<Byte>(((mapper & 0x0f) << 4) | (flags6 & 0x0f));
  rom[7] = mapper & 0xf0;
  rom[8] = static_cast<Byte>(submapper << 4);

  for (size_t i = 0; i < prg_size; ++i)
    rom[kINESHeaderSize + i] = TestPRGByte(i);
  for (size_t i = 0; i < chr_size; ++i)
    rom[kINESHeaderSize + prg_size + i] = TestCHRByte(i);

  return rom;
}

EmulatorStates::SerializableStateData& VectorStateWriter::WriteData(
    const void* data,
    size_t size) {
  const auto* bytes = static_cast<const Byte*>(data);
  data_.insert(data_.end(), bytes, bytes + size);
  return *this;
}

const Bytes& VectorStateWriter::data() const {
  return data_;
}

VectorStateReader::VectorStateReader(const Bytes& data) : data_(data) {}

Bytes VectorStateReader::ReadData(size_t size) {
  if (offset_ + size > data_.size()) {
    ADD_FAILURE() << "Mapper state read exceeded serialized data";
    return Bytes(size);
  }

  Bytes result(data_.begin() + offset_, data_.begin() + offset_ + size);
  offset_ += size;
  return result;
}

size_t VectorStateReader::bytes_read() const {
  return offset_;
}

Bytes SerializeMapper(Mapper* mapper) {
  VectorStateWriter writer;
  mapper->Serialize(writer);
  return writer.data();
}

bool DeserializeMapper(Mapper* mapper, const Bytes& state) {
  EmulatorStates::Header header{};
  header.version = 1;
  VectorStateReader reader(state);
  const bool result = mapper->Deserialize(header, reader);
  EXPECT_EQ(reader.bytes_read(), state.size());
  return result;
}

void WriteMMC1Register(Mapper* mapper, Address address, Byte value) {
  for (int bit = 0; bit < 5; ++bit)
    mapper->WritePRG(address, (value >> bit) & 1);
}

void WriteMMC3Register(Mapper* mapper,
                       Byte target,
                       Byte value,
                       Byte mode_flags) {
  mapper->WritePRG(0x8000, static_cast<Byte>(target | mode_flags));
  mapper->WritePRG(0x8001, value);
}

void MapperTest::SetUp() {
  task_executor_ = std::make_unique<base::SingleThreadTaskExecutor>();
  emulator_ = CreateEmulatorForTesting();
  ASSERT_TRUE(emulator_);
  emulator_->PowerOn();
}

void MapperTest::TearDown() {
  if (emulator_)
    emulator_->PowerOff();
}

scoped_refptr<Cartridge> MapperTest::LoadMapper(Byte mapper,
                                                Byte prg_banks,
                                                Byte chr_banks,
                                                Byte flags6,
                                                Byte submapper) {
  auto cartridge = base::MakeRefCounted<Cartridge>(
      static_cast<EmulatorImpl*>(emulator_.get()));
  const Cartridge::LoadResult result = cartridge->Load(
      MakeTestROM(mapper, prg_banks, chr_banks, flags6, submapper));
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

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
