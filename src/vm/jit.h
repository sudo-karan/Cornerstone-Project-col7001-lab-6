// [INTEGRATION] COPIED FROM: Cornerstone-Project-col7001-lab-4-and-5
#ifndef JIT_H
#define JIT_H

#include <stdint.h>
#include <stddef.h>

// Function pointer type for the JIT-compiled code
typedef int (*jit_func)();

// Compile bytecode into machine code
// Returns a pointer to the executable memory
jit_func compile(uint8_t *code, int length);

#endif
