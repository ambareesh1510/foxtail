#ifndef UTIL_H
#define UTIL_H

typedef unsigned long size_t;
typedef unsigned long long u64;
typedef unsigned int  u32;
typedef unsigned short u16;
typedef unsigned char u8;

typedef long long i64;
typedef int  i32;
typedef short i16;
typedef char i8;

u32 min(u32 a, u32 b);
u32 max(u32 a, u32 b);

__attribute__ ((noreturn)) void panic(char *msg, ...);

void flush_tlb();

void useless();

#endif /* UTIL_H */
