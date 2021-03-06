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
#include <string.h>
#include <ctype.h>

#include "util++.H"

//  Filters out matches that are too short.
//
//  Original implementation in Python by Clark Mobarry.

void
readHeader(char *inLine, FILE *in, uint32 &minLength, FILE *out) {
  bool  printedLength = false;

  fgets(inLine, 1024, in);
  while (!feof(in) && (inLine[0] != 'M')) {

    if (strncmp(inLine, "/globalMatchMinSize", 18) == 0) {
      if (minLength > 0) {
        //  Skip any whitespace, the =, and more whitespace.  Copy.
        char *tmp = inLine + 14;
        while (isspace(*tmp))  tmp++;
        while (*tmp == '=') tmp++;
        while (isspace(*tmp))  tmp++;
        minLength = strtouint32(tmp, 0L);
      }
      sprintf(inLine, "/globalMatchMinSize="uint32FMT"\n", minLength);
      printedLength = true;
    }

    if (out)
      fputs(inLine, out);

    fgets(inLine, 1024, in);
  }

  if (printedLength == false)
    fprintf(stdout, "/globalMatchMinSize="uint32FMT"\n", minLength);

  if (minLength == 0) {
    fprintf(stderr, "I didn't find /globalMatchMinSize, please set it with -l\n");
    exit(1);
  }
}


int
main(int argc, char **argv) {
  char      inLine[1024]       = {0};
  uint32    minLength          = 0;
  uint32    totalDumped        = 0;
  uint32    totalDumpedLength  = 0;
  uint32    totalSaved         = 0;
  uint32    totalSavedLength   = 0;

  int arg = 1;
  while (arg < argc) {
    if (strcmp(argv[arg], "-l") == 0) {
      minLength = strtouint32(argv[++arg], 0L);
    } else {
      fprintf(stderr, "usage: %s [-h] [-l length] < matches.atac > matches.atac\n", argv[0]);
      fprintf(stderr, "       filters out all matches less than 'length' long.\n");
      exit(1);
    }
    arg++;
  }

  readHeader(inLine, stdin, minLength, stdout);

  //  we need to parse the header to get globalMatchMinSize,
  //  and then let the command line override it.  just make
  //  a custom readHeader() for here, do it there.  nothing
  //  difficult.

  while (!feof(stdin)) {
    if (inLine[0] == 'M') {
      splitToWords  S(inLine);

      if ((strtouint32(S[ 6], 0L) >= minLength) &&
          (strtouint32(S[10], 0L) >= minLength)) {
        totalSaved++;
        totalSavedLength += strtouint32(S[ 6], 0L);
        fputs(inLine, stdout);
      } else {
        totalDumped++;
        totalDumpedLength += strtouint32(S[ 6], 0L);
      }
    } else {
      fputs(inLine, stdout);
    }

    fgets(inLine, 1024, stdin);
  }

  fprintf(stderr, "lengthFilter:  Discarded "uint32FMTW(8)" matches with total length "uint32FMTW(10)", %7.3f%% of the sequence in matches.\n",
          totalDumped, totalDumpedLength, (double)totalDumpedLength / (totalDumpedLength + totalSavedLength) * 100.0);
  fprintf(stderr, "lengthFilter:  Saved     "uint32FMTW(8)" matches with total length "uint32FMTW(10)", %7.3f%% of the sequence in matches.\n",
          totalSaved, totalSavedLength, (double)totalSavedLength / (totalDumpedLength + totalSavedLength) * 100.0);
}
