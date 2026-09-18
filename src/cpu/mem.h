#pragma once
#include "gba.h"
#include <stdint.h>

uint8_t readmem8(GBA_Memory *mem, uint32_t addr);
uint16_t readmem16(GBA_Memory *mem, uint32_t addr);
uint32_t readmem32(GBA_Memory *mem, uint32_t addr);

void *writemem8(GBA_Memory *mem, uint32_t addr, uint8_t data);
void *writemem16(GBA_Memory *mem, uint32_t addr, uint16_t data);
void *writemem32(GBA_Memory *mem, uint32_t addr, uint32_t data);
