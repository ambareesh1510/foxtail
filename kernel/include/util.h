#ifndef UTIL_H
#define UTIL_H

typedef unsigned long size_t;
typedef unsigned int  uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;

// Placeholder implementations of string formatting functions
void char_to_hex_string(char c, char *buf);

void uint32_to_string(uint32_t value, char* buf);

#endif /* UTIL_H */
