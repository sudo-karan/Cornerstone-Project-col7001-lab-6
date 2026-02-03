; Memory Test
PUSH 10
ALLOC
STORE 0 ; Store a pointer in memory[0] (Root)

PUSH 20
ALLOC
POP ; Discard the pointer (Leak)

PUSH 30
ALLOC
STORE 1 ; Store another pointer

HALT
