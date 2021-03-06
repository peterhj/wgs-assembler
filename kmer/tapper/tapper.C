#include "tapperTag.H"
#include "tapperResult.H"
#include "tapperAlignment.H"
#include "tapperHit.H"
#include "tapperGlobalData.H"
#include "tapperThreadData.H"
#include "tapperComputation.H"

#undef VERBOSEWORKER

//  Very expensive.  Compare the obvious O(n^2) happy mate finding
//  algorithm against the O(n) algorithm.
//
#undef DEBUG_MATES


void*
tapperReader(void *G) {
  tapperGlobalData  *g = (tapperGlobalData  *)G;
  tapperComputation *s = 0L;
  tapperTag          a, b;

  if (g->TF->metaData()->isPairedTagFile()) {
    if (g->TF->get(&a, &b))
      s = new tapperComputation(&a, &b);
  } else {
    if (g->TF->get(&a))
      s = new tapperComputation(&a, 0L);
  }

  return(s);
}



void
tapperWriter(void *G, void *S) {
  tapperGlobalData  *g = (tapperGlobalData  *)G;
  tapperComputation *s = (tapperComputation *)S;
  tapperResultIndex  result;

  //  Build the result index.

  result._tag1id = s->tag1id;
  result._tag2id = s->tag2id;

  result._maxColrMismatchMapped = g->maxColorError;
  result._maxBaseMismatchMapped = g->maxBaseError;

  result._mean   = g->TF->metaData()->mean();
  result._stddev = g->TF->metaData()->stddev();

  if (s->resultFragmentLen > g->repeatThreshold) {
    result._numFrag          = 0;
    result._numFragDiscarded = s->resultFragmentLen;
  } else {
    result._numFrag          = s->resultFragmentLen;
    result._numFragDiscarded = 0;
  }

  result._numFragSingleton  = s->resultSingletonLen;
  result._numFragTangled    = s->resultTangledAlignmentLen;
  result._numMated          = s->resultMatedLen;
  result._numTangled        = s->resultTangledLen;

  result._pad1 = 0;
  result._pad2 = 0;

  //  Now write.

  g->TA->write(&result,
               s->resultFragment,
               s->resultSingleton,
               s->resultTangledAlignment,
               s->resultMated,
               s->resultTangled,
               s->alignQualHistogram);

  delete s;
}




//  Compose the colors from beg to end.
//
inline
char
composeColors(char *colors, uint32 beg, uint32 end) {
  char c = colors[beg];

  for (uint32 x=beg; x<end; x++)
    c = baseToColor[c][colors[x]];

  return(c);
}



//  Returns true if the the i and j errors result in a consistent
//  encoding, and they're not too far away.  Consistent in that the
//  sequence before agrees and the sequence after agrees.
//
inline
bool
isConsistent(char     *ref, char     *tag,
             uint32    i,   uint32    j) {
  return(composeColors(ref, i, j) == composeColors(tag, i, j));
}



//  Analyze tag[] and ref[], correct differences, call base changes.
//  Return an ACGT sequence for the tag.
//
//  Compose the colors together.  At points where the compositions
//  disagree, the base at that point is different.  The composition
//  tells us how to transform the reference letter to the base at
//  this position, in one step.
//
//  If our final composed value is different, then either we end on
//  a SNP, or we have an error somewhere.  The choice here is
//  arbitrary, and made depending on where that error is.
//
bool
tapperHit::alignToReference(tapperGlobalData *g,
                            uint32  so_in,
                            uint32  po_in,
                            char   *tag_in, uint32 len_in) {

  //  This function is NOT a bottleneck.  Don't bother optimizing.

  //  so_in and po_in are the sequence iid and position in that
  //  sequence where the tag maps.
  //
  //  tag_in is the full tag with reference base at start or end,
  //  either T010203010331 or 01031031033G.  len_in is the length of
  //  the COLOR CALLS in tag_in, NOT the strlen of it.
  //

  uint32      errs = 0;               //  number of errors
  uint32      errp[TAG_LEN_MAX];      //  location of the errors
  uint32      errc[TAG_LEN_MAX];      //  status of confirmed or error

  char       _tagCOREC[TAG_LEN_MAX];  //  For holding corrected color calls, only to generate ACGT align

  _seqIdx = so_in;
  _seqPos = po_in;
  _tagIdx = 0;

  //  _rev: Yeah, we assume ASCII and UNIX newlines all over the
  //  place.  A forward read starts with a reference base; reverse
  //  reads have a number here.
  //
  //  _len -- the length of the tag + reference base.
  //       -- number of color calls / ACGT + 1.
  //
  _pad                = 0;
  _len                = len_in + 1;
  _rev                = (tag_in[0] < 'A') ? true : false;

  _basesMismatch      = len_in;  //  Set at end

  _colorMismatch      = 0;       //  Set when parsing errors
  _colorInconsistent  = 0;       //  Set when parsing errors

  _tagCOLOR[0] = 0;
  _tagCOREC[0] = 0;
  _refCOLOR[0] = 0;

  _tagACGT[0] = 0;
  _refACGT[0] = 0;


  //  Copy the tag.
  //
  //  A bit of devilish trickery to make a reverse read look like a
  //  forward read - we locally reverse the reference and read,
  //  process as if the reverse read is a forward read, then clean up
  //  at the end.  See tapperComputation.H for what is in the reverse
  //  tag.
  //
  {
    if (_rev) {
      for (uint32 i=0, j=_len-1; i<_len; i++, j--)
        _tagCOLOR[i] = _tagCOREC[i] = tag_in[j];
      _tagCOLOR[0] = _tagCOREC[0] = complementSymbol[_tagCOLOR[0]];
    } else {
      for (uint32 i=0; i<_len; i++)
        _tagCOLOR[i] = _tagCOREC[i] = tag_in[i];
    }
    _tagCOLOR[_len] = 0;
    _tagCOREC[_len] = 0;
  }


  //  Copy the reference and convert the genomic sequence to
  //  color space using the reference base of the read.
  //
  {
    char     *seq = g->GS->getSequenceInCore(so_in)->sequence();

    strncpy(_refACGT, seq + po_in, _len-1);
    _refACGT[_len-1] = 0;

    if (_rev)
      reverseComplementSequence(_refACGT, _len-1);

    _refCOLOR[0] = _tagCOLOR[0];  //  ALWAYS the reference encoding base, as long as we copy the tag first.
    _refCOLOR[1] = baseToColor[_refCOLOR[0]][_refACGT[0]];

    for (uint32 ti=2; ti<_len; ti++)
      _refCOLOR[ti] = baseToColor[_refACGT[ti-2]][_refACGT[ti-1]];

    _refCOLOR[_len] = 0;
  }

  //fprintf(stderr, "tag: %s %s ref: %s %s\n", tag_in, _tagCOLOR, _refCOLOR, _refACGT);

  //  Count the number of color space errors
  //
  //  Note that errp[] is actaully 1-based; the first position is
  //  never an error; it's the reference base.

  for (uint32 ti=1; ti<_len; ti++) {
    if (_tagCOLOR[ti] != _refCOLOR[ti]) {
      errp[errs] = ti;
      errc[errs] = 0;
      errs++;
    }
  }

  //
  //  The following if blocks correct single color errors using very
  //  complicated rules.
  //


  if        (errs == 0) {
    _colorMismatch     = 0;
    _colorInconsistent = 0;

  } else if (errs == 1) {
    //  Always corrected, just to get an ACGT alignment.  We can't
    //  tell if the color mismatch is an error, or if the error is
    //  adjacent to the mismatch, which would have resulted in a valid
    //  SNP.
    _colorMismatch     = 0;
    _colorInconsistent = 1;
    _tagCOREC[errp[0]] = _refCOLOR[errp[0]];

  } else if (errs == 2) {
    bool  ok21  = isConsistent(_refCOLOR, _tagCOLOR, 1, _len) && (errp[1] - errp[0] < 4);

    if (ok21) {
      //  MNP of size 4.
      _colorMismatch     = 2;
      _colorInconsistent = 0;
      errc[0] = 1;
      errc[1] = 1;
    } else {
      //  Correct 'em.
      _colorMismatch     = 0;
      _colorInconsistent = 2;
      _tagCOREC[errp[0]] = _refCOLOR[errp[0]];
      _tagCOREC[errp[1]] = _refCOLOR[errp[1]];
    }

  } else if (errs == 3) {
    bool   ok21 = isConsistent(_refCOLOR, _tagCOLOR,         1, errp[2]) && (errp[1] - errp[0] < 4);
    bool   ok22 = isConsistent(_refCOLOR, _tagCOLOR, errp[0]+1, _len)    && (errp[2] - errp[1] < 4);

    bool   ok31 = isConsistent(_refCOLOR, _tagCOLOR,         1, _len)    && (errp[2] - errp[0] < 5);

    if        (ok31) {
      //  MNP of size 5
      _colorMismatch     = 3;
      _colorInconsistent = 0;
      errc[0] = 1;
      errc[1] = 1;
      errc[2] = 1;
    } else if (ok21) {
      //  First two ok, fix the third.
      _colorMismatch     = 2;
      _colorInconsistent = 1;
      _tagCOREC[errp[2]] = _refCOLOR[errp[2]];
      errc[0] = 1;
      errc[1] = 1;
    } else if (ok22) {
      //  Last two ok, fix the first.
      _colorMismatch     = 2;
      _colorInconsistent = 1;
      _tagCOREC[errp[0]] = _refCOLOR[errp[0]];
      errc[1] = 1;
      errc[2] = 1;
    } else {
      //  Nothing consistent, fix all of 'em.
      _colorMismatch     = 0;
      _colorInconsistent = 3;
      _tagCOREC[errp[0]] = _refCOLOR[errp[0]];
      _tagCOREC[errp[1]] = _refCOLOR[errp[1]];
      _tagCOREC[errp[2]] = _refCOLOR[errp[2]];
    }

  } else if (errs == 4) {
    bool   ok21 = isConsistent(_refCOLOR, _tagCOLOR,         1, errp[2]) && (errp[1] - errp[0] < 4);
    bool   ok22 = isConsistent(_refCOLOR, _tagCOLOR, errp[0]+1, errp[2]) && (errp[2] - errp[1] < 4);
    bool   ok23 = isConsistent(_refCOLOR, _tagCOLOR, errp[1]+1, _len)    && (errp[3] - errp[2] < 4);

    bool   ok31 = isConsistent(_refCOLOR, _tagCOLOR,         1, errp[3]) && (errp[2] - errp[0] < 5);
    bool   ok32 = isConsistent(_refCOLOR, _tagCOLOR, errp[0]+1, _len)    && (errp[3] - errp[1] < 5);

    bool   ok41 = isConsistent(_refCOLOR, _tagCOLOR,         1, _len)    && (errp[3] - errp[0] < 6);

    //  With two exceptions, exactly one of the ok's will be true.
    //  The exceptions are:
    //
    //  a) ok21 and ok23 will imply ok41.  However there is nothing to
    //  correct here.  We just need to make sure that we stop
    //  processing rules on ok41.
    //
    //  b) ok41 and ok22.  Not sure if this can ever happen, but like
    //  case a, we're ok if we stop after ok41.
    //

    if        (ok41) {
      //  MNP of size 6
      _colorMismatch     = 4;
      _colorInconsistent = 0;
      errc[0] = 1;
      errc[1] = 1;
      errc[2] = 1;
      errc[3] = 1;
    } else if (ok31) {
      //  First three ok, fix the last one.
      _colorMismatch     = 3;
      _colorInconsistent = 1;
      _tagCOREC[errp[3]] = _refCOLOR[errp[3]];
      errc[0] = 1;
      errc[1] = 1;
      errc[2] = 1;
    } else if (ok32) {
      //  Last three ok, fix the first one.
      _colorMismatch     = 3;
      _colorInconsistent = 1;
      _tagCOREC[errp[0]] = _refCOLOR[errp[0]];
      errc[1] = 1;
      errc[2] = 1;
      errc[3] = 1;
    } else if (ok21) {
      //  First two ok, fix the last two.
      _colorMismatch     = 2;
      _colorInconsistent = 2;
      _tagCOREC[errp[2]] = _refCOLOR[errp[2]];
      _tagCOREC[errp[3]] = _refCOLOR[errp[3]];
      errc[0] = 1;
      errc[1] = 1;
    } else if (ok22) {
      //  Middle two ok, fix the outties.
      _colorMismatch     = 2;
      _colorInconsistent = 2;
      _tagCOREC[errp[0]] = _refCOLOR[errp[0]];
      _tagCOREC[errp[3]] = _refCOLOR[errp[3]];
      errc[1] = 1;
      errc[2] = 1;
    } else if (ok23) {
      //  Last two ok, fix the first two.
      _colorMismatch     = 2;
      _colorInconsistent = 2;
      _tagCOREC[errp[0]] = _refCOLOR[errp[0]];
      _tagCOREC[errp[1]] = _refCOLOR[errp[1]];
      errc[2] = 1;
      errc[3] = 1;
    } else {
      //  Nothing consistent, fix all of 'em.
      _colorMismatch     = 0;
      _colorInconsistent = 4;
      _tagCOREC[errp[0]] = _refCOLOR[errp[0]];
      _tagCOREC[errp[1]] = _refCOLOR[errp[1]];
      _tagCOREC[errp[2]] = _refCOLOR[errp[2]];
      _tagCOREC[errp[3]] = _refCOLOR[errp[3]];
    }
  } else if (errs == 5) {
    //fprintf(stderr, "Five errors detected.  Code doesn't know what to do.\n");
    _colorMismatch     = 0;
    _colorInconsistent = 5;
  } else if (errs == 6) {
    //fprintf(stderr, "Six errors detected.  Code doesn't know what to do.\n");
    _colorMismatch     = 0;
    _colorInconsistent = 6;
  } else {
    //fprintf(stderr, "Wow, you got a lot of errors.  Code doesn't know what to do.\n");
    _colorMismatch     = 0;
    _colorInconsistent = errs;
  }

  //  Too many errors already?  Fail.
  //
  if (_colorMismatch + _colorInconsistent > g->maxColorError)
    return(false);

  //  Compute alignments of corrected color strings.

  _basesMismatch = 0;

  _tagACGT[0] = baseToColor[_tagCOREC[0]][_tagCOREC[1]];
  _refACGT[0] = baseToColor[_refCOLOR[0]][_refCOLOR[1]];
  for (uint32 ti=1; ti<_len; ti++) {
    _tagACGT[ti] = baseToColor[_tagACGT[ti-1]][_tagCOREC[ti+1]];
    _refACGT[ti] = baseToColor[_refACGT[ti-1]][_refCOLOR[ti+1]];
  }
  _tagACGT[_len-1] = 0;
  _refACGT[_len-1] = 0;

  for (uint32 ti=0; ti<_len-1; ti++) {
    if (_tagACGT[ti] != _refACGT[ti]) {
      _basesMismatch++;

      _tagACGT[ti] = toUpper[_tagACGT[ti]];
      _refACGT[ti] = toUpper[_refACGT[ti]];
    }
  }

  if (_rev) {
    //  Undo the tag and ref reversals.
    _tagCOLOR[0] = complementSymbol[_tagCOLOR[0]];
    reverseString(_tagCOLOR, _len);

    _tagCOREC[0] = complementSymbol[_tagCOREC[0]];
    reverseString(_tagCOREC, _len);

    _refCOLOR[0] = complementSymbol[_refCOLOR[0]];
    reverseString(_refCOLOR, _len);

    //  Reverse complement the alignments

    reverseComplementSequence(_tagACGT, _len-1);
    reverseComplementSequence(_refACGT, _len-1);

    //  Adjust the error positions...once we start caring about positions.

    for (uint32 x=0; x<errs; x++)
      errp[x] = _len - errp[x];
  }

  //  Too much ACGT difference?  Fail.
  //
  if (_basesMismatch > g->maxBaseError)
    return(false);

  //fprintf(stderr, "tag: %s %s ref: %s %s "uint32FMT" "uint32FMT" "uint32FMT"\n",
  //        tag_in, _tagCOLOR, _refCOLOR, _refACGT, _basesMismatch, _colorMismatch, _colorInconsistent);

  //  Stuff the errors into the hit.

  uint32 nn = 0;

  for (uint32 x=0; x<errs; x++)
    if (errc[x] == 1)
      _tagColorDiffs[nn++] = (letterToBits[ _tagCOLOR[ errp[x] ] ] << 6) | errp[x];

  assert(nn == _colorMismatch);

  for (uint32 x=0; x<errs; x++)
    if (errc[x] == 0)
      _tagColorDiffs[nn++] = (letterToBits[ _tagCOLOR[ errp[x] ] ] << 6) | errp[x];

  assert(nn == _colorMismatch + _colorInconsistent);

  return(true);
}



//  The big value of this function is to convert from a chained
//  position to a (seqID,pos), and save the hit onto a bitpacked list.
//  This list can then be numerically sorted to order all hits.  Of
//  course, we could have just sorted the original chained positions.
//
//  It saves (seqID,pos,isTag2,isReverse)
//
inline
void
tapperWorker_addHits(uint64    *posn, uint64 posnLen,
                     tapperGlobalData   *g,
                     tapperComputation  *s,
                     bool                rev,
                     bool                tag1) {
  tapperHit  h;
  char      *tagseq;
  uint32     taglen;

  if (tag1) {
    tagseq = (rev) ? s->tag1rseq : s->tag1fseq;
    taglen = s->tag1size;
  } else {
    tagseq = (rev) ? s->tag2rseq : s->tag2fseq;
    taglen = s->tag2size;
  }

  for (uint32 i=0; i<posnLen; i++) {
    uint64  pos = posn[i];
    uint64  seq = g->SS->sequenceNumberOfPosition(pos);

    pos -= g->SS->startOf(seq);
    seq  = g->SS->IIDOf(seq);

    //  Search ignores first letter, align needs it.  This makes for a
    //  very special case, 0, which isn't a full match.

    if (pos > 0) {
      pos--;

      if (h.alignToReference(g, seq, pos, tagseq, taglen) == true)
        s->addHit(g, h, tag1);
    }
  }
}


void
tapperWorker(void *G, void *T, void *S) {
  tapperGlobalData  *g = (tapperGlobalData  *)G;
  tapperThreadData  *t = (tapperThreadData  *)T;
  tapperComputation *s = (tapperComputation *)S;

  //
  //  Get the hits.
  //

#ifdef VERBOSEWORKER
  fprintf(stderr, "GET HITS %s %s.\n", s->tag1fseq, s->tag2fseq);
#endif

  t->posn1fLen = t->posn1rLen = t->posn2fLen = t->posn2rLen = 0;

  if (s->tag1size > 0) {
    g->PS->getUpToNMismatches(s->tag1f, g->maxColorError, t->posn1f, t->posn1fMax, t->posn1fLen);
    g->PS->getUpToNMismatches(s->tag1r, g->maxColorError, t->posn1r, t->posn1rMax, t->posn1rLen);
  }

  if (s->tag2size > 0) {
    g->PS->getUpToNMismatches(s->tag2f, g->maxColorError, t->posn2f, t->posn2fMax, t->posn2fLen);
    g->PS->getUpToNMismatches(s->tag2r, g->maxColorError, t->posn2r, t->posn2rMax, t->posn2rLen);
  }

  //  Quit if nothing there.

  if (t->posn1fLen + t->posn1rLen + t->posn2fLen + t->posn2rLen == 0)
    return;

#ifdef VERBOSEWORKER
  fprintf(stderr, " raw hits: "uint64FMT" "uint64FMT" "uint64FMT" "uint64FMT"\n",
          t->posn1fLen, t->posn1rLen, t->posn2fLen, t->posn2rLen);
#endif

  //
  //  Align to reference to get rid of the 3/4 false hits.
  //

#ifdef VERBOSEWORKER
  fprintf(stderr, "ALIGN TO REFERENCE.\n");
#endif

  tapperWorker_addHits(t->posn1f, t->posn1fLen, g, s, false, true);
  tapperWorker_addHits(t->posn1r, t->posn1rLen, g, s, true,  true);

  tapperWorker_addHits(t->posn2f, t->posn2fLen, g, s, false, false);
  tapperWorker_addHits(t->posn2r, t->posn2rLen, g, s, true,  false);

  //  Quit if nothing there.

  if (s->tag1hitsLen + s->tag2hitsLen == 0)
    return;

  //
  //  If mated, tease out any valid mate relationships and build the
  //  results.  If fragment, just build.
  //

#ifdef VERBOSEWORKER
  fprintf(stderr, "REPORT.\n");
#endif

  //  OUTPUT CASE 1 - nothing.
  if ((s->tag1size == 0) && (s->tag2size == 0)) {
    assert(0);

    //  OUTPUT CASE 2 - unmated fragments
  } else if ((s->tag1size > 0) && (s->tag2size == 0)) {
    s->resultFragment    = new tapperResultFragment [s->tag1hitsLen];
    s->resultFragmentLen = s->tag1hitsLen;

    memset(s->resultFragment, 0, sizeof(tapperResultFragment) * s->tag1hitsLen);

    for (uint32 i=0; i<s->tag1hitsLen; i++) {
      s->resultFragment[i]._seq = s->tag1hits[i]._seqIdx;
      s->resultFragment[i]._pos = s->tag1hits[i]._seqPos;

      s->resultFragment[i]._qual._tag1valid             = 1;
      s->resultFragment[i]._qual._tag1basesMismatch     = s->tag1hits[i]._basesMismatch;
      s->resultFragment[i]._qual._tag1colorMismatch     = s->tag1hits[i]._colorMismatch;
      s->resultFragment[i]._qual._tag1colorInconsistent = s->tag1hits[i]._colorInconsistent;
      s->resultFragment[i]._qual._tag1rev               = s->tag1hits[i]._rev;

      s->resultFragment[i]._qual._diffSize = MAX_COLOR_MISMATCH_MAPPED;

      memcpy(s->resultFragment[i]._qual._tag1colorDiffs,
             s->tag1hits[i]._tagColorDiffs,
             sizeof(uint8) * MAX_COLOR_MISMATCH_MAPPED);
    }

    //  OUTPUT CASE 3 - unmated fragments (but wrong set, should always be in tag1)
  } else if ((s->tag1size == 0) && (s->tag2size > 0)) {
    assert(0);

    //  OUTPUT CASE 4 - mated fragments
  } else if ((s->tag1size > 0) && (s->tag2size > 0)) {
    if (t->tangle == 0L)
      t->tangle = new intervalList [g->GS->getNumberOfSequences()];

    if ((t->numHappiesMax < s->tag1hitsLen) || (t->numHappiesMax < s->tag2hitsLen)) {
      delete [] t->tag1happies;
      delete [] t->tag1mate;
      delete [] t->tag1tangled;

      delete [] t->tag2happies;
      delete [] t->tag2mate;
      delete [] t->tag2tangled;

      t->numHappiesMax = MAX(s->tag1hitsLen, s->tag2hitsLen) + 16 * 1024;

      fprintf(stderr, "Reallocate t->numHappiesMax to "uint32FMT"\n", t->numHappiesMax);

      t->tag1happies = new uint32 [t->numHappiesMax];
      t->tag1mate    = new uint32 [t->numHappiesMax];
      t->tag1tangled = new uint32 [t->numHappiesMax];

      t->tag2happies = new uint32 [t->numHappiesMax];
      t->tag2mate    = new uint32 [t->numHappiesMax];
      t->tag2tangled = new uint32 [t->numHappiesMax];
    }

#ifdef VERBOSEWORKER
    fprintf(stderr, "  Found "uint32FMT" and "uint32FMT" hits.\n", s->tag1hitsLen, s->tag2hitsLen);
#endif

    //  Sort by position.
    s->sortHitsByPosition();

    uint32  mean   = g->TF->metaData()->mean();
    uint32  stddev = g->TF->metaData()->stddev();

    tapperHit *t1h = s->tag1hits;
    tapperHit *t2h = s->tag2hits;

    //  Pass zero, clear.  Tangles are cleared below.
    //
    memset(t->tag1happies, 0, sizeof(uint32) * s->tag1hitsLen);
    memset(t->tag1tangled, 0, sizeof(uint32) * s->tag1hitsLen);
    memset(t->tag2happies, 0, sizeof(uint32) * s->tag2hitsLen);
    memset(t->tag2tangled, 0, sizeof(uint32) * s->tag2hitsLen);

    //  Pass one.  Count the number of times each fragment is in a
    //  happy relationship.
    //
    {
#ifdef DEBUG_MATES
      uint32  debug_numHappies = 0;
      uint64  debug_happyCheck = 0;

      for (uint32 a=0; a<s->tag1hitsLen; a++) {
        for (uint32 b=0; b<s->tag2hitsLen; b++) {
          if (t1h[a].happy(t2h[b], mean, stddev)) {
            debug_numHappies += 1;
            debug_happyCheck += t1h[a]._seqPos ^ t2h[b]._seqPos;
          }
        }
      }
#endif

      uint32  bbaserev = 0;
      uint32  bbasefor = 0;

      for (uint32 a=0; a<s->tag1hitsLen; a++) {

        //  Both lists of hits are sorted by position.  For each tag1 (a)
        //  hit, we first advance the bbase to the first hit that is
        //  within the proper distance before the a tag.  Then scan forward
        //  until the b tag is too far away to be mated.

        uint32 b = 0;

        if (t1h[a]._rev == true) {
          while ((bbaserev < s->tag2hitsLen) && (t1h[a].mateTooFarBefore(t2h[bbaserev], mean, stddev)))
            bbaserev++;
          b = bbaserev;
        } else {
          while ((bbasefor < s->tag2hitsLen) && (t1h[a].mateTooFarBefore(t2h[bbasefor], mean, stddev)))
            bbasefor++;
          b = bbasefor;
        }

        //  Now, until the b read is too far away to be mated, check
        //  for happiness and do stuff.

        for (; (b<s->tag2hitsLen) && (t1h[a].mateTooFarAfter(t2h[b], mean, stddev) == false); b++) {
          if (t1h[a].happy(t2h[b], mean, stddev)) {

#ifdef DEBUG_MATES
            debug_numHappies -= 1;
            debug_happyCheck -= t1h[a]._seqPos ^ t2h[b]._seqPos;
#endif

            //  Count.
            t->tag1happies[a]++;
            t->tag2happies[b]++;

            //  Add the previous mate pair if we just became tangled.
            //  It is possible for both to be == 2, but in that case,
            //  we've already added the previous mate pair.
            if ((t->tag1happies[a] == 2) && (t->tag2happies[b] == 1)) {
              uint32 c  = t->tag1mate[a];
              uint32 mn = MIN(t1h[a]._seqPos,               t2h[c]._seqPos);
              uint32 mx = MAX(t1h[a]._seqPos + s->tag1size, t2h[c]._seqPos + s->tag2size);

              t->tangle[t1h[a]._seqIdx].add(mn, mx-mn);
              t->tag1tangled[a]++;
              t->tag2tangled[c]++;
            }

            if ((t->tag1happies[a] == 1) && (t->tag2happies[b] == 2)) {
              uint32 c  = t->tag2mate[b];
              uint32 mn = MIN(t1h[c]._seqPos,               t2h[b]._seqPos);
              uint32 mx = MAX(t1h[c]._seqPos + s->tag1size, t2h[b]._seqPos + s->tag2size);

              t->tangle[t1h[c]._seqIdx].add(mn, mx-mn);
              t->tag1tangled[c]++;
              t->tag2tangled[b]++;
            }

            //  Finally, add the current mate pair to the tangle.
            if ((t->tag1happies[a] >= 2) || (t->tag2happies[b] >= 2)) {
              uint32 mn = MIN(t1h[a]._seqPos,               t2h[b]._seqPos);
              uint32 mx = MAX(t1h[a]._seqPos + s->tag1size, t2h[b]._seqPos + s->tag2size);

              t->tangle[t1h[a]._seqIdx].add(mn, mx-mn);
              t->tag1tangled[a]++;
              t->tag2tangled[b]++;
            }

            //  Remember the mate; only valid if tag1happies[a] and
            //  tag2happies[b] both == 1.
            t->tag1mate[a] = b;
            t->tag2mate[b] = a;
          }
        }
      }

#ifdef DEBUG_MATES
      if ((debug_numHappies != 0) || (debug_happyCheck != 0)) {
        FILE *df = fopen("tapper.DEBUG_MATES.err", "w");

        fprintf(df, "numHappies: "uint64FMT"\n", debug_numHappies);
        fprintf(df, "happyCheck: "uint64FMT"\n", debug_happyCheck);

        for (uint32 a=0; a<s->tag1hitsLen; a++)
          fprintf(df, "a="uint32FMT" ori=%c pos="uint32FMT","uint32FMT"\n",
                  a, t1h[a]._rev ? 'r' : 'f', t1h[a]._seqIdx, t1h[a]._seqPos);

        for (uint32 b=0; b<s->tag2hitsLen; b++)
          fprintf(df, "b="uint32FMT" ori=%c pos="uint32FMT","uint32FMT"\n",
                  b, t2h[b]._rev ? 'r' : 'f', t2h[b]._seqIdx, t2h[b]._seqPos);

        uint32  bbaserev = 0;
        uint32  bbasefor = 0;

        for (uint32 a=0; a<s->tag1hitsLen; a++) {
          uint32 b = 0;

          if (t1h[a]._rev == true) {
            while ((bbaserev < s->tag2hitsLen) && (t1h[a].mateTooFarBefore(t2h[bbaserev], mean, stddev))) {
              fprintf(df, "rev bbaserev <- "uint32FMT" + 1\n", bbaserev);
              bbaserev++;
            }
            b = bbaserev;
          } else {
            while ((bbasefor < s->tag2hitsLen) && (t1h[a].mateTooFarBefore(t2h[bbasefor], mean, stddev))) {
              fprintf(df, "rev bbasefor <- "uint32FMT" + 1\n", bbasefor);
              bbasefor++;
            }
            b = bbasefor;
          }

          for (; (b<s->tag2hitsLen) && (t1h[a].mateTooFarAfter(t2h[b], mean, stddev) == false); b++) {
            fprintf(df, "test a="uint32FMT" b="uint32FMT"\n", a, b);
            if (t1h[a].happy(t2h[b], mean, stddev)) {
              fprintf(df, "HAPPY CLEVER     a="uint32FMT" b="uint32FMT"\n", a, b);
            }
          }
        }

        for (uint32 a=0; a<s->tag1hitsLen; a++) {
          for (uint32 b=0; b<s->tag2hitsLen; b++) {
            if (t1h[a].happy(t2h[b], mean, stddev)) {
              fprintf(df, "HAPPY EXHAUSTIVE a="uint32FMT" b="uint32FMT"\n", a, b);
            }
          }
        }

        fclose(df);
      }
      assert(debug_numHappies == 0);
      assert(debug_happyCheck == 0);
#endif


#ifdef VERBOSEWORKER
      fprintf(stderr, "  Paired.\n");
#endif
    }

    //  Allocate space for the outputs.

#if 0
    //  We can kind of guess how much to grab.  Not perfect.  Can do a
    //  lot better.
    //
    s->resultFragmentLen          = s->tag1hitsLen + s->tag2hitsLen;
    s->resultSingletonLen         = s->tag1hitsLen + s->tag2hitsLen;
    s->resultTangledAlignmentLen  = s->tag1hitsLen + s->tag2hitsLen;
    s->resultMatedLen             = MIN(s->tag1hitsLen, s->tag2hitsLen);
    s->resultTangledLen           = MIN(s->tag1hitsLen, s->tag2hitsLen);
#else
    //  Count exactly how much space is needed.  The test for
    //  singleton vs fragment is somewhat expensive, so we skip it.
    //
    for (uint32 a=0; a<s->tag1hitsLen; a++) {
      if (t->tag1tangled[a] != 0) {
        s->resultTangledAlignmentLen++;
      } else if (t->tag1happies[a] == 1) {
        s->resultMatedLen++;
      } else {
        s->resultSingletonLen++;
        s->resultFragmentLen++;
      }
    }

    for (uint32 b=0; b<s->tag2hitsLen; b++) {
      if (t->tag2tangled[b] != 0) {
        s->resultTangledAlignmentLen++;
      } else if (t->tag2happies[b] == 1) {
        s->resultMatedLen++;
      } else {
        s->resultSingletonLen++;
        s->resultFragmentLen++;
      }
    }

    s->resultMatedLen /= 2;

    //s->resultFragmentLen          += 8;
    //s->resultSingletonLen         += 8;
    //s->resultTangledAlignmentLen  += 8;
    //s->resultMatedLen             += 8;
    //s->resultTangledLen           += 8;
#endif

    s->resultFragment             = new tapperResultFragment   [s->resultFragmentLen];
    s->resultSingleton            = new tapperResultFragment   [s->resultSingletonLen];
    s->resultTangledAlignment     = new tapperResultFragment   [s->resultTangledAlignmentLen];
    s->resultMated                = new tapperResultMated      [s->resultMatedLen];
    s->resultTangled              = new tapperResultTangled    [s->resultTangledLen];

    s->resultFragmentLen          = 0;
    s->resultSingletonLen         = 0;
    s->resultTangledAlignmentLen  = 0;
    s->resultMatedLen             = 0;
    s->resultTangledLen           = 0;

    //  For anything with zero happies, emit to the
    //  singleton file.

    for (uint32 a=0; a<s->tag1hitsLen; a++) {
      tapperResultFragment *f;

      if (t->tag1tangled[a] != 0) {
        f = s->resultTangledAlignment + s->resultTangledAlignmentLen++;

      } else if (t->tag1happies[a] == 1) {
        //  Happy; do nothing.  We'll do it later.
        f = 0L;

      } else if (s->tag1hits[a].happyNearEnd(true, mean, stddev, g->GS->getSequenceLength(s->tag1hits[a]._seqIdx))) {
        f = s->resultSingleton + s->resultSingletonLen++;

      } else {
        f = s->resultFragment  + s->resultFragmentLen++;
      }

      if (f) {
        memset(f, 0, sizeof(tapperResultFragment));

        f->_seq = s->tag1hits[a]._seqIdx;
        f->_pos = s->tag1hits[a]._seqPos;

        f->_qual._tag1valid             = 1;
        f->_qual._tag1basesMismatch     = s->tag1hits[a]._basesMismatch;
        f->_qual._tag1colorMismatch     = s->tag1hits[a]._colorMismatch;
        f->_qual._tag1colorInconsistent = s->tag1hits[a]._colorInconsistent;
        f->_qual._tag1rev               = s->tag1hits[a]._rev;

        f->_qual._diffSize = MAX_COLOR_MISMATCH_MAPPED;

        memcpy(f->_qual._tag1colorDiffs,
               s->tag1hits[a]._tagColorDiffs,
               sizeof(uint8) * MAX_COLOR_MISMATCH_MAPPED);
      }
    }

    for (uint32 b=0; b<s->tag2hitsLen; b++) {
      tapperResultFragment *f;

      if (t->tag2tangled[b] != 0) {
        f = s->resultTangledAlignment + s->resultTangledAlignmentLen++;

      } else if (t->tag2happies[b] == 1) {
        //  Happy; do nothing.  We'll do it later.
        f = 0L;

      } else if (s->tag2hits[b].happyNearEnd(false, mean, stddev, g->GS->getSequenceLength(s->tag2hits[b]._seqIdx))) {
        f = s->resultSingleton + s->resultSingletonLen++;

      } else {
        f = s->resultFragment + s->resultFragmentLen++;
      }

      if (f) {
        memset(f, 0, sizeof(tapperResultFragment));

        f->_seq = s->tag2hits[b]._seqIdx;
        f->_pos = s->tag2hits[b]._seqPos;

        f->_qual._tag2valid             = 1;
        f->_qual._tag2basesMismatch     = s->tag2hits[b]._basesMismatch;
        f->_qual._tag2colorMismatch     = s->tag2hits[b]._colorMismatch;
        f->_qual._tag2colorInconsistent = s->tag2hits[b]._colorInconsistent;
        f->_qual._tag2rev               = s->tag2hits[b]._rev;

        f->_qual._diffSize = MAX_COLOR_MISMATCH_MAPPED;

        memcpy(f->_qual._tag2colorDiffs,
               s->tag2hits[b]._tagColorDiffs,
               sizeof(uint8) * MAX_COLOR_MISMATCH_MAPPED);
      }
    }

    //  For anything with a pair of single happies, emit to the happy
    //  mate file.

    for (uint32 a=0; a<s->tag1hitsLen; a++) {
      uint32 b = t->tag1mate[a];

      if ((t->tag1happies[a] == 1) && (t->tag2happies[b] == 1)) {
        tapperResultMated     *m = s->resultMated + s->resultMatedLen++;

        memset(m, 0, sizeof(tapperResultMated));

        assert(t->tag1mate[a] == b);
        assert(t->tag2mate[b] == a);

        m->_seq  = s->tag1hits[a]._seqIdx;
        m->_pos1 = s->tag1hits[a]._seqPos;
        m->_pos2 = s->tag2hits[b]._seqPos;

        m->_qual._tag1valid             = 1;
        m->_qual._tag1basesMismatch     = s->tag1hits[a]._basesMismatch;
        m->_qual._tag1colorMismatch     = s->tag1hits[a]._colorMismatch;
        m->_qual._tag1colorInconsistent = s->tag1hits[a]._colorInconsistent;
        m->_qual._tag1rev               = s->tag1hits[a]._rev;

        m->_qual._tag2valid             = 1;
        m->_qual._tag2basesMismatch     = s->tag2hits[b]._basesMismatch;
        m->_qual._tag2colorMismatch     = s->tag2hits[b]._colorMismatch;
        m->_qual._tag2colorInconsistent = s->tag2hits[b]._colorInconsistent;
        m->_qual._tag2rev               = s->tag2hits[b]._rev;

        m->_qual._diffSize = MAX_COLOR_MISMATCH_MAPPED;

        memcpy(m->_qual._tag1colorDiffs,
               s->tag1hits[a]._tagColorDiffs,
               sizeof(uint8) * MAX_COLOR_MISMATCH_MAPPED);
        memcpy(m->_qual._tag2colorDiffs,
               s->tag2hits[b]._tagColorDiffs,
               sizeof(uint8) * MAX_COLOR_MISMATCH_MAPPED);
      }
    }

    //  Emit and then clear the tangles.

    {
      uint32 simax = g->GS->getNumberOfSequences();

      for (uint32 si=0; si<simax; si++) {

        if (t->tangle[si].numberOfIntervals() > 0) {
          t->tangle[si].merge();

          for (uint32 ti=0; ti<t->tangle[si].numberOfIntervals(); ti++) {
            tapperResultTangled   *x = s->resultTangled + s->resultTangledLen++;

            x->_tag1count = 0;
            x->_tag2count = 0;

            x->_seq = si;

            x->_bgn = t->tangle[si].lo(ti);
            x->_end = t->tangle[si].hi(ti);

            for (uint32 a=0; a<s->tag1hitsLen; a++) {
              if ((t->tag1tangled[a] > 0) &&
                  (x->_seq == s->tag1hits[a]._seqIdx) &&
                  (x->_bgn <= s->tag1hits[a]._seqPos) && (s->tag1hits[a]._seqPos <= x->_end))
                x->_tag1count++;
            }
            for (uint32 b=0; b<s->tag2hitsLen; b++) {
              if ((t->tag2tangled[b] > 0) &&
                  (x->_seq == s->tag2hits[b]._seqIdx) &&
                  (x->_bgn <= s->tag2hits[b]._seqPos) && (s->tag2hits[b]._seqPos <= x->_end))
                x->_tag2count++;
            }
          }

          //  This is persistent; clear it for the next mate pair.
          t->tangle[si].clear();
        }
      }
    }
  }
}



int
main(int argc, char **argv) {
  tapperGlobalData  *g = new tapperGlobalData();

  fprintf(stderr, "sizeof(tapperResultIndex) --       "sizetFMT"\n", sizeof(tapperResultIndex));
  fprintf(stderr, "sizeof(tapperResultQV) --          "sizetFMT"\n", sizeof(tapperResultQV));
  fprintf(stderr, "sizeof(tapperResultFragment) --    "sizetFMT"\n", sizeof(tapperResultFragment));
  fprintf(stderr, "sizeof(tapperResultMated) --       "sizetFMT"\n", sizeof(tapperResultMated));
  fprintf(stderr, "sizeof(tapperResultTangled) --     "sizetFMT"\n", sizeof(tapperResultTangled));
  fprintf(stderr, "sizeof(tapperHit) --               "sizetFMT"\n", sizeof(tapperHit));
  fprintf(stderr, "sizeof(tapperTag) --               "sizetFMT"\n", sizeof(tapperTag));

  int arg=1;
  int err=0;
  while (arg < argc) {
    if        (strncmp(argv[arg], "-genomic", 2) == 0) {
      g->genName = argv[++arg];
    } else if (strncmp(argv[arg], "-queries", 2) == 0) {
      g->qryName = argv[++arg];
    } else if (strncmp(argv[arg], "-output", 2) == 0) {
      g->outName = argv[++arg];

    } else if (strncmp(argv[arg], "-begin", 2) == 0) {
      g->bgnRead = strtouint32(argv[++arg], 0L);
      g->thisPartition = 0;
      g->numPartitions = 1;

    } else if (strncmp(argv[arg], "-end", 2) == 0) {
      g->endRead = strtouint32(argv[++arg], 0L);
      g->thisPartition = 0;
      g->numPartitions = 1;

    } else if (strncmp(argv[arg], "-partition", 2) == 0) {
      g->thisPartition = strtouint32(argv[++arg], 0L);
      g->numPartitions = strtouint32(argv[++arg], 0L);

    } else if (strncmp(argv[arg], "-repeatthreshold", 2) == 0) {
      g->repeatThreshold = strtouint32(argv[++arg], 0L);

    } else if (strncmp(argv[arg], "-maxcolorerror", 5) == 0) {
      g->maxColorError = strtouint32(argv[++arg], 0L);

    } else if (strncmp(argv[arg], "-maxbaseerror", 5) == 0) {
      g->maxBaseError = strtouint32(argv[++arg], 0L);

    } else if (strncmp(argv[arg], "-maxmemory", 5) == 0) {
      g->maxMemory = atoi(argv[++arg]);

    } else if (strncmp(argv[arg], "-threads", 2) == 0) {
      g->numThreads = atoi(argv[++arg]);

    } else if (strncmp(argv[arg], "-verbose", 2) == 0) {
      g->beVerbose = true;
    } else {
      fprintf(stderr, "%s: unknown option '%s'\n", argv[0], argv[arg]);
      err++;
    }
    arg++;
  }
  if ((err > 0) || (g->genName == 0L) || (g->qryName == 0L) || (g->outName == 0L)) {
    fprintf(stderr, "usage: %s [opts]\n", argv[0]);
    fprintf(stderr, "\n");
    fprintf(stderr, "  MANDATORY\n");
    fprintf(stderr, "          -genomic genomic.fasta\n");
    fprintf(stderr, "          -queries tags.tapperTags\n");
    fprintf(stderr, "          -output  tapperResultFile directory path\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  OPTIONAL\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "          -begin b               Start aligning at read b (or mate pair b)\n");
    fprintf(stderr, "          -end   e               Stop aligning at read e (or mate pair e)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "          -partition n m         Run partition n out of m total partitions.\n");
    fprintf(stderr, "                                 This sets -b and -e so that the reads/mate pairs\n");
    fprintf(stderr, "                                 are in m partitions.  Partitions start at 0 and\n");
    fprintf(stderr, "                                 end at m-1.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "          -repeatthreshold x     Do not report fragment alignments for tags\n");
    fprintf(stderr, "                                 with more than x alignments.  Singletons, mated\n");
    fprintf(stderr, "                                 tags and are still reported and computed using\n");
    fprintf(stderr, "                                 all alignments.  The default is "uint32FMT".\n", g->repeatThreshold);
    fprintf(stderr, "\n");
    fprintf(stderr, "          -maxcolorerror  n\n");
    fprintf(stderr, "          -maxbaseerror   n\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "          -maxmemory      m (MB)\n");
    fprintf(stderr, "          -threads        n\n");
    fprintf(stderr, "          -verbose\n");

    exit(1);
  }

  g->initialize();

  sweatShop *ss = new sweatShop(tapperReader, tapperWorker, tapperWriter);

  ss->setLoaderQueueSize(16384);
  ss->setLoaderBatchSize(512);
  ss->setWorkerBatchSize(1024);
  ss->setWriterQueueSize(65536);

  ss->setNumberOfWorkers(g->numThreads);

  for (uint32 w=0; w<g->numThreads; w++)
    ss->setThreadData(w, new tapperThreadData(g));

  ss->run(g, g->beVerbose);

  delete g;
  delete ss;

  fprintf(stderr, "\nSuccess!  Bye.\n");
  return(0);
}
