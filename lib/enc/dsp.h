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

#ifndef DSP_H
#define DSP_H

#include "theora/theora.h"
#include "../cpu.h"

typedef struct
{
  void   (*save_fpu)              (void);
  void   (*restore_fpu)           (void);

  void   (*set8x8)                (unsigned char val, unsigned char *ptr,
				   ogg_uint32_t stride);

  void   (*sub8x8)                (unsigned char *FiltPtr, unsigned char *ReconPtr,
				   ogg_int16_t *DctInputPtr, ogg_uint32_t stride);

  void   (*sub8x8_128)            (unsigned char *FiltPtr, ogg_int16_t *DctInputPtr,
				   ogg_uint32_t stride);

  void   (*sub8x8avg2)            (unsigned char *FiltPtr, unsigned char *ReconPtr1,
				   unsigned char *ReconPtr2, ogg_int16_t *DctInputPtr,
				   ogg_uint32_t stride);

  void   (*copy8x8)               (unsigned char *src, unsigned char *dest, 
				   ogg_uint32_t stride);
  
  void   (*copy8x8_half)          (unsigned char *src1, unsigned char *src2, 
				   unsigned char *dest, ogg_uint32_t stride);

  void   (*recon8x8)              (unsigned char *ReconPtr, ogg_int16_t *ChangePtr, 
				   ogg_uint32_t stride);

  void   (*fdct_short)            (ogg_int16_t *InputData, ogg_int16_t *OutputData);

  ogg_uint32_t (*row_sad8)        (unsigned char *Src1, unsigned char *Src2);

  ogg_uint32_t (*col_sad8x8)      (unsigned char *Src1, unsigned char *Src2,
				   ogg_uint32_t stride);
  
  ogg_uint32_t (*sad8x8)          (unsigned char *ptr1, unsigned char *ptr2, 
				   ogg_uint32_t stride);

  ogg_uint32_t (*sad8x8_thres)    (unsigned char *ptr1, unsigned char *ptr2, 
				   ogg_uint32_t stride, ogg_uint32_t thres);

  ogg_uint32_t (*sad8x8_xy2_thres)(unsigned char *SrcData, unsigned char *RefDataPtr1,
				   unsigned char *RefDataPtr2, ogg_uint32_t stride,
				   ogg_uint32_t thres);
  
  ogg_uint32_t (*intra8x8_err)    (unsigned char *DataPtr, ogg_uint32_t stride);
  
  ogg_uint32_t (*inter8x8_err)    (unsigned char *SrcData, unsigned char *RefData, 
				   ogg_uint32_t stride);

  ogg_uint32_t (*inter8x8_err_xy2)(unsigned char *SrcData, unsigned char *RefDataPtr1,
				   unsigned char *RefDataPtr2, ogg_uint32_t stride);
               
  void (*LoopFilter)              (CP_INSTANCE *cpi, int FLimit);

  void (*FilterVert)              (unsigned char * PixelPtr,
				   ogg_int32_t LineLength, ogg_int16_t *BoundingValuePtr);
  
  void (*IDctSlow)                (ogg_int16_t *InputData, 
				   ogg_int16_t *QuantMatrix, ogg_int16_t *OutputData);

  void (*IDct3)                   (ogg_int16_t *InputData, 
				   ogg_int16_t *QuantMatrix, ogg_int16_t *OutputData);
  
  void (*IDct10)                  (ogg_int16_t *InputData, 
				   ogg_int16_t *QuantMatrix, ogg_int16_t *OutputData);
} DspFunctions;

extern void dsp_dct_init(DspFunctions *funcs, ogg_uint32_t cpu_flags);
extern void dsp_recon_init (DspFunctions *funcs, ogg_uint32_t cpu_flags);
extern void dsp_dct_decode_init(DspFunctions *funcs, ogg_uint32_t cpu_flags);
extern void dsp_idct_init(DspFunctions *funcs, ogg_uint32_t cpu_flags);

void dsp_init(DspFunctions *funcs);
void dsp_static_init(DspFunctions *funcs);
#if defined(USE_ASM) && (defined(__i386__) || defined(__x86_64__) || defined(WIN32))
extern void dsp_mmx_init(DspFunctions *funcs);
extern void dsp_mmxext_init(DspFunctions *funcs);
extern void dsp_mmx_fdct_init(DspFunctions *funcs);
extern void dsp_mmx_recon_init(DspFunctions *funcs);
extern void dsp_mmx_dct_decode_init(DspFunctions *funcs);
extern void dsp_mmx_idct_init(DspFunctions *funcs);
#endif

#define dsp_save_fpu(funcs) (funcs.save_fpu ())

#define dsp_restore_fpu(funcs) (funcs.restore_fpu ())

#define dsp_set8x8(funcs,a1,a2,a3) (funcs.set8x8 (a1,a2,a3))

#define dsp_sub8x8(funcs,a1,a2,a3,a4) (funcs.sub8x8 (a1,a2,a3,a4))

#define dsp_sub8x8_128(funcs,a1,a2,a3) (funcs.sub8x8_128 (a1,a2,a3))

#define dsp_sub8x8avg2(funcs,a1,a2,a3,a4,a5) (funcs.sub8x8avg2 (a1,a2,a3,a4,a5))

#define dsp_copy8x8(funcs,ptr1,ptr2,str1) (funcs.copy8x8 (ptr1,ptr2,str1))

#define dsp_copy8x8_half(funcs,ptr1,ptr2,ptr3,str1) (funcs.copy8x8_half (ptr1,ptr2,ptr3,str1))

#define dsp_recon8x8(funcs,ptr1,ptr2,str1) (funcs.recon8x8 (ptr1,ptr2,str1))

#define dsp_fdct_short(funcs,in,out) (funcs.fdct_short (in,out))

#define dsp_row_sad8(funcs,ptr1,ptr2) (funcs.row_sad8 (ptr1,ptr2))

#define dsp_col_sad8x8(funcs,ptr1,ptr2,str1) (funcs.col_sad8x8 (ptr1,ptr2,str1))

#define dsp_sad8x8(funcs,ptr1,ptr2,str) (funcs.sad8x8 (ptr1,ptr2,str))

#define dsp_sad8x8_thres(funcs,ptr1,ptr2,str,t) (funcs.sad8x8_thres (ptr1,ptr2,str,t))

#define dsp_sad8x8_xy2_thres(funcs,ptr1,ptr2,ptr3,str,t) \
  (funcs.sad8x8_xy2_thres (ptr1,ptr2,ptr3,str,t))

#define dsp_intra8x8_err(funcs,ptr1,str1) (funcs.intra8x8_err (ptr1,str1))

#define dsp_inter8x8_err(funcs,ptr1,ptr2,str) \
  (funcs.inter8x8_err (ptr1,ptr2,str))

#define dsp_inter8x8_err_xy2(funcs,ptr1,ptr2,ptr3,str) \
  (funcs.inter8x8_err_xy2 (ptr1,ptr2,ptr3,str))

#define dsp_LoopFilter(funcs, ptr1, i) \
  (funcs.LoopFilter(ptr1, i))

#define dsp_IDctSlow(funcs, ptr1, ptr2, ptr3) \
    (funcs.IDctSlow(ptr1, ptr2, ptr3))

#define dsp_IDct3(funcs, ptr1, ptr2, ptr3) \
    (funcs.IDctSlow(ptr1, ptr2, ptr3))

#define dsp_IDct10(funcs, ptr1, ptr2, ptr3) \
   (funcs.IDctSlow(ptr1, ptr2, ptr3))

#endif /* DSP_H */
