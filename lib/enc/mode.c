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
    chooser->mode_ranks[i] = ModeSchemes[i-1];

  memset(chooser->mode_counts,0,OC_NMODES*sizeof(*chooser->mode_counts));
  
  /* Scheme 0 starts with 24 bits to store the mode list in. */
  chooser->scheme_bits[0] = 24;
  memset(chooser->scheme_bits+1,0,7*sizeof(*chooser->scheme_bits));
  for(i=0;i<8;i++){
    /* Scheme 7 should always start first, and scheme 0 should always start
       last. */
    chooser->scheme_list[i] = 7-i;
    chooser->scheme0_list[i] = chooser->scheme0_ranks[i] = i;
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

static int BInterSAD(CP_INSTANCE *cpi, int fi, int plane, int goldenp, mv_t mv){
  int sad = 0;
  unsigned char *b = cpi->frame + cpi->frag_buffer_index[fi];
  int qp = (plane>0);
  int mx = mvmap[qp][mv.x+31];
  int my = mvmap[qp][mv.y+31];
  int mx2 = mvmap2[qp][mv.x+31];
  int my2 = mvmap2[qp][mv.y+31];

  int stride = cpi->stride[plane];
  unsigned char *r = (goldenp ? cpi->golden : cpi->lastrecon ) + 
    cpi->frag_buffer_index[fi] + my * stride + mx;
  
  if(mx2 || my2){
    unsigned char *r2 = r + my2 * stride + mx2;
    sad =  dsp_sad8x8_xy2_thres (cpi->dsp, b, r, r2, stride, 9999999);
  }else{
    sad =  dsp_sad8x8 (cpi->dsp, b, r, stride);
  }

  if(plane)
    return sad<<2;
  else
    return sad;
}

static void MBCost(CP_INSTANCE *cpi, int qi, int mbi, int cost[8], int sad[8][3][4]){
  unsigned char *cp = cpi->frag_coded;
  macroblock_t *mb = &cpi->macro[mbi];
  int i,j;

  for(i=0;i<3;i++){
    for(j=0;j<4;j++){
      int fi=mb->Ryuv[i][j];
      if(cp[fi]){
	cost[0] += BINMAP(mode_rate[qi][i][0],sad[0][i][j]);
	cost[1] += BINMAP(mode_rate[qi][i][1],sad[1][i][j]);
	cost[2] += BINMAP(mode_rate[qi][i][0],sad[2][i][j]);
	cost[3] += BINMAP(mode_rate[qi][i][0],sad[3][i][j]);
	cost[4] += BINMAP(mode_rate[qi][i][0],sad[4][i][j]);
	cost[5] += BINMAP(mode_rate[qi][i][0],sad[5][i][j]);
	cost[6] += BINMAP(mode_rate[qi][i][0],sad[6][i][j]);
	cost[7] += BINMAP(mode_rate[qi][i][0],sad[7][i][j]);
      }
    }
  }
}

static int cost_intra(CP_INSTANCE *cpi, int qi, int mbi, ogg_uint32_t *intrabits){
  unsigned char *cp = cpi->frag_coded;
  macroblock_t *mb = &cpi->macro[mbi];
  int i,j;
  int cost = 0;
  for(i=0;i<3;i++){
    for(j=0;j<4;j++){
      int fi=mb->Ryuv[i][j];
      if(cp[fi]){
	int sad = BIntraSAD(cpi,fi,i);
	cost += BINMAP(mode_rate[qi][i][1],sad);
      }
    }
  }
  *intrabits+=cost;
  return cost + (oc_mode_cost(cpi,1) << OC_BIT_SCALE);
}

static int cost_inter(CP_INSTANCE *cpi, int qi, int mbi, mv_t mv, int mode){
  unsigned char *cp = cpi->frag_coded;
  macroblock_t *mb = &cpi->macro[mbi];
  int i,j;
  int cost = 0;
  for(i=0;i<3;i++){
    for(j=0;j<4;j++){
      int fi=mb->Ryuv[i][j];
      if(cp[fi]){
	int sad = BInterSAD(cpi,fi,i,mode==CODE_USING_GOLDEN,mv);
	cost += BINMAP(mode_rate[qi][i][0],sad);
      }
    }
  }
  return cost + (oc_mode_cost(cpi,mode) << OC_BIT_SCALE);
}

static int cost_inter1mv(CP_INSTANCE *cpi, int qi, int mbi, int golden, int *bits0){
  unsigned char *cp = cpi->frag_coded;
  macroblock_t *mb = &cpi->macro[mbi];
  int i,j;
  int cost = 0;

  for(i=0;i<3;i++){
    for(j=0;j<4;j++){
      int fi=mb->Ryuv[i][j];
      if(cp[fi]){
	int sad = BInterSAD(cpi,fi,i,golden,mb->analysis_mv[0][golden]);
	cost += BINMAP(mode_rate[qi][i][0],sad);
      }
    }
  }
  cost += oc_mode_cost(cpi,golden?CODE_GOLDEN_MV:CODE_INTER_PLUS_MV) << OC_BIT_SCALE;

  *bits0  = MvBits[mb->analysis_mv[0][golden].x + MAX_MV_EXTENT] + 
    MvBits[mb->analysis_mv[0][golden].y + MAX_MV_EXTENT];
  
  return cost + ((OC_MINI(cpi->MVBits_0 + *bits0, cpi->MVBits_1+12)-
		  OC_MINI(cpi->MVBits_0, cpi->MVBits_1)) << OC_BIT_SCALE);
}

static int cost_inter4mv(CP_INSTANCE *cpi, int qi, int mbi, int *bits0, int *bits1){
  unsigned char *cp = cpi->frag_coded;
  macroblock_t *mb = &cpi->macro[mbi];
  int j;
  int cost = 0;
  mv_t ch;

  /* 4:2:0 specific for now; fill in other modes here (not in encode loop) */

  for(j=0;j<4;j++){
    int fi=mb->Ryuv[0][j];
    if(cp[fi]){
      int sad = BInterSAD(cpi,fi,0,0,mb->mv[j]);
      cost += BINMAP(mode_rate[qi][0][0],sad);
    }
  }

  ch.x = mb->mv[0].x + mb->mv[1].x + mb->mv[2].x + mb->mv[3].x;
  ch.y = mb->mv[0].y + mb->mv[1].y + mb->mv[2].y + mb->mv[3].y;
  
  ch.x = ( ch.x >= 0 ? (ch.x + 2) / 4 : (ch.x - 2) / 4);
  ch.y = ( ch.y >= 0 ? (ch.y + 2) / 4 : (ch.y - 2) / 4);

  {
    int fi=mb->Ryuv[1][0];
    if(cp[fi]){
      int sad = BInterSAD(cpi,fi,1,0,ch);
      cost += BINMAP(mode_rate[qi][1][0],sad);
    }
  }
  {
    int fi=mb->Ryuv[2][0];
    if(cp[fi]){
      int sad = BInterSAD(cpi,fi,2,0,ch);
      cost += BINMAP(mode_rate[qi][2][0],sad);
    }
  }

  cost += (oc_mode_cost(cpi,CODE_INTER_FOURMV) << OC_BIT_SCALE);

  *bits0 = *bits1 = 0;
      
  if(mb->coded & 1){
    *bits0 += MvBits[mb->mv[0].x + MAX_MV_EXTENT] + 
      MvBits[mb->mv[0].y+MAX_MV_EXTENT];
    *bits1 += 12;
  }
  if(mb->coded & 2){
    *bits0 += MvBits[mb->mv[1].x+MAX_MV_EXTENT] + 
      MvBits[mb->mv[1].y+MAX_MV_EXTENT];
    *bits1 += 12;
  }
  if(mb->coded & 4){
    *bits0 += MvBits[mb->mv[2].x+MAX_MV_EXTENT] + 
      MvBits[mb->mv[2].y+MAX_MV_EXTENT];
    *bits1 += 12;
  }
  if(mb->coded & 8){
    *bits0 += MvBits[mb->mv[3].x+MAX_MV_EXTENT] + 
      MvBits[mb->mv[3].y+MAX_MV_EXTENT];
    *bits1 += 12;
  }
      
  return cost + ((OC_MINI(cpi->MVBits_0 + *bits0, cpi->MVBits_1 + *bits1)-
		  OC_MINI(cpi->MVBits_0, cpi->MVBits_1)) << OC_BIT_SCALE);
}

static void MBSAD420(CP_INSTANCE *cpi, int mbi, mv_t last, mv_t last2,
			int sad[8][3][4]){
  unsigned char *cp = cpi->frag_coded;
  macroblock_t *mb = &cpi->macro[mbi];
  int fi,i,j;
  mv_t ch;

  for(i=0;i<4;i++){
    for(j=0;j<3;j++){
      fi=mb->Ryuv[j][i];
      if(cp[fi]){
	sad[0][j][i] = BInterSAD(cpi,fi,j,0,(mv_t){0,0});
	sad[1][j][i] = BIntraSAD(cpi,fi,j);
	sad[2][j][i] = BInterSAD(cpi,fi,j,0,mb->analysis_mv[0][0]);
	sad[3][j][i] = BInterSAD(cpi,fi,j,0,last);
	sad[4][j][i] = BInterSAD(cpi,fi,j,0,last2);
	sad[5][j][i] = BInterSAD(cpi,fi,j,1,(mv_t){0,0});
	sad[6][j][i] = BInterSAD(cpi,fi,j,1,mb->analysis_mv[0][1]);
      }
    }
    fi=mb->Ryuv[0][i];
    if(cp[fi])
      sad[7][0][i] = BInterSAD(cpi,fi,0,0,mb->mv[i]);
  }

  /* 4:2:0-specific */
  /* Calculate motion vector as the average of the Y plane ones. */
  /* Uncoded members are 0,0 and not special-cased */
  ch.x = mb->mv[0].x + mb->mv[1].x + mb->mv[2].x + mb->mv[3].x;
  ch.y = mb->mv[0].y + mb->mv[1].y + mb->mv[2].y + mb->mv[3].y;
  
  ch.x = ( ch.x >= 0 ? (ch.x + 2) / 4 : (ch.x - 2) / 4);
  ch.y = ( ch.y >= 0 ? (ch.y + 2) / 4 : (ch.y - 2) / 4);

  fi=mb->Ryuv[1][0];
  if(cp[fi])
    sad[7][1][0] = BInterSAD(cpi,fi,1,0,ch);
  fi=mb->Ryuv[2][0];
  if(cp[fi])
    sad[7][2][0] = BInterSAD(cpi,fi,2,0,ch);
}

static void TQB (CP_INSTANCE *cpi, int mode, int fi, ogg_int32_t *iq, ogg_int16_t *q, mv_t mv, int plane){
  if ( cpi->frag_coded[fi] ) {
    ogg_int16_t buffer[64];
    ogg_int16_t *data = cpi->frag_dct[fi].data;
    int bi = cpi->frag_buffer_index[fi];
    int stride = cpi->stride[plane];
    int xqp = (plane && cpi->info.pixelformat != OC_PF_444);
    int yqp = (plane && cpi->info.pixelformat == OC_PF_420);
    unsigned char *frame_ptr = &cpi->frame[bi];
    unsigned char *lastrecon = ((mode == CODE_USING_GOLDEN || 
				 mode == CODE_GOLDEN_MV) ? 
				cpi->golden : cpi->lastrecon)+bi;
    unsigned char *thisrecon = cpi->recon+bi;
    int nonzero=63;

    /* motion comp */
    switch(mode){
    case CODE_INTER_PLUS_MV:
    case CODE_INTER_LAST_MV:
    case CODE_INTER_PRIOR_LAST:
    case CODE_GOLDEN_MV:
    case CODE_INTER_FOURMV:
      
      {    
	int mx = mvmap[xqp][mv.x+31];
	int my = mvmap[yqp][mv.y+31];
	int mx2 = mvmap2[xqp][mv.x+31];
	int my2 = mvmap2[yqp][mv.y+31];
	
	unsigned char *r1 = lastrecon + my * stride + mx;
	
	if(mx2 || my2){
	  unsigned char *r2 = r1 + my2 * stride + mx2;
	  dsp_copy8x8_half (cpi->dsp, r1, r2, thisrecon, stride);
	  dsp_sub8x8(cpi->dsp, frame_ptr, thisrecon, data, stride);
	}else{
	  dsp_copy8x8 (cpi->dsp, r1, thisrecon, stride);
	  dsp_sub8x8(cpi->dsp, frame_ptr, r1, data, stride);
	}
      }
      break;
      
    case CODE_USING_GOLDEN:
    case CODE_INTER_NO_MV:
      dsp_copy8x8 (cpi->dsp, lastrecon, thisrecon, stride);
      dsp_sub8x8(cpi->dsp, frame_ptr, lastrecon, data, stride);
      break;
    case CODE_INTRA:
      dsp_sub8x8_128(cpi->dsp, frame_ptr, data, stride);
      dsp_set8x8(cpi->dsp, 128, thisrecon, stride);
      break;
    }

    /* transform */
    dsp_fdct_short(cpi->dsp, data, buffer);
    
    /* collect rho metrics */
    
    /* quantize */
    quantize (cpi, iq, buffer, data);
    cpi->frag_dc[fi] = cpi->frag_dct[fi].data[0];

    /* reconstruct */
    while(!data[nonzero] && --nonzero);
    switch(nonzero){
    case 0:
      IDct1( data, q, buffer );
      break;
    case 1: case 2:
      dsp_IDct3(cpi->dsp, data, q, buffer );
      break;
    case 3:case 4:case 5:case 6:case 7:case 8: case 9:
      dsp_IDct10(cpi->dsp, data, q, buffer );
      break;
    default:
      dsp_IDctSlow(cpi->dsp, data, q, buffer );
    }
    
    dsp_recon8x8 (cpi->dsp, thisrecon, buffer, stride);
      
  }
}

static void TQMB ( CP_INSTANCE *cpi, macroblock_t *mb, int qi){
  int pf = cpi->info.pixelformat;
  int mode = mb->mode;
  int inter = (mode != CODE_INTRA);
  ogg_int32_t *iq = cpi->iquant_tables[inter][0][qi];
  ogg_int16_t  *q = cpi->quant_tables[inter][0][qi];
  int i;

  for(i=0;i<4;i++)
    TQB(cpi,mode,mb->Ryuv[0][i],iq,q,mb->mv[i],0);

  switch(pf){
  case OC_PF_420:
    if(mode == CODE_INTER_FOURMV){
      mv_t mv;
	  
      mv.x = mb->mv[0].x + mb->mv[1].x + mb->mv[2].x + mb->mv[3].x;
      mv.y = mb->mv[0].y + mb->mv[1].y + mb->mv[2].y + mb->mv[3].y;
      
      mv.x = ( mv.x >= 0 ? (mv.x + 2) / 4 : (mv.x - 2) / 4);
      mv.y = ( mv.y >= 0 ? (mv.y + 2) / 4 : (mv.y - 2) / 4);
      
      iq = cpi->iquant_tables[inter][1][qi];
      q = cpi->quant_tables[inter][1][qi];
      TQB(cpi,mode,mb->Ryuv[1][0],iq,q,mv,1);
      iq = cpi->iquant_tables[inter][2][qi];
      q = cpi->quant_tables[inter][2][qi];
      TQB(cpi,mode,mb->Ryuv[2][0],iq,q,mv,2);
    }else{ 
      iq = cpi->iquant_tables[inter][1][qi];
      q = cpi->quant_tables[inter][1][qi];
      TQB(cpi,mode,mb->Ryuv[1][0],iq,q,mb->mv[0],1);
      iq = cpi->iquant_tables[inter][2][qi];
      q = cpi->quant_tables[inter][2][qi];
      TQB(cpi,mode,mb->Ryuv[2][0],iq,q,mb->mv[0],2);
    }
    break;

  case OC_PF_422:
    if(mode == CODE_INTER_FOURMV){
      mv_t mvA;
      mv_t mvB;
	  
      mvA.x = mb->mv[0].x + mb->mv[1].x;
      mvA.y = mb->mv[0].y + mb->mv[1].y;
      mvA.x = ( mvA.x >= 0 ? (mvA.x + 1) / 2 : (mvA.x - 1) / 2);
      mvA.y = ( mvA.y >= 0 ? (mvA.y + 1) / 2 : (mvA.y - 1) / 2);
      mvB.x = mb->mv[0].x + mb->mv[1].x;
      mvB.y = mb->mv[0].y + mb->mv[1].y;
      mvB.x = ( mvB.x >= 0 ? (mvB.x + 1) / 2 : (mvB.x - 1) / 2);
      mvB.y = ( mvB.y >= 0 ? (mvB.y + 1) / 2 : (mvB.y - 1) / 2);
      
      iq = cpi->iquant_tables[inter][1][qi];
      q = cpi->quant_tables[inter][1][qi];
      TQB(cpi,mode,mb->Ryuv[1][0],iq,q,mvA,1);
      TQB(cpi,mode,mb->Ryuv[1][1],iq,q,mvB,1);

      iq = cpi->iquant_tables[inter][2][qi];
      q = cpi->quant_tables[inter][2][qi];
      TQB(cpi,mode,mb->Ryuv[2][0],iq,q,mvA,2);
      TQB(cpi,mode,mb->Ryuv[2][1],iq,q,mvB,2);

    }else{ 
      iq = cpi->iquant_tables[inter][1][qi];
      q = cpi->quant_tables[inter][1][qi];
      TQB(cpi,mode,mb->Ryuv[1][0],iq,q,mb->mv[0],1);
      TQB(cpi,mode,mb->Ryuv[1][1],iq,q,mb->mv[0],1);
      iq = cpi->iquant_tables[inter][2][qi];
      q = cpi->quant_tables[inter][2][qi];
      TQB(cpi,mode,mb->Ryuv[2][0],iq,q,mb->mv[0],2);
      TQB(cpi,mode,mb->Ryuv[2][1],iq,q,mb->mv[0],2);
    }
    break;

  case OC_PF_444:
    iq = cpi->iquant_tables[inter][1][qi];
    q = cpi->quant_tables[inter][1][qi];
    for(i=0;i<4;i++)
      TQB(cpi,mode,mb->Ryuv[1][i],iq,q,mb->mv[i],1);
    iq = cpi->iquant_tables[inter][2][qi];
    q = cpi->quant_tables[inter][2][qi];
    for(i=0;i<4;i++)
      TQB(cpi,mode,mb->Ryuv[2][i],iq,q,mb->mv[i],2);
    break;
  }
}

int PickModes(CP_INSTANCE *cpi, int recode){
  unsigned char qi = cpi->BaseQ; // temporary
  superblock_t *sb = cpi->super[0];
  superblock_t *sb_end = sb + cpi->super_n[0];
  int i,j;
  ogg_uint32_t interbits = 0;
  ogg_uint32_t intrabits = 0;
  mc_state mcenc;
  mv_t last_mv = {0,0};
  mv_t prior_mv = {0,0};
  unsigned char *cp = cpi->frag_coded;
#ifdef COLLECT_METRICS
  int sad[8][3][4];
#endif
  oc_mode_scheme_chooser_init(cpi);

  cpi->MVBits_0 = 0;
  cpi->MVBits_1 = 0;
 
  if(!recode)
    oc_mcenc_start(cpi, &mcenc); 
  
  /* Choose mvs, modes; must be done in Hilbert order */
  for(; sb<sb_end; sb++){
    for(j = 0; j<4; j++){ /* mode addressing is through Y plane, always 4 MB per SB */
      int mbi = sb->m[j];
      int cost[8] = {0,0,0,0, 0,0,0,0};
      int mb_mv_bits_0;
      int mb_gmv_bits_0;
      int mb_4mv_bits_0;
      int mb_4mv_bits_1;
      int mode,bi;
      int aerror;
      int gerror;
      int block_err[4];
      int flag4mv=0;

      macroblock_t *mb = &cpi->macro[mbi];

      if(!recode){
	/* Motion estimation */

	/* Move the motion vector predictors back a frame */
	memmove(mb->analysis_mv+1,mb->analysis_mv,2*sizeof(mb->analysis_mv[0]));
	
	/* basic 1MV search always done for all macroblocks, coded or not, keyframe or not */
	flag4mv = oc_mcenc_search(cpi, &mcenc, mbi, 0, mb->mv, &aerror, block_err);
	
	/* replace the block MVs for not-coded blocks with (0,0).*/   
	mb->coded = 0;
	for ( bi=0; bi<4; bi++ ){
	  int fi = mb->Ryuv[0][bi];
	  if(!cp[fi]) 
	    mb->mv[bi]=(mv_t){0,0};
	  else
	    mb->coded |= (1<<bi);
	}
	
	/* search golden frame */
	oc_mcenc_search(cpi, &mcenc, mbi, 1, NULL, &gerror, NULL);
	
      }

      if(cpi->FrameType == KEY_FRAME){
	mb->mode = CODE_INTRA;
      }else{
	
	/**************************************************************
           Find the block choice with the lowest estimated coding cost

           NOTE THAT if U or V is coded but no Y from a macro block then
           the mode will be CODE_INTER_NO_MV as this is the default
           state to which the mode data structure is initialised in
           encoder and decoder at the start of each frame. */
	
	/* block coding cost is estimated from correlated SAD metrics */
	/* At this point, all blocks that are in frame are still marked coded */

	cost[CODE_INTER_NO_MV] = cost_inter(cpi, qi, mbi, (mv_t){0,0}, CODE_INTER_NO_MV);
	cost[CODE_INTRA] = cost_intra(cpi, qi, mbi, &intrabits);
	cost[CODE_INTER_PLUS_MV] = cost_inter1mv(cpi, qi, mbi, 0, &mb_mv_bits_0);
	cost[CODE_INTER_LAST_MV] = cost_inter(cpi, qi, mbi, last_mv, CODE_INTER_LAST_MV);
	cost[CODE_INTER_PRIOR_LAST] = cost_inter(cpi, qi, mbi, prior_mv, CODE_INTER_PRIOR_LAST);
	cost[CODE_USING_GOLDEN] = cost_inter(cpi, qi, mbi, (mv_t){0,0},CODE_USING_GOLDEN);
	cost[CODE_GOLDEN_MV] = cost_inter1mv(cpi, qi, mbi, 1, &mb_gmv_bits_0);
	if(flag4mv)
	  cost[CODE_INTER_FOURMV] = cost_inter4mv(cpi, qi, mbi, &mb_4mv_bits_0, &mb_4mv_bits_1);
	else
	  cost[CODE_INTER_FOURMV] = 99999999;
	
	/* train this too... because the bit cost of an MV should be
	   considered in the context of LAST_MV and PRIOR_LAST. */
	cost[CODE_INTER_PLUS_MV] -= 384;
	
	/* the explicit MV modes (2,6,7) have not yet gone through
	   halfpel refinement. We choose the explicit mv mode that's
	   already furthest ahead on bits and refine only that one */
	if(flag4mv && cost[CODE_INTER_FOURMV]<cost[CODE_INTER_PLUS_MV] && cost[CODE_INTER_FOURMV]<cost[CODE_GOLDEN_MV]){
	  oc_mcenc_refine4mv(cpi, mbi, block_err);
	  cost[CODE_INTER_FOURMV] = cost_inter4mv(cpi, qi, mbi, &mb_4mv_bits_0, &mb_4mv_bits_1);
	}else if (cost[CODE_GOLDEN_MV]<cost[CODE_INTER_PLUS_MV]){
	  oc_mcenc_refine1mv(cpi, mbi, 1, gerror);
	  cost[CODE_GOLDEN_MV] = cost_inter1mv(cpi, qi, mbi, 1, &mb_gmv_bits_0);
	}else{
	  oc_mcenc_refine1mv(cpi, mbi, 0, aerror);
	  cost[CODE_INTER_PLUS_MV] = cost_inter1mv(cpi, qi, mbi, 0, &mb_mv_bits_0);
	  cost[CODE_INTER_PLUS_MV] -= 384;
	}

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
	
	interbits += cost[mb->mode];
      }

#ifdef COLLECT_METRICS
      MBSAD420(cpi, mbi, last_mv, prior_mv, sad);
      
      for(i=0;i<4;i++){
	for(j=0;j<3;j++){
	  fi=mb->Ryuv[j][i];
	  if(cp[fi])
	    cpi->frag_sad[fi]=sad[mb->mode][j][i];
	}
      }
#endif

      /* Transform, quantize, collect rho metrics */
      TQMB(cpi, mb, qi);

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

static void ModeMetricsGroup(CP_INSTANCE *cpi, int plane, int group, int huff, int eobcounts[3][64], int *actual_bits){
  int ti;
  int *stack = cpi->dct_eob_fi_stack[plane][group];
  int *tfi = cpi->dct_token_frag[plane][group];
  int tn = cpi->dct_token_count[plane][group];
  
  for(ti=0;ti<tn;ti++){
    int token = cpi->dct_token[plane][group][ti];
    int bits = cpi->HuffCodeLengthArray_VP3x[huff][token] + cpi->ExtraBitLengths_VP3x[token];
      
    if(token>DCT_REPEAT_RUN4_TOKEN){
      /* not an EOB run; this token belongs to a single fragment */
      int fi = tfi[ti];
      actual_bits[fi] += (bits<<OC_BIT_SCALE);
    }else{
      /* EOB run; its bits should be split up between all the fragments in the run */
      int run = parse_eob_run(token, cpi->dct_token_eb[plane][group][ti]);
      int fracbits = ((bits<<OC_BIT_SCALE) + (run>>1))/run;
      
      if(ti+1<tn){
	/* tokens follow EOB so it must be entirely ensconced within this plane/group */
	while(run--){
	  int fi = stack[eobcounts[plane][group]++];
	  actual_bits[fi]+=fracbits;
	}
      }else{
	/* EOB is the last token in this plane/group, so it may span into the next plane/group */
	int n = cpi->dct_eob_fi_count[plane][group];
	while(run){
	  while(eobcounts[plane][group] < n && run){
	    int fi = stack[eobcounts[plane][group]++];
	    actual_bits[fi]+=fracbits;
	    run--;
	  }
	  plane++;
	  if(plane>=3){
	    group++;
	    plane=0;
	  }
	  n = cpi->dct_eob_fi_count[plane][group];
	  stack = cpi->dct_eob_fi_stack[plane][group];
	 
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
  int eobcounts[3][64];
  int qi = cpi->BaseQ; /* temporary */
  int actual_bits[cpi->frag_total];
  memset(actual_bits,0,sizeof(actual_bits));
  memset(eobcounts,0,sizeof(eobcounts));

  if(mode_metrics==0){
    memset(mode_metric,0,sizeof(mode_metric));
    mode_metrics=1;
  }

  /* count bits for tokens */
  ModeMetricsGroup(cpi, 0, 0, huff[0], eobcounts, actual_bits);
  ModeMetricsGroup(cpi, 1, 0, huff[1], eobcounts, actual_bits);
  ModeMetricsGroup(cpi, 2, 0, huff[1], eobcounts, actual_bits);
  for(gi=1;gi<=AC_TABLE_2_THRESH;gi++){
    ModeMetricsGroup(cpi, 0, gi,  huff[2], eobcounts, actual_bits);
    ModeMetricsGroup(cpi, 1, gi,  huff[3], eobcounts, actual_bits);
    ModeMetricsGroup(cpi, 2, gi,  huff[3], eobcounts, actual_bits);
  }
  for(;gi<=AC_TABLE_3_THRESH;gi++){
    ModeMetricsGroup(cpi, 0, gi, huff[2]+AC_HUFF_CHOICES, eobcounts, actual_bits);
    ModeMetricsGroup(cpi, 1, gi, huff[3]+AC_HUFF_CHOICES, eobcounts, actual_bits);
    ModeMetricsGroup(cpi, 2, gi, huff[3]+AC_HUFF_CHOICES, eobcounts, actual_bits);
  }
  for(;gi<=AC_TABLE_4_THRESH;gi++){
    ModeMetricsGroup(cpi, 0, gi, huff[2]+AC_HUFF_CHOICES*2, eobcounts, actual_bits);
    ModeMetricsGroup(cpi, 1, gi, huff[3]+AC_HUFF_CHOICES*2, eobcounts, actual_bits);
    ModeMetricsGroup(cpi, 2, gi, huff[3]+AC_HUFF_CHOICES*2, eobcounts, actual_bits);
  }
  for(;gi<BLOCK_SIZE;gi++){
    ModeMetricsGroup(cpi, 0, gi, huff[2]+AC_HUFF_CHOICES*3, eobcounts, actual_bits);
    ModeMetricsGroup(cpi, 1, gi, huff[3]+AC_HUFF_CHOICES*3, eobcounts, actual_bits);
    ModeMetricsGroup(cpi, 2, gi, huff[3]+AC_HUFF_CHOICES*3, eobcounts, actual_bits);
  }

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
	  fprintf(stdout,"%12ldLL,",mode_metric[qi][plane][mode].bits[bin]);
	}
	fprintf(stdout," },\n");
	fprintf(stdout,"        { ");
	for(bin=0;bin<OC_SAD_BINS;bin++){
	  if(bin && !(bin&0x3))fprintf(stdout,"\n          ");
	  fprintf(stdout,"%12ldLL,",mode_metric[qi][plane][mode].frag[bin]);
	}
	fprintf(stdout," },\n");
	fprintf(stdout,"        { ");
	for(bin=0;bin<OC_SAD_BINS;bin++){
	  if(bin && !(bin&0x3))fprintf(stdout,"\n          ");
	  fprintf(stdout,"%12ldLL,",mode_metric[qi][plane][mode].sad[bin]);
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
