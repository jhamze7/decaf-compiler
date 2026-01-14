/**
 * @file p2-parser.c
 * @brief Compiler phase 2: parser
 * @author Sean McCormick
 * AI WAS USED TO HELP REMOVE QUOTES FROM STRLIT
 */

#include "p2-parser.h"

/*
 * helper functions
 */

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
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input (expected \'%s\')\n", text);
    }
    Token* token = TokenQueue_remove(input);
    if (token->type != type || !token_str_eq(token->text, text)) {
        Error_throw_printf("Expected \'%s\' but found '%s' on line %d\n",
                text, token->text, get_next_token_line(input));
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
        Error_throw_printf("Invalid type '%s' on line %d\n", token->text, get_next_token_line(input));
    }
    DecafType t = VOID;
    if (token_str_eq("int", token->text)) {
        t = INT;
    } else if (token_str_eq("bool", token->text)) {
        t = BOOL;
    } else if (token_str_eq("void", token->text)) {
        t = VOID;
    } else {
        Error_throw_printf("Invalid type '%s' on line %d\n", token->text, get_next_token_line(input));
    }
    Token_free(token);
    return t;
}

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
        Error_throw_printf("Invalid ID '%s' on line %d\n", token->text, get_next_token_line(input));
    }
    snprintf(buffer, MAX_ID_LEN, "%s", token->text);
    Token_free(token);
}

/*
 * node-level parsing functions
 */
// Function declarations

ASTNode* parse_expr (TokenQueue* input);
ASTNode* parse_stmt (TokenQueue* input);
ASTNode* parse_vardecl (TokenQueue* input);
ASTNode* parse_funcdecl (TokenQueue* input);
ASTNode* parse_program (TokenQueue* input);
ParameterList* parse_parameters(TokenQueue* input);
ASTNode* parse_block(TokenQueue* input);
ASTNode* parse_base_expr(TokenQueue* input);

ASTNode* parse (TokenQueue* input)
{
    if (input == NULL) {
        Error_throw_printf("Null token queue passed to parser\n");
    }
    return parse_program(input);
}

ASTNode* parse_program (TokenQueue* input)
{
    NodeList* vars = NodeList_new();
    NodeList* funcs = NodeList_new();
    while (!TokenQueue_is_empty(input)) {
        if (check_next_token(input, KEY, "def")) {
            NodeList_add(funcs, parse_funcdecl(input));
        } else {
            NodeList_add(vars, parse_vardecl(input));

        }
    }
    return ProgramNode_new(vars, funcs);
}

ASTNode* parse_vardecl (TokenQueue* input){
    // Variable declaration
    int line = get_next_token_line(input);
    DecafType type = parse_type(input);
    char id[MAX_ID_LEN];
    parse_id(input, id);
    match_and_discard_next_token(input, SYM, ";");
    return VarDeclNode_new(id, type, false, 1, line);
}

ASTNode* parse_funcdecl (TokenQueue* input) {
    // Function declaration
    int line = get_next_token_line(input);
    match_and_discard_next_token(input, KEY, "def");
    DecafType type = parse_type(input);
    char id[MAX_ID_LEN];
    parse_id(input, id);
    match_and_discard_next_token(input, SYM, "(");
    ParameterList* params = parse_parameters(input);
    match_and_discard_next_token(input, SYM, ")");
    ASTNode* body = parse_block(input);
    return FuncDeclNode_new(id, type, params, body, line);
}

// Helper function to parse parameters
ParameterList* parse_parameters(TokenQueue* input) {
    ParameterList* params = ParameterList_new();
    while (!check_next_token(input, SYM, ")")) {
        Parameter* param = (Parameter*)calloc(1, sizeof(Parameter));
        CHECK_MALLOC_PTR(param)
        param->type = parse_type(input);
        parse_id(input, param->name);
        if (check_next_token(input, SYM, ",")) {
            discard_next_token(input);
        }
        ParameterList_add(params, param);
    }
    return params;
}

ASTNode* parse_block(TokenQueue* input) {
    // Block
    int line = get_next_token_line(input) + 1;
    match_and_discard_next_token(input, SYM, "{");
    NodeList* vars = NodeList_new();
    NodeList* stmts = NodeList_new();
    //0 or more var decls or stmts
    while (!check_next_token(input, SYM, "}")) {
        if (check_next_token_type(input, KEY) &&
            (check_next_token(input, KEY, "int") ||
             check_next_token(input, KEY, "bool") ||
             check_next_token(input, KEY, "void"))) {
            NodeList_add(vars, parse_vardecl(input));
        } else {
            NodeList_add(stmts, parse_stmt(input));
        }
    }
    match_and_discard_next_token(input, SYM, "}");
    return BlockNode_new(vars, stmts, line);
}

ASTNode* parse_stmt (TokenQueue* input) {
    if (check_next_token_type(input, ID)) {
        // Check for loc or function call
        char id[MAX_ID_LEN];
        parse_id(input, id);
        if (check_next_token(input, SYM, "(")) {
            // Check for function call
            discard_next_token(input);
            NodeList* args = NodeList_new();
            int line = get_next_token_line(input);
            while (!check_next_token(input, SYM, ")")) {
                NodeList_add(args, parse_expr(input));
                if (check_next_token(input, SYM, ",")) {
                    discard_next_token(input);
                }
            }
            match_and_discard_next_token(input, SYM, ")");
            return FuncCallNode_new(id, args, line);
        } else {
            // Loc = Expr;
            ASTNode* loc = NULL;
            int line = get_next_token_line(input);
            if (check_next_token(input, SYM, "[")) {
                // Check for location with index
                discard_next_token(input);
                ASTNode* index = parse_expr(input);
                match_and_discard_next_token(input, SYM, "]");
                loc = LocationNode_new(id, index, line);
            } else {
                // Location without index
                loc = LocationNode_new(id, NULL, line);
            }
            match_and_discard_next_token(input, SYM, "=");
            ASTNode* expr = parse_expr(input);
            match_and_discard_next_token(input, SYM, ";");
            return AssignmentNode_new(loc, expr, line);            
        }
    } else if (check_next_token(input, KEY, "break")) {
        // Check for break
        discard_next_token(input);
        match_and_discard_next_token(input, SYM, ";");
        return BreakNode_new(1);
    } else if (check_next_token(input, KEY, "continue")) {
        // Check for continue
        discard_next_token(input);
        match_and_discard_next_token(input, SYM, ";");
        return ContinueNode_new(1);
    } else if (check_next_token(input, KEY, "return")) {
        // Check for return
        int line = get_next_token_line(input);
        discard_next_token(input);
        ASTNode* expr = parse_expr(input);
        match_and_discard_next_token(input, SYM, ";");
        return ReturnNode_new(expr, line);
    } else if (check_next_token(input, KEY, "if")) {
        // Check for if
        int line = get_next_token_line(input);
        discard_next_token(input);
        match_and_discard_next_token(input, SYM, "(");
        ASTNode* expr = parse_expr(input);
        match_and_discard_next_token(input, SYM, ")");       
        ASTNode* if_block = parse_block(input);
        ASTNode* else_block = NULL;
        if (check_next_token(input, KEY, "else")) {
            // Check for else
            discard_next_token(input);
            else_block = parse_block(input);
        }
        return ConditionalNode_new(expr, if_block, else_block, line);
    } else if (check_next_token(input, KEY, "while")) {
        // Check for while
        int line = get_next_token_line(input);
        discard_next_token(input);
        match_and_discard_next_token(input, SYM, "(");
        ASTNode* condition = parse_expr(input);
        match_and_discard_next_token(input, SYM, ")");
        ASTNode* body = parse_block(input);
        return WhileLoopNode_new(condition, body, line);
    } else {
        // Invalid statement
        Error_throw_printf("Invalid statement on line %d\n", get_next_token_line(input));
        return NULL;
    }
}

// UNFINISHED, only handles one binop at a time
ASTNode* parse_expr (TokenQueue* input) {
    int line = get_next_token_line(input);
    if (check_next_token(input, SYM, "-") || check_next_token(input, SYM, "!")) {
        // Check for unary op
        UnaryOpType op = check_next_token(input, SYM, "-") ? NEGOP : NOTOP;
        discard_next_token(input);
        return UnaryOpNode_new(op, parse_base_expr(input), line);
    } else {
        // Check for binary op
        int line = get_next_token_line(input);
        ASTNode* left = parse_base_expr(input);
        BinaryOpType op = UNKNOWN;
        if (check_next_token(input, SYM, "*")) {
            op = MULOP;
        } else if (check_next_token(input, SYM, "/")) {
            op = DIVOP;
        } else if (check_next_token(input, SYM, "%")) {
            op = MODOP;
        } else if (check_next_token(input, SYM, "+")) {
            op = ADDOP;
        } else if (check_next_token(input, SYM, "-")) {
            op = SUBOP;
        } else if (check_next_token(input, SYM, "<")) {
            op = LTOP;
        } else if (check_next_token(input, SYM, "<=")) {
            op = LEOP;
        } else if (check_next_token(input, SYM, ">")) {
            op = GTOP;
        } else if (check_next_token(input, SYM, ">=")) {
            op = GEOP;
        } else if (check_next_token(input, SYM, "==")) {
            op = EQOP;
        } else if (check_next_token(input, SYM, "!=")) {
            op = NEQOP;
        } else if (check_next_token(input, SYM, "&&")) {
            op = ANDOP;
        } else if (check_next_token(input, SYM, "||")) {
            op = OROP;
        } else {
            // no binary op, just return the base expression
            return left;
        }
        discard_next_token(input);
        // Parse the right-hand side expression
        ASTNode* right = parse_base_expr(input);
        return BinaryOpNode_new(op, left, right, line);
    }
}

ASTNode* parse_base_expr (TokenQueue* input) {
    if (check_next_token(input, SYM, "(")) {
        // Check for (expr)
        discard_next_token(input);
        ASTNode* expr = parse_expr(input);
        match_and_discard_next_token(input, SYM, ")");
        return expr;
    } 
    else if (check_next_token_type(input, ID)) {
            // Check for loc or function call
            char id[MAX_ID_LEN];
            parse_id(input, id);
        if (check_next_token(input, SYM, "(")) {
            int line = get_next_token_line(input);
            // Check for function call
            discard_next_token(input);
            NodeList* args = NodeList_new();
            while (!check_next_token(input, SYM, ")")) {
                NodeList_add(args, parse_expr(input));
                if (check_next_token(input, SYM, ",")) {
                    discard_next_token(input);
                }
            }
            match_and_discard_next_token(input, SYM, ")");
            return FuncCallNode_new(id, args, line);
        } else {
            int line = get_next_token_line(input);
            if (check_next_token(input, SYM, "[")) {
                // Check for location with index
                discard_next_token(input);
                ASTNode* index = parse_expr(input);
                match_and_discard_next_token(input, SYM, "]");
                return LocationNode_new(id, index, line);
            } else {
                // Location without index
                return LocationNode_new(id, NULL, line);
           }
        }
    } else {
        // Check for literal
        ASTNode* operand = NULL;
            if (check_next_token(input, KEY, "true") || check_next_token(input, KEY, "false")) {
                // Check for boolean literal
                Token* bool_token = TokenQueue_remove(input);
                operand = LiteralNode_new_bool(token_str_eq(bool_token->text, "true"), bool_token->line);
                Token_free(bool_token);
            } else if (check_next_token_type(input, DECLIT)) {
                // Check for decimal integer literal
                Token* int_token = TokenQueue_remove(input);
                operand = LiteralNode_new_int(atoi(int_token->text), int_token->line);
                Token_free(int_token);
            } else if (check_next_token_type(input, HEXLIT)) {
                // Check for hexadecimal integer literal
                Token* hex_token = TokenQueue_remove(input);
                operand = LiteralNode_new_int((int)strtol(hex_token->text, NULL, 16), hex_token->line);
                Token_free(hex_token);
            } else if (check_next_token_type(input, STRLIT)) {
                // Check for string literal
                Token* str_token = TokenQueue_remove(input);
                size_t len = strlen(str_token->text);
                // Remove surrounding quotes
                char unquoted[MAX_TOKEN_LEN];
                if (len >= 2 && str_token->text[0] == '"' && str_token->text[len - 1] == '"') {
                    snprintf(unquoted, sizeof(unquoted), "%.*s", (int)(len - 2), str_token->text + 1);
                } else {
                    snprintf(unquoted, sizeof(unquoted), "%s", str_token->text);
                }
                operand = LiteralNode_new_string(unquoted, str_token->line);
                Token_free(str_token);
            }    
        return operand;
    }
}