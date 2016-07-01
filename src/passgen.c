/*
 * Copyright (C) 2014, Raphaël Bois
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define BUFFER_SIZE 128
#define PASS_LENGTH_DEFAULT 12
#define PASS_COUNT_DEFAULT 1

static const char ALLCHARS[] =
  "abcdefghijklmnopqrstuvwxyz0123456789" /* alnum: 36 */
#define ALNUM_LEN 36
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ" /* alphanum: 62 */
#define ALPHANUM_LEN 62
  "-_"
#define BASE64_LEN 64
  ",;.:=+@%" /* standard: 72 */
#define STANDARD_LEN 72
  "&\"#'{([<|>])}$*?/!" /* extended: 90 */
#define EXTENDED_LEN 90
   ;

static int pwlen = PASS_LENGTH_DEFAULT;  /* Length of passwords */
static int pwcount = PASS_COUNT_DEFAULT; /* Number of passwords */
static const char *pwchars = ALLCHARS;   /* Password character set */
static int pwcharslen = STANDARD_LEN;    /* Character count in set */
static int printchars = 0;  /* 1 => print charset and exit */

static int check_number(const char *num, const char *opt)
{
  const char *c;
  for (c = num; *c != '\0'; c++) {
    if (!isdigit(*c)) {
      fprintf(stderr, "Integer expected for option %s\n", opt);
      return 0;
    }
  }
  return c != num;
}

static void usage(const char *prog)
{
  fprintf(stderr,
    "Usage: %s [-l LENGTH] [-c COUNT] [-a|-A|-S|-E|-C CHARS]\n"
    "       %s -h\n"
    "\n"
    "Options:\n"
    "  Arguments for long options are required for related short options as well.\n"
    "\n"
    "  -h, --help           Print this help message and exit.\n"
    "\n",
    prog, prog);
  fprintf(stderr,
    "  -a, --alnum          Use lowercase alphanum character set.\n"
    "  -A, --alphanum       Use full alphanum character set.\n"
    "  -B, --base64         Use base64 character set (use -_ instead of +/).\n"
    "  -c, --count COUNT    Generate COUNT passwords, one per line. [default: %d]\n"
    "  -C, --chars CHARS    Use a custom character set.\n"
    "  -E, --extended       Use the extended character set.\n"
    "  -l, --length LENGTH  Set the length of passwords. [default: %d]\n"
    "  -S, --standard       Use the standard character set. [default]\n"
    "  -p, --print          Print the character set and exit.\n",
    PASS_COUNT_DEFAULT, PASS_LENGTH_DEFAULT);
}

static int parse_opts(int argc, char *argv[])
{
  char *opt;
  int i, ok;

#define isopt(s, l) (!strcmp(opt, s) || !strcmp(opt, l))
#define optint(var) do { \
    i++; \
    if (i >= argc || !check_number(argv[i], opt)) return 0; \
    var = atoi(argv[i]); \
  } while(0)
#define setchars(buf, len) do { \
    pwchars = buf; \
    pwcharslen = len; \
  } while(0)

  for (i = 1; i < argc; i++) {
    opt = argv[i];
    if isopt("-l", "--length") {
      optint(pwlen);
      if (pwlen == 0) {
        pwlen = PASS_LENGTH_DEFAULT;
      } else if (pwlen >= BUFFER_SIZE) {
        fprintf(stderr, "Maximum password length is %d.\n", BUFFER_SIZE - 1);
        return 0;
      }
    } else if isopt("-c", "--count") {
      optint(pwcount);
    } else if isopt("-a", "--alnum") {
      setchars(ALLCHARS, ALNUM_LEN);
    } else if isopt("-A", "--alphanum") {
      setchars(ALLCHARS, ALPHANUM_LEN);
    } else if isopt("-B", "--base64") {
      setchars(ALLCHARS, BASE64_LEN);
    } else if isopt("-S", "--standard") {
      setchars(ALLCHARS, STANDARD_LEN);
    } else if isopt("-E", "--extended") {
      setchars(ALLCHARS, EXTENDED_LEN);
    } else if isopt("-C", "--chars") {
      ok = 0;
      i++;
      if (i < argc) {
        setchars(argv[i], strlen(argv[i]));
        if (pwcharslen) ok = 1;
      }
      if (!ok) {
        fprintf(stderr, "Missing argument for option '%s'.\n", opt);
        return 0;
      }
    } else if isopt("-p", "--print") {
      printchars = 1;
    } else if isopt("-h", "--help") {
      usage(argv[0]);
      return 0;
    } else if (opt[0] == '-') {
      fprintf(stderr, "Unknown option '%s'\n", opt);
      return 0;
    } else {
      fprintf(stderr, "Unexpected argument '%s'\n", opt);
      return 0;
    }
  }

  return 1;
}

static void print_chars()
{
  printf("count=%d\n", pwcharslen);
  fwrite(pwchars, sizeof (char), pwcharslen, stdout);
  printf("\n");
}


#define dbl(var) ((double)(var))
static int gen_password(FILE *randsrc)
{
  int i, c;
  uint32_t in;
  char buf[BUFFER_SIZE];
  for (i = 0; i < pwlen; i++) {
    if (!fread(&in, 4, 1, randsrc)) {
      perror("fread");
      return 0;
    }
    c = ((int)(dbl(pwcharslen) * (dbl(in) / dbl((uint32_t)0xffffffff)))) % pwcharslen;
    buf[i] = pwchars[c];
  }
  buf[i] = 0;
  printf("%s\n", buf);
  return 1;
}

int main(int argc, char *argv[])
{
  int i;
  FILE *randf;
  if (!parse_opts(argc, argv)) {
    return 1;
  }
  if (printchars) {
    print_chars();
    return 0;
  }

  randf = fopen("/dev/urandom", "r");
  if (!randf) {
    perror("fopen");
    return 2;
  }

  for (i = 0; i < pwcount; i++) {
    if (!gen_password(randf)) {
      return 2;
    }
  }

  return 0;
}
