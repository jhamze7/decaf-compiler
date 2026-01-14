/**
 *
 * @author Julian Hamze
 *
 * @file p4-codegen.c
 * @brief Compiler phase 4: code generation
 *
 * AI Statement: I used GitHub Copilot to generate code and generate testcases.
 */
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
 
     /**
      * @brief Reference to the loop end label for break statements
      */
     Operand current_loop_end_label;
 
     /**
      * @brief Reference to the loop start label for continue statements
      */
     Operand current_loop_start_label;
 
     /* add any new desired state information (and clean it up in CodeGenData_free) */
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
     data->current_loop_end_label = empty_operand();
     data->current_loop_start_label = empty_operand();
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
     ASTNode_set_attribute(node, "code", InsnList_new(), (Destructor)InsnList_free);
 
     /* copy code from each function */
     FOR_EACH(ASTNode*, func, node->program.functions) {
         ASTNode_copy_code(node, func);
     }
 }
 
 void CodeGenVisitor_previsit_funcdecl (NodeVisitor* visitor, ASTNode* node)
 {
     DATA->current_epilogue_jump_label = anonymous_label();
 }
 
 
 void CodeGenVisitor_gen_funcdecl (NodeVisitor* visitor, ASTNode* node)
 {
     // function label
     EMIT1OP(LABEL, call_label(node->funcdecl.name));
 
     // --- PROLOGUE ---
     EMIT1OP(PUSH, base_register());                  // push BP
     EMIT2OP(I2I, stack_register(), base_register()); // BP = SP
 
     int localSize = ASTNode_get_int_attribute(node, "localSize");
     // allocate space for locals: SP = SP - localSize (or SP = SP + 0 if no locals)
     EMIT3OP(ADD_I, stack_register(), int_const(-localSize), stack_register());
 
     // body
     ASTNode_copy_code(node, node->funcdecl.body);
 
     // epilogue label (for returns)
     EMIT1OP(LABEL, DATA->current_epilogue_jump_label);
 
     // --- EPILOGUE ---
     EMIT2OP(I2I, base_register(), stack_register()); // SP = BP
     EMIT1OP(POP, base_register());                   // restore BP
     EMIT0OP(RETURN);
 }
 
 void CodeGenVisitor_gen_block (NodeVisitor* visitor, ASTNode* node){
     FOR_EACH(ASTNode*, stmt, node->block.statements) {
         ASTNode_copy_code(node, stmt);
     }
 
 }
 
 void CodeGenVisitor_gen_return (NodeVisitor* visitor, ASTNode* node)
 {
     if (node->funcreturn.value) {
         ASTNode_copy_code(node, node->funcreturn.value);
         Operand val = ASTNode_get_temp_reg(node->funcreturn.value);
         EMIT2OP(I2I, val, return_register());
     }
     EMIT1OP(JUMP, DATA->current_epilogue_jump_label);
 }
 
 void CodeGenVisitor_gen_literal (NodeVisitor* visitor, ASTNode* node){
     Operand reg = virtual_register();
     if (node->literal.type == BOOL) {
         // Boolean literal: true = 1, false = 0
         EMIT2OP(LOAD_I, int_const(node->literal.boolean ? 1 : 0), reg);
     } else {
         // Integer literal
         EMIT2OP(LOAD_I, int_const(node->literal.integer), reg);
     }
     ASTNode_set_temp_reg(node, reg);
 
 }
 
 void CodeGenVisitor_gen_binaryop (NodeVisitor* visitor, ASTNode* node){
     ASTNode_copy_code(node, node->binaryop.left);
     ASTNode_copy_code(node, node->binaryop.right);
 
     Operand left = ASTNode_get_temp_reg(node->binaryop.left);
     Operand right = ASTNode_get_temp_reg(node->binaryop.right);
     Operand result_reg = virtual_register();
 
     switch (node->binaryop.operator)
     {
         
     case ADDOP:
         EMIT3OP(ADD, left, right, result_reg);
         break;
     
     case SUBOP:
         EMIT3OP(SUB, left, right, result_reg);
         break;
     
     case MULOP:
         EMIT3OP(MULT, left, right, result_reg);
         break;
     
     case DIVOP:
         EMIT3OP(DIV, left, right, result_reg);
         break;
     
     case LTOP:
         EMIT3OP(CMP_LT, left, right, result_reg);
         break;
     
     case LEOP:
         EMIT3OP(CMP_LE, left, right, result_reg);
         break;
     
     case GTOP:
         EMIT3OP(CMP_GT, left, right, result_reg);
         break;
     
     case GEOP:
         EMIT3OP(CMP_GE, left, right, result_reg);
         break;
     
     case EQOP:
         EMIT3OP(CMP_EQ, left, right, result_reg);
         break;
     
     case NEQOP:
         EMIT3OP(CMP_NE, left, right, result_reg);
         break;
     
     case ANDOP:
         EMIT3OP(AND, left, right, result_reg);
         break;
     
     case OROP:
         EMIT3OP(OR, left, right, result_reg);
         break;
     
     case MODOP: {
         // Modulus: a % b = a - (a / b) * b
         Operand div_result = virtual_register();
         EMIT3OP(DIV, left, right, div_result);
         Operand mul_result = virtual_register();
         EMIT3OP(MULT, right, div_result, mul_result);
         EMIT3OP(SUB, left, mul_result, result_reg);
         break;
     }
     
     default:
         break;
     }
 
     ASTNode_set_temp_reg(node, result_reg);
 
 }
 
 void CodeGenVisitor_gen_unaryop(NodeVisitor* visitor, ASTNode* node)
 {
     ASTNode_copy_code(node, node->unaryop.child);
     Operand val = ASTNode_get_temp_reg(node->unaryop.child);
     Operand result = virtual_register();
 
     switch (node->unaryop.operator) {
         case NEGOP:
             // Integer negation (-r1 -> r2)
             EMIT2OP(NEG, val, result);
             break;
 
         case NOTOP:
             // Boolean NOT (!r1 -> r2)
             EMIT2OP(NOT, val, result);
             break;
 
         default:
             break;
     }
 
     ASTNode_set_temp_reg(node, result);
 }
 
 void CodeGenVisitor_gen_location(NodeVisitor* visitor, ASTNode* node)
 {
     Symbol* variable = NULL;
     
     // Check if symbol attribute exists before trying to get it
     if (ASTNode_has_attribute(node, "symbol")) {
         variable = (Symbol*)ASTNode_get_attribute(node, "symbol");
     }
     
     // If not found, look it up
     if (!variable) {
         variable = lookup_symbol(node, node->location.name);
     }
     
     if (!variable) {
         return;
     }
 
     Operand result = virtual_register();
 
     // Handle array indexing if present
     if (node->location.index != NULL) {
         // Array access - need to compute address with index
         Operand base = var_base(node, variable);
         Operand offset = var_offset(node, variable);
         Operand base_addr = virtual_register();
         EMIT3OP(ADD_I, base, offset, base_addr);
         
         // Get index expression register
         ASTNode_copy_code(node, node->location.index);
         Operand index_reg = ASTNode_get_temp_reg(node->location.index);
         
         // Multiply index by WORD_SIZE
         Operand index_offset = virtual_register();
         EMIT3OP(MULT_I, index_reg, int_const(WORD_SIZE), index_offset);
         
         // Load: result = [base_addr + index_offset]
         EMIT3OP(LOAD_AO, base_addr, index_offset, result);
         
         // Store base address for assignment
         Operand* addr_box = (Operand*)calloc(1, sizeof(Operand));
         *addr_box = base_addr;
         ASTNode_set_printable_attribute(node, "addr", addr_box, reg_attr_print, free);
     } else {
         // Scalar variable: use direct offset addressing
         Operand base = var_base(node, variable);
         Operand offset = var_offset(node, variable);
         
         EMIT3OP(LOAD_AI, base, offset, result);
         
         // Store base register for assignment
         Operand* base_box = (Operand*)calloc(1, sizeof(Operand));
         *base_box = base;
         ASTNode_set_printable_attribute(node, "base", base_box, reg_attr_print, free);
         
         // Store offset for assignment
         Operand* offset_box = (Operand*)calloc(1, sizeof(Operand));
         *offset_box = offset;
         ASTNode_set_printable_attribute(node, "offset", offset_box, reg_attr_print, free);
     }
 
     // Stash value for expression nodes
     ASTNode_set_temp_reg(node, result);
 }
 
 void CodeGenVisitor_gen_assignment(NodeVisitor* visitor, ASTNode* node)
 {
     // RHS code first
     ASTNode_copy_code(node, node->assignment.value);
     Operand value = ASTNode_get_temp_reg(node->assignment.value);
 
     // get symbol directly without calling copy_code
     Symbol* variable = NULL;
     
     if (ASTNode_has_attribute(node->assignment.location, "symbol")) {
         variable = (Symbol*)ASTNode_get_attribute(node->assignment.location, "symbol");
     }
     
     if (!variable) {
         variable = lookup_symbol(node->assignment.location, node->assignment.location->location.name);
     }
     
     if (!variable) {
         return;
     }
 
     if (node->assignment.location->location.index != NULL) {
         // Array assignment
         Operand base = var_base(node, variable);
         Operand offset = var_offset(node, variable);
         Operand base_addr = virtual_register();
         EMIT3OP(ADD_I, base, offset, base_addr);
         
         ASTNode_copy_code(node, node->assignment.location->location.index);
         Operand index_reg = ASTNode_get_temp_reg(node->assignment.location->location.index);
         
         Operand index_offset = virtual_register();
         EMIT3OP(MULT_I, index_reg, int_const(WORD_SIZE), index_offset);
         
         EMIT3OP(STORE_AO, value, base_addr, index_offset);
     } else {
         // Scalar assignment
         Operand base = var_base(node, variable);
         Operand offset = var_offset(node, variable);
         
         EMIT3OP(STORE_AI, value, base, offset);
     }
 }
 
 void CodeGenVisitor_gen_conditional(NodeVisitor* visitor, ASTNode* node)
 {
     // Condition code already generated by visitor traversal
     ASTNode_copy_code(node, node->conditional.condition);
     Operand cond = ASTNode_get_temp_reg(node->conditional.condition);
     
     // Create labels
     Operand if_label = anonymous_label();
     Operand else_label = anonymous_label();
     Operand end_label = anonymous_label();
     
     // if conditional has else
     if (node->conditional.else_block != NULL) {
         EMIT3OP(CBR, cond, if_label, else_label);
         
         // If block
         EMIT1OP(LABEL, if_label);
         ASTNode_copy_code(node, node->conditional.if_block);
         EMIT1OP(JUMP, end_label);
         
         // Else block
         EMIT1OP(LABEL, else_label);
         ASTNode_copy_code(node, node->conditional.else_block);
     } else {
         EMIT3OP(CBR, cond, if_label, end_label);
         
         // If block
         EMIT1OP(LABEL, if_label);
         ASTNode_copy_code(node, node->conditional.if_block);
     }
     
     // End label
     EMIT1OP(LABEL, end_label);
 }
 
 void CodeGenVisitor_previsit_whileloop(NodeVisitor* visitor, ASTNode* node)
 {
     Operand saved_end = DATA->current_loop_end_label;
     Operand saved_start = DATA->current_loop_start_label;
     
     DATA->current_loop_end_label = anonymous_label();
     DATA->current_loop_start_label = anonymous_label();
     
     // Store saved labels in node attributes for restoration
     Operand* saved_end_ptr = (Operand*)calloc(1, sizeof(Operand));
     Operand* saved_start_ptr = (Operand*)calloc(1, sizeof(Operand));
     *saved_end_ptr = saved_end;
     *saved_start_ptr = saved_start;
     ASTNode_set_attribute(node, "_saved_loop_end", saved_end_ptr, free);
     ASTNode_set_attribute(node, "_saved_loop_start", saved_start_ptr, free);
 }
 
 void CodeGenVisitor_gen_whileloop(NodeVisitor* visitor, ASTNode* node)
 {
     Operand loop_start_label = DATA->current_loop_start_label;
     Operand loop_end_label = DATA->current_loop_end_label;
     Operand loop_body_label = anonymous_label();
     
     EMIT1OP(LABEL, loop_start_label);
     
     ASTNode_copy_code(node, node->whileloop.condition);
     Operand cond = ASTNode_get_temp_reg(node->whileloop.condition);
     
     // if condition is true, go to body, if false, go to end
     EMIT3OP(CBR, cond, loop_body_label, loop_end_label);
     
     EMIT1OP(LABEL, loop_body_label);
     ASTNode_copy_code(node, node->whileloop.body);
     
     EMIT1OP(JUMP, loop_start_label);
     
     EMIT1OP(LABEL, loop_end_label);
     
     // Restore previous loop labels
     Operand* saved_end_ptr = (Operand*)ASTNode_get_attribute(node, "_saved_loop_end");
     Operand* saved_start_ptr = (Operand*)ASTNode_get_attribute(node, "_saved_loop_start");
     if (saved_end_ptr) {
         DATA->current_loop_end_label = *saved_end_ptr;
     } else {
         DATA->current_loop_end_label = empty_operand();
     }
     if (saved_start_ptr) {
         DATA->current_loop_start_label = *saved_start_ptr;
     } else {
         DATA->current_loop_start_label = empty_operand();
     }
 }
 
 void CodeGenVisitor_gen_break(NodeVisitor* visitor, ASTNode* node)
 {
     // Jump to end of current loop
     if (DATA->current_loop_end_label.type != EMPTY) {
         EMIT1OP(JUMP, DATA->current_loop_end_label);
     }
 }
 
 void CodeGenVisitor_gen_continue(NodeVisitor* visitor, ASTNode* node)
 {
     // Jump to start of current loop
     if (DATA->current_loop_start_label.type != EMPTY) {
         EMIT1OP(JUMP, DATA->current_loop_start_label);
     }
 }
 
void CodeGenVisitor_gen_funccall(NodeVisitor* visitor, ASTNode* node)
{
    // Check if built-in print function
    bool is_print = (strcmp(node->funccall.name, "print_int") == 0 ||
                     strcmp(node->funccall.name, "print_bool") == 0 ||
                     strcmp(node->funccall.name, "print_str") == 0);
    
    // Get argument count from the list
    int arg_count = NodeList_size(node->funccall.arguments);
    
    // Allocate array for argument pointers
    ASTNode** arg_array = (ASTNode**)calloc(arg_count, sizeof(ASTNode*));
    
    // collect arguments and generate code
    int i = 0;
    FOR_EACH(ASTNode*, arg, node->funccall.arguments) {
        // Generate code for this argument
        ASTNode_copy_code(node, arg);
        // Store in array for later access
        arg_array[i++] = arg;
    }
    
    // Handle built-in print functions
    if (is_print) {
        if (arg_count > 0 && arg_array[0] != NULL) {
            Operand arg_reg = ASTNode_get_temp_reg(arg_array[0]);
            EMIT1OP(PRINT, arg_reg);
        }
        free(arg_array);
        return;
    }
    
    // Regular function call
    Operand* arg_regs = (Operand*)calloc(arg_count, sizeof(Operand));
    for (int j = 0; j < arg_count; j++) {
        arg_regs[j] = ASTNode_get_temp_reg(arg_array[j]);
    }
    
    // Push arguments in reverse order (right to left)
    for (int j = arg_count - 1; j >= 0; j--) {
        EMIT1OP(PUSH, arg_regs[j]);
    }
    
    free(arg_array);
    free(arg_regs);
    
    // Call the function
    Operand call_lbl = call_label(node->funccall.name);
    EMIT1OP(CALL, call_lbl);
    
    // emit even for 0 args
    EMIT3OP(ADD_I, stack_register(), int_const(arg_count * WORD_SIZE), stack_register());
    
    Operand result_reg = virtual_register();
    EMIT2OP(I2I, return_register(), result_reg);
    ASTNode_set_temp_reg(node, result_reg);
}
 
 #endif
 InsnList* generate_code (ASTNode* tree)
 {
     InsnList* iloc = InsnList_new();
 
     /* handle NULL tree case */
     if (tree == NULL) {
         return iloc;
     }
 
     NodeVisitor* v = NodeVisitor_new();
     v->data = CodeGenData_new();
     v->dtor = (Destructor)CodeGenData_free;
     v->postvisit_program     = CodeGenVisitor_gen_program;
     v->previsit_funcdecl     = CodeGenVisitor_previsit_funcdecl;
     v->postvisit_funcdecl    = CodeGenVisitor_gen_funcdecl;
     v->postvisit_block       = CodeGenVisitor_gen_block;
     v->postvisit_return      = CodeGenVisitor_gen_return;
     v->postvisit_binaryop    = CodeGenVisitor_gen_binaryop;
     v->postvisit_literal     = CodeGenVisitor_gen_literal;
     v->postvisit_unaryop     = CodeGenVisitor_gen_unaryop;
     v->postvisit_location    = CodeGenVisitor_gen_location;
     v->postvisit_assignment  = CodeGenVisitor_gen_assignment;
     v->postvisit_conditional = CodeGenVisitor_gen_conditional;
     v->previsit_whileloop    = CodeGenVisitor_previsit_whileloop;
     v->postvisit_whileloop   = CodeGenVisitor_gen_whileloop;
     v->postvisit_break       = CodeGenVisitor_gen_break;
     v->postvisit_continue    = CodeGenVisitor_gen_continue;
     v->postvisit_funccall    = CodeGenVisitor_gen_funccall;
 
     /* generate code into AST attributes */
     NodeVisitor_traverse_and_free(v, tree);
 
     /* copy generated code into new list */
     FOR_EACH(ILOCInsn*, i, (InsnList*)ASTNode_get_attribute(tree, "code")) {
         InsnList_add(iloc, ILOCInsn_copy(i));
     }
     
     // DEBUG: Print the ILOC directly here
     // printf("DEBUG: About to return ILOC with %d instructions\n", InsnList_size(iloc));
     // InsnList_print(iloc, stdout);
     
     return iloc;
 }