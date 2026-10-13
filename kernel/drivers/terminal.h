#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdint.h>

void terminal_initialize(void);
void terminal_clear(void);
void terminal_putchar(char c);
void terminal_write(const char *str);
void terminal_write_number(uint32_t number);
void terminal_set_color(uint8_t color);

void terminal_write_at(
    uint8_t row,
    uint8_t column,
    const char *text
);

#endif
