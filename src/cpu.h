#pragma once
#include "gba.h"
#include <stdint.h>

#define CPSR_T_BIT (1 << 5)
#define CPSR_F_BIT (1 << 6)
#define CPSR_I_BIT (1 << 7)

#define OVERFLOW_FLAG (1 << 28)
#define CARRY_FLAG (1 << 29)
#define ZERO_FLAG (1 << 30)
#define SIGN_FLAG (1 << 31)

uint8_t readmem_8(GBA_Memory *mem, uint32_t address);

uint16_t readmem_16(GBA_Memory *mem, uint32_t address);
uint32_t readmem_32(GBA_Memory *mem, uint32_t address);

void handle_CPSR_mode_switch(GBA_CPU *cpu, uint8_t new_mode);
void run_cpu(GBA_CPU *cpu, GBA_Memory *mem);
