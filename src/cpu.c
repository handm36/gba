#include "cpu.h"
#include "gba.h"
#include <stdint.h>
#include <string.h>

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

void run_cpu(GBA_CPU *cpu, GBA_Memory *mem) {
  if (cpu->CPSR & CPSR_T_BIT) { // Check the 5th bit for arm/thumb mode
    // Thumb
    uint16_t opcode = readmem16(mem, cpu->regs[15]);
    cpu->regs[15] += 2;
    switch (decode_thumb(opcode)) {}
  } else {
    // Arm
    uint32_t opcode = readmem_32(mem, cpu->regs[15]);
    cpu->regs[15] += 4;

    if (check_condition_code(cpu->CPSR, ((opcode & 0xF0000000) >> 28)))
      return;

    switch (decode_arm(opcode)) {}
  }
}
