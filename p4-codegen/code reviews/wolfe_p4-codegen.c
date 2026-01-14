/**
 * @file p4-codegen.c
 * @brief Compiler phase 4: code generation
 */

//AI Statement: AI was used to generate the test suite and to help with array locations and assignments
               
#include "p4-codegen.h"

/**
 * @brief State/data for the code generator visitor
 */
typedef struct CodeGenData
{
    /**
     * @brief Reference to the epilogue jump label for the current function
     */
    Operand current_epilogue_jump_label;

    /* add any new desired state information (and clean it up in CodeGenData_free) */
    Operand current_loop_start;
    Operand current_loop_exit;
} CodeGenData;

/**
 * @brief Allocate memory for code gen data
 * 
 * @returns Pointer to allocated structure
 */
CodeGenData* CodeGenData_new (void)
{
    CodeGenData* data = (CodeGenData*)calloc(1, sizeof(CodeGenData));
    CHECK_MALLOC_PTR(data);
    data->current_epilogue_jump_label = empty_operand();
    data->current_loop_start = empty_operand();
    data->current_loop_exit  = empty_operand();
    return data;
}

/**
 * @brief Deallocate memory for code gen data
 * 
 * @param data Pointer to the structure to be deallocated
 */
void CodeGenData_free (CodeGenData* data)
{
    /* free everything in data that is allocated on the heap */

    /* free "data" itself */
    free(data);
}

/**
 * @brief Macro for more convenient access to the error list inside a @c visitor
 * data structure
 */
#define DATA ((CodeGenData*)visitor->data)

/**
 * @brief Fills a register with the base address of a variable.
 * 
 * @param node AST node to emit code into (if needed)
 * @param variable Desired variable
 * @returns Virtual register that contains the base address
 */
Operand var_base (ASTNode* node, Symbol* variable)
{
    Operand reg = empty_operand();
    switch (variable->location) {
        case STATIC_VAR:
            reg = virtual_register();
            ASTNode_emit_insn(node,
                    ILOCInsn_new_2op(LOAD_I, int_const(variable->offset), reg));
            break;
        case STACK_PARAM:
        case STACK_LOCAL:
            reg = base_register();
            break;
        default:
            break;
    }
    return reg;
}

/**
 * @brief Calculates the offset of a scalar variable reference and fills a register with that offset.
 * 
 * @param node AST node to emit code into (if needed)
 * @param variable Desired variable
 * @returns Virtual register that contains the base address
 */
Operand var_offset (ASTNode* node, Symbol* variable)
{
    Operand op = empty_operand();
    switch (variable->location) {
        case STATIC_VAR:    op = int_const(0); break;
        case STACK_PARAM:
        case STACK_LOCAL:   op = int_const(variable->offset);
        default:
            break;
    }
    return op;
}

#ifndef SKIP_IN_DOXYGEN

/*
 * Macros for more convenient instruction generation
 */

#define EMIT0OP(FORM)             ASTNode_emit_insn(node, ILOCInsn_new_0op(FORM))
#define EMIT1OP(FORM,OP1)         ASTNode_emit_insn(node, ILOCInsn_new_1op(FORM,OP1))
#define EMIT2OP(FORM,OP1,OP2)     ASTNode_emit_insn(node, ILOCInsn_new_2op(FORM,OP1,OP2))
#define EMIT3OP(FORM,OP1,OP2,OP3) ASTNode_emit_insn(node, ILOCInsn_new_3op(FORM,OP1,OP2,OP3))

void CodeGenVisitor_gen_program (NodeVisitor* visitor, ASTNode* node)
{
    /*
     * make sure "code" attribute exists at the program level even if there are
     * no functions (although this shouldn't happen if static analysis is run first)
     */
    ASTNode_set_attribute(node, "code", InsnList_new(), (Destructor)InsnList_free);

    /* copy code from each function */
    FOR_EACH(ASTNode*, func, node->program.functions) {
        ASTNode_copy_code(node, func);
    }
}

void CodeGenVisitor_previsit_funcdecl (NodeVisitor* visitor, ASTNode* node)
{
    /* generate a label reference for the epilogue that can be used while
     * generating the rest of the function (e.g., to be used when generating
     * code for a "return" statement) */
    DATA->current_epilogue_jump_label = anonymous_label();
}

void CodeGenVisitor_gen_funcdecl(NodeVisitor* visitor, ASTNode* node)
{
    // Emit function label 
    EMIT1OP(LABEL, call_label(node->funcdecl.name));

    //prologue

    // Save caller BP 
    EMIT1OP(PUSH, base_register());
    // BP = SP 
    EMIT2OP(I2I, stack_register(), base_register());
    // Allocate space for locals 
    int localSize = ASTNode_get_int_attribute(node, "localSize"); 
    if (localSize > 0) {
        EMIT3OP(ADD_I, stack_register(), int_const(-localSize), stack_register());
    }

    //function body 
    ASTNode_copy_code(node, node->funcdecl.body);

    //epilogue
    EMIT1OP(LABEL, DATA->current_epilogue_jump_label);

   
    EMIT2OP(I2I, base_register(), stack_register());


    EMIT1OP(POP, base_register());

    //return to caller
    EMIT0OP(RETURN);
}

void CodeGenVisitor_gen_block(NodeVisitor* visitor, ASTNode* node)
{
    FOR_EACH(ASTNode*, stmt, node->block.statements)
    {
        ASTNode_copy_code(node,stmt);
    }
}

void CodeGenVisitor_gen_return(NodeVisitor* visitor, ASTNode* node)
{
    /*copy code from child*/
    ASTNode_copy_code(node,node->funcreturn.value);
    
    Operand reg = ASTNode_get_temp_reg(node->funcreturn.value);

    /* get child's temp reg and use that instead of "reg" below*/
    EMIT2OP(I2I, reg, return_register());

    /* jump to the function epilogue */
    EMIT1OP(JUMP, DATA->current_epilogue_jump_label);
}

void CodeGenVisitor_gen_literal(NodeVisitor* visitor, ASTNode* node)
{
    Operand reg = virtual_register();
    ASTNode_set_temp_reg(node, reg);

    switch(node->literal.type)
    {
        case INT:
            EMIT2OP(LOAD_I, int_const(node->literal.integer), reg);
            break;
        case BOOL:
            EMIT2OP(LOAD_I, int_const(node->literal.boolean), reg);
            break;
        default:
            break;
    }
}

void CodeGenVisitor_gen_Conditional(NodeVisitor* visitor, ASTNode* node)
{

    //condition code
    ASTNode_copy_code(node, node->conditional.condition);
    Operand cond_reg = ASTNode_get_temp_reg(node->conditional.condition);

    //labels
    Operand l_then = anonymous_label();
    Operand l_else = anonymous_label();
    Operand l_end  = anonymous_label();

    //generate block code
    if (node->conditional.else_block) {
        EMIT3OP(CBR, cond_reg, l_then, l_else);

        EMIT1OP(LABEL, l_then);
        ASTNode_copy_code(node, node->conditional.if_block);
        EMIT1OP(JUMP, l_end);

        EMIT1OP(LABEL, l_else);
        ASTNode_copy_code(node, node->conditional.else_block);
    } else {
        EMIT3OP(CBR, cond_reg, l_then, l_end);
        EMIT1OP(LABEL, l_then);
        ASTNode_copy_code(node, node->conditional.if_block);
    }

    EMIT1OP(LABEL, l_end);
}

void CodeGenVisitor_gen_While(NodeVisitor* visitor, ASTNode* node)
{
    // labels
    Operand l_start = anonymous_label();
    Operand l_body  = anonymous_label();
    Operand l_exit  = anonymous_label();

    //save old loop context
    Operand old_start = DATA->current_loop_start;
    Operand old_exit  = DATA->current_loop_exit;

    // new loop context
    DATA->current_loop_start = l_start;
    DATA->current_loop_exit  = l_exit;


    EMIT1OP(LABEL, l_start);

    // condition code
    ASTNode_copy_code(node, node->whileloop.condition);
    Operand cond_reg = ASTNode_get_temp_reg(node->whileloop.condition);


    EMIT3OP(CBR, cond_reg, l_body, l_exit);

    // body code
    EMIT1OP(LABEL, l_body);
    ASTNode_copy_code(node, node->whileloop.body);


    EMIT1OP(JUMP, l_start);

    EMIT1OP(LABEL, l_exit);

    // restore old loop context
    DATA->current_loop_start = old_start;
    DATA->current_loop_exit  = old_exit;
}


void CodeGenVisitor_gen_FuncCall(NodeVisitor* visitor, ASTNode* node)
{
    const char* fname = node->funccall.name;
    NodeList* args_list = node->funccall.arguments;

    // check for built-in functions
    if (strcmp(fname, "print_int") == 0 ||
        strcmp(fname, "print_bool") == 0 ||
        strcmp(fname, "print_str") == 0)
    {
        //built in's only have 1 arg
        ASTNode* arg = args_list->head;
        ASTNode_copy_code(node, arg);

        Operand arg_reg = ASTNode_get_temp_reg(arg);

        //print
        EMIT1OP(PRINT, arg_reg);

        //return 0
        Operand rr = virtual_register();
        EMIT2OP(LOAD_I, int_const(0), rr);
        ASTNode_set_temp_reg(node, rr);
        return;
    }

    //user defined functions
    int n = args_list->size;

    // Generate arugument code
    ASTNode* curr = args_list->head;
    while (curr != NULL) {
        ASTNode_copy_code(node, curr);
        curr = curr->next;
    }

    // Put arguments into an array for easy access
    ASTNode* args[n];
    curr = args_list->head;
    int i = 0;
    while (curr != NULL) {
        args[i++] = curr;
        curr = curr->next;
    }

    // Push arguments in reverse order
    for (int k = n - 1; k >= 0; k--) {
        Operand arg_reg = ASTNode_get_temp_reg(args[k]);
        EMIT1OP(PUSH, arg_reg);
    }

    EMIT1OP(CALL, call_label(fname));

    // Pop aruments off
    if (n > 0)
        EMIT3OP(ADD_I, stack_register(), int_const(8 * n), stack_register());

    
    Operand rr = virtual_register();
    EMIT2OP(I2I, return_register(), rr);
    ASTNode_set_temp_reg(node, rr);
}

void CodeGenVisitor_gen_BinaryOP(NodeVisitor* visitor, ASTNode* node)
{
    Operand reg = virtual_register();
    ASTNode_copy_code(node,node->binaryop.left);
    ASTNode_copy_code(node,node->binaryop.right);

    Operand left_reg = ASTNode_get_temp_reg(node->binaryop.left);
    Operand right_reg = ASTNode_get_temp_reg(node->binaryop.right);
    ASTNode_set_temp_reg(node, reg);

    switch(node->binaryop.operator)
    {
        case ADDOP:
            EMIT3OP(ADD, left_reg,right_reg, reg);
            break;
        case SUBOP:
            EMIT3OP(SUB, left_reg, right_reg, reg);
            break;
        case MULOP:
            EMIT3OP(MULT, left_reg,right_reg,reg);
            break;
        case DIVOP:
            EMIT3OP(DIV, left_reg,right_reg,reg);
            break;
        case ANDOP:
            EMIT3OP(AND, left_reg, right_reg, reg);
            break;
        case OROP:
            EMIT3OP(OR, left_reg,right_reg,reg);
            break;
        case EQOP:
            EMIT3OP(CMP_EQ, left_reg, right_reg, reg);
            break;
        case NEQOP:
            EMIT3OP(CMP_NE,left_reg,right_reg,reg);
            break;
        case LTOP:
            EMIT3OP(CMP_LT, left_reg,right_reg,reg);
            break;
        case GTOP:
            EMIT3OP(CMP_GT, left_reg,right_reg,reg);
            break;
        case LEOP:
            EMIT3OP(CMP_LE,left_reg,right_reg,reg);
            break;
        case GEOP:
            EMIT3OP(CMP_GE,left_reg,right_reg,reg);
            break;
        default:
            break;
    }  
}   

void CodeGenVisitor_gen_UnaryOP(NodeVisitor* visitor, ASTNode* node)
{
    Operand reg = virtual_register();
    ASTNode_copy_code(node,node->unaryop.child);
    Operand child_reg = ASTNode_get_temp_reg(node->unaryop.child);
    ASTNode_set_temp_reg(node,reg);
    switch(node->unaryop.operator)
    {
        case NEGOP:
            EMIT2OP(NEG,child_reg,reg);
            break;
        case NOTOP:
            EMIT2OP(NOT,child_reg,reg);
            break;
    }
}

void CodeGenVisitor_gen_Assignment(NodeVisitor* visitor, ASTNode* node)
{
    ASTNode* lhs = node->assignment.location;
    ASTNode* rhs = node->assignment.value;

    Symbol* sym = lookup_symbol(node, lhs->location.name);

    // Scalar variable
    if (sym->symbol_type == SCALAR_SYMBOL)
    {
        ASTNode_copy_code(node, rhs);
        Operand rhs_reg = ASTNode_get_temp_reg(rhs);

        Operand base   = var_base(node, sym);
        Operand offset = var_offset(node, sym);

        EMIT3OP(STORE_AI, rhs_reg, base, offset);
        return;
    }

    // Array variable
    if (sym->symbol_type == ARRAY_SYMBOL)
    {
        // Index expression
        ASTNode_copy_code(node, lhs->location.index);
        Operand idx_reg = ASTNode_get_temp_reg(lhs->location.index);

        // Right-hand side value
        ASTNode_copy_code(node, rhs);
        Operand rhs_reg = ASTNode_get_temp_reg(rhs);

        // Offset size
        int elem_size = 4;  // ints = 4 bytes
        Operand scaled = virtual_register();
        EMIT3OP(MULT_I, idx_reg, int_const(elem_size), scaled);

      
        Operand base = var_base(node, sym);

        
        EMIT3OP(STORE_AO, rhs_reg, base, scaled);
        return;
    }
}

void CodeGenVisitor_gen_Break(NodeVisitor* visitor, ASTNode* node)
{
    EMIT1OP(JUMP, DATA->current_loop_exit);
}

void CodeGenVisitor_gen_Continue(NodeVisitor* visitor, ASTNode* node)
{
    EMIT1OP(JUMP, DATA->current_loop_start);
}

void CodeGenVisitor_gen_Location(NodeVisitor* visitor, ASTNode* node)
{
    Symbol* sym = lookup_symbol(node, node->location.name);

    // Scalar variable
    if (sym->symbol_type == SCALAR_SYMBOL)
    {
        Operand r = virtual_register();
        Operand rb = var_base(node, sym);
        Operand xo = var_offset(node, sym);

        EMIT3OP(LOAD_AI, rb, xo, r);

        ASTNode_set_temp_reg(node, r);
        return;
    }


    if (sym->symbol_type == ARRAY_SYMBOL)
    {
        
        ASTNode_copy_code(node, node->location.index);
        Operand re = ASTNode_get_temp_reg(node->location.index);

    
        int xs = 4;   

        
        Operand ro = virtual_register();
        EMIT3OP(MULT_I, re, int_const(xs), ro);

       
        Operand rb = var_base(node, sym);
        Operand r  = virtual_register();
        EMIT3OP(LOAD_AO, rb, ro, r);

        ASTNode_set_temp_reg(node, r);
        return;
    }
}

#endif
InsnList* generate_code (ASTNode* tree)
{
    if(tree == NULL) {
        return NULL;
    }
    else{
        InsnList* iloc = InsnList_new();


        NodeVisitor* v = NodeVisitor_new();
        v->data = CodeGenData_new();
        v->dtor = (Destructor)CodeGenData_free;
        v->postvisit_program     = CodeGenVisitor_gen_program;
        v->previsit_funcdecl     = CodeGenVisitor_previsit_funcdecl;
        v->postvisit_funcdecl    = CodeGenVisitor_gen_funcdecl;
        v->postvisit_block       = CodeGenVisitor_gen_block;
        v->postvisit_return      = CodeGenVisitor_gen_return;
        v->postvisit_literal     = CodeGenVisitor_gen_literal;
        v->postvisit_binaryop    = CodeGenVisitor_gen_BinaryOP;
        v->postvisit_location    = CodeGenVisitor_gen_Location;
        v->postvisit_assignment  = CodeGenVisitor_gen_Assignment;
        v->postvisit_unaryop     = CodeGenVisitor_gen_UnaryOP;
        v->postvisit_conditional = CodeGenVisitor_gen_Conditional;
        v->postvisit_whileloop   = CodeGenVisitor_gen_While;
        v->postvisit_funccall    = CodeGenVisitor_gen_FuncCall;
        v->postvisit_break    = CodeGenVisitor_gen_Break;
        v->postvisit_continue = CodeGenVisitor_gen_Continue;
        /* generate code into AST attributes */
        NodeVisitor_traverse_and_free(v, tree);

        /* copy generated code into new list (the AST may be deallocated before
        * the ILOC code is needed) */
        FOR_EACH(ILOCInsn*, i, (InsnList*)ASTNode_get_attribute(tree, "code")) {
            InsnList_add(iloc, ILOCInsn_copy(i));
        }
        return iloc; 
    }
}
