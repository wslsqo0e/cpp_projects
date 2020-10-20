#ifndef _ACISM_H_
#define _ACISM_H_
#include<stdint.h>
#include "utils.h"

#define SYM_BITS psp->sym_bits
#define SYM_MASK psp->sym_mask

#define BACK ((unsigned short)0)
#define ROOT ((unsigned) 0)

typedef int (*qsort_cmp)(const void *, const void *);

typedef struct { unsigned state; unsigned strno; } STRASH;

// MATCH and SUFFIX are the top 2 bits of a TRAN:
enum {
  IS_MATCH  = (unsigned)1 << (8*sizeof(unsigned) - 1),
  IS_SUFFIX = (unsigned)1 << (8*sizeof(unsigned) - 2),
  T_FLAGS   = IS_MATCH | IS_SUFFIX
};

struct acism {
  unsigned* tranv;
  STRASH* hashv;
  unsigned flags;

  unsigned sym_mask;   // =~(~0 << sym_bits)     等价于 2**sym_bits - 1
  unsigned sym_bits;  // 存储number nsyms 需要多少字节  1 + floor(log2(nsyms))

  unsigned hash_mod; // search hashv starting at (state + sym) % hash_mod.
  unsigned hash_size; // #(hashv): hash_mod plus the overflows past [hash_mod-1]
  unsigned tran_size; // #(tranv)
  unsigned nsyms, nchars, nstrs, maxlen;
  unsigned short symv[256];
};

typedef struct acism ACISM;

ACISM* acism_create(MEMREF const *strv, int nstrs);
void   acism_destroy(ACISM*);

static inline void set_tranv(ACISM *psp, void *mem)
{ psp->hashv = (STRASH*)&(psp->tranv = (unsigned*)mem)[psp->tran_size]; }

static inline size_t p_size(ACISM const *psp)
{ return psp->hash_size * sizeof*psp->hashv
    + psp->tran_size * sizeof*psp->tranv; }

static inline unsigned  p_hash(ACISM const *psp, unsigned s)
{ return s * 107 % psp->hash_mod; }

static inline unsigned  p_tran(ACISM const *psp, unsigned s, unsigned sym)
{ return psp->tranv[s + sym] ^ sym; }

static inline unsigned t_sym(ACISM const *psp, unsigned t)    { (void)psp; return t & SYM_MASK; }
static inline unsigned   t_next(ACISM const *psp, unsigned t)   { (void)psp; return (t & ~T_FLAGS) >> SYM_BITS; }
static inline int     t_isleaf(ACISM const *psp, unsigned t) { return t_next(psp, t) >= psp->tran_size; }
static inline int     t_strno(ACISM const *psp, unsigned t)  { return t_next(psp, t) - psp->tran_size; }
static inline unsigned t_valid(ACISM const *psp, unsigned t)  { return !t_sym(psp, t); }

typedef int (ACISM_ACTION)(int strnum, int textpos, void *context);
int acism_more(ACISM const*, MEMREF const text,
               ACISM_ACTION *fn, void *fndata, int *state);

#endif
