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

#ifndef NES_CPU_H_
#define NES_CPU_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "nes/cpu_observer.h"
#include "nes/emulator_states.h"
#include "nes/opcodes.h"
#include "nes/registers.h"

namespace kiwi {
namespace nes {
class CPUBus;

// Class CPU represents 6502 Assembly RP2A03 Model CPU.
// For registers, see https://www.nesdev.org/wiki/CPU_registers for more
// details.
class CPU : public EmulatorStates::SerializableState {
 public:
  struct LastAction {
    Address last_address;
    int64_t cycles_to_wait;
  };

  // Three interrupts are supported: https://www.nesdev.org/wiki/CPU_interrupts
  enum class InterruptType {
    IRQ,
    NMI,
    BRK,
  };

 public:
  explicit CPU(CPUBus* cpu_bus);
  ~CPU() override;

 public:
  // Power up and reset states:
  // See https://www.nesdev.org/wiki/CPU_power_up_state for more details.
  void PowerUp();
  void Reset();
  void Interrupt(InterruptType type);
  // Step() should be called every cycle.
  void Step();
  void Step(int64_t cycles);
  void SkipDMACycles();

  CPURegisters registers() { return registers_; }
  LastAction get_last_action() {
    return LastAction{last_address_, cycles_to_skip_};
  }

  void increase_skip_cycle() { ++cycles_to_skip_; }

  void SetObserver(CPUObserver* observer);
  void RemoveObserver();

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 private:
  // Stack Operation.
  ALWAYS_INLINE void Push(Byte value);
  ALWAYS_INLINE Byte Pop();

  // Push next PC onto stack.
  ALWAYS_INLINE void PushNextPC();
  // Push current PC onto stack.
  ALWAYS_INLINE void PushPC();
  // Pop an address from the top of the stack, which takes 2 bytes, and write to
  // PC.
  ALWAYS_INLINE void PopPC();

  // Set Z flag and N flag indicated by |value|. |opcode| just check whether ZN
  // flags should be set.
  ALWAYS_INLINE void SetZN(Byte value);

  // Run Opcode.
  // See http://www.oxyron.de/html/opcodes02.html,
  // https://www.nesdev.org/6502_cpu.txt, and
  // https://www.nesdev.org/wiki/CPU_addressing_modes for more details.
  ALWAYS_INLINE bool Execute(Opcode opcode);

  // Devides opcodes into move, arithmetic, jump flag, and 4-blocks parts.
  ALWAYS_INLINE bool ExecuteMove(Opcode opcode);
  ALWAYS_INLINE bool ExecuteArithmetic(Opcode opcode);
  ALWAYS_INLINE bool ExecuteJumpFlags(Opcode opcode);

  // There are four blocks in
  // https://www.nesdev.org/wiki/CPU_unofficial_opcodes. Each instruction has
  // more than one opcode. We handle them by their block.
  ALWAYS_INLINE bool ExecuteBlock0(Byte opcode);
  ALWAYS_INLINE bool ExecuteBlock1(Byte opcode);
  ALWAYS_INLINE bool ExecuteBlock2(Byte opcode);
  ALWAYS_INLINE bool ExecuteBlock3(Byte opcode);

  ALWAYS_INLINE void InterruptSequence(InterruptType type);

  // Gets the target address by |mode|, in current context.
  ALWAYS_INLINE Address Addressing(AddressingMode mode, bool& is_page_crossed);

 private:
  CPUBus* cpu_bus_ = nullptr;
  CPURegisters registers_{};
  bool pending_NMI_ = false;
  bool pending_IRQ_ = false;
  int64_t cycles_to_skip_ = 0;

  // For debugging
  Address last_address_ = 0;
  bool has_break_ = false;
  bool should_break_ = false;
  CPUObserver* observer_ = nullptr;
};
}  // namespace nes
}  // namespace kiwi

#endif  // NES_CPU_H_
