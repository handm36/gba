#define SDL_MAIN_USE_CALLBACKS 1
#include "audio/audio.h"
#include "cpu/cpu.h"
#include "display/display.h"
#include "gba.h"
#include "input/input.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static SDL_AppResult parse_file(GBA_Memory *mem, char *path) {
  FILE *file_ptr = fopen(path, "rb");

  if (file_ptr == NULL) {
    printf("No file specified\n");
    return SDL_APP_FAILURE;
  }

  if (fseek(file_ptr, 0, SEEK_END) != 0) {
    fclose(file_ptr);
    printf("Failed to read file\n");
    return SDL_APP_FAILURE;
  }

  long size = ftell(file_ptr);
  if (size == -1) {
    fclose(file_ptr);
    printf("Failed to get file size\n");
    return SDL_APP_FAILURE;
  }

  if ((size == 0) || (size >= MAX_ROM_SIZE)) {
    fclose(file_ptr);
    printf("Invalid GBA rom file\n");
    return SDL_APP_FAILURE;
  }
  fseek(file_ptr, 0, SEEK_SET);

  uint8_t *game_ROM = malloc(size);
  if (game_ROM == NULL) {
    fclose(file_ptr);
    perror("Couldn't allocate memory");
    return SDL_APP_FAILURE;
  }

  size_t ret = fread(game_ROM, 1, size, file_ptr);

  if (ret != size) {
    fclose(file_ptr);
    free(game_ROM);
    printf("Failed to read file\n");
    return SDL_APP_FAILURE;
  }

  mem->game_ROM = game_ROM;
  mem->rom_size = size;

  fclose(file_ptr);
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
    printf("Couldn't initialize SDL: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  SDL_SetAppMetadata("GBA", "1.0", "com.gba");

  if (init_display() == SDL_APP_FAILURE) {
    return SDL_APP_FAILURE;
  }

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
  if (event->type == SDL_EVENT_QUIT) {
    return SDL_APP_SUCCESS;
  }
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) { return SDL_APP_CONTINUE; }

void SDL_AppQuit(void *appstate, SDL_AppResult result) {}
