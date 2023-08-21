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

#ifndef NES_EMULATOR_STATE_H_
#define NES_EMULATOR_STATE_H_

#include <cstring>
#include <type_traits>
#include <vector>

#include "nes/nes_export.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {
class EmulatorImpl;
class EmulatorStates {
 public:
  struct Header {
    char header[16];
    uint32_t version;
  };
  static_assert(sizeof(Header) == 20);

  class SerializableStateData {
   public:
    SerializableStateData() = default;
    virtual ~SerializableStateData() = default;

    virtual SerializableStateData& WriteData(const void* data, size_t size) = 0;

    template <typename T>
    SerializableStateData& WriteData(const T& data) {
      static_assert(std::is_trivial_v<T> == true);
      static_assert(std::is_pointer_v<T> == false);
      return WriteData(&data, sizeof(T));
    }

    template <typename T>
    SerializableStateData& WriteData(const std::vector<T>& data) {
      return WriteData(data.data(), data.size());
    }
  };

  class DeserializableStateData {
   public:
    DeserializableStateData() = default;
    virtual ~DeserializableStateData() = default;

    virtual Bytes ReadData(size_t size) = 0;

    template <typename T>
    DeserializableStateData& ReadData(T* data) {
      static_assert(std::is_trivial_v<T> == true);
      Bytes t = ReadData(sizeof(T));
      std::memcpy(data, t.data(), t.size());
      return *this;
    }

    template <typename T>
    DeserializableStateData& ReadData(std::vector<T>* data) {
      Bytes t = ReadData(data->size());
      *data = std::move(t);
      return *this;
    }
  };

  class SerializableState {
   public:
    SerializableState() = default;
    virtual ~SerializableState() = default;
    virtual void Serialize(SerializableStateData& data) = 0;

    // If any components return false, deserialize will be terminated
    virtual bool Deserialize(const Header& header,
                             DeserializableStateData& data) = 0;
  };

 public:
  static EmulatorStates CreateStateForVersion(EmulatorImpl* impl,
                                              uint32_t version);

  Bytes Build();
  bool Restore(const Bytes& data);

 private:
  bool RestoreInternal(const Bytes& data);

 public:
  ~EmulatorStates();

 private:
  EmulatorStates(uint32_t version);

  EmulatorStates& AddComponent(SerializableState* component);

 private:
  uint32_t version_ = 0;
  std::vector<SerializableState*> components_;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_EMULATOR_STATE_H_
