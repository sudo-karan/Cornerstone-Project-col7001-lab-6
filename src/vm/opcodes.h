// [INTEGRATION] COPIED FROM: Cornerstone-Project-col7001-lab-4-and-5
#ifndef OPCODES_H
#define OPCODES_H

// Data Movement
#define PUSH 0x01
#define POP  0x02
#define DUP  0x03
#define HALT 0xFF

// Arithmetic
#define ADD  0x10
#define SUB  0x11
#define MUL  0x12
#define DIV  0x13
#define CMP  0x14

// Control Flow
#define JMP  0x20
#define JZ   0x21
#define JNZ  0x22

// Memory & Functions
#define STORE 0x30
#define LOAD  0x31
#define CALL  0x40
#define RET   0x41

// Standard Library
#define PRINT 0x50
#define INPUT 0x51
#define ALLOC 0x60

#endif
