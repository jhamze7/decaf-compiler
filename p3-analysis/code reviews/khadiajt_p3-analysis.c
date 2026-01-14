/**
 * @file p3-analysis.c
 * @brief Compiler phase 3: static analysis
 * @authors Khadiajt, Naweed
 * We used an AI-assist tool -ChatGPT) to help refine our logic and fix issues with the B-level tests.
    It guided us in improving error handling and preventing duplicate or cascading errors.
    We also used it to generate extra test cases to confirm our code worked correctly.
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

    /**
     * @brief Current function return type (for checking return statements)
     */
    DecafType current_function_return_type;

    /**
     * @brief Loop depth counter (for checking break/continue)
     */
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
    data->current_function_return_type = UNKNOWN;
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

/*
 * VISITOR CALLBACKS
 */

void previsit_program(NodeVisitor* visitor, ASTNode* node)
{
    /* Add built-in print functions to the global symbol table */
    SymbolTable* table = (SymbolTable*)ASTNode_get_attribute(node, "symbolTable");
    
    /* Check if print functions are already in the symbol table */
    if (SymbolTable_lookup(table, "print_str") == NULL) {
        /* print_str : STR -> VOID */
        ParameterList* print_str_params = ParameterList_new();
        ParameterList_add_new(print_str_params, "value", STR);
        Symbol* print_str = Symbol_new_function("print_str", VOID, print_str_params);
        SymbolTable_insert(table, print_str);
    }
    
    if (SymbolTable_lookup(table, "print_int") == NULL) {
        /* print_int : INT -> VOID */
        ParameterList* print_int_params = ParameterList_new();
        ParameterList_add_new(print_int_params, "value", INT);
        Symbol* print_int = Symbol_new_function("print_int", VOID, print_int_params);
        SymbolTable_insert(table, print_int);
    }
    
    if (SymbolTable_lookup(table, "print_bool") == NULL) {
        /* print_bool : BOOL -> VOID */
        ParameterList* print_bool_params = ParameterList_new();
        ParameterList_add_new(print_bool_params, "value", BOOL);
        Symbol* print_bool = Symbol_new_function("print_bool", VOID, print_bool_params);
        SymbolTable_insert(table, print_bool);
    }
}

void postvisit_program(NodeVisitor* visitor, ASTNode* node)
{
    /* Check that a main function exists and is a function (not a variable) */
    SymbolTable* table = (SymbolTable*)ASTNode_get_attribute(node, "symbolTable");
    Symbol* main_symbol = SymbolTable_lookup(table, "main");
    
    if (main_symbol == NULL) {
        ErrorList_printf(ERROR_LIST, "Program does not contain a 'main' function");
    } else if (main_symbol->symbol_type != FUNCTION_SYMBOL) {
        ErrorList_printf(ERROR_LIST, "'main' must be a function");
    }
}

void postvisit_vardecl(NodeVisitor* visitor, ASTNode* node)
{
    /* Check for void variable declarations */
    if (node->vardecl.type == VOID) {
        ErrorList_printf(ERROR_LIST, "Variable '%s' declared void on line %d",
                         node->vardecl.name, node->source_line);
    }
    
    /* Check for arrays of size zero */
    if (node->vardecl.is_array && node->vardecl.array_length == 0) {
        ErrorList_printf(ERROR_LIST, "Array '%s' has invalid size 0 on line %d",
                         node->vardecl.name, node->source_line);
    }
    
    /* Check for duplicate declarations in the same scope */
    /* We need to look at the symbol table and check if there are duplicates with the same name */
    /* The symbol table might have duplicates if the same name was declared twice */
    ASTNode* parent = (ASTNode*)ASTNode_get_attribute(node, "parent");
    if (parent != NULL) {
        SymbolTable* table = NULL;
        
        /* Find the appropriate symbol table based on parent type */
        if (parent->type == PROGRAM) {
            table = (SymbolTable*)ASTNode_get_attribute(parent, "symbolTable");
        } else if (parent->type == BLOCK) {
            table = (SymbolTable*)ASTNode_get_attribute(parent, "symbolTable");
        }
        
        if (table != NULL) {
            /* Count occurrences of this variable name in the LOCAL symbols only */
            int count = 0;
            FOR_EACH(Symbol*, sym, table->local_symbols) {
                if (strncmp(sym->name, node->vardecl.name, MAX_ID_LEN) == 0) {
                    count++;
                }
            }
            
            if (count > 1) {
                ErrorList_printf(ERROR_LIST, "Duplicate declaration of variable '%s' on line %d",
                                 node->vardecl.name, node->source_line);
            }
        }
    }
}

void previsit_funcdecl(NodeVisitor* visitor, ASTNode* node)
{
    /* Track current function return type for return statement checking */
    DATA->current_function_return_type = node->funcdecl.return_type;
    
    /* Check for duplicate function declarations in the global scope */
    ASTNode* parent = (ASTNode*)ASTNode_get_attribute(node, "parent");
    if (parent != NULL && parent->type == PROGRAM) {
        SymbolTable* table = (SymbolTable*)ASTNode_get_attribute(parent, "symbolTable");
        
        if (table != NULL) {
            /* Count occurrences of this function name in the LOCAL symbols only */
            int count = 0;
            FOR_EACH(Symbol*, sym, table->local_symbols) {
                if (strncmp(sym->name, node->funcdecl.name, MAX_ID_LEN) == 0) {
                    count++;
                }
            }
            
            if (count > 1) {
                ErrorList_printf(ERROR_LIST, "Duplicate declaration of function '%s' on line %d",
                                 node->funcdecl.name, node->source_line);
            }
        }
    }
    
    /* Check for duplicate parameters */
    ParameterList* params = node->funcdecl.parameters;
    FOR_EACH(Parameter*, p1, params) {
        int count = 0;
        FOR_EACH(Parameter*, p2, params) {
            if (strncmp(p1->name, p2->name, MAX_ID_LEN) == 0) {
                count++;
            }
        }
        if (count > 1) {
            ErrorList_printf(ERROR_LIST, "Duplicate parameter '%s' in function '%s' on line %d",
                             p1->name, node->funcdecl.name, node->source_line);
            break;  /* Only report once */
        }
    }
}

void postvisit_funcdecl(NodeVisitor* visitor, ASTNode* node)
{
    /* Reset current function return type */
    DATA->current_function_return_type = UNKNOWN;
}

void previsit_whileloop(NodeVisitor* visitor, ASTNode* node)
{
    /* Increment loop depth */
    DATA->loop_depth++;
}

void postvisit_whileloop(NodeVisitor* visitor, ASTNode* node)
{
    /* Check that condition is boolean */
    DecafType cond_type = GET_INFERRED_TYPE(node->whileloop.condition);
    if (cond_type != BOOL && cond_type != UNKNOWN) {
        ErrorList_printf(ERROR_LIST, "While condition must be bool, not %s on line %d",
                         DecafType_to_string(cond_type), node->source_line);
    }
    
    /* Decrement loop depth */
    DATA->loop_depth--;
}

void postvisit_conditional(NodeVisitor* visitor, ASTNode* node)
{
    /* Check that condition is boolean */
    DecafType cond_type = GET_INFERRED_TYPE(node->conditional.condition);
    if (cond_type != BOOL && cond_type != UNKNOWN) {
        ErrorList_printf(ERROR_LIST, "If condition must be bool, not %s on line %d",
                         DecafType_to_string(cond_type), node->source_line);
    }
}

void postvisit_assignment(NodeVisitor* visitor, ASTNode* node)
{
    /* Check that location and value have compatible types */
    DecafType loc_type = GET_INFERRED_TYPE(node->assignment.location);
    DecafType val_type = GET_INFERRED_TYPE(node->assignment.value);
    
    if (loc_type != UNKNOWN && val_type != UNKNOWN && loc_type != val_type) {
        ErrorList_printf(ERROR_LIST, "Assignment type mismatch: %s = %s on line %d",
                         DecafType_to_string(loc_type), DecafType_to_string(val_type),
                         node->source_line);
    }
}

void postvisit_return(NodeVisitor* visitor, ASTNode* node)
{
    DecafType expected_type = DATA->current_function_return_type;
    
    if (node->funcreturn.value == NULL) {
        /* return with no value */
        if (expected_type != VOID && expected_type != UNKNOWN) {
            ErrorList_printf(ERROR_LIST, "Return value expected for non-void function on line %d",
                             node->source_line);
        }
    } else {
        /* return with a value */
        DecafType return_type = GET_INFERRED_TYPE(node->funcreturn.value);
        
        if (expected_type == VOID) {
            ErrorList_printf(ERROR_LIST, "Void function cannot return a value on line %d",
                             node->source_line);
        } else if (expected_type != UNKNOWN && return_type != UNKNOWN && 
                   expected_type != return_type) {
            ErrorList_printf(ERROR_LIST, "Return type mismatch: expected %s, got %s on line %d",
                             DecafType_to_string(expected_type),
                             DecafType_to_string(return_type),
                             node->source_line);
        }
    }
}

void postvisit_break(NodeVisitor* visitor, ASTNode* node)
{
    /* Check that break is inside a loop */
    if (DATA->loop_depth == 0) {
        ErrorList_printf(ERROR_LIST, "Break statement outside loop on line %d",
                         node->source_line);
    }
}

void postvisit_continue(NodeVisitor* visitor, ASTNode* node)
{
    /* Check that continue is inside a loop */
    if (DATA->loop_depth == 0) {
        ErrorList_printf(ERROR_LIST, "Continue statement outside loop on line %d",
                         node->source_line);
    }
}

void postvisit_literal(NodeVisitor* visitor, ASTNode* node)
{
    /* Infer type from literal */
    SET_INFERRED_TYPE(node->literal.type);
}

void postvisit_location(NodeVisitor* visitor, ASTNode* node)
{
    /* Look up the symbol */
    Symbol* symbol = lookup_symbol_with_reporting(visitor, node, node->location.name);
    
    if (symbol == NULL) {
        /* Error already reported, set type to UNKNOWN */
        SET_INFERRED_TYPE(UNKNOWN);
        return;
    }
    
    /* Check if this is an array access */
    if (node->location.index != NULL) {
        /* This should be an array */
        if (symbol->symbol_type != ARRAY_SYMBOL) {
            ErrorList_printf(ERROR_LIST, "Variable '%s' is not an array on line %d",
                             node->location.name, node->source_line);
            SET_INFERRED_TYPE(UNKNOWN);
            return;
        }
        
        /* Check that index is an integer */
        DecafType index_type = GET_INFERRED_TYPE(node->location.index);
        if (index_type != INT && index_type != UNKNOWN) {
            ErrorList_printf(ERROR_LIST, "Array index must be int, not %s on line %d",
                             DecafType_to_string(index_type), node->source_line);
        }
        
        /* Infer type from array element type */
        SET_INFERRED_TYPE(symbol->type);
    } else {
        /* This should be a scalar */
        if (symbol->symbol_type == ARRAY_SYMBOL) {
            ErrorList_printf(ERROR_LIST, "Array '%s' used without index on line %d",
                             node->location.name, node->source_line);
            SET_INFERRED_TYPE(UNKNOWN);
            return;
        }
        
        /* Infer type from symbol */
        SET_INFERRED_TYPE(symbol->type);
    }
}

void postvisit_funccall(NodeVisitor* visitor, ASTNode* node)
{
    /* Look up the function symbol */
    Symbol* symbol = lookup_symbol_with_reporting(visitor, node, node->funccall.name);
    
    if (symbol == NULL) {
        /* Error already reported, set type to UNKNOWN */
        SET_INFERRED_TYPE(UNKNOWN);
        return;
    }
    
    /* Check that this is actually a function */
    if (symbol->symbol_type != FUNCTION_SYMBOL) {
        ErrorList_printf(ERROR_LIST, "'%s' is not a function on line %d",
                         node->funccall.name, node->source_line);
        SET_INFERRED_TYPE(UNKNOWN);
        return;
    }
    
    /* Check parameter count */
    int expected_count = 0;
    Parameter* param = symbol->parameters->head;
    while (param != NULL) {
        expected_count++;
        param = param->next;
    }
    
    int actual_count = 0;
    ASTNode* arg = node->funccall.arguments->head;
    while (arg != NULL) {
        actual_count++;
        arg = arg->next;
    }
    
    if (expected_count != actual_count) {
        ErrorList_printf(ERROR_LIST, "Function '%s' expects %d arguments, got %d on line %d",
                         node->funccall.name, expected_count, actual_count, node->source_line);
        SET_INFERRED_TYPE(symbol->type);
        return;
    }
    
    /* Check parameter types */
    param = symbol->parameters->head;
    arg = node->funccall.arguments->head;
    int arg_num = 1;
    
    while (param != NULL && arg != NULL) {
        DecafType expected_type = param->type;
        DecafType actual_type = GET_INFERRED_TYPE(arg);
        
        if (expected_type != UNKNOWN && actual_type != UNKNOWN && 
            expected_type != actual_type) {
            ErrorList_printf(ERROR_LIST, 
                             "Function '%s' parameter %d type mismatch: expected %s, got %s on line %d",
                             node->funccall.name, arg_num,
                             DecafType_to_string(expected_type),
                             DecafType_to_string(actual_type),
                             node->source_line);
        }
        
        param = param->next;
        arg = arg->next;
        arg_num++;
    }
    
    /* Infer return type */
    SET_INFERRED_TYPE(symbol->type);
}

void postvisit_unaryop(NodeVisitor* visitor, ASTNode* node)
{
    DecafType child_type = GET_INFERRED_TYPE(node->unaryop.child);
    
    switch (node->unaryop.operator) {
        case NEGOP:
            /* Negation requires int operand */
            if (child_type != INT && child_type != UNKNOWN) {
                ErrorList_printf(ERROR_LIST, "Negation requires int operand, not %s on line %d",
                                 DecafType_to_string(child_type), node->source_line);
            }
            SET_INFERRED_TYPE(INT);
            break;
            
        case NOTOP:
            /* Logical NOT requires bool operand */
            if (child_type != BOOL && child_type != UNKNOWN) {
                ErrorList_printf(ERROR_LIST, "Logical NOT requires bool operand, not %s on line %d",
                                 DecafType_to_string(child_type), node->source_line);
            }
            SET_INFERRED_TYPE(BOOL);
            break;
            
        default:
            SET_INFERRED_TYPE(UNKNOWN);
            break;
    }
}

void postvisit_binaryop(NodeVisitor* visitor, ASTNode* node)
{
    DecafType left_type = GET_INFERRED_TYPE(node->binaryop.left);
    DecafType right_type = GET_INFERRED_TYPE(node->binaryop.right);
    
    switch (node->binaryop.operator) {
        case ADDOP:
        case SUBOP:
        case MULOP:
        case DIVOP:
        case MODOP:
            /* Arithmetic operators require int operands */
            if (left_type != INT && left_type != UNKNOWN) {
                ErrorList_printf(ERROR_LIST, "Arithmetic operator requires int operands, not %s on line %d",
                                 DecafType_to_string(left_type), node->source_line);
            }
            if (right_type != INT && right_type != UNKNOWN) {
                ErrorList_printf(ERROR_LIST, "Arithmetic operator requires int operands, not %s on line %d",
                                 DecafType_to_string(right_type), node->source_line);
            }
            SET_INFERRED_TYPE(INT);
            break;
            
        case LTOP:
        case LEOP:
        case GEOP:
        case GTOP:
            /* Relational operators require int operands, return bool */
            if (left_type != INT && left_type != UNKNOWN) {
                ErrorList_printf(ERROR_LIST, "Relational operator requires int operands, not %s on line %d",
                                 DecafType_to_string(left_type), node->source_line);
            }
            if (right_type != INT && right_type != UNKNOWN) {
                ErrorList_printf(ERROR_LIST, "Relational operator requires int operands, not %s on line %d",
                                 DecafType_to_string(right_type), node->source_line);
            }
            SET_INFERRED_TYPE(BOOL);
            break;
            
        case EQOP:
        case NEQOP:
            /* Equality operators require operands of the same type, return bool */
            if (left_type != UNKNOWN && right_type != UNKNOWN && left_type != right_type) {
                ErrorList_printf(ERROR_LIST, "Equality operator requires operands of the same type on line %d",
                                 node->source_line);
            }
            SET_INFERRED_TYPE(BOOL);
            break;
            
        case ANDOP:
        case OROP:
            /* Logical operators require bool operands */
            if (left_type != BOOL && left_type != UNKNOWN) {
                ErrorList_printf(ERROR_LIST, "Logical operator requires bool operands, not %s on line %d",
                                 DecafType_to_string(left_type), node->source_line);
            }
            if (right_type != BOOL && right_type != UNKNOWN) {
                ErrorList_printf(ERROR_LIST, "Logical operator requires bool operands, not %s on line %d",
                                 DecafType_to_string(right_type), node->source_line);
            }
            SET_INFERRED_TYPE(BOOL);
            break;
            
        default:
            SET_INFERRED_TYPE(UNKNOWN);
            break;
    }
}

ErrorList* analyze (ASTNode* tree)
{
    /* Handle NULL tree - return an empty error list */
    if (tree == NULL) {
        return ErrorList_new();
    }
    
    /* allocate analysis structures */
    NodeVisitor* v = NodeVisitor_new();
    v->data = (void*)AnalysisData_new();
    v->dtor = (Destructor)AnalysisData_free;

    /* register analysis callbacks */
    v->previsit_program = previsit_program;
    v->postvisit_program = postvisit_program;
    v->postvisit_vardecl = postvisit_vardecl;
    v->previsit_funcdecl = previsit_funcdecl;
    v->postvisit_funcdecl = postvisit_funcdecl;
    v->previsit_whileloop = previsit_whileloop;
    v->postvisit_whileloop = postvisit_whileloop;
    v->postvisit_conditional = postvisit_conditional;
    v->postvisit_assignment = postvisit_assignment;
    v->postvisit_return = postvisit_return;
    v->postvisit_break = postvisit_break;
    v->postvisit_continue = postvisit_continue;
    v->postvisit_literal = postvisit_literal;
    v->postvisit_location = postvisit_location;
    v->postvisit_funccall = postvisit_funccall;
    v->postvisit_unaryop = postvisit_unaryop;
    v->postvisit_binaryop = postvisit_binaryop;

    /* perform analysis, save error list, clean up, and return errors */
    NodeVisitor_traverse(v, tree);
    ErrorList* errors = ((AnalysisData*)v->data)->errors;
    NodeVisitor_free(v);
    return errors;
}

