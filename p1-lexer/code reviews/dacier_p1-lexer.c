/**
 * Kane Dacier
 * I used some assistance from generative AI as stated in my code
 * @file p1-lexer.c
 * @brief Compiler phase 1: lexer
 */
#include "p1-lexer.h"
#include <string.h>
/*
    IsKeyword: Helper function that returns True if the given text is a keyword

*/

bool IsKeyword(char* text){
    // AI ACKNOWLEDGEMENT: I pasted the list of keywords into chatGPT to generate this array
    // so I didnt have to type it myself.
    char *keywords[] = {
        "def",
        "if",
        "else",
        "while",
        "return",
        "break",
        "continue",
        "int",
        "bool",
        "void",
        "true",
        "false",
        

    }; 
    for (int i = 0; i < 12; i++){
        if (strcmp(keywords[i], text) == 0){
            return true;
        }
    } 
    return false; 
}

/*
    InvalidKeyword: Helper function that returns True if the given text is a reserved 
    word in decaf.

    AI ACKNOWLEDGEMENT: I plugged the list of reserved words and the IsKeyword function into
        ChatGPT to generate this function so I didnt have to type it out.
*/
bool IsInvalidKeyword(char* text) {
    // Keywords that are considered invalid in this context
    char *invalidKeywords[] = {
        "for",
        "callout",
        "class",
        "interface",
        "extends",
        "implements",
        "new",
        "this",
        "string",
        "float",
        "double",
        "null"
    };

    for (int i = 0; i < 12; i++) {
        if (strcmp(invalidKeywords[i], text) == 0) {
            return true;
        }
    }
    return false;
}


TokenQueue* lex (const char* text)
{
    TokenQueue* tokens = TokenQueue_new();

    /* compile regular expressions */
    Regex* whitespace =     Regex_new("^[ \n\t]");
    Regex* letter =         Regex_new("^[a-z]");
    Regex* identifier =     Regex_new("^[a-zA-Z][a-zA-Z0-9/_]*");
    Regex* newline =        Regex_new("^[\n]");
    Regex* symbol =         Regex_new("^([][()+!=*%;<>{}|,/]|-)");
    Regex* equal_to =       Regex_new("^==");
    Regex* less_equal =     Regex_new("^<=");
    Regex* greater_equal =  Regex_new("^>=");
    Regex* not_equal =      Regex_new("^!=");
    Regex* bitwise_and =    Regex_new("^&&");
    Regex* bitwise_or =     Regex_new("^\\|\\|"); 
    
    // Used AI to generate the comment Regex since mine wasnt working
    Regex* comment =        Regex_new("^\\/\\/[^\n]*");
    Regex* decimal =        Regex_new("^(0|[1-9][0-9]*)");
    Regex* hex =            Regex_new("^0x[0-9a-f]*");
    // Also used AI to fix my string Regex
    Regex* string =     Regex_new("^\\\"(\\\\.|[^\\\"\\\\])*\\\"");
    /* read and handle input */
    char match[MAX_TOKEN_LEN];
    /* Handle a null pointer to the text parameter gracefully */
    if (text == NULL){
       Error_throw_printf("Null Pointer to Public Function");
    }

    int linecount = 1;

    while (*text != '\0') {
        /* match regular expressions */
        if (Regex_match(whitespace, text, match) ||(Regex_match(comment, text, match) )) {
            /* ignore whitespace and comments */
            if (Regex_match(newline, text, match)){
                // If newline is found, increment current line numner
                linecount += 1;
            }    
        // Check for a Hexadecimal token first so 0x is checked before 0
        }
        else if (Regex_match(hex, text, match)){
            TokenQueue_add(tokens, Token_new(HEXLIT, match, linecount));
        } else if (Regex_match(identifier, text, match)) {
            /* Check if text is a valid keyword */
            if (IsKeyword(match)){
                TokenQueue_add(tokens, Token_new(KEY, match, linecount));
            // If text is a reserved keyword throw an error
            } else if (IsInvalidKeyword(match)){
                printf("[LEX ERROR] Reserved keyword at line %d \n", linecount);
                printf("line %d| %s \n", linecount, match);
                Error_throw_printf("Exit\n");
            }
            else {
                TokenQueue_add(tokens, Token_new(ID, match, linecount));
            }
        
        // I made use of short circuiting in C to check for two character symbols before one character symbols
        }   else if (Regex_match(equal_to, text, match) ||
                   Regex_match(less_equal, text, match) || 
                   Regex_match(greater_equal, text, match) ||
                   Regex_match(not_equal, text, match) ||
                   Regex_match(bitwise_and, text, match) ||
                   Regex_match(bitwise_or, text, match) ||
                   Regex_match(symbol, text, match)){
            TokenQueue_add(tokens, Token_new(SYM, match, linecount));
        } 
        else if (Regex_match(decimal, text, match)){
            TokenQueue_add(tokens, Token_new(DECLIT, match, linecount));
        } else if (Regex_match(string, text, match)){
            TokenQueue_add(tokens, Token_new(STRLIT, match, linecount));
        } 
        /* Ignore all comments*/
        else if (!Regex_match(comment, text, match)) {
            
            // Print line number and the text where the error is 
            printf("[LEX ERROR] Invalid token at line %d \n %d | %s \n", linecount, linecount, text);
            Error_throw_printf("Exit\n");

        }
 
        /* skip matched text to look for next token */
        text += strlen(match);
    }
 
    /* clean up */
    Regex_free(whitespace);
    Regex_free(letter);
    Regex_free(identifier);
    Regex_free(newline);
    Regex_free(symbol);
    Regex_free(equal_to);
    Regex_free(less_equal);
    Regex_free(greater_equal);
    Regex_free(not_equal);
    Regex_free(bitwise_and);
    Regex_free(bitwise_or);
    Regex_free(comment);
    Regex_free(decimal);
    Regex_free(hex);
    Regex_free(string);

    return tokens;
}

