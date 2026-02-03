#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "opcodes.h"
#include "jit.h"
#include <time.h>

#define STACK_SIZE 256
#define MEM_SIZE 1024
#define HEAP_SIZE 65536

/* DEBUG METADATA */
typedef struct {
    int address;
    int line_num;
} DebugEntry;

DebugEntry *debug_table = NULL;
int debug_table_size = 0;

void load_debug_info(const char *bin_filename) {
    char dbg_filename[256];
    strncpy(dbg_filename, bin_filename, sizeof(dbg_filename));
    char *dot = strrchr(dbg_filename, '.');
    if (dot) strcpy(dot, ".dbg");
    else strcat(dbg_filename, ".dbg");

    FILE *f = fopen(dbg_filename, "r");
    if (!f) return; // No debug info

    // First pass: count lines
    int count = 0;
    char line[128];
    while (fgets(line, sizeof(line), f)) count++;
    rewind(f);

    debug_table = malloc(count * sizeof(DebugEntry));
    debug_table_size = count;
    
    int i = 0;
    while (fgets(line, sizeof(line), f)) {
        int addr, ln;
        if (sscanf(line, "%d %d", &addr, &ln) == 2) {
            debug_table[i].address = addr;
            debug_table[i].line_num = ln;
            i++;
        }
    }
    fclose(f);
    printf("[VM] Loaded debug info from %s (%d entries)\n", dbg_filename, i);
}

int get_line_number(int pc) {
    // Find entry with max address <= pc
    int best_line = -1;
    for (int i = 0; i < debug_table_size; i++) {
        if (debug_table[i].address <= pc) {
             // Because table is sorted by address (assembler writes sequentially), 
             // we can just keep updating.
             // Actually, assembler writes in order. 
             // Ideally we want the EXACT address match or the associated block.
             // But assembler.py maps "Instruction Start Address" -> "Line".
             // If PC is inside an instruction (args), it works.
             // Reset best_line if we find a closer one? 
             // Since it's sorted, the 'last' one <= pc is the correct one.
             best_line = debug_table[i].line_num;
        } else {
            break;
        }
    }
    return best_line;
}

typedef struct {
    int32_t size;      // Payload size in words
    int32_t next;      // Pointer to next allocated object (for GC sweeping)
    uint8_t marked;    // Garbage Collection accessibility flag (0 = Unmarked, 1 = Marked)
} ObjectHeader;

typedef struct {
    int32_t stack[STACK_SIZE];
    int sp;                // Data Stack Pointer
    int32_t memory[MEM_SIZE];
    int32_t heap[HEAP_SIZE];
    int32_t free_ptr;      // Heap allocation pointer (Bump Pointer)
    int32_t allocated_list; // Linked list head of allocated objects
    uint32_t return_stack[STACK_SIZE];
    int rsp;               // Return Stack Pointer
    uint8_t *code;         // Bytecode array
    int pc;                // Program Counter
    int running;
    int error;             // Error flag
    // GC Statistics
    int stats_gc_runs;
    int stats_freed_objects;
    double stats_total_gc_time;
    int stats_max_heap_used;

    // DEBUGGER FIELDS
    int debug_mode;
    int step_mode;
    uint8_t breakpoints[4096]; // Simple breakpoint map
} VM;

/* GLOBAL VM POINTER FOR SIGNALS */
VM *global_vm = NULL;

void mark(VM *vm, int32_t addr);
void vm_gc(VM *vm);

void handle_sigusr1(int sig) {
    (void)sig;
    if (global_vm) {
        printf("\n[VM Memory Stats]\n");
        printf("  Heap Used: %d / %d words\n", global_vm->free_ptr, HEAP_SIZE);
        printf("  GC Runs: %d\n", global_vm->stats_gc_runs);
        printf("  Freed Objects: %d\n", global_vm->stats_freed_objects);
        // Calculate fragmentation or simple usage
        int allocated_count = 0;
        int curr = global_vm->allocated_list;
        while(curr != -1) {
            allocated_count++;
            curr = global_vm->heap[curr + 1];
        }
        printf("  Live Objects: %d\n", allocated_count);
        fsync(STDOUT_FILENO); // Ensure shell sees it
    }
}

// Forward decl
void check_leaks(VM *vm);

void handle_sigusr2(int sig) {
    (void)sig;
    if (global_vm) {
        // Trigger leak check asynchronously
        check_leaks(global_vm);
        fsync(STDOUT_FILENO);
    }
}


void handle_sigurg(int sig) {
    (void)sig;
    if (global_vm) {
        printf("\n[VM] Forcing Garbage Collection...\n");
        vm_gc(global_vm);
        printf("[VM] GC Complete. Heap: %d / %d words\n", global_vm->free_ptr, HEAP_SIZE);
        fsync(STDOUT_FILENO);
    }
}

// Reuse mark logic for LEAKS command
// (Prototype definition to match valid C)
void mark(VM *vm, int32_t addr);

void check_leaks(VM *vm) {
    // 1. Clear all marks
    int curr = vm->allocated_list;
    while (curr != -1) {
        vm->heap[curr+2] = 0;
        curr = vm->heap[curr+1];
    }

    // 2. Mark Roots
    for (int i = 0; i <= vm->sp; i++) {
        int32_t val = vm->stack[i];
         if (val >= MEM_SIZE && val < MEM_SIZE + HEAP_SIZE) {
            int32_t payload_idx = val - MEM_SIZE;
            int32_t header_idx = payload_idx - 3;
            if (header_idx >= 0) mark(vm, header_idx);
        }
    }
    // Check Memory Roots too (Global Vars)
    for (int i=0; i<MEM_SIZE; i++) {
        int32_t val = vm->memory[i];
         if (val >= MEM_SIZE && val < MEM_SIZE + HEAP_SIZE) {
            int32_t payload_idx = val - MEM_SIZE;
            int32_t header_idx = payload_idx - 3;
            if (header_idx >= 0) mark(vm, header_idx);
        }
    }

    // 3. Scan for UNMARKED objects
    printf("[Leaks Report]\n");
    int leaks_found = 0;
    int total_bytes = 0;
    curr = vm->allocated_list;
    while (curr != -1) {
        if (vm->heap[curr+2] == 0) {
            int size = vm->heap[curr];
            printf("  Leak: Object at Heap[%d] (Size: %d words)\n", curr, size);
            leaks_found++;
            total_bytes += size;
        }
        curr = vm->heap[curr+1];
    }
    
    if (leaks_found == 0) {
        printf("  No leaks detected.\n");
    } else {
        printf("  Summary: %d leaked objects, %d total words.\n", leaks_found, total_bytes);
    }
}

void run_debug_shell(VM *vm) {
    char line[128];
    // Show current line info
    if (debug_table) {
        int source_line = get_line_number(vm->pc);
        if (source_line != -1) {
            printf("[Source Line %d] ", source_line);
        }
    }

    while (1) {
        printf("vm-dbg> ");
        if (fgets(line, sizeof(line), stdin) == NULL) break;
        line[strcspn(line, "\n")] = 0;
        
        if (strcmp(line, "step") == 0 || strcmp(line, "s") == 0) {
            vm->step_mode = 1;
            return; // Execute one instruction
        }
        else if (strcmp(line, "continue") == 0 || strcmp(line, "c") == 0) {
            vm->step_mode = 0;
            return; // Run until next breakpoint
        }
        else if (strcmp(line, "registers") == 0 || strcmp(line, "r") == 0) {
            printf("PC: %d, SP: %d, RSP: %d\n", vm->pc, vm->sp, vm->rsp);
            if (vm->sp >= 0) printf("Top of Stack: %d\n", vm->stack[vm->sp]);
        }
        else if (strcmp(line, "leaks") == 0) {
            check_leaks(vm);
        }
        else if (strcmp(line, "quit") == 0) {
            vm->running = 0;
            return;
        }
        else if (strcmp(line, "memstat") == 0) {
             printf("Heap Ptr: %d\n", vm->free_ptr);
        }
        else if (strncmp(line, "break ", 6) == 0) {
            int addr = atoi(line + 6);
            if (addr >= 0 && addr < 4096) {
                vm->breakpoints[addr] = 1;
                printf("Breakpoint set at %d\n", addr);
            }
        }
        else {
            printf("Commands: step, continue, registers, memstat, leaks, break <addr>, quit\n");
        }
    }
}

// Helper to handle runtime errors safely
void error(VM *vm, const char *msg) {
    fprintf(stderr, "Runtime Error: %s\n", msg);
    vm->running = 0;
    vm->error = 1;
}



void mark(VM *vm, int32_t addr) {
    if (addr < 0 || addr >= HEAP_SIZE) return; // Invalid address
    
    // Address Validation: ensure we are within heap bounds.
    int32_t obj_idx = addr; 
    
    // Check mark bit in object header (offset +2 from base address).
    if (vm->heap[obj_idx + 2]) return; 
    
    vm->heap[obj_idx + 2] = 1; // Set mark bit.

    // Recursive Marking (Transitive Reachability)
    int32_t size = vm->heap[obj_idx]; // Header[0] is size
    int32_t payload_idx = obj_idx + 3; // Skip 3-word header
    
    for (int i = 0; i < size; i++) {
        int32_t val = vm->heap[payload_idx + i];
        // Check if value is a pointer into the heap
        if (val >= MEM_SIZE && val < MEM_SIZE + HEAP_SIZE) {
            int32_t child_payload_idx = val - MEM_SIZE;
            int32_t child_header_idx = child_payload_idx - 3;
            if (child_header_idx >= 0) {
                mark(vm, child_header_idx);
            }
        }
    }
}

void sweep(VM *vm) {
    int32_t *curr_ptr = &vm->allocated_list; // Pointer to the 'next' field of previous node (or head)
    int32_t curr = vm->allocated_list;

    while (curr != -1) {
        // curr is index of Header[0] (Size)
        // Header[2] is Mark
        int marked = vm->heap[curr + 2];
        int next = vm->heap[curr + 1];

        if (marked) {
            vm->heap[curr + 2] = 0; // Unmark for next cycle
            curr_ptr = &vm->heap[curr + 1]; // Advance ptr-to-next to this node's next field
            curr = next;
        } else {
            // Unlink
            *curr_ptr = next; // Previous node now points to next
            // Essentially "freeing" logic (logical collection)
            vm->stats_freed_objects++;
            curr = next;
        }
    }
    
    // Optimization: If heap is completely empty, reset bump pointer
    if (vm->allocated_list == -1) {
        vm->free_ptr = 0;
    }
}

void vm_gc(VM *vm) {
    clock_t start = clock();
    vm->stats_gc_runs++;
    // 1. Mark Phase: Scan Stack
    for (int i = 0; i <= vm->sp; i++) {
        int32_t val = vm->stack[i];
        if (val >= MEM_SIZE && val < MEM_SIZE + HEAP_SIZE) {
            // Potential Heap Pointer
            int32_t payload_idx = val - MEM_SIZE;
            int32_t header_idx = payload_idx - 3;
            if (header_idx >= 0) { // Basic sanity check
                mark(vm, header_idx);
            }
        }
    }
    
    // Scan Memory too (Globals)
     for (int i=0; i<MEM_SIZE; i++) {
        int32_t val = vm->memory[i];
         if (val >= MEM_SIZE && val < MEM_SIZE + HEAP_SIZE) {
            int32_t payload_idx = val - MEM_SIZE;
            int32_t header_idx = payload_idx - 3;
            if (header_idx >= 0) mark(vm, header_idx);
        }
    }

    // 2. Sweep Phase
    sweep(vm);
    
    clock_t end = clock();
    vm->stats_total_gc_time += (double)(end - start) / CLOCKS_PER_SEC;
}

void push(VM *vm, int32_t val) {
    if (vm->sp >= STACK_SIZE - 1) {
        error(vm, "Stack Overflow");
        return;
    }
    vm->stack[++vm->sp] = val;
}

int32_t pop(VM *vm) {
    if (vm->sp < 0) {
        error(vm, "Stack Underflow");
        return 0; // Return dummy value, VM will stop anyway
    }
    return vm->stack[vm->sp--];
}

void run_vm(VM *vm) {
    vm->pc = 0;
    vm->sp = -1;
    vm->rsp = -1;
    vm->running = 1;
    vm->pc = 0;
    vm->sp = -1;
    vm->rsp = -1;
    vm->running = 1;
    vm->pc = 0;
    vm->sp = -1;
    vm->rsp = -1;
    vm->running = 1;
    vm->error = 0;
    vm->free_ptr = 0; // Initialize heap pointer to start
    vm->allocated_list = -1; // -1 denotes end of linked list
    vm->stats_gc_runs = 0;
    vm->stats_freed_objects = 0;
    vm->stats_total_gc_time = 0.0;
    vm->stats_max_heap_used = 0;
    
    global_vm = vm;
    signal(SIGUSR1, handle_sigusr1);
    signal(SIGUSR2, handle_sigusr2);
    signal(SIGURG, handle_sigurg);

    // We assume the code size is large enough or trusted, assuming proper loader checks.
    // In a real VM, you'd also check bounds of vm->pc against code size.

    while (vm->running) {
        // DEBUG CHECK
        if (vm->debug_mode) {
             if (vm->step_mode || vm->breakpoints[vm->pc]) {
                 printf("[DEBUG] PC: %d, Opcode: 0x%02X\n", vm->pc, vm->code[vm->pc]);
                 run_debug_shell(vm);
                 if (!vm->running) break;
             }
        }

        uint8_t opcode = vm->code[vm->pc++];
        switch (opcode) {
        // 1.6.1 Data Movement
        case PUSH: {
            int32_t val = *(int32_t*)&vm->code[vm->pc];
            push(vm, val);
            vm->pc += 4;
            break;
        }

        case POP: {
            pop(vm);
            break;
        }
        case DUP: {
            if (vm->sp < 0) { error(vm, "Stack Underflow"); break; }
            push(vm, vm->stack[vm->sp]);
            break;
        }
        case HALT: {
            vm->running = 0;
            break;
        }

        // 1.6.2 Arithmetic & Logical
        case ADD: {
            int32_t b = pop(vm);
            int32_t a = pop(vm);
            if (vm->running) push(vm, a + b);
            break;
        }
        case SUB: {
            int32_t b = pop(vm);
            int32_t a = pop(vm);
            if (vm->running) push(vm, a - b);
            break;
        }
        case MUL: {
            int32_t b = pop(vm);
            int32_t a = pop(vm);
            if (vm->running) push(vm, a * b);
            break;
        }
        case DIV: {
            int32_t b = pop(vm);
            int32_t a = pop(vm);
            if (!vm->running) break; 
            if (b != 0) push(vm, a / b);
            else error(vm, "Division by Zero");
            break;
        }
        case CMP: {
            int32_t b = pop(vm);
            int32_t a = pop(vm);
            if (vm->running) push(vm, (a < b) ? 1 : 0);
            break;
        }

        // 1.6.3 Control Flow
        case JMP: {
            vm->pc = *(int32_t*)&vm->code[vm->pc];
            break;
        }
        case JZ: {
            int32_t addr = *(int32_t*)&vm->code[vm->pc];
            vm->pc += 4;
            int32_t val = pop(vm);
            if (vm->running && val == 0) vm->pc = addr;
            break;
        }
        case JNZ: {
            int32_t addr = *(int32_t*)&vm->code[vm->pc];
            vm->pc += 4;
            int32_t val = pop(vm);
            if (vm->running && val != 0) vm->pc = addr;
            break;
        }

        // 1.6.4 Memory & Functions
        case STORE: {
            int32_t idx = *(int32_t*)&vm->code[vm->pc];
            vm->pc += 4;
            int32_t val = pop(vm);
            if (!vm->running) break;
            
            if (idx < 0) {
                error(vm, "Memory Access Out of Bounds");
            } else if (idx < MEM_SIZE) {
                vm->memory[idx] = val;
            } else {
                int heap_idx = idx - MEM_SIZE;
                if (heap_idx >= HEAP_SIZE) {
                    error(vm, "Heap Access Out of Bounds");
                } else {
                    vm->heap[heap_idx] = val;
                }
            }
            break;
        }
        case LOAD: {
            int32_t idx = *(int32_t*)&vm->code[vm->pc];
            vm->pc += 4;
            
            if (idx < 0) {
                error(vm, "Memory Access Out of Bounds");
            } else if (idx < MEM_SIZE) {
                push(vm, vm->memory[idx]);
            } else {
                int heap_idx = idx - MEM_SIZE;
                if (heap_idx >= HEAP_SIZE) {
                    error(vm, "Heap Access Out of Bounds");
                } else {
                    push(vm, vm->heap[heap_idx]);
                }
            }
            break;
        }
        case CALL: {
            uint32_t addr = *(uint32_t*)&vm->code[vm->pc];
            vm->pc += 4;
            
            if (vm->rsp >= STACK_SIZE - 1) {
                error(vm, "Return Stack Overflow");
                break;
            }
            vm->return_stack[++vm->rsp] = vm->pc; 
            vm->pc = addr;
            break;
        }
        case RET: {
            if (vm->rsp < 0) {
                error(vm, "Return Stack Underflow");
                break;
            }
            vm->pc = vm->return_stack[vm->rsp--];
            break;
        }

        // 1.6.5 Standard Library
        case PRINT: {
            if (vm->sp < 0) {
                error(vm, "Stack Underflow");
                break;
            }
            printf("%d\n", vm->stack[vm->sp--]);
            fflush(stdout);
            break;
        }
        case INPUT: {
            int val;
            printf("Enter number: ");
            if (scanf("%d", &val) == 1) {
                if (vm->sp >= STACK_SIZE - 1) {
                    error(vm, "Stack Overflow");
                    break;
                }
                vm->stack[++vm->sp] = val;
            } else {
                fprintf(stderr, "Error: Invalid input\n");
                vm->running = 0;
                vm->error = 1;
            }
            break;
        }

        case ALLOC: {
            int32_t size = pop(vm);
            if (size < 0) { error(vm, "Invalid Allocation Size"); break; }
            
            // Header: 3 words [Size, Next, Marked]
            int needed = size + 3; 
            if (vm->free_ptr + needed > HEAP_SIZE) {
                vm_gc(vm); // Trigger Garbage Collection
                if (vm->free_ptr + needed > HEAP_SIZE) { // Retry Allocation
                    error(vm, "Heap Overflow");
                    break;
                }
            }

            int32_t addr = vm->free_ptr;
            vm->heap[addr] = size;                     // Header[0]: Size
            vm->heap[addr + 1] = vm->allocated_list;   // Header[1]: Next Object
            vm->heap[addr + 2] = 0;                    // Header[2]: Mark Bit
            
            vm->allocated_list = addr;                 // Update List Head
            vm->free_ptr += needed;                    // Advance Pointer
            
            if (vm->free_ptr > vm->stats_max_heap_used) {
                vm->stats_max_heap_used = vm->free_ptr;
            }
            
            // Push address of payload (skip header) to stack
            push(vm, MEM_SIZE + addr + 3);
            break;
        }

        default:
            fprintf(stderr, "Unknown Opcode: 0x%02X\n", opcode);
            vm->running = 0;
            vm->error = 1;
        }
    }

    if (vm->debug_mode && !vm->error) {
         printf("[DEBUG] Execution Finished.\n");
         run_debug_shell(vm);
    }
}

#ifndef TESTING
int main(int argc, char **argv) {
#else
int run_vm_main(int argc, char **argv) {
#endif
    if (argc < 2) return 1;
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        fprintf(stderr, "Error opening file %s\n", argv[1]);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    uint8_t *code = malloc(size);
    if (!code) {
        fclose(f);
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }
    
    fread(code, 1, size, f);
    fclose(f);

    VM vm = { .code = code };

    // Check for JIT flag or Debug flag
    int use_jit = 0;
    // Simple arg parsing logic loop
    for(int i=2; i<argc; i++) {
        if (strcmp(argv[i], "--jit") == 0) use_jit = 1;
        if (strcmp(argv[i], "--debug") == 0) vm.debug_mode = 1;
    }

    if (use_jit) {
        printf("Running with JIT...\n");
        jit_func jitted_code = compile(code, size);
        if (jitted_code) {
            // JIT returns the top of the stack as an integer
            int result = jitted_code();
            printf("JIT Result: %d\n", result);
        } else {
            fprintf(stderr, "JIT Compilation Failed\n");
            return 1;
        }
    } else {
        if (vm.debug_mode) {
            printf("VM running in DEBUG mode. Type 'help' for commands.\n");
            load_debug_info(argv[1]);
            vm.step_mode = 1; // Start paused
        }
        run_vm(&vm);
        
        if (!vm.error && vm.sp >= 0)
            printf("Top of stack: %d\n", vm.stack[vm.sp]);
        else if (!vm.error)
            printf("Stack empty\n");

        if (vm.stats_gc_runs > 0) {
            printf("[GC Stats] Runs: %d, Freed: %d, Total GC Time: %.6fs, Max Heap: %d words\n", 
                vm.stats_gc_runs, vm.stats_freed_objects, vm.stats_total_gc_time, vm.stats_max_heap_used);
        }
    }

    free(code);
    if (debug_table) free(debug_table);
    return vm.error ? 1 : 0;
}
