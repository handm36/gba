#pragma once
#include "gba.h"
#include <stdint.h>

void handle_CPSR_mode_switch(GBA_CPU *cpu, uint8_t new_mode);
void run_cpu(GBA_CPU *cpu, GBA_Memory *mem);
