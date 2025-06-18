#include "console.h"
#include <stddef.h> // For size_t, NULL

// VGA I/O Ports
#define VGA_CTRL_REGISTER 0x3D4
#define VGA_DATA_REGISTER 0x3D5

// VGA Register Commands for Cursor
#define VGA_CMD_CURSOR_HIGH_BYTE 14
#define VGA_CMD_CURSOR_LOW_BYTE  15

// Default VGA Buffer Address (Physical)
// This will be updated/remapped by VMM
volatile unsigned short* vga_buffer = (unsigned short*)0xB8000;
const int VGA_WIDTH = 80;
const int VGA_HEIGHT = 25;

int cursor_x = 0;
int cursor_y = 0;
uint8_t current_color = DEFAULT_COLOR;


// Basic outportb function (platform specific, often in a separate utils file)
// For now, a simple inline assembly version for GCC/Clang.
static inline void outb(unsigned short port, unsigned char val) {
    asm volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

// Basic inportb function
static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    asm volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

void console_set_cursor_hw(int x, int y) {
    unsigned short position = (y * VGA_WIDTH) + x;
    outb(VGA_CTRL_REGISTER, VGA_CMD_CURSOR_HIGH_BYTE);
    outb(VGA_DATA_REGISTER, (position >> 8) & 0xFF);
    outb(VGA_CTRL_REGISTER, VGA_CMD_CURSOR_LOW_BYTE);
    outb(VGA_DATA_REGISTER, position & 0xFF);
}

void console_set_cursor(int x, int y) {
    cursor_x = x;
    cursor_y = y;
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = VGA_WIDTH - 1;
    }
    if (cursor_x < 0) {
        cursor_x = 0;
    }
    if (cursor_y >= VGA_HEIGHT) {
        cursor_y = VGA_HEIGHT - 1;
    }
    if (cursor_y < 0) {
        cursor_y = 0;
    }
    console_set_cursor_hw(cursor_x, cursor_y);
}

void console_get_cursor(int* x, int* y) {
    *x = cursor_x;
    *y = cursor_y;
}


void console_clear(uint8_t background, uint8_t foreground) {
    uint8_t color_byte = vga_entry_color(foreground, background);
    unsigned short blank = (color_byte << 8) | ' '; // Space character with given color

    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = blank;
        }
    }
    cursor_x = 0;
    cursor_y = 0;
    console_set_cursor_hw(cursor_x, cursor_y);
    current_color = color_byte;
}

void console_init(uint8_t background, uint8_t foreground) {
    console_clear(background, foreground);
    // Any other init, like enabling cursor visibility if needed (often enabled by default)
}


void console_putc_at(char c, uint8_t color, int x, int y) {
    if (x >= 0 && x < VGA_WIDTH && y >= 0 && y < VGA_HEIGHT) {
        vga_buffer[y * VGA_WIDTH + x] = (color << 8) | c;
    }
}

static void scroll_screen() {
    // Move all lines up by one
    for (int y = 0; y < VGA_HEIGHT - 1; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = vga_buffer[(y + 1) * VGA_WIDTH + x];
        }
    }

    // Clear the last line
    unsigned short blank = (current_color << 8) | ' ';
    for (int x = 0; x < VGA_WIDTH; x++) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = blank;
    }
    cursor_y = VGA_HEIGHT - 1; // Cursor stays on the last line after scroll
}

void console_write_char(char c, uint8_t color) {
    current_color = color; // Update current color for potential scrolls

    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\b') { // Backspace
        if (cursor_x > 0) {
            cursor_x--;
            console_putc_at(' ', color, cursor_x, cursor_y); // Erase char at new pos
        } else if (cursor_y > 0) { // Backspace at start of line
            cursor_y--;
            cursor_x = VGA_WIDTH - 1;
            // Optionally, could erase the char at the end of the previous line
            // console_putc_at(' ', color, cursor_x, cursor_y);
        }
    } else {
        console_putc_at(c, color, cursor_x, cursor_y);
        cursor_x++;
    }

    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }

    if (cursor_y >= VGA_HEIGHT) {
        scroll_screen();
    }
    console_set_cursor_hw(cursor_x, cursor_y);
}

void console_write_string(const char* str, uint8_t color) {
    if (!str) return;
    while (*str) {
        console_write_char(*str++, color);
    }
}

void console_write_dec(unsigned int n, uint8_t color) {
    if (n == 0) {
        console_write_char('0', color);
        return;
    }
    char buffer[11]; // Max 10 digits for uint32_t + null terminator
    int i = 0;
    while (n > 0) {
        buffer[i++] = (n % 10) + '0';
        n /= 10;
    }
    buffer[i] = '\0';

    // Reverse the buffer
    for (int j = 0; j < i / 2; j++) {
        char temp = buffer[j];
        buffer[j] = buffer[i - j - 1];
        buffer[i - j - 1] = temp;
    }
    console_write_string(buffer, color);
}

void console_write_hex(unsigned int n, uint8_t color) {
    console_write_string("0x", color);
    if (n == 0) {
        console_write_char('0', color);
        return;
    }

    char buffer[9]; // Max 8 hex digits for uint32_t + null terminator
    int i = 0;
    bool leading_zeros = true;

    for (int j = 7; j >= 0; j--) {
        unsigned char hex_digit = (n >> (j * 4)) & 0xF;
        if (hex_digit != 0 || !leading_zeros || j == 0) {
            leading_zeros = false;
            if (hex_digit < 10) {
                buffer[i++] = hex_digit + '0';
            } else {
                buffer[i++] = hex_digit - 10 + 'A';
            }
        }
    }
    buffer[i] = '\0';
    console_write_string(buffer, color);
}

// This global variable will be set by VMM after paging is enabled.
// The console functions will then use this new virtual address.
void console_set_vga_buffer_address(volatile unsigned short* new_address) {
    vga_buffer = new_address;
}
