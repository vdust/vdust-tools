/*
 * Copyright (C) 2015, Raphaël Bois Rousseau
 * All rights reserved.
 *
 * Redistribution   and  use in  source  and  binary  forms,  with  or  without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *     * Redistributions  in  binary form  must reproduce  the above  copyright
 *       notice, this list  of conditions and  the following  disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither   the  name  of the   copyright   holders  nor  the  names  of
 *       contributors may be used  to endorse or promote  products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT  HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR  IMPLIED WARRANTIES,  INCLUDING, BUT  NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF  MERCHANTABILITY AND FITNESS  FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN  NO EVENT SHALL THE  COPYRIGHT HOLDERS  BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL,  SPECIAL, EXEMPLARY,  OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS  INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY,  WHETHER IN CONTRACT,  STRICT LIABILITY, OR TORT
 * (INCLUDING  NEGLIGENCE OR  OTHERWISE) ARISING  IN ANY WAY OUT  OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Brainf*ck interpreter
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef NULL
#define NULL ((void *)0)
#endif

#define warn(fmt, ...) fprintf(stderr, fmt"\n", ##__VA_ARGS__)
#define critical(fmt, ...) \
  do { \
    fprintf(stderr, fmt"\n", ##__VA_ARGS__); \
    exit(1); \
  } while(0)

#define is_bf_char(c) (((c) != 0) && (strchr("<>+-.,[]", (int)((unsigned char)(c))) != NULL))

#ifndef BFI_ALLOC_CHUNK
#define BFI_ALLOC_CHUNK 256
#endif

#ifdef BFI_ALLOC_LIMIT
#define check_alloc(size) \
  do if(size > BFI_ALLOC_LIMIT) \
    critical("%s", "Allocation limit exceeded"); \
  while(0)
#else
#define check_alloc(size) (void)0
#endif

#ifndef BFI_READ_CHUNK
#define BFI_READ_CHUNK 256
#endif

typedef struct {
  char *data;
  size_t data_alloc;
  char *data_p;

  char *script;
  size_t script_alloc;
  char *script_p;

  FILE *in;
  int close_in;
  FILE *out;
  int close_out;

  /* syntax error checks */
  size_t script_read_length;
  size_t errored;
} bfi_state_t;

static int _buffer_check_size(char **buffer_p, size_t *alloc, char **cursor);
static int _buffer_set_byte(char **buffer, size_t *alloc, char **cursor, char byte);
static int _buffer_get_byte(char **buffer, size_t *alloc, char **cursor, char *byte);
static int _buffer_delta_byte(char **buffer, size_t *alloc, char **cursor, char delta);

static inline size_t _max(size_t a, size_t b) {
  return (a < b) ? b : a;
}

static int
_buffer_check_size(char **buffer_p, size_t *allocated, char **cursor) {
  size_t osize, size, cpos;
  char *buffer;

  if (!buffer_p || !allocated || !cursor) return 0; /* no missing pointer allowed */

  if ((*cursor) < (*buffer_p)) return 0; /* out of bound */

  cpos = (size_t)(*cursor) - (size_t)(*buffer_p); /* working if both are NULL */
  osize = *allocated;

  if (!(*buffer_p) || ((*buffer_p) + ((*allocated) - 1) <= (*cursor))) {
    size = _max(cpos + 1, BFI_ALLOC_CHUNK + *allocated);
    check_alloc(size); /* noop if BFI_ALLOC_LIMIT is not defined at compile time */
    buffer = realloc(*buffer_p, size);

    if (!buffer) return 0;

    *buffer_p = buffer;
    *allocated = size;
    *cursor = buffer + cpos;
    memset(buffer + osize, 0, *allocated - osize);
  }
  
  return 1;
}

static int
_buffer_set_byte(char **buffer, size_t *allocated, char **cursor, char byte) {
  if (!_buffer_check_size(buffer, allocated, cursor)) {
    return 0;
  }
  **cursor = byte;
  return 1;
}

static int
_buffer_get_byte(char **buffer, size_t *allocated, char **cursor, char *byte) {
  /* Lazy allocation model. return 0 on read if out of bound. */
  if (!(*cursor) || (*cursor) < (*buffer) || ((*cursor) > (*buffer + *allocated))) {
    *byte = 0;
    return 1;
  }

  *byte = **cursor;
  return 1;
}

static int
_buffer_delta_byte(char **buffer, size_t *allocated, char **cursor, char delta) {
  if (!_buffer_check_size(buffer, allocated, cursor)) {
    return 0;
  }
  **cursor += delta;
  return 1;
}



int bfi_restart(bfi_state_t *bfi) {
  if (!bfi) return 0;
  if (bfi->data) {
    free(bfi->data);
    bfi->data = NULL;
    bfi->data_alloc = 0;
    bfi->data_p = 0;
  }
  bfi->script_p = bfi->script;
  return 1;
}

int bfi_reset(bfi_state_t *bfi) {
  if (!bfi_restart(bfi)) return 0;

  if (bfi->script) {
    free(bfi->script);
    bfi->script = NULL;
    bfi->script_alloc = 0;
    bfi->script_p = NULL;
  }

  return 1;
}

bfi_state_t *bfi_new() {
  bfi_state_t *bfi;

  bfi = (bfi_state_t *) calloc(1, sizeof (*bfi));

  if (bfi) {
    bfi->in = stdin;
    bfi->out = stdout;
  }

  return bfi;
}

void bfi_free(bfi_state_t *bfi) {
  if (!bfi) return;

  bfi_reset(bfi);

  if (bfi->close_in && bfi->in) {
    fclose(bfi->in);
  }
  if (bfi->close_out && bfi->out) {
    fclose(bfi->out);
  }

  free(bfi);
}


int bfi_script_put_char(bfi_state_t *bfi, char c) {
  if (!bfi) return 0;

  if (bfi->errored) return 0;

  bfi->script_read_length++;

  if (is_bf_char(c)) {
    if (!_buffer_set_byte(&(bfi->script), &(bfi->script_alloc), &(bfi->script_p), c)) {
      bfi->errored = bfi->script_read_length;
      return 0;
    }
    bfi->script_p++;
  }

  return 1;
}

int bfi_script_from_buffer(bfi_state_t *bfi, const char*buffer, int buffer_length) {
  const char *p;

  if (!bfi) return 0;

  if (bfi->errored) return 0;

  for (p = buffer; buffer_length > 0; p++, buffer_length--) {
    if (!bfi_script_put_char(bfi, *p)) return 0;
  }

  return 1;
}

int bfi_data_set(bfi_state_t *bfi, char byte) {
  if (!bfi) return 0;
  return _buffer_set_byte(&(bfi->data), &(bfi->data_alloc), &(bfi->data_p), byte);
}

int bfi_data_get(bfi_state_t *bfi, char *byte_p) {
  if (!bfi) return 0;
  return _buffer_get_byte(&(bfi->data), &(bfi->data_alloc), &(bfi->data_p), byte_p);
}

int bfi_data_inc(bfi_state_t *bfi) {
  if (!bfi) return 0;
  return _buffer_delta_byte(&(bfi->data), &(bfi->data_alloc), &(bfi->data_p), 1);
}

int bfi_data_dec(bfi_state_t *bfi) {
  if (!bfi) return 0;
  return _buffer_delta_byte(&(bfi->data), &(bfi->data_alloc), &(bfi->data_p), -1);
}

int bfi_data_read(bfi_state_t *bfi) {
  char d;
  if (!bfi || !bfi->in || !fread(&d, 1, 1, bfi->in)) return 0;
  return bfi_data_set(bfi, d);
}
int bfi_data_write(bfi_state_t *bfi) {
  char d;
  if (!bfi_data_get(bfi, &d)) return 0;
  if (!bfi->out || !fwrite(&d, 1, 1, bfi->out)) return 0;
  fflush(bfi->out);
  return 1;
}

int bfi_test(bfi_state_t *bfi) {
  char c, d, nb;

  if (!bfi_data_get(bfi, &d)) return 0;

  c = *(bfi->script_p);
  nb = 1;
  switch (c) {
    case '[':
      if (d) return 1;
      /* Seek forward to closing bracket */
      while ((c = *++(bfi->script_p)) > 0) {
        if (c == ']') nb--;
        if (c == '[') nb++;
        if (!nb) break;
      }
      break;
    case ']':
      if (!d) return 1;
      /* Seek backward to opening bracket */
      while (--bfi->script_p >= bfi->script) {
        c = *(bfi->script_p);
        if (c == '[') nb--;
        else if (c == ']') nb++;
        if (!nb) break;
      }
      break;
    default: /* should not occur */
      break;
  }
  return !nb;
}

int bfi_load(bfi_state_t *bfi, FILE *script) {
  char read_buffer[BFI_READ_CHUNK];
  size_t read_len = 1;
  int err = 0;

  if (!script || !bfi_reset(bfi)) return 0;

  while (read_len) {
    if ((read_len = fread(read_buffer, 1, BFI_READ_CHUNK, script)) > 0) {
      if (!bfi_script_from_buffer(bfi, read_buffer, read_len)) {
        err = -1;
        break;
      }
    }
  }

  if (!err && (err = ferror(script)) != 0) {
    warn("Error reading file: errno=%d", err);
  }

  return !err;
}

int bfi_load_file(bfi_state_t *bfi, const char *filename) {
  int ret;
  FILE *script;

  if (!bfi || !filename) return 0;

  script = fopen(filename, "r");

  if (!script) return 0;

  ret = bfi_load(bfi, script);

  fclose(script);

  return ret;
}

int bfi_cycle(bfi_state_t *bfi) {
  char c;
  size_t dpos;

  if (!bfi || !bfi->script_p) return 0;

  c = *(bfi->script_p);
  if (!c) return 0; /* end of script */
  dpos = (size_t)(bfi->data_p - bfi->data);
  switch (c) {
    case '<':
      bfi->data_p--;
      break;
    case '>':
      bfi->data_p++;
      break;
    case '+':
      if (!bfi_data_inc(bfi)) {
        warn("Failed to increment byte %lu", dpos);
        bfi->errored = -1;
        return 0;
      }
      break;
    case '-':
      if (!bfi_data_dec(bfi)) {
        warn("Failed to increment byte %lu", dpos);
        bfi->errored = -1;
        return 0;
      }
      break;
    case '.':
      if (!bfi_data_write(bfi)) {
        warn("Failed to write byte %lu", dpos);
        bfi->errored = -1;
        return 0;
      }
      break;
    case ',':
      if (!bfi_data_read(bfi)) {
        warn("Failed to read byte at %lu", dpos);
        bfi->errored = -1;
        return 0;
      }
      break;
    case '[':
    case ']':
      if (!bfi_test(bfi)) {
        warn("Failed to test byte %lu (%c)", dpos, c);
        bfi->errored = -1;
        return 0;
      }
      break;
    default:
      return 0; /* At this point, there should be no other character in the script. */
  }
  bfi->script_p++;

  return 1;
}

void bfi_run(bfi_state_t *bfi) {
  bfi_restart(bfi);
  while (bfi_cycle(bfi));
}

int main(int argc, char *argv[])
{
  int err = 0;
  bfi_state_t *bfi;

  if (argc != 2) {
    critical("%s", "usage: bfi script.b");
  }

  bfi = bfi_new();
  if (!bfi || !bfi_load_file(bfi, argv[1]) || bfi->errored) {
    critical("%s", "Failed to load script");
    err = 1;
    goto cleanup;
  }

  bfi_run(bfi);

  err = bfi->errored ? 2 : 0;

cleanup:
  bfi_free(bfi);
  return err;
}
