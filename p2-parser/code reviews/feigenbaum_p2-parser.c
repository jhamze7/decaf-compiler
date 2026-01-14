/**
 * @file p2-parser.c
 * @brief Compiler phase 2: parser
 * @authors Ray Steen & Mitchell Feigenbaum
 * AI Use statement: We used AI in this project. Primarily for Binary Expression Parsing.
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
int get_next_token_line(TokenQueue *input)
{
  if (TokenQueue_is_empty(input))
  {
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
void match_and_discard_next_token(TokenQueue *input, TokenType type, const char *text)
{
  if (TokenQueue_is_empty(input))
  {
    Error_throw_printf("Unexpected end of input (expected \'%s\')\n", text);
  }
  Token *token = TokenQueue_remove(input);
  if (token->type != type || !token_str_eq(token->text, text))
  {
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
void discard_next_token(TokenQueue *input)
{
  if (TokenQueue_is_empty(input))
  {
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
bool check_next_token_type(TokenQueue *input, TokenType type)
{
  if (TokenQueue_is_empty(input))
  {
    return false;
  }
  Token *token = TokenQueue_peek(input);
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
bool check_next_token(TokenQueue *input, TokenType type, const char *text)
{
  if (TokenQueue_is_empty(input))
  {
    return false;
  }
  Token *token = TokenQueue_peek(input);
  return (token->type == type) && (token_str_eq(token->text, text));
}

/**
 * @brief Parse and return a Decaf type
 *
 * @param input Token queue to modify
 * @returns Parsed type (it is also removed from the queue)
 */
DecafType parse_type(TokenQueue *input)
{
  if (TokenQueue_is_empty(input))
  {
    Error_throw_printf("Unexpected end of input (expected type)\n");
  }
  Token *token = TokenQueue_remove(input);
  if (token->type != KEY)
  {
    Error_throw_printf("Invalid type '%s' on line %d\n", token->text, get_next_token_line(input));
  }
  DecafType t = VOID;
  if (token_str_eq("int", token->text))
  {
    t = INT;
  }
  else if (token_str_eq("bool", token->text))
  {
    t = BOOL;
  }
  else if (token_str_eq("void", token->text))
  {
    t = VOID;
  }
  else
  {
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
void parse_id(TokenQueue *input, char *buffer)
{
  if (TokenQueue_is_empty(input))
  {
    Error_throw_printf("Unexpected end of input (expected identifier)\n");
  }
  Token *token = TokenQueue_remove(input);
  if (token->type != ID)
  {
    Error_throw_printf("Invalid ID '%s' on line %d\n", token->text, get_next_token_line(input));
  }
  snprintf(buffer, MAX_ID_LEN, "%s", token->text);
  Token_free(token);
}

// Map operator lexeme to precedence (higher number = higher precedence)
// * / % : 7
// + -   : 6
// < <= >= > : 5
// == != : 4
// &&    : 3
// ||    : 2
int get_binop_precedence(const char *op)
{
  if (strcmp(op, "*") == 0 || strcmp(op, "/") == 0 || strcmp(op, "%") == 0)
    return 7;
  if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0)
    return 6;
  if (strcmp(op, "<") == 0 || strcmp(op, "<=") == 0 ||
      strcmp(op, ">=") == 0 || strcmp(op, ">") == 0)
    return 5;
  if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0)
    return 4;
  if (strcmp(op, "&&") == 0)
    return 3;
  if (strcmp(op, "||") == 0)
    return 2;
  return -1;
}

// Map operator lexeme to BinOp enum (adjust names if your enum differs)
BinaryOpType get_binop_kind(const char *op)
{
  if (strcmp(op, "*") == 0)
    return MULOP;
  if (strcmp(op, "/") == 0)
    return DIVOP;
  if (strcmp(op, "%") == 0)
    return MODOP;
  if (strcmp(op, "+") == 0)
    return ADDOP;
  if (strcmp(op, "-") == 0)
    return SUBOP;
  if (strcmp(op, "<") == 0)
    return LTOP;
  if (strcmp(op, "<=") == 0)
    return LEOP;
  if (strcmp(op, ">") == 0)
    return GTOP;
  if (strcmp(op, ">=") == 0)
    return GEOP;
  if (strcmp(op, "==") == 0)
    return EQOP;
  if (strcmp(op, "!=") == 0)
    return NEQOP;
  if (strcmp(op, "&&") == 0)
    return ANDOP;
  if (strcmp(op, "||") == 0)
    return OROP;
  // Fallback (should not happen if checked first)
  return ADDOP;
}

// helper function to check if token is next variable is a type
bool is_type(TokenQueue *input)
{
  Token *curr = NULL;
  return (curr = TokenQueue_peek(input))->type == KEY && (token_str_eq(curr->text, "int") || token_str_eq(curr->text, "bool") || token_str_eq(curr->text, "void"));
}

int parse_number(TokenQueue *input, TokenType expected_type)
{
  int base = (expected_type == DECLIT) ? 10 : 16;
  int line = get_next_token_line(input);
  Token *token = TokenQueue_remove(input);
  if (token == NULL || token->type != expected_type)
  {
    Token_free(token);
    Error_throw_printf("Error on line %d: Expected %s literal\n", line, base == 10 ? "decimal" : "hexadecimal");
  }
  int dec = strtol(token->text, NULL, base);
  Token_free(token);
  return dec;
}
ASTNode *parse_literal(TokenQueue *input)
{
  if (TokenQueue_is_empty(input))
  {
    Error_throw_printf("Unexpected end of input (expected literal)\n");
  }
  int source_line = get_next_token_line(input);

  switch (TokenQueue_peek(input)->type)
  {
  case DECLIT:
    return LiteralNode_new_int(parse_number(input, DECLIT), source_line);
  case HEXLIT:
    return LiteralNode_new_int(parse_number(input, HEXLIT), source_line);
  case STRLIT:
  {
    Token *tok = TokenQueue_remove(input);
    // Remove the first and last character (quotes)
    size_t len = strlen(tok->text);
    char *str = malloc(len - 1); // len-2 for content, +1 for null terminator
    if (str == NULL)
    {
      Token_free(tok);
      Error_throw_printf("Memory allocation failed for string literal\n");
    }
    strncpy(str, tok->text + 1, len - 2); // Copy content without quotes
    str[len - 2] = '\0';                  // Null-terminate the string
    // Replace all escaped characters with their actual values
    size_t i = 0, j = 0;
    while (i < len - 2)
    {
      if (str[i] == '\\')
      {
        i++;
        switch (str[i])
        {
        case 'n':
          str[j++] = '\n';
          break;
        case 't':
          str[j++] = '\t';
          break;
        case 'r':
          str[j++] = '\r';
          break;
        case '\\':
          str[j++] = '\\';
          break;
        case '"':
          str[j++] = '"';
          break;
        case '\'':
          str[j++] = '\'';
          break;
        default:
          str[j++] = str[i];
          break;
        }
        i++;
      }
      else
      {
        str[j++] = str[i++];
      }
    }
    str[j] = '\0';

    ASTNode *node = LiteralNode_new_string(str, source_line); // This will copy the string so it is safe to free
    free(str);
    Token_free(tok);
    return node;
  }
  case KEY:
    if (check_next_token(input, KEY, "true"))
    {
      TokenQueue_remove(input);
      return LiteralNode_new_bool(true, source_line);
    }
    else if (check_next_token(input, KEY, "false"))
    {
      TokenQueue_remove(input);
      return LiteralNode_new_bool(false, source_line);
    }
    else
    {
      Error_throw_printf("Error on line %d: unexpected keyword '%s' in expression\n", source_line, TokenQueue_peek(input)->text);
    }
    break;
  default:
    Error_throw_printf("Error on line %d: unexpected token '%s' in expression\n", source_line, TokenQueue_peek(input)->text);
  }
  return NULL; // should never reach here
}
ASTNode *parse_loc_or_func_call(TokenQueue *input);
ASTNode *parse_expr(TokenQueue *input);
ASTNode *parse_base_expr(TokenQueue *input)
{
  if (TokenQueue_is_empty(input))
  {
    Error_throw_printf("Unexpected end of input (expected expression)\n");
  }
  if (check_next_token_type(input, ID))
  {
    return parse_loc_or_func_call(input);
  }
  else if (check_next_token(input, SYM, "("))
  {
    discard_next_token(input); // discard the (
    ASTNode *expr = parse_expr(input);
    match_and_discard_next_token(input, SYM, ")");
    return expr;
  }
  else
  {
    return parse_literal(input);
  }
  return NULL; // should never reach here
}

ASTNode *parse_unary_expr(TokenQueue *input)
{
  int source_line = get_next_token_line(input);
  if (check_next_token_type(input, SYM))
  {
    if (check_next_token(input, SYM, "-"))
    {
      discard_next_token(input);
      ASTNode *child = parse_base_expr(input);
      return UnaryOpNode_new(NEGOP, child, source_line);
    }
    else if (check_next_token(input, SYM, "!"))
    {
      discard_next_token(input);
      ASTNode *child = parse_base_expr(input);
      return UnaryOpNode_new(NOTOP, child, source_line);
    }
    else
    {
      return parse_base_expr(input);
    }
  }
  else
  {
    return parse_base_expr(input);
  }
  return NULL; // should never reach here
}
ASTNode *parse_bin_expr_prec(TokenQueue *input, int min_prec)
{
  ASTNode *lhs = parse_unary_expr(input);

  while (!TokenQueue_is_empty(input) && check_next_token_type(input, SYM))
  {
    Token *tok = TokenQueue_peek(input);
    int prec = get_binop_precedence(tok->text);
    if (prec < min_prec)
      break;
    int op_line = tok->line;
    // save operator info before consuming
    char op_lex[MAX_TOKEN_LEN];
    snprintf(op_lex, sizeof(op_lex), "%s", tok->text);
    // consume operator
    discard_next_token(input);
    // parse RHS (at next higher precedence level for right-binding)
    ASTNode *rhs = parse_unary_expr(input);

    // Handle any operators of higher precedence that bind to rhs
    while (!TokenQueue_is_empty(input) && check_next_token_type(input, SYM))
    {
      Token *next = TokenQueue_peek(input);
      int next_prec = get_binop_precedence(next->text);
      if (next_prec < prec)
      {
        rhs = parse_bin_expr_prec(input, next_prec);
      }
      else
      {
        break;
      }
    }

    lhs = BinaryOpNode_new(get_binop_kind(op_lex), lhs, rhs, op_line);
  }

  return lhs;
}

ASTNode *parse_bin_expr(TokenQueue *input)
{
  if (TokenQueue_is_empty(input))
  {
    Error_throw_printf("Unexpected end of input (expected expression)\n");
  }

  // BinExpr ::= UnaryExpr BinExprRest
  ASTNode *lhs = parse_unary_expr(input);

  // BinExprRest ::= (BinOp UnaryExpr)*  (left associative chain)
  while (!TokenQueue_is_empty(input) && check_next_token_type(input, SYM))
  {
    Token *tok = TokenQueue_peek(input);
    int prec = get_binop_precedence(tok->text);
    if (prec < 0)
    {
      break; // not a binary operator
    }

    // Save operator info before consuming (discard frees the token)
    int op_line = tok->line;
    char op_lex[MAX_TOKEN_LEN];
    snprintf(op_lex, sizeof(op_lex), "%s", tok->text);

    discard_next_token(input); // consume operator
    ASTNode *rhs = parse_unary_expr(input);
    lhs = BinaryOpNode_new(get_binop_kind(op_lex), lhs, rhs, op_line);
  }

  return lhs;
}
ASTNode *parse_expr(TokenQueue *input)
{
  // TODO: Implement expression parsing
  /*
  Expressions can be:
  BinaryOp (contains two child expressions)
  UnaryOp (contains one child expression)
  Location
  FuncCall (contains a list of expressions)
  Literal
  */
  /*
  Original Grammar:
      Expr      ::= BinExpr
      BinExpr   ::= BinExpr BinOp BinExpr
                  | UnaryExpr
      UnaryExpr ::= UnaryOp BaseExpr
                  | BaseExpr
      BaseExpr  ::= '(' Expr ')'
                  | Location
                  | FuncCall
                  | Literal
  */
  /*
  Grammar that is LL(1) compliant:

  Expr        ::= BinExpr
  BinExpr     ::= UnaryExpr BinExprRest
  BinExprRest ::= BinOp UnaryExpr BinExprRest
                | e
  UnaryExpr   ::= UnaryOp BaseExpr
                | BaseExpr
  BaseExpr    ::= '(' Expr ')'
                | Location
                | FuncCall
                | Literal

  BinExpr can be simplified to BinExpr ::= UnaryExpr (BinOp UnaryExpr)*
  */
  return parse_bin_expr(input);
}

ASTNode *parse_vardecl(TokenQueue *input)
{
  // part 1
  DecafType type;
  char name[MAX_ID_LEN];
  int source_line = get_next_token_line(input);
  // parse type of var
  type = parse_type(input);
  // parse name of var
  parse_id(input, name);
  bool is_array = false;
  int array_length = 1;
  if (TokenQueue_is_empty(input))
  {
    Error_throw_printf("Error on line %d: variable declaration does not end with a semicolon.\n", source_line);
  }
  if (token_str_eq(TokenQueue_peek(input)->text, "["))
  { // is an array
    is_array = true;
    match_and_discard_next_token(input, SYM, "[");
    array_length = parse_number(input, DECLIT);
    match_and_discard_next_token(input, SYM, "]");
  }
  // end the var decleration with a semicolon
  match_and_discard_next_token(input, SYM, ";");
  return VarDeclNode_new(name, type, is_array, array_length, source_line);
}

ParameterList *parse_params(TokenQueue *input)
{
  // TODO: Implement Parameter parsing
  ParameterList *params = ParameterList_new();
  if (token_str_eq(TokenQueue_peek(input)->text, ")"))
  {
    // no parameters given, just return an empty parameterlist
    return params;
  }
  // otherwise, parse it like in the spec
  DecafType type;
  char name[MAX_ID_LEN];
  // get first parameter
  type = parse_type(input);
  parse_id(input, name);
  ParameterList_add_new(params, name, type);

  // loop through parameters until we hit a )
  // decaf spec does not allow trailing commas
  while (check_next_token(input, SYM, ","))
  {
    match_and_discard_next_token(input, SYM, ",");
    type = parse_type(input);
    parse_id(input, name);
    ParameterList_add_new(params, name, type);
  }

  return params;
}
ASTNode *parse_block(TokenQueue *);

NodeList *parse_args(TokenQueue *input)
{
  // otherwise, parse it like in the spec
  NodeList *args = NodeList_new();
  // get first argument
  ASTNode *expr = parse_expr(input);
  NodeList_add(args, expr);

  // loop through arguments until we hit not a ,
  while (check_next_token(input, SYM, ","))
  {
    discard_next_token(input);
    ASTNode *expr = parse_expr(input);
    NodeList_add(args, expr);
  }

  return args;
}

ASTNode *parse_loc_or_func_call(TokenQueue *input)
{
  if (TokenQueue_is_empty(input))
  {
    Error_throw_printf("Unexpected end of input (expected location or function call)\n");
  }
  char name[MAX_ID_LEN];
  int source_line = get_next_token_line(input);
  parse_id(input, name);
  if (check_next_token(input, SYM, "("))
  {
    // parse func call
    discard_next_token(input); // discard the (
    if (check_next_token(input, SYM, ")"))
    {
      // no arguments
      match_and_discard_next_token(input, SYM, ")");
      return FuncCallNode_new(name, NodeList_new(), source_line);
    }
    NodeList *args = parse_args(input);
    match_and_discard_next_token(input, SYM, ")");
    return FuncCallNode_new(name, args, source_line);
  }
  else
  {
    // parse location
    ASTNode *index = NULL;
    if (check_next_token(input, SYM, "["))
    {
      discard_next_token(input); // discard the [
      index = parse_expr(input);
      match_and_discard_next_token(input, SYM, "]");
    }
    return LocationNode_new(name, index, source_line);
  }
  return NULL; // should never reach here
}
ASTNode *parse_stmt(TokenQueue *input)
{
  // TODO: implement parse_stmt
  /*
  Statements can be:
  Assignment (contains a Location and an expression)
  Conditional (contains an expression and either one or two Blocks)
  WhileLoop (contains an expression and a Block)
  Return (contains an optional expression)
  Break
  Continue
  */
  int source_line = get_next_token_line(input);
  // check if keyword or identifier
  if (check_next_token_type(input, KEY))
  {
    if (check_next_token(input, KEY, "if"))
    {
      // parse conditional
      discard_next_token(input);
      match_and_discard_next_token(input, SYM, "(");
      ASTNode *condition = parse_expr(input);
      match_and_discard_next_token(input, SYM, ")");
      ASTNode *if_block = parse_block(input);
      ASTNode *else_block = NULL;
      if (check_next_token(input, KEY, "else"))
      {
        discard_next_token(input);
        else_block = parse_block(input);
      }
      return ConditionalNode_new(condition, if_block, else_block, source_line);
    }
    else if (check_next_token(input, KEY, "while"))
    {
      // parse while loop
      discard_next_token(input);
      match_and_discard_next_token(input, SYM, "(");
      ASTNode *condition = parse_expr(input);
      match_and_discard_next_token(input, SYM, ")");
      ASTNode *while_block = parse_block(input);
      return WhileLoopNode_new(condition, while_block, source_line);
    }
    else if (check_next_token(input, KEY, "return"))
    {
      // parse return
      discard_next_token(input);
      if (check_next_token(input, SYM, ";"))
      {
        // return without value
        discard_next_token(input);
        return ReturnNode_new(NULL, source_line);
      }
      else
      {
        // return with value
        // TODO: Uncomment when parse_expr is implemented
        ASTNode *expr = parse_expr(input);
        match_and_discard_next_token(input, SYM, ";");
        return ReturnNode_new(expr, source_line);
      }
    }
    else if (check_next_token(input, KEY, "break"))
    {
      // parse break
      ASTNode *break_node = BreakNode_new(source_line);
      discard_next_token(input);
      match_and_discard_next_token(input, SYM, ";");
      return break_node;
    }
    else if (check_next_token(input, KEY, "continue"))
    {
      // parse continue
      ASTNode *continue_node = ContinueNode_new(source_line);
      discard_next_token(input);
      match_and_discard_next_token(input, SYM, ";");
      return continue_node;
    }
    else
    {
      Error_throw_printf("Error on line %d: Invalid statement\n", get_next_token_line(input));
    }
  }
  else if (check_next_token_type(input, ID))
  {
    ASTNode *location_or_func_call = parse_loc_or_func_call(input);
    if (location_or_func_call->type == FUNCCALL)
    {
      return location_or_func_call;
    }
    else if (location_or_func_call->type == LOCATION)
    {
      // parse assignment
      ASTNode *location = location_or_func_call;
      match_and_discard_next_token(input, SYM, "=");
      ASTNode *expr = parse_expr(input);
      match_and_discard_next_token(input, SYM, ";");
      return AssignmentNode_new(location, expr, source_line);
    }
    else
    {
      Error_throw_printf("Error on line %d: Invalid statement\n", get_next_token_line(input));
    }
  }
  else
  {
    Error_throw_printf("Error on line %d: Invalid statement\n", get_next_token_line(input));
  }
  // Placeholder until this is implemented
  return NULL;
}

ASTNode *parse_block(TokenQueue *input)
{
  // start the block
  int source_line = get_next_token_line(input);
  match_and_discard_next_token(input, SYM, "{");

  NodeList *vars = NodeList_new();
  NodeList *stmts = NodeList_new();

  // first we parse for 0 or more vardeclerations. we find this
  // by checking if the input node is a type keyword, then use
  // the parse_vardecl function. We then parse statements until
  // we hit a right curly bracket

  // parse for variable declarations
  while (!TokenQueue_is_empty(input) && is_type(input))
  {
    NodeList_add(vars, parse_vardecl(input));
  }

  // if tokenqueue ends before }, invalid program
  if (TokenQueue_is_empty(input))
  {
    NodeList_free(vars);
    NodeList_free(stmts);
    return NULL;
  }

  // parse for statements
  ASTNode *stmt = NULL;
  while (!TokenQueue_is_empty(input) && !check_next_token(input, SYM, "}"))
  {
    stmt = parse_stmt(input);
    if (stmt == NULL)
    {
      NodeList_free(vars);
      NodeList_free(stmts);
      return NULL;
    }
    NodeList_add(stmts, stmt);
  }

  // end the block
  match_and_discard_next_token(input, SYM, "}");
  return BlockNode_new(vars, stmts, source_line);
}

ASTNode *parse_funcdecl(TokenQueue *input)
{
  DecafType type;
  char name[MAX_ID_LEN];
  int source_line = get_next_token_line(input);
  // first token is def, so lets get rid of it
  match_and_discard_next_token(input, KEY, "def");
  // get type of function
  type = parse_type(input);
  // get name of function
  parse_id(input, name);
  // next char should be open parenthesis
  match_and_discard_next_token(input, SYM, "(");
  // TODO: handle params
  ParameterList *parameters = parse_params(input);
  if (parameters == NULL)
  {
    Error_throw_printf("Error on line %d: Function parameters not properly formatted\n", source_line);
  }
  // parse_params parses until it finds the final right
  // parenthesis, signalling for the parser to finish parsing
  // the params.
  match_and_discard_next_token(input, SYM, ")");
  ASTNode *body = parse_block(input);
  if (body == NULL)
  {
    ParameterList_free(parameters);
    Error_throw_printf("Error on line %d: Function block not properly formatted\n", source_line);
  }
  // we are done parsing the function, return the funcdeclnode
  return FuncDeclNode_new(name, type, parameters, body, source_line);
}
/*
 * node-level parsing functions
 */

ASTNode *parse_program(TokenQueue *input)
{
  NodeList *vars = NodeList_new();
  NodeList *funcs = NodeList_new();
  Token *currToken = NULL;
  while (!TokenQueue_is_empty(input))
  {
    // choose whether to parse a vardecl or funcdecl
    // if it starts with def, do parse_funcdecl, else
    // parse vardecl

    // store current token in a temp variable
    currToken = TokenQueue_peek(input);

    // check if it should be parsed as a function declaration
    if (currToken->type == KEY && token_str_eq(currToken->text, "def"))
    {
      NodeList_add(funcs, parse_funcdecl(input));

      // try to parse it as a variable
    }
    else if (is_type(input))
    {
      NodeList_add(vars, parse_vardecl(input));
    }
    else
    {
      // Parse error, top level is not a keyword
      Error_throw_printf("Error: %s is not a valid function or variable decleration.\n", currToken->text);
    }
  }

  return ProgramNode_new(vars, funcs);
}

ASTNode *parse(TokenQueue *input)
{
  if (input == NULL)
  {
    Error_throw_printf("Error: Null TokenQueue.\n");
  }
  return parse_program(input);
}
