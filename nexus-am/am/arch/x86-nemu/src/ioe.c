#include <am.h>
#include <x86.h>

#define RTC_PORT 0x48   // Note that this is not standard
#define VGA_SYNC_PORT 0x100
static unsigned long boot_time;

void _ioe_init() {
  boot_time = inl(RTC_PORT);
}

unsigned long _uptime() {
  return inl(RTC_PORT) - boot_time;
}

uint32_t* const fb = (uint32_t *)0x40000;

_Screen _screen = {
  .width  = 400,
  .height = 300,
};

extern void* memcpy(void *, const void *, int);

void _draw_rect(const uint32_t *pixels, int x, int y, int w, int h) {
  if (w <= 0 || h <= 0) return;
  int src_w = w;

  int src_x = 0, src_y = 0;
  if (x < 0) { src_x = -x; w += x; x = 0; }
  if (y < 0) { src_y = -y; h += y; y = 0; }
  if (x + w > _screen.width)  w = _screen.width - x;
  if (y + h > _screen.height) h = _screen.height - y;
  if (w <= 0 || h <= 0) return;

  int row;
  for (row = 0; row < h; row++) {
    const uint32_t *src = pixels + (src_y + row) * src_w + src_x;
    memcpy(fb + (y + row) * _screen.width + x, src, w * sizeof(uint32_t));
  }
}

void _draw_sync() {
  outb(VGA_SYNC_PORT, 0);
}

int _read_key() {
  if ((inb(0x64) & 0x1) == 0) {
    return _KEY_NONE;
  }
  return inl(0x60);
}
