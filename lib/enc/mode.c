/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2008                *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

  function: mode selection code
  last mod: $Id$

 ********************************************************************/

#include <string.h>
#include "codec_internal.h"
#include "mode_select.h"
#include "encoder_lookup.h"

/* Mode decision is done by exhaustively examining all potential
   choices.  Since we use a minimum-quality encoding strategy, this
   amounts to simply selecting the mode which uses the smallest number
   of bits, since the minimum quality will be met in any mode.
   Obviously, doing the motion compensation, fDCT, tokenization, and
   then counting the bits each token uses is computationally
   expensive.  Theora's EOB runs can also split the cost of these
   tokens across multiple fragments, and naturally we don't know what
   the optimal choice of Huffman codes will be until we know all the
   tokens we're going to encode in all the fragments.

   So we use a simple approach to estimating the bit cost of each mode
   based upon the SAD value of the residual.  The mathematics behind
   the technique are outlined by Kim \cite{Kim03}, but the process is
   very simple.  For each quality index and SAD value, we have a table
   containing the average number of bits needed to code a fragment.
   The SAD values are placed into a small number of bins (currently
   16).  The bit counts are obtained by examining actual encoded
   frames, with optimal Huffman codes selected and EOB bits
   appropriately divided among all the blocks they involve.  A
   separate QIxSAD table is kept for each mode and color plane.  It
   may be possible to combine many of these, but only experimentation
   will tell which ones truly represent the same distribution.

   @ARTICLE{Kim03,
     author="Hyun Mun Kim",
     title="Adaptive Rate Control Using Nonlinear Regression",
     journal="IEEE Transactions on Circuits and Systems for Video
     Technology",
     volume=13,
     number=5,
     pages="432--439",
     month="May",
     year=2003
   }

*/

/* Initialize the mode scheme chooser.
   
   Schemes 0-6 use a highly unbalanced Huffman code to code each of
   the modes.  The same set of Huffman codes is used for each of these
   7 schemes, but the mode assigned to each code varies.

   Schemes 1-6 have a fixed mapping from Huffman code to MB mode,
   while scheme 0 writes a custom mapping to the bitstream before all
   the modes.  Finally, scheme 7 just encodes each mode directly in 3
   bits. 

*/

void oc_mode_scheme_chooser_init(CP_INSTANCE *cpi){
  oc_mode_scheme_chooser *chooser = &cpi->chooser;
  int i;

  for(i=0;i<7;i++)
    chooser->mode_bits[i] = ModeBitLengths;
  chooser->mode_bits[7] = ModeBitLengthsD;
  
  chooser->mode_ranks[0] = chooser->scheme0_ranks;
  for(i=1;i<8;i++)
    chooser->mode_ranks[i] = ModeSchemes[i];

  memset(chooser->mode_counts,0,sizeof(chooser->mode_counts));
  
  /* Scheme 0 starts with 24 bits to store the mode list in. */
  chooser->scheme_bits[0] = 24;
  memset(chooser->scheme_bits+1,0,7*sizeof(chooser->scheme_bits[1]));
  for(i=0;i<8;i++){
    /* Scheme 7 should always start first, and scheme 0 should always start
       last. */
    chooser->scheme_list[i] = 7-i;
    chooser->scheme0_list[i] = chooser->scheme0_ranks[i]=i;
  }
}

/* This is the real purpose of this data structure: not actually
   selecting a mode scheme, but estimating the cost of coding a given
   mode given all the modes selected so far.

   This is done via opportunity cost: the cost is defined as the number of bits
   required to encode all the modes selected so far including the current one
   using the best possible scheme, minus the number of bits required to encode
   all the modes selected so far not including the current one using the best
   possible scheme.
  
   The computational expense of doing this probably makes it overkill.
   Just be happy we take a greedy approach instead of trying to solve the
   global mode-selection problem (which is NP-hard).
   _mode: The mode to determine the cost of.
   Return: The number of bits required to code this mode.*/

int oc_mode_cost(CP_INSTANCE *cpi,
		 int _mode){

  oc_mode_scheme_chooser *chooser = &cpi->chooser;
  int scheme0 = chooser->scheme_list[0];
  int scheme1 = chooser->scheme_list[1];
  int best_bits = chooser->scheme_bits[scheme0];
  int mode_bits = chooser->mode_bits[scheme0][chooser->mode_ranks[scheme0][_mode]];
  int si;
  int scheme_bits;

  /*Typical case: If the difference between the best scheme and the
     next best is greater than 6 bits, then adding just one mode
     cannot change which scheme we use.*/

  if(chooser->scheme_bits[scheme1]-best_bits > 6) return mode_bits;

  /*Otherwise, check to see if adding this mode selects a different scheme as the best.*/
  si = 1;
  best_bits += mode_bits;

  do{
    /* For any scheme except 0, we can just use the bit cost of the mode's rank in that scheme.*/
    if(scheme1!=0){

      scheme_bits = chooser->scheme_bits[scheme1]+
	chooser->mode_bits[scheme1][chooser->mode_ranks[scheme1][_mode]];

    }else{
      int ri;

      /* For scheme 0, incrementing the mode count could potentially
         change the mode's rank.

        Find the index where the mode would be moved to in the optimal
        list, and use its bit cost instead of the one for the mode's
        current position in the list. */

      /* don't recompute scheme bits; this is computing opportunity
	 cost, not an update. */

      for(ri = chooser->scheme0_ranks[_mode] ; ri>0 &&
	    chooser->mode_counts[_mode]>=
	    chooser->mode_counts[chooser->scheme0_list[ri-1]] ; ri--);

      scheme_bits = chooser->scheme_bits[0] + ModeBitLengths[ri];
    }

    if(scheme_bits<best_bits) best_bits = scheme_bits;
    if(++si>=8) break;
    scheme1 = chooser->scheme_list[si];
  } while(chooser->scheme_bits[scheme1] - chooser->scheme_bits[scheme0] <= 6);

  return best_bits - chooser->scheme_bits[scheme0];
}

/* Incrementally update the mode counts and per-scheme bit counts and re-order the scheme
   lists once a mode has been selected.

  _mode: The mode that was chosen.*/

static void oc_mode_set( CP_INSTANCE *cpi,
			 macroblock_t *mb,
			 int _mode){

  oc_mode_scheme_chooser *chooser = &cpi->chooser;
  int ri;
  int si;

  mb->mode = _mode;
  chooser->mode_counts[_mode]++;

  /* Re-order the scheme0 mode list if necessary. */
  for(ri = chooser->scheme0_ranks[_mode]; ri>0; ri--){
    int pmode;
    pmode=chooser->scheme0_list[ri-1];
    if(chooser->mode_counts[pmode] >= chooser->mode_counts[_mode])break;

    /* reorder the mode ranking */
    chooser->scheme0_ranks[pmode]++;
    chooser->scheme0_list[ri]=pmode;

  }
  chooser->scheme0_ranks[_mode]=ri;
  chooser->scheme0_list[ri]=_mode;

  /*Now add the bit cost for the mode to each scheme.*/
  for(si=0;si<8;si++){
    chooser->scheme_bits[si]+=
      chooser->mode_bits[si][chooser->mode_ranks[si][_mode]];
  }

  /* Finally, re-order the list of schemes. */
  for(si=1;si<8;si++){
    int sj = si;
    int scheme0 = chooser->scheme_list[si];
    int bits0 = chooser->scheme_bits[scheme0];
    do{
      int scheme1 = chooser->scheme_list[sj-1];
      if(bits0 >= chooser->scheme_bits[scheme1]) break;
      chooser->scheme_list[sj] = scheme1;
    } while(--sj>0);
    chooser->scheme_list[sj]=scheme0;
  }
}

static int BIntraSAD(CP_INSTANCE *cpi, int fi, int plane){
  int sad = 0;
  unsigned char *b = cpi->frame + cpi->frag_buffer_index[fi];
  ogg_int32_t acc = 0;
  int stride = cpi->stride[plane];
  int j,k;
  
  for(j=0;j<8;j++){
    for(k=0;k<8;k++)
      acc += b[k]; 
    b += stride;
  }
  
  b = cpi->frame + cpi->frag_buffer_index[fi];
  for(j=0;j<8;j++){
    for(k=0;k<8;k++)
      sad += abs ((b[k]<<6)-acc); 
    b += stride;
  }

  return sad>>6;
}

static int BINMAP(ogg_int32_t *lookup,int sad){
  int bin = OC_MINI((sad >> OC_SAD_SHIFT),(OC_SAD_BINS-1));
  ogg_int32_t *y = lookup + bin;
  int xdel = sad - (bin<<OC_SAD_SHIFT);
  int ydel = y[1] - y[0];
  return y[0] + ((ydel*xdel)>>OC_SAD_SHIFT);
}

/* equivalent to adding up the abs values of the AC components of a block */
static int MBIntraCost420(CP_INSTANCE *cpi, int qi, int mbi, int all){
  unsigned char *cp = cpi->frag_coded;
  macroblock_t *mb = &cpi->macro[mbi];
  int cost=0;
  int fi;
  /* all frags in a macroblock are valid so long as the macroblock itself is valid */
  if(mbi < cpi->macro_total){ 
    fi = mb->yuv[0][0];
    if(all || cp[fi])
      cost += BINMAP(mode_rate[qi][0][1],BIntraSAD(cpi,fi,0));
    fi = mb->yuv[0][1];
    if(all || cp[fi])
      cost += BINMAP(mode_rate[qi][0][1],BIntraSAD(cpi,fi,0));
    fi = mb->yuv[0][2];
    if(all || cp[fi])
      cost += BINMAP(mode_rate[qi][0][1],BIntraSAD(cpi,fi,0));
    fi = mb->yuv[0][3];
    if(all || cp[fi])
      cost += BINMAP(mode_rate[qi][0][1],BIntraSAD(cpi,fi,0));

    fi = mb->yuv[1][0];
    if(all || cp[fi])
      cost += BINMAP(mode_rate[qi][1][1],BIntraSAD(cpi,fi,1));
    fi = mb->yuv[2][0];
    if(all || cp[fi])
      cost += BINMAP(mode_rate[qi][2][1],BIntraSAD(cpi,fi,2));
   
  }
  
  /* Bit costs are stored in the table with extra precision. Round them down to whole bits.*/
  return cost;
}

static signed char mvmap[2][63] = {
  {     -15,-15,-14, -14,-13,-13,-12, -12,-11,-11,-10, -10, -9, -9, -8,
     -8, -7, -7, -6,  -6, -5, -5, -4,  -4, -3, -3, -2,  -2, -1, -1,  0,
      0,  0,  1,  1,   2,  2,  3,  3,   4,  4,  5,  5,   6,  6,  7,  7, 
      8,  8,  9,  9,  10, 10, 11, 11,  12, 12, 13, 13,  14, 14, 15, 15 },
  {      -7, -7, -7,  -7, -6, -6, -6,  -6, -5, -5, -5,  -5, -4, -4, -4,
     -4, -3, -3, -3,  -3, -2, -2, -2,  -2, -1, -1, -1,  -1,  0,  0,  0,
      0,  0,  0,  0,   1,  1,  1,  1,   2,  2,  2,  2,   3,  3,  3,  3,
      4,  4,  4,  4,   5,  5,  5,  5,   6,  6,  6,  6,   7,  7,  7,  7 }
};

static signed char mvmap2[2][63] = {
  {   -1, 0,-1,  0,-1, 0,-1,  0,-1, 0,-1,  0,-1, 0,-1,
    0,-1, 0,-1,  0,-1, 0,-1,  0,-1, 0,-1,  0,-1, 0,-1,
    0, 1, 0, 1,  0, 1, 0, 1,  0, 1, 0, 1,  0, 1, 0, 1,
    0, 1, 0, 1,  0, 1, 0, 1,  0, 1, 0, 1,  0, 1, 0, 1 },
  {   -1,-1,-1,  0,-1,-1,-1,  0,-1,-1,-1,  0,-1,-1,-1,
    0,-1,-1,-1,  0,-1,-1,-1,  0,-1,-1,-1,  0,-1,-1,-1,
    0, 1, 1, 1,  0, 1, 1, 1,  0, 1, 1, 1,  0, 1, 1, 1,
    0, 1, 1, 1,  0, 1, 1, 1,  0, 1, 1, 1,  0, 1, 1, 1 }
};

#define AV(a) ((r[a]+(int)r2[a])>>1)

static int BInterSAD(CP_INSTANCE *cpi, int fi, int plane, int goldenp, mv_t mv, int qp){
  int sad = 0;
  unsigned char *b = cpi->frame + cpi->frag_buffer_index[fi];
  int mx = mvmap[qp][mv.x+31];
  int my = mvmap[qp][mv.y+31];
  int mx2 = mvmap2[qp][mv.x+31];
  int my2 = mvmap2[qp][mv.y+31];
  int stride = cpi->stride[plane];
  unsigned char *r = (goldenp ? cpi->golden : cpi->recon ) + 
    cpi->frag_buffer_index[fi] + my * stride + mx;
  int j;
  
  if(mx2 || my2){
    unsigned char *r2 = r + my2 * stride + mx2;
    
    for(j=0;j<8;j++){
      sad += abs (b[0]-AV(0));
      sad += abs (b[1]-AV(1));
      sad += abs (b[2]-AV(2));
      sad += abs (b[3]-AV(3));
      sad += abs (b[4]-AV(4));
      sad += abs (b[5]-AV(5));
      sad += abs (b[6]-AV(6));
      sad += abs (b[7]-AV(7));
      b += stride;
      r += stride;
      r2 += stride;
    }
    
  }else{
    for(j=0;j<8;j++){
      sad += abs (b[0]-(int)r[0]);
      sad += abs (b[1]-(int)r[1]);
      sad += abs (b[2]-(int)r[2]);
      sad += abs (b[3]-(int)r[3]);
      sad += abs (b[4]-(int)r[4]);
      sad += abs (b[5]-(int)r[5]);
      sad += abs (b[6]-(int)r[6]);
      sad += abs (b[7]-(int)r[7]);
      b += stride;
      r += stride;
    }
  }

  if(plane)
    return sad<<2;
  else
    return sad;
}

static int MBInterCost420(CP_INSTANCE *cpi, int qi, int mbi, mv_t mv, int goldenp){
  unsigned char *cp = cpi->frag_coded;
  macroblock_t *mb = &cpi->macro[mbi];
  int cost = 0;
  int fi;

  fi=mb->yuv[0][0];
  if(cp[fi])
    cost += BINMAP(mode_rate[qi][0][0],BInterSAD(cpi,fi,0,goldenp,mv,0));
  fi=mb->yuv[0][1];
  if(cp[fi])
    cost += BINMAP(mode_rate[qi][0][0],BInterSAD(cpi,fi,0,goldenp,mv,0));
  fi=mb->yuv[0][2];
  if(cp[fi])
    cost += BINMAP(mode_rate[qi][0][0],BInterSAD(cpi,fi,0,goldenp,mv,0));
  fi=mb->yuv[0][3];
  if(cp[fi])
    cost += BINMAP(mode_rate[qi][0][0],BInterSAD(cpi,fi,0,goldenp,mv,0));

  fi=mb->yuv[1][0];
  if(cp[fi])
    cost += BINMAP(mode_rate[qi][1][0],BInterSAD(cpi,fi,1,goldenp,mv,1));
  fi=mb->yuv[2][0];
  if(cp[fi])
    cost += BINMAP(mode_rate[qi][2][0],BInterSAD(cpi,fi,2,goldenp,mv,1));

  /* Bit costs are stored in the table with extra precision. Round them down to whole bits.*/
  return cost;
}

static int MBInter4Cost420(CP_INSTANCE *cpi, int qi, int mbi, mv_t mv[4], int goldenp){
  unsigned char *cp = cpi->frag_coded;
  macroblock_t *mb = &cpi->macro[mbi];
  int cost = 0;
  mv_t ch;
  int fi;

  fi=mb->yuv[0][0];
  if(cp[fi])
    cost += BINMAP(mode_rate[qi][0][0],BInterSAD(cpi,fi,0,goldenp,mv[0],0));
  fi=mb->yuv[0][1];
  if(cp[fi])
    cost += BINMAP(mode_rate[qi][0][0],BInterSAD(cpi,fi,0,goldenp,mv[1],0));
  fi=mb->yuv[0][2];
  if(cp[fi])
    cost += BINMAP(mode_rate[qi][0][0],BInterSAD(cpi,fi,0,goldenp,mv[2],0));
  fi=mb->yuv[0][3];
  if(cp[fi])
    cost += BINMAP(mode_rate[qi][0][0],BInterSAD(cpi,fi,0,goldenp,mv[3],0));

  /* Calculate motion vector as the average of the Y plane ones. */
  /* Uncoded members are 0,0 and not special-cased */
  ch.x = mv[0].x + mv[1].x + mv[2].x + mv[3].x;
  ch.y = mv[0].y + mv[1].y + mv[2].y + mv[3].y;
  
  ch.x = ( ch.x >= 0 ? (ch.x + 2) / 4 : (ch.x - 2) / 4);
  ch.y = ( ch.y >= 0 ? (ch.y + 2) / 4 : (ch.y - 2) / 4);
  
  fi=mb->yuv[1][0];
  if(cp[fi])
    cost += BINMAP(mode_rate[qi][1][0],BInterSAD(cpi,fi,1,goldenp,ch,1));
  fi=mb->yuv[2][0];
  if(cp[fi])
    cost += BINMAP(mode_rate[qi][2][0],BInterSAD(cpi,fi,2,goldenp,ch,1));

  /* Bit costs are stored in the table with extra precision. Round them down to whole bits.*/
  return cost;
}

int PickModes(CP_INSTANCE *cpi){
  unsigned char qi = cpi->BaseQ; // temporary
  superblock_t *sb = cpi->super[0];
  superblock_t *sb_end = sb + cpi->super_n[0];
  int i,j;
  ogg_uint32_t interbits = 0;
  ogg_uint32_t intrabits = 0;

  mv_t last_mv = {0,0};
  mv_t prior_mv = {0,0};

  oc_mode_scheme_chooser_init(cpi);
  cpi->MVBits_0 = 0;
  cpi->MVBits_1 = 0;
 
  /* Choose modes; must be done in Hilbert order */
  for(; sb<sb_end; sb++){
    for(j = 0; j<4; j++){ /* mode addressing is through Y plane, always 4 MB per SB */
      int mbi = sb->m[j];
      ogg_uint32_t  cost[8] = {0,0,0,0, 0,0,0,0};
      macroblock_t *mb = &cpi->macro[mbi];

      if(cpi->FrameType == KEY_FRAME){
	mb->mode = CODE_INTRA;
	continue;
      }

      /**************************************************************
       Find the block choice with the lowest estimated coding cost

       NOTE THAT if U or V is coded but no Y from a macro block then
       the mode will be CODE_INTER_NO_MV as this is the default
       state to which the mode data structure is initialised in
       encoder and decoder at the start of each frame. */

      /* block coding cost is estimated from correlated SAD metrics */

      cost[CODE_INTER_NO_MV] = MBInterCost420(cpi,qi,mbi,(mv_t){0,0},0);

      /* 'should this have been a keyframe in retrospect' tracking;
	 includes none of the rest of the inter-style labelling and
	 flagging overhead, but must count 'uncoded' frags within the
	 frame */
      intrabits += MBIntraCost420(cpi,qi,mbi,1);

      if(mb->coded == 0){

	oc_mode_set(cpi,mb,CODE_INTER_NO_MV);
	mb->mv[0] = mb->mv[1] = mb->mv[2] = mb->mv[3] = (mv_t){0,0};

      }else{
	int mb_mv_bits_0;
	int mb_gmv_bits_0;
	int mb_4mv_bits_0;
	int mb_4mv_bits_1;
	int mode;

	cost[CODE_INTRA] = MBIntraCost420(cpi,qi,mbi,0);
	cost[CODE_INTER_PLUS_MV] = MBInterCost420(cpi,qi,mbi,mb->analysis_mv[0][0],0);
	cost[CODE_INTER_LAST_MV] = MBInterCost420(cpi,qi,mbi,last_mv,0);
	cost[CODE_INTER_PRIOR_LAST] = MBInterCost420(cpi,qi,mbi,prior_mv,0);
	cost[CODE_USING_GOLDEN] = MBInterCost420(cpi,qi,mbi,(mv_t){0,0},1);
	cost[CODE_GOLDEN_MV] = MBInterCost420(cpi,qi,mbi,mb->analysis_mv[0][1],1);
	cost[CODE_INTER_FOURMV] = MBInter4Cost420(cpi,qi,mbi,mb->mv,0);
	
	/* add estimated labelling cost for each mode */
	//for(i = 0; i < 8; i++)
	//cost[i] += oc_mode_cost(cpi,i) << OC_BIT_SCALE;
	
	/* Add the motion vector bits for each mode that requires them.*/
	mb_mv_bits_0  = MvBits[mb->analysis_mv[0][0].x + MAX_MV_EXTENT] + 
	  MvBits[mb->analysis_mv[0][0].y + MAX_MV_EXTENT];
	mb_gmv_bits_0 = MvBits[mb->analysis_mv[0][1].x+MAX_MV_EXTENT] + 
	  MvBits[mb->analysis_mv[0][1].y+MAX_MV_EXTENT];
	mb_4mv_bits_0 = mb_4mv_bits_1 = 0;
	if(mb->coded & 1){
	  mb_4mv_bits_0 += MvBits[mb->mv[0].x + MAX_MV_EXTENT] + 
	    MvBits[mb->mv[0].y+MAX_MV_EXTENT];
	  mb_4mv_bits_1 += 12;
	}
	if(mb->coded & 2){
	  mb_4mv_bits_0 += MvBits[mb->mv[1].x+MAX_MV_EXTENT] + 
	    MvBits[mb->mv[1].y+MAX_MV_EXTENT];
	  mb_4mv_bits_1 += 12;
	}
	if(mb->coded & 4){
	  mb_4mv_bits_0 += MvBits[mb->mv[2].x+MAX_MV_EXTENT] + 
	    MvBits[mb->mv[2].y+MAX_MV_EXTENT];
	  mb_4mv_bits_1 += 12;
	}
	if(mb->coded & 8){
	  mb_4mv_bits_0 += MvBits[mb->mv[3].x+MAX_MV_EXTENT] + 
	    MvBits[mb->mv[3].y+MAX_MV_EXTENT];
	  mb_4mv_bits_1 += 12;
	}
	
	/* We use the same opportunity cost method of estimating the
	   cost of coding the motion vectors with the two different
	   schemes as we do for estimating the cost of the mode
	   labels. However, because there are only two schemes and
	   they're both pretty simple, this can just be done inline.*/
	cost[CODE_INTER_PLUS_MV] += 
	  ((OC_MINI(cpi->MVBits_0 + mb_mv_bits_0, cpi->MVBits_1+12)-
	    OC_MINI(cpi->MVBits_0, cpi->MVBits_1)) << OC_BIT_SCALE);
	cost[CODE_GOLDEN_MV] +=
	  ((OC_MINI(cpi->MVBits_0 + mb_gmv_bits_0, cpi->MVBits_1+12)-
	    OC_MINI(cpi->MVBits_0, cpi->MVBits_1)) << OC_BIT_SCALE);
	cost[CODE_INTER_FOURMV] +=
	  ((OC_MINI(cpi->MVBits_0 + mb_4mv_bits_0, cpi->MVBits_1 + mb_4mv_bits_1)-
	    OC_MINI(cpi->MVBits_0, cpi->MVBits_1)) << OC_BIT_SCALE);

	/* train this too... because the bit cost of an MV should be
	   considered in the context of LAST_MV and PRIOR_LAST. */
	cost[CODE_INTER_PLUS_MV] -= 384;
	
	
	/* Finally, pick the mode with the cheapest estimated bit cost.*/
	mode=0;
	for(i=1;i<8;i++)
	  if(cost[i]<cost[mode])
	    mode=i;

	/* add back such that inter/intra counting are relatively correct */
	cost[CODE_INTER_PLUS_MV] += 384;

	switch(mode){
	case CODE_INTER_PLUS_MV:
	  cpi->MVBits_0 += mb_mv_bits_0;
	  cpi->MVBits_1 += 12;
	  prior_mv = last_mv;
	  last_mv = mb->mv[0] = mb->mv[1] = mb->mv[2] = mb->mv[3] = mb->analysis_mv[0][0];
	  break;
	case CODE_INTER_LAST_MV:
	  mb->mv[0] = mb->mv[1] = mb->mv[2] = mb->mv[3] = last_mv;
	  break;
	case CODE_INTER_PRIOR_LAST:
	  mb->mv[0] = mb->mv[1] = mb->mv[2] = mb->mv[3] = prior_mv;
	  prior_mv = last_mv;
	  last_mv = mb->mv[0];
	  break;
	case CODE_INTER_FOURMV:
	  cpi->MVBits_0 += mb_4mv_bits_0;
	  cpi->MVBits_1 += mb_4mv_bits_1;
	  prior_mv = last_mv;
	  last_mv = mb->mv[3]; /* if coded, it is still used forced to 0,0 according to spec */
	  break;
	case CODE_GOLDEN_MV:
	  cpi->MVBits_0 += mb_gmv_bits_0;
	  cpi->MVBits_1 += 12;
	  mb->mv[0] = mb->mv[1] = mb->mv[2] = mb->mv[3] = mb->analysis_mv[0][1];
	  break;
	default:
	  mb->mv[0] = mb->mv[1] = mb->mv[2] = mb->mv[3] = (mv_t){0,0};
	  break;
	}
	oc_mode_set(cpi,mb,mode);      
      }

      interbits += cost[mb->mode];

    }
  }

  if(cpi->FrameType != KEY_FRAME){

    if(interbits>intrabits) return 1; /* short circuit */
    
    /* finish adding flagging overhead costs to inter bit counts */
    
    if(cpi->MVBits_0 < cpi->MVBits_1)
      interbits += (cpi->MVBits_0 << OC_BIT_SCALE);
    else
      interbits += (cpi->MVBits_1 << OC_BIT_SCALE);
    
    interbits += (cpi->chooser.scheme_bits[cpi->chooser.scheme_list[0]] << OC_BIT_SCALE);
    
    if(interbits>intrabits) return 1; /* short circuit */

    /* The easiest way to count the bits needed for coded/not coded fragments is
       to code them. */
    {
      ogg_uint32_t bits = oggpackB_bits(cpi->oggbuffer);
      PackAndWriteDFArray(cpi);
      interbits += ((oggpackB_bits(cpi->oggbuffer) - bits) << OC_BIT_SCALE);
    }
    
    if(interbits>intrabits) return 1; 
    
  }

  return 0;
}
#ifdef COLLECT_METRICS

#include <stdio.h>
#include <math.h>

#define ZWEIGHT 100
#define BIN(sad) (OC_MINI((sad)>>OC_SAD_SHIFT,(OC_SAD_BINS-1)))

static void UpdateModeEstimation(CP_INSTANCE *cpi){
  /* compile collected SAD/rate metrics into an immediately useful
     mode estimation form */

  int plane,mode,bin;
  int qi = cpi->BaseQ; /* temporary */

  /* Convert raw collected data into cleaned up sample points */
  /* metrics are collected into fewer bins than we eventually use as a
     bitrate metric in mode selection. */

  for(plane=0;plane<3;plane++)
    for(mode=0;mode<2;mode++){
      ogg_int64_t lastx = -1;
      ogg_int64_t lasty = -1;
      int a = -1;
      int b = -1;
      ogg_int64_t sadx=0;
      ogg_int64_t bity=0;
      ogg_int64_t frags=0;
      int rbin=0;
      for(bin=0;bin<OC_SAD_BINS;bin++){
	sadx += mode_metric[qi][plane][mode].sad[bin];
	bity += mode_metric[qi][plane][mode].bits[bin];
	frags += mode_metric[qi][plane][mode].frag[bin];
	if(frags > ZWEIGHT){
	  sadx = (sadx + (frags>>1))/frags;
	  bity = (bity + (frags>>1))/frags;
	  if(lastx != -1LL){
	    b = ((bity - lasty)<<8)/(sadx-lastx);
	    a = lasty - (((lastx * b) + (1<<7))>>8);
	    
	    for(;rbin<<OC_SAD_SHIFT <= sadx && rbin <= OC_SAD_BINS;rbin++)
	      mode_rate[qi][plane][mode][rbin] = a + ((b * (rbin<<OC_SAD_SHIFT) + (1<<7))>>8);
	    
	  }
	  lastx = sadx;
	  lasty = bity;
	  frags = 0;
	}
      }
      if(lastx!=-1LL){
	for(;rbin <= OC_SAD_BINS;rbin++)
	  mode_rate[qi][plane][mode][rbin] = mode_rate[qi][plane][mode][rbin-1];
      }else{
	for(;rbin <= OC_SAD_BINS;rbin++)
	  mode_rate[qi][plane][mode][rbin] = 0;
	
      }
    }
}

static int parse_eob_run(int token, int eb){
  switch(token){
  case DCT_EOB_TOKEN:
    return 1;
  case DCT_EOB_PAIR_TOKEN:
    return 2;
  case DCT_EOB_TRIPLE_TOKEN:
    return 3;
  case DCT_REPEAT_RUN_TOKEN:
    return eb+4;
  case DCT_REPEAT_RUN2_TOKEN:
    return eb+8;
  case DCT_REPEAT_RUN3_TOKEN:
    return eb+16;
  case DCT_REPEAT_RUN4_TOKEN:
    return eb;
  default:
    return 0;
  }
}

static void ModeMetricsGroup(CP_INSTANCE *cpi, int group, int huffY, int huffC, int *eobcounts, int *actual_bits){
  int ti;
  int *stack = cpi->dct_eob_fi_stack[group];
  int ty = cpi->dct_token_ycount[group];
  int *tfi = cpi->dct_token_frag[group];
  int tn = cpi->dct_token_count[group];

  for(ti=0;ti<tn;ti++){
    int huff = (ti<ty?huffY:huffC);
    int token = cpi->dct_token[group][ti];
    int bits = cpi->HuffCodeLengthArray_VP3x[huff][token] + cpi->ExtraBitLengths_VP3x[token];
    
    if(token>DCT_REPEAT_RUN4_TOKEN){
      /* not an EOB run; this token belongs to a single fragment */
      int fi = tfi[ti];
      actual_bits[fi] += (bits<<OC_BIT_SCALE);
    }else{
      /* EOB run; its bits should be split up between all the fragments in the run */
      int run = parse_eob_run(token, cpi->dct_token_eb[group][ti]);
      int fracbits = ((bits<<OC_BIT_SCALE) + (run>>1))/run;

      if(ti+1<tn){
	/* tokens follow EOB so it must be entirely ensconced within this group */
	while(run--){
	  int fi = stack[eobcounts[group]++];
	  actual_bits[fi]+=fracbits;
	}
      }else{
	/* EOB is the last token in this group, so it may span into the next group (or groups) */
	int n = cpi->dct_eob_fi_count[group];
	while(run){
	  while(eobcounts[group] < n && run){
	    int fi = stack[eobcounts[group]++];
	    actual_bits[fi]+=fracbits;
	    run--;
	  }
	  group++;
	  n = cpi->dct_eob_fi_count[group];
	  stack = cpi->dct_eob_fi_stack[group];
	}
      }
    }
  }
}

void ModeMetrics(CP_INSTANCE *cpi, int huff[4]){
  int fi,gi;
  int y = cpi->frag_n[0];
  int u = y + cpi->frag_n[1];
  int v = cpi->frag_total;
  unsigned char *cp = cpi->frag_coded;
  int *sp = cpi->frag_sad;
  int *mp = cpi->frag_mbi;
  int eobcounts[64];
  int qi = cpi->BaseQ; /* temporary */
  int actual_bits[cpi->frag_total];
  memset(actual_bits,0,sizeof(actual_bits));
  memset(eobcounts,0,sizeof(eobcounts));

  if(mode_metrics==0){
    memset(mode_metric,0,sizeof(mode_metric));
    mode_metrics=1;
  }

  /* count bits for tokens */
  ModeMetricsGroup(cpi, 0, huff[0], huff[1], eobcounts, actual_bits);
  for(gi=1;gi<=AC_TABLE_2_THRESH;gi++)
    ModeMetricsGroup(cpi, gi,  huff[2], huff[3], eobcounts, actual_bits);
  for(;gi<=AC_TABLE_3_THRESH;gi++)
    ModeMetricsGroup(cpi, gi, huff[2]+AC_HUFF_CHOICES, huff[3]+AC_HUFF_CHOICES, eobcounts, actual_bits);
  for(;gi<=AC_TABLE_4_THRESH;gi++)
    ModeMetricsGroup(cpi, gi, huff[2]+AC_HUFF_CHOICES*2, huff[3]+AC_HUFF_CHOICES*2, eobcounts, actual_bits);
  for(;gi<BLOCK_SIZE;gi++)
    ModeMetricsGroup(cpi, gi, huff[2]+AC_HUFF_CHOICES*3, huff[3]+AC_HUFF_CHOICES*3, eobcounts, actual_bits);

  /* accumulate */
  for(fi=0;fi<v;fi++)
    if(cp[fi]){
      macroblock_t *mb = &cpi->macro[mp[fi]];
      int mode = mb->mode;
      int plane = (fi<y ? 0 : (fi<u ? 1 : 2));
      int bin = BIN(sp[fi]);
      if(bin<OC_SAD_BINS){
	mode_metric[qi][plane][mode==CODE_INTRA].frag[bin]++;
	mode_metric[qi][plane][mode==CODE_INTRA].sad[bin] += sp[fi];
	mode_metric[qi][plane][mode==CODE_INTRA].bits[bin] += actual_bits[fi];
      }
    }

  /* update global SAD/rate estimation matrix */
  UpdateModeEstimation(cpi);
}

void DumpMetrics(CP_INSTANCE *cpi){
  int qi,plane,mode,bin;

  fprintf(stdout,
	  "/* file generated by libtheora with COLLECT_METRICS defined at compile time */\n\n"

	  "#define OC_BIT_SCALE (7)\n"
	  "#define OC_SAD_BINS (%d)\n"
	  "#define OC_SAD_SHIFT (%d)\n"
	  "\n"

	  "#ifdef COLLECT_METRICS\n"
	  "typedef struct {\n"
	  "  ogg_int64_t      bits[OC_SAD_BINS];\n"
	  "  ogg_int64_t      frag[OC_SAD_BINS];\n"
	  "  ogg_int64_t      sad[OC_SAD_BINS];\n"
	  "} mode_metric_t;\n"

	  "int              mode_metrics = 1;\n"
	  "mode_metric_t    mode_metric[64][3][2]={\n",OC_SAD_BINS,OC_SAD_SHIFT);
  
  for(qi=0;qi<64;qi++){
    fprintf(stdout,"  {\n");
    for(plane=0;plane<3;plane++){
      fprintf(stdout,"    {\n");
      for(mode=0;mode<2;mode++){
	fprintf(stdout,"      { /* qi=%d %c %s */\n",qi,(plane?(plane==1?'U':'V'):'Y'),(mode?"INTRA":"INTER"));

	fprintf(stdout,"        { ");
	for(bin=0;bin<OC_SAD_BINS;bin++){
	  if(bin && !(bin&0x3))fprintf(stdout,"\n          ");
	  fprintf(stdout,"%12lldLL,",mode_metric[qi][plane][mode].bits[bin]);
	}
	fprintf(stdout," },\n");
	fprintf(stdout,"        { ");
	for(bin=0;bin<OC_SAD_BINS;bin++){
	  if(bin && !(bin&0x3))fprintf(stdout,"\n          ");
	  fprintf(stdout,"%12lldLL,",mode_metric[qi][plane][mode].frag[bin]);
	}
	fprintf(stdout," },\n");
	fprintf(stdout,"        { ");
	for(bin=0;bin<OC_SAD_BINS;bin++){
	  if(bin && !(bin&0x3))fprintf(stdout,"\n          ");
	  fprintf(stdout,"%12lldLL,",mode_metric[qi][plane][mode].sad[bin]);
	}
	fprintf(stdout," },\n");
	fprintf(stdout,"      },\n");

      }
      fprintf(stdout,"    },\n");
    }
    fprintf(stdout,"  },\n");
  }
  fprintf(stdout,"};\n\n#endif\n\n");

  fprintf(stdout,
	  "ogg_int32_t     mode_rate[64][3][2][OC_SAD_BINS+1]={\n");
  for(qi=0;qi<64;qi++){
    fprintf(stdout,"  {\n");
    for(plane=0;plane<3;plane++){
      fprintf(stdout,"    {\n");
      for(mode=0;mode<2;mode++){
	fprintf(stdout,"      { /* qi=%d %c %s */\n        ",qi,(plane?(plane==1?'U':'V'):'Y'),(mode?"INTRA":"INTER"));

	for(bin=0;bin<OC_SAD_BINS+1;bin++){
	  if(bin && !(bin&0x7))fprintf(stdout,"\n        ");
	  fprintf(stdout,"%6d,",mode_rate[qi][plane][mode][bin]);
	}

	fprintf(stdout," },\n");
      }
      fprintf(stdout,"    },\n");
    }
    fprintf(stdout,"  },\n");
  }
  fprintf(stdout,"};\n\n");

}

#endif
