/**
 * @file p3-analysis.c
 * @brief Compiler phase 3: static analysis
 * @author Ray Steen
 * @author Mitch Feigenbaum
 */

/**
 AI Assist Statement: Claude AI was used to help generate code blocks and to create test cases.
*/

/**
 * @file p3-analysis.c
 * @brief Compiler phase 3: static analysis
 */
#include "p3-analysis.h"

/**
 * @brief State/data for static analysis visitor
 */
typedef struct AnalysisData {
    ErrorList* errors;
    int loop_depth;            // Track nested loop depth for break/continue validation
    ASTNode* current_function; // Track current function for return type checking
} AnalysisData;

/**
 * @brief Allocate memory for analysis data
 */
AnalysisData* AnalysisData_new(void)
{
    AnalysisData* data = (AnalysisData*)calloc(1, sizeof(AnalysisData));
    CHECK_MALLOC_PTR(data);
    data->errors = ErrorList_new();
    data->loop_depth = 0;
    data->current_function = NULL;
    return data;
}

/**
 * @brief Deallocate memory for analysis data
 */
void AnalysisData_free(AnalysisData* data)
{
    free(data);
}

#define DATA ((AnalysisData*)visitor->data)
#define ERROR_LIST (((AnalysisData*)visitor->data)->errors)

/**
 * @brief Wrapper for lookup_symbol that reports an error if not found
 */
Symbol* lookup_symbol_with_reporting(NodeVisitor* visitor, ASTNode* node, const char* name)
{
    Symbol* symbol = lookup_symbol(node, name);
    if (symbol == NULL) {
        ErrorList_printf(ERROR_LIST, "Symbol '%s' undefined on line %d", name, node->source_line);
    }
    return symbol;
}

#define SET_INFERRED_TYPE(T) ASTNode_set_printable_attribute(node, "type", (void*)(T), \
    type_attr_print, dummy_free)
#define GET_INFERRED_TYPE(N) (DecafType)(long) ASTNode_get_attribute(N, "type")

/**
 * @brief Check for duplicate symbols in symbol table
 */
void check_duplicate_symbols(NodeVisitor* visitor, ASTNode* node)
{
    if (!ASTNode_has_attribute(node, "symbolTable")) {
        return;
    }

    SymbolTable* table = (SymbolTable*)ASTNode_get_attribute(node, "symbolTable");
    SymbolList* symbols = table->local_symbols;

    // track the last called duplicate, so that we can avoid duplicate error messages
    char* last_called_name = "";

    // Check each symbol against all others in the same scope
    for (Symbol* s1 = symbols->head; s1 != NULL; s1 = s1->next) {
        for (Symbol* s2 = s1->next; s2 != NULL; s2 = s2->next) {
            if (strncmp(s1->name, s2->name, MAX_ID_LEN) == 0 && strncmp(s1->name, last_called_name, MAX_ID_LEN) != 0) {
                ErrorList_printf(ERROR_LIST, "Duplicate symbols named '%s' in scope started on line %d", s1->name, node->source_line);
                last_called_name = s1->name;
            }
        }
    }
}

/**
 * @brief Check variable declaration
 */
void AnalysisVisitor_check_vardecl(NodeVisitor* visitor, ASTNode* node)
{
    // Reject void variables
    if (node->vardecl.type == VOID) {
        ErrorList_printf(ERROR_LIST, "Void variable '%s' on line %d",
            node->vardecl.name, node->source_line);
    }

    // Reject arrays of size zero
    if (node->vardecl.is_array && node->vardecl.array_length == 0) {
        ErrorList_printf(ERROR_LIST, "Array '%s' on line %d must have positive non-zero length",
            node->vardecl.name, node->source_line);
    }
    if (node->vardecl.is_array && DATA->current_function != NULL) {
        ErrorList_printf(ERROR_LIST, "Local variable '%s' on line %d cannot be an array", node->vardecl.name, node->source_line);
    }
}

/**
 * @brief Check program structure
 */
void AnalysisVisitor_check_program(NodeVisitor* visitor, ASTNode* node)
{
    // Check to see if there are any duplicate symbols (Extend this to every individual decleration)
    check_duplicate_symbols(visitor, node);
    Symbol* main_symbol = lookup_symbol(node, "main");

    // Check that main exists and is a function
    if (main_symbol == NULL || main_symbol->symbol_type != FUNCTION_SYMBOL) {
        ErrorList_printf(ERROR_LIST, "Program does not contain a 'main' function");
    }
    // Check that main returns int and takes no parameters
    else {
        if (main_symbol->type != INT) {
            ErrorList_printf(ERROR_LIST, "'main' must return an integer");
        }
        if (main_symbol->parameters->size != 0) {
            ErrorList_printf(ERROR_LIST, "'main' must take no parameters");
        }
    }
}

/**
 * @brief Pre-visit function declaration
 */
void AnalysisVisitor_previsit_funcdecl(NodeVisitor* visitor, ASTNode* node)
{
    DATA->current_function = node;
    check_duplicate_symbols(visitor, node);
}

/**
 * @brief Post-visit function declaration
 */
void AnalysisVisitor_postvisit_funcdecl(NodeVisitor* visitor, ASTNode* node)
{
    DATA->current_function = NULL;
}

/**
 * @brief Check block for duplicate symbols
 */
void AnalysisVisitor_previsit_block(NodeVisitor* visitor, ASTNode* node)
{
    check_duplicate_symbols(visitor, node);
}

/**
 * @brief Pre-visit while loop
 */
void AnalysisVisitor_previsit_whileloop(NodeVisitor* visitor, ASTNode* node)
{
    DATA->loop_depth++;
}

/**
 * @brief Post-visit while loop
 */
void AnalysisVisitor_postvisit_whileloop(NodeVisitor* visitor, ASTNode* node)
{
    if (!ASTNode_has_attribute(node->whileloop.condition, "type")) {
        return;
    }

    DecafType cond_type = GET_INFERRED_TYPE(node->whileloop.condition);

    // Skip if type is unknown (error already reported)
    if (cond_type == UNKNOWN) {
        return;
    }

    if (cond_type != BOOL) {
        ErrorList_printf(ERROR_LIST, "Type mismatch: bool expected but %s found on line %d",
            DecafType_to_string(cond_type), node->source_line);
    }

    DATA->loop_depth--;
}

/**
 * @brief Check break statement
 */
void AnalysisVisitor_check_break(NodeVisitor* visitor, ASTNode* node)
{
    if (DATA->loop_depth == 0) {
        ErrorList_printf(ERROR_LIST, "Invalid 'break' outside loop on line %d",
            node->source_line);
    }
}

/**
 * @brief Check continue statement
 */
void AnalysisVisitor_check_continue(NodeVisitor* visitor, ASTNode* node)
{
    if (DATA->loop_depth == 0) {
        ErrorList_printf(ERROR_LIST, "Invalid 'continue' outside loop on line %d",
            node->source_line);
    }
}

/**
 * @brief Check return statement
 */
void AnalysisVisitor_postvisit_return(NodeVisitor* visitor, ASTNode* node)
{
    if (DATA->current_function == NULL) {
        return;
    }

    DecafType expected_type = DATA->current_function->funcdecl.return_type;

    // Check void return
    if (node->funcreturn.value == NULL) {
        if (expected_type != VOID) {
            ErrorList_printf(ERROR_LIST, "Invalid void return from non-void function on line %d", node->source_line);
        }
    } else if (node->funcreturn.value != NULL && expected_type == VOID) {
        ErrorList_printf(ERROR_LIST, "Invalid non-void return from void function on line %d", node->source_line);
    } else {
        // Check return value type - safely check if type attribute exists
        if (!ASTNode_has_attribute(node->funcreturn.value, "type")) {
            return; // Type inference failed, error already reported
        }
        DecafType actual_type = GET_INFERRED_TYPE(node->funcreturn.value);
        // Skip if type is unknown (error already reported)
        if (actual_type == UNKNOWN) {
            return;
        }
        if (actual_type != expected_type) {
            ErrorList_printf(ERROR_LIST, "Type mismatch: %s expected but %s found on line %d",
                DecafType_to_string(expected_type), DecafType_to_string(actual_type),
                node->source_line);
        }
    }
}

/**
 * @brief Infer and check literal types
 */
void AnalysisVisitor_postvisit_literal(NodeVisitor* visitor, ASTNode* node)
{
    SET_INFERRED_TYPE(node->literal.type);
}

/**
 * @brief Infer and check location types
 */
void AnalysisVisitor_postvisit_location(NodeVisitor* visitor, ASTNode* node)
{
    Symbol* symbol = lookup_symbol_with_reporting(visitor, node, node->location.name);
    if (symbol == NULL) {
        SET_INFERRED_TYPE(UNKNOWN);
        return;
    }

    // Check array access
    if (node->location.index != NULL) {
        // Must be array
        if (symbol->symbol_type != ARRAY_SYMBOL) {
            ErrorList_printf(ERROR_LIST, "Non-array '%s' accessed as an array on line %d",
                node->location.name, node->source_line);
            SET_INFERRED_TYPE(symbol->type);
            return;
        }

        // Index must be int - check if type exists first
        if (ASTNode_has_attribute(node->location.index, "type")) {
            DecafType index_type = GET_INFERRED_TYPE(node->location.index);
            if (index_type != UNKNOWN && index_type != INT) {
                ErrorList_printf(ERROR_LIST, "Type mismatch: int expected but %s found on line %d",
                    DecafType_to_string(index_type), node->source_line);
            }
        }

        SET_INFERRED_TYPE(symbol->type);
    } else {
        // Not indexed - must not be array
        if (symbol->symbol_type == ARRAY_SYMBOL) {
            ErrorList_printf(ERROR_LIST, "Array '%s' accessed without index on line %d",
                node->location.name, node->source_line);
            SET_INFERRED_TYPE(UNKNOWN);
            return;
        }

        SET_INFERRED_TYPE(symbol->type);
    }
}

/**
 * @brief Check function call
 */
void AnalysisVisitor_postvisit_funccall(NodeVisitor* visitor, ASTNode* node)
{
    Symbol* symbol = lookup_symbol_with_reporting(visitor, node, node->funccall.name);
    if (symbol == NULL) {
        SET_INFERRED_TYPE(UNKNOWN);
        return;
    }

    // Must be function
    if (symbol->symbol_type != FUNCTION_SYMBOL) {
        ErrorList_printf(ERROR_LIST, "Invalid call to non-function '%s' on line %d",
            node->funccall.name, node->source_line);
        SET_INFERRED_TYPE(UNKNOWN);
        return;
    }

    // Check argument count
    size_t expected_args = symbol->parameters->size;
    size_t actual_args = node->funccall.arguments->size;

    if (expected_args != actual_args) {
        ErrorList_printf(ERROR_LIST, "Invalid number of function arguments on line %d", node->source_line);
        SET_INFERRED_TYPE(symbol->type);
        return;
    }

    // Check argument types
    Parameter* param = symbol->parameters->head;
    ASTNode* arg = node->funccall.arguments->head;
    int arg_num = 0;

    while (param != NULL && arg != NULL) {
        DecafType expected_type = param->type;

        // Safely check if arg has type attribute
        if (ASTNode_has_attribute(arg, "type")) {
            DecafType actual_type = GET_INFERRED_TYPE(arg);

            if (actual_type != expected_type) {
                ErrorList_printf(ERROR_LIST, "Type mismatch in parameter %d of call to '%s': expected %s but found %s on line %d", arg_num, node->funccall.name, DecafType_to_string(expected_type), DecafType_to_string(actual_type), node->source_line);
            }
        }

        param = param->next;
        arg = arg->next;
        arg_num++;
    }

    SET_INFERRED_TYPE(symbol->type);
}

/**
 * @brief Check unary operation
 */
void AnalysisVisitor_postvisit_unaryop(NodeVisitor* visitor, ASTNode* node)
{
    if (!ASTNode_has_attribute(node->unaryop.child, "type")) {
        SET_INFERRED_TYPE(UNKNOWN);
        return;
    }

    DecafType child_type = GET_INFERRED_TYPE(node->unaryop.child);

    // If child type is unknown, propagate unknown but don't report error
    if (child_type == UNKNOWN) {
        SET_INFERRED_TYPE(UNKNOWN);
        return;
    }

    switch (node->unaryop.operator) {
    case NEGOP: // Negation: int -> int
        if (child_type != INT) {
            ErrorList_printf(ERROR_LIST, "Type mismatch: int expected but %s found on line %d",
                DecafType_to_string(child_type), node->source_line);
        }
        SET_INFERRED_TYPE(INT);
        break;

    case NOTOP: // Not: bool -> bool
        if (child_type != BOOL) {
            ErrorList_printf(ERROR_LIST, "Type mismatch: bool expected but %s found on line %d",
                DecafType_to_string(child_type), node->source_line);
        }
        SET_INFERRED_TYPE(BOOL);
        break;
    }
}

/**
 * @brief Check binary operation
 */
void AnalysisVisitor_postvisit_binaryop(NodeVisitor* visitor, ASTNode* node)
{
    if (!ASTNode_has_attribute(node->binaryop.left, "type") || !ASTNode_has_attribute(node->binaryop.right, "type")) {
        SET_INFERRED_TYPE(UNKNOWN);
        return;
    }

    DecafType left_type = GET_INFERRED_TYPE(node->binaryop.left);
    DecafType right_type = GET_INFERRED_TYPE(node->binaryop.right);

    // If either operand type is unknown, propagate unknown but don't report error
    if (left_type == UNKNOWN || right_type == UNKNOWN) {
        SET_INFERRED_TYPE(UNKNOWN);
        return;
    }

    switch (node->binaryop.operator) {
    case ADDOP:
    case SUBOP:
    case MULOP:
    case DIVOP:
    case MODOP:
        // Arithmetic: int x int -> int
        if (left_type != INT) {
            ErrorList_printf(ERROR_LIST, "Type mismatch: int expected but %s found on line %d",
                DecafType_to_string(left_type), node->source_line);
        }
        if (right_type != INT) {
            ErrorList_printf(ERROR_LIST, "Type mismatch: int expected but %s found on line %d",
                DecafType_to_string(right_type), node->source_line);
        }
        SET_INFERRED_TYPE(INT);
        break;

    case LTOP:
    case LEOP:
    case GEOP:
    case GTOP:
        // Comparison: int x int -> bool
        if (left_type != INT) {
            ErrorList_printf(ERROR_LIST, "Type mismatch: int expected but %s found on line %d",
                DecafType_to_string(left_type), node->source_line);
        }
        if (right_type != INT) {
            ErrorList_printf(ERROR_LIST, "Type mismatch: int expected but %s found on line %d",
                DecafType_to_string(right_type), node->source_line);
        }
        SET_INFERRED_TYPE(BOOL);
        break;

    case EQOP:
    case NEQOP:
        // Equality: T x T -> bool (same types)
        if (left_type != right_type) {
            ErrorList_printf(ERROR_LIST, "Type mismatch: %s is incompatible with %s on line %d",
                DecafType_to_string(left_type), DecafType_to_string(right_type),
                node->source_line);
        }
        SET_INFERRED_TYPE(BOOL);
        break;

    case ANDOP:
    case OROP:
        // Logical: bool x bool -> bool
        if (left_type != BOOL) {
            ErrorList_printf(ERROR_LIST, "Type mismatch: bool expected but %s found on line %d",
                DecafType_to_string(left_type), node->source_line);
        }
        if (right_type != BOOL) {
            ErrorList_printf(ERROR_LIST, "Type mismatch: bool expected but %s found on line %d",
                DecafType_to_string(right_type), node->source_line);
        }
        SET_INFERRED_TYPE(BOOL);
        break;
    }
}

/**
 * @brief Check assignment
 */
void AnalysisVisitor_postvisit_assignment(NodeVisitor* visitor, ASTNode* node)
{
    if (!ASTNode_has_attribute(node->assignment.location, "type") || !ASTNode_has_attribute(node->assignment.value, "type")) {
        return;
    }

    DecafType loc_type = GET_INFERRED_TYPE(node->assignment.location);
    DecafType val_type = GET_INFERRED_TYPE(node->assignment.value);

    // Skip if either type is unknown (error already reported)
    if (loc_type == UNKNOWN || val_type == UNKNOWN) {
        return;
    }

    if (loc_type != val_type) {
        ErrorList_printf(ERROR_LIST,
            "Type mismatch: %s is incompatible with %s on line %d",
            DecafType_to_string(loc_type), DecafType_to_string(val_type),
            node->source_line);
    }
}

/**
 * @brief Check conditional
 */
void AnalysisVisitor_postvisit_conditional(NodeVisitor* visitor, ASTNode* node)
{
    if (!ASTNode_has_attribute(node->conditional.condition, "type")) {
        return;
    }

    DecafType cond_type = GET_INFERRED_TYPE(node->conditional.condition);

    // Skip if type is unknown (error already reported)
    if (cond_type == UNKNOWN) {
        return;
    }

    if (cond_type != BOOL) {
        ErrorList_printf(ERROR_LIST, "Type mismatch: bool expected but %s found on line %d",
            DecafType_to_string(cond_type), node->source_line);
    }
}

/**
 * @brief Main analysis function
 */
ErrorList* analyze(ASTNode* tree)
{
    // Handle null tree
    if (tree == NULL) {
        ErrorList* errors = ErrorList_new();
        return errors;
    }

    NodeVisitor* v = NodeVisitor_new();
    v->data = (void*)AnalysisData_new();
    v->dtor = (Destructor)AnalysisData_free;

    // Register callbacks
    v->postvisit_program = AnalysisVisitor_check_program;
    v->previsit_vardecl = AnalysisVisitor_check_vardecl;
    v->previsit_funcdecl = AnalysisVisitor_previsit_funcdecl;
    v->postvisit_funcdecl = AnalysisVisitor_postvisit_funcdecl;
    v->previsit_block = AnalysisVisitor_previsit_block;
    v->previsit_whileloop = AnalysisVisitor_previsit_whileloop;
    v->postvisit_whileloop = AnalysisVisitor_postvisit_whileloop;
    v->previsit_break = AnalysisVisitor_check_break;
    v->previsit_continue = AnalysisVisitor_check_continue;

    // Type inference (postvisit to process children first)
    v->postvisit_literal = AnalysisVisitor_postvisit_literal;
    v->postvisit_location = AnalysisVisitor_postvisit_location;
    v->postvisit_funccall = AnalysisVisitor_postvisit_funccall;
    v->postvisit_unaryop = AnalysisVisitor_postvisit_unaryop;
    v->postvisit_binaryop = AnalysisVisitor_postvisit_binaryop;

    // Type checking
    v->postvisit_assignment = AnalysisVisitor_postvisit_assignment;
    v->postvisit_conditional = AnalysisVisitor_postvisit_conditional;
    v->postvisit_return = AnalysisVisitor_postvisit_return;

    NodeVisitor_traverse(v, tree);

    ErrorList* errors = ((AnalysisData*)v->data)->errors;
    NodeVisitor_free(v);

    return errors;
}
