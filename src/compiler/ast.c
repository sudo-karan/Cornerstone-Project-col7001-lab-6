// [INTEGRATION] COPIED FROM: Cornerstone-Project-col7001-lab-2b-or-3/src
// src/ast.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"


extern int yylineno; // From Lexer

// Helper to safely allocate memory
ASTNode* new_node(NodeType type) {
    ASTNode *node = (ASTNode*)malloc(sizeof(ASTNode));
    if (!node) {
        perror("malloc failed");
        exit(1);
    }
    node->type = type;
    node->left = node->right = node->else_branch = node->next = NULL;
    node->id = node->op = NULL;
    node->int_val = 0;
    node->line = yylineno;
    return node;
}

ASTNode* create_num(int val) {
    ASTNode *node = new_node(NODE_NUM);
    node->int_val = val;
    return node;
}

ASTNode* create_var(char *id) {
    ASTNode *node = new_node(NODE_VAR);
    node->id = strdup(id); // Copy the string "x"
    return node;
}

ASTNode* create_decl(char *id, ASTNode *expr) {
    ASTNode *node = new_node(NODE_VAR_DECL);
    node->id = strdup(id);
    node->left = expr; // The initial value
    return node;
}

ASTNode* create_assign(char *id, ASTNode *expr) {
    ASTNode *node = new_node(NODE_ASSIGN);
    node->id = strdup(id);
    node->left = expr;
    return node;
}

ASTNode* create_bin_op(char *op, ASTNode *left, ASTNode *right) {
    ASTNode *node = new_node(NODE_BIN_OP);
    node->op = strdup(op);
    node->left = left;
    node->right = right;
    return node;
}

ASTNode* create_if(ASTNode *cond, ASTNode *then_branch, ASTNode *else_branch) {
    ASTNode *node = new_node(NODE_IF);
    node->left = cond;
    node->right = then_branch;
    node->else_branch = else_branch;
    return node;
}

ASTNode* create_while(ASTNode *cond, ASTNode *body) {
    ASTNode *node = new_node(NODE_WHILE);
    node->left = cond;
    node->right = body;
    return node;
}

ASTNode* create_block(ASTNode *statements) {
    ASTNode *node = new_node(NODE_BLOCK);
    node->left = statements;
    return node;
}

ASTNode* create_func(char* name, ASTNode* body) {
    ASTNode* node = new_node(NODE_FUNC);
    node->id = strdup(name); 
    node->left = body;
    return node;
}

ASTNode* create_return(ASTNode* expr) {
    ASTNode* node = new_node(NODE_RETURN);
    node->left = expr;
    return node;
}

ASTNode* create_call(char* name, ASTNode* arg) {
    ASTNode* node = new_node(NODE_CALL);
    node->id = strdup(name);
    node->left = arg;
    return node;
}

ASTNode* create_print(ASTNode* expr) {
    ASTNode* node = new_node(NODE_PRINT);
    node->left = expr;
    return node;
}

// Simple recursive printer to see our tree structure
void print_ast(ASTNode *node, int level) {
    if (!node) return;

    // Indent based on depth
    for (int i = 0; i < level; i++) printf("  ");

    switch (node->type) {
        case NODE_NUM: printf("NUM: %d\n", node->int_val); break;
        case NODE_VAR: printf("VAR: %s\n", node->id); break;
        case NODE_VAR_DECL: printf("DECL: %s\n", node->id); break;
        case NODE_ASSIGN: printf("ASSIGN: %s\n", node->id); break;
        case NODE_BIN_OP: printf("OP: %s\n", node->op); break;
        case NODE_IF: printf("IF\n"); break;
        case NODE_WHILE: printf("WHILE\n"); break;
        case NODE_BLOCK: printf("BLOCK\n"); break;
        case NODE_FUNC: printf("FUNC DEF: %s\n", node->id); break;
        case NODE_RETURN: printf("RETURN\n"); break;
        case NODE_CALL: printf("CALL: %s()\n", node->id); break;
        case NODE_PRINT: printf("PRINT\n"); break;
    }
    
    print_ast(node->left, level + 1);
    print_ast(node->right, level + 1);
    if (node->else_branch) print_ast(node->else_branch, level + 1);
    
    // Print the next statement in the list (at the same level)
    if (node->next) print_ast(node->next, level);
}
