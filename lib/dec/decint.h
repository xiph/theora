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

#include <limits.h>
#if !defined(_decint_H)
# define _decint_H (1)
# include "theora/theoradec.h"
# include "../internal.h"

typedef struct th_setup_info oc_setup_info;
typedef struct th_dec_ctx    oc_dec_ctx;

# include "idct.h"
# include "huffdec.h"
# include "dequant.h"

/*Constants for the packet-in state machine specific to the decoder.*/

/*Next packet to read: Data packet.*/
#define OC_PACKET_DATA (0)



struct th_setup_info{
  /*The Huffman codes.*/
  oc_huff_node      *huff_tables[TH_NHUFFMAN_TABLES];
  /*The quantization parameters.*/
  th_quant_info  qinfo;
};



struct th_dec_ctx{
  /*Shared encoder/decoder state.*/
  oc_theora_state          state;
  /*Whether or not packets are ready to be emitted.
    This takes on negative values while there are remaining header packets to
     be emitted, reaches 0 when the codec is ready for input, and goes to 1
     when a frame has been processed and a data packet is ready.*/
  int                      packet_state;
  /*Buffer in which to assemble packets.*/
  oggpack_buffer           opb;
  /*Huffman decode trees.*/
  oc_huff_node            *huff_tables[TH_NHUFFMAN_TABLES];
  /*The index of one past the last token in each plane for each coefficient.
    The final entries are the total number of tokens for each coefficient.*/
  int                      ti0[3][64];
  /*The index of one past the last extra bits entry in each plane for each
     coefficient.
    The final entries are the total number of extra bits entries for each
     coefficient.*/
  int                      ebi0[3][64];
  /*The number of outstanding EOB runs at the start of each coefficient in each
     plane.*/
  int                      eob_runs[3][64];
  /*The DCT token lists.*/
  unsigned char          **dct_tokens;
  /*The extra bits associated with DCT tokens.*/
  ogg_uint16_t           **extra_bits;
  /*The out-of-loop post-processing level.*/
  int                      pp_level;
  /*The DC scale used for out-of-loop deblocking.*/
  int                      pp_dc_scale[64];
  /*The sharpen modifier used for out-of-loop deringing.*/
  int                      pp_sharp_mod[64];
  /*The DC quantization index of each block.*/
  unsigned char           *dc_qis;
  /*The variance of each block.*/
  int                     *variances;
  /*The storage for the post-processed frame buffer.*/
  unsigned char           *pp_frame_data;
  /*Whether or not the post-processsed frame buffer has space for chroma.*/
  int                      pp_frame_has_chroma;
  /*The buffer used for the post-processed frame.*/
  th_ycbcr_buffer      pp_frame_buf;
  /*The striped decode callback function.*/
  th_stripe_callback   stripe_cb;
};

/*Fix-ups for the libogg1 API, which returns -1 when there are insufficient
   bits left in the packet as the value read.
  This has two problems:
  a) Cannot distinguish between reading 32 1 bits and failing to have
   sufficient bits left in the packet.
  b) Returns values that are outside the range [0..(1<<nbits)-1], which can
   crash code that uses such values as indexes into arrays, etc.

  We solve the first problem by doing two reads and combining the results.
  We solve the second problem by masking out the result based on the sign bit
   of the return value.
  It's a little more work, but branchless, so it should not slow us down much.

  The libogg2 API does not have these problems, and the definitions of the
   functions below can be replaced by direct libogg2 calls.

  One issue remaining is that in libogg2, the return value and the number of
   bits parameters are swapped between the read and write functions.
  This can cause some confusion.
  We could fix that in our wrapper here, but then we would be swapped from the
   normal libogg2 calls, which could also cause confusion.
  For the moment we keep the libogg2 parameter ordering.*/

/*Read 32 bits.
  *_ret is set to 0 on failure.
  Return: 0 on success, or a negative value on failure.*/
extern int theora_read32(oggpack_buffer *_opb,long *_ret);
/*Read n bits, where n <= 31 for libogg1.
  *_ret is set to 0 on failure.
  Return: 0 on success, or a negative value on failure.*/
extern int theora_read(oggpack_buffer *_opb,int _nbits,long *_ret);
/*Read 1 bit,
  *_ret is set to 0 on failure.
  Return: 0 on success, or a negative value on failure.*/
extern int theora_read1(oggpack_buffer *_opb,long *_ret);
/*Look ahead n bits, where n <= 31 for libogg1.
  In the event that there are some bits remaining, but fewer than n, then the
   remaining bits are returned, with the missing bits set to 0, and the
   function succeeds.
  The stream can be advanced afterwards with oggpackB_adv().
  *_ret is set to 0 on failure.
  Return: 0 on success, or a negative value on failure.*/
extern int theora_look(oggpack_buffer *_opb,int _nbits,long *_ret);

#endif
