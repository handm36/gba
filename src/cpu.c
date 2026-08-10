#include "cpu.h"
#include "gba.h"
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

static uint8_t *resolve_addr(GBA_Memory *mem, uint32_t addr, uint8_t size) {
  size = size - 1; // added size - 1 so it equals last byte offset
  if (addr <= 0x3FFF - size)
    return &mem->BIOS[addr];
  else if (addr >= 0x02000000 && addr <= 0x0203FFFF - size)
    return &mem->ob_WRAM[addr - 0x02000000];
  else if (addr >= 0x03000000 && addr <= 0x03007FFF - size)
    return &mem->oc_WRAM[addr - 0x03000000];
  else if (addr >= 0x04000000 && addr <= 0x040003FF - size)
    return &mem->IO_regs[addr - 0x04000000];
  else if (addr >= 0x05000000 && addr <= 0x050003FF - size)
    return &mem->palette[addr - 0x05000000];
  else if (addr >= 0x06000000 && addr <= 0x06017FFF - size)
    return &mem->VRAM[addr - 0x06000000];
  else if (addr >= 0x07000000 && addr <= 0x070003FF - size)
    return &mem->OAM[addr - 0x07000000];
  else if (addr >= 0x0E000000 && addr <= 0x0E00FFFF - size)
    return &mem->SRAM[addr - 0x0E000000];
  else if (addr >= 0x08000000 && addr <= 0x09FFFFFF &&
           (addr - 0x08000000 + size) <= mem->rom_size)
    return &mem->game_ROM[addr - 0x08000000];
  else if (addr >= 0x0A000000 && addr <= 0x0BFFFFFF &&
           (addr - 0x0A000000 + size) <= mem->rom_size)
    return &mem->game_ROM[addr - 0x0A000000];
  else if (addr >= 0x0C000000 && addr <= 0x0DFFFFFF &&
           (addr - 0x0C000000 + size) <= mem->rom_size)
    return &mem->game_ROM[addr - 0x0C000000];

  return NULL;
}

uint8_t readmem_8(GBA_Memory *mem, uint32_t addr) {
  uint8_t *result = resolve_addr(mem, addr, 1);
  if (result == NULL)
    return 0;
  return *result;
}

uint16_t readmem16(GBA_Memory *mem, uint32_t addr) {
  uint8_t *result = resolve_addr(mem, addr, 2);
  uint16_t val;
  if (result == NULL)
    return 0;

  memcpy(&val, result, 2);
  return val;
}

uint32_t readmem_32(GBA_Memory *mem, uint32_t addr) {
  uint8_t *result = resolve_addr(mem, addr, 4);
  uint32_t val;
  if (result == NULL)
    return 0;

  memcpy(&val, result, 4);
  return val;
}

// save old mode's banked registers and write new mode's to registers
// https://problemkaputt.de/gbatek.htm#armcpuregisterset
void handle_CPSR_mode_switch(GBA_CPU *cpu, uint8_t new_mode) {
  uint8_t mode_bits = cpu->CPSR & 0x1F; // take the last 5 bits

  // save old mode
  switch (mode_bits) {
  case 0x10: // User (non-privileged)
  case 0x1F: // System
    cpu->regs_usr[0] = cpu->regs[13];
    cpu->regs_usr[1] = cpu->regs[14];
    break;
  case 0x11:                      // FIQ
    for (int i = 0; i < 7; i++) { // 7 fiq banked registers
      cpu->regs_fiq[i] =
          cpu->regs[i + 8]; // 8 registers before the fiq banked registers
    }
    break;
  case 0x12: // IRQ
    cpu->regs_irq[0] = cpu->regs[13];
    cpu->regs_irq[1] = cpu->regs[14];
    break;
  case 0x13: // Supervisor (SWI)
    cpu->regs_svc[0] = cpu->regs[13];
    cpu->regs_svc[1] = cpu->regs[14];
    break;
  case 0x17: // Abort
    cpu->regs_abt[0] = cpu->regs[13];
    cpu->regs_abt[1] = cpu->regs[14];
    break;
  case 0x1B: // Undefined
    cpu->regs_und[0] = cpu->regs[13];
    cpu->regs_und[1] = cpu->regs[14];
    break;
  }

  // write new mode
  switch (new_mode) {
  case 0x10: // User (non-privileged)
  case 0x1F: // System
    cpu->regs[13] = cpu->regs_usr[0];
    cpu->regs[14] = cpu->regs_usr[1];
    break;
  case 0x11: // FIQ
    cpu->SPSR_fiq = cpu->CPSR;
    for (int i = 0; i < 7; i++) { // 7 fiq banked registers
      cpu->regs[i + 8] =
          cpu->regs_fiq[i]; // 8 registers before the fiq banked registers
    }
    break;
  case 0x12: // IRQ
    cpu->SPSR_irq = cpu->CPSR;
    cpu->regs[13] = cpu->regs_irq[0];
    cpu->regs[14] = cpu->regs_irq[1];
    break;
  case 0x13: // Supervisor (SWI)
    cpu->SPSR_svc = cpu->CPSR;
    cpu->regs[13] = cpu->regs_svc[0];
    cpu->regs[14] = cpu->regs_svc[1];
    break;
  case 0x17: // Abort
    cpu->SPSR_abt = cpu->CPSR;
    cpu->regs[13] = cpu->regs_abt[0];
    cpu->regs[14] = cpu->regs_abt[1];
    break;
  case 0x1B: // Undefined
    cpu->SPSR_und = cpu->CPSR;
    cpu->regs[13] = cpu->regs_und[0];
    cpu->regs[14] = cpu->regs_und[1];
    break;
  }

  cpu->CPSR = (cpu->CPSR & ~(0x1F)) | new_mode;
}

static Thumb_Instructions decode_thumb(uint16_t opcode) {
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

static Arm_Instructions decode_arm(uint32_t opcode) {
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
static int check_condition_code(uint32_t CPSR, uint8_t condition_code) {
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

static void decode_data_processing(GBA_CPU *cpu, GBA_Memory *mem,
                                   uint32_t inst) {
  uint8_t opcode = ((inst >> 21) & 0xF);
  uint8_t s = ((inst >> 20) & 0x1);
  uint8_t rn = ((inst >> 16) & 0xF);
  uint8_t rd = ((inst >> 12) & 0xF);
  uint32_t rm = cpu->regs[inst & 0xF];

  uint32_t shifter_operand;
  int8_t shifter_carry_out = -1; // -1 means unset

  // Checks if immediate pg 446 of arm arm
  if (((inst >> 25) & 0x1) == 1) {
    uint8_t rotate_imm_times_2 = ((inst & 0xF00) >> 7);
    uint8_t immed_8 = (inst & 0xFF);

    if (rotate_imm_times_2 == 0) {
      shifter_operand = immed_8;
    } else {
      shifter_operand = ((uint32_t)immed_8 >> rotate_imm_times_2) |
                        ((uint32_t)immed_8 << (32 - rotate_imm_times_2));
      shifter_carry_out = (shifter_operand >> 31);
    }
  } else if ((inst & 0b00000000000000000000111111110000) == 0) {
    // Checks if register pg 448 of arm arm
    shifter_operand = rm;
  } else if ((inst & 0b00000000000000000000111111110000) == 0b000001100000) {
    // Checks if rotate right with extend pg 457 of arm arm
    // 31 - 29 cause the carry flag is already shifted by 29
    shifter_operand = (((cpu->CPSR & CARRY_FLAG) << (31 - 29)) | (rm >> 1));
    shifter_carry_out = (rm & 1);
  } else {
    uint8_t shift_reg = cpu->regs[(inst >> 8) & 0xF] & 0xFF;
    uint8_t shift_imm = ((inst >> 7) & 0x1F);
    uint8_t inst_type = ((inst >> 5) & 0x3);
    uint8_t is_reg = 0;

    // Checks if shift is register
    if (((inst >> 4) & 0x1) == 1) {
      is_reg = 1;
    }

    switch (inst_type) {
    case 3:
      // Rotate
      if (is_reg == 1) {
        if (shift_reg == 0) {
          shifter_operand = rm;
        } else if ((shift_reg & 0xF) == 0) {
          shifter_operand = rm;
          shifter_carry_out = (rm >> 31);
        } else {
          shifter_operand =
              (rm >> (shift_reg & 0xF)) | (rm << (32 - (shift_reg & 0xF)));
          shifter_carry_out = ((rm >> ((shift_reg & 0xF) - 1)) & 1);
        }
      } else {
        // The shift_imm == 0 is handled by RRX
        shifter_operand = (rm >> shift_imm) | (rm << (32 - shift_imm));
        shifter_carry_out = ((rm >> (shift_imm - 1)) & 1);
      }
      break;
    case 2:
      // Arithmatic
      if (is_reg == 1) {
        if (shift_reg == 0) {
          shifter_operand = rm;
        } else if (shift_reg < 32) {
          shifter_operand = (int32_t)rm >> shift_reg;
          shifter_carry_out = ((rm >> (shift_reg - 1)) & 1);
        } else {
          if ((rm >> 31) == 0) {
            shifter_operand = 0;
            shifter_carry_out = 0;
          } else {
            shifter_operand = 0xFFFFFFFF;
            shifter_carry_out = 1;
          }
        }
      } else {
        if (shift_imm == 0) {
          if ((rm >> 31) == 0) {
            shifter_operand = 0;
            shifter_carry_out = 0;
          } else {
            shifter_operand = 0xFFFFFFFF;
            shifter_carry_out = 1;
          }
        } else {
          shifter_operand = (int32_t)rm >> shift_imm;
          shifter_carry_out = ((rm >> (shift_imm - 1)) & 1);
        }
      }
      break;
    case 1:
      // Right shift
      if (is_reg == 1) {
        if (shift_reg == 0) {
          shifter_operand = rm;
        } else if (shift_reg < 32) {
          shifter_operand = rm >> shift_reg;
          shifter_carry_out = ((rm >> (shift_reg - 1)) & 1);
        } else if (shift_reg == 32) {
          shifter_operand = 0;
          shifter_carry_out = (rm >> 31);
        } else {
          shifter_operand = 0;
          shifter_carry_out = 0;
        }
      } else {
        if (shift_imm == 0) {
          shifter_operand = 0;
          shifter_carry_out = (rm >> 31);
        } else {
          shifter_operand = rm >> shift_imm;
          shifter_carry_out = ((rm >> (shift_imm - 1)) & 1);
        }
      }
      break;
    case 0:
      // Left shift
      if (is_reg == 1) {
        if (shift_reg == 0) {
          shifter_operand = rm;
        } else if (shift_reg < 32) {
          shifter_operand = rm << shift_reg;
          shifter_carry_out = ((rm >> (32 - shift_reg)) & 1);
        } else if (shift_reg == 32) {
          shifter_operand = 0;
          shifter_carry_out = (rm & 1);
        } else {
          shifter_operand = 0;
          shifter_carry_out = 0;
        }
      } else {
        if (shift_imm == 0) {
          shifter_operand = rm;
        } else {
          shifter_operand = rm << shift_imm;
          shifter_carry_out = ((rm >> (32 - shift_imm)) & 1);
        }
      }
      break;
    }
  }
}

inline static uint8_t carry_from(uint32_t x, uint32_t y, uint32_t z) {
  if ((uint64_t)x + (uint64_t)y + (uint64_t)z > UINT32_MAX)
    return 1;
  return 0;
}

inline static uint8_t borrow_from(int32_t x, int32_t y, int32_t z) {
  if ((int64_t)x - (int64_t)y - (int64_t)z < 0)
    return 1;
  return 0;
}

inline static uint8_t overflow_from(int32_t x, int32_t y, int32_t result,
                                    uint8_t is_sub) {
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

static void execute_data_processing(GBA_CPU *cpu, GBA_Memory *mem,
                                    uint32_t shifter_operand,
                                    int8_t shifter_carry_out, uint32_t rm,
                                    uint8_t rd, uint8_t rn, uint8_t s,
                                    uint8_t opcode) {
  uint32_t temp;
  uint8_t n_flag = !!(cpu->CPSR & SIGN_FLAG);
  uint8_t z_flag = !!(cpu->CPSR & ZERO_FLAG);
  uint8_t c_flag = !!(cpu->CPSR & CARRY_FLAG);
  uint8_t v_flag = !!(cpu->CPSR & OVERFLOW_FLAG);
  switch (opcode) {
  case 0b0101:
    // ADC
    cpu->regs[rd] = cpu->regs[rn] + shifter_operand + c_flag;
    if (s == 1) {
      if (rd == 0xF) {
        if (cpu->SPSR_abt == 0xF0F0F0F0) // yea its unimplemented now
          cpu->CPSR = cpu->SPSR_abt;
      } else {
        n_flag = cpu->regs[rd] >> 31;
        z_flag = !cpu->regs[rd];
        c_flag = carry_from(cpu->regs[rn], shifter_operand, c_flag);
        v_flag =
            overflow_from(cpu->regs[rn], shifter_operand, cpu->regs[rd], 0);
      }
    }
    break;
  case 0b0100:
    // ADD
    cpu->regs[rd] = cpu->regs[rn] + shifter_operand;
    if (s == 1) {
      if (rd == 0xF) {
        if (cpu->SPSR_abt == 0xF0F0F0F0) // yea its unimplemented now
          cpu->CPSR = cpu->SPSR_abt;
      } else {
        n_flag = cpu->regs[rd] >> 31;
        z_flag = !cpu->regs[rd];
        c_flag = carry_from(cpu->regs[rn], shifter_operand, 0);
        v_flag =
            overflow_from(cpu->regs[rn], shifter_operand, cpu->regs[rd], 0);
      }
    }
    break;
  case 0b0000:
    // AND
    cpu->regs[rd] = cpu->regs[rn] & shifter_operand;
    if (s == 1) {
      if (rd == 0xF) {
        if (cpu->SPSR_abt == 0xF0F0F0F0) // yea its unimplemented now
          cpu->CPSR = cpu->SPSR_abt;
      } else {
        n_flag = cpu->regs[rd] >> 31;
        z_flag = !cpu->regs[rd];
        c_flag = shifter_carry_out;
      }
    }
    break;
  case 0b1110:
    // BIC
    cpu->regs[rd] = cpu->regs[rn] & ~shifter_operand;
    if (s == 1) {
      if (rd == 0xF) {
        if (cpu->SPSR_abt == 0xF0F0F0F0) // yea its unimplemented now
          cpu->CPSR = cpu->SPSR_abt;
      } else {
        n_flag = cpu->regs[rd] >> 31;
        z_flag = !cpu->regs[rd];
        c_flag = shifter_carry_out;
      }
    }
    break;
  case 0b1011:
    // CMN
    // Doesn't need to check S for updating flags
    temp = cpu->regs[rn] + shifter_operand;
    n_flag = temp >> 31;
    z_flag = !temp;
    c_flag = carry_from(cpu->regs[rn], shifter_operand, 0);
    v_flag = overflow_from(cpu->regs[rn], shifter_operand, temp, 0);
    break;
  case 0b1010:
    // CMP
    // Doesn't need to check S for updating flags
    temp = cpu->regs[rn] - shifter_operand;
    n_flag = temp >> 31;
    z_flag = !temp;
    c_flag = !borrow_from(cpu->regs[rn], shifter_operand, 0);
    v_flag = overflow_from(cpu->regs[rn], shifter_operand, temp, 1);
    break;
  case 0b0001:
    // EOR
    cpu->regs[rd] = cpu->regs[rn] ^ shifter_operand;
    if (s == 1) {
      if (rd == 0xF) {
        if (cpu->SPSR_abt == 0xF0F0F0F0) // yea its unimplemented now
          cpu->CPSR = cpu->SPSR_abt;
      } else {
        n_flag = cpu->regs[rd] >> 31;
        z_flag = !cpu->regs[rd];
        c_flag = shifter_carry_out;
      }
    }
    break;
  case 0b1101:
    // MOV
    cpu->regs[rd] = shifter_operand;
    if (s == 1) {
      if (rd == 0xF) {
        if (cpu->SPSR_abt == 0xF0F0F0F0) // yea its unimplemented now
          cpu->CPSR = cpu->SPSR_abt;
      } else {
        n_flag = cpu->regs[rd] >> 31;
        z_flag = !cpu->regs[rd];
        c_flag = shifter_carry_out;
      }
    }
    break;
  case 0b1111:
    // MVN
    cpu->regs[rd] = ~shifter_operand;
    if (s == 1) {
      if (rd == 0xF) {
        if (cpu->SPSR_abt == 0xF0F0F0F0) // yea its unimplemented now
          cpu->CPSR = cpu->SPSR_abt;
      } else {
        n_flag = cpu->regs[rd] >> 31;
        z_flag = !cpu->regs[rd];
        c_flag = shifter_carry_out;
      }
    }
    break;
  case 0b1100:
    // ORR
    cpu->regs[rd] = cpu->regs[rn] | shifter_operand;
    if (s == 1) {
      if (rd == 0xF) {
        if (cpu->SPSR_abt == 0xF0F0F0F0) // yea its unimplemented now
          cpu->CPSR = cpu->SPSR_abt;
      } else {
        n_flag = cpu->regs[rd] >> 31;
        z_flag = !cpu->regs[rd];
        c_flag = shifter_carry_out;
      }
    }
    break;
  case 0b0011:
    // RSB
    cpu->regs[rd] = shifter_operand - cpu->regs[rn];
    if (s == 1) {
      if (rd == 0xF) {
        if (cpu->SPSR_abt == 0xF0F0F0F0) // yea its unimplemented now
          cpu->CPSR = cpu->SPSR_abt;
      } else {
        n_flag = cpu->regs[rd] >> 31;
        z_flag = !cpu->regs[rd];
        c_flag = !borrow_from(shifter_operand, cpu->regs[rn], 0);
        v_flag =
            overflow_from(shifter_operand, cpu->regs[rn], cpu->regs[rd], 1);
      }
    }
    break;
  case 0b0111:
    // RSC
    cpu->regs[rd] = shifter_operand - cpu->regs[rn] - !c_flag;
    if (s == 1) {
      if (rd == 0xF) {
        if (cpu->SPSR_abt == 0xF0F0F0F0) // yea its unimplemented now
          cpu->CPSR = cpu->SPSR_abt;
      } else {
        n_flag = cpu->regs[rd] >> 31;
        z_flag = !cpu->regs[rd];
        c_flag = !borrow_from(shifter_operand, cpu->regs[rn], !c_flag);
        v_flag =
            overflow_from(shifter_operand, cpu->regs[rn], cpu->regs[rd], 1);
      }
    }
    break;
  case 0b0110:
    // SBC
    cpu->regs[rd] = cpu->regs[rn] - shifter_operand - !c_flag;
    if (s == 1) {
      if (rd == 0xF) {
        if (cpu->SPSR_abt == 0xF0F0F0F0) // yea its unimplemented now
          cpu->CPSR = cpu->SPSR_abt;
      } else {
        n_flag = cpu->regs[rd] >> 31;
        z_flag = !cpu->regs[rd];
        c_flag = !borrow_from(cpu->regs[rn], shifter_operand, !c_flag);
        v_flag =
            overflow_from(cpu->regs[rn], shifter_operand, cpu->regs[rd], 1);
      }
    }
    break;
  case 0b0010:
    // SUB
    cpu->regs[rd] = cpu->regs[rn] - shifter_operand;
    if (s == 1) {
      if (rd == 0xF) {
        if (cpu->SPSR_abt == 0xF0F0F0F0) // yea its unimplemented now
          cpu->CPSR = cpu->SPSR_abt;
      } else {
        n_flag = cpu->regs[rd] >> 31;
        z_flag = !cpu->regs[rd];
        c_flag = !borrow_from(cpu->regs[rn], shifter_operand, 0);
        v_flag =
            overflow_from(cpu->regs[rn], shifter_operand, cpu->regs[rd], 1);
      }
    }
    break;
  case 0b1001:
    // TEQ
    temp = cpu->regs[rn] ^ shifter_operand;
    n_flag = temp >> 31;
    z_flag = !temp;
    c_flag = shifter_carry_out;
    break;
  case 0b1000:
    // TST
    temp = cpu->regs[rn] & shifter_operand;
    n_flag = temp >> 31;
    z_flag = !temp;
    c_flag = shifter_carry_out;
    break;
  }

  cpu->CPSR = (cpu->CPSR & 0x0FFFFFFF) | (v_flag << 28) | (c_flag << 29) |
              (z_flag << 30) | (n_flag << 31);
}

void run_cpu(GBA_CPU *cpu, GBA_Memory *mem) {
  if (cpu->CPSR & CPSR_T_BIT) { // Check the 5th bit for arm/thumb mode
    // Thumb
    uint16_t inst = readmem16(mem, cpu->regs[15]);
    cpu->regs[15] += 2;
    switch (decode_thumb(inst)) {}
  } else {
    // Arm
    uint32_t inst = readmem_32(mem, cpu->regs[15]);
    cpu->regs[15] += 4;

    if (check_condition_code(cpu->CPSR, ((inst & 0xF0000000) >> 28)))
      return;

    switch (decode_arm(inst)) {
    case DataProcessing:
      decode_data_processing(cpu, mem, inst);
      break;
    }
  }
}
