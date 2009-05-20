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
#include "modedec.h"
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

typedef struct oc_mode_choice oc_mode_choice;

struct oc_mode_choice{
  unsigned cost;
  unsigned ssd;
  unsigned rate;
  unsigned overhead;
};

static void oc_mode_dct_cost_accum(oc_mode_choice *_mode,
 int _qi,int _pli,int _qti,int _sad){
  int      bin;
  int      dx;
  int      y0;
  int      z0;
  int      dy;
  int      dz;
  unsigned rmse;
  bin=OC_MINI(_sad>>OC_SAD_SHIFT,OC_SAD_BINS-2);
  dx=_sad-(bin<<OC_SAD_SHIFT);
  y0=OC_MODE_RD[_qi][_pli][_qti][bin].rate;
  z0=OC_MODE_RD[_qi][_pli][_qti][bin].rmse;
  dy=OC_MODE_RD[_qi][_pli][_qti][bin+1].rate-y0;
  dz=OC_MODE_RD[_qi][_pli][_qti][bin+1].rmse-z0;
  _mode->rate+=OC_MAXI(y0+(dy*dx>>OC_SAD_SHIFT),0);
  rmse=OC_MAXI(z0+(dz*dx>>OC_SAD_SHIFT),0);
  _mode->ssd+=rmse*rmse>>2*OC_RMSE_SCALE-OC_BIT_SCALE;
}

static void oc_mode_set_cost(oc_mode_choice *_mode,int _lambda){
 _mode->cost=_mode->ssd+(_mode->rate+_mode->overhead)*_lambda;
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

static int BIntraSAD(CP_INSTANCE *cpi, int fi, int plane){
  int satd;
  satd=oc_enc_frag_intra_satd(cpi,
   cpi->frame+cpi->frag_buffer_index[fi],cpi->stride[plane]);
  if(plane)satd<<=2;
  return satd;
}

static int BInterSAD(CP_INSTANCE *cpi,int _fi,int _dx,int _dy,
 int _pli,int _goldenp){
  unsigned char *b;
  unsigned char *r;
  int            offs[2];
  int            stride;
  int            sad;
  b=cpi->frame+cpi->frag_buffer_index[_fi];
  r=(_goldenp?cpi->golden:cpi->lastrecon)+cpi->frag_buffer_index[_fi];
  stride=cpi->stride[_pli];
  sad=0;
  if(oc_get_mv_offsets(offs,_dx,_dy,
   cpi->stride[_pli],_pli,cpi->info.pixelformat)>1){
    sad=oc_enc_frag_satd2_thresh(cpi,b,r+offs[0],r+offs[1],stride,0xFF000);
  }
  else sad=oc_enc_frag_satd_thresh(cpi,b,r+offs[0],stride,0xFF000);
  /*TODO: <<2? Really? Why?*/
  if(_pli)return sad<<2;
  else return sad;
}

static void oc_cost_intra(CP_INSTANCE *cpi,oc_mode_choice *_mode,
 int _mbi,int _qi){
  macroblock_t *mb;
  int           pli;
  int           bi;
  mb=cpi->macro+_mbi;
  _mode->rate=_mode->ssd=0;
  for(pli=0;pli<3;pli++){
    for(bi=0;bi<4;bi++){
      int fi;
      fi=mb->Ryuv[pli][bi];
      if(fi<cpi->frag_total){
        oc_mode_dct_cost_accum(_mode,_qi,pli,0,BIntraSAD(cpi,fi,pli));
      }
    }
  }
  _mode->overhead=
   oc_mode_scheme_chooser_cost(&cpi->chooser,CODE_INTRA)<<OC_BIT_SCALE;
  oc_mode_set_cost(_mode,cpi->lambda);
}

static void oc_cost_inter(CP_INSTANCE *cpi,oc_mode_choice *_mode,int _mbi,
 int _modei,const signed char *_mv,int _qi){
  macroblock_t *mb;
  int           goldenp;
  int           pli;
  int           bi;
  int           dx;
  int           dy;
  goldenp=OC_FRAME_FOR_MODE[_modei]==OC_FRAME_GOLD;
  mb=cpi->macro+_mbi;
  _mode->rate=_mode->ssd=0;
  dx=_mv[0];
  dy=_mv[1];
  for(pli=0;pli<3;pli++){
    for(bi=0;bi<4;bi++){
      int fi;
      fi=mb->Ryuv[pli][bi];
      if(fi<cpi->frag_total){
        oc_mode_dct_cost_accum(_mode,_qi,pli,1,
         BInterSAD(cpi,fi,dx,dy,pli,goldenp));
      }
    }
  }
  _mode->overhead=
   oc_mode_scheme_chooser_cost(&cpi->chooser,_modei)<<OC_BIT_SCALE;
  oc_mode_set_cost(_mode,cpi->lambda);
}

static void oc_cost_inter_nomv(CP_INSTANCE *cpi,oc_mode_choice *_mode,int _mbi,
 int _modei,int _qi){
  const unsigned char *ref;
  macroblock_t        *mb;
  int                  pli;
  int                  bi;
  ref=_modei==CODE_INTER_NO_MV?cpi->lastrecon:cpi->golden;
  mb=cpi->macro+_mbi;
  _mode->rate=_mode->ssd=0;
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
        sad=oc_enc_frag_satd_thresh(cpi,
         cpi->frame+offs,ref+offs,stride,0xFF000);
        if(pli)sad<<=2;
        oc_mode_dct_cost_accum(_mode,_qi,pli,1,sad);
      }
    }
  }
  _mode->overhead=
   oc_mode_scheme_chooser_cost(&cpi->chooser,_modei)<<OC_BIT_SCALE;
  oc_mode_set_cost(_mode,cpi->lambda);
}

static int oc_cost_inter1mv(CP_INSTANCE *cpi,oc_mode_choice *_mode,int _mbi,
 int _modei,const signed char *_mv,int _qi){
  int bits0;
  oc_cost_inter(cpi,_mode,_mbi,_modei,_mv,_qi);
  bits0=MvBits[_mv[0]+MAX_MV_EXTENT]+MvBits[_mv[1]+MAX_MV_EXTENT];
  _mode->overhead+=OC_MINI(cpi->MVBits_0+bits0,cpi->MVBits_1+12)
   -OC_MINI(cpi->MVBits_0,cpi->MVBits_1)<<OC_BIT_SCALE;
  oc_mode_set_cost(_mode,cpi->lambda);
  return bits0;
}

static int oc_cost_inter4mv(CP_INSTANCE *cpi,oc_mode_choice *_mode,int _mbi,
 oc_mv _mv[4],int _qi){
  macroblock_t *mb;
  int           pli;
  int           bi;
  int           bits0;
  mb=cpi->macro+_mbi;
  memcpy(mb->mv,_mv,sizeof(mb->mv));
  _mode->rate=_mode->ssd=0;
  bits0=0;
  for(bi=0;bi<4;bi++){
    int fi;
    fi=mb->Ryuv[0][bi];
    if(fi<cpi->frag_total){
      int dx;
      int dy;
      dx=_mv[bi][0];
      dy=_mv[bi][1];
      bits0+=MvBits[dx+MAX_MV_EXTENT]+MvBits[dy+MAX_MV_EXTENT];
      oc_mode_dct_cost_accum(_mode,_qi,0,1,
       BInterSAD(cpi,fi,dx,dy,0,0));
    }
  }
  (*OC_SET_CHROMA_MVS_TABLE[cpi->info.pixelformat])(mb->cbmvs,
   (const oc_mv *)_mv);
  for(pli=1;pli<3;pli++){
    for(bi=0;bi<4;bi++){
      int fi;
      fi=mb->Ryuv[pli][bi];
      if(fi<cpi->frag_total){
        int dx;
        int dy;
        dx=mb->cbmvs[bi][0];
        dy=mb->cbmvs[bi][1];
        oc_mode_dct_cost_accum(_mode,_qi,pli,1,
         BInterSAD(cpi,fi,dx,dy,pli,0));
      }
    }
  }
  _mode->overhead=oc_mode_scheme_chooser_cost(&cpi->chooser,CODE_INTER_FOURMV)
   +OC_MINI(cpi->MVBits_0+bits0,cpi->MVBits_1+48)
   -OC_MINI(cpi->MVBits_0,cpi->MVBits_1)<<OC_BIT_SCALE;
  oc_mode_set_cost(_mode,cpi->lambda);
  return bits0;
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

#if defined(OC_COLLECT_METRICS)
  int sad=0;
  if(mode==CODE_INTRA)sad=BIntraSAD(cpi,fi,ps->plane);
  else{
    sad=BInterSAD(cpi,fi,_dx,_dy,ps->plane,
     OC_FRAME_FOR_MODE[mode]==OC_FRAME_GOLD);
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

#if defined(OC_COLLECT_METRICS)
  {
#else
  if(!keyframe){
#endif
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
#if defined(OC_COLLECT_METRICS)
    cpi->frag_ssd[fi]=coded_ssd;
  }
  if(!keyframe){
#endif
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
      macroblock_t *mb;
      int           mbi;
      mbi=sb->m[j];
      if(mbi>=cpi->macro_total)continue;
      mb=cpi->macro+mbi;
      if(!recode){
        /*Motion estimation:
          We always do a basic 1MV search for all macroblocks, coded or not,
           keyframe or not.*/
        /*Move the motion vector predictors back a frame.*/
        memmove(mb->analysis_mv+1,mb->analysis_mv,2*sizeof(mb->analysis_mv[0]));
        /*Search the last frame.*/
        oc_mcenc_search(cpi,&mcenc,mbi,0,
         mb->block_mv,&mb->asatd,mb->block_satd);
        /*Search the golden frame.*/
        oc_mcenc_search(cpi,&mcenc,mbi,1,NULL,&mb->gsatd,NULL);
      }
      if(cpi->FrameType==KEY_FRAME){
        mb->mode=CODE_INTRA;
        /* Transform, quantize, collect rho metrics */
        TQMB_Y(cpi,mb,j,&ps,rho_count,0,NULL,&fr);
      }
      else{
        oc_mode_choice modes[8];
        int            mb_mv_bits_0;
        int            mb_gmv_bits_0;
        int            mb_4mv_bits_0;
        int            mb_4mv_bits_1;
        int            inter_mv_pref;
        int            mode;
        /*Find the block choice with the lowest estimated coding cost.
          If a Cb or Cr block is coded but no Y' block from a macro block then
           the mode MUST be CODE_INTER_NO_MV.
          This is the default state to which the mode data structure is
           initialised in encoder and decoder at the start of each frame.*/
        /*Block coding cost is estimated from correlated SATD metrics.*/
        /*At this point, all blocks that are in frame are still marked coded.*/
        if(!recode){
          memcpy(mb->unref_mv,mb->analysis_mv[0],sizeof(mb->unref_mv));
          mb->refined=0;
        }
        oc_cost_inter_nomv(cpi,modes+CODE_INTER_NO_MV,mbi,CODE_INTER_NO_MV,qi);
        oc_cost_intra(cpi,modes+CODE_INTRA,mbi,qi);
        intrabits+=modes[CODE_INTRA].rate;
        mb_mv_bits_0=oc_cost_inter1mv(cpi,modes+CODE_INTER_PLUS_MV,mbi,
         CODE_INTER_PLUS_MV,mb->unref_mv[0],qi);
        oc_cost_inter(cpi,modes+CODE_INTER_LAST_MV,mbi,
         CODE_INTER_LAST_MV,last_mv,qi);
        oc_cost_inter(cpi,modes+CODE_INTER_PRIOR_LAST,mbi,
         CODE_INTER_PRIOR_LAST,prior_mv,qi);
        oc_cost_inter_nomv(cpi,modes+CODE_USING_GOLDEN,mbi,
         CODE_USING_GOLDEN,qi);
        mb_gmv_bits_0=oc_cost_inter1mv(cpi,modes+CODE_GOLDEN_MV,mbi,
         CODE_GOLDEN_MV,mb->unref_mv[1],qi);
        mb_4mv_bits_0=oc_cost_inter4mv(cpi,modes+CODE_INTER_FOURMV,mbi,
         mb->block_mv,qi);
        mb_4mv_bits_1=48;
        /*The explicit MV modes (2,6,7) have not yet gone through halfpel
           refinement.
          We choose the explicit MV mode that's already furthest ahead on bits
           and refine only that one.
          We have to be careful to remember which ones we've refined so that
           we don't refine it again if we re-encode this frame.*/
        inter_mv_pref=cpi->lambda*3<<OC_BIT_SCALE;
        if(modes[CODE_INTER_FOURMV].cost<modes[CODE_INTER_PLUS_MV].cost&&
         modes[CODE_INTER_FOURMV].cost<modes[CODE_GOLDEN_MV].cost){
          if(!(mb->refined&0x80)){
            oc_mcenc_refine4mv(cpi, mbi, mb->block_satd);
            mb->refined|=0x80;
          }
          mb_4mv_bits_0=oc_cost_inter4mv(cpi,modes+CODE_INTER_FOURMV,mbi,
           mb->ref_mv,qi);
        }
        else if(modes[CODE_GOLDEN_MV].cost+inter_mv_pref<
         modes[CODE_INTER_PLUS_MV].cost){
          if(!(mb->refined&0x40)){
            oc_mcenc_refine1mv(cpi,mbi,1,mb->gsatd);
            mb->refined|=0x40;
          }
          mb_gmv_bits_0=oc_cost_inter1mv(cpi,modes+CODE_GOLDEN_MV,mbi,
           CODE_GOLDEN_MV,mb->analysis_mv[0][1],qi);
        }
        if(!(mb->refined&0x04)){
          oc_mcenc_refine1mv(cpi,mbi,0,mb->asatd);
          mb->refined|=0x04;
        }
        mb_mv_bits_0=oc_cost_inter1mv(cpi,modes+CODE_INTER_PLUS_MV,mbi,
         CODE_INTER_PLUS_MV,mb->analysis_mv[0][0],qi);
        /*Finally, pick the mode with the cheapest estimated bit cost.*/
        /*We prefer CODE_INTER_PLUS_MV, but not over LAST and LAST2.*/
        mode=0;
        if(modes[1].cost<modes[0].cost)mode=1;
        if(modes[3].cost<modes[mode].cost)mode=3;
        if(modes[4].cost<modes[mode].cost)mode=4;
        if(modes[5].cost<modes[mode].cost)mode=5;
        if(modes[6].cost<modes[mode].cost)mode=6;
        if(modes[7].cost<modes[mode].cost)mode=7;
        if(mode==CODE_INTER_LAST_MV||mode==CODE_INTER_PRIOR_LAST){
          inter_mv_pref=0;
        }
        if(modes[2].cost<modes[mode].cost+inter_mv_pref)mode=2;
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
        if(TQMB_Y(cpi,mb,j,&ps,rho_count,modes[mode].overhead,&mb_mv_bits_0,&fr)){
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
                (*OC_SET_CHROMA_MVS_TABLE[cpi->info.pixelformat])(mb->cbmvs,
                 (const oc_mv *)mb->mv);
              }
            }break;
            default:break;
          }
          oc_mode_scheme_chooser_update(&cpi->chooser,mb->mode);
          interbits+=modes[mb->mode].rate+modes[mb->mode].overhead;
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

#if defined(OC_COLLECT_METRICS)
# include <stdio.h>
# include <math.h>

# define OC_ZWEIGHT   (0.25)
# define OC_BIN(_sad) (OC_MINI((_sad)>>OC_SAD_SHIFT,OC_SAD_BINS-1))

static void oc_mode_metrics_add(oc_mode_metrics *_metrics,
 double _w,int _sad,int _rate,double _rmse){
  double rate;
  /*Accumulate statistics without the scaling; this lets us change the scale
     factor yet still use old data.*/
  rate=ldexp(_rate,-OC_BIT_SCALE);
  if(_metrics->fragw>0){
    double dsad;
    double drate;
    double drmse;
    double w;
    dsad=_sad-_metrics->sad/_metrics->fragw;
    drate=rate-_metrics->rate/_metrics->fragw;
    drmse=_rmse-_metrics->rmse/_metrics->fragw;
    w=_metrics->fragw*_w/(_metrics->fragw+_w);
    _metrics->sad2+=dsad*dsad*w;
    _metrics->sadrate+=dsad*drate*w;
    _metrics->rate2+=drate*drate*w;
    _metrics->sadrmse+=dsad*drmse*w;
    _metrics->rmse2+=drmse*drmse*w;
  }
  _metrics->fragw+=_w;
  _metrics->sad+=_sad*_w;
  _metrics->rate+=rate*_w;
  _metrics->rmse+=_rmse*_w;
}

static void oc_mode_metrics_merge(oc_mode_metrics *_dst,
 const oc_mode_metrics *_src,int _n){
  int i;
  /*Find a non-empty set of metrics.*/
  for(i=0;i<_n&&_src[i].fragw<=0;i++);
  if(i>=_n){
    memset(_dst,0,sizeof(*_dst));
    return;
  }
  memcpy(_dst,_src+i,sizeof(*_dst));
  /*And iterate over the remaining non-empty sets of metrics.*/
  for(i++;i<_n;i++)if(_src[i].fragw>0){
    double wa;
    double wb;
    double dsad;
    double drate;
    double drmse;
    double w;
    wa=_dst->fragw;
    wb=_src[i].fragw;
    dsad=_src[i].sad/wb-_dst->sad/wa;
    drate=_src[i].rate/wb-_dst->rate/wa;
    drmse=_src[i].rmse/wb-_dst->rmse/wa;
    w=wa*wb/(wa+wb);
    _dst->fragw+=_src[i].fragw;
    _dst->sad+=_src[i].sad;
    _dst->rate+=_src[i].rate;
    _dst->rmse+=_src[i].rmse;
    _dst->sad2+=_src[i].sad2+dsad*dsad*w;
    _dst->sadrate+=_src[i].sadrate+dsad*drate*w;
    _dst->rate2+=_src[i].rate2+drate*drate*w;
    _dst->sadrmse+=_src[i].sadrmse+dsad*drmse*w;
    _dst->rmse2+=_src[i].rmse2+drmse*drmse*w;
  }
}

static void oc_enc_mode_metrics_update(CP_INSTANCE *cpi,int _qi){
  int pli;
  int qti;
  oc_enc_restore_fpu(cpi);
  /*Compile collected SAD/rate/RMSE metrics into a form that's immediately
     useful for mode decision.*/
  /*Convert raw collected data into cleaned up sample points.*/
  for(pli=0;pli<3;pli++){
    for(qti=0;qti<2;qti++){
      double fragw;
      int    bin0;
      int    bin1;
      int    bin;
      fragw=0;
      bin0=bin1=0;
      for(bin=0;bin<OC_SAD_BINS;bin++){
        oc_mode_metrics metrics;
        OC_MODE_RD[_qi][pli][qti][bin].rate=0;
        OC_MODE_RD[_qi][pli][qti][bin].rmse=0;
        /*Find some points on either side of the current bin.*/
        while((bin1<bin+1||fragw<OC_ZWEIGHT)&&bin1<OC_SAD_BINS-1){
          fragw+=OC_MODE_METRICS[_qi][pli][qti][bin1++].fragw;
        }
        while(bin0+1<bin&&bin0+1<bin1&&
         fragw-OC_MODE_METRICS[_qi][pli][qti][bin0].fragw>=OC_ZWEIGHT){
          fragw-=OC_MODE_METRICS[_qi][pli][qti][bin0++].fragw;
        }
        /*Merge statistics and fit lines.*/
        oc_mode_metrics_merge(&metrics,
         OC_MODE_METRICS[_qi][pli][qti]+bin0,bin1-bin0);
        if(metrics.fragw>0&&metrics.sad2>0){
          double a;
          double b;
          double msad;
          double mrate;
          double mrmse;
          double rate;
          double rmse;
          msad=metrics.sad/metrics.fragw;
          mrate=metrics.rate/metrics.fragw;
          mrmse=metrics.rmse/metrics.fragw;
          /*Compute the points on these lines corresponding to the actual bin
             value.*/
          b=metrics.sadrate/metrics.sad2;
          a=mrate-b*msad;
          rate=ldexp(a+b*(bin<<OC_SAD_SHIFT),OC_BIT_SCALE);
          OC_MODE_RD[_qi][pli][qti][bin].rate=
           (ogg_int16_t)OC_CLAMPI(-32768,(int)(rate+0.5),32767);
          b=metrics.sadrmse/metrics.sad2;
          a=mrmse-b*msad;
          rmse=ldexp(a+b*(bin<<OC_SAD_SHIFT),OC_RMSE_SCALE);
          OC_MODE_RD[_qi][pli][qti][bin].rmse=
           (ogg_int16_t)OC_CLAMPI(-32768,(int)(rmse+0.5),32767);
        }
      }
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
  double fragw;
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
  oc_enc_restore_fpu(cpi);
  /*Weight the fragments by the inverse frame size; this prevents HD content
     from dominating the statistics.*/
  fragw=1.0/cpi->frag_n[0];
  memset(actual_bits,0,sizeof(actual_bits));
  memset(eobcounts,0,sizeof(eobcounts));
  huff[0] = cpi->huffchoice[interp][0][0];
  huff[1] = cpi->huffchoice[interp][0][1];
  huff[2] = cpi->huffchoice[interp][1][0];
  huff[3] = cpi->huffchoice[interp][1][1];

  memset(cpi->dist_dist,0,sizeof(cpi->dist_dist));
  memset(cpi->dist_bits,0,sizeof(cpi->dist_bits));

  if(!oc_has_mode_metrics){
    FILE *fmetrics;
    int   qi;
    memset(OC_MODE_METRICS,0,sizeof(OC_MODE_METRICS));
    fmetrics=fopen("modedec.stats","rb");
    if(fmetrics!=NULL){
      fread(OC_MODE_METRICS,sizeof(OC_MODE_METRICS),1,fmetrics);
      fclose(fmetrics);
    }
    for(qi=0;qi<64;qi++)oc_enc_mode_metrics_update(cpi,qi);
    oc_has_mode_metrics=1;
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
    int bin = OC_BIN(sp[fi]);
    oc_mode_metrics_add(OC_MODE_METRICS[qi][plane][mode!=CODE_INTRA]+bin,
     fragw,sp[fi],actual_bits[fi],sqrt(cpi->frag_ssd[fi]));
  }
  /* update global SAD/rate estimation matrix */
  oc_enc_mode_metrics_update(cpi,qi);
}

void oc_enc_mode_metrics_dump(CP_INSTANCE *cpi){
  FILE *fmetrics;
  int   qi;
  /*Generate sample points for complete list of QI values.*/
  for(qi=0;qi<64;qi++)oc_enc_mode_metrics_update(cpi,qi);
  fmetrics=fopen("modedec.stats","wb");
  if(fmetrics!=NULL){
    fwrite(OC_MODE_METRICS,sizeof(OC_MODE_METRICS),1,fmetrics);
    fclose(fmetrics);
  }
  fprintf(stdout,
   "/*File generated by libtheora with OC_COLLECT_METRICS"
   " defined at compile time.*/\n"
   "#if !defined(_modedec_H)\n"
   "# define _modedec_H (1)\n"
   "\n"
   "\n"
   "\n"
   "# if defined(OC_COLLECT_METRICS)\n"
   "typedef struct oc_mode_metrics oc_mode_metrics;\n"
   "# endif\n"
   "typedef struct oc_mode_rd      oc_mode_rd;\n"
   "\n"
   "\n"
   "\n"
   "/*The number of extra bits of precision at which to store rate"
   " metrics.*/\n"
   "# define OC_BIT_SCALE  (%i)\n"
   "/*The number of extra bits of precision at which to store RMSE metrics.\n"
   "  This must be at least half OC_BIT_SCALE (rounded up).*/\n"
   "# define OC_RMSE_SCALE (%i)\n"
   "/*The number of bins to partition statistics into.*/\n"
   "# define OC_SAD_BINS   (%i)\n"
   "/*The number of bits of precision to drop"
   " from SAD scores to assign them to a\n"
   "   bin.*/\n"
   "# define OC_SAD_SHIFT  (%i)\n"
   "\n"
   "\n"
   "\n"
   "# if defined(OC_COLLECT_METRICS)\n"
   "struct oc_mode_metrics{\n"
   "  double fragw;\n"
   "  double sad;\n"
   "  double rate;\n"
   "  double rmse;\n"
   "  double sad2;\n"
   "  double sadrate;\n"
   "  double rate2;\n"
   "  double sadrmse;\n"
   "  double rmse2;\n"
   "};\n"
   "\n"
   "\n"
   "int             oc_has_mode_metrics;\n"
   "oc_mode_metrics OC_MODE_METRICS[64][3][2][OC_SAD_BINS];\n"
   "# endif\n"
   "\n"
   "\n"
   "\n"
   "struct oc_mode_rd{\n"
   "  ogg_int16_t rate;\n"
   "  ogg_int16_t rmse;\n"
   "};\n"
   "\n"
   "\n"
   "# if !defined(OC_COLLECT_METRICS)\n"
   "static const\n"
   "# endif\n"
   "oc_mode_rd OC_MODE_RD[64][3][2][OC_SAD_BINS]={\n",
   OC_BIT_SCALE,OC_RMSE_SCALE,OC_SAD_BINS,OC_SAD_SHIFT);
  for(qi=0;qi<64;qi++){
    int pli;
    fprintf(stdout,"  {\n");
    for(pli=0;pli<3;pli++){
      int qti;
      fprintf(stdout,"    {\n");
      for(qti=0;qti<2;qti++){
        int bin;
        static const char *pl_names[3]={"Y'","Cb","Cr"};
        static const char *qti_names[2]={"INTRA","INTER"};
        fprintf(stdout,"      /*%s  qi=%i  %s*/\n",
         pl_names[pli],qi,qti_names[qti]);
        fprintf(stdout,"      {\n");
        fprintf(stdout,"        ");
        for(bin=0;bin<OC_SAD_BINS;bin++){
          if(bin&&!(bin&0x3))fprintf(stdout,"\n        ");
          fprintf(stdout,"{%5i,%5i}",
           OC_MODE_RD[qi][pli][qti][bin].rate,
           OC_MODE_RD[qi][pli][qti][bin].rmse);
          if(bin+1<OC_SAD_BINS)fprintf(stdout,",");
        }
        fprintf(stdout,"\n      }");
        if(qti<1)fprintf(stdout,",");
        fprintf(stdout,"\n");
      }
      fprintf(stdout,"    }");
      if(pli<2)fprintf(stdout,",");
      fprintf(stdout,"\n");
    }
    fprintf(stdout,"  }");
    if(qi<63)fprintf(stdout,",");
    fprintf(stdout,"\n");
  }
  fprintf(stdout,
   "};\n"
   "\n"
   "#endif\n");
}
#endif
