/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2007                *
 * by the Xiph.Org Foundation and contributors http://www.xiph.org/ *
 *                                                                  *
 ********************************************************************

  function:
    last mod: $Id$

 ********************************************************************/

/*MMX acceleration of complete fragment reconstruction algorithm.
  Originally written by Rudolf Marek.*/
#include <string.h>
#include "x86int.h"
#include "mmxfrag.h"
#include "mmxloop.h"

#if defined(OC_X86_ASM)

void oc_state_frag_recon_mmx(const oc_theora_state *_state,oc_fragment *_frag,
 int _pli,ogg_int16_t _dct_coeffs[128],int _last_zzi,int _ncoefs,
 ogg_uint16_t _dc_quant,const ogg_uint16_t _ac_quant[64]){
  ogg_int16_t  __attribute__((aligned(8))) res_buf[64];
  int dst_framei;
  int ystride;
  /*Dequantize and apply the inverse transform.*/
  oc_dequant_idct8x8_mmx(res_buf,_dct_coeffs,
   _last_zzi,_ncoefs,_dc_quant,_ac_quant);
  /*Fill in the target buffer.*/
  dst_framei=_state->ref_frame_idx[OC_FRAME_SELF];
  ystride=_state->ref_frame_bufs[dst_framei][_pli].stride;
  /*For now ystride values in all ref frames assumed to be equal.*/
  if(_frag->mbmode==OC_MODE_INTRA){
    oc_frag_recon_intra_mmx(_frag->buffer[dst_framei],ystride,res_buf);
  }
  else{
    int ref_framei;
    int mvoffsets[2];
    ref_framei=_state->ref_frame_idx[OC_FRAME_FOR_MODE[_frag->mbmode]];
    if(oc_state_get_mv_offsets(_state,mvoffsets,
     _frag->mv[0],_frag->mv[1],ystride,_pli)>1){
      oc_frag_recon_inter2_mmx(_frag->buffer[dst_framei],
       _frag->buffer[ref_framei]+mvoffsets[0],
       _frag->buffer[ref_framei]+mvoffsets[1],ystride,res_buf);
    }
    else{
      oc_frag_recon_inter_mmx(_frag->buffer[dst_framei],
       _frag->buffer[ref_framei]+mvoffsets[0],ystride,res_buf);
    }
  }
}

/*We copy these entire function to inline the actual MMX routines so that we
   use only a single indirect call.*/

/*Copies the fragments specified by the lists of fragment indices from one
   frame to another.
  _fragis:    A pointer to a list of fragment indices.
  _nfragis:   The number of fragment indices to copy.
  _dst_frame: The reference frame to copy to.
  _src_frame: The reference frame to copy from.
  _pli:       The color plane the fragments lie in.*/
void oc_state_frag_copy_list_mmx(const oc_theora_state *_state,
 const int *_fragis,int _nfragis,int _dst_frame,int _src_frame,int _pli){
  const int *fragi;
  const int *fragi_end;
  int        dst_framei;
  int        src_framei;
  int        ystride;
  dst_framei=_state->ref_frame_idx[_dst_frame];
  src_framei=_state->ref_frame_idx[_src_frame];
  ystride=_state->ref_frame_bufs[dst_framei][_pli].stride;
  fragi_end=_fragis+_nfragis;
  for(fragi=_fragis;fragi<fragi_end;fragi++){
    oc_fragment *frag;
    frag=_state->frags+*fragi;
    OC_FRAG_COPY_MMX(frag->buffer[dst_framei],frag->buffer[src_framei],ystride);
  }
}

/*Apply the loop filter to a given set of fragment rows in the given plane.
  The filter may be run on the bottom edge, affecting pixels in the next row of
   fragments, so this row also needs to be available.
  _bv:        The bounding values array.
  _refi:      The index of the frame buffer to filter.
  _pli:       The color plane to filter.
  _fragy0:    The Y coordinate of the first fragment row to filter.
  _fragy_end: The Y coordinate of the fragment row to stop filtering at.*/
void oc_state_loop_filter_frag_rows_mmx(const oc_theora_state *_state,int *_bv,
 int _refi,int _pli,int _fragy0,int _fragy_end){
  unsigned char OC_ALIGN8  ll[8];
  const th_img_plane      *iplane;
  const oc_fragment_plane *fplane;
  oc_fragment             *frag_top;
  oc_fragment             *frag0;
  oc_fragment             *frag;
  oc_fragment             *frag_end;
  oc_fragment             *frag0_end;
  oc_fragment             *frag_bot;
  memset(ll,_state->loop_filter_limits[_state->qis[0]],sizeof(ll));
  iplane=_state->ref_frame_bufs[_refi]+_pli;
  fplane=_state->fplanes+_pli;
  /*The following loops are constructed somewhat non-intuitively on purpose.
    The main idea is: if a block boundary has at least one coded fragment on
     it, the filter is applied to it.
    However, the order that the filters are applied in matters, and VP3 chose
     the somewhat strange ordering used below.*/
  frag_top=_state->frags+fplane->froffset;
  frag0=frag_top+_fragy0*fplane->nhfrags;
  frag0_end=frag0+(_fragy_end-_fragy0)*fplane->nhfrags;
  frag_bot=_state->frags+fplane->froffset+fplane->nfrags;
  while(frag0<frag0_end){
    frag=frag0;
    frag_end=frag+fplane->nhfrags;
    while(frag<frag_end){
      if(frag->coded){
        if(frag>frag0){
          OC_LOOP_FILTER_H_MMX(frag->buffer[_refi],iplane->stride,ll);
        }
        if(frag0>frag_top){
          OC_LOOP_FILTER_V_MMX(frag->buffer[_refi],iplane->stride,ll);
        }
        if(frag+1<frag_end&&!(frag+1)->coded){
          OC_LOOP_FILTER_H_MMX(frag->buffer[_refi]+8,iplane->stride,ll);
        }
        if(frag+fplane->nhfrags<frag_bot&&!(frag+fplane->nhfrags)->coded){
          OC_LOOP_FILTER_V_MMX((frag+fplane->nhfrags)->buffer[_refi],
           iplane->stride,ll);
        }
      }
      frag++;
    }
    frag0+=fplane->nhfrags;
  }
}

#endif
