#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdint.h> // For uint8_t, etc.

// VGA Colors
#define VGA_COLOR_BLACK         0
#define VGA_COLOR_BLUE          1
#define VGA_COLOR_GREEN         2
#define VGA_COLOR_CYAN          3
#define VGA_COLOR_RED           4
#define VGA_COLOR_MAGENTA       5
#define VGA_COLOR_BROWN         6
#define VGA_COLOR_LIGHT_GREY    7
#define VGA_COLOR_DARK_GREY     8
#define VGA_COLOR_LIGHT_BLUE    9
#define VGA_COLOR_LIGHT_GREEN   10
#define VGA_COLOR_LIGHT_CYAN    11
#define VGA_COLOR_LIGHT_RED     12
#define VGA_COLOR_LIGHT_MAGENTA 13
#define VGA_COLOR_LIGHT_BROWN   14 // Often yellow
#define VGA_COLOR_WHITE         15

// Default color: White on Black
#define DEFAULT_COLOR ((VGA_COLOR_BLACK << 4) | VGA_COLOR_WHITE)

void console_init(uint8_t background_color = VGA_COLOR_BLACK, uint8_t foreground_color = VGA_COLOR_WHITE);
void console_clear(uint8_t background_color = VGA_COLOR_BLACK, uint8_t foreground_color = VGA_COLOR_WHITE);

void console_putc_at(char c, uint8_t color, int x, int y);
void console_write_char(char c, uint8_t color); // Handles cursor, newline, scroll
void console_write_string(const char* str, uint8_t color);
void console_write_dec(unsigned int n, uint8_t color);
void console_write_hex(unsigned int n, uint8_t color);

void console_set_cursor(int x, int y);
void console_get_cursor(int* x, int* y);

// Helper to create VGA color byte
static inline uint8_t vga_entry_color(uint8_t fg, uint8_t bg) {
    return fg | bg << 4;
}

#endif // CONSOLE_H
