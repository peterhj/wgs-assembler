// This file is part of A2Amapper.
// Copyright (c) 2005 J. Craig Venter Institute
// Author: Brian Walenz
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received (LICENSE.txt) a copy of the GNU General Public 
// License along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "bio++.H"
#include "seqCache.H"
#include "atac.H"

#define ANNOTATE
#define EXTRAMATCHES

//  Generates a histogram of the exact match block sizes
//  Counts to global number of mismatches
//  Annotates each match with the number of mismatches
//  Checks for identities outside matches


void
updateExactBlockHistogram(uint32 *blockHistogram, uint32 blockMatches) {

  if (blockMatches > 8 * 1024 * 1024)
    blockHistogram[0]++;
  else
    blockHistogram[blockMatches]++;
}



int
main(int argc, char *argv[]) {

  int arg=1;
  while (arg < argc) {
    if        (strcmp(argv[arg], "-h") == 0) {
      //  Generate a histogram of exact-match lengths
    } else if (strcmp(argv[arg], "-a") == 0) {
      //  Annotate each match with the percent error, compute
      //  the global percent error.
    } else if (strcmp(argv[arg], "-e") == 0) {
      //  Generate a histogram of the percent error in each match
    } else if (strcmp(argv[arg], "-c") == 0) {
      //  Check the edges of each match to ensure there isn't a match
    } else {
      fprintf(stderr, "usage: %s [-h exact-match-histogram] [-a] [-e error-histogram] [-c]\n", argv[0]);
      fprintf(stderr, "  -h:     histogram of the length of the exact match blocks\n");
      fprintf(stderr, "  -a:     annotate each match with the percent error, write to stdout\n");
      fprintf(stderr, "  -e:     histogram of the error rate of each match\n");
      fprintf(stderr, "  -c:     check that the next base on each side is a mismatch\n");
      exit(1);
    }
    arg++;
  }

  uint32   globalSequence   = 0;
  uint32   globalMismatches = 0;
  uint32   blockMatches     = 0;
  uint32  *blockHistogram   = new uint32 [8 * 1024 * 1024];

  for (uint32 x=0; x<8*1024*1024; x++)
    blockHistogram[x] = 0;

  atacFile       AF("-");
  atacMatchList &ML = *AF.matches();

  seqCache  *C1 = new seqCache(AF.assemblyFileA(), 1, false);
  seqCache  *C2 = new seqCache(AF.assemblyFileA(), 1, false);

  for (uint32 mi=0; mi<ML.numberOfMatches(); mi++) {
    atacMatch *m = ML.getMatch(mi);

    seqInCore  *S1 = C1->getSequenceInCore(m->iid1);
    seqInCore  *S2 = C2->getSequenceInCore(m->iid2);

    FastAAccessor A1(S1, false);
    FastAAccessor A2(S2, (m->fwd1 != m->fwd2));

    A1.setRange(m->pos1, m->len1);
    A2.setRange(m->pos2, m->len2);

    uint32 localMismatches = 0;

#ifdef EXTRAMATCHES
    uint32 extraMatchesL = 0;
    uint32 extraMatchesR = 0;

    //  Check for matches on either side of the region.

    A1.setPosition(m->pos1);
    A2.setPosition(m->pos2);
    --A1;
    --A2;
    while (A1.isValid() &&
           A2.isValid() &&
           (letterToBits[(int)*A1] != 0xff)&&
           (letterToBits[(int)*A2] != 0xff) &&
           IUPACidentity[(int)*A1][(int)*A2]) {
      extraMatchesL++;
      --A1;
      --A2;
    }

    A1.setPosition(m->pos1 + m->len1 - 1);
    A2.setPosition(m->pos2 + m->len2 - 1);
    ++A1;
    ++A2;
    while (A1.isValid() &&
           A2.isValid() &&
           (letterToBits[(int)*A1] != 0xff)&&
           (letterToBits[(int)*A2] != 0xff) &&
           IUPACidentity[(int)*A1][(int)*A2]) {
      extraMatchesR++;
      ++A1;
      ++A2;
    }

    //  WARN if we found extra identities

#if 0
    if (extraMatchesL + extraMatchesR > 0) {
      A1.setPosition(m->pos1);
      A2.setPosition(m->pos2);

      chomp(inLine);
      fprintf(stderr, "WARNING: found "uint32FMT" extra matches to the left and "uint32FMT" extra matches to the right in %s\n",
              extraMatchesL, extraMatchesR, inLine);

#if 0
      for (uint32 ii=0; ii<m->len1; ii++, ++A1)
        fprintf(stdout, "%c", *A1);
      fprintf(stdout, "\n");

      for (uint32 ii=0; ii<m->len1; ii++, ++A2)
        fprintf(stdout, "%c", *A2);
      fprintf(stdout, "\n");
#endif
    }
#endif

#endif  //  EXTRAMATCHES


    A1.setPosition(m->pos1);
    A2.setPosition(m->pos2);
    for (uint32 ii=0; ii<m->len1; ii++, ++A1, ++A2) {

      //  Count global matches / mismatches
      //
      globalSequence++;
      if (!((letterToBits[(int)*A1] != 0xff) &&
            (letterToBits[(int)*A2] != 0xff) &&
            IUPACidentity[(int)*A1][(int)*A2])) {
        globalMismatches++;
        localMismatches++;
      }

      //  Histogram of exact match block lengths
      //
      if ((letterToBits[(int)*A1] != 0xff) &&
          (letterToBits[(int)*A2] != 0xff) &&
          IUPACidentity[(int)*A1][(int)*A2]) {
        blockMatches++;
      } else {
        updateExactBlockHistogram(blockHistogram, blockMatches);
        blockMatches = 0;
      }
    }

    //  Finish off stuff
    //
    updateExactBlockHistogram(blockHistogram, blockMatches);
    blockMatches = 0;

    //  If annotate, emit a new record.
  }


  //  Report stuff
  //
  fprintf(stderr, "globalSequence   = "uint32FMT"\n", globalSequence);
  fprintf(stderr, "globalMismatches = "uint32FMT"\n", globalMismatches);

#if 0
  FILE *O = fopen("MismatchCounter.block.histogram.out", "w");
  for (uint32 i=0; i<8 * 1024 * 1024; i++)
    fprintf(O, uint32FMT" "uint32FMT"\n", i, blockHistogram[i]);
  fclose(O);
#endif

  return(0);
}
