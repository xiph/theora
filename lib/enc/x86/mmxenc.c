/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2008                *
 * by the Xiph.Org Foundation and contributors http://www.xiph.org/ *
 *                                                                  *
 ********************************************************************

  function:
  last mod: $Id: dct_decode_mmx.c 15078 2008-06-27 22:07:19Z xiphmont $

 ********************************************************************/
#include <string.h>
#include "x86enc.h"
#include "../../dec/x86/mmxloop.h"

#if defined(OC_X86_ASM)

/*Apply the loop filter.*/
void oc_enc_loop_filter_mmx(CP_INSTANCE *cpi,int _flimit){
  unsigned char OC_ALIGN8  ll[8];
  unsigned char           *cp;
  ogg_uint32_t            *bp;
  int                      pli;
  cp=cpi->frag_coded;
  bp=cpi->frag_buffer_index;
  if(_flimit==0)return;
  memset(ll,_flimit,sizeof(ll));
  for(pli=0;pli<3;pli++){
    ogg_uint32_t *bp_begin;
    ogg_uint32_t *bp_end;
    int           stride;
    int           h;
    bp_begin=bp;
    bp_end=bp+cpi->frag_n[pli];
    stride=cpi->stride[pli];
    h=cpi->frag_h[pli];
    while(bp<bp_end){
      ogg_uint32_t *bp_left;
      ogg_uint32_t *bp_right;
      bp_left=bp;
      bp_right=bp+h;
      for(;bp<bp_right;bp++,cp++)if(*cp){
        if(bp>bp_left)OC_LOOP_FILTER_H_MMX(cpi->lastrecon+bp[0],stride,ll);
        if(bp_left>bp_begin){
          OC_LOOP_FILTER_V_MMX(cpi->lastrecon+bp[0],stride,ll);
        }
        if(bp+1<bp_right&&!cp[1]){
          OC_LOOP_FILTER_H_MMX(cpi->lastrecon+bp[0]+8,stride,ll);
        }
        if(bp+h<bp_end&&!cp[h]){
          OC_LOOP_FILTER_V_MMX(cpi->lastrecon+bp[h],stride,ll);
        }
      }
    }
  }
  __asm__ __volatile__("emms\n\t");
}

#endif
