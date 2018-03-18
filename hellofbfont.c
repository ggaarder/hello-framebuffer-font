#include <linux/fb.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

int fb_fd;
struct fb_fix_screeninfo finfo;
struct fb_var_screeninfo vinfo;
long screensize;
uint8_t *fbp;

static void init(void) {
    fb_fd = open("/dev/fb0",O_RDWR);
    //Get variable screen information                                             
    ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
    vinfo.grayscale=0;
    vinfo.bits_per_pixel=32;
    ioctl(fb_fd, FBIOPUT_VSCREENINFO, &vinfo);
    ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
    ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo);
    screensize = vinfo.yres_virtual * finfo.line_length;
    fbp = mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, (off_t)0);
}

static inline int putpixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x >= vinfo.xres || x < 0 || y >= vinfo.yres || y < 0) return -1;
    long location = (x+vinfo.xoffset) * (vinfo.bits_per_pixel/8) + (y+vinfo.yoffset) * finfo.line_length;
    uint32_t color = (r<<vinfo.red.offset) | (g<<vinfo.green.offset) | (b<<vinfo.blue.offset);
    *((uint32_t*)(fbp + location)) = color;
    return 0;
}

#include "font8x8.h"

FILE *bmp;

int (*putpixel_fn)(int x, int y, uint8_t r, uint8_t g, uint8_t b) = putpixel;

int putpixel_file(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  fprintf(bmp, "<rect x=\"%d\" y=\"%d\" width=\"1\" height=\"1\" stroke=\"green\" file=\"green\"/>\n", x, y);
}

#define PIXELSIZ 2
#define CHARSIZ (PIXELSIZ*8)

static inline void square(int x0, int y0, int x1, int y1) {
  int x, y;
  for (x=x0; x<=x1; ++x)
    for (y=y0; y<=y1; ++y)
      putpixel_fn(x, y, 0x00, 0xFF, 0x00);
}
  
static inline void render(const char *bitmap, int x, int y) {
  int xi, yi, set, i, x0, x1, y0, y1, xirot, yirot, xirot2, yirot2;
  for (xi = 0; xi < 8; ++xi)
    for (yi = 0; yi < 8; ++yi) {
      xirot = xi; yirot = 8-yi;
      xirot2 = 8-yirot; yirot2 = xirot; xirot = xirot2; yirot = yirot2;
      
      set = bitmap[xirot] & 1 << yirot;
      if (set) { // draw not a pixel (x, y) but a square with a=CHARSIZE
        x0 = x+PIXELSIZ*xi;
        y0 = y+PIXELSIZ*yi;
        x1 = x0+PIXELSIZ-1;
        y1 = y0+PIXELSIZ-1;
        square(x0, y0, x1, y1);
      }
    }
}

static inline int draw_font(const char *bitmap, int *x, int *y) {
  if (*x+CHARSIZ >= vinfo.xres)
    { *x = 0; *y += CHARSIZ; return draw_font(bitmap, x, y); }
  if (*y >= vinfo.yres) { return -1; }
  render(bitmap, *x, *y);
  *x += CHARSIZ;
  return 0;
}

#define DRAW_FONT(ftname) \
  fontsiz = sizeof(ftname)/sizeof(ftname[0]); \
  for (i = 0; i < fontsiz && y < vinfo.yres; ++i) { \
    draw_font(ftname[i], &x, &y);                   \
  }

void bkg() {
  int x, y;
  for (x = 0; x < vinfo.xres; ++x)
    for (y = 0; y < vinfo.yres; ++y)
      putpixel(x, y, 0x00, 0x00, 0x00);
}

int main() {
  init();
  int fontsiz, x=0, y=0, i;
  vinfo.xres = CHARSIZ*50;
  bkg();

  // print to fb
  DRAW_FONT(font8x8_ascii);
  DRAW_FONT(font8x8_sga);
  DRAW_FONT(font8x8_greek);
  DRAW_FONT(font8x8_hiragana);
  DRAW_FONT(font8x8_box);
  DRAW_FONT(font8x8_block);
  DRAW_FONT(font8x8_misc);
  
  // print to bmp file
  bmp = fopen("out.svg", "wb");
  if (!bmp) {
    printf("Failed creating svg.\n");
    return -1;
  }
  x = 0; y = 0;
  fprintf(bmp, "<svg width=\"%d\" height=\"%d\" viewBox=\"0 0 %d %d\">\n",
          vinfo.xres, vinfo.yres, vinfo.xres, vinfo.yres);
  putpixel_fn = putpixel_file;
  DRAW_FONT(font8x8_ascii);
  DRAW_FONT(font8x8_sga);
  DRAW_FONT(font8x8_greek);
  DRAW_FONT(font8x8_hiragana);
  DRAW_FONT(font8x8_box);
  DRAW_FONT(font8x8_block);
  DRAW_FONT(font8x8_misc);
  fprintf(bmp, "</svg>\n");
  fclose(bmp);
  
  return 0;
}
