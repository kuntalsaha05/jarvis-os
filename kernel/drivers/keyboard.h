#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

void keyboard_init(void);
void keyboard_handler(void);
int keyboard_enqueue(uint8_t scancode);
int keyboard_dequeue(uint8_t *scancode);
uint32_t keyboard_get_irq_count(void);
char keyboard_read_char(void);
char keyboard_scancode_to_ascii(uint8_t scancode);
uint8_t keyboard_read_scancode(void);

#endif
