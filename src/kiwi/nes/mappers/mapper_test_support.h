// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef NES_MAPPERS_MAPPER_TEST_SUPPORT_H_
#define NES_MAPPERS_MAPPER_TEST_SUPPORT_H_

#include <cstddef>
#include <memory>

#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_executor.h"
#include "nes/cartridge.h"
#include "nes/emulator.h"
#include "nes/emulator_states.h"
#include "nes/mapper.h"
#include "nes/types.h"
#include "third_party/googletest-release-1.12.1/googletest/include/gtest/gtest.h"

namespace kiwi {
namespace nes {
namespace testing {

Byte TestPRGByte(size_t index);
Byte TestCHRByte(size_t index);
Bytes MakeTestROM(Byte mapper,
                  Byte prg_banks,
                  Byte chr_banks,
                  Byte flags6 = 0,
                  Byte submapper = 0);

class VectorStateWriter final : public EmulatorStates::SerializableStateData {
 public:
  SerializableStateData& WriteData(const void* data, size_t size) override;
  const Bytes& data() const;

 private:
  Bytes data_;
};

class VectorStateReader final : public EmulatorStates::DeserializableStateData {
 public:
  explicit VectorStateReader(const Bytes& data);

  Bytes ReadData(size_t size) override;
  size_t bytes_read() const;

 private:
  const Bytes& data_;
  size_t offset_ = 0;
};

Bytes SerializeMapper(Mapper* mapper);
bool DeserializeMapper(Mapper* mapper, const Bytes& state);
void WriteMMC1Register(Mapper* mapper, Address address, Byte value);
void WriteMMC3Register(Mapper* mapper,
                       Byte target,
                       Byte value,
                       Byte mode_flags = 0);

class MapperTest : public ::testing::Test {
 protected:
  void SetUp() override;
  void TearDown() override;

  scoped_refptr<Cartridge> LoadMapper(Byte mapper,
                                      Byte prg_banks,
                                      Byte chr_banks,
                                      Byte flags6 = 0,
                                      Byte submapper = 0);

  int mirroring_changes_ = 0;
  int irq_count_ = 0;
  std::unique_ptr<base::SingleThreadTaskExecutor> task_executor_;
  scoped_refptr<Emulator> emulator_;
};

}  // namespace testing
}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER_TEST_SUPPORT_H_
