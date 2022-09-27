/*
 * textproto.h
 *
 * Copyright (c) 2022 Xiongfei Shi
 *
 * Author: Xiongfei Shi <xiongfei.shi(a)icloud.com>
 * License: Apache-2.0
 *
 * https://github.com/shixiongfei/textproto
 */

#ifndef __TEXTPROTO_H__
#define __TEXTPROTO_H__

#include <sstr.h>
#include <stddef.h>
#include <stdint.h>

#define TEXTPROTO_OK 1
#define TEXTPROTO_MORE 0
#define TEXTPROTO_ERROR (-1)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct textproto_element_t {
  struct textproto_element_t **elements;
  int elem_count;
  int size;
  char data[1];
} textproto_element_t;

typedef struct textproto_t textproto_t;

void textproto_setalloc(void *(*allocator)(void *, size_t));

textproto_t *textproto_create(void);
void textproto_destroy(textproto_t *tp);
void textproto_element_destroy(textproto_element_t *elem);

int textproto_parse(textproto_t *tp, textproto_element_t **root,
                    const char *protocol, int *len);
sstr_t textproto_write_buffer(sstr_t sstr, const void *buf, int size);
sstr_t textproto_write_string(sstr_t sstr, const char *str);
sstr_t textproto_write_array(sstr_t sstr, int n);
sstr_t textproto_write_int64(sstr_t sstr, int64_t i64);
sstr_t textproto_write_uint64(sstr_t sstr, uint64_t u64);
sstr_t textproto_write_double(sstr_t sstr, double d);
sstr_t textproto_write_null(sstr_t sstr);
sstr_t textproto_finalize(sstr_t sstr);

#ifdef __cplusplus
};
#endif

#endif /* __TEXTPROTO_H__ */
