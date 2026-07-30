#pragma once
#include "gba.h"
#include <stdint.h>

uint8_t readmem_8(GBA_Memory *mem, uint32_t address);
uint16_t readmem_16(GBA_Memory *mem, uint32_t address);
uint32_t readmem_32(GBA_Memory *mem, uint32_t address);
