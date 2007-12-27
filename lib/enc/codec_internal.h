/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2005                *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

  function:
  last mod: $Id$

 ********************************************************************/

#ifndef ENCODER_INTERNAL_H
#define ENCODER_INTERNAL_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "theora/theora.h"
#include "encoder_huffman.h"
#include "dsp.h"

#ifndef LIBOGG2
#define theora_read(x,y,z) ( *z = oggpackB_read(x,y) )
#else
#define theora_read(x,y,z) ( oggpackB_read(x,y,z) )
#endif

#define CURRENT_ENCODE_VERSION   1
#define HUGE_ERROR              (1<<28)  /*  Out of range test value */

/* Baseline dct height and width. */
#define BLOCK_HEIGHT_WIDTH          8
#define HFRAGPIXELS                 8
#define VFRAGPIXELS                 8

/* Blocks on INTRA/INTER Y/U/V planes */
enum BlockMode {
  BLOCK_Y,
  BLOCK_U,
  BLOCK_V,
  BLOCK_INTER_Y,
  BLOCK_INTER_U,
  BLOCK_INTER_V
};

/* Baseline dct block size */
#define BLOCK_SIZE              (BLOCK_HEIGHT_WIDTH * BLOCK_HEIGHT_WIDTH)

/* Border is for unrestricted mv's */
#define UMV_BORDER              16
#define STRIDE_EXTRA            (UMV_BORDER * 2)

#define Q_TABLE_SIZE            64

#define KEY_FRAME               0
#define DELTA_FRAME             1

#define MAX_MODES               8
#define MODE_BITS               3
#define MODE_METHODS            8
#define MODE_METHOD_BITS        3

/* Different key frame types/methods */
#define DCT_KEY_FRAME           0

#define KEY_FRAME_CONTEXT       5

/* Number of search sites for a 4-step search (at pixel accuracy) */
#define MAX_SEARCH_SITES       33

#define MAX_MV_EXTENT 31  /* Max search distance in half pixel increments */

/** block coding modes */
typedef enum{
  CODE_INTER_NO_MV        = 0x0, /* INTER prediction, (0,0) motion
                                    vector implied.  */
    CODE_INTRA            = 0x1, /* INTRA i.e. no prediction. */
    CODE_INTER_PLUS_MV    = 0x2, /* INTER prediction, non zero motion
                                    vector. */
    CODE_INTER_LAST_MV    = 0x3, /* Use Last Motion vector */
    CODE_INTER_PRIOR_LAST = 0x4, /* Prior last motion vector */
    CODE_USING_GOLDEN     = 0x5, /* 'Golden frame' prediction (no MV). */
    CODE_GOLDEN_MV        = 0x6, /* 'Golden frame' prediction plus MV. */
    CODE_INTER_FOURMV     = 0x7  /* Inter prediction 4MV per macro block. */
} coding_mode_t;

/** Huffman table entry */
typedef struct HUFF_ENTRY {
  struct HUFF_ENTRY *ZeroChild;
  struct HUFF_ENTRY *OneChild;
  struct HUFF_ENTRY *Previous;
  struct HUFF_ENTRY *Next;
  ogg_int32_t        Value;
  ogg_uint32_t       Frequency;
} HUFF_ENTRY;

typedef struct {
  ogg_int16_t data[64];
} dct_t;

typedef struct{
  ogg_int32_t   x;
  ogg_int32_t   y;
} mv_t;

typedef struct macroblock {
  /* the blocks comprising this macroblock */
  int yuv[3][4]; /* [Y,U,V][raster order] */

  coding_mode_t mode;
  mv_t mv[4];

  char coded;
} macroblock_t;

#define SB_MB_BLFRAG(sb,mbnum) ((sb).f[ ((mbnum)<2? ((mbnum)==0?0:4) : ((mbnum)==2?8:14)) ])
typedef struct superblock {
  int f[16]; // hilbert order
  int m[16]; // hilbert order; 4 for Y, 4 for UZ in 4:4:4, 8 for UV in 4:2:2, 16 for UV in 4:2:0
} superblock_t;

typedef ogg_int16_t    quant_table[64];
typedef quant_table    quant_tables[64];

typedef ogg_int32_t    iquant_table[64];
typedef iquant_table   iquant_tables[64];

/* Encoder (Compressor) instance -- installed in a theora_state */
typedef struct CP_INSTANCE {
  /*This structure must be first.
    It contains entry points accessed by the decoder library's API wrapper, and
     is the only assumption that library makes about our internal format.*/
  oc_state_dispatch_vtbl dispatch_vtbl;

  theora_info      info;

  /* ogg bitpacker for use in packet coding, other API state */
  oggpack_buffer   *oggbuffer;
#ifdef LIBOGG2  /* Remember, this is just until we drop libogg1 */
  ogg_buffer_state *oggbufferstate;
#endif

  unsigned char   *frame;
  unsigned char   *recon;
  unsigned char   *golden;
  unsigned char   *lastrecon;
  ogg_uint32_t     frame_size;

  /* SuperBlock, MacroBLock and Fragment Information */
  unsigned char   *frag_coded;
  ogg_uint32_t    *frag_buffer_index;
  unsigned char   *frag_nonzero;
  ogg_int16_t     *frag_dc;
  dct_t           *frag_dct;

  macroblock_t    *macro;
  superblock_t    *super[3];

  ogg_uint32_t     frag_h[3];
  ogg_uint32_t     frag_v[3];
  ogg_uint32_t     frag_n[3];
  ogg_uint32_t     frag_total;

  ogg_uint32_t     macro_h;
  ogg_uint32_t     macro_v;
  ogg_uint32_t     macro_total;
  
  ogg_uint32_t     super_h[3];
  ogg_uint32_t     super_v[3];
  ogg_uint32_t     super_n[3];
  ogg_uint32_t     super_total;

  ogg_uint32_t     stride[3]; // stride of image and recon planes, accounting for borders
  ogg_uint32_t     offset[3]; // data offset of first coded pixel in plane

  /*********************************************************************/
  /* state and stats */
  
  int              HeadersWritten;
  ogg_uint32_t     LastKeyFrame;
  ogg_int64_t      CurrentFrame;
  unsigned char    FrameType;
  int              readyflag;
  int              packetflag;
  int              doneflag;
  
  /*********************************************************************/
  /* Token Buffers */

  unsigned char   *dct_token_storage;
  ogg_uint16_t    *dct_token_eb_storage;
  unsigned char   *dct_token[64];
  ogg_uint16_t    *dct_token_eb[64];

  ogg_uint32_t     dct_token_count[64];
  ogg_uint32_t     dct_token_ycount[64];

  ogg_uint32_t     dc_bits[2][DC_HUFF_CHOICES];
  ogg_uint32_t     ac_bits[2][AC_HUFF_CHOICES];

  ogg_int32_t      ModeCount[MAX_MODES]; /* Frequency count of modes */

  ogg_uint32_t     MVBits_0; /* count of bits used by MV coding mode 0 */
  ogg_uint32_t     MVBits_1; /* count of bits used by MV coding mode 1 */


  /********************************************************************/
  /* Setup */
  int              keyframe_granule_shift;
  int              BaseQ;
  int              GoldenFrameEnabled;
  int              InterPrediction;
  int              MotionCompensation;

  /* Controlling Block Selection */
  ogg_uint32_t     MVChangeFactor;
  ogg_uint32_t     FourMvChangeFactor;
  ogg_uint32_t     MinImprovementForNewMV;
  ogg_uint32_t     ExhaustiveSearchThresh;
  ogg_uint32_t     MinImprovementForFourMV;
  ogg_uint32_t     FourMVThreshold;

  /*********************************************************************/

  ogg_int32_t      MVPixelOffsetY[MAX_SEARCH_SITES];
  ogg_uint32_t     InterTripOutThresh;
  unsigned char    MVEnabled;
  ogg_uint32_t     MotionVectorSearchCount;
  ogg_uint32_t     FrameMVSearcOunt;
  ogg_int32_t      MVSearchSteps;
  ogg_int32_t      MVOffsetX[MAX_SEARCH_SITES];
  ogg_int32_t      MVOffsetY[MAX_SEARCH_SITES];
  ogg_int32_t      HalfPixelRef2Offset[9]; /* Offsets for half pixel
                                               compensation */
  signed char      HalfPixelXOffset[9];    /* Half pixel MV offsets for X */
  signed char      HalfPixelYOffset[9];    /* Half pixel MV offsets for Y */

  /* hufftables and quant setup ****************************************/

  HUFF_ENTRY      *HuffRoot_VP3x[NUM_HUFF_TABLES];
  ogg_uint32_t    *HuffCodeArray_VP3x[NUM_HUFF_TABLES];
  unsigned char   *HuffCodeLengthArray_VP3x[NUM_HUFF_TABLES];
  const unsigned char *ExtraBitLengths_VP3x;

  th_quant_info    quant_info;
  quant_tables     quant_tables[2][3];
  iquant_tables    iquant_tables[2][3];


  DspFunctions     dsp;  /* Selected functions for this platform */

} CP_INSTANCE;

#define clamp255(x) ((unsigned char)((((x)<0)-1) & ((x) | -((x)>255))))

extern void IDct1( ogg_int16_t *InputData,
                   ogg_int16_t *QuantMatrix,
                   ogg_int16_t *OutputData );

extern void ReconRefFrames (CP_INSTANCE *cpi);

extern void quantize( CP_INSTANCE *cpi,
		      ogg_int32_t *iquant_table,
                      ogg_int16_t *DCT_block,
                      ogg_int16_t *quantized_list);

extern void fdct_short ( ogg_int16_t *InputData, ogg_int16_t *OutputData );

extern void DPCMTokenize (CP_INSTANCE *cpi);

extern void TransformQuantizeBlock (CP_INSTANCE *cpi, coding_mode_t mode, int fi, mv_t mv) ;

extern void WriteQTables(CP_INSTANCE *cpi,oggpack_buffer *opb);

extern void InitQTables( CP_INSTANCE *cpi );

extern void InitHuffmanSet( CP_INSTANCE *cpi );

extern void ClearHuffmanSet( CP_INSTANCE *cpi );

extern void WriteHuffmanTrees(HUFF_ENTRY *HuffRoot[NUM_HUFF_TABLES],
                              oggpack_buffer *opb);

extern void PackAndWriteDFArray( CP_INSTANCE *cpi );

extern void InitMotionCompensation ( CP_INSTANCE *cpi );

extern ogg_uint32_t GetMBIntraError (CP_INSTANCE *cpi, 
				     macroblock_t *mp );

extern ogg_uint32_t GetMBInterError (CP_INSTANCE *cpi,
                                     unsigned char *SrcPtr,
                                     unsigned char *RefPtr,
				     macroblock_t *mp,
                                     ogg_int32_t LastXMV,
                                     ogg_int32_t LastYMV );

extern void WriteFrameHeader( CP_INSTANCE *cpi) ;

extern ogg_uint32_t GetMBMVInterError (CP_INSTANCE *cpi,
                                       unsigned char *RefFramePtr,
				       macroblock_t *mp,
                                       ogg_int32_t *MVPixelOffset,
                                       mv_t *MV );

extern ogg_uint32_t GetMBMVExhaustiveSearch (CP_INSTANCE *cpi,
                                             unsigned char *RefFramePtr,
					     macroblock_t *mp,
                                             mv_t *MV );

extern ogg_uint32_t GetFOURMVExhaustiveSearch (CP_INSTANCE *cpi,
                                               unsigned char *RefFramePtr,
					       macroblock_t *mp,
                                               mv_t *MV ) ;

extern void EncodeData(CP_INSTANCE *cpi);

extern ogg_uint32_t PickIntra( CP_INSTANCE *cpi );

extern ogg_uint32_t PickModes(CP_INSTANCE *cpi,
                              ogg_uint32_t *InterError,
                              ogg_uint32_t *IntraError);

extern void InitFrameInfo(CP_INSTANCE *cpi);

extern void ClearFrameInfo (CP_INSTANCE *cpi);

#endif /* ENCODER_INTERNAL_H */
