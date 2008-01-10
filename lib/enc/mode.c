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
  
  /*Scheme 0 starts with 24 bits to store the mode list in.*/
  chooser->scheme_bits[0] = 24;
  memset(chooser->scheme_bits+1,0,7*sizeof(chooser->scheme_bits[1]));
  for(i=0;i<8;i++){
    /*Scheme 7 should always start first, and scheme 0 should always start
       last.*/
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

void oc_mode_unset(CP_INSTANCE *cpi, 
		   macroblock_t *mb){

  oc_mode_scheme_chooser *chooser = &cpi->chooser;
  int ri;
  int si;
  int mode = mb->mode;

  chooser->mode_counts[mode]--;

  /* Re-order the scheme0 mode list if necessary. */
  for(ri = chooser->scheme0_ranks[mode]; ri<7; ri++){
    int pmode;
    pmode=chooser->scheme0_list[ri+1];
    if(chooser->mode_counts[pmode] <= chooser->mode_counts[mode])break;

    /* reorder the mode ranking */
    chooser->scheme0_ranks[pmode]--;
    chooser->scheme0_list[ri]=pmode;

  }
  chooser->scheme0_ranks[mode]=ri;
  chooser->scheme0_list[ri]=mode;

  /* Now remove the bit cost of the mode from each scheme.*/
  for(si=0;si<8;si++){
    chooser->scheme_bits[si]-=
      chooser->mode_bits[si][chooser->mode_ranks[si][mode]];
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
  ogg_uint32_t acc = 0;
  int stride = cpi->stride[plane];
  int j;
  
  for(j=0;j<8;j++){
    acc += b[0]; 
    acc += b[1]; 
    acc += b[2]; 
    acc += b[3]; 
    acc += b[4]; 
    acc += b[5]; 
    acc += b[6]; 
    acc += b[7]; 
    b += stride;
  }
  
  b = cpi->frame + cpi->frag_buffer_index[fi];
  for(j=0;j<8;j++){
    sad += abs ((b[0]<<6)-acc); 
    sad += abs ((b[1]<<6)-acc); 
    sad += abs ((b[2]<<6)-acc); 
    sad += abs ((b[3]<<6)-acc); 
    sad += abs ((b[4]<<6)-acc); 
    sad += abs ((b[5]<<6)-acc); 
    sad += abs ((b[6]<<6)-acc); 
      sad += abs ((b[7]<<6)-acc); 
      b += stride;
  }

  return sad;
}

/* equivalent to adding up the abs values of the AC components of a block */
static int MBIntraCost420(CP_INSTANCE *cpi, int qi, int mbi, int all){
  unsigned char *cp = cpi->frag_coded;
  macroblock_t *mb = &cpi->macro[mbi];
  int cost=0;

  /* all frags in a macroblock are valid so long as the macroblock itself is valid */
  if(mbi < cpi->macro_total){ 
    if(all || cp[mb->yuv[0][0]])
      cost += OC_RES_BITRATES[qi][0][OC_MODE_INTRA]
	[OC_MINI(BIntraSAD(cpi,mb->yuv[0][0],0)>>12,15)];
    if(all || cp[mb->yuv[0][1]])
      cost += OC_RES_BITRATES[qi][0][OC_MODE_INTRA]
	[OC_MINI(BIntraSAD(cpi,mb->yuv[0][1],0)>>12,15)];
    if(all || cp[mb->yuv[0][2]])
      cost += OC_RES_BITRATES[qi][0][OC_MODE_INTRA]
	[OC_MINI(BIntraSAD(cpi,mb->yuv[0][2],0)>>12,15)];
    if(all || cp[mb->yuv[0][3]])
      cost += OC_RES_BITRATES[qi][0][OC_MODE_INTRA]
	[OC_MINI(BIntraSAD(cpi,mb->yuv[0][3],0)>>12,15)];
    
    if(all || cp[mb->yuv[1][0]])
      cost += OC_RES_BITRATES[qi][1][OC_MODE_INTRA]
	[OC_MINI(BIntraSAD(cpi,mb->yuv[1][0],1)>>12,15)];
    if(all || cp[mb->yuv[2][0]])
      cost += OC_RES_BITRATES[qi][2][OC_MODE_INTRA]
	[OC_MINI(BIntraSAD(cpi,mb->yuv[2][0],2)>>12,15)];
  }
  
  /* Bit costs are stored in the table with extra precision. Round them down to whole bits.*/
  return cost + (1<<OC_BIT_SCALE-1) >> OC_BIT_SCALE;
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
      sad += abs (b[0]-((r[0]+r2[0])>>1));
      sad += abs (b[1]-((r[1]+r2[1])>>1));
      sad += abs (b[2]-((r[2]+r2[2])>>1));
      sad += abs (b[3]-((r[3]+r2[3])>>1));
      sad += abs (b[4]-((r[4]+r2[4])>>1));
      sad += abs (b[5]-((r[5]+r2[5])>>1));
      sad += abs (b[6]-((r[6]+r2[6])>>1));
      sad += abs (b[7]-((r[7]+r2[7])>>1));
      b += stride;
    }
    
  }else{
    for(j=0;j<8;j++){
      sad += abs (b[0]-r[0]);
      sad += abs (b[1]-r[1]);
      sad += abs (b[2]-r[2]);
      sad += abs (b[3]-r[3]);
      sad += abs (b[4]-r[4]);
      sad += abs (b[5]-r[5]);
      sad += abs (b[6]-r[6]);
      sad += abs (b[7]-r[7]);
      b += stride;
    }
  }
  return sad;
}

static int MBInterCost420(CP_INSTANCE *cpi, int qi, int modei, int mbi, mv_t mv, int goldenp){
  unsigned char *cp = cpi->frag_coded;
  macroblock_t *mb = &cpi->macro[mbi];
  int cost = 0;

  if(cp[mb->yuv[0][0]])
    cost += OC_RES_BITRATES[qi][0][modei]
      [OC_MINI(BInterSAD(cpi,mb->yuv[0][0],0,goldenp,mv,0)>>6,15)];
  if(cp[mb->yuv[0][1]])
    cost += OC_RES_BITRATES[qi][0][modei]
      [OC_MINI(BInterSAD(cpi,mb->yuv[0][1],0,goldenp,mv,0)>>6,15)];
  if(cp[mb->yuv[0][2]])
    cost += OC_RES_BITRATES[qi][0][modei]
      [OC_MINI(BInterSAD(cpi,mb->yuv[0][2],0,goldenp,mv,0)>>6,15)];
  if(cp[mb->yuv[0][3]])
    cost += OC_RES_BITRATES[qi][0][modei]
      [OC_MINI(BInterSAD(cpi,mb->yuv[0][3],0,goldenp,mv,0)>>6,15)];

  if(cp[mb->yuv[1][0]])
    cost += OC_RES_BITRATES[qi][1][modei]
      [OC_MINI(BInterSAD(cpi,mb->yuv[1][0],1,goldenp,mv,1)>>6,15)];
  if(cp[mb->yuv[2][0]])
    cost += OC_RES_BITRATES[qi][2][modei]
      [OC_MINI(BInterSAD(cpi,mb->yuv[2][0],2,goldenp,mv,1)>>6,15)];

  /* Bit costs are stored in the table with extra precision. Round them down to whole bits.*/
  return cost + (1<<OC_BIT_SCALE-1) >> OC_BIT_SCALE;
}

static int MBInter4Cost420(CP_INSTANCE *cpi, int qi, int mbi, mv_t mv[4], int goldenp){
  unsigned char *cp = cpi->frag_coded;
  macroblock_t *mb = &cpi->macro[mbi];
  int cost = 0;
  mv_t ch;

  if(cp[mb->yuv[0][0]])
    cost += OC_RES_BITRATES[qi][0][CODE_INTER_FOURMV]
      [OC_MINI(BInterSAD(cpi,mb->yuv[0][0],0,goldenp,mv[0],0)>>6,15)];
  if(cp[mb->yuv[0][1]])
    cost += OC_RES_BITRATES[qi][0][CODE_INTER_FOURMV]
      [OC_MINI(BInterSAD(cpi,mb->yuv[0][1],0,goldenp,mv[1],0)>>6,15)];
  if(cp[mb->yuv[0][2]])
    cost += OC_RES_BITRATES[qi][0][CODE_INTER_FOURMV]
      [OC_MINI(BInterSAD(cpi,mb->yuv[0][2],0,goldenp,mv[2],0)>>6,15)];
  if(cp[mb->yuv[0][3]])
    cost += OC_RES_BITRATES[qi][0][CODE_INTER_FOURMV]
      [OC_MINI(BInterSAD(cpi,mb->yuv[0][3],0,goldenp,mv[3],0)>>6,15)];

  /* Calculate motion vector as the average of the Y plane ones. */
  /* Uncoded members are 0,0 and not special-cased */
  ch.x = mv[0].x + mv[1].x + mv[2].x + mv[3].x;
  ch.y = mv[0].y + mv[1].y + mv[2].y + mv[3].y;
  
  ch.x = ( ch.x >= 0 ? (ch.x + 2) / 4 : (ch.x - 2) / 4);
  ch.y = ( ch.y >= 0 ? (ch.y + 2) / 4 : (ch.y - 2) / 4);
  
  if(cp[mb->yuv[1][0]])
    cost += OC_RES_BITRATES[qi][1][CODE_INTER_FOURMV]
      [OC_MINI(BInterSAD(cpi,mb->yuv[1][0],1,goldenp,ch,1)>>6,15)];
  if(cp[mb->yuv[2][0]])
    cost += OC_RES_BITRATES[qi][2][CODE_INTER_FOURMV]
      [OC_MINI(BInterSAD(cpi,mb->yuv[2][0],2,goldenp,ch,1)>>6,15)];

  /* Bit costs are stored in the table with extra precision. Round them down to whole bits.*/
  return cost + (1<<OC_BIT_SCALE-1) >> OC_BIT_SCALE;
}

int PickModes(CP_INSTANCE *cpi){
  
  unsigned char qi = cpi->BaseQ; // temporary
  superblock_t *sb = cpi->super[0];
  superblock_t *sb_end = sb + cpi->super_n[0];
  unsigned char *cp = cpi->frag_coded;
  mc_state mcenc;
  int mbi, bi, i;
  ogg_uint32_t interbits = 0;
  ogg_uint32_t intrabits = 0;

  mv_t last_mv = {0,0};
  mv_t prior_mv = {0,0};

  oc_mcenc_start(cpi, &mcenc); 
  for(mbi = 0; mbi<cpi->macro_total; mbi++){
    macroblock_t *mb     = &cpi->macro[mbi];

    /*Move the motion vector predictors back a frame */
    memmove(mb->analysis_mv+1,mb->analysis_mv,2*sizeof(mb->analysis_mv[0]));

    /* basic 1MV search always done for all macroblocks, coded or not, keyframe or not */
    oc_mcenc_search(cpi, &mcenc, mbi, 0, mb->mv);

    /* replace the block MVs for not-coded blocks with (0,0).*/    
    mb->coded = 0;
    for ( bi=0; bi<4; bi++ ){
      int fi = mb->yuv[0][bi];
      if(!cp[fi]) 
	mb->mv[fi]=(mv_t){0,0};
      else
	mb->coded |= (1<<bi);
    }

    if(mb->coded==0){
      /* Don't bother to do a MV search against the golden frame.
	 Just re-use the last vector, which should match well since the
	 contents of the MB haven't changed much.*/
      mb->analysis_mv[0][1]=mb->analysis_mv[1][1];
      continue;
    }

    /* search golden frame */
    oc_mcenc_search(cpi, &mcenc, mbi, 1, NULL);
  }
  
  oc_mode_scheme_chooser_init(cpi);
  cpi->MVBits_0 = 0;
  cpi->MVBits_1 = 0;
 
  /* Choose modes; must be done in Hilbert order */
  for(; sb<sb_end; sb++){
    for(mbi = 0; mbi<4; mbi++){ /* mode addressing is through Y plane, always 4 MB per SB */
      ogg_uint32_t  cost[8] = {0,0,0,0, 0,0,0,0};
      macroblock_t *mb = &cpi->macro[sb->m[mbi]];

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

      cost[CODE_INTER_NO_MV] = MBInterCost420(cpi,qi,CODE_INTER_NO_MV,mbi,(mv_t){0,0},0);

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
	cost[CODE_INTER_PLUS_MV] = MBInterCost420(cpi,qi,CODE_INTER_PLUS_MV,mbi,mb->analysis_mv[0][0],0);
	cost[CODE_INTER_LAST_MV] = MBInterCost420(cpi,qi,CODE_INTER_LAST_MV,mbi,last_mv,0);
	cost[CODE_INTER_PRIOR_LAST] = MBInterCost420(cpi,qi,CODE_INTER_PRIOR_LAST,mbi,prior_mv,0);
	cost[CODE_USING_GOLDEN] = MBInterCost420(cpi,qi,CODE_USING_GOLDEN,mbi,(mv_t){0,0},1);
	cost[CODE_GOLDEN_MV] = MBInterCost420(cpi,qi,CODE_GOLDEN_MV,mbi,mb->analysis_mv[0][1],0);
	cost[CODE_INTER_FOURMV] = MBInter4Cost420(cpi,qi,mbi,mb->mv,0);
	
	/* add estimated labelling cost for each mode */
	for(i = 0; i < 8; i++)
	  cost[i] += oc_mode_cost(cpi,i);
	
	/* Add the motion vector bits for each mode that requires them.*/
	mb_mv_bits_0  = MvBits[mb->analysis_mv[0][0].x] + MvBits[mb->analysis_mv[0][0].y];
	mb_gmv_bits_0 = MvBits[mb->analysis_mv[0][1].x] + MvBits[mb->analysis_mv[0][1].y];
	mb_4mv_bits_0 = mb_4mv_bits_1 = 0;
	if(mb->coded & 1){
	  mb_4mv_bits_0 += MvBits[mb->mv[0].x] + MvBits[mb->mv[0].y];
	  mb_4mv_bits_1 += 12;
	}
	if(mb->coded & 2){
	  mb_4mv_bits_0 += MvBits[mb->mv[1].x] + MvBits[mb->mv[1].y];
	  mb_4mv_bits_1 += 12;
	}
	if(mb->coded & 4){
	  mb_4mv_bits_0 += MvBits[mb->mv[2].x] + MvBits[mb->mv[2].y];
	  mb_4mv_bits_1 += 12;
	}
	if(mb->coded & 8){
	  mb_4mv_bits_0 += MvBits[mb->mv[3].x] + MvBits[mb->mv[3].y];
	  mb_4mv_bits_1 += 12;
	}
	
	/* We use the same opportunity cost method of estimating the
	   cost of coding the motion vectors with the two different
	   schemes as we do for estimating the cost of the mode
	   labels. However, because there are only two schemes and
	   they're both pretty simple, this can just be done inline.*/
	cost[CODE_INTER_PLUS_MV] += 
	  OC_MINI(cpi->MVBits_0 + mb_mv_bits_0, cpi->MVBits_1+12)-
	  OC_MINI(cpi->MVBits_0, cpi->MVBits_1);
	cost[CODE_GOLDEN_MV] +=
	  OC_MINI(cpi->MVBits_0 + mb_gmv_bits_0, cpi->MVBits_1+12)-
	  OC_MINI(cpi->MVBits_0, cpi->MVBits_1);
	cost[CODE_INTER_FOURMV] +=
	  OC_MINI(cpi->MVBits_0 + mb_4mv_bits_0, cpi->MVBits_1 + mb_4mv_bits_1)-
	  OC_MINI(cpi->MVBits_0, cpi->MVBits_1);
	
	/* Finally, pick the mode with the cheapest estimated bit cost.*/
	mode=0;
	for(i=1;i<8;i++)
	  if(cost[i]<cost[mode])
	    mode=i;

	switch(mode){
	case CODE_INTER_PLUS_MV:
	  cpi->MVBits_0 += mb_mv_bits_0;
	  cpi->MVBits_1 += 12;
	  prior_mv = last_mv;
	  last_mv = mb->mv[0] = mb->mv[1] = mb->mv[2] = mb->mv[3] = mb->analysis_mv[0][0];
	  break;
	case OC_MODE_INTER_MV_LAST:
	  mb->mv[0] = mb->mv[1] = mb->mv[2] = mb->mv[3] = last_mv;
	  break;
	case OC_MODE_INTER_MV_LAST2:
	  mb->mv[0] = mb->mv[1] = mb->mv[2] = mb->mv[3] = prior_mv;
	  prior_mv = last_mv;
	  last_mv = mb->mv[0];
	  break;
	case OC_MODE_INTER_MV_FOUR:
	  cpi->MVBits_0 += mb_4mv_bits_0;
	  cpi->MVBits_1 += mb_4mv_bits_1;
	  prior_mv = last_mv;
	  last_mv = mb->mv[3]; /* if coded, it is still used forced to 0,0 according to spec */
	  break;
	case OC_MODE_GOLDEN_MV:
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
      interbits += cpi->MVBits_0;
    else
      interbits += cpi->MVBits_1;
    
    interbits+=cpi->chooser.scheme_bits[cpi->chooser.scheme_list[0]];
    
    if(interbits>intrabits) return 1; /* short circuit */

    /* The easiest way to count the bits needed for coded/not coded fragments is
       to code them. */
    {
      ogg_uint32_t bits = oggpackB_bits(cpi->oggbuffer);
      PackAndWriteDFArray(cpi);
      interbits += oggpackB_bits(cpi->oggbuffer) - bits;
    }
    
    if(interbits>intrabits) return 1; 
    
  }

  return 0;
}


