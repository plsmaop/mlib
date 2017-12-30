/*
 * Copyright (c) 2017, Patrick Pelissier
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * + Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * + Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <gmp.h> // For testing purpose only.

#include "m-snapshot.h"
#include "m-mutex.h"

#include "coverage.h"
START_COVERAGE
SNAPSHOT_DEF(snapshot_uint, unsigned int)
SNAPSHOT_DEF(snapshot_mpz, mpz_t,
             (INIT(mpz_init), INIT_SET(mpz_init_set), SET(mpz_set), CLEAR(mpz_clear)))
END_COVERAGE

static void test_uint(void)
{
  unsigned int *p_w;
  const unsigned int *p_r;
  snapshot_uint_t t;
  snapshot_uint_init(t);

  p_r = snapshot_uint_read(t);
  assert (p_r == snapshot_uint_get_read_buffer(t));
  assert(*p_r == 0);
  assert(snapshot_uint_read(t) == p_r);
  
  p_w = snapshot_uint_write(t);
  assert (snapshot_uint_updated_p (t) == true);
  assert (p_w != p_r);
  *p_w = 1;
  assert (*p_r == 0);
  assert (snapshot_uint_read(t) != p_r);
  assert (snapshot_uint_updated_p (t) == false);
  assert (*snapshot_uint_read(t) == 0);
  p_r = snapshot_uint_read(t);
  assert (*p_r == 0);
  
  p_w = snapshot_uint_write(t);
  *p_w = 2;
  assert (*snapshot_uint_read(t) == 1);
  
  p_w = snapshot_uint_write(t);
  assert (p_w == snapshot_uint_get_write_buffer(t));
  *p_w = 3;
  p_w = snapshot_uint_write(t);
  *p_w = 4;
  assert (*snapshot_uint_read(t) == 3);

  snapshot_uint_t t2;
  snapshot_uint_init_set (t2, t);
  snapshot_uint_clear(t);
  assert (*snapshot_uint_read(t) == 3);
  snapshot_uint_init(t);
  snapshot_uint_set (t, t2);
  assert (*snapshot_uint_read(t) == 3);
}

typedef struct {
  unsigned int n;
  unsigned int c;
  unsigned int p;
} data_t;

static void data_crc(data_t *p) { p->p = -p->n; p->c = ~p->n; }
static void data_init(data_t *p) { p->n = 0; data_crc(p); }
static void data_clear(data_t *p) { (void) p;}
static void data_set(data_t *p, data_t o) { p->p = o.p; data_crc(p); }

SNAPSHOT_DEF(snapshot_data, data_t,
             (INIT(data_init M_IPTR), INIT_SET(data_set M_IPTR), SET(data_set M_IPTR), CLEAR(data_clear M_IPTR))
             )

snapshot_data_t g_buff;

static void conso(void *arg)
{
  assert (arg == NULL);
  while (true) {
    const data_t *p = snapshot_data_read(g_buff);
    assert (p->n == -p->p);
    assert (p->n == ~p->c);
    if (p->n == 0)
      return;
  }
}

static void prod(void *arg)
{
  assert (arg == NULL);
  data_t *p = snapshot_data_get_write_buffer(g_buff);
  for(unsigned int i = 1; i < 100000;i++) {
    p->n = i * i;
    data_crc(p);
    p = snapshot_data_write(g_buff);
  }
  p->n = 0;
  data_crc(p);
  p = snapshot_data_write(g_buff);
  assert (p != NULL);
}

static void test_global(void)
{
  m_thread_t idx[2];

  snapshot_data_init(g_buff);
  data_t *p = snapshot_data_write(g_buff);
  assert (p != NULL);
  p->n = 42;
  data_crc(p);
  p = snapshot_data_write(g_buff);
  assert (p != NULL);

  m_thread_create (idx[0], conso, NULL);
  m_thread_create (idx[1], prod, NULL);

  m_thread_join(idx[0]);
  m_thread_join(idx[1]);

  snapshot_data_clear(g_buff);
}

int main(void)
{
  test_uint();
  test_global();
  exit(0);
}

