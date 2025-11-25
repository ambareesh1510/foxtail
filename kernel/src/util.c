#include "util.h"

void char_to_hex_string(char c, char *buf) {
    const char hex_digits[] = "0123456789ABCDEF";
    buf[0] = hex_digits[(c >> 4) & 0xF];
    buf[1] = hex_digits[c & 0xF];
    buf[2] = '\0';
}

void uint32_to_string(uint32_t value, char* buf) {
    // Handle the special case of 0.
    if (value == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }

    int i = 0;
    // Find the digits in reverse order.
    while (value > 0) {
        buf[i] = '0' + (value % 10);  // Get the last digit
        value /= 10;                   // Remove the last digit
        i++;
    }

    // Null-terminate the string
    buf[i] = '\0';

    // Reverse the string to get the correct order
    int start = 0;
    int end = i - 1;
    while (start < end) {
        // Swap the characters
        char temp = buf[start];
        buf[start] = buf[end];
        buf[end] = temp;
        start++;
        end--;
    }
}
