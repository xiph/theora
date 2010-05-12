/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2009                *
 * by the Xiph.Org Foundation and contributors http://www.xiph.org/ *
 *                                                                  *
 ********************************************************************

  function:
    last mod: $Id: $

 ********************************************************************/

#include "ARMint.h"

#if defined(OC_ARM_ASM)

void oc_frag_copy_list_arm(unsigned char *dst_frame,
                           const unsigned char *src_frame,
                           int ystride,
                           ptrdiff_t _nfragis,
                           const ptrdiff_t *_fragis,
                           const ptrdiff_t *frag_buf_offs);

static void oc_state_frag_copy_list_arm(const oc_theora_state *_state,
 const ptrdiff_t *_fragis,ptrdiff_t _nfragis,
 int _dst_frame,int _src_frame,int _pli){
  const ptrdiff_t     *frag_buf_offs;
  const unsigned char *src_frame_data;
  unsigned char       *dst_frame_data;
  ptrdiff_t            fragii;
  int                  ystride;
  oc_frag_copy_list_arm(
                   _state->ref_frame_data[_state->ref_frame_idx[_dst_frame]],
                   _state->ref_frame_data[_state->ref_frame_idx[_src_frame]],
                   _state->ref_ystride[_pli],
                   _nfragis,
                   frag_bug_offs);
}

static void oc_state_frag_recon_arm(const oc_theora_state *_state,ptrdiff_t _fragi,
 int _pli,ogg_int16_t _dct_coeffs[64],int _last_zzi,ogg_uint16_t _dc_quant){
  unsigned char *dst;
  ptrdiff_t      frag_buf_off;
  int            ystride;
  int            mb_mode;
  /*Apply the inverse transform.*/
  /*Special case only having a DC component.*/
  if(_last_zzi<2){
    ogg_int16_t p;
    int         ci;
    /*We round this dequant product (and not any of the others) because there's
       no iDCT rounding.*/
    p=(ogg_int16_t)(_dct_coeffs[0]*(ogg_int32_t)_dc_quant+15>>5);
    /*LOOP VECTORIZES.*/
    for(ci=0;ci<64;ci++)_dct_coeffs[ci]=p;
  }
  else{
    /*First, dequantize the DC coefficient.*/
    _dct_coeffs[0]=(ogg_int16_t)(_dct_coeffs[0]*(int)_dc_quant);
    oc_idct8x8_arm(_dct_coeffs,_last_zzi);
  }
  /*Fill in the target buffer.*/
  frag_buf_off=_state->frag_buf_offs[_fragi];
  mb_mode=_state->frags[_fragi].mb_mode;
  ystride=_state->ref_ystride[_pli];
  dst=_state->ref_frame_data[_state->ref_frame_idx[OC_FRAME_SELF]]+frag_buf_off;
  if(mb_mode==OC_MODE_INTRA)oc_frag_recon_intra_arm(dst,ystride,_dct_coeffs);
  else{
    const unsigned char *ref;
    int                  mvoffsets[2];
    ref=
     _state->ref_frame_data[_state->ref_frame_idx[OC_FRAME_FOR_MODE(mb_mode)]]
     +frag_buf_off;
    if(oc_state_get_mv_offsets(_state,mvoffsets,_pli,
     _state->frag_mvs[_fragi][0],_state->frag_mvs[_fragi][1])>1){
      oc_frag_recon_inter2_arm(
       dst,ref+mvoffsets[0],ref+mvoffsets[1],ystride,_dct_coeffs);
    }
    else oc_frag_recon_inter_arm(dst,ref+mvoffsets[0],ystride,_dct_coeffs);
  }
}


void oc_state_vtable_init_arm(oc_theora_state *_state){
  _state->opt_vtable.frag_copy=NULL;
  _state->opt_vtable.frag_recon_intra=oc_frag_recon_intra_arm;
  _state->opt_vtable.frag_recon_inter=oc_frag_recon_inter_arm;
  _state->opt_vtable.frag_recon_inter2=oc_frag_recon_inter2_arm;
  _state->opt_vtable.idct8x8=oc_idct8x8_arm;
  _state->opt_vtable.state_frag_recon=oc_state_frag_recon_arm;
  _state->opt_vtable.state_frag_copy_list=oc_state_frag_copy_list_arm;
  _state->opt_vtable.state_loop_filter_frag_rows=
   oc_state_loop_filter_frag_rows_arm;
  _state->opt_vtable.restore_fpu=oc_restore_fpu_arm;
  _state->opt_data.dct_fzig_zag=OC_FZIG_ZAG;
}

void oc_restore_fpu_arm(void)
{
}

#endif
