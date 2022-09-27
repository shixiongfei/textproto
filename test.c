/*
 * test.c
 *
 * Copyright (c) 2022 Xiongfei Shi
 *
 * Author: Xiongfei Shi <xiongfei.shi(a)icloud.com>
 * License: Apache-2.0
 *
 * https://github.com/shixiongfei/textproto
 */

#include "textproto.h"
#include <stdio.h>

static void print_proto(textproto_element_t *root, int depth) {
  int i;

  for (i = 0; i < depth; ++i)
    printf("  ");

  if (root->size > 0)
    printf("%s ", root->data);

  if (root->elem_count > 0)
    printf("<ARRAY %d> ", root->elem_count);
  else if (root->size < 0)
    printf("<NULL> ");

  printf("\n");

  for (i = 0; i < root->elem_count; ++i)
    print_proto(root->elements[i], depth + 1);
}

int main(int argc, char *argv[]) {
  sstr_t sp = sstr_empty();
  sstr_t repr = sstr_empty();
  textproto_t *tp = textproto_create();
  textproto_element_t *root = NULL;
  int len;

  sp = textproto_write_uint64(sp, 987654);
  sp = textproto_write_array(sp, 3);
  sp = textproto_write_int64(sp, 123456);
  sp = textproto_write_string(sp, "Hello World!");
  sp = textproto_write_double(sp, 3.14);
  sp = textproto_write_null(sp);
  sp = textproto_write_string(sp, "");
  sp = textproto_write_string(sp, "Text Protocol");
  sp = textproto_finalize(sp);

  len = sstr_length(sp);
  repr = sstr_catrepr(repr, sp, len);

  printf("Text Proto: %s\n", repr);
  printf("Text Proto Parse: %d\n", textproto_parse(tp, &root, sp, &len));

  if (root) {
    print_proto(root, 0);
    textproto_element_destroy(root);
  }

  sstr_destroy(sp);
  sstr_destroy(repr);
  textproto_destroy(tp);

  return 0;
}
