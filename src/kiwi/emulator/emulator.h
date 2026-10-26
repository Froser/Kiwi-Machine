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

#ifndef EMULATOR_EMULATOR_H_
#define EMULATOR_EMULATOR_H_

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "emulator/emulator_export.h"
#include "emulator/types.h"

namespace kiwi {
namespace gba {
class Emulator;
}

namespace nes {
class Emulator;
}

namespace emu {

class EMULATOR_EXPORT Emulator : public base::RefCountedThreadSafe<Emulator> {
 public:
  using LoadCallback = base::OnceCallback<void(bool)>;
  using UnloadCallback = base::OnceClosure;
  using ResetCallback = base::OnceClosure;
  using SaveStateCallback = base::OnceCallback<void(Bytes)>;

  enum class Type {
    kNES,
    kGBA,
  };

  Type GetType() const { return type_; }

  virtual void PowerOn() = 0;
  virtual void PowerOff() = 0;
  virtual void Run() = 0;
  virtual void RunOneFrame() = 0;
  virtual void Pause() = 0;
  virtual void Reset(ResetCallback reset_callback) = 0;
  virtual bool IsRunning() = 0;
  virtual bool IsPaused() = 0;
  virtual bool HasROM() = 0;
  virtual void SetVolume(float volume) = 0;
  virtual float GetVolume() = 0;

  nes::Emulator* AsNESEmulator();
  const nes::Emulator* AsNESEmulator() const;
  gba::Emulator* AsGBAEmulator();
  const gba::Emulator* AsGBAEmulator() const;

 protected:
  explicit Emulator(Type type);
  virtual ~Emulator();

 private:
  friend class base::RefCountedThreadSafe<Emulator>;

  const Type type_;
};

}  // namespace emu
}  // namespace kiwi

#endif  // EMULATOR_EMULATOR_H_
