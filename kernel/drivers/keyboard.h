#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

void keyboard_init(void);
char keyboard_read_char(void);
char keyboard_scancode_to_ascii(uint8_t scancode);
uint8_t keyboard_read_scancode(void);

#endif
