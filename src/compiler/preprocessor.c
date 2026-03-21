#include "preprocessor.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Macro {
  char* name;
  char* value;
  char** args;
  int num_args;
  struct Macro* next;
} Macro;

static Macro* macros = NULL;
static int (*underlying_getchar)(void) = NULL;
static char pending_buf[256];
static int pending_pos = 0;
static int pending_len = 0;

static void add_macro(char* name, char* value, char** args, int num_args) {
  Macro* m = (Macro*)malloc(sizeof(Macro));
  m->name = strdup(name);
  m->value = strdup(value);
  m->args = args;
  m->num_args = num_args;
  m->next = macros;
  macros = m;
}

static Macro* find_macro(const char* name, int length) {
  Macro* m = macros;
  while (m) {
    if (strlen(m->name) == (size_t)length &&
        strncmp(m->name, name, length) == 0)
      return m;
    m = m->next;
  }
  return NULL;
}

void preprocessor_init(int (*getchar_cb)(void)) {
  underlying_getchar = getchar_cb;
  pending_pos = 0;
  pending_len = 0;
}

static void append_to_pending(const char* s) {
  int len = strlen(s);
  if (pending_len + len < (int)sizeof(pending_buf)) {
    strcpy(pending_buf + pending_len, s);
    pending_len += len;
  }
}

int preprocessor_getchar(void) {
  if (pending_pos < pending_len) {
    return (unsigned char)pending_buf[pending_pos++];
  }

  pending_pos = 0;
  pending_len = 0;

  int c = underlying_getchar();
  if (c == EOF) return EOF;

  if (c == '#') {
    // Read directive
    char line[256];
    int i = 0;
    while ((c = underlying_getchar()) != '\n' && c != EOF && i < 255) {
      line[i++] = c;
    }
    line[i] = '\0';

    char* s = line;
    while (isspace(*s)) s++;
    if (strncmp(s, "define", 6) == 0) {
      s += 6;
      while (isspace(*s)) s++;
      char* name_start = s;
      while (isalnum(*s) || *s == '_') s++;
      int name_len = s - name_start;
      char* name = strndup(name_start, name_len);

      char** args = NULL;
      int num_args = 0;
      if (*s == '(') {
        s++;
        while (*s != ')' && *s != '\0') {
          while (isspace(*s) || *s == ',') s++;
          char* arg_start = s;
          while (isalnum(*s) || *s == '_') s++;
          int arg_len = s - arg_start;
          if (arg_len > 0) {
            args = (char**)realloc(args, sizeof(char*) * (num_args + 1));
            args[num_args++] = strndup(arg_start, arg_len);
          }
          while (isspace(*s)) s++;
        }
        if (*s == ')') s++;
      }

      while (isspace(*s)) s++;
      char* value = strdup(s);
      add_macro(name, value, args, num_args);
      free(name);
      free(value);
    }
    return preprocessor_getchar();  // skip the # line
  }

  if (isalpha(c) || c == '_') {
    char id[128];
    int i = 0;
    id[i++] = c;
    int next_c;

    while (isalnum(next_c = underlying_getchar()) || next_c == '_') {
      if (i < 127) id[i++] = next_c;
    }
    id[i] = '\0';

    Macro* m = find_macro(id, i);
    if (m) {
      if (m->num_args == 0) {
        append_to_pending(m->value);
        if (next_c != EOF) {
          char tmp[2] = {(char)next_c, '\0'};
          append_to_pending(tmp);
        }
      } else {
        if (next_c == '(') {
          char** call_args = NULL;
          int num_call_args = 0;
          while ((next_c = underlying_getchar()) != ')' && next_c != EOF) {
            while (isspace(next_c) || next_c == ',')
              next_c = underlying_getchar();
            if (next_c == ')' || next_c == EOF) break;

            char arg[128];
            int ai = 0;
            int parens = 0;
            while (((next_c != ',' && next_c != ')') || parens > 0) &&
                   next_c != EOF) {
              if (next_c == '(') parens++;
              if (next_c == ')') parens--;
              if (ai < 127) arg[ai++] = next_c;
              next_c = underlying_getchar();
            }
            arg[ai] = '\0';
            call_args =
                (char**)realloc(call_args, sizeof(char*) * (num_call_args + 1));
            call_args[num_call_args++] = strdup(arg);
            if (next_c == ')') break;
          }

          char* m_val = m->value;
          while (*m_val) {
            if (isalnum(*m_val) || *m_val == '_') {
              char* v_word_start = m_val;
              while (isalnum(*m_val) || *m_val == '_') m_val++;
              int v_word_len = m_val - v_word_start;
              int replaced = 0;
              for (int j = 0; j < m->num_args; j++) {
                if (strlen(m->args[j]) == (size_t)v_word_len &&
                    strncmp(m->args[j], v_word_start, v_word_len) == 0) {
                  if (j < num_call_args) append_to_pending(call_args[j]);
                  replaced = 1;
                  break;
                }
              }
              if (!replaced) {
                char tmp = *m_val;
                *m_val = '\0';
                append_to_pending(v_word_start);
                *m_val = tmp;
              }
            } else {
              char tmp[2] = {*m_val++, '\0'};
              append_to_pending(tmp);
            }
          }
          for (int j = 0; j < num_call_args; j++) free(call_args[j]);
          free(call_args);
          return preprocessor_getchar();
        } else {
          append_to_pending(id);
          if (next_c != EOF) {
            char tmp[2] = {(char)next_c, '\0'};
            append_to_pending(tmp);
          }
        }
      }
      return preprocessor_getchar();
    } else {
      append_to_pending(id);
      if (next_c != EOF) {
        char tmp[2] = {(char)next_c, '\0'};
        append_to_pending(tmp);
      }
      return preprocessor_getchar();
    }
  }

  return c;
}

static const char* global_src_ptr;
static int string_getchar() {
  return *global_src_ptr ? (unsigned char)*global_src_ptr++ : EOF;
}

char* preprocess(const char* source) {
  global_src_ptr = source;
  preprocessor_init(string_getchar);

  int capacity = strlen(source) * 2 + 1024;
  char* result = malloc(capacity);
  int i = 0;
  int c;
  while ((c = preprocessor_getchar()) != EOF) {
    if (i + 1 >= capacity) {
      capacity *= 2;
      result = realloc(result, capacity);
    }
    result[i++] = c;
  }
  result[i] = '\0';
  return result;
}
