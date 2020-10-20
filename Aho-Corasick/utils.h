#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdlib.h>
#include <stdio.h>

extern char const *errname[];

typedef struct { char *ptr; size_t len; }	MEMBUF;
typedef struct { char const *ptr; size_t len; } MEMREF;

#define	NILBUF		(MEMBUF){NULL,0}
#define	NILREF		(MEMREF){NULL,0}

inline void buffree(MEMBUF buf) {
  free(buf.ptr);
}

inline MEMBUF membuf(int size) {
  return size ? (MEMBUF){static_cast<char*>(calloc(size+1, 1)), static_cast<size_t>(size)} : NILBUF;
}

MEMBUF read_file(const char *filename);
MEMBUF chomp(MEMBUF buf);  // split according space character
MEMREF* refsplit(char *text, char sep, int *pcount);
double  tick(void);
void    die(char const *fmt, ...);

#endif /* _UTILS_H_ */
