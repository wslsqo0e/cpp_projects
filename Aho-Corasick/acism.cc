#include "acism.h"
#include <cstring>

typedef enum { BASE=2, USED=1 } USES;

// bitwid: 1+floor(log2(u))
static inline int bitwid(unsigned u)
{
  int ret = !!u;
  if (u & 0xFFFF0000) u >>= 16, ret += 16;
  if (u & 0x0000FF00) u >>= 8, ret += 8;
  if (u & 0x000000F0) u >>= 4, ret += 4;
  if (u & 0x0000000C) u >>= 2, ret += 2;
  if (u & 0x00000002) ret++;
  return ret;
}

typedef struct tnode {
  struct tnode *child, *next, *back;
  // nrefs was used in "prune_backlinks".
  //  It will be used again in "curtail".
  unsigned    nrefs;
  unsigned    state;
  unsigned    match;
  unsigned short    sym;
  char        is_suffix;      // "bool"
} TNODE;

static void   fill_symv(ACISM*, MEMREF const*, int ns);
static int    create_tree(TNODE*, unsigned short const*symv, MEMREF const*strv, int nstrs);
static void add_backlinks(TNODE *troot, TNODE **v1, TNODE **v2);
static int    interleave(TNODE*, int nnodes, int nsyms, TNODE**, TNODE**);

static TNODE* find_child(TNODE*, unsigned short);
static void fill_tranv(ACISM *psp, TNODE const*tp);
static void fill_hashv(ACISM *psp, TNODE const treev[], int nnodes);

// (ns) is either a STATE, or a (STRNO + tran_size)
static inline void
set_tran(ACISM *psp, unsigned s, unsigned short sym, int match, int suffix, unsigned ns)
{
  psp->tranv[s + sym] = sym | (match ? IS_MATCH : 0) \
      | (suffix ? IS_SUFFIX : 0)
      | (ns << SYM_BITS);
}

void acism_destroy(ACISM *psp)
{
  if (!psp) return;

  free(psp->tranv);
  free(psp);
}

ACISM* acism_create(MEMREF const* strv, int nstrs)
{
  TNODE **v1 = NULL, **v2 = NULL;
  ACISM *psp = static_cast<ACISM*>(calloc(1, sizeof*psp));

  fill_symv(psp, strv, nstrs);
  TNODE *troot = static_cast<TNODE*>(calloc(psp->nchars + 1, sizeof*troot));

  int nnodes = create_tree(troot, psp->symv, strv, nstrs);

  // v1, v2: breadth-first work vectors for add_backlink and interleave.
  int i = (nstrs + 1) * sizeof(TNODE);
  add_backlinks(troot, v1 = (TNODE**)malloc(i), v2 = (TNODE**)malloc(i));

  int     nhash = 0;
  TNODE*  tp = troot + nnodes;
  while (--tp > troot)
    nhash += tp->match && tp->child;

  // Calculate each node's offset in tranv[]:
  psp->tran_size = interleave(troot, nnodes, psp->nsyms, v1, v2);

  if (bitwid(psp->tran_size + nstrs - 1) + SYM_BITS > sizeof(unsigned)*8 - 2) {
    acism_destroy(psp), psp = NULL;
    return psp;
  }

  if (nhash) {
    // Hash table is for match info of non-leaf nodes (only).
    // Set hash_size for p_size(psp):
    psp->hash_mod = nhash * 5 / 4 + 1;
    // Initially oversize the table for overflows without wraparound.
    psp->hash_size = psp->hash_mod + nhash;
  }
  set_tranv(psp, calloc(p_size(psp), 1));
  if (!psp->tranv) {
    acism_destroy(psp), psp = NULL;
    return psp;
  }
  fill_tranv(psp, troot);
  // The root state (0) must not look like a valid backref.
  // Any symbol value other than (0) in tranv[0] ensures that.
  psp->tranv[0] = 1;

  if (nhash) {
    fill_hashv(psp, troot, nnodes);
    // Adjust hash_size to include trailing overflows
    //  but trim trailing empty slots.
    psp->hash_size = psp->hash_mod;
    while ( psp->hashv[psp->hash_size].state)     ++psp->hash_size;
    while (!psp->hashv[psp->hash_size - 1].state) --psp->hash_size;
    set_tranv(psp, realloc(psp->tranv, p_size(psp)));   // 节省内存
  }

  // Diagnostics/statistics only:
  psp->nstrs = nstrs;
  for (i = psp->maxlen = 0; i < nstrs; ++i)
    if (psp->maxlen < strv[i].len) psp->maxlen = strv[i].len;

  free(troot), free(v1), free(v2);
  return psp;
}

typedef struct { int freq; int rank; } FRANK;
static int frcmp(FRANK*a, FRANK*b) { return a->freq - b->freq; }

static void fill_symv(ACISM *psp, MEMREF const *strv, int nstrs)
{
  int i, j;
  FRANK frv[256];   // one byte, 256 character, 统计每个character的频次

  for (i = 0; i < 256; ++i) frv[i] = (FRANK){0,i};
  for (i = 0; i < nstrs; ++i) {
    for (psp->nchars += j = strv[i].len; --j >= 0;) {
      frv[(uint8_t)strv[i].ptr[j]].freq++;
    }
  }
  qsort(frv, 256, sizeof*frv, (qsort_cmp)frcmp);  // 按照freq从小到大排序, rank记录对应字符的ACSII 码

  for (i = 256; --i >= 0 && frv[i].freq;) {
    psp->symv[frv[i].rank] = ++psp->nsyms;    // psp->nsyms 记录出现character的种类数
  }
  ++psp->nsyms;                               // 出现种类数 +1

  psp->sym_bits = bitwid(psp->nsyms);
  psp->sym_mask = ~(~0 << psp->sym_bits);
}

static int create_tree(TNODE *Tree, unsigned short const *symv, MEMREF const *strv, int nstrs)
{
  int i, j;
  TNODE *nextp = Tree + 1;

  for (i = 0; i < nstrs; ++i) {
    TNODE *tp = Tree;

    for (j = 0; tp->child && j < (int)strv[i].len; ++j) {
      unsigned short sym = symv[(uint8_t)strv[i].ptr[j]];

      if (sym < tp->child->sym) {
        // Prep to insert new node before tp->child
        nextp->next = tp->child;
        break;
      }

      tp = tp->child;
      while (tp->next && sym >= tp->next->sym) {
        tp = tp->next;
      }

      // Insert new siblint after tp
      if (sym > tp->sym) {
        nextp->next = tp->next;
        tp = tp->next = nextp++;
        tp->sym = sym;
        tp->back = Tree;
      }
    }

    for (; j < (int) strv[i].len; ++j) {
      tp = tp->child = nextp++;
      tp->sym = symv[(uint8_t)strv[i].ptr[j]];
      tp->back = Tree;
    }
    tp->match = i + 1; // Encode strno as nonzero
  }
  return nextp - Tree;
}

static void add_backlinks(TNODE *troot, TNODE **v1, TNODE **v2)
{
  TNODE *tp, **tmp;

  // troot 第一层子节点 拷贝到 v1 中
  for (tp = troot->child, tmp = v1; tp; tp = tp->next)
    *tmp++ = tp;
  *tmp = NULL;

  while (*v1) {
    TNODE **spp = v1, **dpp = v2, *srcp, *dstp;

    while ((srcp = *spp++)) {
      for (dstp = srcp->child; dstp; dstp = dstp->next) {
        TNODE *bp = NULL;
        if (dstp->child)
          *dpp++ = dstp;

        // Go through the parent (srcp) node's backlink chain,
        //  looking for a useful backlink for the child (dstp).
        // If the parent (srcp) has a backlink to (tp),
        //  and (tp) has a child matching the transition sym
        //  for (srcp -> dstp), then it is a useful backlink
        //  for the child (dstp).
        // Note that backlinks do not point at the suffix match;
        //  they point at the PARENT of that match.

        for (tp = srcp->back; tp; tp = tp->back)
          if ((bp = find_child(tp, dstp->sym)))
            break;
        if (!bp)
          bp = troot;

        dstp->back = dstp->child ? bp : tp ? tp : troot;
        dstp->back->nrefs++;
        dstp->is_suffix = bp->match || bp->is_suffix;
      }
    }
    *dpp = 0;
    tmp = v1; v1 = v2; v2 = tmp;
  }
}


static TNODE * find_child(TNODE *tp, unsigned short sym)
{
  for (tp = tp->child; tp && tp->sym < sym; tp = tp->next);
  return tp && tp->sym == sym ? tp : NULL;
}

static int
interleave(TNODE *troot, int nnodes, int nsyms, TNODE **v1, TNODE **v2)
{
  unsigned usev_size = nnodes + nsyms;
  char *usev = static_cast<char*>(calloc(usev_size, sizeof*usev));
  unsigned last_trans = 0, startv[257][2] = { 0 };
  TNODE *cp, **tmp;

  memset(startv, 0, nsyms * sizeof*startv);

  // Iterate through one level of the Tree at a time.
  //  That srsly improves locality (L1-cache use).
  v1[0] = troot, v1[1] = NULL;
  for (; *v1; tmp = v1, v1 = v2, v2 = tmp) {
    TNODE **srcp = v1, **dstp = v2, *tp;
    while ((tp = *srcp++)) {
      if (!tp->child) continue;

      if (tp->back == troot) tp->back = NULL; // simplify tests.
      cp = tp->child;

      unsigned pos, *startp = &startv[cp->sym][!!tp->back];
      while ((cp = cp->next)) {
        unsigned *newp = &startv[cp->sym][!!tp->back];
        if (*startp < *newp) startp = newp;
      }

      // If (tp) has a backref, we need a slot at offset 0
      //  that is free as a base AND to be used (filled in).
      char need = tp->back ? BASE|USED : BASE;
      for (pos = *startp;; ++pos) {
        if (usev[pos] & need) {
          continue;
        }

        for (cp = tp->child; cp; cp = cp->next) {
          if (usev[pos + cp->sym] & USED) break;
        }

        // No child needs an in-use slot? We're done.
        if (!cp) break;
      }
      tp->state = pos;

      // Mark node's base and children as used:
      usev[pos] |= need;
      unsigned last = 0; // Make compiler happy
      int nkids = 0;
      for (cp = tp->child; cp; *dstp++ = cp, cp = cp->next, ++nkids)
        usev[last = pos + cp->sym] |= USED;

      // This is a HEURISTIC for advancing search for other nodes
      *startp += (pos - *startp) / nkids;

      if (last_trans < last) {
        last_trans = last;
        if (last + nsyms >= usev_size) {
          usev = static_cast<char*>(realloc(usev, usev_size << 1));
          memset(usev + usev_size, 0, usev_size);
          usev_size <<= 1;
        }
      }
    }
    *dstp = NULL;
  }
  free(usev);
  return last_trans + 1;
}

static void fill_tranv(ACISM *psp, TNODE const*tp)
{
  TNODE const *cp = tp->child;

  if (cp && tp->back)
    set_tran(psp, tp->state, 0, 0, 0, tp->back->state);

  for (; cp; cp = cp->next) {
    //NOTE: cp->match is (strno+1) so that !cp->match means "no match".
    set_tran(psp, tp->state, cp->sym, cp->match, cp->is_suffix,
             cp->child ? cp->state : cp->match - 1 + psp->tran_size);
    if (cp->child)
      fill_tranv(psp, cp);
  }
}

static void fill_hashv(ACISM *psp, TNODE const treev[], int nnodes)
{
  STRASH *sv = static_cast<STRASH*>(malloc(psp->hash_mod * sizeof*sv)), *sp = sv;
  int i;

  // First pass: insert without resolving collisions.
  for (i = 0; i < nnodes; ++i) {
    unsigned base = treev[i].state;
    TNODE const *tp;
    for (tp = treev[i].child; tp; tp = tp->next) {
      if (tp->match && tp->child) {
        unsigned state = base + tp->sym;
        STRASH *hp = &psp->hashv[p_hash(psp, state)];
        *(hp->state ? sp++ : hp) = (STRASH){state, tp->match - 1};
      }
    }
  }

  while (--sp >= sv) {
    for (i = p_hash(psp, sp->state); psp->hashv[i].state; ++i);
      psp->hashv[i] = *sp;
  }

  free(sv);
}

int
acism_more(ACISM const *psp, MEMREF const text,
           ACISM_ACTION *cb, void *context, int *statep)
{
  char const *cp = text.ptr, *endp = cp + text.len;
  unsigned state = *statep;
  int ret = 0;

  while (cp < endp) {
    unsigned sym = psp->symv[(uint8_t)*cp++];
    if (!sym) {
      // Input byte is not in any pattern string.
      state = ROOT;
      continue;
    }

    // Search for a valid transition from this (state, sym),
    //  following the backref chain.

    // 沿着backlink搜索，直到找到有效匹配位置，或者抵达根节点
    unsigned next;
    while (!t_valid(psp, next = p_tran(psp, state, sym)) && state != ROOT) {
      unsigned back = p_tran(psp, state, BACK);
      state = t_valid(psp, back) ? t_next(psp, back) : ROOT;
    }

    // 还是没有匹配节点，尝试字符串的下个位置 此时必然处于 ROOT
    if (!t_valid(psp, next))
      continue;

    if (!(next & (IS_MATCH | IS_SUFFIX))) {
      // No complete match yet; keep going.
      state = t_next(psp, next);
      continue;
    }

    // At this point, one or more patterns have matched.
    // Find all matches by following the backref chain.
    // A valid node for (sym) with no SUFFIX flag marks the
    //  end of the suffix chain.
    // In the same backref traversal, find a new (state),
    //  if the original transition is to a leaf.

    unsigned s = state;

    // Initially state is ROOT. The chain search saves the
    //  first state from which the next char has a transition.
    state = t_isleaf(psp, next) ? 0 : t_next(psp, next);

    while (1) {
      if (t_valid(psp, next)) {
        if (next & IS_MATCH) {
          unsigned strno, ss = s + sym, i;
          if (t_isleaf(psp, psp->tranv[ss])) {
            strno = t_strno(psp, psp->tranv[ss]);
          } else {
            for (i = p_hash(psp, ss); psp->hashv[i].state != ss; ++i); // 基于hash的搜索，加速
            strno = psp->hashv[i].strno;
          }

          if ((ret = cb(strno, cp - text.ptr, context)))
            return *statep = state, ret;
        }
        // If the original match was a leaf, state was set to 0, to be set
        //  The first node in the backref chain with a forward transition
        if (!state && !t_isleaf(psp, next))
          state = t_next(psp, next);
        if ( state && !(next & IS_SUFFIX))
          break;
      }

      if (s == ROOT)
        break;

      unsigned b = p_tran(psp, s, BACK);
      s = t_valid(psp, b) ? t_next(psp, b) : ROOT;
      next = p_tran(psp, s, sym);
    }
  }

  return *statep = state, ret;
}
