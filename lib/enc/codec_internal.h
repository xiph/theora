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

typedef struct{
  ogg_int32_t   x;
  ogg_int32_t   y;
} mv_t;

typedef struct fragment {
  int coded;
  coding_mode_t mode;
  mv_t mv;
  ogg_int16_t pred_dc;
  ogg_int16_t dct[64];

  unsigned char nonzero;
  ogg_uint32_t  token_list[128];
  unsigned char tokens_coded;
  unsigned char coeffs_packed;
  unsigned char tokens_packed;

  ogg_uint32_t buffer_index;

} fragment_t;

typedef struct macroblock {
  int mode;
  fragment_t *y[4]; // MV (raster) order
  fragment_t *u;
  fragment_t *v;
} macroblock_t;

#define SB_MB_BLFRAG(sb,mbnum) ((sb).f[ ((mbnum)<2? ((mbnum)==0?0:4) : ((mbnum)==2?8:14)) ])
typedef struct superblock {
  fragment_t *f[16]; // hilbert order
  macroblock_t *m[4]; // hilbert order
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

  unsigned char   *frame;
  unsigned char   *recon;
  unsigned char   *golden;
  unsigned char   *lastrecon;
  
  /* flag to indicate if he headers already have been written */
  int              HeadersWritten;
  /* how far do we shift the granulepos to seperate out P frame counts? */
  int              keyframe_granule_shift;

  /* Compressor Configuration */
  int              BaseQ;
  int              GoldenFrameEnabled;
  int              InterPrediction;
  int              MotionCompensation;

  ogg_uint32_t     LastKeyFrame;

  /* Compressor Statistics */
  ogg_int64_t      KeyFrameCount; /* Count of key frames. */
  ogg_int64_t      TotKeyFrameBytes;

  /* Frame Statistics  */
  ogg_int64_t      CurrentFrame;
  int              ThisIsFirstFrame;
  int              ThisIsKeyFrame;

  /* Controlling Block Selection */
  ogg_uint32_t     MVChangeFactor;
  ogg_uint32_t     FourMvChangeFactor;
  ogg_uint32_t     MinImprovementForNewMV;
  ogg_uint32_t     ExhaustiveSearchThresh;
  ogg_uint32_t     MinImprovementForFourMV;
  ogg_uint32_t     FourMVThreshold;

  /*********************************************************************/
  /* Token Buffers */
  ogg_uint32_t    *OptimisedTokenListEb; /* Optimised token list extra bits */
  unsigned char   *OptimisedTokenList;   /* Optimised token list. */
  unsigned char   *OptimisedTokenListHi; /* Optimised token list huffman
                                             table index */

  unsigned char    *OptimisedTokenListPl; /* Plane to which the token
                                             belongs Y = 0 or UV = 1 */
  ogg_int32_t      OptimisedTokenCount;           /* Count of Optimized tokens */
  ogg_uint32_t     RunHuffIndex;         /* Huffman table in force at
                                             the start of a run */
  ogg_uint32_t     RunPlaneIndex;        /* The plane (Y=0 UV=1) to
                                             which the first token in
                                             an EOB run belonged. */
  ogg_uint32_t     TotTokenCount;
  ogg_int32_t      TokensToBeCoded;
  ogg_int32_t      TokensCoded;
  /********************************************************************/
  /* SuperBlock, MacroBLock and Fragment Information */
  fragment_t      *frag[3];
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

  /* Coded flag arrays and counters for them */

  ogg_uint32_t    *RunHuffIndices;

  unsigned char   *BlockCodedFlags;

  mv_t            *MVList;
  ogg_uint32_t     MvListCount;
  ogg_uint32_t    *ModeList;
  ogg_uint32_t     ModeListCount;

  unsigned char   *DataOutputBuffer;

  unsigned char    FrameType;
  int              CodedBlockIndex;
  fragment_t     **CodedBlockList;           

  /*********************************************************************/

  ogg_uint32_t     RunLength;

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

  /* ogg bitpacker for use in packet coding, other API state */
  oggpack_buffer   *oggbuffer;
#ifdef LIBOGG2  /* Remember, this is just until we drop libogg1 */
  ogg_buffer_state *oggbufferstate;
#endif
  int              readyflag;
  int              packetflag;
  int              doneflag;

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

extern ogg_uint32_t DPCMTokenizeBlock (CP_INSTANCE *cpi, fragment_t *fp);

extern void TransformQuantizeBlock (CP_INSTANCE *cpi, fragment_t *fp) ;

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
                                     unsigned char * SrcPtr,
                                     unsigned char * RefPtr,
				     macroblock_t *mp,
                                     ogg_int32_t LastXMV,
                                     ogg_int32_t LastYMV );

extern void WriteFrameHeader( CP_INSTANCE *cpi) ;

extern ogg_uint32_t GetMBMVInterError (CP_INSTANCE *cpi,
                                       unsigned char * RefFramePtr,
				       macroblock_t *mp,
                                       ogg_int32_t *MVPixelOffset,
                                       mv_t *MV );

extern ogg_uint32_t GetMBMVExhaustiveSearch (CP_INSTANCE *cpi,
                                             unsigned char * RefFramePtr,
					     macroblock_t *mp,
                                             mv_t *MV );

extern ogg_uint32_t GetFOURMVExhaustiveSearch (CP_INSTANCE *cpi,
                                               unsigned char * RefFramePtr,
					       macroblock_t *mp,
                                               mv_t *MV ) ;

extern void EncodeData(CP_INSTANCE *cpi);

extern ogg_uint32_t PickIntra( CP_INSTANCE *cpi );

extern ogg_uint32_t PickModes(CP_INSTANCE *cpi,
                              ogg_uint32_t SBRows,
                              ogg_uint32_t SBCols,
                              ogg_uint32_t *InterError,
                              ogg_uint32_t *IntraError);

extern void InitFrameInfo(CP_INSTANCE *cpi);

extern void ClearFrameInfo (CP_INSTANCE *cpi);

#endif /* ENCODER_INTERNAL_H */
