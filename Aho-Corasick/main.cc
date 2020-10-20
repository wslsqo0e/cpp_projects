#include <stdio.h>
#include <fstream>
#include "acism.h"

static int actual = 0, details = 1;

static int on_match(int strnum, int textpos, MEMREF const *pattv)
{
  (void)strnum, (void)textpos, (void)pattv;
  ++actual;
  if (details) fprintf(stderr, "%9d %7d '%.*s'\n", textpos, strnum, (int)pattv[strnum].len, pattv[strnum].ptr);
  return 0;
}

int main(int argc, char *argv[])
{
  if (argc < 2 || argc > 4) {
    fprintf(stderr, "%s pattern_file target_file [[-]expected]\ne.g. %s patts act.txt -5\n",
            argv[0], argv[0]);
    return 1;
  }

  MEMBUF patt = chomp(read_file(argv[1]));

  if (!patt.ptr)
    die("cannot read %s", argv[1]);

  int npatts;
  MEMREF *pattv = refsplit(patt.ptr, '\n', &npatts);

  double t = tick();
  ACISM *psp = acism_create(pattv, npatts);
  printf("%p\n", psp);
  t = tick() - t;

  fprintf(stdout, "acism_create(pattv[%d]) compiled, in %.3f secs\n", npatts, t);

  if (argc > 2) {
    int state = 0;

    details = 1;
    std::ifstream ifs(argv[2]);
    if (!ifs) {
      die("cannot open %s", argv[2]);
    }
  }

  if (argc > 2) {
    int			state = 0;

    details = 0;
    std::ifstream ifs(argv[2]);
    // printf("%s\n", argv[2]);
    if (!ifs) {
      die("cannot open %s", argv[2]);
    }
    std::string line;
    while (std::getline(ifs, line)) {
      actual = 0;
      MEMREF text = {line.c_str(), line.size()};
      (void)acism_more(psp, text, (ACISM_ACTION*)on_match, pattv, &state);
      if (actual >= 2) {
          printf("%s\n", line.c_str());
      }
    }
  }
  buffree(patt);
  free(pattv);
  acism_destroy(psp);
  return 0;
}
