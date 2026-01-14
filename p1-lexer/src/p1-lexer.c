/** 
 * @file p1-lexer.c
 * @brief Compiler phase 1: lexer
 * 
 * Author: Julian Hamze
 * Date: 9/10/25
 */
#include "p1-lexer.h"
#include <ctype.h>

//escape codes: \t \n \r
//inside string, need to be escaped
//ref. compiler: /cs/students/cs432/f25/decaf

TokenQueue* lex (const char* text)
{
    TokenQueue* tokens = TokenQueue_new();

    Regex* whitespace = Regex_new("^[ \n\t\r]+");
    Regex* comment = Regex_new("^//[^\r\n]*(\r?\n|$)");
    
    Regex* identifier = Regex_new("^[A-Za-z][A-Za-z0-9_]*");
    Regex* hex = Regex_new("^0[xX][0-9A-Fa-f]+");
    Regex* dec = Regex_new("^(0|[1-9][0-9]*)");
    Regex* str = Regex_new("^\"([^\"\\\\\n\r]|\\\\[nt\\\"\\\\])*\""); 
    Regex* symbol = Regex_new("^(<=|>=|==|!=|&&|\\|\\|"
     "|\\(|\\)|\\{|\\}|\\[|\\]|,|;|=|\\+|-|\\*|/|%|<|>|!)");

    /* read and handle input */
    char match[MAX_TOKEN_LEN];
    int line_count = 1;
    while (*text != '\0') {
 
        /* match regular expressions */
        if (Regex_match(whitespace, text, match) || Regex_match(comment, text, match)) {
            /* ignore whitespace */
            //line_count increment logic
            for (const char *p = match; *p; ++p) if (*p == '\n') line_count++;

        } else if (Regex_match(identifier, text, match)) {
            if (is_keyword(match)) {
                TokenQueue_add(tokens, Token_new(KEY, match, line_count));
            } else {
                if (is_reserved(match)) {
                    Error_throw_printf("The word '%s' is reserved and cannot be used as a variable\n", match);
                } else {
                    TokenQueue_add(tokens, Token_new(ID, match, line_count));
                }
            }

        } else if (Regex_match(hex, text, match)) { //Hexadecimal literal
            TokenQueue_add(tokens, Token_new(HEXLIT, match, line_count)); 
        
        } else if (Regex_match(dec, text, match)) { //Decimal literal
            TokenQueue_add(tokens, Token_new(DECLIT, match, line_count));

        } else if (Regex_match(str, text, match)) { //String literal
            TokenQueue_add(tokens, Token_new(STRLIT, match, line_count));
        } 
        else if (Regex_match(symbol, text, match)) { //Symbols
            TokenQueue_add(tokens, Token_new(SYM, match, line_count));

        } else {
            // Gets text up until space
            int n = 0;
            while (text[n] && text[n] != '\n' && !isspace((unsigned char)text[n]) && n < 40) 
                n++;
            Error_throw_printf("Invalid token on line %d: %.*s\n", line_count, n, text);
        }
 
        /* skip matched text to look for next token */
        text += strlen(match);
    }
 
    /* clean up */
    Regex_free(whitespace);
    Regex_free(comment);
    Regex_free(identifier);
    Regex_free(dec);
    Regex_free(hex);
    Regex_free(str);
    Regex_free(symbol);

    return tokens;
}

/**
 * @brief Check whether a string is language keyword.
 *
 * @param s identifier to check.
 * @return true if s matches a keyword exactly; false otherwise.
 */
bool is_keyword(const char *s) {
    const char* keyword_set[] = {
        "def", "if", "else", "while", "return",
        "break", "continue", "int", "bool",
        "void", "true", "false", NULL
    };

    for (int i = 0; keyword_set[i] != NULL; i++) {
        if (strcmp(s, keyword_set[i]) == 0) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Check whether a string is a reserved (non-keyword) word.
 * 
 * @param s identifier to check.
 * @return true if s matches a reserved word exactly; false otherwise.
 */
bool is_reserved(const char *s) {
    const char* reserved_set[] = {
        "for", "callout", "class", "interface", 
        "extends", "implements", "new", "this", 
        "string", "float", "double", "null", NULL
    };

    for (int i = 0; reserved_set[i] != NULL; i++) {
        if (strcmp(s, reserved_set[i]) == 0) {
            return true;
        }
    }
    return false;
}