#ifndef FONT_H
#define FONT_H

#include <stdint.h>

#define FONT_WIDTH  8
#define FONT_HEIGHT 16

/* Standard 8x16 Bitmap Font for ASCII 0x20 (' ') to 0x7E ('~') */
extern const uint8_t font_8x16[96][16];

#endif /* FONT_H */

