/**
 * expr_parser.c
 *
 * CS 432 (Compilers), James Madison University
 * Mike Lam, Fall 2021
 *
 * Parser for simple math expressions.
 *
 * Here is the grammar:
 *
 *      S  ::=  E
 *
 *      E  ::=  E + T
 *           |  T
 *
 *      T  ::=  T * F
 *           |  F
 *
 *      F  ::=  ( E )
 *           |  {DEC}
 *
 *  Where {DEC} is a decimal literal: [0-9]+
 *
 *  Note that the E and T non-terminals are left-recursive, and so special
 *  accomodations must be made to parse them using recursive descent. Here are
 *  the standard LL(1) transformations:
 *
 *      E  ::=  T E'
 *      E' ::=  + T E'
 *           |  e
 *
 *      T  ::=  F T'
 *      T' ::=  * F T'
 *           |  e
 *
 *  Where 'e' is the empty string. Such constructs are handled in this
 *  implementation using a loop structure inside the main non-terminal handler
 *  (i.e., the parsing of multiple E' non-terminals is handled by a loop inside
 *  the parser for E). This allows us to preserve a clean, left-associative
 *  parse tree without requiring left recursion.
 *
 *  Exercises for the reader (in order of increasing complexity):
 *
 *      1) Add support for '-' and '/' operations, with their normal arithmetic
 *      association and precedence.
 *
 *      2) Add support for a '^' operator for exponentiation. It should be
 *      right-associative, and should have higher precedence than the other
 *      arithmetic operators.
 *
 *      3) Add supprt for a unary postfix operator '!' to represent the
 *      factorial operation. This operation should have higher precedence than
 *      any binary operator.
 *
 *      4) Add support for relational operators: '<', '>', '<=', '>=', '==',
 *      '!='. They should all have lower precedence than arithmetic, and the two
 *      equality operators should have lower precedence than the other four.
 *
 */

#include "expr_lexer.c"


/**
 * Debug output
 */

//#define DEBUG

#ifdef DEBUG
    int debug_level = 0;
#   define DEBUG_INDENT for (int _dl=0; _dl < debug_level; _dl++) { printf("  "); }
#   define DEBUG_BEGIN(PTYPE) \
        DEBUG_INDENT \
        printf("Debug: Parsing " #PTYPE " at token \"%s\"\n", TokenQueue_peek(input)->text); \
        debug_level++;
#   define DEBUG_END \
        debug_level--;
#else
#   define DEBUG_BEGIN(PTYPE)
#   define DEBUG_END
#endif


/**
 * Binary parse nodes
 */

typedef struct parse_node
{
    char text[MAX_STR_LEN];
    struct parse_node *left;
    struct parse_node *right;

} ParseNode;

ParseNode* ParseNode_new(const char* text)
{
    ParseNode* node = (ParseNode*)malloc(sizeof(ParseNode));
    if (node == NULL) {
        printf("Error: Could not allocate node!\n");
        exit(EXIT_FAILURE);
    }
    snprintf(node->text, MAX_STR_LEN, "%s", text);
    node->left = NULL;
    node->right = NULL;
    return node;
}

void ParseNode_free(ParseNode* node)
{
    if (node->left != NULL) {
        ParseNode_free(node->left);
    }
    if (node->right != NULL) {
        ParseNode_free(node->right);
    }
    free(node);
}

void ParseNode_print(ParseNode* node, FILE* out)
{
    if (node->left != NULL) {
        fprintf(out, "(");
        ParseNode_print(node->left, out);
    }
    fprintf(out, "%s", node->text);
    if (node->right != NULL) {
        ParseNode_print(node->right, out);
        fprintf(out, ")");
    }
}


/**
 * Helper routines
 */

bool is_next_token_dec(TokenQueue* input)
{
    return !TokenQueue_is_empty(input) &&
        TokenQueue_peek(input)->type == DEC;
}

bool is_next_token(TokenQueue* input, const char* text)
{
    return !TokenQueue_is_empty(input) &&
        TokenQueue_peek(input)->type == SYM &&
        strncmp(TokenQueue_peek(input)->text, text, MAX_STR_LEN) == 0;
}

void match_and_remove_token(TokenQueue* input, const char* text)
{
    if (TokenQueue_is_empty(input)) {
        printf("Error: Expected \"%s\" but encountered end of input\n", text);
        exit(EXIT_FAILURE);
    }
    Token* t = TokenQueue_remove(input);
    if (strncmp(t->text, text, MAX_STR_LEN) != 0) {
        printf("Error: Expected \"%s\" but encountered \"%s\"\n", text, t->text);
        exit(EXIT_FAILURE);
    }
    Token_free(t);
}


/**
 * Parsing routines
 */

/* this prototype is required because of mutual recursion */
ParseNode* parse_expr(TokenQueue* input);

ParseNode* parse_factor(TokenQueue* input)
{
    DEBUG_BEGIN(factor)

    ParseNode* node = NULL;

    /* use lookahead to determine which production to parse */
    if (is_next_token_dec(input)) {

        /*
         * F -> {DEC}
         */
        Token* t = TokenQueue_remove(input);
        node = ParseNode_new(t->text);
        Token_free(t);

    } else if (is_next_token(input, "(")) {

        /*
         * F -> ( E )
         */
        match_and_remove_token(input, "(");
        node = parse_expr(input);
        match_and_remove_token(input, ")");

    } else {
        printf("Error: Expected decimal literal or subexpression but encountered \"%s\"\n",
                TokenQueue_peek(input)->text);
        exit(EXIT_FAILURE);
    }

    DEBUG_END
    return node;
}

ParseNode* parse_term(TokenQueue* input)
{
    DEBUG_BEGIN(term)

    /*
     * T -> F T'
     */
    ParseNode* root = parse_factor(input);

    /*
     * T' -> * F T'
     *     | e
     */
    while (is_next_token(input, "*")) {
        ParseNode* new_root = ParseNode_new("*");
        new_root->left = root;
        match_and_remove_token(input, "*");
        new_root->right = parse_factor(input);
        root = new_root;
    }

    DEBUG_END
    return root;
}

ParseNode* parse_expr(TokenQueue* input)
{
    DEBUG_BEGIN(expr)

    /*
     * E -> T E'
     */
    ParseNode* root = parse_term(input);

    /*
     * E' -> + T E'
     *     | e
     */
    while (is_next_token(input, "+")) {
        ParseNode* new_root = ParseNode_new("+");
        new_root->left = root;
        match_and_remove_token(input, "+");
        new_root->right = parse_term(input);
        root = new_root;
    }

    DEBUG_END
    return root;
}

ParseNode* parse(TokenQueue* input)
{
    /* parse top-level expression */
    ParseNode* root = parse_expr(input);

    /* check for extra input */
    if (!TokenQueue_is_empty(input)) {
        Token* t = TokenQueue_remove(input);
        printf("Error: Extraneous input: \"%s\"\n", t->text);
        exit(EXIT_FAILURE);
    }

    /* clean up */
    TokenQueue_free(input);
    return root;
}


/**
 * Application entry point.
 */
int main(int argc, char* argv[])
{
    /* check for command-line argument */
    if (argc != 2) {
        printf("Usage: %s <text>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* parse and print result */
    ParseNode* tree = parse(lex(argv[1]));
    ParseNode_print(tree, stdout);
    printf("\n");

    /* clean up */
    ParseNode_free(tree);
    return EXIT_SUCCESS;
}

