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

uint16_t readmem_16(GBA_Memory *mem, uint32_t addr) {
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
