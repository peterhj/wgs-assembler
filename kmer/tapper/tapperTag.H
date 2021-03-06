#include "bio++.H"


//  A single tag, binary encoded.
//
//  The current ascii encoding for a 25bp tag needs 44 bytes for
//  sequence and 110 bytes for QVs -> 154 bytes per tag.
//
//  Without QVs, we can fit upto a 60bp tag into 24 bytes, using a
//  int64 global id instead of the sequence name.  At 24B per tag, 10x
//  human is 27GB.
//
//  Including QVs, we now need to use 7 bits per bp, but we then
//  truncate QV's to a maximum of 32.  Not really a problem, since all
//  the files I've seen have a QV from 4 to 32 inclusive.
//
//  The infrastructure of the bitPackedFile is used, so all we need to
//  define is the number of words in our tapperTag (which, since we
//  already do a similar hack for a kMer, isn't so terrible).
//
//  WORDS     1  2  3  4  5  6  7  8  9 10 11 12  13  14  15  16
//  BYTES     8 16 24 32 40 48 56 64 72 80 88 96 104 112 120 128
//  TAG SIZE  0  7 17 26 35 44 53 62 71 81 90 99 108 117 126 135 (if 5 bits for qv)
//  TAG SIZE  0  6 14 22 30 38 46 54 62 70 78 86  94 102 110 118 (if 6 bits for qv)
//

#define TAPPER_TAG_WORDS 4
#define TAG_LEN_MAX   32


inline
uint64
encodeTagID(uint16 id[4]) {
  uint64  tid;

  tid = id[0];
  tid <<= 16;
  tid  |= id[1];
  tid <<= 16;
  tid  |= id[2];
  tid <<= 16;
  tid  |= id[3];

  return(tid);
}


inline
void
decodeTagID(uint64 tid, uint16 id[4]) {
  id[0] = (tid >> 48) & uint64MASK(16);
  id[1] = (tid >> 32) & uint64MASK(16);
  id[2] = (tid >> 16) & uint64MASK(16);
  id[3] = (tid)       & uint64MASK(16);
}


class tapperTag {
public:
  tapperTag() {};

  //  Expects seq to be "T01023030122303" and qlt to be 0 through 31,
  //  1-1 correspondence.
  //
  void     encode(uint16 id[4], char *seq, uint64 *qlt) {
    uint64   pos = 64;
    uint32   len = strlen(seq);
    uint32   i   = 0;

    for (i=1; i<TAPPER_TAG_WORDS; i++)
      _w[i] = uint64ZERO;

    _w[0] = encodeTagID(id);

    setDecodedValue(_w, pos,  2, letterToBits[seq[0]]);
    pos += 2;

    setDecodedValue(_w, pos,  7, len-1);
    pos += 7;

#undef DEBUG_TAG_ENCODING
#ifdef DEBUG_TAG_ENCODING
    fprintf(stderr, "tapperTag::encode()-- seq: %s id "uint64HEX" %c/%d len "uint32FMT" "uint64FMT" "uint64FMT"\n",
            seq, _w[0], seq[0], seq[0], len, pos-7, getDecodedValue(_w, pos-7, 7));
#endif

    for (i=1; i<len; i++) {
      if (qlt[i] > 31) {
        //fprintf(stderr, "tapperTag::encode()-- WARNING!  QV too big; "uint64FMT" truncated to 31.\n", qlt[i]);
        qlt[i] = 31;
      }
      setDecodedValue(_w, pos, 7, (letterToBits[seq[i]] << 5) | (qlt[i-1]));
      pos += 7;
    }
  };

  //  Decodes a tag into seq and qlt (as for encode()), returns the
  //  tagID, or 0 if failure.
  //
  uint64   decode(uint16 id[4], char *seq, uint64 *qlt) {
    uint64   pos = 64;
    uint32   len = 0;
    uint32   i   = 0;

    seq[0] = bitsToLetter[getDecodedValue(_w, pos, 2)];
    qlt[0] = 0;
    pos += 2;

    len = getDecodedValue(_w, pos, 7) + 1;
    pos += 7;

    for (i=1; i<len; i++) {
      uint64 x = getDecodedValue(_w, pos, 7);
      seq[i]   = bitsToColor[(x >> 5) & 0x03];
      qlt[i-1] = x & uint64MASK(5);
      pos += 7;
    }

    seq[len]   = 0;
    qlt[len-1] = 0;

#ifdef DEBUG_TAG_ENCODING
    fprintf(stderr, "tapperTag::decode()-- seq: %s id "uint64HEX" %c/%d len "uint32FMT"\n",
            seq, _w[0], seq[0], seq[0], len);
#endif

    decodeTagID(_w[0], id);

    return(_w[0]);
  };

  uint64   tagID(void) const     { return(_w[0]); };
  uint64   length(void)          { return(getDecodedValue(_w, 66, 7)); };

private:
  friend class tapperTagFile;

  uint64  _w[TAPPER_TAG_WORDS];
};





class tapperTagFileMetaData {
public:
  uint32   tagSize(void)          { return(_minTagLen); };
  uint32   isPairedTagFile(void)  { return(_isPaired);  };
  uint32   mean(void)             { return(_mean);      };
  uint32   stddev(void)           { return(_stddev);    };

  void     setMeanStdDev(uint32 mean_, uint32 stddev_) {
    _mean   = mean_;
    _stddev = stddev_;
  };

private:
  friend class tapperTagFile;
  uint32   _minTagLen;
  uint32   _maxTagLen;
  uint32   _tagWords;
  uint32   _isPaired;
  uint32   _isFragment;
  uint32   _mean;
  uint32   _stddev;
};




//  Notes:
//
//  1 Stores EITHER mated tags or fragment tags, NEVER both in the same file.
//  2 Variable tag size for every tag (even mated tags)
//  3 QVs too.
//  4 Random access
//  5 Reads are assigned a 64-bit UID



class tapperTagFile {
public:

  tapperTagFile(char *name, char mode) {
    if ((mode == 'r') && (fileExists(name) == false)) {
      fprintf(stderr, "tapperTagFile()--  ERROR!  Tag file '%s' doesn't exist.\n", name);
      exit(1);
    }

    _tagFile  = new recordFile(name, sizeof(tapperTagFileMetaData), TAPPER_TAG_WORDS * sizeof(uint64), mode);
    _metaData = (tapperTagFileMetaData *)_tagFile->header();

    if (_metaData->_tagWords == 0) {
      _metaData->_minTagLen  = ~uint32ZERO;
      _metaData->_maxTagLen  = 0;
      _metaData->_tagWords   = TAPPER_TAG_WORDS;
      _metaData->_isPaired   = 0;
      _metaData->_isFragment = 0;
      _metaData->_mean       = 0;
      _metaData->_stddev     = 0;
    }

    if (_metaData->_tagWords != TAPPER_TAG_WORDS) {
      fprintf(stderr, "tapperTagFile()--  ERROR!  Tag file was built with TAPPER_TAG_WORDS="uint32FMT", but code has %d.\n",
              _metaData->_tagWords, TAPPER_TAG_WORDS);
      exit(1);
    }
  };


  ~tapperTagFile() {
    //  Metadata is updated automagically when tagFile is deleted.
    delete _tagFile;
  };


  tapperTagFileMetaData   *metaData(void) {
    return(_metaData);
  };

  uint64 numberOfFragmentTags(void) {
    if (metaData()->isPairedTagFile())
      return(0);
    else
      return(_tagFile->numRecords());
  };

  uint64 numberOfMatePairs(void) {
    if (metaData()->isPairedTagFile())
      return(_tagFile->numRecords() / 2);
    else
      return(0);
  };

  void   setBegin(uint32 bgn) {
    if (metaData()->isPairedTagFile())
      _tagFile->seek(bgn * 2);
    else
      _tagFile->seek(bgn);
  };


  void   setEnd(uint32 end) {
    if (metaData()->isPairedTagFile())
      _tagFile->limit(end * 2);
    else
      _tagFile->limit(end);
  };


  void   put(tapperTag *tag) {
    uint64 len = tag->length();

    _metaData->_isFragment = 1;
    if (_metaData->_isPaired)
      fprintf(stderr, "tapperTagFile()--  ERROR: file contains mated tags, tried to pet a fragment tag.\n"), exit(1);

    if (len < _metaData->_minTagLen) _metaData->_minTagLen = len;
    if (_metaData->_minTagLen < len) _metaData->_maxTagLen = len;

    _tagFile->putRecord(tag->_w);
  };


  void   put(tapperTag *ta1, tapperTag *ta2) {
    uint64 len1 = ta1->length();
    uint64 len2 = ta2->length();

    _metaData->_isPaired  = 1;
    if (_metaData->_isFragment)
      fprintf(stderr, "tapperTagFile()--  ERROR: file contains fragment tags, tried to pet a mated tag.\n"), exit(1);

    if (len1 < _metaData->_minTagLen)  _metaData->_minTagLen = len1;
    if (_metaData->_minTagLen < len1)  _metaData->_maxTagLen = len1;

    if (len2 < _metaData->_minTagLen)  _metaData->_minTagLen = len2;
    if (_metaData->_minTagLen < len2)  _metaData->_maxTagLen = len2;

    _tagFile->putRecord(ta1->_w);
    _tagFile->putRecord(ta2->_w);
  };


  bool   get(tapperTag *tag) {
    if (_metaData->_isPaired == 1)
      fprintf(stderr, "tapperTagFile()--  ERROR: file contains mated tags, tried to get a fragment tag.\n"), exit(1);
    return(_tagFile->getRecord(tag->_w) == 1);
  };


  bool   get(tapperTag *ta1, tapperTag *ta2) {
    if (_metaData->_isFragment == 1)
      fprintf(stderr, "tapperTagFile()--  ERROR: file contains fragment tags, tried to get a mated tag.\n"), exit(1);
    return((_tagFile->getRecord(ta1->_w) == 1) &&
           (_tagFile->getRecord(ta2->_w) == 1));
  };

private:
  tapperTagFileMetaData  *_metaData;
  recordFile             *_tagFile;
};

