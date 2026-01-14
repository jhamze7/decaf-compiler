/**
 * @file p2-parser.c
 * @brief Compiler phase 2: parser
 * 
 * Author: Julian Hamze
 * 
 * AI Statement: I used code generation to assist me with the helper functions,
 * as well as some assistance with design and debugging.
 * 
 */

#include "p2-parser.h"

static ASTNode* parse_block(TokenQueue* input);
static ASTNode* parse_stmt(TokenQueue* input);
static ASTNode* parse_literal(TokenQueue* input);
static ASTNode* parse_expr(TokenQueue* input);
static ASTNode* parse_or(TokenQueue* in);
static ASTNode* parse_and(TokenQueue* in);
static ASTNode* parse_eq(TokenQueue* in);
static ASTNode* parse_rel(TokenQueue* in);
static ASTNode* parse_add(TokenQueue* in);
static ASTNode* parse_mul(TokenQueue* in);
static ASTNode* parse_unary(TokenQueue* in);
static ASTNode* parse_base_expr(TokenQueue* in);
static ParameterList* parse_paramlist(TokenQueue* input);
static void parse_id(TokenQueue* input, char* buffer);
static bool check_next_token_type(TokenQueue* input, TokenType type);
static bool check_next_token(TokenQueue* input, TokenType type, const char* text);
DecafType parse_type(TokenQueue* input);
ASTNode* parse_vardecl(TokenQueue* input);

/*
 * helper functions
 */

static bool is_reserved_kw(const char* s) {
    return  token_str_eq(s, "if")      || token_str_eq(s, "else")   ||
            token_str_eq(s, "while")   || token_str_eq(s, "return") ||
            token_str_eq(s, "break")   || token_str_eq(s, "continue") ||
            token_str_eq(s, "def")     || token_str_eq(s, "int")    ||
            token_str_eq(s, "bool")    || token_str_eq(s, "void")   ||
            token_str_eq(s, "true")    || token_str_eq(s, "false");
}

static bool is_bool_kw(const char* s) {
    return token_str_eq(s, "true") || token_str_eq(s, "false");
}

// Used to handle statements like "return true && false"
static bool is_disallowed_after_return_text(const char* s) {
    return is_reserved_kw(s) && !is_bool_kw(s);
}


static bool is_expr_start(TokenQueue* input)
{
    if (TokenQueue_is_empty(input)) return false;
    if (check_next_token(input, SYM, "(")) return true;
    if (check_next_token_type(input, ID)) return true;
    if (check_next_token_type(input, DECLIT)) return true;
    if (check_next_token_type(input, HEXLIT)) return true;
    if (check_next_token_type(input, STR)) return true;
    if (check_next_token(input, KEY, "true") || check_next_token(input, KEY, "false")) return true;
    if (check_next_token(input, SYM, "!") || check_next_token(input, SYM, "-")) return true;

    return false;
}

/**
 * @brief Look up the source line of the next token in the queue.
 * 
 * @param input Token queue to examine
 * @returns Source line
 */
int get_next_token_line (TokenQueue* input)
{
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input\n");
    }
    return TokenQueue_peek(input)->line;
}

/**
 * @brief Check next token for a particular type and text and discard it
 * 
 * Throws an error if there are no more tokens or if the next token in the
 * queue does not match the given type or text.
 * 
 * @param input Token queue to modify
 * @param type Expected type of next token
 * @param text Expected text of next token
 */
void match_and_discard_next_token (TokenQueue* input, TokenType type, const char* text)
{
    // if (TokenQueue_is_empty(input)) {
    //     Error_throw_printf("Unexpected end of input (expected \'%s\')\n", text);
    // }
    // Token* token = TokenQueue_remove(input);
    // if (token->type != type || !token_str_eq(token->text, text)) {
    //     Token_free(token);
    //     Error_throw_printf("Expected \'%s\' but found '%s' on line %d\n",
    //             text, token->text, get_next_token_line(input));
    // }
    // Token_free(token);
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input (expected '%s')\n", text);
    }

    Token* token = TokenQueue_remove(input);

    if (token->type != type || !token_str_eq(token->text, text)) {
        int line = token->line;
        const char* got = token->text;
        Error_throw_printf("Expected '%s' but found '%s' on line %d\n", text, got, line);
    }

    Token_free(token);
}

/**
 * @brief Remove next token from the queue
 * 
 * Throws an error if there are no more tokens.
 * 
 * @param input Token queue to modify
 */
void discard_next_token (TokenQueue* input)
{
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input\n");
    }
    Token_free(TokenQueue_remove(input));
}

/**
 * @brief Look ahead at the type of the next token
 * 
 * @param input Token queue to examine
 * @param type Expected type of next token
 * @returns True if the next token is of the expected type, false if not
 */
bool check_next_token_type (TokenQueue* input, TokenType type)
{
    if (TokenQueue_is_empty(input)) {
        return false;
    }
    Token* token = TokenQueue_peek(input);
    return (token->type == type);
}

/**
 * @brief Look ahead at the type and text of the next token
 * 
 * @param input Token queue to examine
 * @param type Expected type of next token
 * @param text Expected text of next token
 * @returns True if the next token is of the expected type and text, false if not
 */
bool check_next_token (TokenQueue* input, TokenType type, const char* text)
{
    if (TokenQueue_is_empty(input)) {
        return false;
    }
    Token* token = TokenQueue_peek(input);
    return (token->type == type) && (token_str_eq(token->text, text));
}

//----------------------------------LITERALS-------------------------------------------------------
static ASTNode* parse_literal(TokenQueue* input)
{
    int line = get_next_token_line(input);

    if (check_next_token_type(input, DECLIT)) { //Decimal literal
        Token* tok = TokenQueue_remove(input);
        long val = strtol(tok->text, NULL, 0);
        Token_free(tok);
        return LiteralNode_new_int((int)val, line);
    }

    if (check_next_token_type(input, HEXLIT)) { //Hexadecimal literal
        Token* tok = TokenQueue_remove(input);
        long val = strtol(tok->text, NULL, 0);
        Token_free(tok);
        return LiteralNode_new_int((int)val, line);
    }

    if (check_next_token_type(input, STR)) { //String literal
        Token* tok = TokenQueue_remove(input);
        ASTNode* s = LiteralNode_new_string(tok->text, line);
        Token_free(tok); // Free token before returing
        return s;
    }

    //True or False
    if (check_next_token(input, KEY, "true")) {
        discard_next_token(input);
        return LiteralNode_new_bool(true, line);
    }
    if (check_next_token(input, KEY, "false")) {
        discard_next_token(input);
        return LiteralNode_new_bool(false, line);
    }

    Error_throw_printf("Expected literal on line %d\n", line);
    return NULL;
}

//----------------------------------BASE EXPRESSIONS-----------------------------------------------

static ASTNode* parse_base_expr(TokenQueue* in)
{
    int line = get_next_token_line(in);

    // Parenthesized
    if (check_next_token(in, SYM, "(")) {
        discard_next_token(in);
        ASTNode* e = parse_expr(in);
        match_and_discard_next_token(in, SYM, ")");
        return e;
    }

    // ID -> FuncCall or Location
    if (check_next_token_type(in, ID)) {
        char name[MAX_ID_LEN];
        parse_id(in, name);

        // FuncCall
        if (check_next_token(in, SYM, "(")) {
            discard_next_token(in);
            NodeList* args = NodeList_new();
            if (!check_next_token(in, SYM, ")")) {
                do {
                    ASTNode* e = parse_expr(in);
                    NodeList_add(args, e);
                    if (!check_next_token(in, SYM, ",")) break;
                    discard_next_token(in);
                } while (1);
            }
            match_and_discard_next_token(in, SYM, ")");
            return FuncCallNode_new(name, args, line);
        }

        // Location
        ASTNode* index = NULL;
        if (check_next_token(in, SYM, "[")) {
            discard_next_token(in);
            index = parse_expr(in);
            match_and_discard_next_token(in, SYM, "]");
        }
        return LocationNode_new(name, index, line);
    }

    // Literal (int, hex, str, bool)
    if (check_next_token_type(in, DECLIT) || check_next_token_type(in, HEXLIT) ||
        check_next_token_type(in, STR) || check_next_token(in, KEY, "true") ||
        check_next_token(in, KEY, "false")) {
        return parse_literal(in);
    }

    Token* t = TokenQueue_peek(in);
    Error_throw_printf("Unexpected token '%s' on line %d (expected expression)\n", t->text, line);
    return NULL;
}

//-----------------------------------UNARY OPERATORS-----------------------------------------------

static ASTNode* parse_unary(TokenQueue* in)
{
    if (check_next_token(in, SYM, "!")) {
        int line = get_next_token_line(in);
        discard_next_token(in);
        ASTNode* child = parse_unary(in);
        return UnaryOpNode_new(NOTOP, child, line);
    }
    if (check_next_token(in, SYM, "-")) {
        int line = get_next_token_line(in);
        discard_next_token(in);
        ASTNode* child = parse_unary(in);
        return UnaryOpNode_new(NEGOP, child, line);
    }
    return parse_base_expr(in);
}

//----------------------------------BINARY OPERATORS-----------------------------------------------
static ASTNode* parse_or(TokenQueue* in)
{
    ASTNode* left = parse_and(in);
    while (check_next_token(in, SYM, "||")) {
        int line = get_next_token_line(in);
        discard_next_token(in);
        ASTNode* right = parse_and(in);
        left = BinaryOpNode_new(OROP, left, right, line);
    }
    return left;
}

static ASTNode* parse_and(TokenQueue* in)
{
    ASTNode* left = parse_eq(in);
    while (check_next_token(in, SYM, "&&")) {
        int line = get_next_token_line(in);
        discard_next_token(in);
        ASTNode* right = parse_eq(in);
        left = BinaryOpNode_new(ANDOP, left, right, line);
    }
    return left;
}

static ASTNode* parse_eq(TokenQueue* in)
{
    ASTNode* left = parse_rel(in);
    while (check_next_token(in, SYM, "==") || check_next_token(in, SYM, "!=")) {
        int line = get_next_token_line(in);
        bool is_eq = check_next_token(in, SYM, "==");
        discard_next_token(in);
        ASTNode* right = parse_rel(in);
        left = BinaryOpNode_new(is_eq ? EQOP : NEQOP, left, right, line);
    }
    return left;
}

static ASTNode* parse_rel(TokenQueue* in)
{
    ASTNode* left = parse_add(in);
    while (check_next_token(in, SYM, "<") || check_next_token(in, SYM, "<=") ||
           check_next_token(in, SYM, ">") || check_next_token(in, SYM, ">=")) {
        int line = get_next_token_line(in);
        BinaryOpType op;
        if (check_next_token(in, SYM, "<"))       op = LTOP;
        else if (check_next_token(in, SYM, "<=")) op = LEOP;
        else if (check_next_token(in, SYM, ">"))  op = GTOP;
        else                                      op = GEOP;
        discard_next_token(in);
        ASTNode* right = parse_add(in);
        left = BinaryOpNode_new(op, left, right, line);
    }
    return left;
}

static ASTNode* parse_add(TokenQueue* in)
{
    ASTNode* left = parse_mul(in);
    while (check_next_token(in, SYM, "+") || check_next_token(in, SYM, "-")) {
        int line = get_next_token_line(in);
        bool is_add = check_next_token(in, SYM, "+");
        discard_next_token(in);
        ASTNode* right = parse_mul(in);
        left = BinaryOpNode_new(is_add ? ADDOP : SUBOP, left, right, line);
    }
    return left;
}

static ASTNode* parse_mul(TokenQueue* in)
{
    ASTNode* left = parse_unary(in);
    while (check_next_token(in, SYM, "*") || check_next_token(in, SYM, "/") || check_next_token(in, SYM, "%")) {
        int line = get_next_token_line(in);
        BinaryOpType op = check_next_token(in, SYM, "*") ? MULOP :
                          check_next_token(in, SYM, "/") ? DIVOP : MODOP;
        discard_next_token(in);
        ASTNode* right = parse_unary(in);
        left = BinaryOpNode_new(op, left, right, line);
    }
    return left;
}

//---------------------------------------EXPRESSIONS-----------------------------------------------

static ASTNode* parse_expr(TokenQueue* input)
{
    return parse_or(input);
}

//---------------------------------------STATEMENTS------------------------------------------------

static ASTNode* parse_stmt(TokenQueue* input)
{
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input (expected statement)\n");
    }
    int line = get_next_token_line(input);

    // if (...) Block (else Block)?
    if (check_next_token(input, KEY, "if")) {
        discard_next_token(input);                  // 'if'
        match_and_discard_next_token(input, SYM, "(");
        ASTNode* cond = parse_expr(input);
        match_and_discard_next_token(input, SYM, ")");

        ASTNode* ifblk = parse_block(input);
        ASTNode* elseblk = NULL;

        if (check_next_token(input, KEY, "else")) {
            discard_next_token(input);              // 'else'
            elseblk = parse_block(input);
        }
        return ConditionalNode_new(cond, ifblk, elseblk, line);
    }

    // while (...) Block
    if (check_next_token(input, KEY, "while")) {
        discard_next_token(input);                  // 'while'
        match_and_discard_next_token(input, SYM, "(");
        ASTNode* cond = parse_expr(input);
        match_and_discard_next_token(input, SYM, ")");
        ASTNode* body = parse_block(input);
        return WhileLoopNode_new(cond, body, line);
    }

    if (check_next_token(input, KEY, "return")) {
        discard_next_token(input);
        int ret_line = line;

        if (check_next_token(input, SYM, ";")) {
            discard_next_token(input);
            return ReturnNode_new(NULL, ret_line);
        }

        // checks to see if the token after the return if a boolean, which is
        // a keyword but should be allowed
        if (check_next_token_type(input, KEY)) {
            Token* t = TokenQueue_peek(input);
            if (is_disallowed_after_return_text(t->text)) {
                Error_throw_printf("Expected expression after 'return' but found '%s' on line %d\n",
                                t->text, ret_line);
            }
        }
        // Also catch identifiers spelled like reserved words
        if (check_next_token_type(input, ID)) {
            Token* t = TokenQueue_peek(input);
            if (is_disallowed_after_return_text(t->text)) {
                Error_throw_printf("Expected expression after 'return' but found '%s' on line %d\n",
                                t->text, ret_line);
            }
        }

        if (!is_expr_start(input)) {
            Token* t = TokenQueue_peek(input);
            if (!t) {
                Error_throw_printf("Expected expression or ';' after 'return' but hit end of input on line %d\n",
                                ret_line);
            }
            Error_throw_printf("Expected expression or ';' after 'return' but found '%s' on line %d\n",
                            t->text, ret_line);
        }

        ASTNode* value = parse_expr(input);
        match_and_discard_next_token(input, SYM, ";");
        return ReturnNode_new(value, ret_line);
    }



    if (check_next_token(input, KEY, "break")) {
        discard_next_token(input);
        match_and_discard_next_token(input, SYM, ";");
        return BreakNode_new(line);
    }

    if (check_next_token(input, KEY, "continue")) {
        discard_next_token(input);
        match_and_discard_next_token(input, SYM, ";");
        return ContinueNode_new(line);
    }

    if (check_next_token(input, SYM, "{")) {
        return parse_block(input);
    }

    if (check_next_token_type(input, ID)) {
    char name[MAX_ID_LEN];
    parse_id(input, name);

    if (check_next_token(input, SYM, "(")) {
        discard_next_token(input);
        NodeList* args = NodeList_new();
        if (!check_next_token(input, SYM, ")")) {
            do {
                ASTNode* e = parse_expr(input);
                NodeList_add(args, e);
                if (!check_next_token(input, SYM, ",")) break;
                discard_next_token(input);
            } while (1);
        }
        match_and_discard_next_token(input, SYM, ")");
        match_and_discard_next_token(input, SYM, ";");
        return FuncCallNode_new(name, args, line);
    }

    ASTNode* index = NULL;
    if (check_next_token(input, SYM, "[")) {
        discard_next_token(input);
        index = parse_expr(input);
        match_and_discard_next_token(input, SYM, "]");
    }
    ASTNode* loc = LocationNode_new(name, index, line);

    match_and_discard_next_token(input, SYM, "=");
    ASTNode* rhs = parse_expr(input);
    match_and_discard_next_token(input, SYM, ";");
    return AssignmentNode_new(loc, rhs, line);
    }

    Token* t = TokenQueue_peek(input);
    Error_throw_printf("Unexpected token '%s' on line %d (while parsing statement)\n",
                       t->text, line);
    return NULL;
}

//---------------------------------------BLOCKS----------------------------------------------------

ASTNode* parse_block(TokenQueue* input)
{
    int line = get_next_token_line(input);

    match_and_discard_next_token(input, SYM, "{");

    NodeList* vars  = NodeList_new();
    NodeList* stmts = NodeList_new();

    while (check_next_token(input, KEY, "int") || check_next_token(input, KEY, "bool")) {
        ASTNode* v = parse_vardecl(input);
        NodeList_add(vars, v);
    }

    while (!TokenQueue_is_empty(input) && !check_next_token(input, SYM, "}")) {
        ASTNode* s = parse_stmt(input);
        NodeList_add(stmts, s);
    }
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input while parsing block (missing '}')\n");
    }

    match_and_discard_next_token(input, SYM, "}");
    return BlockNode_new(vars, stmts, line);
}

//---------------------------------------PARAMS----------------------------------------------------

static ParameterList* parse_paramlist(TokenQueue* input)
{
    ParameterList* params = ParameterList_new();

    if (check_next_token(input, SYM, ")")) {
        return params;
    }

    while (true) {
        int param_line = get_next_token_line(input);

        DecafType ptype = parse_type(input);
        if (ptype == VOID) {
            Error_throw_printf("Parameter cannot have type 'void' on line %d\n", param_line);
        }

        char pname[MAX_ID_LEN];
        parse_id(input, pname);

        ParameterList_add_new(params, pname, ptype);

        if (!check_next_token(input, SYM, ",")) break;
        discard_next_token(input);
    }

    return params;
}

//---------------------------------------FUNCDECL--------------------------------------------------

ASTNode* parse_funcdecl(TokenQueue* input)
{
    int line = get_next_token_line(input);

    match_and_discard_next_token(input, KEY, "def");
    DecafType ret_type = parse_type(input);

    char fname[MAX_ID_LEN];
    parse_id(input, fname);

    match_and_discard_next_token(input, SYM, "(");
    ParameterList* params = parse_paramlist(input);
    match_and_discard_next_token(input, SYM, ")");

    ASTNode* body = parse_block(input);

    return FuncDeclNode_new(fname, ret_type, params, body, line);
}

//---------------------------------------TYPES-----------------------------------------------------

/**
 * @brief Parse and return a Decaf type
 * 
 * @param input Token queue to modify
 * @returns Parsed type (it is also removed from the queue)
 */
DecafType parse_type (TokenQueue* input)
{
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input (expected type)\n");
    }

    Token* token = TokenQueue_remove(input);

    if (token->type != KEY) {
        int line = token->line;
        const char* got = token->text;
        Error_throw_printf("Invalid type '%s' on line %d\n", got, line);
    }

    DecafType t;
    if (token_str_eq("int", token->text))       t = INT;
    else if (token_str_eq("bool", token->text)) t = BOOL;
    else if (token_str_eq("void", token->text)) t = VOID;
    else {
        int line = token->line;
        const char* got = token->text;
        Error_throw_printf("Invalid type '%s' on line %d\n", got, line);
    }

    Token_free(token);
    return t;
}

//---------------------------------------IDENTIFIERS-----------------------------------------------

/**
 * @brief Parse and return a Decaf identifier
 * 
 * @param input Token queue to modify
 * @param buffer String buffer for parsed identifier (should be at least
 * @c MAX_TOKEN_LEN characters long)
 */
void parse_id (TokenQueue* input, char* buffer)
{
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input (expected identifier)\n");
    }
    Token* token = TokenQueue_remove(input);
    if (token->type != ID) {
        Token_free(token);
        Error_throw_printf("Invalid ID '%s' on line %d\n", token->text, get_next_token_line(input));
    }
    snprintf(buffer, MAX_ID_LEN, "%s", token->text);
    Token_free(token);
}

//---------------------------------------VARDECL---------------------------------------------------

// VarDecl     -> Type ID ( '[' DEC ']' )? ';'
ASTNode* parse_vardecl(TokenQueue* input)
{
    int line = get_next_token_line(input);

    DecafType type = parse_type(input);

    char name[MAX_ID_LEN];
    parse_id(input, name);

    bool is_array = false;
    int  length   = 1;

    if (check_next_token(input, SYM, "[")) {
        discard_next_token(input);

        if (!check_next_token_type(input, DECLIT)) {
            Error_throw_printf("Expected array length after '[' on line %d\n", line);
        }
        Token* tlen = TokenQueue_remove(input);
        long val = strtol(tlen->text, NULL, 0);
        Token_free(tlen);

        if (val <= 0) {
            Error_throw_printf("Array length must be positive on line %d\n", line);
        }

        length = (int)val;
        match_and_discard_next_token(input, SYM, "]");
        is_array = true;
    }

    match_and_discard_next_token(input, SYM, ";");

    return VarDeclNode_new(name, type, is_array, length, line);

}

//---------------------------------------PROGRAM---------------------------------------------------

/*
 * node-level parsing functions
 */

// Program     → ( VarDecl | FuncDecl )*
ASTNode* parse_program (TokenQueue* input)
{
    NodeList* vars  = NodeList_new();
    NodeList* funcs = NodeList_new();

    while (!TokenQueue_is_empty(input)) {
        if (check_next_token(input, KEY, "def")) {
            ASTNode* f = parse_funcdecl(input);
            NodeList_add(funcs, f);
        } else {
            ASTNode* v = parse_vardecl(input);
            NodeList_add(vars, v);
        }
    }

    return ProgramNode_new(vars, funcs);
}

ASTNode* parse (TokenQueue* input)
{
    return parse_program(input);
}