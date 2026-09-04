#include "display.h"
#include "gba.h"
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture;

int init_display() {
  if (!SDL_CreateWindowAndRenderer("GBA", DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                   SDL_WINDOW_RESIZABLE, &window, &renderer)) {
    printf("Couldn't create window/renderer: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  SDL_SetRenderLogicalPresentation(renderer, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                   SDL_LOGICAL_PRESENTATION_LETTERBOX);

  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                              SDL_TEXTUREACCESS_STREAMING, DISPLAY_WIDTH,
                              DISPLAY_HEIGHT);
  SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

  return SDL_APP_CONTINUE;
}
