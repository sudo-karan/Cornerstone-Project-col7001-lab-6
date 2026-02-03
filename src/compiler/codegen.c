
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

extern int yyparse();
extern ASTNode *root;

// -- Symbol Table (Simple Linked List) --
typedef struct Symbol {
    char *name;
    int addr; // Memory Index
    struct Symbol *next;
} Symbol;

Symbol *sym_table = NULL;
int global_addr_counter = 0;

int get_symbol_addr(char *name) {
    Symbol *curr = sym_table;
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            return curr->addr;
        }
        curr = curr->next;
    }
    return -1; // Not found
}

int add_symbol(char *name) {
    if (get_symbol_addr(name) != -1) return get_symbol_addr(name); // Already exists
    
    Symbol *new_sym = malloc(sizeof(Symbol));
    new_sym->name = strdup(name);
    new_sym->addr = global_addr_counter++;
    new_sym->next = sym_table;
    sym_table = new_sym;
    return new_sym->addr;
}

// -- Label Geneartion --
int label_counter = 0;
int new_label() {
    return label_counter++;
}

// -- Code Generation --

void gen(ASTNode *node) {
    if (!node) return;

    // Emit debug metadata (Line Number)
    if (node->line > 0) {
        printf(".line %d\n", node->line);
    }

    switch (node->type) {
        case NODE_NUM:
            printf("PUSH %d\n", node->int_val);
            break;

        case NODE_VAR: {
            int addr = get_symbol_addr(node->id);
            if (addr == -1) {
                fprintf(stderr, "Error: Undefined variable '%s'\n", node->id);
                exit(1);
            }
            printf("LOAD %d\n", addr);
            break;
        }

        case NODE_VAR_DECL: {
            // "var x = 5;"
            int addr = add_symbol(node->id);
            if (node->left) {
                gen(node->left); // Generate code for initializer
                printf("STORE %d\n", addr);
            } else {
                // Initialize to 0 by default? Or just do nothing?
                // Let's push 0 and store to be safe
                printf("PUSH 0\n");
                printf("STORE %d\n", addr);
            }
            break;
        }

        case NODE_ASSIGN: {
            // "x = 10;"
            int addr = get_symbol_addr(node->id);
            if (addr == -1) {
                fprintf(stderr, "Error: Undefined variable '%s'\n", node->id);
                exit(1);
            }
            gen(node->left);
            printf("STORE %d\n", addr);
            break;
        }

        case NODE_BIN_OP: {
            gen(node->left);
            gen(node->right);
            
            if (strcmp(node->op, "+") == 0) printf("ADD\n");
            else if (strcmp(node->op, "-") == 0) printf("SUB\n");
            else if (strcmp(node->op, "*") == 0) printf("MUL\n");
            else if (strcmp(node->op, "/") == 0) printf("DIV\n");
            else if (strcmp(node->op, "==") == 0) { 
                 // Equality Synthesis
                 int l_eq = new_label();
                 int l_done = new_label();

                 // a == b  <==> (a - b) == 0
                 // Stack: [a, b]
                 printf("SUB\n"); 
                 // Stack: [diff]
                 
                 printf("DUP\n");      // Duplicate diff to check it
                 printf("JZ L%d\n", l_eq);
                 
                 // Case: Not Equal (diff != 0)
                 printf("POP\n");      // Pop the diff
                 printf("PUSH 0\n");   // Result False
                 printf("JMP L%d\n", l_done);
                 
                 // Case: Equal (diff == 0)
                 printf("L%d:\n", l_eq);
                 printf("POP\n");      // Pop the diff (which is 0)
                 printf("PUSH 1\n");   // Result True
                 
                 printf("L%d:\n", l_done);
            }
            else if (strcmp(node->op, "<") == 0) printf("CMP\n");
            else {
                fprintf(stderr, "Error: Unknown BinOp '%s'\n", node->op);
            }
            break;
        }

        case NODE_IF: {
            int lbl_else = new_label();
            int lbl_end = new_label();

            gen(node->left); // Condition
            // Assume 0 is False, Non-Zero is True.
            // CMP returns 1 (True) or 0 (False).
            // JZ jumps if 0 (False).
            
            printf("JZ L%d\n", lbl_else);
            
            gen(node->right); // Then block
            printf("JMP L%d\n", lbl_end);
            
            printf("L%d:\n", lbl_else);
            if (node->else_branch) {
                gen(node->else_branch);
            }
            
            printf("L%d:\n", lbl_end);
            break;
        }

        case NODE_WHILE: {
            int lbl_start = new_label();
            int lbl_end = new_label();

            printf("L%d:\n", lbl_start);
            gen(node->left); // Condition
            printf("JZ L%d\n", lbl_end);
            
            gen(node->right); // Body
            printf("JMP L%d\n", lbl_start);
            
            printf("L%d:\n", lbl_end);
            break;
        }

        case NODE_BLOCK: {
            Symbol *saved_sym_table = sym_table; // Save visibility state
            
            ASTNode *stmt = node->left;
            while (stmt) {
                gen(stmt);
                stmt = stmt->next;
            }
            
            // Restore visibility state (popping block-local variables)
            // Note: We do NOT reset global_addr_counter so addresses don't collide
            sym_table = saved_sym_table;
            break;
        }

        case NODE_FUNC: {
            // "func name() { body }"
            // In Lab 4 ISA, we can use labels.
            // JMP over the function body so we don't execute it linearly
            int lbl_func_end = new_label();
            printf("JMP L%d\n", lbl_func_end);
            
            // Label for the function
            printf("%s:\n", node->id);
            gen(node->left); // Body
            // Assume Void return? If implicit, add RET?
            // "return" node will generate RET.
            // Fallback RET just in case
            printf("RET\n"); 
            
            printf("L%d:\n", lbl_func_end);
            
            // Register function in symbol table?
            // Wait, CALL instruction takes an ADDRESS (Label).
            // Assembler handles labels.
            // So we don't need to put functions in "var" symbol table.
            break;
        }

        case NODE_RETURN: {
            if (node->left) {
                gen(node->left);
            }
            printf("RET\n");
            break;
        }

        case NODE_CALL: {
            // "name()"
            printf("CALL %s\n", node->id);
            break;
        }

        case NODE_PRINT: {
            gen(node->left); // Push expression
            printf("PRINT\n");
            break;
        }

        default:
            fprintf(stderr, "Error: Unknown Node Type %d\n", node->type);
    }
}

extern FILE *yyin;

int main(int argc, char **argv) {
    if (argc > 1) {
        FILE *f = fopen(argv[1], "r");
        if (!f) {
            perror(argv[1]);
            return 1;
        }
        yyin = f;
    }
    fprintf(stderr, "Compiler started...\n");
    if (yyparse() == 0) {
        fprintf(stderr, "Parsing successful.\n");
        if (root) {
            fprintf(stderr, "Root exists. Generating code...\n");
            ASTNode *curr = root;
            // Root is a statement_list
            while (curr) {
                 gen(curr);
                 curr = curr->next;
            }
        } else {
             fprintf(stderr, "Root is NULL!\n");
        }
        printf("HALT\n");
    } else {
        fprintf(stderr, "Parsing failed.\n");
        return 1;
    }
    return 0;
}
