/**
 * @file p1-lexer.c
 * @brief Compiler phase 1: lexer
 */
#include "p1-lexer.h"
//AI was used to debug my null string check, assistance for properly using backspace in my keyword regex, ensuring utf-8 support for strings, and for stress testing my regexes"

TokenQueue* lex (const char* text)
{

    if (text == NULL) {
        Error_throw_printf("Input string is NULL!\n");
    }

    TokenQueue* tokens = TokenQueue_new();

    /* compile regular expressions */
    Regex* whitespace = Regex_new("^[ \t\n\r]");

    Regex* identifier = Regex_new("^[a-zA-Z]([a-zA-Z]|_|[0-9])*");

    Regex* symbol =
        Regex_new("^((==)|(<=)|(>=)|(!=)|(&&)|(\\|\\|)|(/)|(\\])|(-)|[()+*\\{\\}\\[\\%=;!><,])");

    Regex* decimal_integer = Regex_new("^(0|[1-9][0-9]*)");

    Regex* string_literal =
        Regex_new("^\"(\\\\[nt\"\\\\]|[^\\\\\n\r])*\""); //use negation rather than listing all of UTF-8 value ranges

    Regex* hex_literal = Regex_new("^(0x([0-9]|[a-fA-F])*)");

    Regex* invalid_hex_literal =
        Regex_new("^(0x([0]+)(([0-9]|[a-fA-F])+))"); //catches 0 padded hex numbers

    Regex* keyword =
        Regex_new("^(def|if|else|while|return|break|continue|int|bool|void|true|false)\\b");

    Regex* reserved =
        Regex_new("(for|callout|class|interface|extends|implements|new|this|string|float|double|null)");

    Regex* comments =
        Regex_new("^//([\\n\\t\"\\]|[^\n\r\\])*"); //same as string, just preceeded with //

    /* read and handle input */
    char match[MAX_TOKEN_LEN];
    int new_line = 1;
    while (*text != '\0') {

        if(*text == '\n') {
            new_line++;
        }
        /* match regular expressions */
        if (Regex_match(whitespace, text, match)) {
            /* ignore whitespace */
        }else if (Regex_match(string_literal,text, match)) {
            TokenQueue_add(tokens, Token_new(STRLIT, match, new_line));
        }else if (Regex_match(comments,text, match)) {

        }else if(Regex_match(reserved,text,match)) {
            //Cleanup prior to error
            Regex_free(whitespace);
            Regex_free(identifier);
            Regex_free(symbol);
            Regex_free(decimal_integer);
            Regex_free(hex_literal);
            Regex_free(string_literal);
            Regex_free(reserved);
            Regex_free(comments);
            Regex_free(keyword);
            Regex_free(invalid_hex_literal);
            TokenQueue_free(tokens);
            Error_throw_printf("Reserved word!\n");
        }else if (Regex_match(identifier, text, match) && !(Regex_match(keyword,text, match))) {
            TokenQueue_add(tokens, Token_new(ID, match, new_line));
        }else if (Regex_match(keyword,text, match)) {
            TokenQueue_add(tokens, Token_new(KEY, match, new_line));
        }else if (Regex_match(invalid_hex_literal,text, match)) {
            //Cleanup prior to error
            Regex_free(whitespace);
            Regex_free(identifier);
            Regex_free(symbol);
            Regex_free(decimal_integer);
            Regex_free(hex_literal);
            Regex_free(string_literal);
            Regex_free(reserved);
            Regex_free(comments);
            Regex_free(keyword);
            Regex_free(invalid_hex_literal);
            TokenQueue_free(tokens);
            Error_throw_printf("Invalid Hex Literal!\n");
        }else if (Regex_match(hex_literal,text, match)) {
            TokenQueue_add(tokens, Token_new(HEXLIT, match, new_line));
        }else if (Regex_match(decimal_integer,text, match)) {
            TokenQueue_add(tokens, Token_new(DECLIT, match, new_line));
        }else if (Regex_match(symbol,text, match)) {
            TokenQueue_add(tokens, Token_new(SYM, match, new_line));
        } else {
            //Cleanup prior to error
            Regex_free(whitespace);
            Regex_free(identifier);
            Regex_free(symbol);
            Regex_free(decimal_integer);
            Regex_free(hex_literal);
            Regex_free(string_literal);
            Regex_free(reserved);
            Regex_free(comments);
            Regex_free(keyword);
            Regex_free(invalid_hex_literal);
            TokenQueue_free(tokens);
            Error_throw_printf("Invalid token!\n");
        }

        /* skip matched text to look for next token */
        text += strlen(match);
    }

    /* clean up */
    Regex_free(whitespace);
    Regex_free(identifier);
    Regex_free(symbol);
    Regex_free(decimal_integer);
    Regex_free(hex_literal);
    Regex_free(string_literal);
    Regex_free(reserved);
    Regex_free(comments);
    Regex_free(keyword);
    Regex_free(invalid_hex_literal);
    return tokens;
}

