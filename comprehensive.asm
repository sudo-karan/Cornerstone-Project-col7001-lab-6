.line 1
.line 1
PUSH 0
STORE 0
.line 2
.line 2
PUSH 0
STORE 1
.line 4
.line 4
PUSH 8888
PRINT
.line 11
L0:
.line 6
.line 6
LOAD 0
.line 6
PUSH 10000
CMP
JZ L1
.line 11
.line 8
.line 8
PUSH 0
STORE 1
.line 10
.line 10
.line 10
LOAD 0
.line 10
PUSH 1
ADD
STORE 0
JMP L0
L1:
.line 16
L2:
.line 14
PUSH 1
JZ L3
.line 16
.line 15
.line 15
.line 15
LOAD 0
.line 15
PUSH 1
ADD
STORE 0
JMP L2
L3:
HALT
