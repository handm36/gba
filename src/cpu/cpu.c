#include "cpu.h"
#include "helper.h"
#include "mem.h"
#include <stdint.h>

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

static void execute_data_processing(GBA_CPU *cpu, uint32_t shifter_operand,
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
        if (current_mode_has_SPSR(cpu))
          cpu->CPSR = get_current_spsr(cpu);
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
        if (current_mode_has_SPSR(cpu))
          cpu->CPSR = get_current_spsr(cpu);
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
        if (current_mode_has_SPSR(cpu))
          cpu->CPSR = get_current_spsr(cpu);
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
        if (current_mode_has_SPSR(cpu))
          cpu->CPSR = get_current_spsr(cpu);
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
        if (current_mode_has_SPSR(cpu))
          cpu->CPSR = get_current_spsr(cpu);
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
        if (current_mode_has_SPSR(cpu))
          cpu->CPSR = get_current_spsr(cpu);
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
        if (current_mode_has_SPSR(cpu))
          cpu->CPSR = get_current_spsr(cpu);
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
        if (current_mode_has_SPSR(cpu))
          cpu->CPSR = get_current_spsr(cpu);
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
        if (current_mode_has_SPSR(cpu))
          cpu->CPSR = get_current_spsr(cpu);
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
        if (current_mode_has_SPSR(cpu))
          cpu->CPSR = get_current_spsr(cpu);
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
        if (current_mode_has_SPSR(cpu))
          cpu->CPSR = get_current_spsr(cpu);
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
        if (current_mode_has_SPSR(cpu))
          cpu->CPSR = get_current_spsr(cpu);
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

  cpu->CPSR =
      (cpu->CPSR & ~(OVERFLOW_FLAG | CARRY_FLAG | ZERO_FLAG | SIGN_FLAG)) |
      (v_flag << OVERFLOW_FLAG_LOC) | (c_flag << CARRY_FLAG_LOC) |
      (z_flag << ZERO_FLAG_LOC) | (n_flag << SIGN_FLAG_LOC);
}

static void decode_execute_data_processing(GBA_CPU *cpu, uint32_t inst) {
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

  execute_data_processing(cpu, shifter_operand, shifter_carry_out, rm, rd, rn,
                          s, opcode);
}

static void decode_execute_multiply(GBA_CPU *cpu, uint32_t inst) {
  uint64_t temp;

  uint8_t opcode = ((inst >> 21) & 0x7F);
  uint8_t s = ((inst >> 20) & 0x1);
  uint8_t rd = ((inst >> 16) & 0xF);
  uint8_t rdlo = ((inst >> 12) & 0xF);
  uint32_t rn = cpu->regs[(inst >> 12) & 0xF];
  uint32_t rs = cpu->regs[(inst >> 8) & 0xF];
  uint32_t rm = cpu->regs[inst & 0xF];

  uint8_t n_flag = !!(cpu->CPSR & SIGN_FLAG);
  uint8_t z_flag = !!(cpu->CPSR & ZERO_FLAG);

  switch (opcode) {
  // MUL
  case 0b0000000:
    cpu->regs[rd] = (rm * rs);
    if (s == 1) {
      n_flag = cpu->regs[rd] >> 31;
      z_flag = !cpu->regs[rd];
    }

    break;
  // MLA
  case 0b0000001:
    cpu->regs[rd] = (rm * rs + rn);
    if (s == 1) {
      n_flag = cpu->regs[rd] >> 31;
      z_flag = !cpu->regs[rd];
    }

    break;
  // UMULL
  case 0b0000100:
    temp = ((uint64_t)rm * (uint64_t)rs);
    cpu->regs[rdlo] = temp & UINT32_MAX;
    cpu->regs[rd] = temp >> 32;

    if (s == 1) {
      n_flag = cpu->regs[rd] >> 31;
      z_flag = (!cpu->regs[rd] && !cpu->regs[rdlo]);
    }

    break;
  // UMLAL
  case 0b0000101:
    temp = ((uint64_t)rm * (uint64_t)rs);
    temp += ((uint64_t)cpu->regs[rd] << 32) | cpu->regs[rdlo];
    cpu->regs[rd] = (temp >> 32);
    cpu->regs[rdlo] = temp & UINT32_MAX;

    if (s == 1) {
      n_flag = cpu->regs[rd] >> 31;
      z_flag = (!cpu->regs[rd] && !cpu->regs[rdlo]);
    }

    break;
  // SMULL
  case 0b0000110:
    temp = ((int64_t)(int32_t)rm * (int64_t)(int32_t)rs);
    cpu->regs[rdlo] = temp & UINT32_MAX;
    cpu->regs[rd] = temp >> 32;

    if (s == 1) {
      n_flag = cpu->regs[rd] >> 31;
      z_flag = (!cpu->regs[rd] && !cpu->regs[rdlo]);
    }

    break;
  // SMLAL
  case 0b0000111:
    temp = ((int64_t)(int32_t)rm * (int64_t)(int32_t)rs);
    temp += ((int64_t)cpu->regs[rd] << 32) | cpu->regs[rdlo];
    cpu->regs[rd] = (temp >> 32);
    cpu->regs[rdlo] = temp & UINT32_MAX;

    if (s == 1) {
      n_flag = cpu->regs[rd] >> 31;
      z_flag = (!cpu->regs[rd] && !cpu->regs[rdlo]);
    }

    break;
  default:
    break;
  }

  cpu->CPSR = (cpu->CPSR & ~(ZERO_FLAG | SIGN_FLAG)) |
              (z_flag << ZERO_FLAG_LOC) | (n_flag << SIGN_FLAG_LOC);
}

static void decode_execute_branch_branch_link(GBA_CPU *cpu, uint32_t inst) {
  uint8_t l = (inst >> 24) & 0x1;
  int32_t signed_immed_24 = inst & 0xFFFFFF;

  // B and BL
  if (l == 1)
    cpu->regs[14] = cpu->regs[15] + 4;

  cpu->regs[15] += (signed_immed_24 << 8) >> 6;
}

static void decode_execute_branch_exchange(GBA_CPU *cpu, uint32_t inst) {
  uint32_t rm = cpu->regs[inst & 0xF];

  // BX
  cpu->CPSR = (cpu->CPSR & ~CPSR_T_BIT) | ((rm & 0x1) << CPSR_T_LOC);
  cpu->regs[15] = rm & 0xFFFFFFFE;
}

void run_cpu(GBA_CPU *cpu, GBA_Memory *mem) {
  if (cpu->CPSR & CPSR_T_BIT) { // Check the 5th bit for arm/thumb mode
    // Thumb
    uint16_t inst = readmem16(mem, cpu->regs[15]);
    cpu->regs[15] += 2;
    switch (decode_thumb(inst)) {}
  } else {
    // Arm
    uint32_t inst = readmem32(mem, cpu->regs[15]);
    cpu->regs[15] += 4;

    if (check_condition_code(cpu->CPSR, ((inst & 0xF0000000) >> 28)))
      return;

    switch (decode_arm(inst)) {
    case DataProcessing:
      decode_execute_data_processing(cpu, inst);
      break;
    case Multiply:
      decode_execute_multiply(cpu, inst);
      break;
    case BranchAndBranchWithLink:
      decode_execute_branch_branch_link(cpu, inst);
      break;
    case BranchAndBranchExchange:
      decode_execute_branch_exchange(cpu, inst);
      break;
    }
  }
}
