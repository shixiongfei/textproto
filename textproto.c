/*
 * textproto.c
 *
 * Copyright (c) 2022 Xiongfei Shi
 *
 * Author: Xiongfei Shi <xiongfei.shi(a)icloud.com>
 * License: Apache-2.0
 *
 * https://github.com/shixiongfei/textproto
 */

#include "textproto.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TP_ENDLN_LEN 2
#define TP_STACK_SIZE 8

struct textproto_t {
  int err;
  int elements;    /* number of elements in multibulk container */
  int stack_index; /* Index of current read task */
  int stack_size;
  int array_size;
  int top_size;
  int offset;
  int len;
  char *buf;
  textproto_element_t **stack_protos;
};

static void *alloc_emul(void *ptr, size_t size) {
  if (size)
    return realloc(ptr, size);
  free(ptr);
  return NULL;
}

static void *(*tp_realloc)(void *, size_t) = alloc_emul;

#define tp_malloc(size) tp_realloc(NULL, size)
#define tp_free(ptr) tp_realloc(ptr, 0)

void textproto_setalloc(void *(*allocator)(void *, size_t)) {
  tp_realloc = allocator ? allocator : alloc_emul;
}

textproto_t *textproto_create(void) {
  textproto_t *tp = (textproto_t *)tp_malloc(sizeof(textproto_t));

  if (!tp)
    return NULL;

  memset(tp, 0, sizeof(textproto_t));
  tp->stack_index = -1;

  return tp;
}

void textproto_destroy(textproto_t *tp) {
  if (tp->stack_protos) {
    if (tp->stack_index > 0)
      while (tp->stack_index-- > 0)
        textproto_element_destroy(tp->stack_protos[tp->stack_index]);

    tp_free(tp->stack_protos);
  }
  tp_free(tp);
}

void textproto_element_destroy(textproto_element_t *elem) {
  int i;

  if (!elem)
    return;

  if (elem->elements) {
    for (i = 0; i < elem->elem_count; ++i)
      textproto_element_destroy(elem->elements[i]);

    tp_free(elem->elements);
  }
  tp_free(elem);
}

static char *tp_seek_newline(char *p, int len) {
  int pos = 0;

  len -= 1;
  while (pos < len) {
    while ((pos < len) && ('\r' != p[pos]))
      pos += 1;

    if ('\n' == p[pos + 1])
      return p + pos;

    pos += 1;
  }
  return NULL;
}

static int tp_check_numeric(char *p, int len) {
  while ((len--) > 0) {
    if ((p[len] < '0') || (p[len] > '9')) {
      if ((0 == len) && (('-' == p[len]) || ('+' == p[len])))
        return 1;

      return 0;
    }
  }
  return 1;
}

static int64_t tp_read_int64(char *p, int len) {
  char num[32] = {0};
  strncpy(num, p, len);
  return strtoll(num, NULL, 10);
}

static int tp_stack_expand_if_needed(textproto_t *tp) {
  /* check stack size */
  if (tp->stack_index >= tp->stack_size) {
    textproto_element_t **new_stack;
    int i;

    tp->stack_size = tp->stack_size ? (tp->stack_size << 1) : TP_STACK_SIZE;
    new_stack = (textproto_element_t **)tp_malloc(
        tp->stack_size * sizeof(textproto_element_t *));

    if (tp->stack_protos) {
      for (i = 0; i < tp->stack_index; ++i)
        new_stack[i] = tp->stack_protos[i];

      tp_free(tp->stack_protos);
    }

    tp->stack_protos = new_stack;
  }
  return 0;
}

static int tp_parse_bulk(textproto_t *tp) {
  textproto_element_t *root = NULL;
  int bytelen;
  int bulk_size;
  int is_array = 0;

  char *p = tp->buf + tp->offset;
  char *s = tp_seek_newline(p, tp->len - tp->offset);

  if (!s) {
    tp->err = TEXTPROTO_MORE;
    return TEXTPROTO_MORE;
  }

  bytelen = (int)(s - p) + TP_ENDLN_LEN;

  /* command finished */
  if (TP_ENDLN_LEN == bytelen) {
    /* check proto is integrity */
    if (tp->array_size > 0) {
      tp->err = TEXTPROTO_ERROR;
      return TEXTPROTO_ERROR;
    }

    tp->elements = tp->stack_index;
    tp->stack_index = -1;

    tp->offset += bytelen;

    tp->err = TEXTPROTO_OK;
    return TEXTPROTO_OK;
  }

  if (!tp_check_numeric(p, bytelen - TP_ENDLN_LEN)) {
    /* the first elements can not be an array */
    if ((0 == tp->offset) || ('*' != p[0])) {
      tp->err = TEXTPROTO_ERROR;
      return TEXTPROTO_ERROR;
    }

    /* parse array */
    if (!tp_check_numeric(p + 1, bytelen - 1 - TP_ENDLN_LEN)) {
      tp->err = TEXTPROTO_ERROR;
      return TEXTPROTO_ERROR;
    }

    is_array = 1;
  }

  bulk_size =
      (int)tp_read_int64(p + is_array, bytelen - is_array - TP_ENDLN_LEN);

  if (bulk_size <= 0) {
    root = (textproto_element_t *)tp_malloc(sizeof(textproto_element_t));

    root->elem_count = -1;
    root->elements = NULL;
    root->size = (0 == bulk_size) ? 0 : (-1);
    root->data[0] = 0;
  } else {
    if (!is_array) {
      bytelen += (bulk_size + TP_ENDLN_LEN);

      if ((tp->offset + bytelen) <= tp->len) {
        root = (textproto_element_t *)tp_malloc(sizeof(textproto_element_t) +
                                                bulk_size);
        root->elem_count = -1;
        root->elements = NULL;
        root->size = bulk_size;
        root->data[bulk_size] = 0;
        memcpy(root->data, s + TP_ENDLN_LEN, bulk_size);
      }
    } else {
      root = (textproto_element_t *)tp_malloc(sizeof(textproto_element_t));

      root->elem_count = bulk_size;
      root->elements = (textproto_element_t **)tp_malloc(
          sizeof(textproto_element_t *) * root->elem_count);
      root->size = -1;
      root->data[0] = 0;
    }
  }

  if (!root) {
    tp->err = TEXTPROTO_MORE;
    return TEXTPROTO_MORE;
  }

  if (0 == tp->array_size)
    tp->top_size += 1;
  else
    tp->array_size -= 1;

  if (is_array)
    tp->array_size += root->elem_count;

  tp_stack_expand_if_needed(tp);

  tp->stack_protos[tp->stack_index++] = root;
  tp->offset += bytelen;

  tp->err = TEXTPROTO_OK;
  return TEXTPROTO_OK;
}

static int textproto_protocol(textproto_element_t *root,
                              textproto_element_t **protos) {
  int i, j;

  for (i = 0, j = 0; i < root->elem_count; ++i, ++j) {
    root->elements[i] = protos[j];

    if (root->elements[i]->elem_count > 0)
      j += textproto_protocol(root->elements[i], protos + j + 1);
  }
  return j;
}

int textproto_parse(textproto_t *tp, textproto_element_t **root,
                    const char *protocol, int *len) {
  if ((TEXTPROTO_ERROR == tp->err) || (NULL == len))
    return TEXTPROTO_ERROR;

  if (0 == *len)
    return TEXTPROTO_MORE;

  if (-1 == tp->stack_index) {
    tp->elements = 0;
    tp->stack_index = 0;
    tp->array_size = 0;
    tp->top_size = 0;
  }

  tp->buf = (char *)protocol;
  tp->len = *len;
  *len = 0; /* reset parsed size */

  while (tp->stack_index >= 0)
    if (TEXTPROTO_OK != tp_parse_bulk(tp))
      break;

  if (TEXTPROTO_OK == tp->err) {
    if (tp->offset > 0) {
      *len = tp->offset;
      tp->offset = 0;
    }

    if (-1 == tp->stack_index) {
      if (tp->elements > 0) {
        *root = tp->stack_protos[0];

        (*root)->elem_count = tp->top_size - 1;
        (*root)->elements = (textproto_element_t **)tp_malloc(
            (*root)->elem_count * sizeof(textproto_element_t *));

        textproto_protocol(*root, tp->stack_protos + 1);
      } else
        *root = NULL;
    }
  }

  return tp->err;
}

sstr_t textproto_write_buffer(sstr_t sstr, const void *buf, int size) {
  char num[32] = {0};
  int len;

  len = sprintf(num, "%i\r\n", size);
  sstr = sstr_catlen(sstr, num, len);

  if (size > 0) {
    sstr = sstr_catlen(sstr, buf, size);
    sstr = sstr_catlen(sstr, "\r\n", 2);
  }

  return sstr;
}

sstr_t textproto_write_string(sstr_t sstr, const char *str) {
  return textproto_write_buffer(sstr, str, (int)strlen(str));
}

sstr_t textproto_write_array(sstr_t sstr, int n) {
  char num[32] = {0};
  int len;

  len = sprintf(num, "*%i\r\n", n);
  return sstr_catlen(sstr, num, len);
}

sstr_t textproto_write_int64(sstr_t sstr, int64_t i64) {
  char num[32] = {0};
  int len;

  len = sprintf(num, "%" PRIi64, i64);
  return textproto_write_buffer(sstr, num, len);
}

sstr_t textproto_write_uint64(sstr_t sstr, uint64_t u64) {
  char num[32] = {0};
  int len;

  len = sprintf(num, "%" PRIu64, u64);
  return textproto_write_buffer(sstr, num, len);
}

sstr_t textproto_write_double(sstr_t sstr, double d) {
  char num[32] = {0};
  int len;

  len = sprintf(num, "%lf", (d));
  return textproto_write_buffer(sstr, num, len);
}

sstr_t textproto_write_null(sstr_t sstr) {
  return sstr_catlen(sstr, "-1\r\n", 4);
}

sstr_t textproto_finalize(sstr_t sstr) { return sstr_catlen(sstr, "\r\n", 2); }
