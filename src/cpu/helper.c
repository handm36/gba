#include "helper.h"
#include <stdint.h>
#include <sys/types.h>

Thumb_Instructions decode_thumb(uint16_t opcode) {
  if ((opcode & 0b1111111100000000) == 0b1101111100000000) {
    return SoftwareInterruptTHUMB;
  }

  if ((opcode & 0b1111100000000000) == 0b1110000000000000) {
    return UnconditionalBranch;
  }

  if ((opcode & 0b1111000000000000) == 0b1101000000000000) {
    return ConditionalBranch;
  }

  if ((opcode & 0b1111000000000000) == 0b1100000000000000) {
    return MultipleLoadstore;
  }

  if ((opcode & 0b1111000000000000) == 0b1111000000000000) {
    return LongBranchWithLink;
  }

  if ((opcode & 0b1111111100000000) == 0b1011000000000000) {
    return AddOffsetToStackPointer;
  }

  if ((opcode & 0b1111011000000000) == 0b1011010000000000) {
    return PushPopRegisters;
  }

  if ((opcode & 0b1111000000000000) == 0b1000000000000000) {
    return LoadStoreHalfword;
  }

  if ((opcode & 0b1111000000000000) == 0b1001000000000000) {
    return SPRelativeLoadStore;
  }

  if ((opcode & 0b1111000000000000) == 0b1010000000000000) {
    return LoadAddress;
  }

  if ((opcode & 0b1110000000000000) == 0b0110000000000000) {
    return LoadStoreWithImmediateOffset;
  }

  if ((opcode & 0b1111001000000000) == 0b0101000000000000) {
    return LoadStoreWithRegisterOffset;
  }

  if ((opcode & 0b1111001000000000) == 0b0101001000000000) {
    return LoadStoreSignExtendedByteHalfword;
  }

  if ((opcode & 0b1111100000000000) == 0b0100100000000000) {
    return PCRelativeLoad;
  }

  if ((opcode & 0b1111110000000000) == 0b0100010000000000) {
    return HiRegisterOperationsBranchExchange;
  }

  if ((opcode & 0b1111110000000000) == 0b0100000000000000) {
    return ALUOperations;
  }

  if ((opcode & 0b1110000000000000) == 0b0010000000000000) {
    return MoveCompareAddSubtractImmediate;
  }

  if ((opcode & 0b1111100000000000) == 0b0001100000000000) {
    return AddSubtract;
  }

  if ((opcode & 0b1110000000000000) == 0b0000000000000000) {
    return MoveShiftedRegister;
  }

  return UnimplementedTHUMB;
}

Arm_Instructions decode_arm(uint32_t opcode) {
  if ((opcode & 0b00001111111111111111111111110000) ==
      0b00000001001011111111111100010000) {
    return BranchAndBranchExchange;
  }

  if ((opcode & 0b00001110000000000000000000000000) ==
      0b00001000000000000000000000000000) {
    return BlockDataTransfer;
  }

  if ((opcode & 0b00001110000000000000000000000000) ==
      0b00001010000000000000000000000000) {
    return BranchAndBranchWithLink;
  }

  if ((opcode & 0b00001111000000000000000000000000) ==
      0b00001111000000000000000000000000) {
    return SoftwareInterrupt;
  }

  if ((opcode & 0b00001110000000000000000000010000) ==
      0b00000110000000000000000000010000) {
    return Undefined;
  }

  if ((opcode & 0b00001100000000000000000000000000) ==
      0b00000100000000000000000000000000) {
    return SingleDataTransfer;
  }

  if ((opcode & 0b00001111100000000000111111110000) ==
      0b00000001000000000000000010010000) {
    return SingleDataSwap;
  }

  if ((opcode & 0b00001111000000000000000011110000) ==
      0b00000000000000000000000010010000) {
    return Multiply;
  }

  if ((opcode & 0b00001110010000000000111110010000) ==
      0b00000000000000000000000010010000) {
    return HalfwordDataTransferRegister;
  }

  if ((opcode & 0b00001110010000000000000010010000) ==
      0b00000000010000000000000010010000) {
    return HalfwordDataTransferImmediate;
  }

  if ((opcode & 0b00001111101111110000000000000000) ==
      0b00000001000011110000000000000000) {
    return PSRTransferMRS;
  }

  if ((opcode & 0b00001101101100001111000000000000) ==
      0b00000001001000001111000000000000) {
    return PSRTransferMSR;
  }

  if ((opcode & 0b00001100000000000000000000000000) ==
      0b00000000000000000000000000000000) {
    return DataProcessing;
  }

  return Unimplemented;
}

// Checks if the instruction should be skippped based on its condition flag
// returns 1 if it should be skipped
int check_condition_code(uint32_t CPSR, uint8_t condition_code) {
  int zero_flag = !!(CPSR & ZERO_FLAG);
  int carry_flag = !!(CPSR & CARRY_FLAG);
  int sign_flag = !!(CPSR & SIGN_FLAG);
  int overflow_flag = !!(CPSR & OVERFLOW_FLAG);

  switch (condition_code) {
  case 0:
    // EQ checks ZERO=1
    if (zero_flag == 1)
      break;
    return 1;
  case 1:
    // NE checks ZERO=0
    if (zero_flag == 0)
      break;
    return 1;
  case 2:
    // CS/HS checks CARRY=1
    if (carry_flag == 1)
      break;
    return 1;
  case 3:
    // CC/LO checks CARRY=0
    if (carry_flag == 0)
      break;
    return 1;
  case 4:
    // MI checks SIGN=1
    if (sign_flag == 1)
      break;
    return 1;
  case 5:
    // PL checks SIGN=0
    if (sign_flag == 0)
      break;
    return 1;
  case 6:
    // VS checks OVERFLOW=1
    if (overflow_flag == 1)
      break;
    return 1;
  case 7:
    // VC checks OVERFLOW=0
    if (overflow_flag == 0)
      break;
    return 1;
  case 8:
    // HI CARRY=1 and ZERO=0
    if ((overflow_flag == 1) && (zero_flag == 0))
      break;
    return 1;
  case 9:
    // LS CARRY=0 or ZERO=1
    if ((carry_flag == 0) || (zero_flag == 1))
      break;
    return 1;
  case 0xA:
    // GE checks SIGN=OVERFLOW
    if (sign_flag == overflow_flag)
      break;
    return 1;
  case 0xB:
    // LT checks SIGN != OVERFLOW
    if (sign_flag != overflow_flag)
      break;
    return 1;
  case 0xC:
    // GT checks ZERO=0 and SIGN=OVERFLOW
    if (zero_flag == 0 && (sign_flag == overflow_flag))
      break;
    return 1;
  case 0xD:
    // LE checks ZERO=1 and SIGN != OVERFLOW
    if (zero_flag == 1 && (sign_flag != overflow_flag))
      break;
    return 1;
  case 0xE:
    // AL always true
    return 0;
  case 0xF:
    // Unpredictable
    return 1;
  }

  return 0;
}

uint8_t carry_from(uint32_t x, uint32_t y, uint32_t z) {
  if ((uint64_t)x + (uint64_t)y + (uint64_t)z > UINT32_MAX)
    return 1;
  return 0;
}

uint8_t borrow_from(int32_t x, int32_t y, int32_t z) {
  if ((int64_t)x - (int64_t)y - (int64_t)z < 0)
    return 1;
  return 0;
}

uint8_t overflow_from(int32_t x, int32_t y, int32_t result, uint8_t is_sub) {
  uint8_t x_sign = ((x >> 31) & 0x1);
  uint8_t y_sign = ((y >> 31) & 0x1);
  uint8_t result_sign = ((result >> 31) & 0x1);
  if (is_sub == 0) {
    if (x_sign == y_sign && result_sign != x_sign)
      return 1;
  } else {
    if (x_sign != y_sign && result_sign != x_sign)
      return 1;
  }
  return 0;
}

// returns true(1) if the current processor mode is not User mode or System
// mode, else false(0)
int current_mode_has_SPSR(GBA_CPU *cpu) {
  uint8_t mode_bits = cpu->CPSR & 0x1F; // take the last 5 bits
  if (mode_bits == 0x10 || mode_bits == 0x1F) {
    return 0;
  }

  return 1;
}

uint32_t get_current_spsr(GBA_CPU *cpu) {
  uint8_t mode_bits = cpu->CPSR & 0x1F; // take the last 5 bits

  // write new mode
  switch (mode_bits) {
  case 0x10: // User (non-privileged)
  case 0x1F: // System
             // execution should never reach here
    break;
  case 0x11: // FIQ
    return cpu->SPSR_fiq;
  case 0x12: // IRQ
    return cpu->SPSR_irq;
  case 0x13: // Supervisor (SWI)
    return cpu->SPSR_svc;
  case 0x17: // Abort
    return cpu->SPSR_abt;
    break;
  case 0x1B: // Undefined
    return cpu->SPSR_und;
  }

  return cpu->CPSR;
}
