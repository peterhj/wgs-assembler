#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util++.H"


intervalList::intervalList(uint32 initialSize) {
  _isSorted = true;
  _isMerged = true;
  _listLen  = 0;
  _listMax  = initialSize;
  _list     = new _intervalPair [_listMax];
}


intervalList::~intervalList() {
  delete [] _list;
}


intervalList &
intervalList::operator=(intervalList &src) {
  _isSorted = src._isSorted;
  _isMerged = src._isMerged;
  _listLen = src._listLen;

  if (_listMax < src._listMax) {
    delete [] _list;
    _listMax = src._listMax;
    _list    = new _intervalPair [_listMax];
  }

  memcpy(_list, src._list, _listLen * sizeof(_intervalPair));

  return(*this);
}


void
intervalList::add(intervalNumber position, intervalNumber length) {

  if (_listLen >= _listMax) {
    _listMax *= 2;
    _intervalPair *l = new _intervalPair [_listMax];
    memcpy(l, _list, sizeof(_intervalPair) * _listLen);
    delete [] _list;
    _list = l;
  }

  _list[_listLen].lo   = position;
  _list[_listLen].hi   = position + length;
  _list[_listLen].ct   = 1;

#if 0
  //  Aborted attempt to add a data field here.  Got stuck
  //  deciding how to handle merges lightweight

  _list[_listLen].data = 0L;

  if (data != ~uint64ZERO) {
    _list[_listLen].dataLen = 1;
    _list[_listLen].dataMax = 4;
    _list[_listLen].data    = new uint64 [_list[_listLen].dataMax];
    _list[_listLen].data[0] = data;
  }
#endif
    
  if ((_listLen > 0) &&
      (_list[_listLen-1].lo > _list[_listLen].lo)) {
    _isSorted = false;
    _isMerged = false;
  }

  _listLen++;
}


static
int
intervalList_sort_helper(const void *a, const void *b) {
  _intervalPair *A = (_intervalPair *)a;
  _intervalPair *B = (_intervalPair *)b;

  if (A->lo < B->lo) return(-1);
  if (A->lo > B->lo) return(1);
  if (A->hi < B->hi) return(-1);
  if (A->hi > B->hi) return(1);
  return(0);
}


void
intervalList::sort(void) {

  if (_isSorted)
    return;

  if (_listLen > 1)
    qsort(_list, _listLen, sizeof(_intervalPair), intervalList_sort_helper);

  _isSorted = true;
}


void
intervalList::merge(uint32 minOverlap) {
  uint32  thisInterval  = 0;
  uint32  nextInterval = 1;

  if (_listLen < 2)
    return;

  sort();

  while (nextInterval < _listLen) {

    if ((_list[thisInterval].lo == 0) &&
        (_list[thisInterval].hi == 0)) {

      //  Our interval is empty.  Copy in the interval we are
      //  examining and move to the next.

      //  XXX This is probably useless, thisInterval should always be
      //  valid.

      _list[thisInterval].lo = _list[nextInterval].lo;
      _list[thisInterval].hi = _list[nextInterval].hi;
      _list[thisInterval].ct = _list[nextInterval].ct;

      _list[nextInterval].lo = 0;
      _list[nextInterval].hi = 0;
      nextInterval++;
    } else {

      //  This interval is valid.  See if it overlaps with the next
      //  interval.

      bool  intersects = false;

      if ((_list[thisInterval].lo <= _list[nextInterval].lo) &&
          (_list[nextInterval].hi <= _list[thisInterval].hi))
        //  next is contained in this
        intersects = true;

      if (_list[thisInterval].hi - minOverlap >= _list[nextInterval].lo)
        //  next has thick overlap to this
        intersects = true;


      if (intersects) {

        //  Got an intersection.

        //  Merge nextInterval into thisInterval -- the hi range
        //  is extended if the nextInterval range is larger.
        //
        if (_list[thisInterval].hi < _list[nextInterval].hi)
          _list[thisInterval].hi = _list[nextInterval].hi;

        _list[thisInterval].ct += _list[nextInterval].ct;
        
        //  Clear the just merged nextInterval and move to the next one.
        //
        _list[nextInterval].lo = 0;
        _list[nextInterval].hi = 0;
        _list[nextInterval].ct = 0;
        nextInterval++;
      } else {

        //  No intersection.  Move along.  Nothing to see here.

        //  If there is a gap between the target and the examine (we
        //  must have merged sometime in the past), copy examine to
        //  the next target.

        thisInterval++;

        if (thisInterval != nextInterval) {
          _list[thisInterval].lo = _list[nextInterval].lo;
          _list[thisInterval].hi = _list[nextInterval].hi;
          _list[thisInterval].ct = _list[nextInterval].ct;
        }

        nextInterval++;
      }
    }
  }

  if (thisInterval+1 < _listLen)
    _listLen = thisInterval + 1;

  _isMerged = true;
}


void
intervalList::invert(intervalNumber lo, intervalNumber hi) {

  if (!_isSorted || !_isMerged) {
    fprintf(stderr, "intervalList::invert()--  ERROR!  List is not sorted or not merged!\n");
    exit(1);
  }

  //  Create a new list to store the inversion
  //
  uint32             invLen = 0;
  uint32             invMax = _listLen + 2;
  _intervalPair     *inv    = new _intervalPair [invMax];

  //  Add the first
  //
  if (lo < _list[0].lo) {
    inv[invLen].lo = lo;
    inv[invLen].hi = _list[0].lo;
    invLen++;
  }

  //  Add the pieces
  for (uint32 i=1; i<_listLen; i++) {
    if (_list[i-1].hi < _list[i].lo) {
      inv[invLen].lo = _list[i-1].hi;
      inv[invLen].hi = _list[i].lo;
      invLen++;
    }
  }

  //  Add the last
  if (_list[_listLen-1].hi < hi) {
    inv[invLen].lo = _list[_listLen-1].hi;
    inv[invLen].hi = hi;
    invLen++;
  }

  //  Nuke the old list, swap in the new one
  delete [] _list;

  _list = inv;
  _listLen = invLen;
  _listMax = invMax;
}



void
intervalList::merge(intervalList *IL) {
  //bool  isSorted = _isSorted;
  //bool  isMerged = _isMerged;

  for (uint32 i=0; i<IL->_listLen; i++)
    add(IL->_list[i].lo, IL->_list[i].hi - IL->_list[i].lo);

  //if (isSorted)  sort();
  //if (isMerged)  merge();
}



void
intervalList::intersect(intervalList &A,
                        intervalList &B) {
  A.merge();
  B.merge();

  uint32  ai = 0;
  uint32  bi = 0;

  while ((ai < A.numberOfIntervals()) &&
         (bi < B.numberOfIntervals())) {
    uint32   al = A.lo(ai);
    uint32   ah = A.hi(ai);
    uint32   bl = B.lo(bi);
    uint32   bh = B.hi(bi);
    uint32   nl = 0;
    uint32   nh = 0;

    //  If they intersect, make a new region
    //
    if ((al <= bl) && (bl < ah)) {
      nl = bl;
      nh = (ah < bh) ? ah : bh;
    }

    if ((bl <= al) && (al < bh)) {
      nl = al;
      nh = (ah < bh) ? ah : bh;
    }

    if (nl < nh)
      add(nl, nh - nl);

    //  Advance the list with the earlier region.
    //
    if        (ah < bh) {
      //  A ends before B
      ai++;
    } else if (ah > bh) {
      //  B ends before A
      bi++;
    } else {
      //  Exactly the same ending!
      ai++;
      bi++;
    }
  }
}



//  Populates an array with the intervals that are within the supplied interval.
//
//  Naive implementation that is easy to verify (and that works on an unsorted list).
//
uint32
intervalList::overlapping(intervalNumber    rangelo,
                          intervalNumber    rangehi,
                          uint32          *&intervals,
                          uint32           &intervalsLen,
                          uint32           &intervalsMax) {

  if (intervals == 0L) {
    intervalsMax = 256;
    intervals    = new uint32 [intervalsMax];
  }

  intervalsLen = 0;

  for (uint32 i=0; i<_listLen; i++) {
    if ((rangelo <= _list[i].hi) &&
        (rangehi >= _list[i].lo)) {
      if (intervalsLen >= intervalsMax) {
        intervalsMax *= 2;
        uint32 *X = new uint32 [intervalsMax];
        memcpy(X, intervals, sizeof(uint32) * intervalsLen);
        delete [] intervals;
        intervals = X;
      }

      intervals[intervalsLen++] = i;
    }
  }

  return(intervalsLen);
}



static
int
intervalDepth_sort_helper(const void *a, const void *b) {
  intervalDepthRegions *A = (intervalDepthRegions *)a;
  intervalDepthRegions *B = (intervalDepthRegions *)b;

  if (A->pos < B->pos) return(-1);
  if (A->pos > B->pos) return(1);
  return(0);
}


intervalDepth::intervalDepth(intervalList &IL) {
  uint32                 idlen = IL.numberOfIntervals() * 2;
  intervalDepthRegions  *id    = new intervalDepthRegions [idlen];

  for (uint32 i=0; i<IL.numberOfIntervals(); i++) {
    id[2*i  ].pos = IL.lo(i);
    id[2*i  ].cha = 1;
    id[2*i+1].pos = IL.hi(i);
    id[2*i+1].cha = -1;
  }

  qsort(id, idlen, sizeof(intervalDepthRegions), intervalDepth_sort_helper);
  computeIntervals(id, idlen);

  delete [] id;
}


intervalDepth::intervalDepth(intervalDepthRegions *id, uint32 idlen) {
  qsort(id, idlen, sizeof(intervalDepthRegions), intervalDepth_sort_helper);
  computeIntervals(id, idlen);
}


void
intervalDepth::computeIntervals(intervalDepthRegions *id, uint32 idlen) {

  //  Scan the list, counting how many times we change depth.
  //
  _listMax = 1;
  for (uint32 i=1; i<idlen; i++) {
    if (id[i-1].pos != id[i].pos)
      _listMax++;
  }

  //  Allocate the real depth of coverage intervals
  //
  _listLen = 0;
  _list    = new _intervalDepth [_listMax];

  //  Build new intervals

  //  Init first interval.
  _list[_listLen].lo = id[0].pos;
  _list[_listLen].hi = id[0].pos;
  _list[_listLen].de = id[0].cha;

  for (uint32 i=1; i<idlen; i++) {
    //  Update the end of the current interval.
    _list[_listLen].hi = id[i].pos;

    //fprintf(stderr, "ID %d %d - cur %d %d-%d %d\n",
    //        id[i].pos, id[i].cha,
    //        _listLen, _list[_listLen].lo, _list[_listLen].hi, _list[_listLen].de);

    //  If the position is different than the last, make a new interval
    if (id[i-1].pos != id[i].pos) {
      _listLen++;

      _list[_listLen].lo = id[i].pos;
      _list[_listLen].de = _list[_listLen-1].de;
    }

    //  Finally, update the depth and end of the current interval
    _list[_listLen].hi  = id[i].pos;
    _list[_listLen].de += id[i].cha;

    //fprintf(stderr, "ID %d %d - cur %d %d-%d %d POST\n",
    //        id[i].pos, id[i].cha,
    //        _listLen, _list[_listLen].lo, _list[_listLen].hi, _list[_listLen].de);
  }

  assert(_listLen > 0);
  assert(_listLen < _listMax);
}

intervalDepth::~intervalDepth() {
  delete [] _list;
}
