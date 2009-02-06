/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2007                *
 * by the Xiph.Org Foundation and contributors http://www.xiph.org/ *
 *                                                                  *
 ********************************************************************

  function:
  last mod: $Id$

 ********************************************************************/

#include <stdlib.h>
#include <string.h>
#include "codec_internal.h"
#include "quant_lookup.h"

static void SetupBoundingValueArray_Generic(ogg_int16_t *BoundingValuePtr,
                                            ogg_int32_t FLimit){

  ogg_int32_t i;

  /* Set up the bounding value array. */
  memset ( BoundingValuePtr, 0, (256*sizeof(*BoundingValuePtr)) );
  for ( i = 0; i < FLimit; i++ ){
    BoundingValuePtr[127-i-FLimit] = (-FLimit+i);
    BoundingValuePtr[127-i] = -i;
    BoundingValuePtr[127+i] = i;
    BoundingValuePtr[127+i+FLimit] = FLimit-i;
  }
}

static void UpdateUMV_HBorders( CP_INSTANCE *cpi,
                                unsigned char *DestReconPtr,
				int plane){
  ogg_uint32_t  i;
  ogg_uint32_t  PixelIndex;

  ogg_uint32_t  PlaneStride = cpi->stride[plane];
  ogg_uint32_t  BlockVStep = cpi->stride[plane] * (VFRAGPIXELS - 1);
  ogg_uint32_t  PlaneFragments = cpi->frag_n[plane];
  ogg_uint32_t  LineFragments = cpi->frag_h[plane];
  ogg_uint32_t  PlaneBorderWidth = (plane ? UMV_BORDER / 2 : UMV_BORDER );

  unsigned char   *SrcPtr1;
  unsigned char   *SrcPtr2;
  unsigned char   *DestPtr1;
  unsigned char   *DestPtr2;
  ogg_uint32_t    *bp = cpi->frag_buffer_index;

  if(plane) bp += cpi->frag_n[0];
  if(plane>1) bp += cpi->frag_n[1];

  /* Setup the source and destination pointers for the top and bottom
     borders */
  PixelIndex = bp[0];
  SrcPtr1 = &DestReconPtr[ PixelIndex - PlaneBorderWidth ];
  DestPtr1 = SrcPtr1 - (PlaneBorderWidth * PlaneStride);

  PixelIndex = bp[PlaneFragments - LineFragments] + BlockVStep;
  SrcPtr2 = &DestReconPtr[ PixelIndex - PlaneBorderWidth];
  DestPtr2 = SrcPtr2 + PlaneStride;

  /* Now copy the top and bottom source lines into each line of the
     respective borders */
  for ( i = 0; i < PlaneBorderWidth; i++ ) {
    memcpy( DestPtr1, SrcPtr1, PlaneStride );
    memcpy( DestPtr2, SrcPtr2, PlaneStride );
    DestPtr1 += PlaneStride;
    DestPtr2 += PlaneStride;
  }
}

static void UpdateUMV_VBorders( CP_INSTANCE *cpi,
                                unsigned char * DestReconPtr,
                                int plane){
  ogg_uint32_t   i;
  ogg_uint32_t   PixelIndex;

  ogg_uint32_t   PlaneStride = cpi->stride[plane];
  ogg_uint32_t   LineFragments = cpi->frag_h[plane];
  ogg_uint32_t   PlaneBorderWidth = (plane ? UMV_BORDER / 2 : UMV_BORDER );
  ogg_uint32_t   PlaneHeight = (plane ? cpi->info.height/2 : cpi->info.height );

  unsigned char   *SrcPtr1;
  unsigned char   *SrcPtr2;
  unsigned char   *DestPtr1;
  unsigned char   *DestPtr2;
  ogg_uint32_t    *bp = cpi->frag_buffer_index;

  if(plane) bp += cpi->frag_n[0];
  if(plane>1) bp += cpi->frag_n[1];

  /* Setup the source data values and destination pointers for the
     left and right edge borders */
  PixelIndex = bp[0];
  SrcPtr1 = &DestReconPtr[ PixelIndex ];
  DestPtr1 = &DestReconPtr[ PixelIndex - PlaneBorderWidth ];

  PixelIndex = bp[LineFragments - 1] + (HFRAGPIXELS - 1);
  SrcPtr2 = &DestReconPtr[ PixelIndex ];
  DestPtr2 = &DestReconPtr[ PixelIndex + 1 ];

  /* Now copy the top and bottom source lines into each line of the
     respective borders */
  for ( i = 0; i < PlaneHeight; i++ ) {
    memset( DestPtr1, SrcPtr1[0], PlaneBorderWidth );
    memset( DestPtr2, SrcPtr2[0], PlaneBorderWidth );
    SrcPtr1 += PlaneStride;
    SrcPtr2 += PlaneStride;
    DestPtr1 += PlaneStride;
    DestPtr2 += PlaneStride;
  }
}

void UpdateUMVBorder( CP_INSTANCE *cpi,
                      unsigned char * DestReconPtr ) {
  /* Y plane */
  UpdateUMV_VBorders( cpi, DestReconPtr, 0);
  UpdateUMV_HBorders( cpi, DestReconPtr, 0);

  /* Then the U and V Planes */
  UpdateUMV_VBorders( cpi, DestReconPtr, 1);
  UpdateUMV_HBorders( cpi, DestReconPtr, 1);

  UpdateUMV_VBorders( cpi, DestReconPtr, 2);
  UpdateUMV_HBorders( cpi, DestReconPtr, 2);
}

static void loop_filter_h(unsigned char * PixelPtr,
			  ogg_int32_t LineLength,
			  ogg_int16_t *BoundingValuePtr){
  ogg_int32_t j;
  ogg_int32_t FiltVal;
  PixelPtr-=2;

  for ( j = 0; j < 8; j++ ){
    FiltVal =
      ( PixelPtr[0] ) -
      ( PixelPtr[1] * 3 ) +
      ( PixelPtr[2] * 3 ) -
      ( PixelPtr[3] );

    FiltVal = *(BoundingValuePtr+((FiltVal + 4) >> 3));

    PixelPtr[1] = clamp255(PixelPtr[1] + FiltVal);
    PixelPtr[2] = clamp255(PixelPtr[2] - FiltVal);

    PixelPtr += LineLength;
  }
}

static void loop_filter_v(unsigned char * PixelPtr,
			  ogg_int32_t LineLength,
			  ogg_int16_t *BoundingValuePtr){
  ogg_int32_t j;
  ogg_int32_t FiltVal;
  PixelPtr -= 2*LineLength;

  for ( j = 0; j < 8; j++ ) {
    FiltVal = ( (ogg_int32_t)PixelPtr[0] ) -
      ( (ogg_int32_t)PixelPtr[LineLength] * 3 ) +
      ( (ogg_int32_t)PixelPtr[2 * LineLength] * 3 ) -
      ( (ogg_int32_t)PixelPtr[3 * LineLength] );

    FiltVal = *(BoundingValuePtr+((FiltVal + 4) >> 3));

    PixelPtr[LineLength] = clamp255(PixelPtr[LineLength] + FiltVal);
    PixelPtr[2 * LineLength] = clamp255(PixelPtr[2*LineLength] - FiltVal);

    PixelPtr ++;
  }
}

static void LoopFilter__c(CP_INSTANCE *cpi, int FLimit){

  int j;
  ogg_int16_t BoundingValues[256];
  ogg_int16_t *bvp = BoundingValues+127;
  unsigned char *cp = cpi->frag_coded;
  ogg_uint32_t *bp = cpi->frag_buffer_index;

  if ( FLimit == 0 ) return;
  SetupBoundingValueArray_Generic(BoundingValues, FLimit);

  for ( j = 0; j < 3 ; j++){
    ogg_uint32_t *bp_begin = bp;
    ogg_uint32_t *bp_end = bp + cpi->frag_n[j];
    int stride = cpi->stride[j];
    int h = cpi->frag_h[j];

    while(bp<bp_end){
      ogg_uint32_t *bp_left = bp;
      ogg_uint32_t *bp_right = bp + h;
      while(bp<bp_right){
	if(cp[0]){
	  if(bp>bp_left)
	    loop_filter_h(&cpi->lastrecon[bp[0]],stride,bvp);
	  if(bp_left>bp_begin)
	    loop_filter_v(&cpi->lastrecon[bp[0]],stride,bvp);
	  if(bp+1<bp_right && !cp[1])
	    loop_filter_h(&cpi->lastrecon[bp[0]]+8,stride,bvp);
	  if(bp+h<bp_end && !cp[h])
	    loop_filter_v(&cpi->lastrecon[bp[h]],stride,bvp);
	}
	bp++;
	cp++;
      }
    }
  }
}

void ReconRefFrames (CP_INSTANCE *cpi){
  unsigned char *temp = cpi->lastrecon;

  /* swap */
  cpi->lastrecon=cpi->recon;
  cpi->recon=temp;

  /* Apply a loop filter to edge pixels of updated blocks */
  dsp_LoopFilter(cpi->dsp, cpi, cpi->quant_info.loop_filter_limits[cpi->BaseQ] /* temp */);

  /* We may need to update the UMV border */
  UpdateUMVBorder(cpi, cpi->lastrecon);
  
  if ( cpi->FrameType == KEY_FRAME )
    memcpy(cpi->golden,cpi->lastrecon,sizeof(*cpi->lastrecon)*cpi->frame_size);

}

void dsp_dct_decode_init (DspFunctions *funcs, ogg_uint32_t cpu_flags)
{
  funcs->LoopFilter = LoopFilter__c;
#if defined(USE_ASM)
  if (cpu_flags & OC_CPU_X86_MMX) {
    dsp_mmx_dct_decode_init(funcs);
  }
#endif
}
