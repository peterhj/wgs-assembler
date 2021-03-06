#include <stdio.h>

#include "util++.H"

mt_s  *mt = 0L;

void
test(void) {
  int           e = 0;
  intervalList  I;

  I.add(71,  3);
  I.add( 5,  3);
  I.add(32,  5);
  I.add(73,  3);
  I.add(55, 10);
  I.add( 5,  3);
  I.add(10,  5);
  I.add(20, 10);
  I.add(30, 10);
  I.add(50, 10);
  I.add(70,  3);
  I.add(72,  3);
  I.add( 5,  3);
  I.add(15,  5);

#if 0
  for (uint32 i=0; i<I.numberOfIntervals(); i++)
    fprintf(stderr, "%2d] %2d %2d\n", i, I.lo(i), I.hi(i));
#endif

  I.sort();
  I.merge();

  if (I.sumOfLengths() != 54)
    fprintf(stderr, "Lengths don't add up.\n"), e++;

  if (I.numberOfIntervals() != 4)
    fprintf(stderr, "Wrong number of intervals.\n"), e++;

  if ((I.lo(0) != 5) || (I.hi(0) != 8))
    fprintf(stderr, "Interval 0 is wrong.\n"), e++;
  if ((I.lo(1) != 10) || (I.hi(1) != 40))
    fprintf(stderr, "Interval 1 is wrong.\n"), e++;
  if ((I.lo(2) != 50) || (I.hi(2) != 65))
    fprintf(stderr, "Interval 2 is wrong.\n"), e++;
  if ((I.lo(3) != 70) || (I.hi(3) != 76))
    fprintf(stderr, "Interval 3 is wrong.\n"), e++;

  if (e)
    exit(e);
}



void
testIntersect(uint32 type) {
  uint32   numTests = 1000000;
  uint32  *beg     = new uint32 [numTests];
  uint32  *len     = new uint32 [numTests];
  uint32  *end     = new uint32 [numTests];
  uint32  *abegh   = new uint32 [numTests];
  uint32  *aendh   = new uint32 [numTests];
  uint32  *bbegh   = new uint32 [numTests];
  uint32  *bendh   = new uint32 [numTests];
  uint32   errors  = 0;
  uint32   passed  = 0;

  intervalList  A;
  intervalList  B;

  //
  //  Build two interval lists
  //
  //  type == 0 --> all pairwise
  //  type == 1 --> A sequence is solid
  //  type == 2 --> B sequence is solid
  //

  if (type == 1)
    A.add(1, 1500000000);
  if (type == 2)
    B.add(1, 1500000000);

  for (uint32 i=0; i<numTests; i++) {

    //  Compute the result we want to get
    //
    len[i] = mtRandom32(mt) % 200;
    if (len[i] < 100) {
      beg[i] = end[i] = mtRandom32(mt) % 100 + 100;
    } else {
      beg[i] = mtRandom32(mt) % 100 + 100;
      end[i] = beg[i] + len[i];
    }

    //  Reset if the type is 1 or 2.
    //
    if ((type == 1) || (type == 2)) {
      len[i] = mtRandom32(mt) % 100 + 100;
      beg[i] = mtRandom32(mt) % 100 + 100;
      end[i] = beg[i] + len[i];
    }

    //  Extend it to an interval -- we can extend exactly one end, or
    //  two opposite ends.
    //
    abegh[i] = 0;
    aendh[i] = 0;
    bbegh[i] = 0;
    bendh[i] = 0;

    if (type == 0) {
      switch (mtRandom32(mt) % 8) {
        case 0:
          abegh[i] = mtRandom32(mt) % 50;
          break;
        case 1:
          aendh[i] = mtRandom32(mt) % 50;
          break;
        case 2:
          bbegh[i] = mtRandom32(mt) % 50;
          break;
        case 3:
          bendh[i] = mtRandom32(mt) % 50;
          break;
        case 4:
          abegh[i] = mtRandom32(mt) % 50;
          aendh[i] = mtRandom32(mt) % 50;
          break;
        case 5:
          bbegh[i] = mtRandom32(mt) % 50;
          bendh[i] = mtRandom32(mt) % 50;
          break;
        case 6:
          abegh[i] = mtRandom32(mt) % 50;
          bendh[i] = mtRandom32(mt) % 50;
          break;
        case 7:
          aendh[i] = mtRandom32(mt) % 50;
          bbegh[i] = mtRandom32(mt) % 50;
          break;
      }
    }

    //  Add it to the lists -- if type == 1 or 2, these should then
    //  get merged into the one big thing.
    //
    A.add(1000 * i + beg[i] - abegh[i], abegh[i] + end[i] - beg[i] + aendh[i]);
    B.add(1000 * i + beg[i] - bbegh[i], bbegh[i] + end[i] - beg[i] + bendh[i]);
  }

  intervalList I;
  I.intersect(A, B);

  //
  //  Check the result.
  //

  for (uint32 i=0, j=0; i<numTests; i++) {
    uint32  b = I.lo(j) - 1000 * i;
    uint32  e = I.hi(j) - 1000 * i;

    if (len[i] < 100) {
      //
      //  Expect no result here.  We ca only test that the stuff that
      //  should be intersecting is correct, and if all that is, then
      //  I guess the non-intersection stuff is correct too.
      //
    } else {
      if ((b != beg[i]) || (e != end[i])) {
        fprintf(stderr, "FAILED[%4d]: "uint32FMT"-"uint32FMT" X "uint32FMT"-"uint32FMT" -> "uint32FMT","uint32FMT" ("uint32FMT","uint32FMT") (should have been "uint32FMT","uint32FMT")\n",
                i,
                beg[i] - abegh[i], beg[i] - abegh[i] + abegh[i] + end[i] - beg[i] + aendh[i],
                beg[i] - bbegh[i], beg[i] - bbegh[i] + bbegh[i] + end[i] - beg[i] + bendh[i],
                b, e, (uint32)I.lo(j), (uint32)I.hi(j),
                beg[i], end[i]);
        errors++;
      } else {
        passed++;
      }
      j++;
    }
  }

  fprintf(stderr, "intersection test had "uint32FMT" successes and "uint32FMT" errors.\n", passed, errors);
}




void
testMerge(void) {
  intervalList  IL;

  //  Test 1:  one long sequence containing lots of little non-overlapping sequences
  //  Test 2:  three long overlapping sequences, containing lots of non-overlapping sequences
  //  Test 3:  dense random
  //  Test 4:  special cases

  fprintf(stderr, "Merge test 1\n");
  IL.clear();
  IL.add(0, 100000);
  for (uint32 i=0; i<999; i++)
    IL.add(100 + 100 * i, 50);
  IL.merge();
  for (uint32 i=0; i<IL.numberOfIntervals(); i++)
    fprintf(stderr, "IL["uint32FMTW(3)"] "uint64FMT" "uint64FMT"\n", i, IL.lo(i), IL.hi(i));

  fprintf(stderr, "Merge test 2\n");
  IL.clear();
  IL.add(0, 25000);
  IL.add(25000, 25000);
  IL.add(50000, 50000);
  for (uint32 i=0; i<999; i++)
    IL.add(100 + 100 * i, 50);
  IL.merge();
  for (uint32 i=0; i<IL.numberOfIntervals(); i++)
    fprintf(stderr, "IL["uint32FMTW(3)"] "uint64FMT" "uint64FMT"\n", i, IL.lo(i), IL.hi(i));

  fprintf(stderr, "Merge test 3\n");
  IL.clear();
  uint32 lo = 200;
  uint32 hi = 0;
  for (uint32 i=0; i<999; i++) {
    uint32 beg = mtRandom32(mt) % 100;
    uint32 end = mtRandom32(mt) % 100 + 100;
    if (beg < lo)  lo = beg;
    if (end > hi)  hi = end;
    IL.add(beg, end - beg);
  }
  IL.merge();
  if ((IL.lo(0) != lo) || (IL.hi(0) != hi))
    fprintf(stderr, "ERROR!\n");
  for (uint32 i=0; i<IL.numberOfIntervals(); i++)
    fprintf(stderr, "IL["uint32FMTW(3)"] "uint64FMT" "uint64FMT"\n", i, IL.lo(i), IL.hi(i));

  fprintf(stderr, "Merge test 4a\n");
  IL.clear();
  IL.add(0, 25000);
  IL.add(25000, 25000);
  IL.add(50000, 50000);
  IL.merge();
  for (uint32 i=0; i<IL.numberOfIntervals(); i++)
    fprintf(stderr, "IL["uint32FMTW(3)"] "uint64FMT" "uint64FMT"\n", i, IL.lo(i), IL.hi(i));

  fprintf(stderr, "Merge test 4b\n");
  IL.clear();
  IL.add(0, 25000);
  IL.add(25000, 25000);
  IL.add(50000, 50000);
  IL.add(20000, 5000);
  IL.add(45000, 5000);
  IL.add(95000, 5000);
  IL.merge();
  for (uint32 i=0; i<IL.numberOfIntervals(); i++)
    fprintf(stderr, "IL["uint32FMTW(3)"] "uint64FMT" "uint64FMT"\n", i, IL.lo(i), IL.hi(i));
}



int
main(int argc, char **argv) {

  mt = mtInit(time(NULL));

  test();

  testIntersect(0);
  testIntersect(1);
  testIntersect(2);

  testMerge();

  exit(0);
}
