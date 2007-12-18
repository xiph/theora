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

static void ExpandBlock ( CP_INSTANCE *cpi, int fi){
  fragment_t   *fp = &cpi->frag[0][fi]; 
  int           mode = cpi->frag_mode[0][fi];
  int           qi = cpi->BaseQ; // temporary 
  int           plane = (fp<cpi->frag[1] ? 0 : (fp<cpi->frag[2] ? 1 : 2));
  int           inter = (mode != CODE_INTRA);
  ogg_int16_t   reconstruct[64];
  ogg_int16_t  *quantizers = cpi->quant_tables[inter][plane][qi];
  ogg_int16_t  *data = fp->dct;
  
  /* Invert quantisation and DCT to get pixel data. */
  switch(fp->nonzero){
  case 0:case 1:
    IDct1( data, quantizers, reconstruct );
    break;
  case 2: case 3:
    dsp_IDct3(cpi->dsp, data, quantizers, reconstruct );
    break;
  case 4:case 5:case 6:case 7:case 8: case 9:case 10:
    dsp_IDct10(cpi->dsp, data, quantizers, reconstruct );
    break;
  default:
    dsp_IDctSlow(cpi->dsp, data, quantizers, reconstruct );
  }
  
  /* Convert fragment number to a pixel offset in a reconstruction buffer. */
  dsp_recon8x8 (cpi->dsp, &cpi->recon[fp->buffer_index],
		reconstruct, cpi->stride[plane]);

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
  
  fragment_t      *fp = cpi->frag[plane];

  /* Setup the source and destination pointers for the top and bottom
     borders */
  PixelIndex = fp[0].buffer_index;
  SrcPtr1 = &DestReconPtr[ PixelIndex - PlaneBorderWidth ];
  DestPtr1 = SrcPtr1 - (PlaneBorderWidth * PlaneStride);

  PixelIndex = fp[PlaneFragments - LineFragments].buffer_index + BlockVStep;
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

  fragment_t      *fp = cpi->frag[plane];

  /* Setup the source data values and destination pointers for the
     left and right edge borders */
  PixelIndex = fp[0].buffer_index;
  SrcPtr1 = &DestReconPtr[ PixelIndex ];
  DestPtr1 = &DestReconPtr[ PixelIndex - PlaneBorderWidth ];

  PixelIndex = fp[LineFragments - 1].buffer_index + (HFRAGPIXELS - 1);
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

static void CopyRecon( CP_INSTANCE *cpi, unsigned char * DestReconPtr,
		       unsigned char * SrcReconPtr ) {
  ogg_uint32_t  i,plane;
  fragment_t *fp = cpi->frag[0];
  unsigned char *cp = cpi->frag_coded[0];

  /* Copy over only updated blocks.*/
  for(plane=0;plane<3;plane++){  
    int PlaneLineStep = cpi->stride[plane];
    for ( i = 0; i < cpi->frag_n[plane]; i++,fp++ ) {
      if ( cp[i] ) {
	int pi= fp->buffer_index;
	unsigned char *src = &SrcReconPtr[ pi ];
	unsigned char *dst = &DestReconPtr[ pi ];
	dsp_copy8x8 (cpi->dsp, src, dst, PlaneLineStep);
      }
    }
  }
}

static void FilterHoriz__c(unsigned char * PixelPtr,
			   ogg_int32_t LineLength,
			   ogg_int16_t *BoundingValuePtr){
  ogg_int32_t j;
  ogg_int32_t FiltVal;

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

static void FilterVert__c(unsigned char * PixelPtr,
                ogg_int32_t LineLength,
                ogg_int16_t *BoundingValuePtr){
  ogg_int32_t j;
  ogg_int32_t FiltVal;
  PixelPtr -= 2*LineLength;
  /* the math was correct, but negative array indicies are forbidden
     by ANSI/C99 and will break optimization on several modern
     compilers */

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

static void LoopFilter(CP_INSTANCE *cpi){

  ogg_int16_t BoundingValues[256];
  ogg_int16_t *BoundingValuePtr = BoundingValues+127;
  ogg_int32_t FLimit = cpi->quant_info.loop_filter_limits[cpi->BaseQ]; // temp
  int j,m,n;
  fragment_t *fp;
  unsigned char *cp;

  if ( FLimit == 0 ) return;
  SetupBoundingValueArray_Generic(BoundingValues, FLimit);

  for ( j = 0; j < 3 ; j++){
    ogg_int32_t LineFragments = cpi->frag_h[j];
    ogg_int32_t LineLength = cpi->stride[j];
    fp = cpi->frag[j];
    cp = cpi->frag_coded[j];

    /**************************************************************
     First Row
    **************************************************************/
    /* first column conditions */
    /* only do 2 prediction if fragment coded and on non intra or if
       all fragments are intra */
    if( cp[0]){
      /* Filter right hand border only if the block to the right is
         not coded */
      if ( !cp[1] ){
        dsp_FilterHoriz(cpi->dsp,cpi->lastrecon+
			fp[0].buffer_index+6,
			LineLength,BoundingValuePtr);
      }

      /* Bottom done if next row set */
      if( !cp[LineFragments] ){
        dsp_FilterVert(cpi->dsp,cpi->lastrecon+
		       fp[LineFragments].buffer_index,
		       LineLength, BoundingValuePtr);
      }
    }
    fp++;
    cp++;

    /***************************************************************/
    /* middle columns  */
    for ( n = 1 ; n < cpi->frag_h[j] - 1 ; n++, fp++) {
      if( cp[0]){
        /* Filter Left edge always */
        dsp_FilterHoriz(cpi->dsp,cpi->lastrecon+
			fp[0].buffer_index-2,
			LineLength, BoundingValuePtr);

        /* Filter right hand border only if the block to the right is
           not coded */
        if ( !cp[1] ){
          dsp_FilterHoriz(cpi->dsp,cpi->lastrecon+
			  fp[0].buffer_index+6,
			  LineLength, BoundingValuePtr);
        }

        /* Bottom done if next row set */
        if( !cp[LineFragments] ){
          dsp_FilterVert(cpi->dsp,cpi->lastrecon+
			 fp[LineFragments].buffer_index,
			 LineLength, BoundingValuePtr);
        }
	
      }
    }

    /***************************************************************/
    /* Last Column */
    if(cp[0]){
      /* Filter Left edge always */
      dsp_FilterHoriz(cpi->dsp,cpi->lastrecon+
		      fp[0].buffer_index - 2 ,
		      LineLength, BoundingValuePtr);
      
      /* Bottom done if next row set */
      if( !cp[LineFragments] ){
        dsp_FilterVert(cpi->dsp,cpi->lastrecon+
		       fp[LineFragments].buffer_index,
		       LineLength, BoundingValuePtr);
      }
    }
    fp++;
    cp++;

    /***************************************************************/
    /* Middle Rows */
    /***************************************************************/
    for ( m = 1 ; m < cpi->frag_v[j]-1 ; m++) {

      /*****************************************************************/
      /* first column conditions */
      /* only do 2 prediction if fragment coded and on non intra or if
         all fragments are intra */
      if( cp[0] ){
        /* TopRow is always done */
        dsp_FilterVert(cpi->dsp,cpi->lastrecon+
		       fp[0].buffer_index,
		       LineLength, BoundingValuePtr);

        /* Filter right hand border only if the block to the right is
           not coded */
        if ( !cp[1] ){
          dsp_FilterHoriz(cpi->dsp,cpi->lastrecon+
			  fp[0].buffer_index + 6,
			  LineLength, BoundingValuePtr);
        }
	
        /* Bottom done if next row set */
        if( !cp[LineFragments] ){
          dsp_FilterVert(cpi->dsp,cpi->lastrecon+
			 fp[LineFragments].buffer_index,
			 LineLength, BoundingValuePtr);
        }
      }
      fp++;
      cp++;

      /*****************************************************************/
      /* middle columns  */
      for ( n = 1 ; n < cpi->frag_h[j] - 1 ; n++, fp++){
        if( cp[0] ){
          /* Filter Left edge always */
          dsp_FilterHoriz(cpi->dsp,cpi->lastrecon+
			  fp[0].buffer_index - 2,
			  LineLength, BoundingValuePtr);

          /* TopRow is always done */
          dsp_FilterVert(cpi->dsp,cpi->lastrecon+
			 fp[0].buffer_index,
			 LineLength, BoundingValuePtr);
	  
          /* Filter right hand border only if the block to the right
             is not coded */
          if ( !cp[1] ){
            dsp_FilterHoriz(cpi->dsp,cpi->lastrecon+
			    fp[0].buffer_index + 6,
			    LineLength, BoundingValuePtr);
          }

          /* Bottom done if next row set */
          if( !cp[LineFragments] ){
            dsp_FilterVert(cpi->dsp,cpi->lastrecon+
			   fp[LineFragments].buffer_index,
			   LineLength, BoundingValuePtr);
          }
        }
      }

      /******************************************************************/
      /* Last Column */
      if(cp[0]){
        /* Filter Left edge always*/
        dsp_FilterHoriz(cpi->dsp,cpi->lastrecon+
			fp[0].buffer_index - 2,
			LineLength, BoundingValuePtr);

        /* TopRow is always done */
        dsp_FilterVert(cpi->dsp,cpi->lastrecon+
		       fp[0].buffer_index,		       
		       LineLength, BoundingValuePtr);
	
        /* Bottom done if next row set */
        if( !cp[LineFragments] ){
          dsp_FilterVert(cpi->dsp,cpi->lastrecon+
			 fp[LineFragments].buffer_index,
			 LineLength, BoundingValuePtr);
        }
      }
      fp++;
      cp++;
    }

    /*******************************************************************/
    /* Last Row  */

    /* first column conditions */
    /* only do 2 prediction if fragment coded and on non intra or if
       all fragments are intra */
    if(cp[0]){

      /* TopRow is always done */
      dsp_FilterVert(cpi->dsp,cpi->lastrecon+
		     fp[0].buffer_index,
		     LineLength, BoundingValuePtr);

      /* Filter right hand border only if the block to the right is
         not coded */
      if ( !cp[1] ){
        dsp_FilterHoriz(cpi->dsp,cpi->lastrecon+
			fp[0].buffer_index + 6,
			LineLength, BoundingValuePtr);
      }
    }
    fp++;
    cp++;

    /******************************************************************/
    /* middle columns  */
    for ( n = 1 ; n < cpi->frag_h[j] - 1 ; n++, fp++){
      if( cp[0] ){
        /* Filter Left edge always */
        dsp_FilterHoriz(cpi->dsp,cpi->lastrecon+
			fp[0].buffer_index - 2,
			LineLength, BoundingValuePtr);
	
        /* TopRow is always done */
        dsp_FilterVert(cpi->dsp,cpi->lastrecon+
		       fp[0].buffer_index,
		       LineLength, BoundingValuePtr);

        /* Filter right hand border only if the block to the right is
           not coded */
        if ( !cp[1] ){
          dsp_FilterHoriz(cpi->dsp,cpi->lastrecon+
			  fp[0].buffer_index + 6,
			  LineLength, BoundingValuePtr);
        }
      }
    }

    /******************************************************************/
    /* Last Column */
    if( cp[0] ){
      /* Filter Left edge always */
      dsp_FilterHoriz(cpi->dsp,cpi->lastrecon+
		      fp[0].buffer_index - 2,
		      LineLength, BoundingValuePtr);

      /* TopRow is always done */
      dsp_FilterVert(cpi->dsp,cpi->lastrecon+
		     fp[0].buffer_index,
		     LineLength, BoundingValuePtr);
      
    }
    fp++;
    cp++;
  }
}

void ReconRefFrames (CP_INSTANCE *cpi){
  fragment_t *fp = cpi->coded_tail;
  
  while(fp){
    ExpandBlock( cpi, fp-cpi->frag[0] );
    fp = fp->next;
  }

  memcpy(cpi->lastrecon,cpi->recon,sizeof(*cpi->recon)*cpi->frame_size);

  /* Apply a loop filter to edge pixels of updated blocks */
  LoopFilter(cpi);

  /* We may need to update the UMV border */
  UpdateUMVBorder(cpi, cpi->lastrecon);
  
  /* Reconstruct the golden frame if necessary.
     For VFW codec only on key frames */
  if ( cpi->FrameType == KEY_FRAME ){
    CopyRecon( cpi, cpi->golden, cpi->lastrecon );
    /* We may need to update the UMV border */
    UpdateUMVBorder(cpi, cpi->golden);
  }
}

void dsp_dct_decode_init (DspFunctions *funcs, ogg_uint32_t cpu_flags)
{
  funcs->FilterVert = FilterVert__c;
  funcs->FilterHoriz = FilterHoriz__c;
#if defined(USE_ASM)
  if (cpu_flags & OC_CPU_X86_MMX) {
    dsp_mmx_dct_decode_init(funcs);
  }
#endif
}
