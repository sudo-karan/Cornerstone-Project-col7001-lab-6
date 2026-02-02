// [INTEGRATION] COPIED FROM: Cornerstone-Project-col7001-lab-2b-or-3/src
// src/ast.h
#ifndef AST_H
#define AST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Enum to identify the type of node
typedef enum {
    NODE_VAR_DECL,  // "var x = 10;"
    NODE_ASSIGN,    // "x = 5;"
    NODE_IF,        // "if (x < 10) ..."
    NODE_WHILE,     // "while (x > 0) ..."
    NODE_BLOCK,     // "{ ... }"
    NODE_BIN_OP,    // "x + 5" or "x < 10"
    NODE_NUM,       // "10"
    NODE_VAR,      // "x" (using a variable)
    NODE_FUNC,      // Function Definition "func f() { ... }"
    NODE_RETURN,    // Return statement "return x;"
    NODE_CALL       // Function call "f()" */
} NodeType;

// The Main Node Structure
typedef struct ASTNode {
    NodeType type;
    
    // For Variables (e.g., "x")
    char *id;
    
    // For Numbers (e.g., 10)
    int int_val;
    
    // For Binary Ops (e.g., '+', '-', '<')
    char *op;
    
    // Children (The tree structure)
    struct ASTNode *left;
    struct ASTNode *right;
    struct ASTNode *third;
    struct ASTNode *else_branch; // Only for IF-ELSE
    struct ASTNode *next;        // To link statements in a list
} ASTNode;

// Function Prototypes (We will implement these in ast.c)
ASTNode* create_num(int val);
ASTNode* create_var(char *id);
ASTNode* create_decl(char *id, ASTNode *expr);
ASTNode* create_assign(char *id, ASTNode *expr);
ASTNode* create_bin_op(char *op, ASTNode *left, ASTNode *right);
ASTNode* create_if(ASTNode *cond, ASTNode *then_branch, ASTNode *else_branch);
ASTNode* create_while(ASTNode *cond, ASTNode *body);
ASTNode* create_block(ASTNode *statements);

// Helper to print the tree (for debugging)
void print_ast(ASTNode *node, int level);

ASTNode* create_func(char* name, ASTNode* body);
ASTNode* create_return(ASTNode* expr);
ASTNode* create_call(char* name, ASTNode* arg);

#endif
