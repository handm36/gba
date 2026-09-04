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

typedef struct {
  uint32_t regs[16];
  uint32_t regs_fiq[7]; // starts from R8
  uint32_t regs_svc[2]; // R13_svc and R14_svc
  uint32_t regs_abt[2]; // R13_abt and R14_abt
  uint32_t regs_irq[2]; // R13_irq and R14_irq
  uint32_t regs_und[2]; // R13_und and R14_und
  uint32_t regs_usr[2]; // R13 and R14 for user
  uint32_t CPSR;
  uint32_t SPSR_fiq;
  uint32_t SPSR_svc;
  uint32_t SPSR_abt;
  uint32_t SPSR_irq;
  uint32_t SPSR_und;
} GBA_CPU;
