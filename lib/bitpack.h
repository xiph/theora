/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggTheora SOURCE CODE IS (C) COPYRIGHT 1994-2009             *
 * by the Xiph.Org Foundation and contributors http://www.xiph.org/ *
 *                                                                  *
 ********************************************************************

  function: packing variable sized words into an octet stream
  last mod: $Id: bitwise.c 7675 2004-09-01 00:34:39Z xiphmont $

 ********************************************************************/
#if !defined(_bitpack_H)
# define _bitpack_H (1)
# include <stddef.h>
# include <limits.h>



typedef size_t             oc_pb_window;
typedef struct oc_pack_buf oc_pack_buf;



# define OC_PB_WINDOW_SIZE ((int)sizeof(oc_pb_window)*CHAR_BIT)
/*This is meant to be a large, positive constant that can still be efficiently
   loaded as an immediate (on platforms like ARM, for example).
  Even relatively modest values like 100 would work fine.*/
# define OC_LOTS_OF_BITS (0x40000000)


#ifdef OC_LIBOGG2
#define oc_pack_buf oggpack_buffer

#define oc_pack_adv(B,A) theorapackB_adv(B,A)
#define oc_pack_look(B,A) theorapackB_look(B,A)
#define oc_pack_readinit(B,R) theorapackB_readinit(B,R)
#define oc_pack_read(B,L) theorapackB_read((B),(L))
#define oc_pack_read1(B) theorapackB_read1((B))
#define oc_pack_bytes_left(B) theorapackB_bytesleft(B)

long theorapackB_lookARM(oggpack_buffer *_b, int bits);
long theorapackB_readARM(oggpack_buffer *_b,int _bits);
long theorapackB_read1ARM(oggpack_buffer *_b);

#define theorapackB_look theorapackB_lookARM
#define theorapackB_read theorapackB_readARM
#define theorapackB_read1 theorapackB_read1ARM
#define theorapackB_adv  oggpack_adv
#define theorapackB_readinit oggpack_readinit
#define theorapackB_bytes oggpack_bytes
#define theorapackB_bits oggpack_bits
#define theorapackB_bytesleft oggpack_bytesleft

#define oggpack_bytesleft(B) (((B)->bitsLeftInSegment+7)>>3)

#else
struct oc_pack_buf{
  oc_pb_window         window;
  const unsigned char *ptr;
  const unsigned char *stop;
  int                  bits;
  int                  eof;
};

void oc_pack_readinit(oc_pack_buf *_b,unsigned char *_buf,long _bytes);
int oc_pack_look1(oc_pack_buf *_b);
void oc_pack_adv1(oc_pack_buf *_b);
/*Here we assume 0<=_bits&&_bits<=32.*/
long oc_pack_read(oc_pack_buf *_b,int _bits);
int oc_pack_read1(oc_pack_buf *_b);
/* returns -1 for read beyond EOF, or the number of whole bytes available */
long oc_pack_bytes_left(oc_pack_buf *_b);

/*These two functions are implemented in huffdec.c*/
/*Read in bits without advancing the bitptr.
  Here we assume 0<=_bits&&_bits<=32.*/
long oc_pack_look(oc_pack_buf *_b,int _bits);
void oc_pack_adv(oc_pack_buf *_b,int _bits);
#endif

#endif
