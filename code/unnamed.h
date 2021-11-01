#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

// @TODO: use std::vector for now
#include <vector>

template <typename A>
using Array = std::vector<A>;

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;

typedef int64_t i64;
typedef int32_t i32;
typedef int16_t i16;
typedef int8_t  i8;

bool string_match(const char *x, const char *y) {
    return (strcmp(x, y) == 0);
}

#include "lexer.h"
#include "parser.h"
// #include "bytecode.h"
