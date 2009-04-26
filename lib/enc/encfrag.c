/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2007                *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

  function:
  last mod: $Id$

 ********************************************************************/
#include <stdlib.h>
#include <string.h>
#include "codec_internal.h"


void oc_enc_frag_sub(const CP_INSTANCE *_cpi,ogg_int16_t _diff[64],
 const unsigned char *_src,const unsigned char *_ref,int _ystride){
  (*_cpi->opt_vtable.frag_sub)(_diff,_src,_ref,_ystride);
}

void oc_enc_frag_sub_c(ogg_int16_t _diff[64],const unsigned char *_src,
 const unsigned char *_ref,int _ystride){
  int i;
  for(i=0;i<8;i++){
    int j;
    for(j=0;j<8;j++)_diff[i*8+j]=(ogg_int16_t)(_src[j]-_ref[j]);
    _src+=_ystride;
    _ref+=_ystride;
  }
}

void oc_enc_frag_sub_128(const CP_INSTANCE *_cpi,ogg_int16_t _diff[64],
 const unsigned char *_src,int _ystride){
  (*_cpi->opt_vtable.frag_sub_128)(_diff,_src,_ystride);
}

void oc_enc_frag_sub_128_c(ogg_int16_t *_diff,
 const unsigned char *_src,int _ystride){
  int i;
  for(i=0;i<8;i++){
    int j;
    for(j=0;j<8;j++)_diff[i*8+j]=(ogg_int16_t)(_src[j]-128);
    _src+=_ystride;
  }
}

unsigned oc_enc_frag_sad(const CP_INSTANCE *_cpi,const unsigned char *_x,
 const unsigned char *_y,int _ystride){
  return (*_cpi->opt_vtable.frag_sad)(_x,_y,_ystride);
}

unsigned oc_enc_frag_sad_c(const unsigned char *_src,
 const unsigned char *_ref,int _ystride){
  unsigned sad;
  int      i;
  sad=0;
  for(i=8;i-->0;){
    int j;
    for(j=0;j<8;j++)sad+=abs(_src[j]-_ref[j]);
    _src+=_ystride;
    _ref+=_ystride;
  }
  return sad;
}

unsigned oc_enc_frag_sad_thresh(const CP_INSTANCE *_cpi,
 const unsigned char *_src,const unsigned char *_ref,int _ystride,
 unsigned _thresh){
  return (*_cpi->opt_vtable.frag_sad_thresh)(_src,_ref,_ystride,_thresh);
}

unsigned oc_enc_frag_sad_thresh_c(const unsigned char *_src,
 const unsigned char *_ref,int _ystride,unsigned _thresh){
  unsigned sad;
  int      i;
  sad=0;
  for(i=8;i-->0;){
    int j;
    for(j=0;j<8;j++)sad+=abs(_src[j]-_ref[j]);
    if(sad>_thresh)break;
    _src+=_ystride;
    _ref+=_ystride;
  }
  return sad;
}

unsigned oc_enc_frag_sad2_thresh(const CP_INSTANCE *_cpi,
 const unsigned char *_src,const unsigned char *_ref1,
 const unsigned char *_ref2,int _ystride,unsigned _thresh){
  return (*_cpi->opt_vtable.frag_sad2_thresh)(_src,_ref1,_ref2,_ystride,
   _thresh);
}

unsigned oc_enc_frag_sad2_thresh_c(const unsigned char *_src,
 const unsigned char *_ref1,const unsigned char *_ref2,int _ystride,
 unsigned _thresh){
  unsigned sad;
  int      i;
  sad=0;
  for(i=8;i-->0;){
    int j;
    for(j=0;j<8;j++)sad+=abs(_src[j]-(_ref1[j]+_ref2[j]>>1));
    if(sad>_thresh)break;
    _src+=_ystride;
    _ref1+=_ystride;
    _ref2+=_ystride;
  }
  return sad;
}

void oc_enc_frag_copy(const CP_INSTANCE *_cpi,unsigned char *_dst,
 const unsigned char *_src,int _ystride){
  (*_cpi->opt_vtable.frag_copy)(_dst,_src,_ystride);
}

void oc_enc_frag_copy2(const CP_INSTANCE *_cpi,unsigned char *_dst,
 const unsigned char *_src1,const unsigned char *_src2,int _ystride){
  (*_cpi->opt_vtable.frag_copy2)(_dst,_src1,_src2,_ystride);
}

void oc_enc_frag_copy2_c(unsigned char *_dst,
 const unsigned char *_src1,const unsigned char *_src2,int _ystride){
  int i;
  int j;
  for(i=8;i-->0;){
    for(j=0;j<8;j++)_dst[j]=_src1[j]+_src2[j]>>1;
    _dst+=_ystride;
    _src1+=_ystride;
    _src2+=_ystride;
  }
}

void oc_enc_frag_recon_intra(const CP_INSTANCE *_cpi,
 unsigned char *_dst,int _ystride,const ogg_int16_t _residue[64]){
  (*_cpi->opt_vtable.frag_recon_intra)(_dst,_ystride,_residue);
}

void oc_enc_frag_recon_inter(const CP_INSTANCE *_cpi,unsigned char *_dst,
 const unsigned char *_src,int _ystride,const ogg_int16_t _residue[64]){
  (*_cpi->opt_vtable.frag_recon_inter)(_dst,_src,_ystride,_residue);
}

void oc_enc_restore_fpu(const CP_INSTANCE *_cpi){
  (*_cpi->opt_vtable.restore_fpu)();
}
