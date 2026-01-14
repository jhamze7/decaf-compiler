/**
 * @file p4-codegen.c
 * @brief Compiler phase 4: code generation
 */
// Authors: Jerome Donfack, Alex Boyce-Draeger
/*
    AI Statement: The use of ChatGPT was used in assitance with generating test cases and the code to
    run the test cases. CoPilot was used in order to help with auto filling code.
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

  /* Loop label stacks to support nesting */
  Operand loop_check_stack[128];
  Operand loop_body_stack[128];
  Operand loop_end_stack[128];
  int loop_sp;

  /* Per-node lvalue suppression for assignments */
  ASTNode *suppress_location;

  /* add any new desired state information (and clean it up in
   * CodeGenData_free) */
} CodeGenData;

/* Small helpers for the loop stacks */
static inline void
loop_push (CodeGenData *d, Operand chk, Operand body, Operand end)
{
  ++d->loop_sp;
  d->loop_check_stack[d->loop_sp] = chk;
  d->loop_body_stack[d->loop_sp] = body;
  d->loop_end_stack[d->loop_sp] = end;
}
static inline void
loop_pop (CodeGenData *d)
{
  --d->loop_sp;
}
static inline Operand
loop_check_top (CodeGenData *d)
{
  return d->loop_check_stack[d->loop_sp];
}
static inline Operand
loop_body_top (CodeGenData *d)
{
  return d->loop_body_stack[d->loop_sp];
}
static inline Operand
loop_end_top (CodeGenData *d)
{
  return d->loop_end_stack[d->loop_sp];
}

/**
 * @brief Allocate memory for code gen data
 *
 * @returns Pointer to allocated structure
 */
CodeGenData *
CodeGenData_new (void)
{
  CodeGenData *data = (CodeGenData *)calloc (1, sizeof (CodeGenData));
  CHECK_MALLOC_PTR (data);
  data->current_epilogue_jump_label = empty_operand ();
  data->loop_sp = -1;
  data->suppress_location = NULL;
  return data;
}

/**
 * @brief Deallocate memory for code gen data
 *
 * @param data Pointer to the structure to be deallocated
 */
void
CodeGenData_free (CodeGenData *data)
{
  /* free everything in data that is allocated on the heap */

  /* free "data" itself */
  free (data);
}

/**
 * @brief Macro for more convenient access to the error list inside a @c
 * visitor data structure
 */
#define DATA ((CodeGenData *)visitor->data)

#ifndef SKIP_IN_DOXYGEN

/*
 * Helpers for variable base/offset
 */

Operand
var_base (ASTNode *node, Symbol *variable)
{
  Operand reg = empty_operand ();
  switch (variable->location)
    {
    case STATIC_VAR:
      reg = virtual_register ();
      ASTNode_emit_insn (
          node, ILOCInsn_new_2op (LOAD_I, int_const (variable->offset), reg));
      break;
    case STACK_PARAM:
    case STACK_LOCAL:
      reg = base_register ();
      break;
    default:
      break;
    }
  return reg;
}

Operand
var_offset (ASTNode *node, Symbol *variable)
{
  Operand op = empty_operand ();
  switch (variable->location)
    {
    case STATIC_VAR:
      op = int_const (0);
      break;
    case STACK_PARAM:
    case STACK_LOCAL:
      op = int_const (variable->offset);
    default:
      break;
    }
  return op;
}

/*
 * Macros for more convenient instruction generation
 */

#define EMIT0OP(FORM) ASTNode_emit_insn (node, ILOCInsn_new_0op (FORM))
#define EMIT1OP(FORM, OP1)                                                    \
  ASTNode_emit_insn (node, ILOCInsn_new_1op (FORM, OP1))
#define EMIT2OP(FORM, OP1, OP2)                                               \
  ASTNode_emit_insn (node, ILOCInsn_new_2op (FORM, OP1, OP2))
#define EMIT3OP(FORM, OP1, OP2, OP3)                                          \
  ASTNode_emit_insn (node, ILOCInsn_new_3op (FORM, OP1, OP2, OP3))

void
CodeGenVisitor_gen_program (NodeVisitor *visitor, ASTNode *node)
{
  /*
   * make sure "code" attribute exists at the program level even if there are
   * no functions (although this shouldn't happen if static analysis is run
   * first)
   */
  ASTNode_set_attribute (node, "code", InsnList_new (),
                         (Destructor)InsnList_free);

  /* copy code from each function */
  FOR_EACH (ASTNode *, func, node->program.functions)
  {
    ASTNode_copy_code (node, func);
  }
}

void
CodeGenVisitor_previsit_funcdecl (NodeVisitor *visitor, ASTNode *node)
{
  /* generate a label reference for the epilogue that can be used while
   * generating the rest of the function (e.g., to be used when generating
   * code for a "return" statement) */
  DATA->current_epilogue_jump_label = anonymous_label ();
}

void
CodeGenVisitor_gen_funcdecl (NodeVisitor *visitor, ASTNode *node)
{
  Operand base_pointer = base_register ();
  Operand stack_pointer = stack_register ();
  int local_size = (int)ASTNode_get_int_attribute (node, "localSize");
  /* every function begins with the corresponding call label */
  EMIT1OP (LABEL, call_label (node->funcdecl.name));

  EMIT1OP (PUSH, base_pointer);
  EMIT2OP (I2I, stack_pointer, base_pointer);
  EMIT3OP (ADD_I, stack_pointer, int_const (-local_size), stack_pointer);

  /* copy code from body */
  ASTNode_copy_code (node, node->funcdecl.body);

  /* Unified epilogue label and epilogue */
  EMIT1OP (LABEL, DATA->current_epilogue_jump_label);
  EMIT2OP (I2I, base_pointer, stack_pointer);
  EMIT1OP (POP, base_pointer);
  EMIT0OP (RETURN);
}

static inline void
emit_add_sp (ASTNode *node, long bytes)
{
  if (bytes != 0)
    {
      EMIT3OP (ADD_I, stack_register (), int_const (bytes), stack_register ());
    }
}

void
CodeGenVisitor_gen_funccall (NodeVisitor *visitor, ASTNode *node)
{
  /* Built-in print_* handlers */
  if (strcmp (node->funccall.name, "print_int") == 0)
    {
      ASTNode *arg = node->funccall.arguments->head;
      ASTNode_copy_code (node, arg);
      Operand r = ASTNode_get_temp_reg (arg);
      EMIT1OP (PRINT, r);
      return;
    }
  else if (strcmp (node->funccall.name, "print_bool") == 0)
    {
      ASTNode *arg = node->funccall.arguments->head;
      ASTNode_copy_code (node, arg);
      Operand v = ASTNode_get_temp_reg (arg);
      EMIT1OP (PRINT, v);
      return;
    }

  else if (strcmp (node->funccall.name, "print_str") == 0)
    {
      EMIT1OP (PRINT,
               str_const (node->funccall.arguments->head->literal.string));
      return;
    }

  /* Normal calls: evaluate args, push R->L, call, unconditionally adjust SP */
  int arg_count = NodeList_size (node->funccall.arguments);

  if (arg_count > 0)
    {
      Operand *arg_regs = (Operand *)calloc (arg_count, sizeof (Operand));
      CHECK_MALLOC_PTR (arg_regs);

      int i = 0;
      FOR_EACH (ASTNode *, arg, node->funccall.arguments)
      {
        ASTNode_copy_code (node, arg);              /* emit child first */
        arg_regs[i++] = ASTNode_get_temp_reg (arg); /* then read temp   */
      }

      for (int j = arg_count - 1; j >= 0; --j)
        {
          EMIT1OP (PUSH, arg_regs[j]);
        }
      free (arg_regs);
    }

  EMIT1OP (CALL, call_label (node->funccall.name));

  /* Caller stack cleanup: the reference shows addI SP, 0 even for 0 args. */
  EMIT3OP (ADD_I, stack_register (), int_const (arg_count * 8),
           stack_register ());

  /* Move return register into a fresh temp and set it as this node's temp */
  DecafType return_type = (DecafType )ASTNode_get_int_attribute (node, "type");
  if(return_type != VOID) // only set temp reg if there is a return value.
  {
    Operand temp_ret_reg = virtual_register ();
    EMIT2OP (I2I, return_register (), temp_ret_reg);
    ASTNode_set_temp_reg (node, temp_ret_reg);
  }
}

void
CodeGenVisitor_gen_block (NodeVisitor *visitor, ASTNode *node)
{
  /* copy code from each statement in the block */
  FOR_EACH (ASTNode *, stmt, node->block.statements)
  {
    ASTNode_copy_code (node, stmt);
  }
}

void
CodeGenVisitor_gen_return (NodeVisitor *visitor, ASTNode *node)
{
  if (node->funcreturn.value != NULL) // only generate i2i if there is a return value.
    {
      ASTNode_copy_code (node, node->funcreturn.value);
      Operand child_reg = ASTNode_get_temp_reg (node->funcreturn.value);
      EMIT2OP (I2I, child_reg, return_register ());
    }
  EMIT1OP (JUMP, DATA->current_epilogue_jump_label);
}

void
CodeGenVisitor_previsit_assignment (NodeVisitor *visitor, ASTNode *node)
{
  /* Only suppress the lvalue LOCATION's own load; not its index. */
  DATA->suppress_location = node->assignment.location;
}

void
CodeGenVisitor_gen_assignment (NodeVisitor *visitor, ASTNode *node)
{
  Symbol *var_symbol
      = lookup_symbol (node, node->assignment.location->location.name);

  if (var_symbol->symbol_type == ARRAY_SYMBOL)
    {
      /* Emit index and value code first, then read regs */
      ASTNode_copy_code (node, node->assignment.location->location.index);
      ASTNode_copy_code (node, node->assignment.value);

      Operand index_reg
          = ASTNode_get_temp_reg (node->assignment.location->location.index);
      Operand child_reg = ASTNode_get_temp_reg (node->assignment.value);

      Operand var_base_reg = var_base (node, var_symbol);
      Operand offset_reg = virtual_register ();
      if (var_symbol->type == BOOL)
        {
          EMIT3OP (MULT_I, index_reg, int_const (8), offset_reg);
        }
      else if (var_symbol->type == INT)
        {
          EMIT3OP (MULT_I, index_reg, int_const (8), offset_reg);
        }
      EMIT3OP (STORE_AO, child_reg, var_base_reg, offset_reg);
    }
  else
    {
      /* Scalar */
      ASTNode_copy_code (node, node->assignment.value);
      Operand child_reg = ASTNode_get_temp_reg (node->assignment.value);

      Operand var_offset_op = var_offset (node, var_symbol);
      Operand var_base_reg = var_base (node, var_symbol);
      EMIT3OP (STORE_AI, child_reg, var_base_reg, var_offset_op);
    }
}

void
CodeGenVisitor_gen_location (NodeVisitor *visitor, ASTNode *node)
{
  if (DATA->suppress_location == node)
    {
      /* Skip generating a LOAD for the lvalue itself,
         but DO NOT affect the index child’s codegen. */
      DATA->suppress_location = NULL;
      return;
    }

  Symbol *var_symbol = lookup_symbol (node, node->location.name);
  Operand base_reg = var_base (node, var_symbol);
  Operand reg = virtual_register ();
  ASTNode_set_temp_reg (node, reg);

  if (var_symbol->symbol_type == SCALAR_SYMBOL) // calculate offset in stack
    {
      Operand offset_op = var_offset (node, var_symbol);
      EMIT3OP (LOAD_AI, base_reg, offset_op, reg);
    }
  else // calculate offset using array index
    {
      ASTNode_copy_code (node, node->location.index);
      Operand offset_reg = virtual_register ();
      Operand idx_reg = ASTNode_get_temp_reg (node->location.index);
      if (var_symbol->type == BOOL)
        EMIT3OP (MULT_I, idx_reg, int_const (8), offset_reg);
      else if (var_symbol->type == INT)
        EMIT3OP (MULT_I, idx_reg, int_const (8), offset_reg);
      EMIT3OP (LOAD_AO, base_reg, offset_reg, reg);
    }
}

//
/*CONDITIONALS */
//

void
CodeGenVisitor_gen_conditional (NodeVisitor *visitor, ASTNode *node)
{
  Operand if_label = anonymous_label ();
  Operand end_label = anonymous_label ();
  Operand else_label = empty_operand ();

  /* Emit condition first, then read its reg */
  ASTNode_copy_code (node, node->conditional.condition);
  Operand cond_reg = ASTNode_get_temp_reg (node->conditional.condition);

  /* condition */
  if (node->conditional.else_block != NULL) // add else label if there is an else block
    {
      else_label = end_label;
      end_label = anonymous_label ();
      EMIT3OP (CBR, cond_reg, if_label, else_label);
    }
  else
    {
      EMIT3OP (CBR, cond_reg, if_label, end_label);
    }

  /* if block */
  EMIT1OP (LABEL, if_label);
  ASTNode_copy_code (node, node->conditional.if_block);

  /* else block (if applicable) */
  if (node->conditional.else_block != NULL) // generate else block if it exists
    {
      EMIT1OP (JUMP, end_label);
      EMIT1OP (LABEL, else_label);
      ASTNode_copy_code (node, node->conditional.else_block);
    }

  /* exit */
  EMIT1OP (LABEL, end_label);
}

//
/* WHILE LOOPS (stack-based labels) */
//

void
CodeGenVisitor_previsit_whileloop (NodeVisitor *visitor, ASTNode *node)
{
  Operand check_label = anonymous_label ();
  Operand body_label = anonymous_label ();
  Operand end_label = anonymous_label ();
  loop_push (DATA, check_label, body_label, end_label);
}

void
CodeGenVisitor_gen_whileloop (NodeVisitor *visitor, ASTNode *node)
{
  Operand check_label = loop_check_top (DATA);
  Operand body_label = loop_body_top (DATA);
  Operand end_label = loop_end_top (DATA);

  EMIT1OP (LABEL, check_label);

  /* condition */
  ASTNode_copy_code (node, node->whileloop.condition);
  Operand cond_reg = ASTNode_get_temp_reg (node->whileloop.condition);
  EMIT3OP (CBR, cond_reg, body_label, end_label);

  /* body */
  EMIT1OP (LABEL, body_label);
  ASTNode_copy_code (node, node->whileloop.body);

  /* jump back */
  EMIT1OP (JUMP, check_label);

  /* exit */
  EMIT1OP (LABEL, end_label);

  loop_pop (DATA);
}

void
CodeGenVisitor_gen_break (NodeVisitor *visitor, ASTNode *node)
{
  EMIT1OP (JUMP, loop_end_top (DATA));
}

void
CodeGenVisitor_gen_continue (NodeVisitor *visitor, ASTNode *node)
{
  EMIT1OP (JUMP, loop_check_top (DATA));
}

void
unary_op_code_gen (NodeVisitor *visitor, ASTNode *node, InsnForm form)
{
  /* Emit child first, then use its temp */
  ASTNode_copy_code (node, node->unaryop.child);
  Operand child_reg = ASTNode_get_temp_reg (node->unaryop.child);
  Operand result_reg = virtual_register ();
  EMIT2OP (form, child_reg, result_reg);
  ASTNode_set_temp_reg (node, result_reg);
}

void
CodeGenVisitor_gen_unaryop (NodeVisitor *visitor, ASTNode *node)
{
  switch (node->unaryop.operator)
    {
    case NEGOP:
      unary_op_code_gen (visitor, node, NEG);
      break;
    case NOTOP:
      unary_op_code_gen (visitor, node, NOT);
      break;
    default:
      break;
    }
}

//
/* BINARY OPERATIONS */
//

static void
binary_op_emit (ASTNode *node, Operand l, Operand r, InsnForm form)
{
  Operand result_reg = virtual_register ();
  EMIT3OP (form, l, r, result_reg);
  ASTNode_set_temp_reg (node, result_reg);
}

void
binary_op_code_gen (NodeVisitor *visitor, ASTNode *node, InsnForm form)
{
  /* Emit left then right, then read regs */
  ASTNode_copy_code (node, node->binaryop.left);
  ASTNode_copy_code (node, node->binaryop.right);
  Operand left_reg = ASTNode_get_temp_reg (node->binaryop.left);
  Operand right_reg = ASTNode_get_temp_reg (node->binaryop.right);
  binary_op_emit (node, left_reg, right_reg, form);
}

void
comparison_op_code_gen (NodeVisitor *visitor, ASTNode *node, InsnForm form)
{
  ASTNode_copy_code (node, node->binaryop.left);
  ASTNode_copy_code (node, node->binaryop.right);
  Operand left_reg = ASTNode_get_temp_reg (node->binaryop.left);
  Operand right_reg = ASTNode_get_temp_reg (node->binaryop.right);
  binary_op_emit (node, left_reg, right_reg, form);
}

static void
modulus_code_gen (NodeVisitor *visitor, ASTNode *node)
{
  // a % b = a - (a / b) * b
  ASTNode_copy_code (node, node->binaryop.left);
  ASTNode_copy_code (node, node->binaryop.right);

  Operand left_reg = ASTNode_get_temp_reg (node->binaryop.left);
  Operand right_reg = ASTNode_get_temp_reg (node->binaryop.right);

  // Allocate in reference order: result first, then quotient, then product
  Operand result_reg = virtual_register ();   // will be r10 in the ref trace
  Operand quotient_reg = virtual_register (); // r11
  Operand product_reg = virtual_register ();  // r12

  EMIT3OP (DIV, left_reg, right_reg, quotient_reg);     // a/b => r11
  EMIT3OP (MULT, right_reg, quotient_reg, product_reg); // (a/b)*b => r12
  EMIT3OP (SUB, left_reg, product_reg, result_reg);     // a - (...) => r10

  ASTNode_set_temp_reg (node, result_reg); // ensure later ops (cmp_EQ) use r10
}

static void
CodeGenVisitor_gen_binaryop (NodeVisitor *visitor, ASTNode *node)
{
  switch (node->binaryop.operator)
    {
    /* Logical OR/AND (integer 0/1 semantics) */
    case OROP:
      binary_op_code_gen (visitor, node, OR);
      break;
    case ANDOP:
      binary_op_code_gen (visitor, node, AND);
      break;

    /* Equality and inequality */
    case EQOP:
      comparison_op_code_gen (visitor, node, CMP_EQ);
      break;
    case NEQOP:
      comparison_op_code_gen (visitor, node, CMP_NE);
      break;

    /* Relational comparisons */
    case LTOP:
      comparison_op_code_gen (visitor, node, CMP_LT);
      break;
    case LEOP:
      comparison_op_code_gen (visitor, node, CMP_LE);
      break;
    case GEOP:
      comparison_op_code_gen (visitor, node, CMP_GE);
      break;
    case GTOP:
      comparison_op_code_gen (visitor, node, CMP_GT);
      break;

    /* Arithmetic */
    case ADDOP:
      binary_op_code_gen (visitor, node, ADD);
      break;
    case SUBOP:
      binary_op_code_gen (visitor, node, SUB);
      break;
    case MULOP:
      binary_op_code_gen (visitor, node, MULT);
      break;
    case DIVOP:
      binary_op_code_gen (visitor, node, DIV);
      break;
    case MODOP:
      modulus_code_gen (visitor, node);
      break;

    default:
      break;
    }
}

//
/* LITERALS */
//

void
CodeGenVisitor_gen_int (NodeVisitor *visitor, ASTNode *node)
{
  Operand reg = virtual_register ();
  EMIT2OP (LOAD_I, int_const (node->literal.integer), reg);
  ASTNode_set_temp_reg (node, reg);
}

void
CodeGenVisitor_gen_bool (NodeVisitor *visitor, ASTNode *node)
{
  Operand reg = virtual_register ();
  EMIT2OP (LOAD_I, int_const (node->literal.boolean ? 1 : 0), reg);
  ASTNode_set_temp_reg (node, reg);
}

void
CodeGenVisitor_gen_literal (NodeVisitor *visitor, ASTNode *node)
{
  switch (node->literal.type)
    {
    case INT:
      CodeGenVisitor_gen_int (visitor, node);
      break;
    case BOOL:
      CodeGenVisitor_gen_bool (visitor, node);
      break;
    default:
      break;
    }
}

#endif
InsnList *
generate_code (ASTNode *tree)
{
  InsnList *iloc = InsnList_new ();

  if (tree == NULL)
    {
      return iloc;
    }

  NodeVisitor *v = NodeVisitor_new ();
  v->data = CodeGenData_new ();
  v->dtor = (Destructor)CodeGenData_free;
  v->postvisit_program = CodeGenVisitor_gen_program;
  v->previsit_funcdecl = CodeGenVisitor_previsit_funcdecl;
  v->postvisit_funcdecl = CodeGenVisitor_gen_funcdecl;
  v->postvisit_funccall = CodeGenVisitor_gen_funccall;
  v->postvisit_block = CodeGenVisitor_gen_block;
  v->postvisit_return = CodeGenVisitor_gen_return;
  v->postvisit_literal = CodeGenVisitor_gen_literal;
  v->previsit_assignment = CodeGenVisitor_previsit_assignment;
  v->postvisit_assignment = CodeGenVisitor_gen_assignment;
  v->postvisit_binaryop = CodeGenVisitor_gen_binaryop;
  v->postvisit_unaryop = CodeGenVisitor_gen_unaryop;
  v->postvisit_location = CodeGenVisitor_gen_location;

  /* while with stacks */
  v->previsit_whileloop = CodeGenVisitor_previsit_whileloop;
  v->postvisit_whileloop = CodeGenVisitor_gen_whileloop;
  v->postvisit_break = CodeGenVisitor_gen_break;
  v->postvisit_continue = CodeGenVisitor_gen_continue;

  v->postvisit_conditional = CodeGenVisitor_gen_conditional;

  /* generate code into AST attributes */
  NodeVisitor_traverse_and_free (v, tree);

  /* copy generated code into new list (the AST may be deallocated before
   * the ILOC code is needed) */
  FOR_EACH (ILOCInsn *, i, (InsnList *)ASTNode_get_attribute (tree, "code"))
  {
    InsnList_add (iloc, ILOCInsn_copy (i));
  }
  return iloc;
}
