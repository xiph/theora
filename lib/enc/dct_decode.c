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

static void SetupBoundingValueArray_Generic(PB_INSTANCE *pbi,
                                            ogg_int32_t FLimit){

  ogg_int16_t * BoundingValuePtr = pbi->FiltBoundingValue+127;
  ogg_int32_t i;

  /* Set up the bounding value array. */
  memset ( pbi->FiltBoundingValue, 0, (256*sizeof(*pbi->FiltBoundingValue)) );
  for ( i = 0; i < FLimit; i++ ){
    BoundingValuePtr[-i-FLimit] = (-FLimit+i);
    BoundingValuePtr[-i] = -i;
    BoundingValuePtr[i] = i;
    BoundingValuePtr[i+FLimit] = FLimit-i;
  }
}

static void ExpandBlock ( CP_INSTANCE *cpi, ogg_int32_t FragmentNumber){
  PB_INSTANCE   *pbi = &cpi->pb;
  ogg_uint32_t   ReconPixelsPerLine; /* Pixels per line */
  ogg_int32_t    ReconPixelIndex;    /* Offset for block into a
                                        reconstruction buffer */
  ogg_int16_t    reconstruct[64];
  ogg_int16_t  *quantizers;
  ogg_int16_t   *data = &pbi->QFragData[FragmentNumber][0];

  int            mode = pbi->FragCodingMethod[FragmentNumber];
  int            qi = cpi->BaseQ; // temporary 

  /* Select the appropriate inverse Q matrix and line stride */
  if ( FragmentNumber<pbi->YPlaneFragments ) {
    ReconPixelsPerLine = pbi->YStride;
    if ( mode == CODE_INTRA )
      quantizers = pbi->quant_tables[0][0][qi];
    else
      quantizers = pbi->quant_tables[1][0][qi];

  }else{
    ReconPixelsPerLine = pbi->UVStride;
    if ( mode == CODE_INTRA )
      if ( FragmentNumber < pbi->YPlaneFragments + pbi->UVPlaneFragments )
	quantizers = pbi->quant_tables[0][1][qi];
      else
	quantizers = pbi->quant_tables[0][2][qi];
    else
      if ( FragmentNumber < pbi->YPlaneFragments + pbi->UVPlaneFragments )
	quantizers = pbi->quant_tables[1][1][qi];
      else
	quantizers = pbi->quant_tables[1][2][qi];
  }

#ifdef _TH_DEBUG_
 {
   int i;
   for(i=0;i<64;i++)
     pbi->QFragFREQ[FragmentNumber][dezigzag_index[i]]= 
       pbi->quantized_list[i] * quantizers[i];
 }
#endif

  /* Invert quantisation and DCT to get pixel data. */
  switch(pbi->FragCoefEOB[FragmentNumber]){
  case 0:case 1:
    IDct1( data, quantizers, reconstruct );
    break;
  case 2: case 3:
    dsp_IDct3(pbi->dsp, data, quantizers, reconstruct );
    break;
  case 4:case 5:case 6:case 7:case 8: case 9:case 10:
    dsp_IDct10(pbi->dsp, data, quantizers, reconstruct );
    break;
  default:
    dsp_IDctSlow(pbi->dsp, data, quantizers, reconstruct );
  }

#ifdef _TH_DEBUG_
 {
   int i;
   for(i=0;i<64;i++)
     pbi->QFragTIME[FragmentNumber][i]= reconstruct[i];
 }
#endif

  /* Convert fragment number to a pixel offset in a reconstruction buffer. */
  ReconPixelIndex = pbi->recon_pixel_index_table[FragmentNumber];
  dsp_recon8x8 (pbi->dsp, &pbi->ThisFrameRecon[ReconPixelIndex],
		reconstruct, ReconPixelsPerLine);

}

static void UpdateUMV_HBorders( PB_INSTANCE *pbi,
                                unsigned char * DestReconPtr,
                                ogg_uint32_t  PlaneFragOffset ) {
  ogg_uint32_t  i;
  ogg_uint32_t  PixelIndex;

  ogg_uint32_t  PlaneStride;
  ogg_uint32_t  BlockVStep;
  ogg_uint32_t  PlaneFragments;
  ogg_uint32_t  LineFragments;
  ogg_uint32_t  PlaneBorderWidth;

  unsigned char   *SrcPtr1;
  unsigned char   *SrcPtr2;
  unsigned char   *DestPtr1;
  unsigned char   *DestPtr2;

  /* Work out various plane specific values */
  if ( PlaneFragOffset == 0 ) {
    /* Y Plane */
    BlockVStep = (pbi->YStride *
                  (VFRAGPIXELS - 1));
    PlaneStride = pbi->YStride;
    PlaneBorderWidth = UMV_BORDER;
    PlaneFragments = pbi->YPlaneFragments;
    LineFragments = pbi->HFragments;
  }else{
    /* U or V plane. */
    BlockVStep = (pbi->UVStride *
                  (VFRAGPIXELS - 1));
    PlaneStride = pbi->UVStride;
    PlaneBorderWidth = UMV_BORDER / 2;
    PlaneFragments = pbi->UVPlaneFragments;
    LineFragments = pbi->HFragments / 2;
  }

  /* Setup the source and destination pointers for the top and bottom
     borders */
  PixelIndex = pbi->recon_pixel_index_table[PlaneFragOffset];
  SrcPtr1 = &DestReconPtr[ PixelIndex - PlaneBorderWidth ];
  DestPtr1 = SrcPtr1 - (PlaneBorderWidth * PlaneStride);

  PixelIndex = pbi->recon_pixel_index_table[PlaneFragOffset +
                                           PlaneFragments - LineFragments] +
    BlockVStep;
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
                                ogg_uint32_t  PlaneFragOffset ){
  PB_INSTANCE   *pbi = &cpi->pb;
  ogg_uint32_t   i;
  ogg_uint32_t   PixelIndex;

  ogg_uint32_t   PlaneStride;
  ogg_uint32_t   LineFragments;
  ogg_uint32_t   PlaneBorderWidth;
  ogg_uint32_t   PlaneHeight;

  unsigned char   *SrcPtr1;
  unsigned char   *SrcPtr2;
  unsigned char   *DestPtr1;
  unsigned char   *DestPtr2;

  /* Work out various plane specific values */
  if ( PlaneFragOffset == 0 ) {
    /* Y Plane */
    PlaneStride = pbi->YStride;
    PlaneBorderWidth = UMV_BORDER;
    LineFragments = pbi->HFragments;
    PlaneHeight = cpi->info.height;
  }else{
    /* U or V plane. */
    PlaneStride = pbi->UVStride;
    PlaneBorderWidth = UMV_BORDER / 2;
    LineFragments = pbi->HFragments / 2;
    PlaneHeight = cpi->info.height / 2;
  }

  /* Setup the source data values and destination pointers for the
     left and right edge borders */
  PixelIndex = pbi->recon_pixel_index_table[PlaneFragOffset];
  SrcPtr1 = &DestReconPtr[ PixelIndex ];
  DestPtr1 = &DestReconPtr[ PixelIndex - PlaneBorderWidth ];

  PixelIndex = pbi->recon_pixel_index_table[PlaneFragOffset +
                                           LineFragments - 1] +
    (HFRAGPIXELS - 1);
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
  ogg_uint32_t  PlaneFragOffset;
  PB_INSTANCE   *pbi = &cpi->pb;

  /* Y plane */
  PlaneFragOffset = 0;
  UpdateUMV_VBorders( cpi, DestReconPtr, PlaneFragOffset );
  UpdateUMV_HBorders( pbi, DestReconPtr, PlaneFragOffset );

  /* Then the U and V Planes */
  PlaneFragOffset = pbi->YPlaneFragments;
  UpdateUMV_VBorders( cpi, DestReconPtr, PlaneFragOffset );
  UpdateUMV_HBorders( pbi, DestReconPtr, PlaneFragOffset );

  PlaneFragOffset = pbi->YPlaneFragments + pbi->UVPlaneFragments;
  UpdateUMV_VBorders( cpi, DestReconPtr, PlaneFragOffset );
  UpdateUMV_HBorders( pbi, DestReconPtr, PlaneFragOffset );
}

static void CopyRecon( PB_INSTANCE *pbi, unsigned char * DestReconPtr,
                unsigned char * SrcReconPtr ) {
  ogg_uint32_t  i;
  ogg_uint32_t  PlaneLineStep; /* Pixels per line */
  ogg_uint32_t  PixelIndex;

  unsigned char  *SrcPtr;      /* Pointer to line of source image data */
  unsigned char  *DestPtr;     /* Pointer to line of destination image data */

  /* Copy over only updated blocks.*/

  /* First Y plane */
  PlaneLineStep = pbi->YStride;
  for ( i = 0; i < pbi->YPlaneFragments; i++ ) {
    if ( pbi->display_fragments[i] ) {
      PixelIndex = pbi->recon_pixel_index_table[i];
      SrcPtr = &SrcReconPtr[ PixelIndex ];
      DestPtr = &DestReconPtr[ PixelIndex ];

      dsp_copy8x8 (pbi->dsp, SrcPtr, DestPtr, PlaneLineStep);
    }
  }

  /* Then U and V */
  PlaneLineStep = pbi->UVStride;
  for ( i = pbi->YPlaneFragments; i < pbi->UnitFragments; i++ ) {
    if ( pbi->display_fragments[i] ) {
      PixelIndex = pbi->recon_pixel_index_table[i];
      SrcPtr = &SrcReconPtr[ PixelIndex ];
      DestPtr = &DestReconPtr[ PixelIndex ];

      dsp_copy8x8 (pbi->dsp, SrcPtr, DestPtr, PlaneLineStep);

    }
  }
}

static void CopyNotRecon( PB_INSTANCE *pbi, unsigned char * DestReconPtr,
                   unsigned char * SrcReconPtr ) {
  ogg_uint32_t  i;
  ogg_uint32_t  PlaneLineStep; /* Pixels per line */
  ogg_uint32_t  PixelIndex;

  unsigned char  *SrcPtr;      /* Pointer to line of source image data */
  unsigned char  *DestPtr;     /* Pointer to line of destination image data*/

  /* Copy over only updated blocks. */

  /* First Y plane */
  PlaneLineStep = pbi->YStride;
  for ( i = 0; i < pbi->YPlaneFragments; i++ ) {
    if ( !pbi->display_fragments[i] ) {
      PixelIndex = pbi->recon_pixel_index_table[i];
      SrcPtr = &SrcReconPtr[ PixelIndex ];
      DestPtr = &DestReconPtr[ PixelIndex ];

      dsp_copy8x8 (pbi->dsp, SrcPtr, DestPtr, PlaneLineStep);
    }
  }

  /* Then U and V */
  PlaneLineStep = pbi->UVStride;
  for ( i = pbi->YPlaneFragments; i < pbi->UnitFragments; i++ ) {
    if ( !pbi->display_fragments[i] ) {
      PixelIndex = pbi->recon_pixel_index_table[i];
      SrcPtr = &SrcReconPtr[ PixelIndex ];
      DestPtr = &DestReconPtr[ PixelIndex ];

      dsp_copy8x8 (pbi->dsp, SrcPtr, DestPtr, PlaneLineStep);

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
  PB_INSTANCE   *pbi = &cpi->pb;
  ogg_int32_t i;

  ogg_int16_t * BoundingValuePtr=pbi->FiltBoundingValue+127;
  int FragsAcross=pbi->HFragments;
  int FromFragment,ToFragment;
  int FragsDown = pbi->VFragments;
  ogg_int32_t LineFragments;
  ogg_int32_t LineLength;
  ogg_int32_t FLimit;
  int j,m,n;

  FLimit = cpi->pb.quant_info.loop_filter_limits[cpi->BaseQ]; // temp
  if ( FLimit == 0 ) return;
  SetupBoundingValueArray_Generic(pbi, FLimit);

  for ( j = 0; j < 3 ; j++){
    switch(j) {
    case 0: /* y */
      FromFragment = 0;
      ToFragment = pbi->YPlaneFragments;
      FragsAcross = pbi->HFragments;
      FragsDown = pbi->VFragments;
      LineLength = pbi->YStride;
      LineFragments = pbi->HFragments;
      break;
    case 1: /* u */
      FromFragment = pbi->YPlaneFragments;
      ToFragment = pbi->YPlaneFragments + pbi->UVPlaneFragments ;
      FragsAcross = pbi->HFragments >> 1;
      FragsDown = pbi->VFragments >> 1;
      LineLength = pbi->UVStride;
      LineFragments = pbi->HFragments / 2;
      break;
    /*case 2:  v */
    default:
      FromFragment = pbi->YPlaneFragments + pbi->UVPlaneFragments;
      ToFragment = pbi->YPlaneFragments + (2 * pbi->UVPlaneFragments) ;
      FragsAcross = pbi->HFragments >> 1;
      FragsDown = pbi->VFragments >> 1;
      LineLength = pbi->UVStride;
      LineFragments = pbi->HFragments / 2;
      break;
    }

    i=FromFragment;

    /**************************************************************
     First Row
    **************************************************************/
    /* first column conditions */
    /* only do 2 prediction if fragment coded and on non intra or if
       all fragments are intra */
    if( pbi->display_fragments[i]){
      /* Filter right hand border only if the block to the right is
         not coded */
      if ( !pbi->display_fragments[ i + 1 ] ){
        dsp_FilterHoriz(pbi->dsp,pbi->LastFrameRecon+
                    pbi->recon_pixel_index_table[i]+6,
                    LineLength,BoundingValuePtr);
      }

      /* Bottom done if next row set */
      if( !pbi->display_fragments[ i + LineFragments] ){
        dsp_FilterVert(pbi->dsp,pbi->LastFrameRecon+
                   pbi->recon_pixel_index_table[i+LineFragments],
                   LineLength, BoundingValuePtr);
      }
    }
    i++;

    /***************************************************************/
    /* middle columns  */
    for ( n = 1 ; n < FragsAcross - 1 ; n++, i++) {
      if( pbi->display_fragments[i]){
        /* Filter Left edge always */
        dsp_FilterHoriz(pbi->dsp,pbi->LastFrameRecon+
                    pbi->recon_pixel_index_table[i]-2,
                    LineLength, BoundingValuePtr);

        /* Filter right hand border only if the block to the right is
           not coded */
        if ( !pbi->display_fragments[ i + 1 ] ){
          dsp_FilterHoriz(pbi->dsp,pbi->LastFrameRecon+
                      pbi->recon_pixel_index_table[i]+6,
                      LineLength, BoundingValuePtr);
        }

        /* Bottom done if next row set */
        if( !pbi->display_fragments[ i + LineFragments] ){
          dsp_FilterVert(pbi->dsp,pbi->LastFrameRecon+
                     pbi->recon_pixel_index_table[i + LineFragments],
                     LineLength, BoundingValuePtr);
        }

      }
    }

    /***************************************************************/
    /* Last Column */
    if( pbi->display_fragments[i]){
      /* Filter Left edge always */
      dsp_FilterHoriz(pbi->dsp,pbi->LastFrameRecon+
                  pbi->recon_pixel_index_table[i] - 2 ,
                  LineLength, BoundingValuePtr);

      /* Bottom done if next row set */
      if( !pbi->display_fragments[ i + LineFragments] ){
        dsp_FilterVert(pbi->dsp,pbi->LastFrameRecon+
                   pbi->recon_pixel_index_table[i + LineFragments],
                   LineLength, BoundingValuePtr);
      }
    }
    i++;

    /***************************************************************/
    /* Middle Rows */
    /***************************************************************/
    for ( m = 1 ; m < FragsDown-1 ; m++) {

      /*****************************************************************/
      /* first column conditions */
      /* only do 2 prediction if fragment coded and on non intra or if
         all fragments are intra */
      if( pbi->display_fragments[i]){
        /* TopRow is always done */
        dsp_FilterVert(pbi->dsp,pbi->LastFrameRecon+
                   pbi->recon_pixel_index_table[i],
                   LineLength, BoundingValuePtr);

        /* Filter right hand border only if the block to the right is
           not coded */
        if ( !pbi->display_fragments[ i + 1 ] ){
          dsp_FilterHoriz(pbi->dsp,pbi->LastFrameRecon+
                      pbi->recon_pixel_index_table[i] + 6,
                      LineLength, BoundingValuePtr);
        }

        /* Bottom done if next row set */
        if( !pbi->display_fragments[ i + LineFragments] ){
          dsp_FilterVert(pbi->dsp,pbi->LastFrameRecon+
                     pbi->recon_pixel_index_table[i + LineFragments],
                     LineLength, BoundingValuePtr);
        }
      }
      i++;

      /*****************************************************************/
      /* middle columns  */
      for ( n = 1 ; n < FragsAcross - 1 ; n++, i++){
        if( pbi->display_fragments[i]){
          /* Filter Left edge always */
          dsp_FilterHoriz(pbi->dsp,pbi->LastFrameRecon+
                      pbi->recon_pixel_index_table[i] - 2,
                      LineLength, BoundingValuePtr);

          /* TopRow is always done */
          dsp_FilterVert(pbi->dsp,pbi->LastFrameRecon+
                     pbi->recon_pixel_index_table[i],
                     LineLength, BoundingValuePtr);

          /* Filter right hand border only if the block to the right
             is not coded */
          if ( !pbi->display_fragments[ i + 1 ] ){
            dsp_FilterHoriz(pbi->dsp,pbi->LastFrameRecon+
                        pbi->recon_pixel_index_table[i] + 6,
                        LineLength, BoundingValuePtr);
          }

          /* Bottom done if next row set */
          if( !pbi->display_fragments[ i + LineFragments] ){
            dsp_FilterVert(pbi->dsp,pbi->LastFrameRecon+
                       pbi->recon_pixel_index_table[i + LineFragments],
                       LineLength, BoundingValuePtr);
          }
        }
      }

      /******************************************************************/
      /* Last Column */
      if( pbi->display_fragments[i]){
        /* Filter Left edge always*/
        dsp_FilterHoriz(pbi->dsp,pbi->LastFrameRecon+
                    pbi->recon_pixel_index_table[i] - 2,
                    LineLength, BoundingValuePtr);

        /* TopRow is always done */
        dsp_FilterVert(pbi->dsp,pbi->LastFrameRecon+
                   pbi->recon_pixel_index_table[i],
                   LineLength, BoundingValuePtr);

        /* Bottom done if next row set */
        if( !pbi->display_fragments[ i + LineFragments] ){
          dsp_FilterVert(pbi->dsp,pbi->LastFrameRecon+
                     pbi->recon_pixel_index_table[i + LineFragments],
                     LineLength, BoundingValuePtr);
        }
      }
      i++;

    }

    /*******************************************************************/
    /* Last Row  */

    /* first column conditions */
    /* only do 2 prediction if fragment coded and on non intra or if
       all fragments are intra */
    if( pbi->display_fragments[i]){

      /* TopRow is always done */
      dsp_FilterVert(pbi->dsp,pbi->LastFrameRecon+
                 pbi->recon_pixel_index_table[i],
                 LineLength, BoundingValuePtr);

      /* Filter right hand border only if the block to the right is
         not coded */
      if ( !pbi->display_fragments[ i + 1 ] ){
        dsp_FilterHoriz(pbi->dsp,pbi->LastFrameRecon+
                    pbi->recon_pixel_index_table[i] + 6,
                    LineLength, BoundingValuePtr);
      }
    }
    i++;

    /******************************************************************/
    /* middle columns  */
    for ( n = 1 ; n < FragsAcross - 1 ; n++, i++){
      if( pbi->display_fragments[i]){
        /* Filter Left edge always */
        dsp_FilterHoriz(pbi->dsp,pbi->LastFrameRecon+
                    pbi->recon_pixel_index_table[i] - 2,
                    LineLength, BoundingValuePtr);

        /* TopRow is always done */
        dsp_FilterVert(pbi->dsp,pbi->LastFrameRecon+
                   pbi->recon_pixel_index_table[i],
                   LineLength, BoundingValuePtr);

        /* Filter right hand border only if the block to the right is
           not coded */
        if ( !pbi->display_fragments[ i + 1 ] ){
          dsp_FilterHoriz(pbi->dsp,pbi->LastFrameRecon+
                      pbi->recon_pixel_index_table[i] + 6,
                      LineLength, BoundingValuePtr);
        }
      }
    }

    /******************************************************************/
    /* Last Column */
    if( pbi->display_fragments[i]){
      /* Filter Left edge always */
      dsp_FilterHoriz(pbi->dsp,pbi->LastFrameRecon+
                  pbi->recon_pixel_index_table[i] - 2,
                  LineLength, BoundingValuePtr);

      /* TopRow is always done */
      dsp_FilterVert(pbi->dsp,pbi->LastFrameRecon+
                 pbi->recon_pixel_index_table[i],
                 LineLength, BoundingValuePtr);

    }
    i++;
  }
}

void ReconRefFrames (CP_INSTANCE *cpi){
  PB_INSTANCE *pbi = &cpi->pb;
  ogg_int32_t i;
  unsigned char *SwapReconBuffersTemp;

  /* Inverse DCT and reconstitute buffer in thisframe */
  for(i=0;i<pbi->UnitFragments;i++)
    ExpandBlock( cpi, i );

  /* Copy the current reconstruction back to the last frame recon buffer. */
  if(pbi->CodedBlockIndex > (ogg_int32_t) (pbi->UnitFragments >> 1)){
    SwapReconBuffersTemp = pbi->ThisFrameRecon;
    pbi->ThisFrameRecon = pbi->LastFrameRecon;
    pbi->LastFrameRecon = SwapReconBuffersTemp;
    CopyNotRecon( pbi, pbi->LastFrameRecon, pbi->ThisFrameRecon );
  }else{
    CopyRecon( pbi, pbi->LastFrameRecon, pbi->ThisFrameRecon );
  }

  /* Apply a loop filter to edge pixels of updated blocks */
  LoopFilter(cpi);

#ifdef _TH_DEBUG_
  {
    int x,y,i,j,k,xn,yn,stride;
    int plane;
    int buf;
    
    /* dump fragment DCT components */
    for(plane=0;plane<3;plane++){
      char *plstr;
      int offset;
      switch(plane){
      case 0:
	plstr="Y";
	xn = pbi->HFragments;
	yn = pbi->VFragments;
	offset = 0; 
	stride = pbi->YStride;
	break;
      case 1:
	plstr="U";
	xn = pbi->HFragments>>1;
	yn = pbi->VFragments>>1;
	offset = pbi->VFragments * pbi->HFragments;	
	stride = pbi->UVStride;
	break;
      case 2:
	plstr="V";
	xn = pbi->HFragments>>1;
	yn = pbi->VFragments>>1;
	offset = pbi->VFragments * pbi->HFragments + 
	  ((pbi->VFragments * pbi->HFragments) >> 2);
	stride = pbi->UVStride;
	break;
      }
      for(y=0;y<yn;y++){
	for(x=0;x<xn;x++,i++){
	  
	  for(buf=0;buf<3;buf++){
	    Q_LIST_ENTRY (*ptr)[64];
	    char *bufn;
	    
	    switch(buf){
	    case 0:
	      bufn = "coded";
	      ptr = pbi->QFragQUAN;
	      break;
	    case 1:
	      bufn = "coeff";
	      ptr = pbi->QFragFREQ;
	      break;
	    case 2:
	      bufn = "idct";
	      ptr = pbi->QFragTIME;
	      break;
	    }
	    
	    i = offset + y*xn + x;
	    
	    TH_DEBUG("%s %s [%d][%d] = {",bufn,plstr,x,y);
	    if ( !pbi->display_fragments[i] ) 
	      TH_DEBUG(" not coded }\n");
	    else{
	      int l=0;
	      for(j=0;j<8;j++){
		TH_DEBUG("\n   ");
		for(k=0;k<8;k++,l++){
		  TH_DEBUG("%d ",ptr[i][l]);
		}
	      }
	      TH_DEBUG(" }\n");
	    }
	  }
	  
	  /* and the loop filter output, which is a flat struct */
	  TH_DEBUG("recon %s [%d][%d] = {",plstr,x,y);
	  for(j=0;j<8;j++){
	    int l = pbi->recon_pixel_index_table[i] + j*stride;
	    TH_DEBUG("\n   ");
	    for(k=0;k<8;k++,l++)
	      TH_DEBUG("%d ", pbi->LastFrameRecon[l]);
	  }
	  TH_DEBUG(" }\n\n");
	}
      }
    }
  }
#endif
  
  /* We may need to update the UMV border */
  UpdateUMVBorder(cpi, pbi->LastFrameRecon);
  
  /* Reconstruct the golden frame if necessary.
     For VFW codec only on key frames */
  if ( pbi->FrameType == KEY_FRAME ){
    CopyRecon( pbi, pbi->GoldenFrame, pbi->LastFrameRecon );
    /* We may need to update the UMV border */
    UpdateUMVBorder(cpi, pbi->GoldenFrame);
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
