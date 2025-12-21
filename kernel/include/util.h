#ifndef UTIL_H
#define UTIL_H

typedef unsigned long size_t;
typedef unsigned int  uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;

uint32_t min(uint32_t a, uint32_t b);
uint32_t max(uint32_t a, uint32_t b);

__attribute__ ((noreturn)) void panic(char *msg);

void useless();

#endif /* UTIL_H */
