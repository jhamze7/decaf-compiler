/**
 * Lexer code for expression parser
 */

#include <ctype.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_STR_LEN 256


/**
 * Tokens
 */

typedef struct token
{
    char text[MAX_STR_LEN];
    enum {DEC, SYM} type;
    struct token *next;

} Token;

Token* Token_new(const char* text)
{
    Token* t = (Token*)malloc(sizeof(Token));
    if (t == NULL) {
        printf("Error: Could not allocate token!\n");
        exit(EXIT_FAILURE);
    }
    snprintf(t->text, MAX_STR_LEN, "%s", text);
    if (strncmp(text, "+", MAX_STR_LEN) == 0 ||
        strncmp(text, "*", MAX_STR_LEN) == 0 ||
        strncmp(text, "(", MAX_STR_LEN) == 0 ||
        strncmp(text, ")", MAX_STR_LEN) == 0) {
        t->type = SYM;
    } else {
        t->type = DEC;
    }
    t->next = NULL;
    return t;
}

void Token_free(Token* t)
{
    free(t);
}


/**
 * Token queues
 */

typedef struct token_queue
{
    Token *front;
    Token *back;

} TokenQueue;

TokenQueue* TokenQueue_new()
{
    TokenQueue* tq = (TokenQueue*)malloc(sizeof(TokenQueue));
    if (tq == NULL) {
        printf("Error: Could not allocate token queue!\n");
        exit(EXIT_FAILURE);
    }
    tq->front = NULL;
    tq->back = NULL;
    return tq;
}

void TokenQueue_add(TokenQueue* tq, Token* t)
{
#ifdef DEBUG
    printf("Debug: Lexing token \"%s\"\n", t->text);
#endif
    if (tq->back == NULL) {
        tq->front = t;
        tq->back = t;
    } else {
        tq->back->next = t;
        tq->back = t;
    }
}

bool TokenQueue_is_empty(TokenQueue* tq)
{
    return tq->front == NULL;
}

Token* TokenQueue_peek(TokenQueue* tq)
{
    if (tq->front == NULL) {
        printf("Error: Unexpected end of input\n");
        exit(EXIT_FAILURE);
    }
    return tq->front;
}

Token* TokenQueue_remove(TokenQueue* tq)
{
    if (tq->front == NULL) {
        printf("Error: Unexpected end of input\n");
        exit(EXIT_FAILURE);
    }
    Token* front = tq->front;
    tq->front = front->next;
    return front;
}

void TokenQueue_free(TokenQueue* t)
{
    free(t);
}


/**
 * Expression grammar lexer (very simple, hand-coded).
 */
TokenQueue* lex(const char* expr)
{
    TokenQueue* tokens = TokenQueue_new();
    char tmp[MAX_STR_LEN];
    int tmp_len = 0;
    for (int i=0; i < strlen(expr); i++) {
        char c = expr[i];
        if (isdigit(c)) {
            tmp[tmp_len++] = c;
            tmp[tmp_len] = '\0';
        } else {
            if (tmp_len > 0) {
                TokenQueue_add(tokens, Token_new(tmp));
                tmp_len = 0;
            }
            if (c == '+' || c == '*' ||
                c == '(' || c == ')') {
                tmp[0] = c;
                tmp[1] = '\0';
                TokenQueue_add(tokens, Token_new(tmp));
                tmp_len = 0;
            } else {
                printf("Error: Invalid character '%c'\n", c);
                exit(EXIT_FAILURE);
            }
        }
    }
    if (tmp_len > 0) {
        TokenQueue_add(tokens, Token_new(tmp));
    }
    return tokens;
}

