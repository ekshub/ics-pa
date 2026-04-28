#include "common.h"

#ifdef HAS_IOE

#include "device/mmio.h"
#include "device/port-io.h"
#include <SDL2/SDL.h>

#define VMEM 0x40000
#define VGA_SYNC_PORT 0x100

#define SCREEN_H 300
#define SCREEN_W 400

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;

static uint32_t (*vmem) [SCREEN_W];
static uint8_t *vga_sync_port_base;
static bool vmem_dirty = false;

void vga_vmem_io_handler(paddr_t addr, int len, bool is_write) {
  if (is_write) {
    vmem_dirty = true;
  }
}

void update_screen() {
  if (!vmem_dirty) {
    return;
  }

  SDL_UpdateTexture(texture, NULL, vmem, SCREEN_W * sizeof(vmem[0][0]));
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, NULL, NULL);
  SDL_RenderPresent(renderer);
  vmem_dirty = false;
}

void vga_sync() {
  vmem_dirty = true;
  update_screen();
}

static void vga_sync_io_handler(ioaddr_t addr, int len, bool is_write) {
  if (is_write) {
    assert(addr == VGA_SYNC_PORT && len == 1);
    vga_sync();
  }
}

void init_vga() {
  SDL_Init(SDL_INIT_VIDEO);
  SDL_CreateWindowAndRenderer(SCREEN_W * 2, SCREEN_H * 2, 0, &window, &renderer);
  SDL_SetWindowTitle(window, "NEMU");
  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
      SDL_TEXTUREACCESS_STATIC, SCREEN_W, SCREEN_H);

  vmem = add_mmio_map(VMEM, 0x80000, vga_vmem_io_handler);
  vga_sync_port_base = add_pio_map(VGA_SYNC_PORT, 1, vga_sync_io_handler);
  vga_sync_port_base[0] = 0;
  vmem_dirty = true;
}
#endif	/* HAS_IOE */
