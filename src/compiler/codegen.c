#include "codegen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"

typedef struct Symbol {
  char* name;
  int offset;
  TypeInfo* type;
  struct Symbol* next;
} Symbol;

typedef struct Member {
  char* name;
  int offset;
  TypeInfo* type;
  struct Member* next;
} Member;

typedef struct StructDef {
  char* name;
  Member* members;
  int size;
  struct StructDef* next;
} StructDef;

typedef struct {
  Symbol* symbols;
  StructDef* structs;
  int stack_offset;
  int label_count;
  char* current_func_end_label;
} CodegenState;

static CodegenState state;

static void (*emit_cb)(const char*) = NULL;

void codegen_set_emit_cb(void (*cb)(const char*)) { emit_cb = cb; }

#define EMIT(...)                                \
  do {                                           \
    char __buf[128];                             \
    snprintf(__buf, sizeof(__buf), __VA_ARGS__); \
    if (emit_cb) emit_cb(__buf);                 \
  } while (0)

static void visit_struct_decl(ASTNode* node) {
  StructDef* sd = (StructDef*)calloc(1, sizeof(StructDef));
  sd->name = intern_string(node->as.decl.name);
  int offset = 0;

  ASTNode* mem_node = node->first_child;
  while (mem_node) {
    Member* m = (Member*)calloc(1, sizeof(Member));
    m->name = intern_string(mem_node->as.decl.name);
    m->offset = offset;
    m->type = ast_clone_type(mem_node->as.decl.var_type);

    int element_size = 1;
    if (m->type && (m->type->base == TYPE_UINT16 || m->type->base == TYPE_INT ||
                    m->type->base == TYPE_POINTER))
      element_size = 2;

    offset += element_size * mem_node->as.decl.array_size;
    m->next = sd->members;
    sd->members = m;

    mem_node = mem_node->next_sibling;
  }
  sd->size = offset;
  sd->next = state.structs;
  state.structs = sd;
}

static StructDef* find_struct(char* name) {
  StructDef* s = state.structs;
  while (s) {
    if (strcmp(s->name, name) == 0) return s;
    s = s->next;
  }
  return NULL;
}

static Member* find_member(StructDef* sd, char* name) {
  Member* m = sd->members;
  while (m) {
    if (strcmp(m->name, name) == 0) return m;
    m = m->next;
  }
  return NULL;
}

static void push_symbol(char* name, int offset, TypeInfo* type) {
  Symbol* s = (Symbol*)malloc(sizeof(Symbol));
  s->name = intern_string(name);
  s->offset = offset;
  s->type = type;
  s->next = state.symbols;
  state.symbols = s;
}

static Symbol* find_symbol(char* name) {
  Symbol* s = state.symbols;
  while (s) {
    if (strcmp(s->name, name) == 0) return s;
    s = s->next;
  }
  return NULL;
}

static void clear_symbols() {
  Symbol* s = state.symbols;
  while (s) {
    Symbol* next = s->next;
    free(s);
    s = next;
  }
  state.symbols = NULL;
}

static int next_label() { return state.label_count++; }

static void visit(ASTNode* node);

static void visit_program(ASTNode* node) {
  ASTNode* child = node->first_child;
  while (child) {
    visit(child);
    child = child->next_sibling;
  }
}

static int calculate_stack_size(ASTNode* node) {
  if (!node) return 0;
  int size = 0;
  if (node->type == AST_VAR_DECL) {
    size = 1;
    if (node->as.decl.var_type &&
        (node->as.decl.var_type->base == TYPE_UINT16 ||
         node->as.decl.var_type->base == TYPE_INT ||
         node->as.decl.var_type->base == TYPE_POINTER))
      size = 2;
  }

  ASTNode* child = node->first_child;
  while (child) {
    size += calculate_stack_size(child);
    child = child->next_sibling;
  }

  if (node->type == AST_FUNC_DECL && node->as.decl.body)
    size += calculate_stack_size(node->as.decl.body);
  if (node->type == AST_IF) {
    if (node->as.if_stmt.then_branch)
      size += calculate_stack_size(node->as.if_stmt.then_branch);
    if (node->as.if_stmt.else_branch)
      size += calculate_stack_size(node->as.if_stmt.else_branch);
  }
  if ((node->type == AST_WHILE || node->type == AST_FOR) && node->as.loop.body)
    size += calculate_stack_size(node->as.loop.body);
  if (node->type == AST_FOR && node->as.loop.init &&
      node->as.loop.init->type == AST_VAR_DECL)
    size += calculate_stack_size(node->as.loop.init);

  return size;
}

static void visit_func_decl(ASTNode* node) {
  clear_symbols();
  int total_stack = calculate_stack_size(node->as.decl.body);

  ASTNode* child = node->first_child;
  while (child) {
    total_stack += calculate_stack_size(child);
    child = child->next_sibling;
  }

  state.stack_offset = 1;
  char end_label[32];
  sprintf(end_label, "L_end_%s", node->as.decl.name);
  state.current_func_end_label = end_label;

  EMIT(".global %s\n", node->as.decl.name);
  EMIT("%s:\n", node->as.decl.name);

  // Function Prologue
  EMIT("    push r28\n");
  EMIT("    push r29\n");

  if (total_stack > 0) {
    EMIT("    in r28, 0x3D ; SPL\n");
    EMIT("    in r29, 0x3E ; SPH\n");
    EMIT("    sbiw r28, %d\n", total_stack);
    EMIT("    out 0x3D, r28\n");
    EMIT("    out 0x3E, r29\n");
  } else {
    EMIT("    in r28, 0x3D\n");
    EMIT("    in r29, 0x3E\n");
  }

  // Load parameters into stack frame
  ASTNode* param = node->first_child;
  int arg_idx = 0;
  while (param) {
    int reg = 24 - (arg_idx * 2);
    int size = 1;
    if (param->as.decl.var_type &&
        (param->as.decl.var_type->base == TYPE_UINT16 ||
         param->as.decl.var_type->base == TYPE_INT ||
         param->as.decl.var_type->base == TYPE_POINTER))
      size = 2;
    push_symbol(
        param->as.decl.name, state.stack_offset, param->as.decl.var_type
    );
    if (size == 1) {
      EMIT("    std Y+%d, r%d\n", state.stack_offset, reg);
    } else {
      EMIT("    std Y+%d, r%d\n", state.stack_offset, reg);
      EMIT("    std Y+%d, r%d\n", state.stack_offset + 1, reg + 1);
    }
    state.stack_offset += size;
    arg_idx++;
    param = param->next_sibling;
  }

  // Body
  if (node->as.decl.body) {
    visit(node->as.decl.body);
  }

  // Epilogue
  EMIT("%s:\n", end_label);
  if (total_stack > 0) {
    EMIT("    adiw r28, %d\n", total_stack);
    EMIT("    out 0x3D, r28\n");
    EMIT("    out 0x3E, r29\n");
  }

  EMIT("    pop r29\n");
  EMIT("    pop r28\n");
  EMIT("    ret\n\n");
}

static void visit_var_decl(ASTNode* node) {
  int size = 1;
  if (node->as.decl.var_type && (node->as.decl.var_type->base == TYPE_UINT16 ||
                                 node->as.decl.var_type->base == TYPE_INT ||
                                 node->as.decl.var_type->base == TYPE_POINTER))
    size = 2;

  push_symbol(node->as.decl.name, state.stack_offset, node->as.decl.var_type);
  state.stack_offset += size;

  if (node->first_child) {
    visit(node->first_child);
  }
}

static void visit_var_access(ASTNode* node) {
  Symbol* s = find_symbol(node->as.var.name);
  if (!s) {
    fprintf(
        stderr, "Codegen error: variable %s not found\n", node->as.var.name
    );
    return;
  }

  int size = 1;
  if (s->type && (s->type->base == TYPE_UINT16 || s->type->base == TYPE_INT ||
                  s->type->base == TYPE_POINTER))
    size = 2;

  if (size == 1) {
    EMIT("    ldd r24, Y+%d\n", s->offset);
    EMIT("    clr r25\n");
  } else {
    EMIT("    ldd r24, Y+%d\n", s->offset);
    EMIT("    ldd r25, Y+%d\n", s->offset + 1);
  }
}

static TypeInfo* get_expr_type(ASTNode* node) {
  if (!node) return NULL;
  switch (node->type) {
    case AST_VAR_ACCESS: {
      Symbol* s = find_symbol(node->as.var.name);
      if (s) return s->type;
      break;
    }
    case AST_CAST:
      return node->as.single_expr.var_type;
    case AST_MEMBER_ACCESS: {
      TypeInfo* t = get_expr_type(node->as.member.expr);
      if (t) {
        if (t->base == TYPE_POINTER)
          t = t->ptr_to;  // Handle ptr->member (though we only have . for now)
        if (t->base == TYPE_STRUCT) {
          StructDef* sd = find_struct(t->struct_name);
          if (sd) {
            Member* m = find_member(sd, node->as.member.member_name);
            if (m) return m->type;
          }
        }
      }
      break;
    }
    case AST_UNARY_OP:
      if (node->as.single_expr.op == TOK_STAR) {
        TypeInfo* t = get_expr_type(node->as.single_expr.expr);
        if (t && t->base == TYPE_POINTER) return t->ptr_to;
        if (t && t->base == TYPE_STRUCT)
          return t;  // Lenient handling for the user's specific syntax
      }
      break;
    default:
      break;
  }
  return NULL;
}

static void visit_lvalue(ASTNode* node) {
  if (node->type == AST_VAR_ACCESS) {
    Symbol* s = find_symbol(node->as.var.name);
    if (s) {
      EMIT("    movw r30, r28\n");
      if (s->offset > 0) EMIT("    adiw r30, %d\n", s->offset);
    } else {
      EMIT("    ldi r30, lo8(%s)\n", node->as.var.name);
      EMIT("    ldi r31, hi8(%s)\n", node->as.var.name);
    }
  } else if (node->type == AST_UNARY_OP &&
             node->as.single_expr.op == TOK_STAR) {
    visit(node->as.single_expr.expr);
    EMIT("    movw r30, r24\n");
  } else if (node->type == AST_MEMBER_ACCESS) {
    TypeInfo* t = get_expr_type(node->as.member.expr);
    visit_lvalue(node->as.member.expr);  // Get base address in Z
    if (t && t->base == TYPE_STRUCT) {
      StructDef* sd = find_struct(t->struct_name);
      if (sd) {
        Member* m = find_member(sd, node->as.member.member_name);
        if (m && m->offset > 0) {
          EMIT("    adiw r30, %d\n", m->offset);
        }
      }
    }
  }
}

static void visit_assign(ASTNode* node) {
  visit(node->as.binary.right);  // Result in r24:r25
  EMIT("    push r24\n");
  EMIT("    push r25\n");

  visit_lvalue(node->as.binary.left);  // Address in Z (r30:r31)

  EMIT("    pop r25\n");
  EMIT("    pop r24\n");

  TypeInfo* t = get_expr_type(node->as.binary.left);
  int size = 1;
  if (t && (t->base == TYPE_UINT16 || t->base == TYPE_INT ||
            t->base == TYPE_POINTER))
    size = 2;

  if (size == 1) {
    EMIT("    st Z, r24\n");
  } else {
    EMIT("    st Z, r24\n");
    EMIT("    std Z+1, r25\n");
  }
}

static void visit_unary_op(ASTNode* node) {
  if (node->as.single_expr.op == TOK_STAR) {
    visit(node->as.single_expr.expr);  // address in r24:r25
    EMIT("    movw r30, r24\n");
    TypeInfo* t = get_expr_type(node);
    int size = 1;
    if (t && (t->base == TYPE_UINT16 || t->base == TYPE_INT ||
              t->base == TYPE_POINTER))
      size = 2;
    if (size == 1) {
      EMIT("    ld r24, Z\n");
      EMIT("    clr r25\n");
    } else {
      EMIT("    ld r24, Z\n");
      EMIT("    ldd r25, Z+1\n");
    }
  } else if (node->as.single_expr.op == TOK_AMP) {
    visit_lvalue(node->as.single_expr.expr);
    EMIT("    movw r24, r30\n");
  } else if (node->as.single_expr.op == TOK_INC ||
             node->as.single_expr.op == TOK_DEC) {
    visit_lvalue(node->as.single_expr.expr);  // address in Z
    TypeInfo* t = get_expr_type(node->as.single_expr.expr);
    int size = 1;
    if (t && (t->base == TYPE_UINT16 || t->base == TYPE_INT ||
              t->base == TYPE_POINTER))
      size = 2;

    if (size == 1) {
      EMIT("    ld r24, Z\n");
      if (node->as.single_expr.op == TOK_INC)
        EMIT("    inc r24\n");
      else
        EMIT("    dec r24\n");
      EMIT("    st Z, r24\n");
    } else {
      EMIT("    ld r24, Z\n");
      EMIT("    ldd r25, Z+1\n");
      if (node->as.single_expr.op == TOK_INC) {
        EMIT("    adiw r24, 1\n");
      } else {
        EMIT("    sbiw r24, 1\n");
      }
      EMIT("    st Z, r24\n");
      EMIT("    std Z+1, r25\n");
    }
  } else if (node->as.single_expr.op == TOK_NOT) {
    visit(node->as.single_expr.expr);
    int l_true = next_label();
    int l_done = next_label();
    EMIT("    or r24, r25\n");
    EMIT("    breq L%d\n", l_true);
    EMIT("    ldi r24, 0\n");
    EMIT("    rjmp L%d\n", l_done);
    EMIT("L%d:\n", l_true);
    EMIT("    ldi r24, 1\n");
    EMIT("L%d:\n", l_done);
    EMIT("    clr r25\n");
  }
}

static void visit_cast(ASTNode* node) { visit(node->as.single_expr.expr); }

static void visit_if(ASTNode* node) {
  int l_end = next_label();
  int l_else = node->as.if_stmt.else_branch ? next_label() : l_end;

  visit(node->as.if_stmt.cond);
  EMIT("    tst r24\n");
  EMIT("    breq L%d\n", l_else);

  visit(node->as.if_stmt.then_branch);

  if (node->as.if_stmt.else_branch) {
    EMIT("    rjmp L%d\n", l_end);
    EMIT("L%d:\n", l_else);
    visit(node->as.if_stmt.else_branch);
  }
  EMIT("L%d:\n", l_end);
}

static void visit_while(ASTNode* node) {
  int l_start = next_label();
  int l_end = next_label();

  EMIT("L%d:\n", l_start);
  visit(node->as.loop.cond);
  // If r24 is 0, jump to end
  EMIT("    tst r24\n");
  EMIT("    breq L%d\n", l_end);

  visit(node->as.loop.body);
  EMIT("    rjmp L%d\n", l_start);
  EMIT("L%d:\n", l_end);
}

static void visit_for(ASTNode* node) {
  if (node->as.loop.init) visit(node->as.loop.init);

  int l_start = next_label();
  int l_end = next_label();

  EMIT("L%d:\n", l_start);
  if (node->as.loop.cond) {
    visit(node->as.loop.cond);
    EMIT("    tst r24\n");
    EMIT("    breq L%d\n", l_end);
  }

  visit(node->as.loop.body);
  if (node->as.loop.inc) visit(node->as.loop.inc);

  EMIT("    rjmp L%d\n", l_start);
  EMIT("L%d:\n", l_end);
}

static void visit_binary_op(ASTNode* node) {
  visit(node->as.binary.right);
  EMIT("    push r24\n");
  EMIT("    push r25\n");

  visit(node->as.binary.left);
  EMIT("    pop r23\n");  // Right high
  EMIT("    pop r22\n");  // Right low

  switch (node->as.binary.op) {
    case TOK_PLUS:
      EMIT("    add r24, r22\n");
      EMIT("    adc r25, r23\n");
      break;
    case TOK_MINUS:
      EMIT("    sub r24, r22\n");
      EMIT("    sbc r25, r23\n");
      break;
    case TOK_LSHIFT: {
      int l_loop = next_label();
      int l_done = next_label();
      EMIT("    tst r22\n");
      EMIT("    breq L%d\n", l_done);
      EMIT("L%d:\n", l_loop);
      EMIT("    lsl r24\n");
      EMIT("    rol r25\n");
      EMIT("    dec r22\n");
      EMIT("    brne L%d\n", l_loop);
      EMIT("L%d:\n", l_done);
      break;
    }
    case TOK_LT: {
      int l_true = next_label();
      int l_done = next_label();
      EMIT("    cp r24, r22\n");
      EMIT("    cpc r25, r23\n");
      EMIT("    brlt L%d\n", l_true);
      EMIT("    ldi r24, 0\n");
      EMIT("    rjmp L%d\n", l_done);
      EMIT("L%d:\n", l_true);
      EMIT("    ldi r24, 1\n");
      EMIT("L%d:\n", l_done);
      EMIT("    clr r25\n");
      break;
    }
    case TOK_GT: {
      int l_true = next_label();
      int l_done = next_label();
      EMIT("    cp r22, r24\n");
      EMIT("    cpc r23, r25\n");
      EMIT("    brlt L%d\n", l_true);
      EMIT("    ldi r24, 0\n");
      EMIT("    rjmp L%d\n", l_done);
      EMIT("L%d:\n", l_true);
      EMIT("    ldi r24, 1\n");
      EMIT("L%d:\n", l_done);
      EMIT("    clr r25\n");
      break;
    }
    case TOK_LE: {
      int l_true = next_label();
      int l_done = next_label();
      EMIT("    cp r22, r24\n");
      EMIT("    cpc r23, r25\n");
      EMIT("    brge L%d\n", l_true);
      EMIT("    ldi r24, 0\n");
      EMIT("    rjmp L%d\n", l_done);
      EMIT("L%d:\n", l_true);
      EMIT("    ldi r24, 1\n");
      EMIT("L%d:\n", l_done);
      EMIT("    clr r25\n");
      break;
    }
    case TOK_GE: {
      int l_true = next_label();
      int l_done = next_label();
      EMIT("    cp r24, r22\n");
      EMIT("    cpc r25, r23\n");
      EMIT("    brge L%d\n", l_true);
      EMIT("    ldi r24, 0\n");
      EMIT("    rjmp L%d\n", l_done);
      EMIT("L%d:\n", l_true);
      EMIT("    ldi r24, 1\n");
      EMIT("L%d:\n", l_done);
      EMIT("    clr r25\n");
      break;
    }
    case TOK_EQ: {
      int l_true = next_label();
      int l_done = next_label();
      EMIT("    cp r24, r22\n");
      EMIT("    cpc r25, r23\n");
      EMIT("    breq L%d\n", l_true);
      EMIT("    ldi r24, 0\n");
      EMIT("    rjmp L%d\n", l_done);
      EMIT("L%d:\n", l_true);
      EMIT("    ldi r24, 1\n");
      EMIT("L%d:\n", l_done);
      EMIT("    clr r25\n");
      break;
    }
    case TOK_NEQ: {
      int l_true = next_label();
      int l_done = next_label();
      EMIT("    cp r24, r22\n");
      EMIT("    cpc r25, r23\n");
      EMIT("    brne L%d\n", l_true);
      EMIT("    ldi r24, 0\n");
      EMIT("    rjmp L%d\n", l_done);
      EMIT("L%d:\n", l_true);
      EMIT("    ldi r24, 1\n");
      EMIT("L%d:\n", l_done);
      EMIT("    clr r25\n");
      break;
    }
  }
}

static void visit_block(ASTNode* node) {
  ASTNode* child = node->first_child;
  while (child) {
    visit(child);
    child = child->next_sibling;
  }
}

static void visit_number(ASTNode* node) {
  EMIT("    ldi r24, 0x%02X\n", node->as.number.int_val & 0xFF);
  EMIT("    ldi r25, 0x%02X\n", (node->as.number.int_val >> 8) & 0xFF);
}

static void visit_return(ASTNode* node) {
  if (node->as.single_expr.expr) {
    visit(node->as.single_expr.expr);
  }
  EMIT("    rjmp %s\n", state.current_func_end_label);
}

static void visit_expr_stmt(ASTNode* node) {
  if (node->as.single_expr.expr) visit(node->as.single_expr.expr);
}

static void visit_func_call(ASTNode* node) {
  ASTNode* arg = node->first_child;
  int num_args = 0;
  while (arg) {
    visit(arg);
    EMIT("    push r24\n");
    EMIT("    push r25\n");
    num_args++;
    arg = arg->next_sibling;
  }

  for (int i = num_args - 1; i >= 0; i--) {
    int reg = 24 - (i * 2);
    if (reg < 8) {
      fprintf(
          stderr, "Codegen error: Too many arguments for %s\n",
          node->as.call.name
      );
      exit(1);
    }
    EMIT("    pop r%d\n", reg + 1);
    EMIT("    pop r%d\n", reg);
  }

  int l_get_pc = next_label();
  EMIT("    rcall L_get_pc_%d\n", l_get_pc);
  EMIT("L_get_pc_%d:\n", l_get_pc);
  EMIT("    pop r31\n");
  EMIT("    pop r30\n");
  EMIT(
      "    ldi r26, lo8(pm(%s - L_get_pc_%d))\n", node->as.call.name, l_get_pc
  );
  EMIT(
      "    ldi r27, hi8(pm(%s - L_get_pc_%d))\n", node->as.call.name, l_get_pc
  );
  EMIT("    add r30, r26\n");
  EMIT("    adc r31, r27\n");
  EMIT("    icall\n");
}

static void visit(ASTNode* node) {
  if (!node) return;
  switch (node->type) {
    case AST_PROGRAM:
      visit_program(node);
      break;
    case AST_FUNC_DECL:
      visit_func_decl(node);
      break;
    case AST_VAR_DECL:
      visit_var_decl(node);
      break;
    case AST_BLOCK:
      visit_block(node);
      break;
    case AST_EXPR_STMT:
      visit_expr_stmt(node);
      break;
    case AST_IF:
      visit_if(node);
      break;
    case AST_WHILE:
      visit_while(node);
      break;
    case AST_FOR:
      visit_for(node);
      break;
    case AST_RETURN:
      visit_return(node);
      break;
    case AST_ASSIGN:
      visit_assign(node);
      break;
    case AST_MEMBER_ACCESS: {
      visit_lvalue(node);
      TypeInfo* t = get_expr_type(node);
      int size = 1;
      if (t && (t->base == TYPE_UINT16 || t->base == TYPE_INT ||
                t->base == TYPE_POINTER))
        size = 2;
      if (size == 1) {
        EMIT("    ld r24, Z\n");
        EMIT("    clr r25\n");
      } else {
        EMIT("    ld r24, Z\n");
        EMIT("    ldd r25, Z+1\n");
      }
      break;
    }
    case AST_UNARY_OP:
      visit_unary_op(node);
      break;
    case AST_CAST:
      visit_cast(node);
      break;
    case AST_VAR_ACCESS:
      visit_var_access(node);
      break;
    case AST_NUMBER:
      visit_number(node);
      break;
    case AST_BINARY_OP:
      visit_binary_op(node);
      break;
    case AST_STRUCT_DECL:
      visit_struct_decl(node);
      break;
    case AST_FUNC_CALL:
      visit_func_call(node);
      break;
    default:
      fprintf(
          stderr, "Codegen: Unsupported node type %d (line %d)\n", node->type,
          node->line
      );
      break;
  }
}

void codegen_prologue(void) {
  state.label_count = 0;
  state.symbols = NULL;

  EMIT("; Setup Stack Pointer\n");
  EMIT("    ldi r28, 0xFF\n");
  EMIT("    ldi r29, 0x7F\n");
  EMIT("    out 0x3D, r28 ; SPL\n");
  EMIT("    out 0x3E, r29 ; SPH\n\n");
  int l_get_pc = next_label();
  EMIT("    rcall L_get_pc_%d\n", l_get_pc);
  EMIT("L_get_pc_%d:\n", l_get_pc);
  EMIT("    pop r31\n");
  EMIT("    pop r30\n");
  EMIT("    ldi r26, lo8(pm(main - L_get_pc_%d))\n", l_get_pc);
  EMIT("    ldi r27, hi8(pm(main - L_get_pc_%d))\n", l_get_pc);
  EMIT("    add r30, r26\n");
  EMIT("    adc r31, r27\n");
  EMIT("    icall\n");
  EMIT("L_inf_loop:\n");
  EMIT("    rjmp L_inf_loop\n\n");
}

void codegen_top_level(ASTNode* node) { visit(node); }

void codegen(ASTNode* node) {
  codegen_prologue();
  if (node && node->type == AST_PROGRAM) {
    ASTNode* child = node->first_child;
    while (child) {
      visit(child);
      child = child->next_sibling;
    }
  } else {
    visit(node);
  }
}
