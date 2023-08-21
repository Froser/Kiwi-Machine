// Copyright (C) 2023 Yisi Yu
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

#include "nes/emulator_states.h"

#include "base/strings/string_piece.h"
#include "nes/cpu.h"
#include "nes/cpu_bus.h"
#include "nes/emulator_impl.h"
#include "nes/ppu.h"
#include "nes/ppu_bus.h"

namespace kiwi {
namespace nes {
namespace {
#define kStateHeaderSignature "KIWI_NES_STATES"

class SerializableStateDataImpl : public EmulatorStates::SerializableStateData {
 public:
  SerializableStateDataImpl(Bytes& data);
  ~SerializableStateDataImpl();

 public:
  SerializableStateData& WriteData(const void* data, size_t size) override;

 private:
  Bytes& data_ref_;
};

SerializableStateDataImpl::SerializableStateDataImpl(Bytes& data)
    : data_ref_(data) {}
SerializableStateDataImpl::~SerializableStateDataImpl() = default;

EmulatorStates::SerializableStateData& SerializableStateDataImpl::WriteData(
    const void* data,
    size_t size) {
  for (size_t i = 0; i < size; ++i) {
    data_ref_.push_back(static_cast<const Byte*>(data)[i]);
  }

  return *this;
}

class DeserializableStateDataImpl
    : public EmulatorStates::DeserializableStateData {
 public:
  DeserializableStateDataImpl(const Bytes& data);
  ~DeserializableStateDataImpl();

 public:
  Bytes ReadData(size_t size) override;

 private:
  const Bytes& data_ref_;
  size_t index_ = 0;
};

DeserializableStateDataImpl::DeserializableStateDataImpl(const Bytes& data)
    : data_ref_(data) {}

DeserializableStateDataImpl::~DeserializableStateDataImpl() = default;

Bytes DeserializableStateDataImpl::ReadData(size_t size) {
  Bytes read;
  if (!size)
    return Bytes();

  read.resize(size);
  memcpy(read.data(), data_ref_.data() + index_, size);
  index_ += size;
  return read;
}
}  // namespace

EmulatorStates EmulatorStates::CreateStateForVersion(EmulatorImpl* impl,
                                                     uint32_t version) {
  CHECK(version == 1);  // Only version 1 is supported yet.
  EmulatorStates state(version);
  state.AddComponent(impl->cartridge_.get())
      .AddComponent(impl->cpu_.get())
      .AddComponent(impl->cpu_bus_.get())
      .AddComponent(impl->ppu_.get())
      .AddComponent(impl->ppu_bus_.get())
      .AddComponent(impl->apu_.get());
  return state;
}

EmulatorStates::EmulatorStates(uint32_t version) : version_(version) {}
EmulatorStates::~EmulatorStates() = default;

EmulatorStates& EmulatorStates::AddComponent(SerializableState* component) {
  components_.push_back(component);
  return *this;
}

Bytes EmulatorStates::Build() {
  Header header = {kStateHeaderSignature, version_};

  Bytes data;
  SerializableStateDataImpl serializable(data);
  serializable.SerializableStateData::WriteData(header);

  for (const auto& component : components_) {
    component->Serialize(serializable);
  }

  return data;
}

bool EmulatorStates::Restore(const Bytes& data) {
  Bytes backup = Build();  // Make a backup
  if (data.size() != backup.size()) {
    // Wrong size, perhaps state is not saved yet, or data are corrupted / not
    // compatible.
    return false;
  }

  bool success = RestoreInternal(data);
  if (!success) {
    // Fallback if load state failed.
    CHECK(RestoreInternal(backup));
    return false;
  }

  return true;
}

bool EmulatorStates::RestoreInternal(const kiwi::nes::Bytes& data) {
  bool success = true;
  DeserializableStateDataImpl deserializable(data);

  // Restore header first
  Header header = {0};
  deserializable.DeserializableStateData::ReadData<Header>(&header);

  if (base::StringPiece(header.header) != kStateHeaderSignature) {
    LOG(WARNING) << "Wrong state header signature: " << header.header;
    success = false;
  } else {
    LOG(INFO) << "Load state header success, version: " << header.version;
  }

  if (success) {
    for (const auto& component : components_) {
      success = component->Deserialize(header, deserializable);
      if (!success) {
        LOG(WARNING) << "Load state failed.";
        break;
      }
    }
  }

  return success;
}

}  // namespace nes
}  // namespace kiwi
