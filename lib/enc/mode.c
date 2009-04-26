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

/*Mode decision is done by exhaustively examining all potential choices.
  Obviously, doing the motion compensation, fDCT, tokenization, and then
   counting the bits each token uses is computationally expensive.
  Theora's EOB runs can also split the cost of these tokens across multiple
   fragments, and naturally we don't know what the optimal choice of Huffman
   codes will be until we know all the tokens we're going to encode in all the
   fragments.
  So we use a simple approach to estimating the bit cost of each mode based
   upon the SAD value of the residual.
  The mathematics behind the technique are outlined by Kim \cite{Kim03}, but
   the process is very simple.
  For each quality index and SAD value, we have a table containing the average
   number of bits needed to code a fragment.
  The SAD values are placed into a small number of bins (currently 24).
  TODO: The remaining portion is no longer current.
  The bit counts are obtained by examining actual encoded frames, with optimal
   Huffman codes selected and EOB bits appropriately divided among all the
   blocks they involve.
  A separate QIxSAD table is kept for each mode and color plane.
  It may be possible to combine many of these, but only experimentation
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
  }*/

/*Pointers to the list of bit lengths for the VLC codes used for each mode
   scheme.
  Schemes 0-6 use the same VLC, while scheme 7 uses a FLC.*/
static const unsigned char *OC_MODE_SCHEME_BITS[8]={
  ModeBitLengths,
  ModeBitLengths,
  ModeBitLengths,
  ModeBitLengths,
  ModeBitLengths,
  ModeBitLengths,
  ModeBitLengths,
  ModeBitLengthsD,
};

/*Initialize the mode scheme chooser.
  This need only be called once per encoder.
  This is probably the best place to describe the various schemes Theora uses
   to encode macro block modes.
  There are 8 possible schemes.
  Schemes 0-6 use a highly unbalanced Huffman code to code each of the modes.
  The same set of Huffman codes is used for each of these 7 schemes, but the
   mode assigned to each code varies.
  Schemes 1-6 have a fixed mapping from Huffman code to MB mode, while scheme 0
   writes a custom mapping to the bitstream before all the modes.
  Finally, scheme 7 just encodes each mode directly in 3 bits.*/
void oc_mode_scheme_chooser_init(oc_mode_scheme_chooser *_chooser){
  int si;
  _chooser->mode_ranks[0]=_chooser->scheme0_ranks;
  for(si=1;si<8;si++)_chooser->mode_ranks[si]=ModeSchemes[si-1];
}

/*Reset the mode scheme chooser.
  This needs to be called once for each frame, including the first.*/
static void oc_mode_scheme_chooser_reset(oc_mode_scheme_chooser *_chooser){
  int si;
  memset(_chooser->mode_counts,0,OC_NMODES*sizeof(*_chooser->mode_counts));
  /*Scheme 0 starts with 24 bits to store the mode list in.*/
  _chooser->scheme_bits[0]=24;
  memset(_chooser->scheme_bits+1,0,7*sizeof(*_chooser->scheme_bits));
  for(si=0;si<8;si++){
    /*Scheme 7 should always start first, and scheme 0 should always start
       last.*/
    _chooser->scheme_list[si]=7-si;
    _chooser->scheme0_list[si]=_chooser->scheme0_ranks[si]=si;
  }
}

/*This is the real purpose of this data structure: not actually selecting a
   mode scheme, but estimating the cost of coding a given mode given all the
   modes selected so far.
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
static int oc_mode_scheme_chooser_cost(oc_mode_scheme_chooser *_chooser,
 int _mode){
  int scheme0;
  int scheme1;
  int best_bits;
  int mode_bits;
  int si;
  int scheme_bits;
  scheme0=_chooser->scheme_list[0];
  scheme1=_chooser->scheme_list[1];
  best_bits=_chooser->scheme_bits[scheme0];
  mode_bits=OC_MODE_SCHEME_BITS[scheme0][_chooser->mode_ranks[scheme0][_mode]];
  /*Typical case: If the difference between the best scheme and the next best
     is greater than 6 bits, then adding just one mode cannot change which
     scheme we use.*/
  if(_chooser->scheme_bits[scheme1]-best_bits>6)return mode_bits;
  /*Otherwise, check to see if adding this mode selects a different scheme as
     the best.*/
  si=1;
  best_bits+=mode_bits;
  do{
    /*For any scheme except 0, we can just use the bit cost of the mode's rank
       in that scheme.*/
    if(scheme1!=0){
      scheme_bits=_chooser->scheme_bits[scheme1]+
       OC_MODE_SCHEME_BITS[scheme1][_chooser->mode_ranks[scheme1][_mode]];
    }
    else{
      int ri;
      /*For scheme 0, incrementing the mode count could potentially change the
         mode's rank.
        Find the index where the mode would be moved to in the optimal list,
         and use its bit cost instead of the one for the mode's current
         position in the list.*/
      /*We don't recompute scheme bits; this is computing opportunity cost, not
         an update.*/
      for(ri=_chooser->scheme0_ranks[_mode];ri>0&&
       _chooser->mode_counts[_mode]>=
       _chooser->mode_counts[_chooser->scheme0_list[ri-1]];ri--);
      scheme_bits=_chooser->scheme_bits[0]+ModeBitLengths[ri];
    }
    if(scheme_bits<best_bits)best_bits=scheme_bits;
    if(++si>=8)break;
    scheme1=_chooser->scheme_list[si];
  }
  while(_chooser->scheme_bits[scheme1]-_chooser->scheme_bits[scheme0]<=6);
  return best_bits-_chooser->scheme_bits[scheme0];
}

/*Incrementally update the mode counts and per-scheme bit counts and re-order
   the scheme lists once a mode has been selected.
  _mode: The mode that was chosen.*/
static void oc_mode_scheme_chooser_update(oc_mode_scheme_chooser *_chooser,
 int _mode){
  int ri;
  int si;
  _chooser->mode_counts[_mode]++;
  /*Re-order the scheme0 mode list if necessary.*/
  for(ri=_chooser->scheme0_ranks[_mode];ri>0;ri--){
    int pmode;
    pmode=_chooser->scheme0_list[ri-1];
    if(_chooser->mode_counts[pmode]>=_chooser->mode_counts[_mode])break;
    /*Reorder the mode ranking.*/
    _chooser->scheme0_ranks[pmode]++;
    _chooser->scheme0_list[ri]=pmode;
  }
  _chooser->scheme0_ranks[_mode]=ri;
  _chooser->scheme0_list[ri]=_mode;
  /*Now add the bit cost for the mode to each scheme.*/
  for(si=0;si<8;si++){
    _chooser->scheme_bits[si]+=
     OC_MODE_SCHEME_BITS[si][_chooser->mode_ranks[si][_mode]];
  }
  /*Finally, re-order the list of schemes.*/
  for(si=1;si<8;si++){
    int sj;
    int scheme0;
    int bits0;
    sj=si;
    scheme0=_chooser->scheme_list[si];
    bits0=_chooser->scheme_bits[scheme0];
    do{
      int scheme1;
      scheme1=_chooser->scheme_list[sj-1];
      if(bits0>=_chooser->scheme_bits[scheme1])break;
      _chooser->scheme_list[sj]=scheme1;
    }
    while(--sj>0);
    _chooser->scheme_list[sj]=scheme0;
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
  int ret = y[0] + ((ydel*xdel)>>OC_SAD_SHIFT);
  return (ret>0?ret:0);
}

static const signed char OC_MVMAP[2][64]={
  {     -15,-15,-14, -14,-13,-13,-12, -12,-11,-11,-10, -10, -9, -9, -8,
     -8, -7, -7, -6,  -6, -5, -5, -4,  -4, -3, -3, -2,  -2, -1, -1,  0,
      0,  0,  1,  1,   2,  2,  3,  3,   4,  4,  5,  5,   6,  6,  7,  7,
      8,  8,  9,  9,  10, 10, 11, 11,  12, 12, 13, 13,  14, 14, 15, 15 },
  {      -7, -7, -7,  -7, -6, -6, -6,  -6, -5, -5, -5,  -5, -4, -4, -4,
     -4, -3, -3, -3,  -3, -2, -2, -2,  -2, -1, -1, -1,  -1,  0,  0,  0,
      0,  0,  0,  0,   1,  1,  1,  1,   2,  2,  2,  2,   3,  3,  3,  3,
      4,  4,  4,  4,   5,  5,  5,  5,   6,  6,  6,  6,   7,  7,  7,  7 }
};

static const signed char OC_MVMAP2[2][63]={
  {   -1, 0,-1,  0,-1, 0,-1,  0,-1, 0,-1,  0,-1, 0,-1,
    0,-1, 0,-1,  0,-1, 0,-1,  0,-1, 0,-1,  0,-1, 0,-1,
    0, 1, 0, 1,  0, 1, 0, 1,  0, 1, 0, 1,  0, 1, 0, 1,
    0, 1, 0, 1,  0, 1, 0, 1,  0, 1, 0, 1,  0, 1, 0, 1 },
  {   -1,-1,-1,  0,-1,-1,-1,  0,-1,-1,-1,  0,-1,-1,-1,
    0,-1,-1,-1,  0,-1,-1,-1,  0,-1,-1,-1,  0,-1,-1,-1,
    0, 1, 1, 1,  0, 1, 1, 1,  0, 1, 1, 1,  0, 1, 1, 1,
    0, 1, 1, 1,  0, 1, 1, 1,  0, 1, 1, 1,  0, 1, 1, 1 }
};

int oc_get_mv_offsets(int _offsets[2],int _dx,int _dy,
 int _ystride,int _pli,int _pf){
  int qpx;
  int qpy;
  int mx;
  int my;
  int mx2;
  int my2;
  int offs;
  qpy=!(_pf&2)&&_pli;
  my=OC_MVMAP[qpy][_dy+31];
  my2=OC_MVMAP2[qpy][_dy+31];
  qpx=!(_pf&1)&&_pli;
  mx=OC_MVMAP[qpx][_dx+31];
  mx2=OC_MVMAP2[qpx][_dx+31];
  offs=my*_ystride+mx;
  if(mx2||my2){
    _offsets[1]=offs+my2*_ystride+mx2;
    _offsets[0]=offs;
    return 2;
  }
  _offsets[0]=offs;
  return 1;
}

static int BInterSAD(CP_INSTANCE *cpi,int _fi,int _dx,int _dy,
 int _pli,int _goldenp){
  unsigned char *b;
  unsigned char *r;
  int            stride;
  int            sad;
  b=cpi->frame+cpi->frag_buffer_index[_fi];
  r=(_goldenp?cpi->golden:cpi->lastrecon)+cpi->frag_buffer_index[_fi];
  stride=cpi->stride[_pli];
  sad=0;
  if(_dx||_dy){
    int offs[2];
    if(oc_get_mv_offsets(offs,_dx,_dy,
     cpi->stride[_pli],_pli,cpi->info.pixelformat)>1){
      sad=oc_enc_frag_sad2_thresh(cpi,b,r+offs[0],r+offs[1],stride,0x3FC0);
    }
    else sad=oc_enc_frag_sad(cpi,b,r+offs[0],stride);
  }
  /*TODO: Is this special case worth it?*/
  else sad=oc_enc_frag_sad(cpi,b,r,stride);
  /*TODO: <<2? Really? Why?*/
  if(_pli)return sad<<2;
  else return sad;
}

static int cost_intra(CP_INSTANCE *cpi,int _qi,int _mbi,
 ogg_uint32_t *_intrabits,int *_overhead){
  macroblock_t *mb;
  int           pli;
  int           bi;
  int           cost;
  int           overhead;
  mb=cpi->macro+_mbi;
  cost=0;
  for(pli=0;pli<3;pli++){
    for(bi=0;bi<4;bi++){
      int fi;
      fi=mb->Ryuv[pli][bi];
      if(fi<cpi->frag_total){
        int sad;
        sad=BIntraSAD(cpi,fi,pli);
        cost+=BINMAP(mode_rate[_qi][pli][1],sad);
      }
    }
  }
  *_intrabits+=cost;
  overhead=oc_mode_scheme_chooser_cost(&cpi->chooser,CODE_INTRA)<<OC_BIT_SCALE;
  *_overhead=overhead;
  return cost+overhead;
}

static int cost_inter(CP_INSTANCE *cpi,int _qi,int _mbi,int _dx, int _dy,
 int _mode,int *_overhead){
  macroblock_t *mb;
  int           goldenp;
  int           pli;
  int           bi;
  int           cost;
  int           overhead;
  mb=cpi->macro+_mbi;
  goldenp=_mode==CODE_USING_GOLDEN;
  cost=0;
  for(pli=0;pli<3;pli++){
    for(bi=0;bi<4;bi++){
      int fi;
      fi=mb->Ryuv[pli][bi];
      if(fi<cpi->frag_total){
        int sad;
        sad=BInterSAD(cpi,fi,_dx,_dy,pli,goldenp);
        cost+=BINMAP(mode_rate[_qi][pli][0],sad);
      }
    }
  }
  overhead=oc_mode_scheme_chooser_cost(&cpi->chooser,_mode)<<OC_BIT_SCALE;
  *_overhead=overhead;
  return cost+overhead;
}

static int cost_inter_nomv(CP_INSTANCE *cpi,int _qi,int _mbi,int *_overhead){
  macroblock_t *mb;
  int           pli;
  int           bi;
  int           cost;
  int           overhead;
  mb=cpi->macro+_mbi;
  cost=0;
  for(pli=0;pli<3;pli++){
    int stride;
    stride=cpi->stride[pli];
    for(bi=0;bi<4;bi++){
      int fi;
      fi=mb->Ryuv[pli][bi];
      if(fi<cpi->frag_total){
        int offs;
        int sad;
        offs=cpi->frag_buffer_index[fi];
        sad=oc_enc_frag_sad(cpi,cpi->frame+offs,cpi->lastrecon+offs,stride);
        if(pli)sad<<=2;
        cost+=BINMAP(mode_rate[_qi][pli][0],sad);
      }
    }
  }
  overhead=
   oc_mode_scheme_chooser_cost(&cpi->chooser,CODE_INTER_NO_MV)<<OC_BIT_SCALE;
  *_overhead=overhead;
  return cost+overhead;
}

static int cost_inter1mv(CP_INSTANCE *cpi,int _qi,int _mbi,int _goldenp,
 signed char *_mv,int *_bits0,int *_overhead){
  macroblock_t *mb;
  int           dx;
  int           dy;
  int           pli;
  int           bi;
  int           bits0;
  int           cost;
  int           overhead;
  mb=cpi->macro+_mbi;
  dx=_mv[0];
  dy=_mv[1];
  cost=0;
  for(pli=0;pli<3;pli++){
    for(bi=0;bi<4;bi++){
      int fi;
      fi=mb->Ryuv[pli][bi];
      if(fi<cpi->frag_total){
        int          sad;
        sad=BInterSAD(cpi,fi,dx,dy,pli,_goldenp);
        cost+=BINMAP(mode_rate[_qi][pli][0],sad);
      }
    }
  }
  bits0=MvBits[dx+MAX_MV_EXTENT]+MvBits[dy+MAX_MV_EXTENT];
  overhead=oc_mode_scheme_chooser_cost(&cpi->chooser,
   _goldenp?CODE_GOLDEN_MV:CODE_INTER_PLUS_MV)
   +OC_MINI(cpi->MVBits_0+bits0,cpi->MVBits_1+12)
   -OC_MINI(cpi->MVBits_0,cpi->MVBits_1)<<OC_BIT_SCALE;
  *_bits0=bits0;
  *_overhead=overhead;
  return cost+overhead;
}

static void oc_set_chroma_mvs00(oc_mv _cbmvs[4],oc_mv _lbmvs[4]){
  int dx;
  int dy;
  dx=_lbmvs[0][0]+_lbmvs[1][0]+_lbmvs[2][0]+_lbmvs[3][0];
  dy=_lbmvs[0][1]+_lbmvs[1][1]+_lbmvs[2][1]+_lbmvs[3][1];
  _cbmvs[0][0]=(signed char)OC_DIV_ROUND_POW2(dx,2,2);
  _cbmvs[0][1]=(signed char)OC_DIV_ROUND_POW2(dy,2,2);
}

static int cost_inter4mv(CP_INSTANCE *cpi,int _qi,int _mbi,
 oc_mv _mv[4],int *_bits0,int *_bits1,int *_overhead){
  macroblock_t *mb;
  int           pli;
  int           bi;
  int           cost;
  int           overhead;
  int           bits0;
  int           bits1;
  mb=cpi->macro+_mbi;
  cost=bits0=bits1=0;
  memcpy(mb->mv,_mv,sizeof(mb->mv));
  for(bi=0;bi<4;bi++){
    int fi;
    fi=mb->Ryuv[0][bi];
    if(fi<cpi->frag_total){
      int dx;
      int dy;
      int sad;
      dx=_mv[bi][0];
      dy=_mv[bi][1];
      sad=BInterSAD(cpi,fi,dx,dy,0,0);
      cost+=BINMAP(mode_rate[_qi][0][0],sad);
      bits0+=MvBits[dx+MAX_MV_EXTENT]+MvBits[dy+MAX_MV_EXTENT];
      bits1+=12;
    }
  }
  /*TODO: Use OC_SET_CHROMA_MVS_TABLE from decoder; 4:2:0 only for now.*/
  oc_set_chroma_mvs00(mb->cbmvs,_mv);
  for(pli=1;pli<3;pli++){
    for(bi=0;bi<4;bi++){
      int fi;
      fi=mb->Ryuv[pli][bi];
      if(fi<cpi->frag_total){
        int sad;
        sad=BInterSAD(cpi,fi,mb->cbmvs[bi][0],mb->cbmvs[bi][1],pli,0);
        cost+=BINMAP(mode_rate[_qi][pli][0],sad);
      }
    }
  }
  overhead=oc_mode_scheme_chooser_cost(&cpi->chooser,CODE_INTER_FOURMV)
   +OC_MINI(cpi->MVBits_0+bits0,cpi->MVBits_1+bits1)
   -OC_MINI(cpi->MVBits_0,cpi->MVBits_1)<<OC_BIT_SCALE;
  *_overhead=overhead;
  *_bits0=bits0;
  *_bits1=bits1;
  return cost+overhead;
}

#include "quant_lookup.h"

static void uncode_frag(CP_INSTANCE *cpi, int fi, int plane){
  int bi;
  int stride;
  bi=cpi->frag_buffer_index[fi];
  stride=cpi->stride[plane];
  cpi->frag_coded[fi]=0;
  oc_enc_frag_copy(cpi,cpi->recon+bi,cpi->lastrecon+bi,stride);
}

typedef struct{
  int uncoded_ac_ssd;
  int coded_ac_ssd;
  int ac_cost;
  int dc_flag;
} rd_metric_t;

typedef struct{
  int plane;
  int qi;
  ogg_int16_t re_q[2][3][64];
  oc_iquant *iq[2];
  quant_tables *qq[2];
  ogg_int32_t *mode_rate[2];
  int xqp;
  int yqp;
  int ssdmul;
} plane_state_t;

static void ps_setup_frame(CP_INSTANCE *cpi, plane_state_t *ps){
  int i,j,k;
  int qi = cpi->BaseQ; /* temporary */;

  ps->qi = qi;
  for(i=0;i<2;i++)
    for(j=0;j<3;j++)
      for(k=0;k<64;k++)
        ps->re_q[i][j][k]=cpi->quant_tables[i][j][k][qi];
}

static void ps_setup_plane(CP_INSTANCE *cpi, plane_state_t *ps, int plane){
  ps->plane = plane;
  ps->iq[0] = cpi->iquant_tables[0][plane][ps->qi];
  ps->iq[1] = cpi->iquant_tables[1][plane][ps->qi];
  ps->qq[0] = &(cpi->quant_tables[0][plane]);
  ps->qq[1] = &(cpi->quant_tables[1][plane]);
  ps->mode_rate[0] = mode_rate[ps->qi][plane][0];
  ps->mode_rate[1] = mode_rate[ps->qi][plane][1];
  ps->xqp = (plane && cpi->info.pixelformat != OC_PF_444);
  ps->yqp = (plane && cpi->info.pixelformat == OC_PF_420);
  ps->ssdmul = (ps->xqp+1)*(ps->yqp+1);
}

/* coding overhead is unscaled */
#include<stdio.h>
static int TQB (CP_INSTANCE *cpi,plane_state_t *ps,int mode,int fi,
 int _dx,int _dy,int coding_overhead,rd_metric_t *mo,long *rho_count,
 token_checkpoint_t **stack){
  const int keyframe = (cpi->FrameType == KEY_FRAME);
  const oc_iquant *iq = ps->iq[mode != CODE_INTRA];
  ogg_int16_t buffer[64]OC_ALIGN16;
  ogg_int16_t data[64]OC_ALIGN16;
  const int bi = cpi->frag_buffer_index[fi];
  const int stride = cpi->stride[ps->plane];
  const unsigned char *frame_ptr = &cpi->frame[bi];
  unsigned char *lastrecon = ((mode == CODE_USING_GOLDEN ||
                               mode == CODE_GOLDEN_MV) ?
                              cpi->golden : cpi->lastrecon)+bi;
  unsigned char *thisrecon = cpi->recon+bi;
  int nonzero=0;
  const ogg_int16_t *dequant = ps->re_q[mode != CODE_INTRA][ps->plane];
  int uncoded_ssd=0,coded_ssd=0;
  int uncoded_dc=0,coded_dc=0,dc_flag=0;
  int lambda = cpi->lambda;
  token_checkpoint_t *checkpoint=*stack;
  int mv_offs[2];
  int nmv_offs;
  int cost;
  int ci;
  int pi;

  cpi->frag_coded[fi]=1;

  /* by way of explanation: although the f_array coding overhead
     determination is accurate, it is greedy using very coarse-grained
     local information.  Allowing it to mildly discourage coding turns
     out to be beneficial, but it's not clear that allowing it to
     encourage coding through negative coding overhead deltas is
     useful.  For that reason, we disallow negative
     coding_overheads */
  if(coding_overhead<0)coding_overhead = 0;

  /* motion comp */
  switch(mode){
    case CODE_INTRA:{
      nmv_offs=0;
      oc_enc_frag_sub_128(cpi,data,frame_ptr,stride);
    }break;
    case CODE_USING_GOLDEN:
    case CODE_INTER_NO_MV:{
      nmv_offs=1;
      mv_offs[0]=0;
      oc_enc_frag_sub(cpi,data,frame_ptr,lastrecon,stride);
    }break;
    default:{
      nmv_offs=oc_get_mv_offsets(mv_offs,_dx,_dy,
       stride,ps->plane,cpi->info.pixelformat);
      if(nmv_offs>1){
        oc_enc_frag_copy2(cpi,thisrecon,
         lastrecon+mv_offs[0],lastrecon+mv_offs[1],stride);
        oc_enc_frag_sub(cpi,data,frame_ptr,thisrecon,stride);
      }
      else oc_enc_frag_sub(cpi,data,frame_ptr,lastrecon+mv_offs[0],stride);
    }break;
  }

#ifdef COLLECT_METRICS
  int sad=0;
  if(mode==CODE_INTRA){
    int acc=0;
    for(pi=0;pi<64;pi++)
      acc += data[pi];
    for(pi=0;pi<64;pi++)
      sad += abs((data[pi]<<6)-acc);
    sad >>=6;
  }else{
    for(pi=0;pi<64;pi++)
      sad += abs(data[pi]);

    if(ps->plane)sad<<=2;
  }

  cpi->frag_sad[fi]=sad;
#endif

  if(!keyframe){
    if(mode==CODE_INTER_NO_MV){
      for(pi=0;pi<64;pi++){
        uncoded_ssd += data[pi]*data[pi];
        uncoded_dc += data[pi];
      }
    }else{
      oc_enc_frag_sub(cpi,buffer,frame_ptr,cpi->lastrecon+bi,stride);
      for(pi=0;pi<64;pi++){
        uncoded_ssd += buffer[pi]*buffer[pi];
        uncoded_dc += buffer[pi];
      }
    }
    uncoded_ssd <<= 4; /* scale to match DCT domain */
  }

  /* transform */
  oc_enc_fdct8x8(cpi,buffer,data);

  /* collect rho metrics, quantize */
  {
    int          zzi;
#if 0
    quant_tables *qq = ps->qq[mode != CODE_INTRA];
#endif
    for(zzi=0;zzi<64;zzi++){
      int v;
      int val;
      int d;
      ci=dezigzag_index[zzi];
      v=buffer[ci];
      d=dequant[zzi];
      /* rho-domain distribution */
      val=v<<1;
      v=abs(val);
#if 0
      {
        ogg_int16_t *qqq = (*qq)[zzi];
        int pos;
        for(pos=64;pos>0;pos--)if(v<qqq[pos-1])break;
        rho_count[pos]++;
      }
#endif
      if(v>=d){
        int s;
        s=OC_SIGNMASK(val);
        /*The bias added here rounds ties away from zero, since token
           optimization can only decrease the magnitude of the quantized
           value.*/
        val+=(d+s)^s;
        /*Note the arithmetic right shift is not guaranteed by ANSI C.
          Hopefully no one still uses ones-complement architectures.*/
        val=((iq[zzi].m*(ogg_int32_t)val>>16)+val>>iq[zzi].l)-s;
        data[zzi]=OC_CLAMPI(-580,val,580);
        nonzero=zzi;
      }
      else data[zzi]=0;
    }
  }
  cpi->frag_dc[fi] = data[0];

  /* tokenize */
  cost = dct_tokenize_AC(cpi, fi, data, dequant, buffer, fi>=cpi->frag_n[0], stack);

  /*Reconstruct.*/
  oc_enc_dequant_idct8x8(cpi,buffer,data,
   nonzero+1,nonzero+1,dequant[0],(ogg_uint16_t *)dequant);
  if(mode==CODE_INTRA)oc_enc_frag_recon_intra(cpi,thisrecon,stride,buffer);
  else{
    oc_enc_frag_recon_inter(cpi,thisrecon,
     nmv_offs==1?lastrecon+mv_offs[0]:thisrecon,stride,buffer);
  }

  if(!keyframe){
    /* in retrospect, should we have skipped this block? */
    oc_enc_frag_sub(cpi,buffer,frame_ptr,thisrecon,stride);
    for(pi=0;pi<64;pi++){
      coded_ssd+=buffer[pi]*buffer[pi];
      coded_dc+=buffer[pi];
    }
    coded_ssd <<= 4; /* scale to match DCT domain */
    /* We actually only want the AC contribution to the SSDs */
    uncoded_ssd -= ((uncoded_dc*uncoded_dc)>>2);
    coded_ssd -= ((coded_dc*coded_dc)>>2);
    /* for undersampled planes */
    /*coded_ssd*=ps->ssdmul;*/
    /*uncoded_ssd*=ps->ssdmul;*/
    mo->uncoded_ac_ssd+=uncoded_ssd;

    /* DC is a special case; if there's more than a full-quantizer
       improvement in the effective DC component, always force-code
       the block */
    if( abs(uncoded_dc)-abs(coded_dc) > (dequant[0]<<1)){
      mo->dc_flag = dc_flag = 1;
    }

    if(!dc_flag && uncoded_ssd <= coded_ssd+(coding_overhead+cost)*lambda){
      /* Hm, not worth it.  roll back */
      tokenlog_rollback(cpi, checkpoint, (*stack)-checkpoint);
      *stack = checkpoint;
      uncode_frag(cpi,fi,ps->plane);

      mo->coded_ac_ssd+=uncoded_ssd;
      //fprintf(stderr,"skip(%d:%d)",coding_overhead,cost);

      return 0;
    }else{

      //fprintf(stderr,"*****(%d:%d)",coding_overhead,cost);

      mo->coded_ac_ssd+=coded_ssd;
      mo->ac_cost+=cost;

    }
  }

  //for(i=0;i<64;i++)
  //if(data[i]!=0)cpi->rho_postop++;

  return 1;
}

static int macroblock_phase_Y[4][4] = {{0,1,3,2},{0,2,3,1},{0,2,3,1},{3,2,0,1}};

/* mode_overhead is scaled by << OC_BIT_SCALE */
static int TQMB_Y(CP_INSTANCE *cpi,macroblock_t *mb,int mb_phase,
 plane_state_t *ps,long *rc,int mode_overhead,int *mb_mv_bits_0,fr_state_t *fr){

  int full_checkpoint = cpi->fr_full_count;
  int partial_checkpoint = cpi->fr_partial_count;
  int block_checkpoint = cpi->fr_block_count;
  fr_state_t fr_checkpoint = *fr;
  unsigned char *cp=cpi->frag_coded;
  int mode = mb->mode;
  int coded = 0;
  int i;
  token_checkpoint_t stack[64*5]; /* worst case token usage for 4 fragments*/
  token_checkpoint_t *stackptr = stack;
  //int rho_check = cpi->rho_postop;

  rd_metric_t mo;
  memset(&mo,0,sizeof(mo));

  for(i=0;i<4;i++){
    /* Blocks must be handled in Hilbert order which is defined by MB
       position within the SB.  And, of course, the MVs have to be in
       raster order just to make it more difficult. */
    int bi = macroblock_phase_Y[mb_phase][i];
    int fi = mb->Ryuv[0][bi];

    if(TQB(cpi,ps,mode,fi,mb->mv[bi][0],mb->mv[bi][1],
     fr_cost1(fr),&mo,rc,&stackptr)){
      fr_codeblock(cpi,fr);
      coded++;
    }
    else fr_skipblock(cpi,fr);
  }


  if(cpi->FrameType != KEY_FRAME){
    int bi;
    if(coded && !mo.dc_flag){
      /* block by block, still coding the MB.  Now consider the
         macroblock coding cost as a whole (mode and MV) */
      int codecost = mo.ac_cost+fr_cost4(&fr_checkpoint,fr)+(mode_overhead>>OC_BIT_SCALE);
      if(mo.uncoded_ac_ssd <= mo.coded_ac_ssd+cpi->lambda*codecost){

        /* taking macroblock overhead into account, it is not worth coding this MB */
        tokenlog_rollback(cpi, stack, stackptr-stack);
        memcpy(fr,&fr_checkpoint,sizeof(fr_checkpoint));
        cpi->fr_full_count = full_checkpoint;
        cpi->fr_partial_count = partial_checkpoint;
        cpi->fr_block_count = block_checkpoint;
        /*cpi->rho_postop = rho_check;*/

        for(i=0;i<4;i++){
          int fi = mb->Ryuv[0][i];
          if(cp[fi])
            uncode_frag(cpi,fi,0);
          fr_skipblock(cpi,fr);
        }
        coded=0;

      }
    }

    if(coded==0){
      mb->mode = CODE_INTER_NO_MV; /* No luma blocks coded, mode is forced */
      mb->coded = 0;
      memset(mb->mv,0,sizeof(mb->mv));
      memset(mb->cbmvs,0,sizeof(mb->cbmvs));
      return 0;
    }
    /*Assume that a 1mv with a single coded block is always cheaper than a 4mv
       with a single coded block.
      This may not be strictly true: a 4MV computes chroma MVs using (0,0) for
       skipped blocks, while a 1MV does not.*/
    else if(coded==1&&mode==CODE_INTER_FOURMV){
      int dx;
      int dy;
      mode=mb->mode=CODE_INTER_PLUS_MV;
      for(bi=0;!cp[mb->Ryuv[0][bi]];bi++);
      dx=mb->mv[bi][0];
      dy=mb->mv[bi][1];
      mb->cbmvs[0][0]=mb->cbmvs[1][0]=mb->cbmvs[2][0]=mb->cbmvs[3][0]=
       mb->mv[0][0]=mb->mv[1][0]=mb->mv[2][0]=mb->mv[3][0]=(signed char)dx;
      mb->cbmvs[0][1]=mb->cbmvs[1][1]=mb->cbmvs[2][1]=mb->cbmvs[3][1]=
       mb->mv[0][1]=mb->mv[1][1]=mb->mv[2][1]=mb->mv[3][1]=(signed char)dy;
      *mb_mv_bits_0=MvBits[dx+MAX_MV_EXTENT]+MvBits[dy+MAX_MV_EXTENT];
    }
    mb->coded=0;
    for(bi=0;bi<4;bi++)mb->coded|=cp[mb->Ryuv[0][bi]]<<bi;
  }

  /* Commit tokenization */
  tokenlog_commit(cpi, stack, stackptr-stack);

  return coded;
}

static const unsigned char OC_MACROBLOCK_PHASE[16]={
  0,1,3,2,0,2,3,1,0,2,3,1,3,2,0,1
};

static int TQSB_UV ( CP_INSTANCE *cpi, superblock_t *sb, plane_state_t *ps, long *rc, fr_state_t *fr){
  int pf = cpi->info.pixelformat;
  int i;
  int coded = 0;
  rd_metric_t mo;
  token_checkpoint_t stack[64*2]; /* worst case token usage for 1 fragment*/
  memset(&mo,0,sizeof(mo));

  for(i=0;i<16;i++){
    int fi = sb->f[i];

    if(fi<cpi->frag_total){
      token_checkpoint_t *stackptr;
      macroblock_t       *mb;
      int                 bi;
      stackptr = stack;
      mb=cpi->macro+sb->m[i];
      bi=OC_MACROBLOCK_PHASE[i]&pf;
      if(TQB(cpi,ps,mb->mode,fi,mb->cbmvs[bi][0],mb->cbmvs[bi][1],
       fr_cost1(fr),&mo,rc,&stackptr)){
        fr_codeblock(cpi,fr);
        tokenlog_commit(cpi, stack, stackptr-stack);
        coded++;
      }else{
        fr_skipblock(cpi,fr);
      }
    }
  }

  return coded;
}

int PickModes(CP_INSTANCE *cpi, int recode){
  int qi;
  superblock_t *sb;
  superblock_t *sb_end;
  int i,j;
  ogg_uint32_t interbits;
  ogg_uint32_t intrabits;
  mc_state mcenc;
  oc_mv last_mv;
  oc_mv prior_mv;
  long rho_count[65];
  plane_state_t ps;
  fr_state_t fr;
  interbits=intrabits=0;
  last_mv[0]=last_mv[1]=prior_mv[0]=prior_mv[1]=0;
  oc_mode_scheme_chooser_reset(&cpi->chooser);
  ps_setup_frame(cpi,&ps);
  ps_setup_plane(cpi,&ps,0);
  fr_clear(cpi,&fr);
  cpi->fr_full_count=0;
  cpi->fr_partial_count=0;
  cpi->fr_block_count=0;

  //cpi->rho_postop=0;

  memset(rho_count,0,sizeof(rho_count));
  cpi->MVBits_0 = 0;
  cpi->MVBits_1 = 0;

  if(!recode)
    oc_mcenc_start(cpi, &mcenc);

  dct_tokenize_init(cpi);

  /* Choose mvs, modes; must be done in Hilbert order */
  /* quantize and code Luma */
  qi=cpi->BaseQ;
  sb = cpi->super[0];
  sb_end = sb + cpi->super_n[0];
  for(; sb<sb_end; sb++){

    for(j = 0; j<4; j++){ /* mode addressing is through Y plane, always 4 MB per SB */
      int mbi = sb->m[j];

      int cost[8] = {0,0,0,0, 0,0,0,0};
      int overhead[8] = {0,0,0,0, 0,0,0,0};
      int mb_mv_bits_0;
      int mb_gmv_bits_0;
      int mb_4mv_bits_0;
      int mb_4mv_bits_1;
      int mode;
      int aerror;
      int gerror;
      int block_err[4];

      macroblock_t *mb = &cpi->macro[mbi];

      if(mbi >= cpi->macro_total) continue;

      if(!recode){
        /* Motion estimation */

        /* Move the motion vector predictors back a frame */
        memmove(mb->analysis_mv+1,mb->analysis_mv,2*sizeof(mb->analysis_mv[0]));

        /* basic 1MV search always done for all macroblocks, coded or not, keyframe or not */
        oc_mcenc_search(cpi, &mcenc, mbi, 0, mb->block_mv, &aerror, block_err);

        /* search golden frame */
        oc_mcenc_search(cpi, &mcenc, mbi, 1, NULL, &gerror, NULL);

      }else{
        aerror = mb->aerror;
        gerror = mb->gerror;
      }

      if(cpi->FrameType == KEY_FRAME){
        mb->mode = CODE_INTRA;
        /* Transform, quantize, collect rho metrics */
        TQMB_Y(cpi, mb, j, &ps, rho_count, 0, NULL, &fr);

      }else{

        /**************************************************************
           Find the block choice with the lowest estimated coding cost

           NOTE THAT if U or V is coded but no Y from a macro block then
           the mode will be CODE_INTER_NO_MV as this is the default
           state to which the mode data structure is initialised in
           encoder and decoder at the start of each frame. */

        /* block coding cost is estimated from correlated SAD metrics */
        /* At this point, all blocks that are in frame are still marked coded */
        if(!recode){
          memcpy(mb->unref_mv,mb->analysis_mv[0],sizeof(mb->unref_mv));
          mb->refined=0;
        }
        cost[CODE_INTER_NO_MV] =
          cost_inter_nomv(cpi, qi, mbi, &overhead[CODE_INTER_NO_MV]);
        cost[CODE_INTRA] =
          cost_intra(cpi, qi, mbi, &intrabits, &overhead[CODE_INTRA]);
        cost[CODE_INTER_PLUS_MV] =
          cost_inter1mv(cpi,qi,mbi,0,mb->unref_mv[0],
           &mb_mv_bits_0,&overhead[CODE_INTER_PLUS_MV]);
        cost[CODE_INTER_LAST_MV] =
          cost_inter(cpi, qi, mbi, last_mv[0], last_mv[1], CODE_INTER_LAST_MV, &overhead[CODE_INTER_LAST_MV]);
        cost[CODE_INTER_PRIOR_LAST] =
          cost_inter(cpi, qi, mbi, prior_mv[0], prior_mv[1], CODE_INTER_PRIOR_LAST, &overhead[CODE_INTER_PRIOR_LAST]);
        cost[CODE_USING_GOLDEN] =
          cost_inter(cpi, qi, mbi, 0, 0, CODE_USING_GOLDEN, &overhead[CODE_USING_GOLDEN]);
        cost[CODE_GOLDEN_MV] =
          cost_inter1mv(cpi,qi,mbi,1,mb->unref_mv[1],
           &mb_gmv_bits_0, &overhead[CODE_GOLDEN_MV]);
        cost[CODE_INTER_FOURMV] =
          cost_inter4mv(cpi, qi, mbi, mb->block_mv, &mb_4mv_bits_0, &mb_4mv_bits_1, &overhead[CODE_INTER_FOURMV]);

        /*The explicit MV modes (2,6,7) have not yet gone through halfpel
           refinement.
          We choose the explicit MV mode that's already furthest ahead on bits
           and refine only that one.
          We have to be careful to remember which ones we've refined so that
           we don't refine it again if we re-encode this frame.*/
        if(cost[CODE_INTER_FOURMV]<cost[CODE_INTER_PLUS_MV] && cost[CODE_INTER_FOURMV]<cost[CODE_GOLDEN_MV]){
          if(!(mb->refined&0x80)){
            oc_mcenc_refine4mv(cpi, mbi, block_err);
            mb->refined|=0x80;
          }
          cost[CODE_INTER_FOURMV] =
            cost_inter4mv(cpi, qi, mbi, mb->ref_mv,&mb_4mv_bits_0, &mb_4mv_bits_1, &overhead[CODE_INTER_FOURMV]);
        }else if (cost[CODE_GOLDEN_MV]<cost[CODE_INTER_PLUS_MV]-384){
          if(!(mb->refined&0x40)){
            oc_mcenc_refine1mv(cpi,mbi,1,gerror);
            mb->refined|=0x40;
          }
          cost[CODE_GOLDEN_MV] =
            cost_inter1mv(cpi,qi,mbi,1,mb->analysis_mv[0][1],
             &mb_gmv_bits_0,&overhead[CODE_GOLDEN_MV]);
        }
        if(!(mb->refined&0x04)){
          oc_mcenc_refine1mv(cpi,mbi,0,aerror);
          mb->refined|=0x04;
        }
        cost[CODE_INTER_PLUS_MV] =
          cost_inter1mv(cpi,qi,mbi,0,mb->analysis_mv[0][0],
           &mb_mv_bits_0, &overhead[CODE_INTER_PLUS_MV]);

        /* Finally, pick the mode with the cheapest estimated bit cost.*/
        /* prefer CODE_INTER_PLUS_MV, but not over LAST and LAST2 */
        mode=0;
        if(cost[1] < cost[0])mode=1;
        if(cost[3] < cost[mode])mode=3;
        if(cost[4] < cost[mode])mode=4;
        if(cost[5] < cost[mode])mode=5;
        if(cost[6] < cost[mode])mode=6;
        if(cost[7] < cost[mode])mode=7;
        if(mode == CODE_INTER_LAST_MV || mode == CODE_INTER_PRIOR_LAST){
          if(cost[2] < cost[mode])mode=2;
        }else{
          if(cost[2]-384 < cost[mode])mode=2;
        }
        /*If we picked something other than 4MV, propagate the MV to the
           blocks.*/
        if(mode!=CODE_INTER_FOURMV){
          int dx;
          int dy;
          switch(mode){
            case CODE_INTER_PLUS_MV:{
              dx=mb->analysis_mv[0][0][0];
              dy=mb->analysis_mv[0][0][1];
            }break;
            case CODE_INTER_LAST_MV:{
              dx=last_mv[0];
              dy=last_mv[1];
            }break;
            case CODE_INTER_PRIOR_LAST:{
              dx=prior_mv[0];
              dy=prior_mv[1];
            }break;
            case CODE_GOLDEN_MV:{
              dx=mb->analysis_mv[0][1][0];
              dy=mb->analysis_mv[0][1][1];
            }break;
            default:dx=dy=0;break;
          }
          mb->cbmvs[0][0]=mb->cbmvs[1][0]=mb->cbmvs[2][0]=mb->cbmvs[3][0]=
           mb->mv[0][0]=mb->mv[1][0]=mb->mv[2][0]=mb->mv[3][0]=(signed char)dx;
          mb->cbmvs[0][1]=mb->cbmvs[1][1]=mb->cbmvs[2][1]=mb->cbmvs[3][1]=
           mb->mv[0][1]=mb->mv[1][1]=mb->mv[2][1]=mb->mv[3][1]=(signed char)dy;
        }
        mb->mode=mode;
        /* Transform, quantize, collect rho metrics */
        if(TQMB_Y(cpi,mb,j,&ps,rho_count,overhead[mode],&mb_mv_bits_0,&fr)){
          switch(mb->mode){
            case CODE_INTER_PLUS_MV:{
              prior_mv[0]=last_mv[0];
              prior_mv[1]=last_mv[1];
              /*mb->mv[0] is not the same as analysis_mv[0][0] if we're
                 backing out from a 4MV.*/
              last_mv[0]=mb->mv[0][0];
              last_mv[1]=mb->mv[0][1];
              cpi->MVBits_0+=mb_mv_bits_0;
              cpi->MVBits_1+=12;
            }break;
            case CODE_INTER_PRIOR_LAST:{
              oc_mv temp;
              temp[0]=prior_mv[0];
              temp[1]=prior_mv[1];
              prior_mv[0]=last_mv[0];
              prior_mv[1]=last_mv[1];
              last_mv[0]=temp[0];
              last_mv[1]=temp[1];
            }break;
            case CODE_GOLDEN_MV:{
              cpi->MVBits_0 += mb_gmv_bits_0;
              cpi->MVBits_1 += 12;
            }break;
            case CODE_INTER_FOURMV:{
              int bi;
              prior_mv[0]=last_mv[0];
              prior_mv[1]=last_mv[1];
              for(bi=0;bi<4;bi++){
                if(mb->coded&(1<<bi)){
                  cpi->MVBits_0+=MvBits[mb->mv[bi][0]+MAX_MV_EXTENT]
                   +MvBits[mb->mv[bi][1]+MAX_MV_EXTENT];
                  cpi->MVBits_1+=12;
                  last_mv[0]=mb->mv[bi][0];
                  last_mv[1]=mb->mv[bi][1];
                }
                /*Replace the block MVs for not-coded blocks with (0,0).*/
                else mb->mv[bi][0]=mb->mv[bi][1]=0;
              }
              if(mb->coded!=0xF){
                /*TODO: Use OC_SET_CHROMA_MVS_TABLE from decoder; 4:2:0 only
                   for now.*/
                oc_set_chroma_mvs00(mb->cbmvs,mb->mv);
              }
            }break;
            default:break;
          }
          oc_mode_scheme_chooser_update(&cpi->chooser,mb->mode);
          interbits+=cost[mb->mode];
        }
      }
    }
    fr_finishsb(cpi,&fr);
  }

  dct_tokenize_mark_ac_chroma(cpi);

  /* code chroma U */
  sb = cpi->super[1];
  sb_end = sb + cpi->super_n[1];
  ps_setup_plane(cpi,&ps,1);
  for(; sb<sb_end; sb++){
    TQSB_UV(cpi, sb, &ps, rho_count, &fr);
    fr_finishsb(cpi,&fr);
  }

  /* code chroma V */
  sb = cpi->super[2];
  sb_end = sb + cpi->super_n[2];
  ps_setup_plane(cpi,&ps,2);
  for(; sb<sb_end; sb++){
    TQSB_UV(cpi, sb, &ps, rho_count, &fr);
    fr_finishsb(cpi,&fr);
  }

  for(i=1;i<65;i++)
  rho_count[i]+=rho_count[i-1];

  memcpy(cpi->rho_count,rho_count,sizeof(rho_count));
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
      fr_write(cpi,&fr);
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


static void ModeMetricsGroup(CP_INSTANCE *cpi, int group, int huffY, int huffC, int eobcounts[64], int *actual_bits){
  int ti=0;
  int *stack = cpi->dct_eob_fi_stack[group];
  int *tfi = cpi->dct_token_frag[group];
  int ty = cpi->dct_token_ycount[group];
  int tn = cpi->dct_token_count[group];

  for(ti=0;ti<tn;ti++){
    int token = cpi->dct_token[group][ti];
    int bits = cpi->huff_codes[(ti<ty ? huffY : huffC)][token].nbits + OC_DCT_TOKEN_EXTRA_BITS[token];

    if(token>DCT_REPEAT_RUN4_TOKEN){
      /* not an EOB run; this token belongs to a single fragment */
      int fi = tfi[ti];
      actual_bits[fi] += (bits<<OC_BIT_SCALE);
    }else{

      int run = parse_eob_run(token, cpi->dct_token_eb[group][ti]);
      int fi = stack[eobcounts[group]];
      actual_bits[fi]+=(bits<<OC_BIT_SCALE);

      if(ti+1<tn){
        /* tokens follow EOB so it must be entirely ensconced within this plane/group */
        eobcounts[group]+=run;
      }else{
        /* EOB is the last token in this plane/group, so it may span into the next plane/group */
        int n = cpi->dct_eob_fi_count[group];
        while(run){
          int rem = n - eobcounts[group];
          if(rem>run)rem=run;

          eobcounts[group]+=rem;
          run -= rem;
          if(run){
            group++;
            n = cpi->dct_eob_fi_count[group];
            stack = cpi->dct_eob_fi_stack[group];
          }
        }
      }
    }
  }
}

void ModeMetrics(CP_INSTANCE *cpi){
  int interp = (cpi->FrameType!=KEY_FRAME);
  int huff[4];
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
  huff[0] = cpi->huffchoice[interp][0][0];
  huff[1] = cpi->huffchoice[interp][0][1];
  huff[2] = cpi->huffchoice[interp][1][0];
  huff[3] = cpi->huffchoice[interp][1][1];

  memset(cpi->dist_dist,0,sizeof(cpi->dist_dist));
  memset(cpi->dist_bits,0,sizeof(cpi->dist_bits));

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
  for(fi=0;fi<v;fi++)if(cp[fi]){
    int mbi = mp[fi];
    macroblock_t *mb = &cpi->macro[mbi];
    int mode = mb->mode;
    int plane = (fi<y ? 0 : (fi<u ? 1 : 2));
    int bin = BIN(sp[fi]);
    mode_metric[qi][plane][mode==CODE_INTRA].frag[bin]++;
    mode_metric[qi][plane][mode==CODE_INTRA].sad[bin] += sp[fi];
    mode_metric[qi][plane][mode==CODE_INTRA].bits[bin] += actual_bits[fi];
    if(0){
      int bi = cpi->frag_buffer_index[fi];
      unsigned char *frame = cpi->frame+bi;
      unsigned char *recon = cpi->lastrecon+bi;
      int stride = cpi->stride[plane];
      int lssd=0;
      int xi,yi;
      for(yi=0;yi<8;yi++){
        for(xi=0;xi<8;xi++)
          lssd += (frame[xi]-recon[xi])*(frame[xi]-recon[xi]);
        frame+=stride;
        recon+=stride;
      }
      cpi->dist_dist[plane][mode] += lssd;
      cpi->dist_bits[plane][mode] += actual_bits[fi];
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
