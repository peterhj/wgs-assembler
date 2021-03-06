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

#include "atac.H"

static
uint32
decodeAtacName(char *atac,
               char *label) {
  if (label) {
    while (*atac && (*atac != ':'))
      *label++ = *atac++;
    *label = 0;
  } else {
    while (*atac && (*atac != ':'))
      atac++;
  }
  if (*atac)
    return(strtouint32(atac+1, 0L));
  return(~uint32ZERO);
}


atacMatch::atacMatch(char *line) {
  decode(line);
}

atacMatch::atacMatch(char *muid,
                     char *puid,
                     uint32 miid,
                     char *t,
                     uint32 i1, uint32 p1, uint32 l1, uint32 f1,
                     uint32 i2, uint32 p2, uint32 l2, uint32 f2) {

  strncpy(matchuid,  muid, 16);
  strncpy(parentuid, puid, 16);

  matchuid[15]  = 0;
  parentuid[15] = 0;

  matchiid = miid;

  type[0] = 0;
  type[1] = 0;
  type[2] = 0;
  type[3] = 0;

  type[0] = t[0];
  type[1] = t[1];
  if (t[1])
    type[2] = t[2];

  iid1 = i1;
  pos1 = p1;
  len1 = l1;
  fwd1 = f1;
  iid2 = i2;
  pos2 = p2;
  len2 = l2;
  fwd2 = f2;
}

void
atacMatch::decode(char *line) {
  iid1 = 0;
  pos1 = 0;
  len1 = 0;
  fwd1 = 0;
  iid2 = 0;
  pos2 = 0;
  len2 = 0;
  fwd2 = 0;

  splitToWords  S(line);

  iid1 = decodeAtacName(S[4], 0L);
  pos1 = strtouint32(S[5], 0L);
  len1 = strtouint32(S[6], 0L);
  fwd1 = (S[7][0] == '-') ? 0 : 1;
  iid2 = decodeAtacName(S[8], 0L);
  pos2 = strtouint32(S[9], 0L);
  len2 = strtouint32(S[10], 0L);
  fwd2 = (S[11][0] == '-') ? 0 : 1;

  strncpy(matchuid,  S[2], 16);
  strncpy(parentuid, S[3], 16);

  matchuid[15]  = 0;
  parentuid[15] = 0;

  matchiid = 0;

  type[0] = 0;
  type[1] = 0;
  type[2] = 0;
  type[3] = 0;

  type[0] = S[1][0];
  type[1] = S[1][1];
  if (S[1][1])
    type[2] = S[1][2];
}


//  Sanity check the match record -- make sure it's within the
//  sequence itself.
//
bool
atacMatch::sanity(seqCache *A, seqCache *B, char *inLine) {

  bool matchOK = true;

  if (A && B) {
    if ((pos1) > A->getSequenceLength(iid1) || (pos1 + len1) > A->getSequenceLength(iid1)) {
      chomp(inLine);
      fprintf(stderr, "Match longer than sequence (by "uint32FMT"bp) in 1: seqLen="uint32FMTW(8)" %s\n",
              pos1 + len1 - A->getSequenceLength(iid1),
              A->getSequenceLength(iid1), inLine);
      matchOK = false;
    }

    if ((pos2) > B->getSequenceLength(iid2) || (pos2 + len2) > B->getSequenceLength(iid2)) {
      chomp(inLine);
      fprintf(stderr, "Match longer than sequence (by "uint32FMT"bp) in 2: seqLen="uint32FMTW(8)" %s\n",
              pos2 + len2 - B->getSequenceLength(iid2),
              B->getSequenceLength(iid2), inLine);
      matchOK = false;
    }

    if ((iid1 >= A->getNumberOfSequences()) || (iid2 >= B->getNumberOfSequences())) {
      chomp(inLine);
      fprintf(stderr, "Match references invalid sequence iid: %s\n", inLine);
      matchOK = false;
    }
  }

  return(matchOK);
}

