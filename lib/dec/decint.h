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
#include <pthread.h>

#if !defined(_decint_H)
# define _decint_H (1)
# include "theora/theoradec.h"
# include "../internal.h"
# include "bitwise.h"

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



typedef struct {
  oc_dec_ctx    *dec;
  int            done;
  int            pli;
  int            avail_fragy0;
  int            avail_fragy_end;
}oc_dec_pipeline_plane;

typedef struct{
  int  ti[3][64];
  int  ebi[3][64];
  int  eob_runs[3][64];
  int  bounding_values[256];
  int *coded_fragis[3];
  int *uncoded_fragis[3];
  int  fragy0[3];
  int  fragy_end[3];
  int  ncoded_fragis[3];
  int  nuncoded_fragis[3];
  int  pred_last[3][3];
  int  mcu_nvfrags;
  int  loop_filter;
  int  pp_level;
  int  stripe_fragy;
  int  refi;
  int  notstart;
  int  notdone;
  oc_dec_pipeline_plane pplanes[3];
}oc_dec_pipeline_state;


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
  /*The striped decoding pipeline.*/
  oc_dec_pipeline_state    pipe;
  /*Mutex for parallel pipelined decode.*/
  pthread_mutex_t          pipe_lock;
  /*The auxilliary decoder threads.*/
  pthread_t                pipe_thread[2];
  /*Condition variables for the auxilliary decoder threads.*/
  pthread_cond_t           pipe_cond[2];
};

#endif
