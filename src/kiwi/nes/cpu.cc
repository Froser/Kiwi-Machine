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

#include "nes/cpu.h"

#include <iomanip>
#include <set>

#include "base/logging.h"
#include "nes/cpu_bus.h"
#include "nes/mapper.h"
#include "nes/opcodes.h"
#include "nes/registers.h"

namespace kiwi {
namespace nes {
namespace {
// The CPU expects interrupt vectors in a fixed place at the end of the
// cartridge space:
// $FFFA-$FFFB = NMI vector
// $FFFC-$FFFD = Reset vector
// $FFFE-$FFFF = IRQ/BRK vector
// See https://www.nesdev.org/wiki/CPU_interrupts for more details.
constexpr Address kNMIVector = 0xfffa;
constexpr Address kResetVector = 0xfffc;
constexpr Address kIRQVector = 0xfffe;

// Stack always uses some part of the $0100-$01FF page.
constexpr Address kStackBase = 0x0100;

// Masks to extract addressing mode and operation type.
constexpr Byte kInstructionModeMask = 0x03;
constexpr Byte kAddressModeMask = 0x1c;
constexpr Byte kAddressModeShift = 2;
constexpr Byte kOperationMask = 0xe0;
constexpr Byte kOperationShift = 5;

#define IS_OPCODE_IN_BLOCK(opcode, block) \
  (((opcode)&kInstructionModeMask) == (block))

#define OPCODE_ROW_IN_BLOCK(opcode) \
  (((opcode)&kOperationMask) >> kOperationShift)

#define OPCODE_ADDRESS_MODE_IN_BLOCK(opcode) \
  (((opcode)&kAddressModeMask) >> kAddressModeShift)

#define IS_CROSSING_PAGE(a, b) ((a & 0xff00) != (b & 0xff00))
}  // namespace

// Define ENABLE_DCHECK_OPCODE to enable debugging opcode checking runtime. It
// will slow down CPU, and should be disabled after stable.
#if defined(ENABLE_DCHECK_OPCODE)
#define DCHECK_OPCODE_BY_NAME(opcode, name) \
  DCHECK(strcmp(GetOpcodeName(opcode), #name) == 0)
#define DCHECK_OPCODE_ADDRESSING_MODE(opcode, mode) \
  DCHECK_EQ(GetOpcodeAddressingMode(opcode), mode)
#else
#define DCHECK_OPCODE_BY_NAME(opcode, name)
#define DCHECK_OPCODE_ADDRESSING_MODE(opcode, mode)
#endif

CPU::CPU(CPUBus* cpu_bus) : cpu_bus_(cpu_bus) {
  DCHECK(cpu_bus_);
}

CPU::~CPU() = default;

void CPU::PowerUp() {
  registers_.P.value = 0x34;
  registers_.A = registers_.X = registers_.Y = 0;
  registers_.S = 0xfd;
}

void CPU::Reset() {
  PowerUp();
  registers_.PC = cpu_bus_->ReadWord(kResetVector);
  has_break_ = false;
}

void CPU::Interrupt(InterruptType type) {
  switch (type) {
    case InterruptType::NMI:
      pending_NMI_ = true;
      break;
    case InterruptType::IRQ:
      pending_IRQ_ = true;
      break;
    default:
      break;
  }

  if (type == InterruptType::NMI) {
    if (observer_)
      observer_->OnCPUNMI();
  }
}

void CPU::Step() {
  struct M2CylceIRQNotifier {
    M2CylceIRQNotifier(CPUBus* cpu_bus) : cpu_bus_(cpu_bus) {}
    ~M2CylceIRQNotifier() { cpu_bus_->GetMapper()->M2CycleIRQ(); }
    CPUBus* cpu_bus_;
  } notifier(cpu_bus_);

  if (--cycles_to_skip_ >= 0) {
    if (observer_)
      observer_->OnCPUStepped();
    return;
  }
  cycles_to_skip_ = 0;

  // Handle NMI first because it has higher priority.
  if (pending_NMI_) {
    InterruptSequence(InterruptType::NMI);
    pending_NMI_ = pending_IRQ_ = false;
    if (observer_)
      observer_->OnCPUStepped();
    return;
  } else if (pending_IRQ_) {
    InterruptSequence(InterruptType::IRQ);
    pending_NMI_ = pending_IRQ_ = false;
    if (observer_)
      observer_->OnCPUStepped();
    return;
  }

  last_address_ = registers_.PC;
  if (observer_) {
    CPUDebugState state;
    observer_->OnCPUBeforeStep(state);
    if (state.should_break) {
      // To ensure we only break once for one instruction.
      if (!has_break_) {
        has_break_ = true;
        return;
      } else {
        has_break_ = false;
      }
    }
  }

  Opcode opcode = static_cast<Opcode>(cpu_bus_->Read(registers_.PC));

  // opcode has been fetched. Now PC goes to the next address.
  ++registers_.PC;

  int64_t cycle_length = GetOpcodeCycle(static_cast<uint8_t>(opcode));
  DCHECK(cycle_length > 0);
  if (cycle_length && Execute(opcode)) {
    cycles_to_skip_ += (cycle_length - 1);  // One cycle has been spent on this
  } else {
    LOG(ERROR) << "Opcode not handled: "
               << GetOpcodeName(static_cast<uint8_t>(opcode)) << " ($"
               << Hex<8>{static_cast<uint8_t>(opcode)} << ")";
  }

  if (observer_)
    observer_->OnCPUStepped();
}

void CPU::Step(int64_t cycles) {
  for (int64_t i = 0; i < cycles; ++i) {
    Step();
  }
}

void CPU::SkipDMACycles() {
  // https://www.nesdev.org/wiki/Cycle_reference_chart
  cycles_to_skip_ += 513;
  cycles_to_skip_ += (cycles_to_skip_ & 1);
}

void CPU::SetObserver(CPUObserver* observer) {
  observer_ = observer;
}

void CPU::RemoveObserver() {
  observer_ = nullptr;
}

void CPU::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(registers_)
      .WriteData(pending_NMI_)
      .WriteData(pending_IRQ_)
      .WriteData(cycles_to_skip_)
      .WriteData(last_address_);
}

bool CPU::Deserialize(const EmulatorStates::Header& header,
                      EmulatorStates::DeserializableStateData& data) {
  if (header.version == 1) {
    data.ReadData(&registers_)
        .ReadData(&pending_NMI_)
        .ReadData(&pending_IRQ_)
        .ReadData(&cycles_to_skip_)
        .ReadData(&last_address_);
    return true;
  }

  return false;
}

void CPU::Push(Byte value) {
  // A push operation will write |value| into current stack pointer, and stack
  // pointer will decrease, always pointing to the next available space.
  cpu_bus_->Write(kStackBase | registers_.S--, value);
}

Byte CPU::Pop() {
  // A pop operation will increase the stack pointer, and take the value. When
  // the value is taken, the current stack pointer is pointing to an available
  // space for writing.
  return cpu_bus_->Read(kStackBase | ++registers_.S);
}

void CPU::PushNextPC() {
  // Push PCH
  Push(static_cast<Byte>((registers_.PC + 1) >> 8));

  // Push PCL
  Push(static_cast<Byte>(registers_.PC + 1));
}

void CPU::PushPC() {
  // Push PCH
  Push(static_cast<Byte>((registers_.PC) >> 8));

  // Push PCL
  Push(static_cast<Byte>(registers_.PC));
}

void CPU::PopPC() {
  // Pop PCL
  registers_.PC = Pop();

  // Pop PCH, combining PCL and PCH to an address.
  registers_.PC |= Pop() << 8;
}

void CPU::SetZN(Byte value) {
  registers_.P.Z = !value;
  registers_.P.N = (value & 0x80) ? 1 : 0;
}

bool CPU::Execute(Opcode opcode) {
  return ExecuteMove(opcode) || ExecuteArithmetic(opcode) ||
         ExecuteJumpFlags(opcode) || ExecuteBlock0(static_cast<Byte>(opcode)) ||
         ExecuteBlock1(static_cast<Byte>(opcode)) ||
         ExecuteBlock2(static_cast<Byte>(opcode)) ||
         ExecuteBlock3(static_cast<Byte>(opcode));
}

bool CPU::ExecuteJumpFlags(Opcode opcode) {
  // Operations reference:
  // http://www.oxyron.de/html/opcodes02.html
  // https://www.nesdev.org/6502_cpu.txt
  switch (opcode) {
    case Opcode::NOP_TYPE2_0:
    case Opcode::NOP_UNOFFICIAL_0:
    case Opcode::NOP_UNOFFICIAL_1:
    case Opcode::NOP_UNOFFICIAL_2:
    case Opcode::NOP_UNOFFICIAL_3:
    case Opcode::NOP_UNOFFICIAL_4:
    case Opcode::NOP_UNOFFICIAL_5:
      break;
    case Opcode::NOP_TYPE0_0:
    case Opcode::NOP_TYPE0_1:
    case Opcode::NOP_TYPE0_2:
    case Opcode::NOP_TYPE0_3:
    case Opcode::NOP_TYPE0_4:
    case Opcode::NOP_TYPE0_5:
    case Opcode::NOP_TYPE0_6:
    case Opcode::NOP_TYPE0_7:
    case Opcode::NOP_TYPE0_8:
    case Opcode::NOP_TYPE0_9:
    case Opcode::NOP_TYPE0_10:
    case Opcode::NOP_TYPE0_11:
    case Opcode::NOP_TYPE0_12:
    case Opcode::NOP_TYPE0_13:
      ++registers_.PC;
      break;
    case Opcode::NOP_TYPE1_0:
    case Opcode::NOP_TYPE1_1:
    case Opcode::NOP_TYPE1_2:
    case Opcode::NOP_TYPE1_3:
    case Opcode::NOP_TYPE1_4:
    case Opcode::NOP_TYPE1_5:
    case Opcode::NOP_TYPE1_6:
      registers_.PC += 2;
      break;
    case Opcode::BRK:
      InterruptSequence(InterruptType::BRK);
      break;
    case Opcode::JSR:
      PushNextPC();
      registers_.PC = cpu_bus_->ReadWord(registers_.PC);
      break;
    case Opcode::RTS:
      PopPC();
      ++registers_.PC;
      break;
    case Opcode::RTI:
      registers_.P.value = Pop();
      PopPC();
      break;
    case Opcode::JMP:
      registers_.PC = cpu_bus_->ReadWord(registers_.PC);
      break;
    case Opcode::JMPI: {
      Address location = cpu_bus_->ReadWord(registers_.PC);
      Address page = location & 0xff00;
      registers_.PC = cpu_bus_->Read(location) |
                      cpu_bus_->Read(page | ((location + 1) & 0xff)) << 8;
    } break;
    case Opcode::BPL:
    case Opcode::BMI:
    case Opcode::BVC:
    case Opcode::BVS:
    case Opcode::BCC:
    case Opcode::BCS:
    case Opcode::BNE:
    case Opcode::BEQ: {
      bool branch = true;
      if (opcode == Opcode::BPL)
        branch = registers_.P.N == 0;
      else if (opcode == Opcode::BMI)
        branch = registers_.P.N == 1;
      else if (opcode == Opcode::BVC)
        branch = registers_.P.V == 0;
      else if (opcode == Opcode::BVS)
        branch = registers_.P.V == 1;
      else if (opcode == Opcode::BCC)
        branch = registers_.P.C == 0;
      else if (opcode == Opcode::BCS)
        branch = registers_.P.C == 1;
      else if (opcode == Opcode::BNE)
        branch = registers_.P.Z == 0;
      else if (opcode == Opcode::BEQ)
        branch = registers_.P.Z == 1;

      if (branch) {
        // The branch is met.
        int8_t offset = cpu_bus_->Read(registers_.PC++);
        // add 1 cycle on branches if taken.
        ++cycles_to_skip_;
        auto new_pc = static_cast<Address>(registers_.PC + offset);
        if (IS_CROSSING_PAGE(registers_.PC, new_pc)) {
          DCHECK(
              IsNeedAddOneCycleWhenCrossingPage(static_cast<uint8_t>(opcode)));
          increase_skip_cycle();
        }
        registers_.PC = new_pc;
      } else {
        ++registers_.PC;
      }
      break;
    }
    case Opcode::CLC:
      registers_.P.C = 0;
      break;
    case Opcode::SEC:
      registers_.P.C = 1;
      break;
    case Opcode::CLI:
      registers_.P.I = 0;
      break;
    case Opcode::SEI:
      registers_.P.I = 1;
      break;
    case Opcode::CLD:
      registers_.P.D = 0;
      break;
    case Opcode::SED:
      registers_.P.D = 1;
      break;
    case Opcode::CLV:
      registers_.P.V = 0;
      break;
    default:
      return false;
  }
  return true;
}

bool CPU::ExecuteMove(Opcode opcode) {
  // Move commands
  switch (opcode) {
    case Opcode::TAY:
      registers_.Y = registers_.A;
      SetZN(registers_.Y);
      break;
    case Opcode::TYA:
      registers_.A = registers_.Y;
      SetZN(registers_.A);
      break;
    case Opcode::TXA:
      registers_.A = registers_.X;
      SetZN(registers_.A);
      break;
    case Opcode::TXS:
      registers_.S = registers_.X;
      break;
    case Opcode::TAX:
      registers_.X = registers_.A;
      SetZN(registers_.X);
      break;
    case Opcode::TSX:
      registers_.X = registers_.S;
      SetZN(registers_.X);
      break;
    case Opcode::PHA:
      Push(registers_.A);
      break;
    case Opcode::PLA:
      registers_.A = Pop();
      SetZN(registers_.A);
      break;
    case Opcode::PHP: {
      Byte p = registers_.P.value;
      p |= 3 << 4;
      Push(p);
    } break;
    case Opcode::PLP: {
      Byte b = registers_.P.B;
      registers_.P.value = Pop();
      // Reserve B flag
      registers_.P.B = b;
      break;
    }
    case Opcode::SHY: {
      bool is_page_crossed = false;
      DCHECK_OPCODE_ADDRESSING_MODE((Byte)opcode, AddressingMode::kABX);
      Address location = Addressing(AddressingMode::kABX, is_page_crossed);
      location =
          ((registers_.Y & static_cast<Byte>((location >> 8) + 1)) << 8) |
          (static_cast<Byte>(location) & 0xff);
      cpu_bus_->Write(location, location >> 8);
    } break;
    case Opcode::SHX: {
      bool is_page_crossed = false;
      DCHECK_OPCODE_ADDRESSING_MODE((Byte)opcode, AddressingMode::kABX);
      Address location = Addressing(AddressingMode::kABY, is_page_crossed);
      location =
          ((registers_.X & static_cast<Byte>((location >> 8) + 1)) << 8) |
          (static_cast<Byte>(location) & 0xff);
      cpu_bus_->Write(location, location >> 8);
    } break;
    default:
      return false;
  }
  return true;
}

bool CPU::ExecuteArithmetic(Opcode opcode) {
  // Logical and arithmetic commands
  switch (opcode) {
    case Opcode::DEY:
      --registers_.Y;
      SetZN(registers_.Y);
      break;
    case Opcode::DEX:
      --registers_.X;
      SetZN(registers_.X);
      break;
    case Opcode::INY:
      ++registers_.Y;
      SetZN(registers_.Y);
      break;
    case Opcode::INX:
      ++registers_.X;
      SetZN(registers_.X);
      break;
    default:
      return false;
  }
  return true;
}

bool CPU::ExecuteBlock0(Byte opcode) {
  enum class AddressingMode0 {
    kImmediate,
    kZeroPage,
    kAccumulator,
    kAbsolute,
    kZeroPageIndexed = 5,
    kAbsoluteIndexed = 7,
  };
  enum class Operation0 {
    BIT = 1,
    STY = 4,
    LDY,
    CPY,
    CPX,
  };
  Address location = 0;
  if (IS_OPCODE_IN_BLOCK(opcode, 0)) {
    bool is_page_crossed = false;
    switch (
        static_cast<AddressingMode0>(OPCODE_ADDRESS_MODE_IN_BLOCK(opcode))) {
      case AddressingMode0::kImmediate:
        DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kIMM);
        location = Addressing(AddressingMode::kIMM, is_page_crossed);
        break;
      case AddressingMode0::kZeroPage:
        DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kZP);
        location = Addressing(AddressingMode::kZP, is_page_crossed);
        break;
      case AddressingMode0::kAbsolute:
        DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kABS);
        location = Addressing(AddressingMode::kABS, is_page_crossed);
        break;
      case AddressingMode0::kZeroPageIndexed:
        DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kZPX);
        location = Addressing(AddressingMode::kZPX, is_page_crossed);
        break;
      case AddressingMode0::kAbsoluteIndexed:
        DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kABX);
        location = Addressing(AddressingMode::kABX, is_page_crossed);
        break;
      default:
        LOG(ERROR) << "Wrong addressing mode: "
                   << OPCODE_ADDRESS_MODE_IN_BLOCK(opcode);
        return false;
    }

    if (is_page_crossed && IsNeedAddOneCycleWhenCrossingPage(opcode))
      increase_skip_cycle();

    uint16_t operand = 0;
    switch (static_cast<Operation0>(OPCODE_ROW_IN_BLOCK(opcode))) {
      case Operation0::BIT:
        DCHECK_OPCODE_BY_NAME(opcode, BIT);
        operand = cpu_bus_->Read(location);
        registers_.P.Z = !(registers_.A & operand);
        registers_.P.V = (operand & 0x40) ? 1 : 0;
        registers_.P.N = (operand & 0x80) ? 1 : 0;
        break;
      case Operation0::STY:
        DCHECK_OPCODE_BY_NAME(opcode, STY);
        cpu_bus_->Write(location, registers_.Y);
        break;
      case Operation0::LDY:
        DCHECK_OPCODE_BY_NAME(opcode, LDY);
        registers_.Y = cpu_bus_->Read(location);
        SetZN(registers_.Y);
        break;
      case Operation0::CPY: {
        DCHECK_OPCODE_BY_NAME(opcode, CPY);
        uint16_t diff = registers_.Y - cpu_bus_->Read(location);
        registers_.P.C = !(diff & 0x100);
        SetZN(static_cast<Byte>(diff));
      } break;
      case Operation0::CPX: {
        DCHECK_OPCODE_BY_NAME(opcode, CPX);
        uint16_t diff = registers_.X - cpu_bus_->Read(location);
        registers_.P.C = !(diff & 0x100);
        SetZN(static_cast<Byte>(diff));
      } break;
      default:
        LOG(ERROR) << "Wrong opcode: " << opcode;
        return false;
    }
    return true;
  }
  return false;
}

bool CPU::ExecuteBlock1(Byte opcode) {
  enum class AddressingMode1 {
    kIndexedIndirectX,
    kZeroPage,
    kImmediate,
    kAbsolute,
    kIndirectY,
    kIndexedX,
    kAbsoluteY,
    kAbsoluteX,
  };

  enum class Operation1 {
    ORA,
    AND,
    EOR,
    ADC,
    STA,
    LDA,
    CMP,
    SBC,
  };
  bool is_page_crossed = false;
  if (IS_OPCODE_IN_BLOCK(opcode, 1)) {
    Address location = 0;  // Location of the operand, could be in RAM
    auto op = static_cast<Operation1>(OPCODE_ROW_IN_BLOCK(opcode));
    switch (
        static_cast<AddressingMode1>(OPCODE_ADDRESS_MODE_IN_BLOCK(opcode))) {
      case AddressingMode1::kIndexedIndirectX: {
        DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kIZX);
        location = Addressing(AddressingMode::kIZX, is_page_crossed);
      } break;
      case AddressingMode1::kZeroPage:
        DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kZP);
        location = Addressing(AddressingMode::kZP, is_page_crossed);
        break;
      case AddressingMode1::kImmediate:
        DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kIMM);
        location = Addressing(AddressingMode::kIMM, is_page_crossed);
        break;
      case AddressingMode1::kAbsolute:
        DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kABS);
        location = Addressing(AddressingMode::kABS, is_page_crossed);
        break;
      case AddressingMode1::kIndirectY: {
        DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kIZY);
        location = Addressing(AddressingMode::kIZY, is_page_crossed);
      } break;
      case AddressingMode1::kIndexedX:
        DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kZPX);
        location = Addressing(AddressingMode::kZPX, is_page_crossed);
        break;
      case AddressingMode1::kAbsoluteY:
        DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kABY);
        location = Addressing(AddressingMode::kABY, is_page_crossed);
        break;
      case AddressingMode1::kAbsoluteX:
        DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kABX);
        location = Addressing(AddressingMode::kABX, is_page_crossed);
        break;
      default:
        return false;
    }

    if (is_page_crossed && IsNeedAddOneCycleWhenCrossingPage(opcode))
      increase_skip_cycle();

    switch (op) {
      case Operation1::ORA:
        DCHECK_OPCODE_BY_NAME(opcode, ORA);
        registers_.A |= cpu_bus_->Read(location);
        SetZN(registers_.A);
        break;
      case Operation1::AND:
        DCHECK_OPCODE_BY_NAME(opcode, AND);
        registers_.A &= cpu_bus_->Read(location);
        SetZN(registers_.A);
        break;
      case Operation1::EOR:
        DCHECK_OPCODE_BY_NAME(opcode, EOR);
        registers_.A ^= cpu_bus_->Read(location);
        SetZN(registers_.A);
        break;
      case Operation1::ADC: {
        DCHECK_OPCODE_BY_NAME(opcode, ADC);
        uint16_t operand = cpu_bus_->Read(location);
        uint16_t sum = registers_.A + operand + registers_.P.C;
        // Carry forward or UNSIGNED overflow
        registers_.P.C = (sum > 0xff) ? 1 : 0;
        registers_.P.V =
            ((registers_.A ^ sum) & (~(registers_.A ^ operand)) & 0x80) ? 1 : 0;
        registers_.A = static_cast<Byte>(sum);
        SetZN(registers_.A);
      } break;
      case Operation1::STA:
        DCHECK_OPCODE_BY_NAME(opcode, STA);
        cpu_bus_->Write(location, registers_.A);
        break;
      case Operation1::LDA:
        DCHECK_OPCODE_BY_NAME(opcode, LDA);
        registers_.A = cpu_bus_->Read(location);
        SetZN(registers_.A);
        break;
      case Operation1::SBC: {
        DCHECK_OPCODE_BY_NAME(opcode, SBC);
        uint16_t operand = cpu_bus_->Read(location);
        uint16_t diff = registers_.A - operand - (1 - registers_.P.C);
        registers_.P.C = diff < 0x100;
        registers_.P.V =
            ((registers_.A ^ operand) & (registers_.A ^ diff) & 0x80) ? 1 : 0;
        registers_.A = static_cast<Byte>(diff);
        SetZN(registers_.A);
      } break;
      case Operation1::CMP: {
        DCHECK_OPCODE_BY_NAME(opcode, CMP);
        uint16_t diff = registers_.A - cpu_bus_->Read(location);
        registers_.P.C = !(diff & 0x100);
        SetZN(static_cast<Byte>(diff));
      } break;
      default:
        return false;
    }
    return true;
  }
  return false;
}

bool CPU::ExecuteBlock2(Byte opcode) {
  enum class AddressingMode2 {
    kImmediate,
    kZeroPage,
    kAccumulator,
    kAbsolute,
    kIndexed = 5,
    kAbsoluteIndexed = 7,
  };
  enum class Operation2 {
    ASL,
    ROL,
    LSR,
    ROR,
    STX,
    LDX,
    DEC,
    INC,
  };

  bool is_page_crossed = false;
  if (IS_OPCODE_IN_BLOCK(opcode, 2)) {
    Address location = 0;
    auto op = static_cast<Operation2>(OPCODE_ROW_IN_BLOCK(opcode));
    auto addr_mode =
        static_cast<AddressingMode2>(OPCODE_ADDRESS_MODE_IN_BLOCK(opcode));
    switch (addr_mode) {
      case AddressingMode2::kImmediate:
        DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kIMM);
        location = Addressing(AddressingMode::kIMM, is_page_crossed);
        break;
      case AddressingMode2::kZeroPage:
        DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kZP);
        location = Addressing(AddressingMode::kZP, is_page_crossed);
        break;
      case AddressingMode2::kAccumulator:
        break;
      case AddressingMode2::kAbsolute:
        DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kABS);
        location = Addressing(AddressingMode::kABS, is_page_crossed);
        break;
      case AddressingMode2::kIndexed: {
        if (op == Operation2::LDX || op == Operation2::STX) {
          DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kZPY);
          location = Addressing(AddressingMode::kZPY, is_page_crossed);
        } else {
          DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kZPX);
          location = Addressing(AddressingMode::kZPX, is_page_crossed);
        }
      } break;
      case AddressingMode2::kAbsoluteIndexed: {
        if (op == Operation2::LDX || op == Operation2::STX) {
          DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kABY);
          location = Addressing(AddressingMode::kABY, is_page_crossed);
        } else {
          DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kABX);
          location = Addressing(AddressingMode::kABX, is_page_crossed);
        }
      } break;
      default:
        return false;
    }

    if (is_page_crossed && IsNeedAddOneCycleWhenCrossingPage(opcode))
      increase_skip_cycle();

    switch (op) {
      case Operation2::ASL:
      case Operation2::ROL:
        if (op == Operation2::ROL) {
          DCHECK_OPCODE_BY_NAME(opcode, ROL);
        } else {
          DCHECK_OPCODE_BY_NAME(opcode, ASL);
        }
        if (addr_mode == AddressingMode2::kAccumulator) {
          auto prev_C = registers_.P.C;
          registers_.P.C = (registers_.A & 0x80) ? 1 : 0;
          registers_.A <<= 1;
          registers_.A = registers_.A | (prev_C && (op == Operation2::ROL));
          SetZN(registers_.A);
        } else {
          auto prev_C = registers_.P.C;
          uint16_t operand = cpu_bus_->Read(location);
          registers_.P.C = (operand & 0x80) ? 1 : 0;
          operand = operand << 1 | (prev_C && (op == Operation2::ROL));
          SetZN(static_cast<Byte>(operand));
          cpu_bus_->Write(location, static_cast<Byte>(operand));
        }
        break;
      case Operation2::LSR:
      case Operation2::ROR:
        if (op == Operation2::ROR) {
          DCHECK_OPCODE_BY_NAME(opcode, ROR);
        } else {
          DCHECK_OPCODE_BY_NAME(opcode, LSR);
        }
        if (addr_mode == AddressingMode2::kAccumulator) {
          auto prev_C = registers_.P.C;
          registers_.P.C = registers_.A & 1;
          registers_.A >>= 1;
          registers_.A = registers_.A | (prev_C && (op == Operation2::ROR))
                                            << 7;
          SetZN(registers_.A);
        } else {
          auto prev_C = registers_.P.C;
          uint16_t operand = cpu_bus_->Read(location);
          registers_.P.C = operand & 1;
          operand = operand >> 1 | (prev_C && (op == Operation2::ROR)) << 7;
          SetZN(static_cast<Byte>(operand));
          cpu_bus_->Write(location, static_cast<Byte>(operand));
        }
        break;
      case Operation2::STX:
        DCHECK_OPCODE_BY_NAME(opcode, STX);
        cpu_bus_->Write(location, registers_.X);
        break;
      case Operation2::LDX:
        DCHECK_OPCODE_BY_NAME(opcode, LDX);
        registers_.X = cpu_bus_->Read(location);
        SetZN(registers_.X);
        break;
      case Operation2::DEC: {
        DCHECK_OPCODE_BY_NAME(opcode, DEC);
        auto operand = cpu_bus_->Read(location) - 1;
        SetZN(operand);
        cpu_bus_->Write(location, operand);
      } break;
      case Operation2::INC: {
        DCHECK_OPCODE_BY_NAME(opcode, INC);
        auto operand = cpu_bus_->Read(location) + 1;
        SetZN(operand);
        cpu_bus_->Write(location, operand);
      } break;
      default:
        return false;
    }
    return true;
  }
  return false;
}

bool CPU::ExecuteBlock3(Byte opcode) {
  bool is_page_crossed = false;
  switch (static_cast<Opcode>(opcode)) {
    case Opcode::SBC_UNOFFICAL: {
      // This is a illegal instruction. Treats it as 0xe9 (SBC #imm).
      return ExecuteBlock1(0xe9);
    }
    case Opcode::ANC_0:
    case Opcode::ANC_1: {
      DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kIMM);
      Address location = Addressing(AddressingMode::kIMM, is_page_crossed);
      registers_.A &= cpu_bus_->Read(location);
      SetZN(registers_.A);
      registers_.P.C = registers_.P.N;
      return true;
    }
    case Opcode::LAS: {
      DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kABY);
      Address location = Addressing(AddressingMode::kABY, is_page_crossed);
      Byte operand = cpu_bus_->Read(location);
      registers_.A = registers_.X = registers_.S = (registers_.S & operand);
      SetZN(registers_.A);
      return true;
    }
    case Opcode::ALR: {
      DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kIMM);
      Address location = Addressing(AddressingMode::kIMM, is_page_crossed);
      Byte operand = cpu_bus_->Read(location);
      operand &= registers_.A;
      registers_.P.C = operand & 0x01;
      registers_.A = operand >> 1;
      SetZN(registers_.A);
      return true;
    }
    case Opcode::ARR: {
      DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kIMM);
      Address location = Addressing(AddressingMode::kIMM, is_page_crossed);
      Byte operand = cpu_bus_->Read(location);
      operand &= registers_.A;
      registers_.A = (operand >> 1) | (registers_.P.C << 7);
      SetZN(registers_.A);
      registers_.P.C = (registers_.A & 0x40) ? 1 : 0;
      registers_.P.V = (registers_.A >> 6) ^ (registers_.A >> 5);
      return true;
    }
    case Opcode::AXS: {
      DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kIMM);
      Address location = Addressing(AddressingMode::kIMM, is_page_crossed);
      uint16_t result =
          (registers_.A & registers_.X) - cpu_bus_->Read(location);
      registers_.P.C = (result < 0x100) ? 1 : 0;
      registers_.X = result & 0xff;
      SetZN(registers_.X);
      return true;
    }
    default:
      break;
  }

  enum class AddressingMode3 {
    kIndexedIndirectX,
    kZeroPage,
    kImmediate,
    kAbsolute,
    kIndirectY,
    kIndexed,
    kAbsoluteIndexedY,
    kAbsoluteIndexed,
  };

  enum class Operation3 {
    SLO,
    RLA,
    SRE,
    RRA,
    SAX,
    LAX,
    DCP,
    ISC,
  };

  if (IS_OPCODE_IN_BLOCK(opcode, 3)) {
    Address location = 0;  // Location of the operand,
                           // could be in RAM
    auto op = static_cast<Operation3>(OPCODE_ROW_IN_BLOCK(opcode));
    switch (
        static_cast<AddressingMode3>(OPCODE_ADDRESS_MODE_IN_BLOCK(opcode))) {
      case AddressingMode3::kIndexedIndirectX: {
        DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kIZX);
        location = Addressing(AddressingMode::kIZX, is_page_crossed);
      } break;
      case AddressingMode3::kZeroPage:
        DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kZP);
        location = Addressing(AddressingMode::kZP, is_page_crossed);
        break;
      case AddressingMode3::kImmediate:
        DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kIMM);
        location = Addressing(AddressingMode::kIMM, is_page_crossed);
        break;
      case AddressingMode3::kAbsolute:
        DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kABS);
        location = Addressing(AddressingMode::kABS, is_page_crossed);
        break;
      case AddressingMode3::kIndirectY: {
        DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kIZY);
        location = Addressing(AddressingMode::kIZY, is_page_crossed);
      } break;
      case AddressingMode3::kIndexed: {
        if (op == Operation3::SAX || op == Operation3::LAX) {
          DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kZPY);
          location = Addressing(AddressingMode::kZPY, is_page_crossed);
        } else {
          DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kZPX);
          location = Addressing(AddressingMode::kZPX, is_page_crossed);
        }
      } break;
      case AddressingMode3::kAbsoluteIndexedY: {
        DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kABY);
        location = Addressing(AddressingMode::kABY, is_page_crossed);
      } break;
      case AddressingMode3::kAbsoluteIndexed: {
        if (op == Operation3::SAX || op == Operation3::LAX) {
          DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kABY);
          location = Addressing(AddressingMode::kABY, is_page_crossed);
        } else {
          DCHECK_OPCODE_ADDRESSING_MODE(opcode, AddressingMode::kABX);
          location = Addressing(AddressingMode::kABX, is_page_crossed);
        }
      } break;
      default:
        return false;
    }

    if (is_page_crossed && IsNeedAddOneCycleWhenCrossingPage(opcode))
      increase_skip_cycle();

    switch (op) {
      case Operation3::SLO: {
        DCHECK_OPCODE_BY_NAME(opcode, SLO);
        Byte operand = cpu_bus_->Read(location);
        registers_.P.C = (operand & 0x80) ? 1 : 0;
        operand <<= 1;
        registers_.A |= operand;
        SetZN(registers_.A);
        cpu_bus_->Write(location, operand);
      } break;
      case Operation3::RLA: {
        DCHECK_OPCODE_BY_NAME(opcode, RLA);
        Byte operand = cpu_bus_->Read(location);
        if (registers_.P.C) {
          registers_.P.C = (operand & 0x80) ? 1 : 0;
          operand = (operand << 1) | 1;
        } else {
          registers_.P.C = (operand & 0x80) ? 1 : 0;
          operand <<= 1;
        }
        registers_.A &= operand;
        SetZN(registers_.A);
        cpu_bus_->Write(location, operand);
      } break;
      case Operation3::SRE: {
        DCHECK_OPCODE_BY_NAME(opcode, SRE);
        Byte operand = cpu_bus_->Read(location);
        registers_.P.C = (operand & 0x01) ? 1 : 0;
        operand >>= 1;
        registers_.A ^= operand;
        SetZN(registers_.A);
        cpu_bus_->Write(location, operand);
      } break;
      case Operation3::RRA: {
        DCHECK_OPCODE_BY_NAME(opcode, RRA);
        Byte operand = cpu_bus_->Read(location);
        if (registers_.P.C) {
          registers_.P.C = (operand & 0x01) ? 1 : 0;
          operand = (operand >> 1) | 0x80;
        } else {
          registers_.P.C = (operand & 0x01) ? 1 : 0;
          operand >>= 1;
        }
        cpu_bus_->Write(location, operand);

        // ADC
        uint16_t sum = registers_.A + operand + registers_.P.C;
        registers_.P.C = (sum > 0xff) ? 1 : 0;
        registers_.P.V =
            ((registers_.A ^ sum) & (~(registers_.A ^ operand)) & 0x80) ? 1 : 0;
        registers_.A = static_cast<Byte>(sum);
        SetZN(registers_.A);
      } break;
      case Operation3::SAX: {
        DCHECK_OPCODE_BY_NAME(opcode, SAX);
        cpu_bus_->Write(location, registers_.A & registers_.X);
      } break;
      case Operation3::LAX: {
        DCHECK_OPCODE_BY_NAME(opcode, LAX);
        registers_.A = cpu_bus_->Read(location);
        registers_.X = registers_.A;
        SetZN(registers_.A);
      } break;
      case Operation3::DCP: {
        DCHECK_OPCODE_BY_NAME(opcode, DCP);
        Byte operand = cpu_bus_->Read(location) - 1;
        uint16_t diff = registers_.A - operand;
        registers_.P.C = ((diff & 0x8000) == 0) ? 1 : 0;
        SetZN(static_cast<Byte>(diff));
        cpu_bus_->Write(location, operand);
      } break;
      case Operation3::ISC: {
        DCHECK_OPCODE_BY_NAME(opcode, ISC);
        // INC
        Byte operand = cpu_bus_->Read(location) + 1;
        cpu_bus_->Write(location, operand);
        // SBC
        uint16_t diff = registers_.A - operand - (1 - registers_.P.C);
        registers_.P.C = diff < 0x100;
        registers_.P.V =
            ((registers_.A ^ operand) & (registers_.A ^ diff) & 0x80) ? 1 : 0;
        registers_.A = static_cast<Byte>(diff);
        SetZN(registers_.A);
      } break;
      default:
        return false;
    }
    return true;
  }
  return false;
}

void CPU::InterruptSequence(InterruptType type) {
  if (registers_.P.I && type != InterruptType::NMI &&
      type != InterruptType::BRK) {
    // Interrupt is disabled.
    return;
  }

  if (type == InterruptType::BRK) {
    ++registers_.PC;
  }

  CPURegisters::State_t new_P = registers_.P;
  // If BRK, B flags will set to binary 11 (dec 3). Otherwise binary 10 (dec 2)
  new_P.B = type == InterruptType::BRK ? 3 : 2;
  PushPC();
  Push(new_P.value);
  registers_.P.I = 1;

  switch (type) {
    case InterruptType::IRQ:
    case InterruptType::BRK:
      registers_.PC = cpu_bus_->ReadWord(kIRQVector);
      break;
    case InterruptType::NMI:
      registers_.PC = cpu_bus_->ReadWord(kNMIVector);
      break;
  }

  cycles_to_skip_ += 6;
}

Address CPU::Addressing(AddressingMode mode, bool& is_page_crossed) {
  is_page_crossed = false;
  switch (mode) {
    case AddressingMode::kNONE: {
      LOG(ERROR) << "Shouldn't addressing location for kNONE";
      return 0;
    }
    case AddressingMode::kIMM: {
      return registers_.PC++;
    }
    case AddressingMode::kZP: {
      return cpu_bus_->Read(registers_.PC++);
    }
    case AddressingMode::kZPX: {
      Address location = cpu_bus_->Read(registers_.PC++);
      location = (location + registers_.X) & 0xff;
      return location;
    }
    case AddressingMode::kZPY: {
      Address location = cpu_bus_->Read(registers_.PC++);
      location = (location + registers_.Y) & 0xff;
      return location;
    }
    case AddressingMode::kABS: {
      Address location = cpu_bus_->ReadWord(registers_.PC);
      registers_.PC += 2;
      return location;
    }
    case AddressingMode::kABX: {
      Address location = cpu_bus_->ReadWord(registers_.PC);
      registers_.PC += 2;
      Byte index = registers_.X;
      is_page_crossed = IS_CROSSING_PAGE(location, location + index);
      location += index;
      return location;
    }
    case AddressingMode::kABY: {
      Address location = cpu_bus_->ReadWord(registers_.PC);
      registers_.PC += 2;
      Byte index = registers_.Y;
      is_page_crossed = IS_CROSSING_PAGE(location, location + index);
      location += index;
      return location;
    }
    case AddressingMode::kIZX: {
      Byte zero_addr = registers_.X + cpu_bus_->Read(registers_.PC++);
      return cpu_bus_->Read(zero_addr & 0xff) |
             cpu_bus_->Read((zero_addr + 1) & 0xff) << 8;
    }
    case AddressingMode::kIZY: {
      Byte zero_addr = cpu_bus_->Read(registers_.PC++);
      Address location = cpu_bus_->Read(zero_addr & 0xff) |
                         cpu_bus_->Read((zero_addr + 1) & 0xff) << 8;
      is_page_crossed = IS_CROSSING_PAGE(location, location + registers_.Y);
      location += registers_.Y;
      return location;
    }
    case AddressingMode::kIND:
    case AddressingMode::kREL:
      LOG(ERROR) << "JMP and branch instructions are IND and REL addressing "
                    "mode, but they don't need addressing.";
      return 0;
    default:
      LOG(ERROR) << "Wrong addressing mode: " << static_cast<int>(mode);
      return 0;
  }
}

}  // namespace nes
}  // namespace kiwi
