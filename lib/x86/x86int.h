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
    last mod: $Id$

 ********************************************************************/

#if !defined(_x86_x86int_H)
# define _x86_x86int_H (1)
# include "../internal.h"

/*Converts the expression in the argument to a string.*/
#define OC_M2STR(_s) #_s

/*Memory operands do not always include an offset.
  To avoid warnings, we force an offset with %H (which adds 8).*/
# if defined(__GNUC_PREREQ)
#  if __GNUC_PREREQ(4,0)
#   define OC_MEM_OFFS(_offs,_name) \
  OC_M2STR(_offs-8+%H[_name])
#  endif
# endif
/*If your gcc version does't support %H, then you get to suffer the warnings.
  Note that Apple's gas breaks on things like _offs+(%esp): it throws away the
   whole offset, instead of substituting in 0 for the missing operand to +.*/
# if !defined(OC_MEM_OFFS)
#  define OC_MEM_OFFS(_offs,_name) \
  OC_M2STR(_offs+%[_name])
# endif

/*Declare an array operand with an exact size.
  This tells gcc we're going to clobber this memory region, without having to
   clobber all of "memory" and lets us access local buffers directly using the
   stack pointer, without allocating a separate register to point to them.*/
#define OC_ARRAY_OPERAND(_type,_ptr,_size) \
  (*({struct{_type array_value__[_size];} *array_addr__=(void *)_ptr; \
   array_addr__;}))

extern const short __attribute__((aligned(16))) OC_IDCT_CONSTS[64];

void oc_state_vtable_init_x86(oc_theora_state *_state);

void oc_frag_copy_mmx(unsigned char *_dst,
 const unsigned char *_src,int _ystride);
void oc_frag_recon_intra_mmx(unsigned char *_dst,int _ystride,
 const ogg_int16_t *_residue);
void oc_frag_recon_inter_mmx(unsigned char *_dst,
 const unsigned char *_src,int _ystride,const ogg_int16_t *_residue);
void oc_frag_recon_inter2_mmx(unsigned char *_dst,const unsigned char *_src1,
 const unsigned char *_src2,int _ystride,const ogg_int16_t *_residue);
void oc_idct8x8_mmx(ogg_int16_t _y[64],int _last_zzi);
void oc_idct8x8_sse2(ogg_int16_t _y[64],int _last_zzi);
void oc_state_frag_recon_mmx(const oc_theora_state *_state,ptrdiff_t _fragi,
 int _pli,ogg_int16_t _dct_coeffs[64],int _last_zzi,ogg_uint16_t _dc_quant);
void oc_state_quad_recon_mmx(const oc_theora_state *_state,ptrdiff_t _frag_buf_off,
 int _pli,ogg_int16_t _dct_coeffs[][64+8],int _last_zzi[4],
 ogg_uint16_t _dc_quant,int _mask,int _ref_frame,oc_mv _mv);
void oc_state_4mv_recon_mmx(const oc_theora_state *_state,ptrdiff_t _frag_buf_off,
 int _pli,ogg_int16_t _dct_coeffs[][64+8],int _last_zzi[4],
 ogg_uint16_t _dc_quant,int _mask,oc_mv _mvs[4]);
void oc_state_frag_copy_list_mmx(const oc_theora_state *_state,
 const ptrdiff_t *_fragis,ptrdiff_t _nfragis,
 int _dst_frame,int _src_frame,int _pli);
void oc_state_loop_filter_frag_rows_mmx(const oc_theora_state *_state,
 int _bv[256],int _refi,int _pli,int _fragy0,int _fragy_end);
void oc_state_loop_filter_frag_rows_mmxext(const oc_theora_state *_state,
 int _bv[256],int _refi,int _pli,int _fragy0,int _fragy_end);
void oc_restore_fpu_mmx(void);

#endif
