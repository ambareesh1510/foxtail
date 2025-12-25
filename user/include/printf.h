#ifndef PRINTF_H
#define PRINTF_H
#include <stdarg.h>
#include "malloc.h"
#include "string.h"

// TODO: This is a temporary implementation by Claude; review it later

// Helper: Convert integer to string (decimal)
static int itoa(int num, char *buf) {
    int i = 0;
    int is_negative = 0;
    
    if (num == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return 1;
    }
    
    if (num < 0) {
        is_negative = 1;
        num = -num;
    }
    
    // Convert digits in reverse
    char temp[12];  // Max 10 digits + sign + null
    int j = 0;
    while (num > 0) {
        temp[j++] = '0' + (num % 10);
        num /= 10;
    }
    
    // Add negative sign
    if (is_negative) {
        temp[j++] = '-';
    }
    
    // Reverse into output buffer
    for (int k = 0; k < j; k++) {
        buf[k] = temp[j - k - 1];
    }
    buf[j] = '\0';
    
    return j;
}

// Helper: Convert unsigned integer to hex string
static int utoa_hex(unsigned int num, char *buf) {
    if (num == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return 1;
    }
    
    const char *hex_digits = "0123456789abcdef";
    char temp[9];  // Max 8 hex digits + null
    int i = 0;
    
    while (num > 0) {
        temp[i++] = hex_digits[num & 0xF];
        num >>= 4;
    }
    
    // Reverse into output buffer
    for (int j = 0; j < i; j++) {
        buf[j] = temp[i - j - 1];
    }
    buf[i] = '\0';
    
    return i;
}

int printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    // Allocate buffer for output (dynamically sized would be better)
    char *buffer = malloc(1024);
    if (!buffer) {
        return -1;
    }
    
    int buf_pos = 0;
    int i = 0;
    
    while (format[i]) {
        if (format[i] == '%' && format[i + 1]) {
            i++;  // Skip '%'
            
            switch (format[i]) {
                case 'd': {
                    // Signed decimal integer
                    int num = va_arg(args, int);
                    char temp[12];
                    int len = itoa(num, temp);
                    for (int j = 0; j < len; j++) {
                        buffer[buf_pos++] = temp[j];
                    }
                    break;
                }
                
                case 'x': {
                    // Hexadecimal
                    unsigned int num = va_arg(args, unsigned int);
                    char temp[9];
                    int len = utoa_hex(num, temp);
                    for (int j = 0; j < len; j++) {
                        buffer[buf_pos++] = temp[j];
                    }
                    break;
                }
                
                case 's': {
                    // String
                    const char *s = va_arg(args, const char *);
                    if (!s) {
                        s = "(null)";
                    }
                    while (*s) {
                        buffer[buf_pos++] = *s++;
                    }
                    break;
                }
                
                case 'c': {
                    // Character
                    char c = (char)va_arg(args, int);
                    buffer[buf_pos++] = c;
                    break;
                }
                
                case '%': {
                    // Literal '%'
                    buffer[buf_pos++] = '%';
                    break;
                }
                
                default:
                    // Unknown format specifier, just print it
                    buffer[buf_pos++] = '%';
                    buffer[buf_pos++] = format[i];
                    break;
            }
        } else {
            // Regular character
            buffer[buf_pos++] = format[i];
        }
        
        i++;
        
        // Prevent buffer overflow
        if (buf_pos >= 1023) {
            break;
        }
    }
    
    buffer[buf_pos] = '\0';
    
    // Write to stdout
    int result = puts(buffer);
    
    free(buffer);
    va_end(args);
    
    return result;
}

// Bonus: sprintf - format into a user-provided buffer
int sprintf(char *str, const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    int buf_pos = 0;
    int i = 0;
    
    while (format[i]) {
        if (format[i] == '%' && format[i + 1]) {
            i++;
            
            switch (format[i]) {
                case 'd': {
                    int num = va_arg(args, int);
                    char temp[12];
                    int len = itoa(num, temp);
                    for (int j = 0; j < len; j++) {
                        str[buf_pos++] = temp[j];
                    }
                    break;
                }
                
                case 'x': {
                    unsigned int num = va_arg(args, unsigned int);
                    char temp[9];
                    int len = utoa_hex(num, temp);
                    for (int j = 0; j < len; j++) {
                        str[buf_pos++] = temp[j];
                    }
                    break;
                }
                
                case 's': {
                    const char *s = va_arg(args, const char *);
                    if (!s) {
                        s = "(null)";
                    }
                    while (*s) {
                        str[buf_pos++] = *s++;
                    }
                    break;
                }
                
                case 'c': {
                    char c = (char)va_arg(args, int);
                    str[buf_pos++] = c;
                    break;
                }
                
                case '%': {
                    str[buf_pos++] = '%';
                    break;
                }
                
                default:
                    str[buf_pos++] = '%';
                    str[buf_pos++] = format[i];
                    break;
            }
        } else {
            str[buf_pos++] = format[i];
        }
        i++;
    }
    
    str[buf_pos] = '\0';
    va_end(args);
    
    return buf_pos;
}

#endif /* PRINTF_H */
