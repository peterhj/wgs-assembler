
/**************************************************************************
 * This file is part of Celera Assembler, a software program that
 * assembles whole-genome shotgun reads into contigs and scaffolds.
 * Copyright (C) 1999-2004, Applera Corporation. All rights reserved.
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

static const char *rcsid = "$Id: AS_ALN_pieceOlap.C 4371 2013-08-01 17:19:47Z brianwalenz $";

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "AS_global.H"
#include "CA_ALN_local.H"
#include "AS_ALN_aligners.H"

#define OKNAFFINE 1

#undef DEBUG_FIX_OLAP



/* safely copy a substring of a string into static space which is enlarged
   as needed */

static void safe_substr(char **seg,int *segspace,const char *seq,int beg,int end){

  if(*segspace<end-beg+1){
    *segspace=2*(end-beg)+1;
    *seg=(char*)safe_realloc(*seg,sizeof(char)*(*segspace));
  }
  /* copy the segments */
  strncpy(*seg,seq+beg,end-beg);
  (*seg)[end-beg]='\0';
  assert(strlen(*seg)==end-beg);
}






/* construct a trace (with AS_ALN_OKNAffine) for the first local segment,
   copying the result into its own static location (so that we
   can call AS_ALN_OKNAffine on the second local segment without losing the
   result */

static int *get_trace(const char *aseq, const char *bseq,Local_Overlap *O,int piece,
		int which){
  static char *aseg=NULL, *bseg=NULL;
  static int asegspace=0,bsegspace=0;
  static int *segtrace[2], tracespace[2]={0,0};
  int alen,blen;
  int spnt, *tmptrace;
#ifdef OKNAFFINE
  int epnt;
#endif
  int segdiff;
  int i;

  assert(which==0||which==1);

  if(segtrace[which]==NULL){
    tracespace[which]=100;
    segtrace[which]=(int*)safe_malloc(sizeof(int)*tracespace[which]);
  }

  safe_substr(&aseg,&asegspace,aseq,O->chain[piece].piece.abpos,
	      O->chain[piece].piece.aepos);
  safe_substr(&bseg,&bsegspace,bseq,O->chain[piece].piece.bbpos,
	      O->chain[piece].piece.bepos);

  alen=O->chain[piece].piece.aepos-O->chain[piece].piece.abpos;
  blen=O->chain[piece].piece.bepos-O->chain[piece].piece.bbpos;

  //fprintf(stderr, "get_trace: len(aseg)=%d, len(bseg)=%d, alen=%d, blen=%d\n",
  //       strlen(aseg),strlen(bseg), alen,blen);

  /* get trace for the segment from AS_ALN_OKNAlign */
  spnt=0;
  /* subtract because Gene likes to point to one before string start */
  aseg--;
  bseg--;
  segdiff=(int)((O->chain[piece].piece.aepos-O->chain[piece].piece.abpos)
		*(1.5*O->chain[piece].piece.error)    +10);

#ifdef OKNAFFINE
  epnt=0;
  tmptrace=AS_ALN_OKNAffine(aseg,alen,bseg,blen,&spnt,&epnt,segdiff);
#ifdef DEBUG_FIX_OLAP
  fprintf(stderr,"epnt %d\n",epnt);
#endif
#else
  tmptrace=AS_ALN_OKNAlign(aseg,alen,bseg,blen,&spnt,segdiff);
#endif
  //  assert(spnt==0);


#ifdef DEBUG_FIX_OLAP
  fprintf(stderr,"spnt %d\n",spnt);

  {
    fprintf(stderr,"Trace for segments of lengths %d %d\n",strlen(aseg+1),
	    strlen(bseg+1));

    if(epnt!=0){
      if(epnt<0){
	aseg[alen+epnt+1]='\0';
      } else {
	bseg[blen-epnt+1]='\0';
      }
    }

    if(spnt<0){
      int i;
      fprintf(stderr,"(B sequence on top (2nd place in src); spnt=%d!!!\n",spnt);
      i=0;
      while(tmptrace[i]!=0){tmptrace[i++]*=-1;}
      PrintAlign(stderr,-spnt,0,bseg+1,aseg+1,tmptrace);
      i=0; while(tmptrace[i]!=0){tmptrace[i++]*=-1;}
    } else {
      PrintAlign(stderr,spnt,0,aseg+1,bseg+1,tmptrace);
    }
  }
#endif




  if(spnt!=0){
    if(spnt>0){
      O->chain[piece].agap+=spnt;
      O->chain[piece].piece.abpos+=spnt;
      i=0;
      while(tmptrace[i]!=0){
	if(tmptrace[i]<0){
	  tmptrace[i]+=spnt;
	  assert(tmptrace[i]<0);
	}
	i++;
      }
    } else {
      O->chain[piece].bgap+=-spnt;
      O->chain[piece].piece.bbpos+=-spnt;
      i=0;
      while(tmptrace[i]!=0){
	if(tmptrace[i]>0){
	  tmptrace[i]+=spnt;
	  assert(tmptrace[i]>0);
	}
	i++;
      }
    }
  }

#ifdef OKNAFFINE
  {
    //  Find the next non deleted segment
    int ll = piece+1;

    while (ll <= O->num_pieces &&
           O->chain[ll].agap==0 &&
           O->chain[ll].bgap==0 &&
           O->chain[ll].piece.abpos == O->chain[ll].piece.aepos &&
           O->chain[ll].piece.bbpos == O->chain[ll].piece.bepos)
      ll++;

    assert(ll <= O->num_pieces);

    if(epnt!=0){
      if(epnt>0){ /* throwing away some of B segment */
        O->chain[ll].bgap+=epnt;
        O->chain[piece].piece.bepos-=epnt;
        assert(O->chain[piece].piece.bbpos<=O->chain[piece].piece.bepos);
      } else {
        O->chain[ll].agap-=epnt;
        O->chain[piece].piece.aepos+=epnt;
        assert(O->chain[piece].piece.abpos<=O->chain[piece].piece.aepos);
      }
    }
  }
#endif

  aseg++; /* restore because need to know where memory block is allocated,
  		 and so that next time around strncpy will work right! */
  bseg++;
  i=0;
  while(tmptrace[i]!=0){
    segtrace[which][i]=tmptrace[i];
    i++;
    if(i==tracespace[which]){
      tracespace[which]*=2;
      segtrace[which]=(int*)safe_realloc(segtrace[which],
				      sizeof(int)*tracespace[which]);
    }
  }
  segtrace[which][i]=0;
  return(segtrace[which]);

}



typedef struct {
  char *aseg;
  char *bseg;
} PAIRALIGN;




static void safe_add_to_seg(char **seg,int pos,char c,int *len){
  if(pos==*len){
    (*len)=(*len)*2;
    *seg=(char*)safe_realloc(*seg,sizeof(char)*((*len)+1));
  }
  (*seg)[pos]=c;
}



static PAIRALIGN *construct_pair_align(const char *aseq,const char *bseq,Local_Overlap *O,int piece,int *trace,int which){
  static char *aseg[2]={NULL,NULL},*bseg[2]={NULL,NULL};
  static int alen[2]={0,0},blen[2]={0,0};
  static PAIRALIGN pairalign[2];

  int starta,startb;
  int offseta,offsetb;
  int tpos,apos,bpos;


  if(aseg[which]==NULL){
    alen[which]=blen[which]=1000;
    aseg[which]=(char*)safe_malloc((alen[which]+1)*sizeof(char));
    bseg[which]=(char*)safe_malloc((blen[which]+1)*sizeof(char));
  }
  starta=offseta=O->chain[piece].piece.abpos;
  startb=offsetb=O->chain[piece].piece.bbpos;
  tpos=0;
  apos=0;
  bpos=0;

#ifdef DEBUG_FIX_OLAP
fprintf(stderr,"piece:%d\n",piece);
fprintf(stderr,"offsets:%d %d\n",offseta,offsetb);
fprintf(stderr,"gaps:%d %d\n",O->chain[piece].agap,O->chain[piece].bgap);
fprintf(stderr,"bpos:%d %d\n",O->chain[piece].piece.abpos,O->chain[piece].piece.bbpos);
fprintf(stderr,"epos:%d %d\n",O->chain[piece].piece.aepos,O->chain[piece].piece.bepos);
#endif

  while(trace[tpos]!=0){
    if(trace[tpos]<0){
      for(;offseta<-trace[tpos]+starta-1;apos++,offseta++){
	safe_add_to_seg(&(aseg[which]),apos,aseq[offseta],&(alen[which]));
      }
      safe_add_to_seg(&(aseg[which]),apos,'-',&(alen[which]));
      apos++;
    } else {
      for(;offsetb<trace[tpos]+startb-1;bpos++,offsetb++){
	safe_add_to_seg(&(bseg[which]),bpos,bseq[offsetb],&(blen[which]));
      }
      safe_add_to_seg(&(bseg[which]),bpos,'-',&(blen[which]));
      bpos++;
    }
#ifdef DEBUG_FIX_OLAP
fprintf(stderr,"Handled trace position(%d):%d, offsets now %d, %d\n",
	tpos,trace[tpos],offseta,offsetb);
#endif
    tpos++;
  }
  for(;offseta<O->chain[piece].piece.aepos;apos++,offseta++){
    safe_add_to_seg(&(aseg[which]),apos,aseq[offseta],&(alen[which]));
  }
  for(;offsetb<O->chain[piece].piece.bepos;bpos++,offsetb++){
    safe_add_to_seg(&(bseg[which]),bpos,bseq[offsetb],&(blen[which]));
  }
#ifdef DEBUG_FIX_OLAP
fprintf(stderr,"Handled complete trace, offsets now %d, %d\n",offseta,offsetb);

fprintf(stderr,"offsets:%d %d\n",offseta,offsetb);
fprintf(stderr,"bpos:%d %d\n",O->chain[piece].piece.abpos,O->chain[piece].piece.bbpos);
fprintf(stderr,"epos:%d %d\n",O->chain[piece].piece.aepos,O->chain[piece].piece.bepos);
#endif
  assert(offseta==O->chain[piece].piece.aepos);
  assert(offsetb==O->chain[piece].piece.bepos);
  assert(offseta-O->chain[piece].piece.abpos+
	 offsetb-O->chain[piece].piece.bbpos+
	 tpos ==
	 apos+bpos);
  safe_add_to_seg(&(aseg[which]),apos,'\0',&(alen[which]));
  safe_add_to_seg(&(bseg[which]),bpos,'\0',&(blen[which]));

  pairalign[which].aseg=aseg[which];
  pairalign[which].bseg=bseg[which];
  return(pairalign+which);
}


static PAIRALIGN *get_align(const char *aseq,const char *bseq,Local_Overlap *O,int piece,
		      int which){
  int *trace=get_trace(aseq,bseq,O,piece,which);
  PAIRALIGN *pairalign=construct_pair_align(aseq,bseq,O,piece,trace,which);
  return(pairalign);
}


void fix_overlapping_pieces(char *aseq, char *bseq,
			    Local_Overlap *O,int piece0, int piece1){

  PAIRALIGN *pair_align1,*pair_align2;

  int offseta1,offsetb1,offseta2,offsetb2;
  int bestend1a,bestend1b,bestbeg2a,bestbeg2b;
  int into1,into2,bestinto2,bestinto1;
  int errs1,errs2,minerrs;


  assert(O->chain[piece0].piece.aepos>=O->chain[piece1].piece.abpos||
	 O->chain[piece0].piece.bepos>=O->chain[piece1].piece.bbpos);

  assert(O->chain[piece0].piece.aepos<=O->chain[piece1].piece.aepos);
  assert(O->chain[piece0].piece.bepos<=O->chain[piece1].piece.bepos);

  assert(piece0 < piece1);

#ifdef DEBUG_FIX_OLAP
fprintf(stderr,"fixing gap(%d,%d) (%d,%d)---(%d,%d) vs. gap(%d,%d)  (%d,%d)---(%d,%d)\n",
	O->chain[piece0].agap,	O->chain[piece0].bgap,
	O->chain[piece0].piece.abpos,O->chain[piece0].piece.bbpos,
	O->chain[piece0].piece.aepos,O->chain[piece0].piece.bepos,
	O->chain[piece1].agap,	O->chain[piece1].bgap,
	O->chain[piece1].piece.abpos,O->chain[piece1].piece.bbpos,
	O->chain[piece1].piece.aepos,O->chain[piece1].piece.bepos);
#endif

  /* create alignments for the two segments */

  pair_align1=get_align(aseq,bseq,O,piece0,0);
  pair_align2=get_align(aseq,bseq,O,piece1,1);

#ifdef DEBUG_FIX_OLAP
fprintf(stderr,"fixing gap(%d,%d) (%d,%d)---(%d,%d) vs. gap(%d,%d)  (%d,%d)---(%d,%d)\n",
	O->chain[piece0].agap,	O->chain[piece0].bgap,
	O->chain[piece0].piece.abpos,O->chain[piece0].piece.bbpos,
	O->chain[piece0].piece.aepos,O->chain[piece0].piece.bepos,
	O->chain[piece1].agap,	O->chain[piece1].bgap,
	O->chain[piece1].piece.abpos,O->chain[piece1].piece.bbpos,
	O->chain[piece1].piece.aepos,O->chain[piece1].piece.bepos);
#endif


  /* if, in finding the alignments, we shift the ends of the
     alignment of the first segment to before the starts of the
     alignment of the second segment, then the overlap has been
     resolved, so we do nothing more */

  if(!(O->chain[piece0].piece.aepos>O->chain[piece1].piece.abpos||
	 O->chain[piece0].piece.bepos>O->chain[piece1].piece.bbpos)){

#ifdef DEBUG_FIX_OLAP
fprintf(stderr," ... overlap between segments disappeared in perfecting\n"
	"     segment alignments (pieces %d %d)\n",piece0,piece1);
#endif

    return;
  }

  /* if, in finding the alignments, we shift the end of the
     alignment of the second segment to before the end of the
     alignment of the first segment, then the second is contained
     in the first and we need to do something exceptional;
     the most heuristic, but consistent with the practice elsewhere
     in the local overlapper, is to pseudo-delete the second segment */

  if(!(O->chain[piece0].piece.aepos<=O->chain[piece1].piece.aepos)||
     !(O->chain[piece0].piece.bepos<=O->chain[piece1].piece.bepos)){
#ifdef DEBUG_FIX_OLAP
fprintf(stderr,"Fixing by deleting second segment since apparently contained (pieces %d %d)!\n",piece0,piece1);
#endif

    //  DELETE segment.
    O->chain[piece1].agap=0;
    O->chain[piece1].bgap=0;
    O->chain[piece1].piece.abpos=O->chain[piece0].piece.aepos;
    O->chain[piece1].piece.aepos=O->chain[piece0].piece.aepos;
    O->chain[piece1].piece.bbpos=O->chain[piece0].piece.bepos;
    O->chain[piece1].piece.bepos=O->chain[piece0].piece.bepos;

    {
      //  Find the next non deleted segment
      int ll = piece1+1;

      while (ll <= O->num_pieces &&
             O->chain[ll].agap==0 &&
             O->chain[ll].bgap==0 &&
             O->chain[ll].piece.abpos == O->chain[ll].piece.aepos &&
             O->chain[ll].piece.bbpos == O->chain[ll].piece.bepos)
        ll++;

      if(ll<=O->num_pieces){
        O->chain[ll].agap=O->chain[ll].piece.abpos-O->chain[piece0].piece.aepos;
        O->chain[ll].bgap=O->chain[ll].piece.bbpos-O->chain[piece0].piece.bepos;
      }
    }
    return;
  }


  /* if, in finding the alignments, we shift the start of the
     alignment of the first segment to after the start of the
     alignment of the second segment, then the first is contained
     in the second and we need to do something exceptional;
     the most heuristic, but consistent with the practice elsewhere
     in the local overlapper, is to pseudo-delete the first segment */

  if(O->chain[piece0].piece.abpos>O->chain[piece1].piece.abpos||
     O->chain[piece0].piece.bbpos>O->chain[piece1].piece.bbpos){

#ifdef DEBUG_FIX_OLAP
fprintf(stderr,"Fixing by deleting first segment since apparently contained (pieces %d %d)!\n",piece0,piece1);
#endif

          //  DELETE segment.
	  O->chain[piece0].agap=0;
	  O->chain[piece0].bgap=0;
	  if(piece0>0){
	    O->chain[piece0].piece.abpos=O->chain[piece0-1].piece.aepos;
	    O->chain[piece0].piece.aepos=O->chain[piece0-1].piece.aepos;
	    O->chain[piece0].piece.bbpos=O->chain[piece0-1].piece.bepos;
	    O->chain[piece0].piece.bepos=O->chain[piece0-1].piece.bepos;
	  } else {
	    O->chain[piece0].piece.abpos=0;
	    O->chain[piece0].piece.aepos=0;
	    O->chain[piece0].piece.bbpos=0;
	    O->chain[piece0].piece.bepos=0;
          }
          O->chain[piece1].agap=O->chain[piece1].piece.abpos-O->chain[piece0].piece.aepos;
          O->chain[piece1].bgap=O->chain[piece1].piece.bbpos-O->chain[piece0].piece.bepos;

	  return;
  }

  /* find start of region for evaluation in first alignment */
  /* when done,
     offseta1 and offsetb1 should be the offsets into the sequences
     such that they correspond to a column in the alignment of the
     first segment and that column contains the first possible
     overlap with the second segment */

  offseta1=O->chain[piece0].piece.abpos-1;
  offsetb1=O->chain[piece0].piece.bbpos-1;
  into1=-1;
  /*fprintf(stderr,"piece1 alignmentlength %d == %d\n",
	  strlen(pair_align1->aseg),
	  strlen(pair_align1->bseg));*/
  while(offseta1<O->chain[piece1].piece.abpos&&
	offsetb1<O->chain[piece1].piece.bbpos){
    into1++;
    assert(pair_align1->aseg[into1]!='\0');
    assert(pair_align1->bseg[into1]!='\0');

    if(pair_align1->aseg[into1]!='-')offseta1++;
    if(pair_align1->bseg[into1]!='-')offsetb1++;

    /*    fprintf(stderr,"%c : %c\toffsets %d %d\tinto %d\n",pair_align1->bseg[into1],
	  pair_align1->bseg[into1],offseta1,offsetb1,into1);*/
  }
#ifdef DEBUG_FIX_OLAP
  fprintf(stderr,"Advanced to overlapping region; offseta1,offsetb1 = %d,%d\n",
	  offseta1,offsetb1);
  if(into1>0){
    fprintf(stderr,"Revised trace of piece 1:\n%c.%s\n%c.%s\n",
	    pair_align1->aseg[into1-1],pair_align1->aseg+into1,
	    pair_align1->bseg[into1-1],pair_align1->bseg+into1);
  } else {
    fprintf(stderr,"Piece 1 first column overlaps piece 2\n");
  }

#endif


  /* count mismatches in the second alignment */

  into2=0;
  errs2=0;
  while(pair_align2->aseg[into2]!='\0'){
#ifdef DEBUG_FIX_OLAP
    fflush(0);
#endif
    assert(pair_align2->bseg[into2]!='\0');
    if(pair_align2->aseg[into2]!=pair_align2->bseg[into2]){
      errs2++;
    }
    into2++;
  }

  /* initialize solution variables and auxiliaries */
  into2=0;
  errs1 = (pair_align1->aseg[into1]!=pair_align1->bseg[into1] ? 1 : 0);
  minerrs=errs2;
  offseta2=O->chain[piece1].piece.abpos;
  offsetb2=O->chain[piece1].piece.bbpos;
  bestend1a=offseta1 - (pair_align1->aseg[into1]!='-' ? 1 : 0);
  bestend1b=offsetb1 - (pair_align1->bseg[into1]!='-' ? 1 : 0);
  bestbeg2a=offseta2;
  bestbeg2b=offsetb2;
  bestinto1=into1-1;
  bestinto2=0;


#ifdef DEBUG_FIX_OLAP
fprintf(stderr,"init bestend1:%d %d\n",bestend1a,bestend1b);
fprintf(stderr,"init bestbeg2:%d %d\n",bestbeg2a,bestbeg2b);
#endif

  /* while there is potential overlap still to come ... */

  while(pair_align1->aseg[into1]!='\0'&&pair_align2->aseg[into2]!='\0'){

#ifdef DEBUG_FIX_OLAP
    fprintf(stderr,"Top of major fix loop into[%d,%d] (%d,%d)] [ (%d,%d)\n",
	    into1,into2,offseta1,offsetb1,offseta2,offsetb2);
    if(pair_align2->aseg[into2]=='\0'){
      fprintf(stderr,">Aseq\n%s\nBseq\n%s\n",aseq+1,bseq+1);
      fprintf(stderr,"pieces %d, %d:\n%s\n%s\n%s\n%s\n",
	     piece0,piece1,
	     pair_align1->aseg,
	     pair_align1->bseg,
	     pair_align2->aseg,
	     pair_align2->bseg);
      fprintf(stderr,"offsets1 (%d,%d) 2 (%d,%d); into (%d,%d)\n",
	     offseta1,offsetb1,offseta2,offsetb2,into1,into2);
      fprintf(stderr,"lineup:\n%s\n%s\n%s\n%s\n",
     	     pair_align1->aseg+into1,
	     pair_align1->bseg+into1,
	     pair_align2->aseg+into2,
	     pair_align2->bseg+into2);
    }
#endif

    // Once, we did the following assert, assuming that the alignment
    // of pair_align2 would not run out before pair_align1, since otherwise
    // there would be a containment or some such that shouldn't happen;
    // But, as luck would have it, alignment trimming quirks etc can
    // make it happen.  So ... no more assert
    //
    // assert(pair_align2->aseg[into2]!='\0');

    /* while a position in the second segment is no greater than
       the position in the first segment,
         check for mismatch in second segment,
	 counting errors,
	 incrementing the sequence position counters as appropriate;
       advance the second segment
       position */

    while(offseta1>=offseta2||offsetb1>=offsetb2
#undef ADVANCE_PAST_MISMATCHES
#ifdef ADVANCE_PAST_MISMATCHES
	  || pair_align2->aseg[into2]!=pair_align2->bseg[into2]
#endif
	  ){
      errs2-= (pair_align2->aseg[into2]!=pair_align2->bseg[into2] ? 1 : 0);
      offseta2+= ( pair_align2->aseg[into2]!='-'  ? 1 : 0 );
      offsetb2+= ( pair_align2->bseg[into2]!='-'  ? 1 : 0 );
      into2++;
      if(pair_align2->aseg[into2]=='\0'){
	break;
      }
#ifdef DEBUG_FIX_OLAP
      fprintf(stderr,"Once through minor fix loop1 into[%d,%d] (%d,%d)] [ (%d,%d)  -- skipped over (%c,%c)\n",
	    into1,into2,offseta1,offsetb1,offseta2,offsetb2,
	      pair_align2->aseg[into2-1],pair_align2->bseg[into2-1]);
#endif
    }

#define NEWVER
#ifdef NEWVER
    if(errs1+errs2<=minerrs&&
       pair_align1->aseg[into1]==pair_align1->bseg[into1]){
#else
    if(errs1+errs2<=minerrs){
#endif

      minerrs=errs1+errs2;
      bestend1a=offseta1;
      bestend1b=offsetb1;
      bestbeg2a=offseta2;
      bestbeg2b=offsetb2;
      bestinto2=into2;
      bestinto1=into1;

#ifdef DEBUG_FIX_OLAP
      fprintf(stderr,"  accept new solution (%d,%d)] [ (%d,%d)  minerrs %d+%d\n",
	      bestend1a,bestend1b,bestbeg2a,bestbeg2b,errs1,errs2);
#endif


    }

    /* while the positions in the first segment are no greater than
       the positions in the second segment,
         check for mismatch in first segment,
	 counting errors,
	 incrementing the sequence position counters as appropriate;
       advance the first segment
       position */

    while(offseta1<offseta2&&offsetb1<offsetb2){
      into1++;
      offseta1+= ( pair_align1->aseg[into1]!='-'  ? 1 : 0 );
      offsetb1+= ( pair_align1->bseg[into1]!='-'  ? 1 : 0 );
      if(pair_align1->aseg[into1]=='\0'){
	break;
      }
      errs1+= (pair_align1->aseg[into1]!=pair_align1->bseg[into1] ? 1 : 0);
#ifdef DEBUG_FIX_OLAP
      fprintf(stderr,"Once through minor fix loop2 into[%d,%d] (%d,%d)] [ (%d,%d)  -- now include (%c,%c)  errs %d/%d\n",
	    into1,into2,offseta1,offsetb1,offseta2,offsetb2,
	      pair_align1->aseg[into1],pair_align1->bseg[into1],
	      errs1,errs2);
#endif
    }

#ifdef DEBUG_FIX_OLAP
      fprintf(stderr,"End of major fix loop into[%d,%d] (%d,%d)] [ (%d,%d)\n",
	    into1,into2,bestend1a,bestend1b,bestbeg2a,bestbeg2b);
#endif
  }

  if(bestend1a<O->chain[piece0].piece.aepos)
    bestend1a++;
  if(bestend1b<O->chain[piece0].piece.bepos)
    bestend1b++;
  O->chain[piece0].piece.aepos=bestend1a;
  O->chain[piece0].piece.bepos=bestend1b;
  O->chain[piece1].piece.abpos=bestbeg2a;
  O->chain[piece1].piece.bbpos=bestbeg2b;
  O->chain[piece1].agap=bestbeg2a-bestend1a;
  O->chain[piece1].bgap=bestbeg2b-bestend1b;

#ifdef DEBUG_FIX_OLAP
  fprintf(stderr,"bestend1:%d %d\n",bestend1a,bestend1b);
  fprintf(stderr,"bestbeg2:%d %d\n",bestbeg2a,bestbeg2b);
  fprintf(stderr,"final gaps0:%d %d\n",O->chain[piece0].agap,O->chain[piece0].bgap);
  fprintf(stderr,"final gaps1:%d %d\n",O->chain[piece1].agap,O->chain[piece1].bgap);
  fflush(stderr);
#endif

  assert(O->chain[piece1].agap>=0);
  assert(O->chain[piece1].bgap>=0);
  assert(O->chain[piece1].agap==0||O->chain[piece1].bgap==0);

  // now, adjust the segments to trim off any mismatches at trimmed ends
  while ((bestinto1 >= 0) && (pair_align1->aseg[bestinto1] != pair_align1->bseg[bestinto1])) {
    bestend1a -= ( pair_align1->aseg[bestinto1]!='-'  ? 1 : 0 );
    bestend1b -= ( pair_align1->bseg[bestinto1]!='-'  ? 1 : 0 );
#ifdef DEBUG_FIX_OLAP
    fprintf(stderr,"trimming off %c~%c to give bestends %d and %d\n",
	   pair_align1->aseg[bestinto1],
	   pair_align1->bseg[bestinto1],
	   bestend1a,bestend1b);
#endif
    bestinto1--;
  }

  while(pair_align2->aseg[bestinto2]!=pair_align2->bseg[bestinto2]&&
	pair_align2->aseg[bestinto2]!='\0'){
    bestbeg2a += ( pair_align2->aseg[bestinto2]!='-'  ? 1 : 0 );
    bestbeg2b += ( pair_align2->bseg[bestinto2]!='-'  ? 1 : 0 );
    bestinto2++;
  }

  O->chain[piece0].piece.aepos=bestend1a;
  O->chain[piece0].piece.bepos=bestend1b;
  O->chain[piece1].piece.abpos=bestbeg2a;
  O->chain[piece1].piece.bbpos=bestbeg2b;
  O->chain[piece1].agap=bestbeg2a-bestend1a;
  O->chain[piece1].bgap=bestbeg2b-bestend1b;

  assert(O->chain[piece0].piece.abpos<=O->chain[piece0].piece.aepos);
  assert(O->chain[piece0].piece.bbpos<=O->chain[piece0].piece.bepos);
  assert(O->chain[piece1].piece.abpos<=O->chain[piece1].piece.aepos);
  assert(O->chain[piece1].piece.bbpos<=O->chain[piece1].piece.bepos);

#ifdef DEBUG_FIX_OLAP
  fprintf(stderr,"second segment:\n%s\n%s\n",
	  pair_align2->aseg+bestinto2,
	  pair_align2->bseg+bestinto2);
  fprintf(stderr,"bestend1:%d %d\n",bestend1a,bestend1b);
  fprintf(stderr,"bestbeg2:%d %d\n",bestbeg2a,bestbeg2b);
  fprintf(stderr,"final gaps0:%d %d\n",O->chain[piece0].agap,O->chain[piece0].bgap);
  fprintf(stderr,"final gaps1:%d %d\n",O->chain[piece1].agap,O->chain[piece1].bgap);
  fflush(stderr);
#endif

}
