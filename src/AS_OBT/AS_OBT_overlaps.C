
/**************************************************************************
 * This file is part of Celera Assembler, a software program that
 * assembles whole-genome shotgun reads into contigs and scaffolds.
 * Copyright (C) 2013, J. Craig Venter Institute.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received (LICENSE.txt) a copy of the GNU General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *************************************************************************/

static const char *rcsid = "$Id:  $";

#include "AS_OBT_overlaps.H"

uint32
loadOverlaps(uint32         iid,
             OVSoverlap   *&ovl,
             uint32        &ovlLen,
             uint32        &ovlMax,
             OverlapStore  *ovlPrimary,
             OverlapStore  *ovlSecondary) {

  //  Allocate initial space

  if (ovl == NULL) {
    ovlLen = 0;
    ovlMax = 65 * 1024;
    ovl    = new OVSoverlap [ovlMax];
  }

  //  Return if the overlaps are for the current read, or some future read.

  if (iid <= ovl[0].a_iid)
    return(ovlLen);

  //  Until we load the correct overlap, repeat.

  do {
    //  Count the number of overlaps to load
    ovlLen  = 0;
    ovlLen += AS_OVS_readOverlapsFromStore(ovlPrimary,   NULL, 0, AS_OVS_TYPE_ANY);
    ovlLen += AS_OVS_readOverlapsFromStore(ovlSecondary, NULL, 0, AS_OVS_TYPE_ANY);

    //  Quit now if there are no overlaps.  This simplifies the rest of the loop.
    if (ovlLen == 0)
      return(0);

    //  Allocate space for these overlaps.
    while (ovlMax < ovlLen) {
      ovlMax *= 2;
      delete [] ovl;
      ovl = new OVSoverlap [ovlMax];
    }

    //  Load the overlaps
    ovlLen  = 0;
    ovlLen += AS_OVS_readOverlapsFromStore(ovlPrimary,   ovl + ovlLen, ovlMax - ovlLen, AS_OVS_TYPE_ANY);
    ovlLen += AS_OVS_readOverlapsFromStore(ovlSecondary, ovl + ovlLen, ovlMax - ovlLen, AS_OVS_TYPE_ANY);

    //fprintf(stderr, "LOADED %d overlaps for a_iid %d\n", ovlLen, ovl[0].a_iid);

    //  If we read overlaps for a fragment after 'iid', we're done.  The client will properly save
    //  these overlaps until the iid becomes active.  And just in case it doesn't, we return above
    //  if the iid passed in is less than the current overlap.
    //
    if (iid <= ovl[0].a_iid)
      return(ovlLen);

    //  On the otherhand, if we read overlaps for a fragment before 'iid', we can either keep reading
    //  until we find the overlaps for this fragment, or jump to the correct spot to read overlaps.
    //
    //  The rule is simple.  If we're within 50 of the correct IID, keep streaming.  Otherwise, make
    //  a jump.  AS_OVS_setRangeOverlapStore() seems to ALWAYS close and open a file, which is somewhat
    //  expensive, especially if the file doesn't actually change.
    //
    if (50 < iid - ovl[0].a_iid) {
      AS_OVS_setRangeOverlapStore(ovlPrimary,   iid, UINT32_MAX);
      AS_OVS_setRangeOverlapStore(ovlSecondary, iid, UINT32_MAX);
    }
  } while (ovl[0].a_iid < iid);

  return(ovlLen);
}
