/* bbs/types.h - common integer typedefs and small helpers. */
#ifndef BBS_TYPES_H
#define BBS_TYPES_H

#include <stdint.h>
#include <stddef.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;

/* Boolean — avoid stdbool.h for tighter Oscar64 codegen. */
typedef u8 bool_t;
#define TRUE  ((bool_t)1)
#define FALSE ((bool_t)0)

#endif /* BBS_TYPES_H */
