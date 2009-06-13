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
#include <limits.h>
#include <string.h>
#include "encint.h"
#include "modedec.h"



typedef struct oc_plane_state oc_plane_state;
typedef struct oc_frag_state  oc_frag_state;
typedef struct oc_mode_choice oc_mode_choice;
typedef struct oc_rd_metric   oc_rd_metric;



/*Temporary encoder state for a single color plane.*/
struct oc_plane_state{
  /*Condensed dequantization tables.*/
  const ogg_uint16_t *dequant[3][2];
  /*Condensed quantization tables.*/
  const oc_iquant    *enquant[3][2];
  /*Plane index.*/
  int                 pli;
};



/*State to track coded block flags and their bit cost.*/
struct oc_frag_state{
  unsigned   sb_partial_count:16;
  unsigned   sb_full_count:16;
  unsigned   b_count:8;
  unsigned   b_pend:8;
  signed int sb_partial_last:2;
  signed int sb_full_last:2;
  signed int b_last:2;
  unsigned   sb_partial:1;
  unsigned   sb_coded:1;
  unsigned   sb_partial_break:1;
  unsigned   sb_full_break:1;
  ptrdiff_t  bits;
};



/*Cost information about a MB mode.*/
struct oc_mode_choice{
  unsigned cost;
  unsigned ssd;
  unsigned rate;
  unsigned overhead;
};



/*Cost information about the coded blocks in a MB.*/
struct oc_rd_metric{
  int uncoded_ac_ssd;
  int coded_ac_ssd;
  int ac_bits;
  int dc_flag;
};



/*There are 8 possible schemes used to encode macro block modes.
  Schemes 0-6 use a maximally-skewed Huffman code to code each of the modes.
  The same set of Huffman codes is used for each of these 7 schemes, but the
   mode assigned to each codeword varies.
  Scheme 0 writes a custom mapping from codeword to MB mode to the bitstream,
   while schemes 1-6 have a fixed mapping.
  Scheme 7 just encodes each mode directly in 3 bits.*/

/*The mode orderings for the various mode coding schemes.
  Scheme 0 uses a custom alphabet, which is not stored in this table.
  This is the inverse of the equivalent table OC_MODE_ALPHABETS in the
   decoder.*/
static const unsigned char OC_MODE_RANKS[7][OC_NMODES]={
  /*Last MV dominates.*/ 
  /*L P M N I G GM 4*/
  {3,4,2,0,1,5,6,7},
  /*L P N M I G GM 4*/
  {2,4,3,0,1,5,6,7},
  /*L M P N I G GM 4*/
  {3,4,1,0,2,5,6,7},
  /*L M N P I G GM 4*/
  {2,4,1,0,3,5,6,7},
  /*No MV dominates.*/
  /*N L P M I G GM 4*/
  {0,4,3,1,2,5,6,7},
  /*N G L P M I GM 4*/
  {0,5,4,2,3,1,6,7},
  /*Default ordering.*/
  /*N I M L P G GM 4*/
  {0,1,2,3,4,5,6,7}
};



/*Initialize the mode scheme chooser.
  This need only be called once per encoder.*/
void oc_mode_scheme_chooser_init(oc_mode_scheme_chooser *_chooser){
  int si;
  _chooser->mode_ranks[0]=_chooser->scheme0_ranks;
  for(si=1;si<8;si++)_chooser->mode_ranks[si]=OC_MODE_RANKS[si-1];
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
  _mb_mode: The mode to determine the cost of.
  Return: The number of bits required to code this mode.*/
static int oc_mode_scheme_chooser_cost(oc_mode_scheme_chooser *_chooser,
 int _mb_mode){
  int scheme0;
  int scheme1;
  int best_bits;
  int mode_bits;
  int si;
  int scheme_bits;
  scheme0=_chooser->scheme_list[0];
  scheme1=_chooser->scheme_list[1];
  best_bits=_chooser->scheme_bits[scheme0];
  mode_bits=OC_MODE_BITS[scheme0+1>>3][_chooser->mode_ranks[scheme0][_mb_mode]];
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
       OC_MODE_BITS[scheme1+1>>3][_chooser->mode_ranks[scheme1][_mb_mode]];
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
      for(ri=_chooser->scheme0_ranks[_mb_mode];ri>0&&
       _chooser->mode_counts[_mb_mode]>=
       _chooser->mode_counts[_chooser->scheme0_list[ri-1]];ri--);
      scheme_bits=_chooser->scheme_bits[0]+OC_MODE_BITS[0][ri];
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
  _mb_mode: The mode that was chosen.*/
static void oc_mode_scheme_chooser_update(oc_mode_scheme_chooser *_chooser,
 int _mb_mode){
  int ri;
  int si;
  _chooser->mode_counts[_mb_mode]++;
  /*Re-order the scheme0 mode list if necessary.*/
  for(ri=_chooser->scheme0_ranks[_mb_mode];ri>0;ri--){
    int pmode;
    pmode=_chooser->scheme0_list[ri-1];
    if(_chooser->mode_counts[pmode]>=_chooser->mode_counts[_mb_mode])break;
    /*Reorder the mode ranking.*/
    _chooser->scheme0_ranks[pmode]++;
    _chooser->scheme0_list[ri]=pmode;
  }
  _chooser->scheme0_ranks[_mb_mode]=ri;
  _chooser->scheme0_list[ri]=_mb_mode;
  /*Now add the bit cost for the mode to each scheme.*/
  for(si=0;si<8;si++){
    _chooser->scheme_bits[si]+=
     OC_MODE_BITS[si+1>>3][_chooser->mode_ranks[si][_mb_mode]];
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



static void oc_plane_state_plane_setup(oc_enc_ctx *_enc,oc_plane_state *_ps,
 int _pli){
  int qii;
  int qti;
  _ps->pli=_pli;
  for(qii=0;qii<_enc->state.nqis;qii++){
    int qi;
    qi=_enc->state.qis[qii];
    for(qti=0;qti<2;qti++){
      _ps->dequant[qii][qti]=_enc->state.dequant_tables[qi][_pli][qti];
      _ps->enquant[qii][qti]=_enc->enquant_tables[qi][_pli][qti];
    }
  }
}



static int oc_sb_run_bits(int _run_count){
  int i;
  for(i=0;_run_count>=OC_SB_RUN_VAL_MIN[i+1];i++);
  return OC_SB_RUN_CODE_NBITS[i];
}

static int oc_block_run_bits(int _run_count){
  return OC_BLOCK_RUN_CODE_NBITS[_run_count-1];
}



static void oc_fr_state_init(oc_frag_state *_fr){
  _fr->sb_partial_last=-1;
  _fr->sb_partial_count=0;
  _fr->sb_partial_break=0;
  _fr->sb_full_last=-1;
  _fr->sb_full_count=0;
  _fr->sb_full_break=0;
  _fr->b_last=-1;
  _fr->b_count=0;
  _fr->b_pend=0;
  _fr->sb_partial=0;
  _fr->sb_coded=0;
  _fr->bits=0;
}


static void oc_fr_skip_block(oc_frag_state *_fr){
  if(_fr->sb_coded){
    if(!_fr->sb_partial){
      /*The super block was previously fully coded.*/
      if(_fr->b_last==-1){
        /*First run of the frame...*/
        _fr->bits++;
        _fr->b_last=1;
      }
      if(_fr->b_last==1){
        /*The in-progress run is also a coded run.*/
        _fr->b_count+=_fr->b_pend;
      }
      else{
        /*The in-progress run is an uncoded run; flush.*/
        _fr->bits+=oc_block_run_bits(_fr->b_count);
        _fr->b_count=_fr->b_pend;
        _fr->b_last=1;
      }
    }
    /*Add a skip block.*/
    if(_fr->b_last==0)_fr->b_count++;
    else{
      if(_fr->b_count)_fr->bits+=oc_block_run_bits(_fr->b_count);
      _fr->b_count=1;
      _fr->b_last=0;
    }
  }
  _fr->b_pend++;
  _fr->sb_partial=1;
}

static void oc_fr_code_block(oc_frag_state *_fr){
  if(_fr->sb_partial){
    if(!_fr->sb_coded){
      /*The super block was previously completely uncoded...*/
      if(_fr->b_last==-1){
        /*First run of the frame...*/
        _fr->bits++;
        _fr->b_last=0;
      }
      if(_fr->b_last==0){
        /*The in-progress run is also an uncoded run.*/
        _fr->b_count += _fr->b_pend;
      }
      else{
        /*The in-progress run is a coded run; flush.*/
        _fr->bits+=oc_block_run_bits(_fr->b_count);
        _fr->b_count=_fr->b_pend;
        _fr->b_last=0;
      }
    }
    /*Add a coded block.*/
    if(_fr->b_last==1)_fr->b_count++;
    else{
      _fr->bits+=oc_block_run_bits(_fr->b_count);
      _fr->b_count=1;
      _fr->b_last=1;
    }
  }
  _fr->b_pend++;
  _fr->sb_coded=1;
}

static void oc_fr_finish_sb(oc_frag_state *_fr){
  /*Update the partial flag.*/
  int partial;
  partial=_fr->sb_partial&_fr->sb_coded;
  if(_fr->sb_partial_last==-1){
    _fr->bits++;
    _fr->sb_partial_last=partial;
  }
  if(_fr->sb_partial_break){
    _fr->bits++;
    _fr->sb_partial_break=0;
  }
  if(_fr->sb_partial_last==partial&&_fr->sb_partial_count<4129){
    _fr->sb_partial_count++;
  }
  else{
    _fr->bits+=oc_sb_run_bits(_fr->sb_partial_count);
    if(_fr->sb_partial_count>=4129)_fr->sb_partial_break=1;
    _fr->sb_partial_count=1;
  }
  _fr->sb_partial_last=partial;
  /*Fully coded/uncoded state.*/
  if(!_fr->sb_partial||!_fr->sb_coded){
    if(_fr->sb_full_last==-1){
      _fr->bits++;
      _fr->sb_full_last=_fr->sb_coded;
    }
    if(_fr->sb_full_break){
      _fr->bits++;
      _fr->sb_full_break=0;
    }
    if(_fr->sb_full_last==_fr->sb_coded&&_fr->sb_full_count<4129){
      _fr->sb_full_count++;
    }
    else{
      _fr->bits+=oc_sb_run_bits( _fr->sb_full_count);
      if(_fr->sb_full_count>=4129)_fr->sb_full_break=1;
      _fr->sb_full_count=1;
    }
    _fr->sb_full_last=_fr->sb_coded;
  }
  _fr->b_pend=0;
  _fr->sb_partial=0;
  _fr->sb_coded=0;
}

static void oc_fr_flush(oc_frag_state *_fr){
  /*Flush any pending partial run.*/
  if(_fr->sb_partial_break)_fr->bits++;
  if(_fr->sb_partial_count)_fr->bits+=oc_sb_run_bits(_fr->sb_partial_count);
  /*Flush any pending full run.*/
  if(_fr->sb_full_break)_fr->bits++;
  if(_fr->sb_full_count)_fr->bits+=oc_sb_run_bits(_fr->sb_full_count);
  /*Flush any pending block run.*/
  if(_fr->b_count)_fr->bits+=oc_block_run_bits(_fr->b_count);
}

static int oc_fr_cost1(const oc_frag_state *_fr){
  oc_frag_state tmp;
  int           bits;
  *&tmp=*_fr;
  oc_fr_skip_block(&tmp);
  bits=tmp.bits;
  *&tmp=*_fr;
  oc_fr_code_block(&tmp);
  return tmp.bits-bits;
}

static int oc_fr_cost4(const oc_frag_state *_pre,const oc_frag_state *_post){
  oc_frag_state tmp;
  *&tmp=*_pre;
  oc_fr_skip_block(&tmp);
  oc_fr_skip_block(&tmp);
  oc_fr_skip_block(&tmp);
  oc_fr_skip_block(&tmp);
  return _post->bits-tmp.bits;
}



static void oc_enc_frag_uncode(oc_enc_ctx *_enc,int _pli,ptrdiff_t _fragi){
  const unsigned char *src;
  unsigned char       *dst;
  ptrdiff_t            frag_offs;
  int                  ystride;
  frag_offs=_enc->state.frag_buf_offs[_fragi];
  ystride=_enc->state.ref_ystride[_pli];
  src=_enc->state.ref_frame_data[_enc->state.ref_frame_idx[OC_FRAME_PREV]];
  dst=_enc->state.ref_frame_data[_enc->state.ref_frame_idx[OC_FRAME_SELF]];
  oc_frag_copy(&_enc->state,dst+frag_offs,src+frag_offs,ystride);
  _enc->state.frags[_fragi].coded=0;
  /*We do NOT update frags[_fragi].mb_mode or frag_mvs[_fragi], since they are
     not subsequently referenced by uncoded fragments.*/
}

static int oc_enc_block_transform_quantize(oc_enc_ctx *_enc,
 oc_plane_state *_ps,ptrdiff_t _fragi,int _overhead_bits,
 oc_rd_metric *_mo,oc_token_checkpoint **_stack){
  ogg_int16_t          buffer[64]OC_ALIGN16;
  ogg_int16_t          data[64]OC_ALIGN16;
  const ogg_uint16_t  *dequant;
  const oc_iquant     *enquant;
  ptrdiff_t            frag_offs;
  int                  ystride;
  const unsigned char *src;
  const unsigned char *ref;
  unsigned char       *dst;
  int                  frame_type;
  int                  nonzero;
  int                  uncoded_ssd;
  int                  coded_ssd;
  int                  uncoded_dc;
  int                  coded_dc;
  int                  dc_flag;
  oc_token_checkpoint *checkpoint;
  oc_fragment         *frags;
  int                  mb_mode;
  int                  mv_offs[2];
  int                  nmv_offs;
  int                  ac_bits;
  int                  borderi;
  int                  pi;
  int                  zzi;
  frags=_enc->state.frags;
  frag_offs=_enc->state.frag_buf_offs[_fragi];
  ystride=_enc->state.ref_ystride[_ps->pli];
  mb_mode=frags[_fragi].mb_mode;
  src=_enc->state.ref_frame_data[OC_FRAME_IO]+frag_offs;
  ref=_enc->state.ref_frame_data[
   _enc->state.ref_frame_idx[OC_FRAME_FOR_MODE[mb_mode]]]+frag_offs;
  dst=_enc->state.ref_frame_data[_enc->state.ref_frame_idx[OC_FRAME_SELF]]
   +frag_offs;
  /*Although the fragment coding overhead determination is accurate, it is
     greedy, using very coarse-grained local information.
    Allowing it to mildly discourage coding turns out to be beneficial, but
     it's not clear that allowing it to encourage coding through negative
     coding overhead deltas is useful.
    For that reason, we disallow negative coding_overheads.*/
  if(_overhead_bits<0)_overhead_bits=0;
  /*Motion compensation:*/
  switch(mb_mode){
    case OC_MODE_INTRA:{
      nmv_offs=0;
      oc_enc_frag_sub_128(_enc,data,src,ystride);
    }break;
    case OC_MODE_GOLDEN_NOMV:
    case OC_MODE_INTER_NOMV:{
      nmv_offs=1;
      mv_offs[0]=0;
      oc_enc_frag_sub(_enc,data,src,ref,ystride);
    }break;
    default:{
      const oc_mv *frag_mvs;
      frag_mvs=(const oc_mv *)_enc->state.frag_mvs;
      nmv_offs=oc_state_get_mv_offsets(&_enc->state,mv_offs,_ps->pli,
       frag_mvs[_fragi][0],frag_mvs[_fragi][1]);
      if(nmv_offs>1){
        oc_enc_frag_copy2(_enc,dst,
         ref+mv_offs[0],ref+mv_offs[1],ystride);
        oc_enc_frag_sub(_enc,data,src,dst,ystride);
      }
      else oc_enc_frag_sub(_enc,data,src,ref+mv_offs[0],ystride);
    }break;
  }
#if defined(OC_COLLECT_METRICS)
  {
    unsigned satd;
    switch(nmv_offs){
      case 0:satd=oc_enc_frag_intra_satd(_enc,src,ystride);break;
      case 1:{
        satd=oc_enc_frag_satd_thresh(_enc,src,ref+mv_offs[0],ystride,UINT_MAX);
      }break;
      default:{
        satd=oc_enc_frag_satd_thresh(_enc,src,dst,ystride,UINT_MAX);
      }
    }
    _enc->frag_satd[_fragi]=satd;
  }
#endif
  frame_type=_enc->state.frame_type;
  borderi=frags[_fragi].borderi;
  uncoded_ssd=uncoded_dc=0;
  if(frame_type!=OC_INTRA_FRAME){
    if(mb_mode==OC_MODE_INTER_NOMV){
      if(borderi<0){
        for(pi=0;pi<64;pi++){
          uncoded_ssd+=data[pi]*data[pi];
          uncoded_dc+=data[pi];
        }
      }
      else{
        ogg_int64_t mask;
        mask=_enc->state.borders[borderi].mask;
        for(pi=0;pi<64;pi++,mask>>=1)if(mask&1){
          uncoded_ssd+=data[pi]*data[pi];
          uncoded_dc+=data[pi];
        }
      }
    }
    else{
      oc_enc_frag_sub(_enc,buffer,src,
       _enc->state.ref_frame_data[_enc->state.ref_frame_idx[OC_FRAME_PREV]]
       +frag_offs,ystride);
      if(borderi<0){
        for(pi=0;pi<64;pi++){
          uncoded_ssd+=buffer[pi]*buffer[pi];
          uncoded_dc+=buffer[pi];
        }
      }
      else{
        ogg_int64_t mask;
        mask=_enc->state.borders[borderi].mask;
        for(pi=0;pi<64;pi++,mask>>=1)if(mask&1){
          uncoded_ssd+=buffer[pi]*buffer[pi];
          uncoded_dc+=buffer[pi];
        }
      }
    }
    /*Scale to match DCT domain.*/
    uncoded_ssd<<=4;
  }
  /*Transform:*/
  oc_enc_fdct8x8(_enc,buffer,data);
  /*Quantize:*/
  dequant=_ps->dequant[0][mb_mode!=OC_MODE_INTRA];
  enquant=_ps->enquant[0][mb_mode!=OC_MODE_INTRA];
  nonzero=0;
  for(zzi=0;zzi<64;zzi++){
    int v;
    int val;
    int d;
    v=buffer[OC_FZIG_ZAG[zzi]];
    d=dequant[zzi];
    val=v<<1;
    v=abs(val);
    if(v>=d){
      int s;
      s=OC_SIGNMASK(val);
      /*The bias added here rounds ties away from zero, since token
         optimization can only decrease the magnitude of the quantized
         value.*/
      val+=(d+s)^s;
      /*Note the arithmetic right shift is not guaranteed by ANSI C.
        Hopefully no one still uses ones-complement architectures.*/
      val=((enquant[zzi].m*(ogg_int32_t)val>>16)+val>>enquant[zzi].l)-s;
      data[zzi]=OC_CLAMPI(-580,val,580);
      nonzero=zzi;
    }
    else data[zzi]=0;
  }
  frags[_fragi].dc=data[0];
  frags[_fragi].coded=1;
  /*Tokenize.*/
  checkpoint=*_stack;
  ac_bits=oc_enc_tokenize_ac(_enc,_fragi,data,dequant,buffer,
   _ps->pli,_stack,mb_mode==OC_MODE_INTRA?3:0);
  /*Reconstruct.
    TODO: nonzero may need to be adjusted after tokenization.*/
  oc_dequant_idct8x8(&_enc->state,buffer,data,
   nonzero+1,nonzero+1,dequant[0],(ogg_uint16_t *)dequant);
  if(mb_mode==OC_MODE_INTRA)oc_enc_frag_recon_intra(_enc,dst,ystride,buffer);
  else{
    oc_enc_frag_recon_inter(_enc,dst,
     nmv_offs==1?ref+mv_offs[0]:dst,ystride,buffer);
  }
#if !defined(OC_COLLECT_METRICS)
  if(frame_type!=OC_INTRA_FRAME)
#endif
  {
    /*In retrospect, should we have skipped this block?*/
    oc_enc_frag_sub(_enc,buffer,src,dst,ystride);
    coded_ssd=coded_dc=0;
    if(borderi<0){
      for(pi=0;pi<64;pi++){
        coded_ssd+=buffer[pi]*buffer[pi];
        coded_dc+=buffer[pi];
      }
    }
    else{
      ogg_int64_t mask;
      mask=_enc->state.borders[borderi].mask;
      for(pi=0;pi<64;pi++,mask>>=1)if(mask&1){
        coded_ssd+=buffer[pi]*buffer[pi];
        coded_dc+=buffer[pi];
      }
    }
    /*Scale to match DCT domain.*/
    coded_ssd<<=4;
    /*We actually only want the AC contribution to the SSDs.*/
    uncoded_ssd-=uncoded_dc*uncoded_dc>>2;
    coded_ssd-=coded_dc*coded_dc>>2;
#if defined(OC_COLLECT_METRICS)
    _enc->frag_ssd[_fragi]=coded_ssd;
  }
  if(frame_type!=OC_INTRA_FRAME){
#endif
    _mo->uncoded_ac_ssd+=uncoded_ssd;
    /*DC is a special case; if there's more than a full-quantizer improvement
       in the effective DC component, always force-code the block.
      One might expect this to be abs(uncoded_dc-coded_dc), but this performs
       slightly better, since coded_dc will always be near zero, but may be on
       the opposite side of zero from uncoded_dc.*/
    dc_flag=abs(uncoded_dc)-abs(coded_dc)>dequant[0]<<1;
    if(!dc_flag&&uncoded_ssd<=coded_ssd+(_overhead_bits+ac_bits)*_enc->lambda&&
     /*Don't allow luma blocks to be skipped in 4MV mode when VP3 compatibility
        is enabled.*/
     (!_enc->vp3_compatible||mb_mode!=OC_MODE_INTER_MV_FOUR)){
      /*Hm, not worth it; roll back.*/
      oc_enc_tokenlog_rollback(_enc,checkpoint,(*_stack)-checkpoint);
      *_stack=checkpoint;
      oc_enc_frag_uncode(_enc,_ps->pli,_fragi);
      _mo->coded_ac_ssd+=uncoded_ssd;
      return 0;
    }
    else{
      _mo->dc_flag|=dc_flag;
      _mo->coded_ac_ssd+=coded_ssd;
      _mo->ac_bits+=ac_bits;
    }
  }
  return 1;
}

/* mode_overhead is scaled by << OC_BIT_SCALE */
static int oc_enc_mb_transform_quantize_luma(oc_enc_ctx *_enc,
 oc_plane_state *_ps,int _mbi,int _mode_overhead,oc_frag_state *_fr){
  /*Worst case token stack usage for 4 fragments.*/
  oc_token_checkpoint  stack[64*4];
  oc_token_checkpoint *stackptr;
  const oc_sb_map     *sb_maps;
  signed char         *mb_modes;
  oc_fragment         *frags;
  ptrdiff_t           *coded_fragis;
  ptrdiff_t            ncoded_fragis;
  oc_rd_metric         mo;
  oc_frag_state        fr_checkpoint;
  int                  mb_mode;
  int                  ncoded;
  ptrdiff_t            fragi;
  int                  bi;
  *&fr_checkpoint=*_fr;
  sb_maps=(const oc_sb_map *)_enc->state.sb_maps;
  mb_modes=_enc->state.mb_modes;
  frags=_enc->state.frags;
  coded_fragis=_enc->state.coded_fragis;
  ncoded_fragis=_enc->state.ncoded_fragis[0];
  mb_mode=mb_modes[_mbi];
  ncoded=0;
  stackptr=stack;
  memset(&mo,0,sizeof(mo));
  for(bi=0;bi<4;bi++){
    fragi=sb_maps[_mbi>>2][_mbi&3][bi];
    frags[fragi].mb_mode=mb_mode;
    if(oc_enc_block_transform_quantize(_enc,
     _ps,fragi,oc_fr_cost1(_fr),&mo,&stackptr)){
      oc_fr_code_block(_fr);
      coded_fragis[ncoded_fragis++]=fragi;
      ncoded++;
    }
    else oc_fr_skip_block(_fr);
  }
  if(_enc->state.frame_type!=OC_INTRA_FRAME){
    if(ncoded>0&&!mo.dc_flag){
      int cost;
      /*Some individual blocks were worth coding.
        See if that's still true when accounting for mode and MV overhead.*/
      cost=mo.coded_ac_ssd+_enc->lambda*(mo.ac_bits
       +oc_fr_cost4(&fr_checkpoint,_fr)+(_mode_overhead>>OC_BIT_SCALE));
      if(mo.uncoded_ac_ssd<=cost){
        /*Taking macroblock overhead into account, it is not worth coding this
           MB.*/
        oc_enc_tokenlog_rollback(_enc,stack,stackptr-stack);
        *_fr=*&fr_checkpoint;
        for(bi=0;bi<4;bi++){
          fragi=sb_maps[_mbi>>2][_mbi&3][bi];
          if(frags[fragi].coded)oc_enc_frag_uncode(_enc,0,fragi);
          oc_fr_skip_block(_fr);
        }
        ncoded_fragis-=ncoded;
        ncoded=0;
      }
    }
    if(ncoded==0){
      /*No luma blocks coded, mode is forced.*/
      mb_modes[_mbi]=OC_MODE_INTER_NOMV;
      return 0;
    }
    /*Assume that a 1mv with a single coded block is always cheaper than a 4mv
       with a single coded block.
      This may not be strictly true: a 4MV computes chroma MVs using (0,0) for
       skipped blocks, while a 1MV does not.*/
    else if(ncoded==1&&mb_mode==OC_MODE_INTER_MV_FOUR){
      mb_modes[_mbi]=OC_MODE_INTER_MV;
    }
  }
  _enc->state.ncoded_fragis[0]=ncoded_fragis;
  return ncoded;
}

static void oc_enc_sb_transform_quantize_chroma(oc_enc_ctx *_enc,
 oc_plane_state *_ps,int _sbi_start,int _sbi_end,oc_frag_state *_fr){
  const oc_sb_map *sb_maps;
  oc_sb_flags     *sb_flags;
  ptrdiff_t       *coded_fragis;
  ptrdiff_t        ncoded_fragis;
  int              sbi;
  sb_maps=(const oc_sb_map *)_enc->state.sb_maps;
  sb_flags=_enc->state.sb_flags;
  coded_fragis=_enc->state.coded_fragis+_enc->state.fplanes[_ps->pli].froffset;
  ncoded_fragis=_enc->state.ncoded_fragis[_ps->pli];
  for(sbi=_sbi_start;sbi<_sbi_end;sbi++){
    /*Worst case token stack usage for 1 fragment.*/
    oc_token_checkpoint stack[64];
    oc_rd_metric        mo;
    int                 quadi;
    int                 bi;
    memset(&mo,0,sizeof(mo));
    for(quadi=0;quadi<4;quadi++)for(bi=0;bi<4;bi++){
      ptrdiff_t fragi;
      fragi=sb_maps[sbi][quadi][bi];
      if(fragi>=0){
        oc_token_checkpoint *stackptr;
        stackptr=stack;
        if(oc_enc_block_transform_quantize(_enc,
         _ps,fragi,oc_fr_cost1(_fr),&mo,&stackptr)){
          coded_fragis[ncoded_fragis++]=fragi;
          oc_fr_code_block(_fr);
        }
        else oc_fr_skip_block(_fr);
      }
    }
    oc_fr_finish_sb(_fr);
    sb_flags[sbi].coded_fully=_fr->sb_full_last;
    sb_flags[sbi].coded_partially=_fr->sb_partial_last;
  }
  _enc->state.ncoded_fragis[_ps->pli]=ncoded_fragis;
}

/*Mode decision is done by exhaustively examining all potential choices.
  Obviously, doing the motion compensation, fDCT, tokenization, and then
   counting the bits each token uses is computationally expensive.
  Theora's EOB runs can also split the cost of these tokens across multiple
   fragments, and naturally we don't know what the optimal choice of Huffman
   codes will be until we know all the tokens we're going to encode in all the
   fragments.
  So we use a simple approach to estimating the bit cost and distortion of each
   mode based upon the SATD value of the residual before coding.
  The mathematics behind the technique are outlined by Kim \cite{Kim03}, but
   the process (modified somewhat from that of the paper) is very simple.
  We build a non-linear regression of the mappings from
   (pre-transform+quantization) SATD to (post-transform+quantization) bits and
   SSD for each qi.
  A separate set of mappings is kept for each quantization type and color
   plane.
  The mappings are constructed by partitioning the SATD values into a small
   number of bins (currently 24) and using a linear regression in each bin
   (as opposed to the 0th-order regression used by Kim).
  The bit counts and SSD measurements are obtained by examining actual encoded
   frames, with appropriate lambda values and optimal Huffman codes selected.
  EOB bits are assigned to the fragment that started the EOB run (as opposed to
   dividing them among all the blocks in the run; though the latter approach
   seems more theoretically correct, Monty's testing showed a small improvement
   with the former, though that may have been merely statistical noise).

  @ARTICLE{Kim03,
    author="Hyun Mun Kim",
    title="Adaptive Rate Control Using Nonlinear Regression",
    journal="IEEE Transactions on Circuits and Systems for Video Technology",
    volume=13,
    number=5,
    pages="432--439",
    month=May,
    year=2003
  }*/

static void oc_mode_dct_cost_accum(oc_mode_choice *_modec,
 int _qi,int _pli,int _qti,int _satd){
  unsigned rmse;
  int      bin;
  int      dx;
  int      y0;
  int      z0;
  int      dy;
  int      dz;
  /*STAD metrics for chroma planes vary much less than luma, so we scale them
     by 4 to distribute them into the mode decision bins more evenly.*/
  _satd<<=_pli+1&2;
  bin=OC_MINI(_satd>>OC_SAD_SHIFT,OC_SAD_BINS-2);
  dx=_satd-(bin<<OC_SAD_SHIFT);
  y0=OC_MODE_RD[_qi][_pli][_qti][bin].rate;
  z0=OC_MODE_RD[_qi][_pli][_qti][bin].rmse;
  dy=OC_MODE_RD[_qi][_pli][_qti][bin+1].rate-y0;
  dz=OC_MODE_RD[_qi][_pli][_qti][bin+1].rmse-z0;
  _modec->rate+=OC_MAXI(y0+(dy*dx>>OC_SAD_SHIFT),0);
  rmse=OC_MAXI(z0+(dz*dx>>OC_SAD_SHIFT),0);
  _modec->ssd+=rmse*rmse>>2*OC_RMSE_SCALE-OC_BIT_SCALE;
}

static void oc_mode_set_cost(oc_mode_choice *_modec,int _lambda){
 _modec->cost=_modec->ssd+(_modec->rate+_modec->overhead)*_lambda;
}

static void oc_cost_intra(oc_enc_ctx *_enc,oc_mode_choice *_modec,
 int _mbi,int _qi){
  const unsigned char   *src;
  const ptrdiff_t       *frag_buf_offs;
  const oc_mb_map_plane *mb_map;
  const unsigned char   *map_idxs;
  int                    map_nidxs;
  int                    mapii;
  int                    mapi;
  int                    ystride;
  int                    pli;
  int                    bi;
  ptrdiff_t              fragi;
  ptrdiff_t              frag_offs;
  frag_buf_offs=_enc->state.frag_buf_offs;
  mb_map=(const oc_mb_map_plane *)_enc->state.mb_maps[_mbi];
  src=_enc->state.ref_frame_data[OC_FRAME_IO];
  _modec->rate=_modec->ssd=0;
  ystride=_enc->state.ref_ystride[0];
  for(bi=0;bi<4;bi++){
    fragi=mb_map[0][bi];
    frag_offs=frag_buf_offs[fragi];
    oc_mode_dct_cost_accum(_modec,_qi,0,0,
     oc_enc_frag_intra_satd(_enc,src+frag_offs,ystride));
  }
  map_idxs=OC_MB_MAP_IDXS[_enc->state.info.pixel_fmt];
  map_nidxs=OC_MB_MAP_NIDXS[_enc->state.info.pixel_fmt];
  /*Note: This assumes ref_ystride[1]==ref_ystride[2].*/
  ystride=_enc->state.ref_ystride[1];
  for(mapii=4;mapii<map_nidxs;mapii++){
    mapi=map_idxs[mapii];
    pli=mapi>>2;
    bi=mapi&3;
    fragi=mb_map[pli][bi];
    frag_offs=frag_buf_offs[fragi];
    oc_mode_dct_cost_accum(_modec,_qi,pli,0,
     oc_enc_frag_intra_satd(_enc,src+frag_offs,ystride));
  }
  _modec->overhead=
   oc_mode_scheme_chooser_cost(&_enc->chooser,OC_MODE_INTRA)<<OC_BIT_SCALE;
  oc_mode_set_cost(_modec,_enc->lambda);
}

static void oc_cost_inter(oc_enc_ctx *_enc,oc_mode_choice *_modec,
 int _mbi,int _mb_mode,const signed char *_mv,int _qi){
  const unsigned char   *src;
  const unsigned char   *ref;
  int                    ystride;
  const ptrdiff_t       *frag_buf_offs;
  const oc_mb_map_plane *mb_map;
  const unsigned char   *map_idxs;
  int                    map_nidxs;
  int                    mapii;
  int                    mapi;
  int                    mv_offs[2];
  int                    dx;
  int                    dy;
  int                    pli;
  int                    bi;
  ptrdiff_t              fragi;
  ptrdiff_t              frag_offs;
  src=_enc->state.ref_frame_data[OC_FRAME_IO];
  ref=_enc->state.ref_frame_data[
   _enc->state.ref_frame_idx[OC_FRAME_FOR_MODE[_mb_mode]]];
  ystride=_enc->state.ref_ystride[0];
  frag_buf_offs=_enc->state.frag_buf_offs;
  mb_map=(const oc_mb_map_plane *)_enc->state.mb_maps[_mbi];
  dx=_mv[0];
  dy=_mv[1];
  _modec->rate=_modec->ssd=0;
  if(oc_state_get_mv_offsets(&_enc->state,mv_offs,0,dx,dy)>1){
    for(bi=0;bi<4;bi++){
      fragi=mb_map[0][bi];
      frag_offs=frag_buf_offs[fragi];
      oc_mode_dct_cost_accum(_modec,_qi,0,1,oc_enc_frag_satd2_thresh(_enc,
       src+frag_offs,ref+frag_offs+mv_offs[0],ref+frag_offs+mv_offs[1],ystride,
       UINT_MAX));
    }
  }
  else{
    for(bi=0;bi<4;bi++){
      fragi=mb_map[0][bi];
      frag_offs=frag_buf_offs[fragi];
      oc_mode_dct_cost_accum(_modec,_qi,0,1,
       oc_enc_frag_satd_thresh(_enc,src+frag_offs,
       ref+frag_offs+mv_offs[0],ystride,UINT_MAX));
    }
  }
  map_idxs=OC_MB_MAP_IDXS[_enc->state.info.pixel_fmt];
  map_nidxs=OC_MB_MAP_NIDXS[_enc->state.info.pixel_fmt];
  /*Note: This assumes ref_ystride[1]==ref_ystride[2].*/
  ystride=_enc->state.ref_ystride[1];
  if(oc_state_get_mv_offsets(&_enc->state,mv_offs,1,dx,dy)>1){
    for(mapii=4;mapii<map_nidxs;mapii++){
      mapi=map_idxs[mapii];
      pli=mapi>>2;
      bi=mapi&3;
      fragi=mb_map[pli][bi];
      frag_offs=frag_buf_offs[fragi];
      oc_mode_dct_cost_accum(_modec,_qi,pli,1,oc_enc_frag_satd2_thresh(_enc,
       src+frag_offs,ref+frag_offs+mv_offs[0],ref+frag_offs+mv_offs[1],ystride,
       UINT_MAX));
    }
  }
  else{
    for(mapii=4;mapii<map_nidxs;mapii++){
      mapi=map_idxs[mapii];
      pli=mapi>>2;
      bi=mapi&3;
      fragi=mb_map[pli][bi];
      frag_offs=frag_buf_offs[fragi];
      oc_mode_dct_cost_accum(_modec,_qi,pli,1,oc_enc_frag_satd_thresh(_enc,
       src+frag_offs,ref+frag_offs+mv_offs[0],ystride,UINT_MAX));
    }
  }
  _modec->overhead=
   oc_mode_scheme_chooser_cost(&_enc->chooser,_mb_mode)<<OC_BIT_SCALE;
  oc_mode_set_cost(_modec,_enc->lambda);
}

static void oc_cost_inter_nomv(oc_enc_ctx *_enc,oc_mode_choice *_modec,
 int _mbi,int _mb_mode,int _qi){
  static const oc_mv OC_MV_ZERO;
  oc_cost_inter(_enc,_modec,_mbi,_mb_mode,OC_MV_ZERO,_qi);
}

static int oc_cost_inter1mv(oc_enc_ctx *_enc,oc_mode_choice *_modec,
 int _mbi,int _mb_mode,const signed char *_mv,int _qi){
  int bits0;
  oc_cost_inter(_enc,_modec,_mbi,_mb_mode,_mv,_qi);
  bits0=OC_MV_BITS[0][_mv[0]+31]+OC_MV_BITS[0][_mv[1]+31];
  _modec->overhead+=OC_MINI(_enc->mv_bits[0]+bits0,_enc->mv_bits[1]+12)
   -OC_MINI(_enc->mv_bits[0],_enc->mv_bits[1])<<OC_BIT_SCALE;
  oc_mode_set_cost(_modec,_enc->lambda);
  return bits0;
}

static int oc_cost_inter4mv(oc_enc_ctx *_enc,oc_mode_choice *_modec,int _mbi,
 oc_mv _mv[4],int _qi){
  oc_mv                  cbmvs[4];
  const unsigned char   *src;
  const unsigned char   *ref;
  int                    ystride;
  const ptrdiff_t       *frag_buf_offs;
  oc_mv                 *frag_mvs;
  const oc_mb_map_plane *mb_map;
  const unsigned char   *map_idxs;
  int                    map_nidxs;
  int                    mapii;
  int                    mapi;
  int                    mv_offs[2];
  int                    dx;
  int                    dy;
  int                    pli;
  int                    bi;
  ptrdiff_t              fragi;
  ptrdiff_t              frag_offs;
  int                    bits0;
  unsigned               satd;
  src=_enc->state.ref_frame_data[OC_FRAME_IO];
  ref=_enc->state.ref_frame_data[_enc->state.ref_frame_idx[OC_FRAME_PREV]];
  ystride=_enc->state.ref_ystride[0];
  frag_buf_offs=_enc->state.frag_buf_offs;
  frag_mvs=_enc->state.frag_mvs;
  mb_map=(const oc_mb_map_plane *)_enc->state.mb_maps[_mbi];
  _modec->rate=_modec->ssd=0;
  bits0=0;
  for(bi=0;bi<4;bi++){
    fragi=mb_map[0][bi];
    dx=_mv[bi][0];
    dy=_mv[bi][1];
    /*Save the block MVs as the current ones while we're here; we'll replace
       them if we don't ultimately choose 4MV mode.*/
    frag_mvs[fragi][0]=(signed char)dx;
    frag_mvs[fragi][1]=(signed char)dy;
    frag_offs=frag_buf_offs[fragi];
    bits0+=OC_MV_BITS[0][dx+31]+OC_MV_BITS[0][dy+31];
    if(oc_state_get_mv_offsets(&_enc->state,mv_offs,0,dx,dy)>1){
      satd=oc_enc_frag_satd2_thresh(_enc,src+frag_offs,
       ref+frag_offs+mv_offs[0],ref+frag_offs+mv_offs[1],ystride,UINT_MAX);
    }
    else{
      satd=oc_enc_frag_satd_thresh(_enc,src+frag_offs,
       ref+frag_offs+mv_offs[0],ystride,UINT_MAX);
    }
    oc_mode_dct_cost_accum(_modec,_qi,0,1,satd);
  }
  (*OC_SET_CHROMA_MVS_TABLE[_enc->state.info.pixel_fmt])(cbmvs,
   (const oc_mv *)_mv);
  map_idxs=OC_MB_MAP_IDXS[_enc->state.info.pixel_fmt];
  map_nidxs=OC_MB_MAP_NIDXS[_enc->state.info.pixel_fmt];
  /*Note: This assumes ref_ystride[1]==ref_ystride[2].*/
  ystride=_enc->state.ref_ystride[1];
  for(mapii=4;mapii<map_nidxs;mapii++){
    mapi=map_idxs[mapii];
    pli=mapi>>2;
    bi=mapi&3;
    fragi=mb_map[pli][bi];
    dx=cbmvs[bi][0];
    dy=cbmvs[bi][1];
    frag_offs=frag_buf_offs[fragi];
    /*TODO: We could save half these calls by re-using the results for the Cb
       and Cr planes; is it worth it?*/
    if(oc_state_get_mv_offsets(&_enc->state,mv_offs,pli,dx,dy)>1){
      satd=oc_enc_frag_satd2_thresh(_enc,src+frag_offs,
       ref+frag_offs+mv_offs[0],ref+frag_offs+mv_offs[1],ystride,UINT_MAX);
    }
    else{
      satd=oc_enc_frag_satd_thresh(_enc,src+frag_offs,
       ref+frag_offs+mv_offs[0],ystride,UINT_MAX);
    }
    oc_mode_dct_cost_accum(_modec,_qi,pli,1,satd);
  }
  _modec->overhead=oc_mode_scheme_chooser_cost(&_enc->chooser,
   OC_MODE_INTER_MV_FOUR)+OC_MINI(_enc->mv_bits[0]+bits0,_enc->mv_bits[1]+48)
   -OC_MINI(_enc->mv_bits[0],_enc->mv_bits[1])<<OC_BIT_SCALE;
  oc_mode_set_cost(_modec,_enc->lambda);
  return bits0;
}

int oc_enc_analyze(oc_enc_ctx *_enc,int _frame_type,int _recode){
  oc_set_chroma_mvs_func  set_chroma_mvs;
  oc_mcenc_ctx            mcenc;
  oc_plane_state          ps;
  oc_frag_state           fr;
  oc_mv                   last_mv;
  oc_mv                   prior_mv;
  ogg_int64_t             interbits;
  ogg_int64_t             intrabits;
  const unsigned char    *map_idxs;
  int                     nmap_idxs;
  unsigned               *coded_mbis;
  unsigned               *uncoded_mbis;
  size_t                  ncoded_mbis;
  size_t                  nuncoded_mbis;
  oc_sb_flags            *sb_flags;
  signed char            *mb_modes;
  const oc_mb_map        *mb_maps;
  oc_mb_enc_info         *embs;
  oc_fragment            *frags;
  oc_mv                  *frag_mvs;
  int                     qi;
  unsigned                sbi;
  unsigned                sbi_end;
  int                     pli;
  set_chroma_mvs=OC_SET_CHROMA_MVS_TABLE[_enc->state.info.pixel_fmt];
  _enc->state.frame_type=_frame_type;
  if(!_recode)oc_mcenc_start(_enc,&mcenc);
  oc_fr_state_init(&fr);
  oc_mode_scheme_chooser_reset(&_enc->chooser);
  oc_enc_tokenize_start(_enc);
  oc_plane_state_plane_setup(_enc,&ps,0);
  _enc->mv_bits[0]=_enc->mv_bits[1]=0;
  interbits=intrabits=0;
  last_mv[0]=last_mv[1]=prior_mv[0]=prior_mv[1]=0;
  /*Choose MVs and MB modes and quantize and code luma.
    Must be done in Hilbert order.*/
  map_idxs=OC_MB_MAP_IDXS[_enc->state.info.pixel_fmt];
  nmap_idxs=OC_MB_MAP_NIDXS[_enc->state.info.pixel_fmt];
  qi=_enc->state.qis[0];
  coded_mbis=_enc->coded_mbis;
  uncoded_mbis=coded_mbis+_enc->state.nmbs;
  ncoded_mbis=0;
  nuncoded_mbis=0;
  _enc->state.ncoded_fragis[0]=0;
  _enc->state.ncoded_fragis[1]=0;
  _enc->state.ncoded_fragis[2]=0;
  sb_flags=_enc->state.sb_flags;
  mb_modes=_enc->state.mb_modes;
  mb_maps=(const oc_mb_map *)_enc->state.mb_maps;
  embs=_enc->mb_info;
  frags=_enc->state.frags;
  frag_mvs=_enc->state.frag_mvs;
  sbi_end=_enc->state.fplanes[0].nsbs;
  for(sbi=0;sbi<sbi_end;sbi++){
    int quadi;
    /*Mode addressing is through Y plane, always 4 MB per SB.*/
    for(quadi=0;quadi<4;quadi++)if(sb_flags[sbi].quad_valid&1<<quadi){
      unsigned  mbi;
      int       mb_mode;
      int       dx;
      int       dy;
      int       mapii;
      int       mapi;
      int       bi;
      ptrdiff_t fragi;
      mbi=sbi<<2|quadi;
      if(!_recode&&_enc->state.curframe_num>0){
        /*Motion estimation:
          We always do a basic 1MV search for all macroblocks, coded or not,
           keyframe or not.*/
        /*Move the motion vector predictors back a frame.*/
        memmove(embs[mbi].analysis_mv+1,
         embs[mbi].analysis_mv,2*sizeof(embs[mbi].analysis_mv[0]));
        /*Search the last frame.*/
        oc_mcenc_search(_enc,&mcenc,mbi,OC_FRAME_PREV);
        /*Search the golden frame.*/
        oc_mcenc_search(_enc,&mcenc,mbi,OC_FRAME_GOLD);
      }
      dx=dy=0;
      if(_enc->state.frame_type==OC_INTRA_FRAME){
        mb_modes[mbi]=mb_mode=OC_MODE_INTRA;
        oc_enc_mb_transform_quantize_luma(_enc,&ps,mbi,0,&fr);
      }
      else{
        oc_mode_choice modes[8];
        int            mb_mv_bits_0;
        int            mb_gmv_bits_0;
        int            mb_4mv_bits_0;
        int            mb_4mv_bits_1;
        int            inter_mv_pref;
        /*Find the block choice with the lowest estimated coding cost.
          If a Cb or Cr block is coded but no Y' block from a macro block then
           the mode MUST be OC_MODE_INTER_NOMV.
          This is the default state to which the mode data structure is
           initialised in encoder and decoder at the start of each frame.*/
        /*Block coding cost is estimated from correlated SATD metrics.*/
        /*At this point, all blocks that are in frame are still marked coded.*/
        if(!_recode){
          memcpy(embs[mbi].unref_mv,
           embs[mbi].analysis_mv[0],sizeof(embs[mbi].unref_mv));
          embs[mbi].refined=0;
        }
        oc_cost_inter_nomv(_enc,modes+OC_MODE_INTER_NOMV,mbi,OC_MODE_INTER_NOMV,qi);
        oc_cost_intra(_enc,modes+OC_MODE_INTRA,mbi,qi);
        intrabits+=modes[OC_MODE_INTRA].rate;
        mb_mv_bits_0=oc_cost_inter1mv(_enc,modes+OC_MODE_INTER_MV,mbi,
         OC_MODE_INTER_MV,embs[mbi].unref_mv[OC_FRAME_PREV],qi);
        oc_cost_inter(_enc,modes+OC_MODE_INTER_MV_LAST,mbi,
         OC_MODE_INTER_MV_LAST,last_mv,qi);
        oc_cost_inter(_enc,modes+OC_MODE_INTER_MV_LAST2,mbi,
         OC_MODE_INTER_MV_LAST2,prior_mv,qi);
        oc_cost_inter_nomv(_enc,modes+OC_MODE_GOLDEN_NOMV,mbi,
         OC_MODE_GOLDEN_NOMV,qi);
        mb_gmv_bits_0=oc_cost_inter1mv(_enc,modes+OC_MODE_GOLDEN_MV,mbi,
         OC_MODE_GOLDEN_MV,embs[mbi].unref_mv[OC_FRAME_GOLD],qi);
        mb_4mv_bits_0=oc_cost_inter4mv(_enc,modes+OC_MODE_INTER_MV_FOUR,mbi,
         embs[mbi].block_mv,qi);
        mb_4mv_bits_1=48;
        /*The explicit MV modes (2,6,7) have not yet gone through halfpel
           refinement.
          We choose the explicit MV mode that's already furthest ahead on bits
           and refine only that one.
          We have to be careful to remember which ones we've refined so that
           we don't refine it again if we re-encode this frame.*/
        inter_mv_pref=_enc->lambda*3<<OC_BIT_SCALE;
        if(modes[OC_MODE_INTER_MV_FOUR].cost<modes[OC_MODE_INTER_MV].cost&&
         modes[OC_MODE_INTER_MV_FOUR].cost<modes[OC_MODE_GOLDEN_MV].cost){
          if(!(embs[mbi].refined&0x80)){
            oc_mcenc_refine4mv(_enc,mbi);
            embs[mbi].refined|=0x80;
          }
          mb_4mv_bits_0=oc_cost_inter4mv(_enc,modes+OC_MODE_INTER_MV_FOUR,mbi,
           embs[mbi].ref_mv,qi);
        }
        else if(modes[OC_MODE_GOLDEN_MV].cost+inter_mv_pref<
         modes[OC_MODE_INTER_MV].cost){
          if(!(embs[mbi].refined&0x40)){
            oc_mcenc_refine1mv(_enc,mbi,OC_FRAME_GOLD);
            embs[mbi].refined|=0x40;
          }
          mb_gmv_bits_0=oc_cost_inter1mv(_enc,modes+OC_MODE_GOLDEN_MV,mbi,
           OC_MODE_GOLDEN_MV,embs[mbi].analysis_mv[0][OC_FRAME_GOLD],qi);
        }
        if(!(embs[mbi].refined&0x04)){
          oc_mcenc_refine1mv(_enc,mbi,OC_FRAME_PREV);
          embs[mbi].refined|=0x04;
        }
        mb_mv_bits_0=oc_cost_inter1mv(_enc,modes+OC_MODE_INTER_MV,mbi,
         OC_MODE_INTER_MV,embs[mbi].analysis_mv[0][OC_FRAME_PREV],qi);
        /*Finally, pick the mode with the cheapest estimated bit cost.*/
        mb_mode=0;
        if(modes[1].cost<modes[0].cost)mb_mode=1;
        if(modes[3].cost<modes[mb_mode].cost)mb_mode=3;
        if(modes[4].cost<modes[mb_mode].cost)mb_mode=4;
        if(modes[5].cost<modes[mb_mode].cost)mb_mode=5;
        if(modes[6].cost<modes[mb_mode].cost)mb_mode=6;
        if(modes[7].cost<modes[mb_mode].cost)mb_mode=7;
        /*We prefer OC_MODE_INTER_MV, but not over LAST and LAST2.*/
        if(mb_mode==OC_MODE_INTER_MV_LAST||mb_mode==OC_MODE_INTER_MV_LAST2){
          inter_mv_pref=0;
        }
        if(modes[2].cost<modes[mb_mode].cost+inter_mv_pref)mb_mode=2;
        mb_modes[mbi]=mb_mode;
        /*Propagate the MVs to the luma blocks.*/
        if(mb_mode!=OC_MODE_INTER_MV_FOUR){
          switch(mb_mode){
            case OC_MODE_INTER_MV:{
              dx=embs[mbi].analysis_mv[0][OC_FRAME_PREV][0];
              dy=embs[mbi].analysis_mv[0][OC_FRAME_PREV][1];
            }break;
            case OC_MODE_INTER_MV_LAST:{
              dx=last_mv[0];
              dy=last_mv[1];
            }break;
            case OC_MODE_INTER_MV_LAST2:{
              dx=prior_mv[0];
              dy=prior_mv[1];
            }break;
            case OC_MODE_GOLDEN_MV:{
              dx=embs[mbi].analysis_mv[0][OC_FRAME_GOLD][0];
              dy=embs[mbi].analysis_mv[0][OC_FRAME_GOLD][1];
            }break;
          }
          for(bi=0;bi<4;bi++){
            fragi=mb_maps[mbi][0][bi];
            frag_mvs[fragi][0]=(signed char)dx;
            frag_mvs[fragi][1]=(signed char)dy;
          }
        }
        if(oc_enc_mb_transform_quantize_luma(_enc,&ps,mbi,
         modes[mb_mode].overhead,&fr)>0){
          int orig_mb_mode;
          orig_mb_mode=mb_mode;
          mb_mode=mb_modes[mbi];
          switch(mb_mode){
            case OC_MODE_INTER_MV:{
              memcpy(prior_mv,last_mv,sizeof(prior_mv));
              /*If we're backing out from 4MV, find the MV we're actually
                 using.*/
              if(orig_mb_mode==OC_MODE_INTER_MV_FOUR){
                for(bi=0;;bi++){
                  fragi=mb_maps[mbi][0][bi];
                  if(frags[fragi].coded){
                    memcpy(last_mv,frag_mvs[fragi],sizeof(last_mv));
                    dx=frag_mvs[fragi][0];
                    dy=frag_mvs[fragi][1];
                    break;
                  }
                }
                mb_mv_bits_0=OC_MV_BITS[0][dx+31]+OC_MV_BITS[0][dy+31];
              }
              /*Otherwise we used the original analysis MV.*/
              else{
                memcpy(last_mv,
                 embs[mbi].analysis_mv[0][OC_FRAME_PREV],sizeof(last_mv));
              }
              _enc->mv_bits[0]+=mb_mv_bits_0;
              _enc->mv_bits[1]+=12;
            }break;
            case OC_MODE_INTER_MV_LAST2:{
              oc_mv tmp_mv;
              memcpy(tmp_mv,prior_mv,sizeof(tmp_mv));
              memcpy(prior_mv,last_mv,sizeof(prior_mv));
              memcpy(last_mv,tmp_mv,sizeof(last_mv));
            }break;
            case OC_MODE_GOLDEN_MV:{
              _enc->mv_bits[0]+=mb_gmv_bits_0;
              _enc->mv_bits[1]+=12;
            }break;
            case OC_MODE_INTER_MV_FOUR:{
              oc_mv lbmvs[4];
              oc_mv cbmvs[4];
              memcpy(prior_mv,last_mv,sizeof(prior_mv));
              for(bi=0;bi<4;bi++){
                fragi=mb_maps[mbi][0][bi];
                if(frags[fragi].coded){
                  memcpy(last_mv,frag_mvs[fragi],sizeof(last_mv));
                  memcpy(lbmvs[bi],frag_mvs[fragi],sizeof(lbmvs[bi]));
                  _enc->mv_bits[0]+=OC_MV_BITS[0][frag_mvs[fragi][0]+31]
                   +OC_MV_BITS[0][frag_mvs[fragi][1]+31];
                  _enc->mv_bits[1]+=12;
                }
                /*Replace the block MVs for not-coded blocks with (0,0).*/
                else memset(lbmvs[bi],0,sizeof(lbmvs[bi]));
              }
              (*set_chroma_mvs)(cbmvs,(const oc_mv *)lbmvs);
              for(mapii=4;mapii<nmap_idxs;mapii++){
                mapi=map_idxs[mapii];
                pli=mapi>>2;
                bi=mapi&3;
                fragi=mb_maps[mbi][pli][bi];
                frags[fragi].mb_mode=mb_mode;
                memcpy(frag_mvs[fragi],cbmvs[bi],sizeof(frag_mvs[fragi]));
              }
            }break;
          }
          coded_mbis[ncoded_mbis++]=mbi;
          oc_mode_scheme_chooser_update(&_enc->chooser,mb_mode);
          interbits+=modes[mb_mode].rate+modes[mb_mode].overhead;
        }
        else{
          *(uncoded_mbis-++nuncoded_mbis)=mbi;
          mb_mode=OC_MODE_INTER_NOMV;
          dx=dy=0;
        }
      }
      /*Propagate final MB mode and MVs to the chroma blocks.
        This has already been done for 4MV mode, since it requires individual
         block motion vectors.*/
      if(mb_mode!=OC_MODE_INTER_MV_FOUR){
        for(mapii=4;mapii<nmap_idxs;mapii++){
          mapi=map_idxs[mapii];
          pli=mapi>>2;
          bi=mapi&3;
          fragi=mb_maps[mbi][pli][bi];
          frags[fragi].mb_mode=mb_mode;
          frag_mvs[fragi][0]=(signed char)dx;
          frag_mvs[fragi][1]=(signed char)dy;
        }
      }
    }
    oc_fr_finish_sb(&fr);
    sb_flags[sbi].coded_fully=fr.sb_full_last;
    sb_flags[sbi].coded_partially=fr.sb_partial_last;
  }
  /*Code Cb plane.*/
  oc_plane_state_plane_setup(_enc,&ps,1);
  sbi=sbi_end;
  sbi_end=sbi+_enc->state.fplanes[1].nsbs;
  oc_enc_sb_transform_quantize_chroma(_enc,&ps,sbi,sbi_end,&fr);
  /*Code Cr plane.*/
  oc_plane_state_plane_setup(_enc,&ps,2);
  sbi=sbi_end;
  sbi_end=sbi+_enc->state.fplanes[2].nsbs;
  oc_enc_sb_transform_quantize_chroma(_enc,&ps,sbi,sbi_end,&fr);
  if(_enc->state.frame_type!=OC_INTRA_FRAME){
    if(interbits>intrabits)return 1;
    /*Finish adding flagging overhead costs to inter bit counts.*/
    oc_fr_flush(&fr);
    interbits+=fr.bits<<OC_BIT_SCALE;
    interbits+=OC_MINI(_enc->mv_bits[0],_enc->mv_bits[1])<<OC_BIT_SCALE;
    interbits+=
     _enc->chooser.scheme_bits[_enc->chooser.scheme_list[0]]<<OC_BIT_SCALE;
    if(interbits>intrabits)return 1;
  }
  _enc->ncoded_mbis=ncoded_mbis;
  /*Compact the coded fragment list.*/
  {
    ptrdiff_t ncoded_fragis;
    ncoded_fragis=_enc->state.ncoded_fragis[0];
    for(pli=1;pli<3;pli++){
      memmove(_enc->state.coded_fragis+ncoded_fragis,
       _enc->state.coded_fragis+_enc->state.fplanes[pli].froffset,
       _enc->state.ncoded_fragis[pli]*sizeof(*_enc->state.coded_fragis));
      ncoded_fragis+=_enc->state.ncoded_fragis[pli];
    }
    _enc->state.ntotal_coded_fragis=ncoded_fragis;
  }
  return 0;
}

#if defined(OC_COLLECT_METRICS)
# include <stdio.h>
# include <math.h>

# define OC_ZWEIGHT   (0.25)
# define OC_BIN(_satd) (OC_MINI((_satd)>>OC_SAD_SHIFT,OC_SAD_BINS-1))

static void oc_mode_metrics_add(oc_mode_metrics *_metrics,
 double _w,int _satd,int _rate,double _rmse){
  double rate;
  /*Accumulate statistics without the scaling; this lets us change the scale
     factor yet still use old data.*/
  rate=ldexp(_rate,-OC_BIT_SCALE);
  if(_metrics->fragw>0){
    double dsatd;
    double drate;
    double drmse;
    double w;
    dsatd=_satd-_metrics->satd/_metrics->fragw;
    drate=rate-_metrics->rate/_metrics->fragw;
    drmse=_rmse-_metrics->rmse/_metrics->fragw;
    w=_metrics->fragw*_w/(_metrics->fragw+_w);
    _metrics->satd2+=dsatd*dsatd*w;
    _metrics->satdrate+=dsatd*drate*w;
    _metrics->rate2+=drate*drate*w;
    _metrics->satdrmse+=dsatd*drmse*w;
    _metrics->rmse2+=drmse*drmse*w;
  }
  _metrics->fragw+=_w;
  _metrics->satd+=_satd*_w;
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
    double dsatd;
    double drate;
    double drmse;
    double w;
    wa=_dst->fragw;
    wb=_src[i].fragw;
    dsatd=_src[i].satd/wb-_dst->satd/wa;
    drate=_src[i].rate/wb-_dst->rate/wa;
    drmse=_src[i].rmse/wb-_dst->rmse/wa;
    w=wa*wb/(wa+wb);
    _dst->fragw+=_src[i].fragw;
    _dst->satd+=_src[i].satd;
    _dst->rate+=_src[i].rate;
    _dst->rmse+=_src[i].rmse;
    _dst->satd2+=_src[i].satd2+dsatd*dsatd*w;
    _dst->satdrate+=_src[i].satdrate+dsatd*drate*w;
    _dst->rate2+=_src[i].rate2+drate*drate*w;
    _dst->satdrmse+=_src[i].satdrmse+dsatd*drmse*w;
    _dst->rmse2+=_src[i].rmse2+drmse*drmse*w;
  }
}

static void oc_enc_mode_metrics_update(oc_enc_ctx *_enc,int _qi){
  int pli;
  int qti;
  oc_restore_fpu(&_enc->state);
  /*Compile collected SATD/rate/RMSE metrics into a form that's immediately
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
        if(metrics.fragw>0&&metrics.satd2>0){
          double a;
          double b;
          double msatd;
          double mrate;
          double mrmse;
          double rate;
          double rmse;
          msatd=metrics.satd/metrics.fragw;
          mrate=metrics.rate/metrics.fragw;
          mrmse=metrics.rmse/metrics.fragw;
          /*Compute the points on these lines corresponding to the actual bin
             value.*/
          b=metrics.satdrate/metrics.satd2;
          a=mrate-b*msatd;
          rate=ldexp(a+b*(bin<<OC_SAD_SHIFT),OC_BIT_SCALE);
          OC_MODE_RD[_qi][pli][qti][bin].rate=
           (ogg_int16_t)OC_CLAMPI(-32768,(int)(rate+0.5),32767);
          b=metrics.satdrmse/metrics.satd2;
          a=mrmse-b*msatd;
          rmse=ldexp(a+b*(bin<<OC_SAD_SHIFT),OC_RMSE_SCALE);
          OC_MODE_RD[_qi][pli][qti][bin].rmse=
           (ogg_int16_t)OC_CLAMPI(-32768,(int)(rmse+0.5),32767);
        }
      }
    }
  }
}


static void ModeMetricsGroup(oc_enc_ctx *_enc, int group, int huffY, int huffC, int eobcounts[64], int *actual_bits){
  int       *stack;
  ptrdiff_t *tfi;
  int        ty;
  int        tn;
  int        ti;
  stack=_enc->dct_eob_fi_stack[group];
  tfi=_enc->dct_token_frag[group];
  ty=_enc->dct_token_ycount[group];
  tn=_enc->dct_token_count[group];
  for(ti=0;ti<tn;ti++){
    ptrdiff_t fragi;
    int       token;
    int       bits;
    token=_enc->dct_token[group][ti];
    bits=_enc->huff_codes[ti<ty?huffY:huffC][token].nbits
     +OC_DCT_TOKEN_EXTRA_BITS[token];
    /*Not an EOB run; this token belongs to a single fragment.*/
    if(token>=OC_NDCT_EOB_TOKEN_MAX)fragi=tfi[ti];
    else{
      int run;
      int fragi;
      run=-oc_dct_token_skip(token,_enc->dct_token_eb[group][ti]);
      fragi=stack[eobcounts[group]];
      /*Tokens follow EOB so it must be entirely contained within this
         plane/group.*/
      if(ti+1<tn)eobcounts[group]+=run;
      /*EOB is the last token in this plane/group, so it may span into the
         next plane/group.*/
      else{
        int n;
        n=_enc->dct_eob_fi_count[group];
        while(run){
          int rem;
          rem=n-eobcounts[group];
          if(rem>run)rem=run;
          eobcounts[group]+=rem;
          run-=rem;
          if(run){
            group++;
            n=_enc->dct_eob_fi_count[group];
            stack=_enc->dct_eob_fi_stack[group];
          }
        }
      }
    }
    actual_bits[fragi]+=bits<<OC_BIT_SCALE;
  }
}

/*TODO: This code has bitrotted and needs to be re-written.*/
void ModeMetrics(oc_enc_ctx *_enc){
  int actual_bits[_enc->frag_total];
  int          eobcounts[64];
  int          huff[4];
  oc_fragment *frags;
  int         *sp;
  int         *mp;
  double       fragw;
  int          pli;
  int          qti;
  int          qi;
  int          zzi;
  ptrdiff_t    fragi;
  qti=_enc->state.frame_type;
  frags=_enc->state.frags;
  sp=_enc->frag_satd;
  mp=_enc->frag_mbi;
  oc_restore_fpu(&_enc->state);
  /*Weight the fragments by the inverse frame size; this prevents HD content
     from dominating the statistics.*/
  memset(actual_bits,0,sizeof(actual_bits));
  memset(eobcounts,0,sizeof(eobcounts));
  huff[0]=_enc->huff_idxs[qti][0][0];
  huff[1]=_enc->huff_idxs[qti][0][1];
  huff[2]=_enc->huff_idxs[qti][1][0];
  huff[3]=_enc->huff_idxs[qti][1][1];
  memset(_enc->dist_dist,0,sizeof(_enc->dist_dist));
  memset(_enc->dist_bits,0,sizeof(_enc->dist_bits));
  if(!oc_has_mode_metrics){
    FILE *fmetrics;
    memset(OC_MODE_METRICS,0,sizeof(OC_MODE_METRICS));
    fmetrics=fopen("modedec.stats","rb");
    if(fmetrics!=NULL){
      fread(OC_MODE_METRICS,sizeof(OC_MODE_METRICS),1,fmetrics);
      fclose(fmetrics);
    }
    for(qi=0;qi<64;qi++)oc_enc_mode_metrics_update(_enc,qi);
    oc_has_mode_metrics=1;
  }
  /*Count bits for tokens.*/
  ModeMetricsGroup(_enc, 0, huff[0], huff[1], eobcounts, actual_bits);
  for(zzi=1;zzi<6;zzi++)
    ModeMetricsGroup(_enc, zzi,  huff[2]+16, huff[3]+16, eobcounts, actual_bits);
  for(;zzi<15;zzi++)
    ModeMetricsGroup(_enc, zzi, huff[2]+32, huff[3]+32, eobcounts, actual_bits);
  for(;zzi<28;zzi++)
    ModeMetricsGroup(_enc, zzi, huff[2]+48, huff[3]+48, eobcounts, actual_bits);
  for(;zzi<64;zzi++)
    ModeMetricsGroup(_enc, zzi, huff[2]+64, huff[3]+64, eobcounts, actual_bits);
  /*Accumulate.*/
  fragw=1.0/_enc->state.nfrags;
  qi=_enc->state.qis[0];
  for(pli=0;pli<3;pli++){
    ptrdiff_t fragi_end;
    fragi=_enc->state.fplanes[pli].froffset;
    fragi_end=fragi+_enc->state.fplanes[pli].nfrags;
    for(;fragi<fragi_end;fragi++)if(frags[fragi].coded){
      int mbi;
      int mb_mode;
      int bin;
      mbi=mp[fragi];
      mb_mode=_enc->state.mb_modes[mbi];
      bin=OC_BIN(sp[fragi]);
      oc_mode_metrics_add(OC_MODE_METRICS[qi][pli][mb_mode!=OC_MODE_INTRA]+bin,
       fragw,sp[fragi],actual_bits[fragi],sqrt(_enc->frag_ssd[fragi]));
    }
  }
  /*Update global SAD/rate estimation matrix.*/
  oc_enc_mode_metrics_update(_enc,qi);
}

void oc_enc_mode_metrics_dump(oc_enc_ctx *_enc){
  FILE *fmetrics;
  int   qi;
  /*Generate sample points for complete list of QI values.*/
  for(qi=0;qi<64;qi++)oc_enc_mode_metrics_update(_enc,qi);
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
   "  double satd;\n"
   "  double rate;\n"
   "  double rmse;\n"
   "  double satd2;\n"
   "  double satdrate;\n"
   "  double rate2;\n"
   "  double satdrmse;\n"
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
