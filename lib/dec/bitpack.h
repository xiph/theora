/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggTheora SOURCE CODE IS (C) COPYRIGHT 1994-2008             *
 * by the Xiph.Org Foundation and contributors http://www.xiph.org/ *
 *                                                                  *
 ********************************************************************

  function: packing variable sized words into an octet stream
  last mod: $Id: bitwise.c 7675 2004-09-01 00:34:39Z xiphmont $

 ********************************************************************/
#if !defined(_bitpack_H)
# define _bitpack_H (1)
# include <ogg/ogg.h>

void theorapackB_readinit(oggpack_buffer *_b,unsigned char *_buf,int _bytes);
/*Read in bits without advancing the bitptr.
  Here we assume 0<=_bits&&_bits<=32.*/
static int theorapackB_look(oggpack_buffer *_b,int _bits,long *_ret);
int theorapackB_look1(oggpack_buffer *_b,long *_ret);
static void theorapackB_adv(oggpack_buffer *_b,int _bits);
void theorapackB_adv1(oggpack_buffer *_b);
/*Here we assume 0<=_bits&&_bits<=32.*/
int theorapackB_read(oggpack_buffer *_b,int _bits,long *_ret);
int theorapackB_read1(oggpack_buffer *_b,long *_ret);
long theorapackB_bytes(oggpack_buffer *_b);
long theorapackB_bits(oggpack_buffer *_b);
unsigned char *theorapackB_get_buffer(oggpack_buffer *_b);

/*These two functions are only used in one place, and declaring them static so
   they can be inlined saves considerable function call overhead.*/

/*Read in bits without advancing the bitptr.
  Here we assume 0<=_bits&&_bits<=32.*/
static int theorapackB_look(oggpack_buffer *_b,int _bits,long *_ret){
  long ret;
  long m;
  long d;
  m=32-_bits;
  _bits+=_b->endbit;
  d=_b->storage-_b->endbyte;
  if(d<=4){
    /*Not the main path.*/
    if(d<=0){
      *_ret=0L;
      return -(_bits>d*8);
    }
    /*If we have some bits left, but not enough, return the ones we have.*/
    if(d*8<_bits)_bits=d*8;
  }
  ret=_b->ptr[0]<<24+_b->endbit;
  if(_bits>8){
    ret|=_b->ptr[1]<<16+_b->endbit;
    if(_bits>16){
      ret|=_b->ptr[2]<<8+_b->endbit;
      if(_bits>24){
        ret|=_b->ptr[3]<<_b->endbit;
        if(_bits>32)ret|=_b->ptr[4]>>8-_b->endbit;
      }
    }
  }
  *_ret=((ret&0xFFFFFFFF)>>(m>>1))>>(m+1>>1);
  return 0;
}

static void theorapackB_adv(oggpack_buffer *_b,int _bits){
  _bits+=_b->endbit;
  _b->ptr+=_bits>>3;
  _b->endbyte+=_bits>>3;
  _b->endbit=_bits&7;
}

#endif
