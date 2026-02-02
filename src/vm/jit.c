// [INTEGRATION] COPIED FROM: Cornerstone-Project-col7001-lab-4-and-5
#include "jit.h"
#include "opcodes.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>

#define MAX_CODE_SIZE 4096

// Helper to append byte to buffer
void emit_byte(uint8_t **ptr, uint8_t byte) {
    *(*ptr)++ = byte;
}

// Helper to append 32-bit int to buffer
void emit_int32(uint8_t **ptr, int32_t val) {
    *(int32_t*)(*ptr) = val;
    *ptr += 4;
}

jit_func compile(uint8_t *code, int length) {
    // 1. Allocate executable memory
    void *mem = mmap(NULL, MAX_CODE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }

    uint8_t *ptr = (uint8_t *)mem;
    int pc = 0;

    // 2. Prologue: Setup stack frame
    // push rbp
    emit_byte(&ptr, 0x55);
    // mov rbp, rsp
    emit_byte(&ptr, 0x48);
    emit_byte(&ptr, 0x89);
    emit_byte(&ptr, 0xE5);
    // push rbx (callee-saved)
    emit_byte(&ptr, 0x53);

    // Map from bytecode offset to machine code offset
    int mapping[MAX_CODE_SIZE];
    for (int i = 0; i < MAX_CODE_SIZE; i++) mapping[i] = -1;

    while (pc < length) {
        int current_pc = pc;
        mapping[pc] = (int)((uint8_t*)ptr - (uint8_t*)mem);
        
        uint8_t opcode = code[pc++];

        switch (opcode) {
            case PUSH: {
                int32_t val = *(int32_t *)&code[pc];
                pc += 4;
                emit_byte(&ptr, 0x68);
                emit_int32(&ptr, val);
                break;
            }
            case POP: {
                emit_byte(&ptr, 0x58);
                break;
            }
            case DUP: { // Added DUP for loop benchmark
                // pop rax
                emit_byte(&ptr, 0x58);
                // push rax
                emit_byte(&ptr, 0x50);
                // push rax
                emit_byte(&ptr, 0x50);
                break;
            }
            case ADD: {
                emit_byte(&ptr, 0x5B);
                emit_byte(&ptr, 0x58);
                emit_byte(&ptr, 0x48); emit_byte(&ptr, 0x01); emit_byte(&ptr, 0xD8);
                emit_byte(&ptr, 0x50);
                break;
            }
            case SUB: {
                emit_byte(&ptr, 0x5B);
                emit_byte(&ptr, 0x58);
                emit_byte(&ptr, 0x48); emit_byte(&ptr, 0x29); emit_byte(&ptr, 0xD8);
                emit_byte(&ptr, 0x50);
                break;
            }
            case MUL: {
                emit_byte(&ptr, 0x5B);
                emit_byte(&ptr, 0x58);
                emit_byte(&ptr, 0x48); emit_byte(&ptr, 0x0F); emit_byte(&ptr, 0xAF); emit_byte(&ptr, 0xC3);
                emit_byte(&ptr, 0x50);
                break;
            }
            case CMP: {
               // pop rbx (second)
                emit_byte(&ptr, 0x5B);
                // pop rax (first)
                emit_byte(&ptr, 0x58);
                
                // cmp rax, rbx
                emit_byte(&ptr, 0x48);
                emit_byte(&ptr, 0x39);
                emit_byte(&ptr, 0xD8);
                
                // setl al (set if less) - VM CMP is: (a < b) ? 1 : 0
                emit_byte(&ptr, 0x0F); 
                emit_byte(&ptr, 0x9C); 
                emit_byte(&ptr, 0xC0); // setl al
                
                // movzx rax, al (zero extend to 64-bit)
                emit_byte(&ptr, 0x48);
                emit_byte(&ptr, 0x0F);
                emit_byte(&ptr, 0xB6);
                emit_byte(&ptr, 0xC0);
                
                // push rax
                emit_byte(&ptr, 0x50);
                break;
            }
            // Control Flow
            case JMP: {
                int32_t target = *(int32_t *)&code[pc];
                pc += 4;
                // For simplified JIT, assuming backward jump to existing code (e.g. for loop)
                if (target < current_pc && mapping[target] != -1) {
                    // jmp rel32 (E9 rel32)
                    int current_offset = (int)((uint8_t*)ptr - (uint8_t*)mem);
                    int target_offset = mapping[target];
                    int rel32 = target_offset - (current_offset + 5);
                    emit_byte(&ptr, 0xE9);
                    emit_int32(&ptr, rel32);
                } else {
                    fprintf(stderr, "JIT Error: Forward/Unknown JMP not implemented yet\n");
                    return NULL;
                }
                break;
            }
            case JZ: {
                int32_t target = *(int32_t *)&code[pc];
                pc += 4;
                // pop rax
                emit_byte(&ptr, 0x58);
                // test rax, rax (48 85 C0)
                emit_byte(&ptr, 0x48); emit_byte(&ptr, 0x85); emit_byte(&ptr, 0xC0);
                
                if (target < current_pc && mapping[target] != -1) {
                    // je rel32 (0F 84 rel32)
                    int current_offset = (int)((uint8_t*)ptr - (uint8_t*)mem);
                    int target_offset = mapping[target];
                    int rel32 = target_offset - (current_offset + 6); // 2 bytes opcode + 4 bytes rel
                    emit_byte(&ptr, 0x0F);
                    emit_byte(&ptr, 0x84);
                    emit_int32(&ptr, rel32);
                } else {
                    fprintf(stderr, "JIT Error: Forward/Unknown JZ not implemented\n");
                    return NULL;
                }
                break;
            }
            case JNZ: {
                int32_t target = *(int32_t *)&code[pc];
                pc += 4;
                // pop rax
                emit_byte(&ptr, 0x58);
                // test rax, rax
                emit_byte(&ptr, 0x48); emit_byte(&ptr, 0x85); emit_byte(&ptr, 0xC0);
                
                if (target < current_pc && mapping[target] != -1) {
                    // jne rel32 (0F 85 rel32)
                    int current_offset = (int)((uint8_t*)ptr - (uint8_t*)mem);
                    int target_offset = mapping[target];
                    int rel32 = target_offset - (current_offset + 6);
                    emit_byte(&ptr, 0x0F);
                    emit_byte(&ptr, 0x85);
                    emit_int32(&ptr, rel32);
                } else {
                    fprintf(stderr, "JIT Error: Forward/Unknown JNZ not implemented\n");
                    return NULL;
                }
                break;
            }

            case HALT: {
                emit_byte(&ptr, 0x58); // pop rax (return value)
                emit_byte(&ptr, 0x5B); // pop rbx (restore)
                emit_byte(&ptr, 0xC9); // leave
                emit_byte(&ptr, 0xC3); // ret
                return (jit_func)mem;
            }
            default:
                fprintf(stderr, "JIT Error: Unsupported opcode 0x%02X\n", opcode);
                return NULL;
        }
    }
    
    // Fallback Epilogue
    emit_byte(&ptr, 0x58); // pop rax
    emit_byte(&ptr, 0x5B); // pop rbx
    emit_byte(&ptr, 0xC9);
    emit_byte(&ptr, 0xC3);

    return (jit_func)mem;
}
