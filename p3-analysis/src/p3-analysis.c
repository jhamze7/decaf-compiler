/**
 * Julian Hamze
 * 
 * @file p3-analysis.c
 * @brief Compiler phase 3: static analysis
 * 
 * I used AI to ask design questions, and assist with the duplicate var checker
 */
#include "p3-analysis.h"

/**
 * @brief State/data for static analysis visitor
 */
typedef struct AnalysisData
{
    /**
     * @brief List of errors detected
     */
    ErrorList* errors;
 
    /* BOILERPLATE: TODO: add any new desired state information (and clean it up in AnalysisData_free) */
 
    /* TODO: add something to get the current function's return type */
    DecafType current_return_type;
    int loop_depth;
 
} AnalysisData;

/**
 * @brief Allocate memory for analysis data
 * 
 * @returns Pointer to allocated structure
 */
AnalysisData* AnalysisData_new (void)
{
    AnalysisData* data = (AnalysisData*)calloc(1, sizeof(AnalysisData));
    CHECK_MALLOC_PTR(data);
    data->errors = ErrorList_new();
    data->current_return_type = VOID;
    data->loop_depth = 0;
    return data;
}

/**
 * @brief Deallocate memory for analysis data
 * 
 * @param data Pointer to the structure to be deallocated
 */
void AnalysisData_free (AnalysisData* data)
{
    /* free everything in data that is allocated on the heap except the error
     * list; it needs to be returned after the analysis is complete */

    /* free "data" itself */
    free(data);
}

/**
 * @brief Macro for more convenient access to the data inside a @ref AnalysisVisitor
 * data structure
 */
#define DATA ((AnalysisData*)visitor->data)

/**
 * @brief Macro for more convenient access to the error list inside a
 * @ref AnalysisVisitor data structure
 */
#define ERROR_LIST (((AnalysisData*)visitor->data)->errors)

/**
 * @brief Wrapper for @ref lookup_symbol that reports an error if the symbol isn't found
 * 
 * @param visitor Visitor with the error list for reporting
 * @param node AST node to begin the search at
 * @param name Name of symbol to find
 * @returns The @ref Symbol if found, otherwise @c NULL
 */
Symbol* lookup_symbol_with_reporting(NodeVisitor* visitor, ASTNode* node, const char* name)
{
    Symbol* symbol = lookup_symbol(node, name);
    if (symbol == NULL) {
        ErrorList_printf(ERROR_LIST, "Symbol '%s' undefined on line %d", name, node->source_line);
    }
    return symbol;
}

/**
 * @brief Macro for shorter storing of the inferred @c type attribute
 */
#define SET_INFERRED_TYPE(T) ASTNode_set_printable_attribute(node, "type", (void*)(T), \
                                 type_attr_print, dummy_free)

/**
 * @brief Macro for shorter retrieval of the inferred @c type attribute
 */
#define GET_INFERRED_TYPE(N) (DecafType)(long)ASTNode_get_attribute(N, "type")

void AnalysisVisitor_infer_literal (NodeVisitor* visitor, ASTNode* node)
{
    SET_INFERRED_TYPE(node->literal.type);
}
 
/* TODO: infer types of locations (this will require a symbol lookup) */

void AnalysisVisitor_infer_location (NodeVisitor* visitor, ASTNode* node)
{
    Symbol* s = lookup_symbol_with_reporting(visitor, node, node->location.name);
    if (!s) return;

    DecafType t = s->type;

    if (node->location.index) {
        // index must be int
        if (ASTNode_has_attribute(node->location.index, "type")) {
            DecafType idx_t = GET_INFERRED_TYPE(node->location.index);
            if (idx_t != INT) {
                ErrorList_printf(ERROR_LIST,
                    "Array index must be int (found %s) on line %d",
                    DecafType_to_string(idx_t), node->source_line);
            }
        }
    }

    SET_INFERRED_TYPE(t);
}

 
void AnalysisVisitor_check_binaryop (NodeVisitor* visitor, ASTNode* node)
{
    DecafType left_type = GET_INFERRED_TYPE(node->binaryop.left);
    DecafType right_type = GET_INFERRED_TYPE(node->binaryop.right);
 
    switch (node->binaryop.operator) {
 
        /* arithmetic and relational operators */
        case ADDOP: case SUBOP: case MULOP: case DIVOP: case MODOP:
            if (left_type != INT || right_type != INT) {
                ErrorList_printf(ERROR_LIST,
                    "Type error: %s requires integer operands (found %s and %s) on line %d",
                    BinaryOpToString(node->binaryop.operator),
                    DecafType_to_string(left_type),
                    DecafType_to_string(right_type),
                    node->source_line);
            }
            SET_INFERRED_TYPE(INT);
            break;

        // relational, result is BOOL
        case LTOP: case LEOP: case GEOP: case GTOP:
            if (left_type != INT || right_type != INT) {
                ErrorList_printf(ERROR_LIST,
                    "Type error: %s requires integer operands (found %s and %s) on line %d",
                    BinaryOpToString(node->binaryop.operator),
                    DecafType_to_string(left_type),
                    DecafType_to_string(right_type),
                    node->source_line);
            }
            SET_INFERRED_TYPE(BOOL);
            break;

        // logical, result is BOOL
        case OROP: case ANDOP:
            if (left_type != BOOL || right_type != BOOL) {
                ErrorList_printf(ERROR_LIST,
                    "Type error: %s requires boolean operands (found %s and %s) on line %d",
                    BinaryOpToString(node->binaryop.operator),
                    DecafType_to_string(left_type),
                    DecafType_to_string(right_type),
                    node->source_line);
            }
            SET_INFERRED_TYPE(BOOL);
            break;

        // equality, result is BOOL
        case EQOP: case NEQOP:
            if (left_type != right_type) {
                ErrorList_printf(ERROR_LIST,
                    "Type error: %s requires matching operand types (found %s and %s) on line %d",
                    (node->binaryop.operator == EQOP ? "==" : "!="),
                    DecafType_to_string(left_type),
                    DecafType_to_string(right_type),
                    node->source_line);
            }
            SET_INFERRED_TYPE(BOOL);
            break;
    }
}

//Conditionals, Loops, Breaks, Continues

void AnalysisVisitor_check_conditional(NodeVisitor* visitor, ASTNode* node) {
    DecafType ct = GET_INFERRED_TYPE(node->conditional.condition);
    if (ct != BOOL) {
        ErrorList_printf(ERROR_LIST,
            "If-condition must be bool (found %s) on line %d",
            DecafType_to_string(ct), node->source_line);
    }
}

void AnalysisVisitor_enter_whileloop(NodeVisitor* visitor, ASTNode* node) {
    DATA->loop_depth++;
}

void AnalysisVisitor_exit_whileloop(NodeVisitor* visitor, ASTNode* node) {
    DecafType ct = GET_INFERRED_TYPE(node->whileloop.condition);
    if (ct != BOOL) {
        ErrorList_printf(ERROR_LIST,
            "While-condition must be bool (found %s) on line %d",
            DecafType_to_string(ct), node->source_line);
    }
    DATA->loop_depth--;
}

void AnalysisVisitor_check_break(NodeVisitor* visitor, ASTNode* node) {
    if (DATA->loop_depth <= 0) {
        ErrorList_printf(ERROR_LIST, "break outside of any loop on line %d", node->source_line);
    }
}

void AnalysisVisitor_check_continue(NodeVisitor* visitor, ASTNode* node) {
    if (DATA->loop_depth <= 0) {
        ErrorList_printf(ERROR_LIST, "continue outside of any loop on line %d", node->source_line);
    }
}

 
void AnalysisVisitor_check_assignment (NodeVisitor* visitor, ASTNode* node) {
    ASTNode* lhs = node->assignment.location;
    ASTNode* rhs = node->assignment.value;

    DecafType lhs_t = GET_INFERRED_TYPE(lhs);
    DecafType rhs_t = GET_INFERRED_TYPE(rhs);

    if (!ASTNode_has_attribute(lhs, "type") || !ASTNode_has_attribute(rhs, "type")) {
        return;
    }

    if (lhs_t != rhs_t) {
        const char* name = lhs->location.name;
        ErrorList_printf(ERROR_LIST,
            "Type mismatch: cannot assign %s to %s variable '%s' on line %d",
            DecafType_to_string(rhs_t),
            DecafType_to_string(lhs_t),
            name,
            node->source_line);
    }

    SET_INFERRED_TYPE(lhs_t);
}

void AnalysisVisitor_check_vardecl (NodeVisitor* visitor, ASTNode* node)
{
    if (node->vardecl.type == VOID){ //cannot have variables with type "void"
        ErrorList_printf(ERROR_LIST, "Void variable '%s' on line %d",
        node->vardecl.name, node->source_line);
    }
    if (node->vardecl.is_array && node->vardecl.array_length == 0) { //cannot have an array with size 0
        ErrorList_printf(ERROR_LIST, "Array '%s' cannot have size 0 (line %d)",
            node->vardecl.name, node->source_line);
    }

    if (strcmp(node->vardecl.name, "main") == 0) { //cannot have variables named "main"
        ErrorList_printf(ERROR_LIST,
            "Invalid variable name 'main' on line %d",
            node->source_line);
    }

    if (node->vardecl.is_array) {
        ASTNode* parent = node;

        while (parent != NULL && parent->type != FUNCDECL && parent->type != PROGRAM) {
            parent = (ASTNode*)ASTNode_get_attribute(parent, "parent");
        }

        if (parent && parent->type == FUNCDECL) { //arrays must be global
            ErrorList_printf(ERROR_LIST,
                "Array '%s' declared inside function '%s' (line %d): arrays must be global",
                node->vardecl.name,
                parent->funcdecl.name,
                node->source_line);
        }
    }
    
}

void AnalysisVisitor_check_location (NodeVisitor* visitor, ASTNode* node)
{
    lookup_symbol_with_reporting(visitor, node, node->location.name);
}

// helper to check for duplicate variables
static void AnalysisVisitor_check_duplicate_vars(NodeVisitor* visitor, ASTNode* node)
{
    if (node->type != BLOCK){
        return;
    }

    NodeList* vars = node->block.variables;
    for (ASTNode* a = (vars ? vars->head : NULL); a; a = a->next) {
        if (a->type != VARDECL) continue;
        for (ASTNode* b = a->next; b; b = b->next) {
            if (b->type != VARDECL) continue;
            if (strcmp(a->vardecl.name, b->vardecl.name) == 0) {
                ErrorList_printf(ERROR_LIST,
                    "Duplicate local variable '%s' (lines %d and %d)",
                    a->vardecl.name, a->source_line, b->source_line);
            }
        }
    }
}

void AnalysisVisitor_check_funcdecl(NodeVisitor* visitor, ASTNode* node){
    if (strcmp(node->funcdecl.name, "main") == 0) { //checks if the main function has a
        if (node->funcdecl.return_type != INT) {    //return type "int"
            ErrorList_printf(ERROR_LIST,
                "Main function must have return type int (found %s) on line %d",
                DecafType_to_string(node->funcdecl.return_type),
                node->source_line);
        }
    }
}


void AnalysisVisitor_check_funccall(NodeVisitor* visitor, ASTNode* node)
{
    Symbol* s = lookup_symbol_with_reporting(visitor, node, node->funccall.name);
    if (!s) { // already reported if undefined
        return;
    }

    // Must be a function symbol
    if (s->symbol_type != FUNCTION_SYMBOL) {
        ErrorList_printf(ERROR_LIST,
            "'%s' is not a function (line %d)",
            node->funccall.name, node->source_line);
        return;
    }

    // gets parameters and arguments
    Parameter* param = NULL;
    if (s->parameters) {
        param = s->parameters->head;
    }

    ASTNode* arg = NULL;
    if (node->funccall.arguments) {
        arg = node->funccall.arguments->head;
    }

    //compares types of args and params
    int index = 1;
    while (param && arg) {
        if (ASTNode_has_attribute(arg, "type")) {
            DecafType arg_t = GET_INFERRED_TYPE(arg);
            if (arg_t != param->type) {
                ErrorList_printf(ERROR_LIST,
                    "Type mismatch in call to '%s': parameter %d expects %s but got %s (line %d)",
                    node->funccall.name, index,
                    DecafType_to_string(param->type),
                    DecafType_to_string(arg_t),
                    node->source_line);
            }
        }
        param = param->next;
        arg = arg->next;
        index++;
    }

    // Check number of arguments vs params
    if (param || arg) {
        int expected = 0, given = 0;
        for (Parameter* p = s->parameters ? s->parameters->head : NULL; p; p = p->next){
            expected++;
        }
        for (ASTNode* a = node->funccall.arguments ? node->funccall.arguments->head : NULL; a; a = a->next){
            given++;
        }

        ErrorList_printf(ERROR_LIST,
            "Function '%s' expected %d arguments but was given %d (line %d)",
            node->funccall.name, expected, given, node->source_line);
    }

    // Set inferred return type
    SET_INFERRED_TYPE(s->type);
}



void AnalysisVisitor_program (NodeVisitor* visitor, ASTNode* node)
{
    if (lookup_symbol(node, "main") == NULL){ //throws error if no main method found
        ErrorList_printf(ERROR_LIST, "No main function found");
    }
}

/* TODO: type check more AST node types */
 
ErrorList* analyze (ASTNode* tree)
{
    if (tree == NULL) {
        ErrorList* errors = ErrorList_new();
        ErrorList_printf(errors, "No AST provided (null tree)");
        return errors;
    }

    /* allocate analysis structures */
    NodeVisitor* v = NodeVisitor_new();
    v->data = (void*)AnalysisData_new();
    v->dtor = (Destructor)AnalysisData_free;
 
    /* BOILERPLATE: TODO: register analysis callbacks */
    v->previsit_literal = AnalysisVisitor_infer_literal;
    v->postvisit_binaryop = AnalysisVisitor_check_binaryop;
    v->postvisit_vardecl = AnalysisVisitor_check_vardecl;
    v->postvisit_location = AnalysisVisitor_infer_location;
    v->postvisit_program = AnalysisVisitor_program;
    v->postvisit_conditional   = AnalysisVisitor_check_conditional;
    v->previsit_whileloop      = AnalysisVisitor_enter_whileloop;
    v->postvisit_whileloop     = AnalysisVisitor_exit_whileloop;
    v->postvisit_break     = AnalysisVisitor_check_break;
    v->postvisit_continue  = AnalysisVisitor_check_continue;
    v->postvisit_assignment = AnalysisVisitor_check_assignment;
    v->postvisit_funccall = AnalysisVisitor_check_funccall;
    v->postvisit_block = AnalysisVisitor_check_duplicate_vars;
    v->postvisit_funcdecl = AnalysisVisitor_check_funcdecl;
 
    /* perform analysis, save error list, clean up, and return errors */
    NodeVisitor_traverse(v, tree);
    ErrorList* errors = ((AnalysisData*)v->data)->errors;
    NodeVisitor_free(v);
    return errors;
}


