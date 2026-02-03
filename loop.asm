.line 1
.line 1
PUSH 0
STORE 0
.line 4
L0:
.line 2
.line 2
LOAD 0
.line 2
PUSH 10000000
CMP
JZ L1
.line 4
.line 3
.line 3
.line 3
LOAD 0
.line 3
PUSH 1
ADD
STORE 0
JMP L0
L1:
HALT
