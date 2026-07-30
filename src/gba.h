#pragma once
#include <stdint.h>
#include <stdio.h>

#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 160
#define MAX_ROM_SIZE 33554432

typedef struct {
  uint8_t BIOS[0x4000];
  uint8_t ob_WRAM[0x40000]; // on board work ram
  uint8_t oc_WRAM[0x8000];  // on chip work ram
  uint8_t VRAM[0x18000];
  uint8_t OAM[0x400];
  uint8_t palette[0x400];
  uint8_t IO_regs[0x400];
  uint8_t SRAM[0x10000];
  uint8_t *game_ROM;
  size_t rom_size;
} GBA_Memory;
