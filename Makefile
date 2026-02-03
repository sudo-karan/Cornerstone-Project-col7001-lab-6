# Unified Makefile for Cornerstone Lab 6

CC = clang
CFLAGS = -Wall -Wextra -g -Isrc/compiler -Isrc/vm

# Directories
SRC_SHELL = src/shell
SRC_COMPILER = src/compiler
SRC_VM = src/vm
BIN = bin

# Targets
TARGET_SHELL = $(BIN)/myshell
TARGET_COMPILER = $(BIN)/compiler
TARGET_VM = $(BIN)/vm

all: dirs $(TARGET_SHELL) $(TARGET_COMPILER) $(TARGET_VM)

dirs:
	mkdir -p $(BIN)

# --- Shell ---
$(TARGET_SHELL): $(SRC_SHELL)/shell.c
	$(CC) $(CFLAGS) -o $@ $<

# --- Compiler ---
# Lexer & Parser Generation
$(SRC_COMPILER)/lex.yy.c: $(SRC_COMPILER)/lexer.l $(SRC_COMPILER)/parser.tab.h
	flex -o $@ $<

$(SRC_COMPILER)/parser.tab.c $(SRC_COMPILER)/parser.tab.h: $(SRC_COMPILER)/parser.y
	bison -d -o $(SRC_COMPILER)/parser.tab.c $< --verbose

# Compile Compiler
COMPILER_SRCS = $(SRC_COMPILER)/codegen.c $(SRC_COMPILER)/ast.c $(SRC_COMPILER)/parser.tab.c $(SRC_COMPILER)/lex.yy.c
$(TARGET_COMPILER): $(COMPILER_SRCS)
	$(CC) $(CFLAGS) -Wno-sign-compare -o $@ $(COMPILER_SRCS)

# --- VM ---
VM_SRCS = $(SRC_VM)/vm.c $(SRC_VM)/jit.c
$(TARGET_VM): $(VM_SRCS)
	$(CC) $(CFLAGS) -o $@ $^

# Clean
clean:
	rm -rf $(BIN)
	rm -f $(SRC_COMPILER)/lex.yy.c $(SRC_COMPILER)/parser.tab.c $(SRC_COMPILER)/parser.tab.h $(SRC_COMPILER)/parser.output
