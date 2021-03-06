#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "sim4.H"

int
main(int argc, char ** argv) {
  int          c[101] = {0};
  int          i[101] = {0};

  if (isatty(fileno(stdin))) {
    fprintf(stderr, "creates three files:\n");
    fprintf(stderr, "  coverage.histogram\n");
    fprintf(stderr, "  identity.histogram\n");
    fprintf(stderr, "  c-vs-i.scatter\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "error: I cannot read polishes from the terminal!\n\n");
    exit(1);
  }

  FILE *C = fopen("coverage.histogram", "w");
  FILE *I = fopen("identity.histogram", "w");
  FILE *S = fopen("c-vs-i.scatter", "w");

  sim4polishReader *R = new sim4polishReader("-");
  sim4polish       *p = 0L;

  while (R->nextAlignment(p)) {
    fprintf(S, uint32FMT" "uint32FMT"\n", p->_percentIdentity, p->_querySeqIdentity);

    i[p->_percentIdentity]++;
    c[p->_querySeqIdentity]++;
  }

  for (int x=0; x<101; x++) {
    fprintf(C, "%d\n", c[x]);
    fprintf(I, "%d\n", i[x]);
  }

  fclose(C);
  fclose(I);
  fclose(S);

  return(0);
}
