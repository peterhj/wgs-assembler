#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "bio.h"
#include "sim4.H"

//  Reports the amount of sequence covered by ALL matches for that
//  sequence.  Example: if sequence iid 4 has two matches, one
//  covering the first 30% and the second covering the last 30%, this
//  will report that sequence iid 4 is covered 60%.
//
//  Takes no options, reads from stdin, writes to stdout.

int
main(int argc, char **argv) {
  uint32                covMax    = 0;
  intervalList        **cov       = 0L;
  uint32               *len       = 0L;

  uint32                lastIID   = 0;

  bool                  isRaw     = false;
  bool                  isBlast   = false;

  char                 *fastaname = 0L;
  char                 *covname   = 0L;

  seqCache             *F = 0L;

  FILE                 *C = stdout;

  int arg=1;
  int err=0;
  while (arg < argc) {
    if        (strcmp(argv[arg], "-mask") == 0) {
      fastaname = argv[++arg];

    } else if (strcmp(argv[arg], "-cov") == 0) {
      covname   = argv[++arg];

    } else if (strcmp(argv[arg], "-raw") == 0) {
      isRaw = true;

    } else if (strcmp(argv[arg], "-blast") == 0) {
      isBlast = true;

    } else {
      fprintf(stderr, "unknown arg: '%s'\n", argv[arg]);
      err++;
    }
    arg++;
  }
  if ((err) || (isatty(fileno(stdin)))) {
    fprintf(stderr, "usage: %s [-mask in.fasta] [-cov dat] [-raw | -blast] < sim4db-results\n", argv[0]);
    fprintf(stderr, "       -mask    Read sequences from in.fasta, lower-case mask\n");
    fprintf(stderr, "                any base with an alignment, write to out.fasta\n");
    fprintf(stderr, "       -cov     Write coverage statistics to 'dat' instead of stdout\n");
    fprintf(stderr, "       -raw     If present, assume the 'sim4db-results' are\n");
    fprintf(stderr, "                a space-separated list of 'iid begin end', one per line\n");
    fprintf(stderr, "       -blast   Same idea as raw, expects 'UID.IID' for query id,\n");
    fprintf(stderr, "                blast format (-m) 9.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Output on stdout is the masked sequence if -mask is specified,\n");
    fprintf(stderr, "otherwise, it is the coverage statistics.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "-mask is almost a required option - we need it to get the length.\n");
    fprintf(stderr, "of sequences with no mapping (100%% uncovered) and to get the\n");
    fprintf(stderr, "number of sequences.\n");
    fprintf(stderr, "\n");
    if (isatty(fileno(stdin)))
      fprintf(stderr, "error: I cannot read polishes from the terminal!\n\n");
  }

  if (fastaname) {
    C = 0L;
    F = new seqCache(fastaname);
  }

  if (covname) {
    errno = 0;
    C     = fopen(covname, "w");
    if (errno)
      fprintf(stderr, "Failed to open '%s' for write: %s\n", covname, strerror(errno)), exit(1);
  }

  covMax   = 1024 * 1024;
  if (F)
    covMax = F->getNumberOfSequences();
  cov      = new intervalList * [covMax];
  len      = new uint32 [covMax];

  fprintf(stderr, "Found "uint32FMT" sequences in the input file.\n", covMax);

  for (uint32 i=0; i<covMax; i++) {
    cov[i] = 0L;
    len[i] = 0;
  }

  if (isRaw || isBlast) {
    char          inLine[1024];
    splitToWords  S;

    while (!feof(stdin)) {
      fgets(inLine, 1024, stdin);
      S.split(inLine);

      uint32  iid=0, beg=0, end=0;

      if (isRaw) {
        iid = strtouint32(S[0], 0L);
        beg = strtouint32(S[1], 0L) - 1;   //  Convert to space-based
        end = strtouint32(S[2], 0L);
      }
      if (isBlast) {
        char *iii = S[0];
        while ((*iii != '.') && (*iii))
          iii++;
        iii++;
        if (*iii == 0)
          fprintf(stderr, "UID.IID error: '%s'\n", S[0]);

        iid = strtouint32(iii, 0L);
        beg = strtouint32(S[6], 0L) - 1;   //  Convert to space-based
        end = strtouint32(S[7], 0L);
      }

      if (iid >= covMax) {
        fprintf(stderr, "ERROR:  Found iid "uint32FMT", but only allocated "uint32FMT" places!\n",
                iid, covMax);
        exit(1);
      }
      if (cov[iid] == 0L) {
        cov[iid] = new intervalList;
        len[iid] = 0;
      }
      if (iid >= lastIID) {
        lastIID = iid + 1;
      }
      cov[iid]->add(beg, end-beg);
    }

  } else {
    sim4polishReader *R = new sim4polishReader("-");
    sim4polish       *p = 0L;

    while (R->nextAlignment(p)) {
      if (p->_estID > covMax)
        fprintf(stderr, "DIE!  You have more sequences in your polishes than in your source!\n"), exit(1);

      if (p->_estID >= covMax) {
        fprintf(stderr, "ERROR:  Found iid "uint32FMT", but only allocated "uint32FMT" places!\n",
                p->_estID, covMax);
        exit(1);
      }
      if (cov[p->_estID] == 0L) {
        cov[p->_estID] = new intervalList;
        len[p->_estID] = p->_estLen;
      }
      if (p->_estID >= lastIID) {
        lastIID = p->_estID + 1;
      }

      for (uint32 e=0; e<p->_numExons; e++) {
        p->_exons[e]._estFrom--;        //  Convert to space-based

        if (p->_matchOrientation == SIM4_MATCH_FORWARD)
          cov[p->_estID]->add(p->_exons[e]._estFrom,
                              p->_exons[e]._estTo - p->_exons[e]._estFrom);
        else
          cov[p->_estID]->add(p->_estLen - p->_exons[e]._estTo,
                              p->_exons[e]._estTo - p->_exons[e]._estFrom);
      }
    }
  }


  //  Scan the list of intervalLists, compute the amount covered, print.
  //
  for (uint32 iid=0; iid<lastIID; iid++) {

    //  Argh!  If there are no intervals, we need to report the whole
    //  sequence is uncovered!

    uint32  numRegions  = 0;
    uint32  sumLengths  = 0;
    uint32  l, h;

    //  Save the number of regions and the sum of their lengths,
    //  then merge regions
    //
    if (cov[iid]) {
      numRegions = cov[iid]->numberOfIntervals();
      sumLengths = cov[iid]->sumOfLengths();
      cov[iid]->merge();
    }

    if (F) {
      seqInCore *S = F->getSequenceInCore(iid);

      if (len[iid] == 0)
        len[iid] = S->sequenceLength();

      assert(len[iid] == S->sequenceLength());

      char   *seq = new char [len[iid] + 1];
      strcpy(seq, S->sequence());

      for (uint32 p=0; p<len[iid]; p++)
        seq[p] = toUpper[seq[p]];

      if (cov[iid]) {
        for (uint32 c=0; c<cov[iid]->numberOfIntervals(); c++) {
          l = cov[iid]->lo(c);
          h = cov[iid]->hi(c);

          if (h > len[iid]) {
            fprintf(stderr, "ERROR:  range "uint32FMT"-"uint32FMT" out of bounds (seqLen = "uint32FMT")\n",
                    l, h, len[iid]);
            assert(h <= len[iid]);
          }

          for (uint32 p=l; p<h; p++)
            //seq[p] = toLower[seq[p]];
            seq[p] = 'N';
        }
      }

      fprintf(stdout, "%s\n%s\n", S->header(), seq);

      delete [] seq;
      delete    S;
    }

    if (C) {
      double  percentCovered = 0.00;

      if (cov[iid])
        percentCovered = cov[iid]->sumOfLengths() / (double)len[iid];

      fprintf(C, uint32FMT"\t"uint32FMT"\t%5.3f\t"uint32FMT"\t"uint32FMT"\n",
              iid,
              len[iid],
              percentCovered,
              numRegions,
              sumLengths);
    }
  }
}
