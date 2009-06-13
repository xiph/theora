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

#include "encint.h"
#include "../dec/dct.h"



/*Performs a forward 8 point Type-II DCT transform.
  The output is scaled by a factor of 2 from the orthonormal version of the
   transform.
  _y: The buffer to store the result in.
      Data will be placed in every 8th entry (e.g., in a column of an 8x8
       block).
  _x: The input coefficients.
      The first 8 entries are used (e.g., from a row of an 8x8 block).*/
static void oc_fdct8(ogg_int16_t *_y,const ogg_int16_t _x[8]){
  int t0;
  int t1;
  int t2;
  int t3;
  int t4;
  int t5;
  int t6;
  int t7;
  int r;
  int s;
  int u;
  int v;
  /*Stage 1:*/
  /*0-7 butterfly.*/
  t0=_x[0<<3]+(int)_x[7<<3];
  t7=_x[0<<3]-(int)_x[7<<3];
  /*1-6 butterfly.*/
  t1=_x[1<<3]+(int)_x[6<<3];
  t6=_x[1<<3]-(int)_x[6<<3];
  /*2-5 butterfly.*/
  t2=_x[2<<3]+(int)_x[5<<3];
  t5=_x[2<<3]-(int)_x[5<<3];
  /*3-4 butterfly.*/
  t3=_x[3<<3]+(int)_x[4<<3];
  t4=_x[3<<3]-(int)_x[4<<3];
  /*Stage 2:*/
  /*0-3 butterfly.*/
  r=t0+t3;
  t3=t0-t3;
  t0=r;
  /*1-2 butterfly.*/
  r=t1+t2;
  t2=t1-t2;
  t1=r;
  /*6-5 butterfly.*/
  r=t6+t5;
  t5=t6-t5;
  t6=r;
  /*Stages 3 and 4 are where all the approximation occurs.
    These are chosen to be as close to an exact inverse of the approximations
     made in the iDCT as possible, while still using mostly 16-bit arithmetic.
    We use some 16x16->32 signed MACs, but those still commonly execute in 1
     cycle on a 16-bit DSP.
    For example, s=(27146*t5+0x4000>>16)+t5+(t5!=0) is an exact inverse of
     t5=(OC_C4S4*s>>16).
    That is, applying the latter to the output of the former will recover t5
     exactly (over the valid input range of t5, -23171...23169).
    We increase the rounding bias to 0xB500 in this particular case so that
     errors inverting the subsequent butterfly are not one-sided (e.g., the
     mean error is very close to zero).
    The (t5!=0) term could be replaced simply by 1, but we want to send 0 to 0.
    The fDCT of an all-zeros block will still not be zero, because of the
     biases we added at the very beginning of the process, but it will be close
     enough that it is guaranteed to round to zero.*/
  /*Stage 3:*/
  /*4-5 butterfly.*/
  s=(27146*t5+0xB500>>16)+t5+(t5!=0)>>1;
  r=t4+s;
  t5=t4-s;
  t4=r;
  /*7-6 butterfly.*/
  s=(27146*t6+0xB500>>16)+t6+(t6!=0)>>1;
  r=t7+s;
  t6=t7-s;
  t7=r;
  /*Stage 4:*/
  /*0-1 butterfly.*/
  r=(27146*t0+0x4000>>16)+t0+(t0!=0);
  s=(27146*t1+0xB500>>16)+t1+(t1!=0);
  u=r+s>>1;
  v=r-u;
  _y[0]=u;
  _y[4]=v;
  /*3-2 rotation by 6pi/16*/
  u=(OC_C6S2*t2+OC_C2S6*t3+0x6CB7>>16)+(t3!=0);
  s=(OC_C6S2*u>>16)-t2;
  v=(s*21600+0x2800>>18)+s+(s!=0);
  _y[2]=u;
  _y[6]=v;
  /*6-5 rotation by 3pi/16*/
  u=(OC_C5S3*t6+OC_C3S5*t5+0x0E3D>>16)+(t5!=0);
  s=t6-(OC_C5S3*u>>16);
  v=(s*26568+0x3400>>17)+s+(s!=0);
  _y[5]=u;
  _y[3]=v;
  /*7-4 rotation by 7pi/16*/
  u=(OC_C7S1*t4+OC_C1S7*t7+0x7B1B>>16)+(t7!=0);
  s=(OC_C7S1*u>>16)-t4;
  v=(s*20539+0x3000>>20)+s+(s!=0);
  _y[1]=u;
  _y[7]=v;
}

void oc_enc_fdct8x8(const oc_enc_ctx *_enc,ogg_int16_t _y[64],
 const ogg_int16_t _x[64]){
  (*_enc->opt_vtable.fdct8x8)(_y,_x);
}

/*Performs a forward 8x8 Type-II DCT transform.
  The output is scaled by a factor of 4 relative to the orthonormal version
   of the transform.
  _y: The buffer to store the result in.
      This may be the same as _x.
  _x: The input coefficients. */
void oc_enc_fdct8x8_c(ogg_int16_t _y[64],const ogg_int16_t _x[64]){
  const ogg_int16_t *in;
  ogg_int16_t       *end;
  ogg_int16_t       *out;
  ogg_int16_t        w[64];
  int                i;
  /*Add two extra bits of working precision to improve accuracy; any more and
     we could overflow.*/
  for(i=0;i<64;i++)w[i]=_x[i]<<2;
  /*These biases correct for some systematic error that remains in the full
     fDCT->iDCT round trip.*/
  w[0]+=(w[0]!=0)+1;
  w[1]++;
  w[8]--;
  /*Transform columns of w into rows of _y.*/
  for(in=w,out=_y,end=out+64;out<end;in++,out+=8)oc_fdct8(out,in);
  /*Transform columns of _y into rows of w.*/
  for(in=_y,out=w,end=out+64;out<end;in++,out+=8)oc_fdct8(out,in);
  /*Round the result back to the external working precision (which is still
     scaled by four relative to the orthogonal result).
    TODO: We should just update the external working precision.*/
  for(i=0;i<64;i++)_y[i]=w[i]+2>>2;
}
