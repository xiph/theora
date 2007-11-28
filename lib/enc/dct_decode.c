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

static void ExpandBlock ( CP_INSTANCE *cpi, fragment_t *fp, ogg_int32_t FragmentNumber){
  PB_INSTANCE   *pbi = &cpi->pb;
  int            mode = fp->mode;
  int            qi = cpi->BaseQ; // temporary 
  int            plane = (fp<cpi->frag[1] ? 0 : (fp<cpi->frag[2] ? 1 : 2));
  ogg_uint32_t   ReconPixelsPerLine = cpi->recon_stride[plane];
  int            inter = (mode != CODE_INTRA);
  ogg_int16_t    reconstruct[64];
  ogg_int16_t  *quantizers = pbi->quant_tables[inter][plane][qi];
  ogg_int16_t   *data = fp->dct;
  
#ifdef _TH_DEBUG_
 {
   int i;
   for(i=0;i<64;i++)
     pbi->QFragFREQ[FragmentNumber][dezigzag_index[i]]= 
       pbi->quantized_list[i] * quantizers[i];
 }
#endif

  /* Invert quantisation and DCT to get pixel data. */
  switch(fp->nonzero){
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
  dsp_recon8x8 (pbi->dsp, &cpi->ThisFrameRecon[fp->recon_index],
		reconstruct, ReconPixelsPerLine);

}

static void UpdateUMV_HBorders( CP_INSTANCE *cpi,
                                unsigned char *DestReconPtr,
				int plane){
  ogg_uint32_t  i;
  ogg_uint32_t  PixelIndex;

  ogg_uint32_t  PlaneStride = cpi->recon_stride[plane];
  ogg_uint32_t  BlockVStep = cpi->recon_stride[plane] * (VFRAGPIXELS - 1);
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
  PixelIndex = fp[0].recon_index;
  SrcPtr1 = &DestReconPtr[ PixelIndex - PlaneBorderWidth ];
  DestPtr1 = SrcPtr1 - (PlaneBorderWidth * PlaneStride);

  PixelIndex = fp[PlaneFragments - LineFragments].recon_index + BlockVStep;
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

  ogg_uint32_t   PlaneStride = cpi->recon_stride[plane];
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
  PixelIndex = fp[0].recon_index;
  SrcPtr1 = &DestReconPtr[ PixelIndex ];
  DestPtr1 = &DestReconPtr[ PixelIndex - PlaneBorderWidth ];

  PixelIndex = fp[LineFragments - 1].recon_index + (HFRAGPIXELS - 1);
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
  
  /* Copy over only updated blocks.*/
  for(plane=0;plane<3;plane++){  
    int PlaneLineStep = cpi->recon_stride[plane];
    for ( i = 0; i < cpi->frag_n[plane]; i++,fp++ ) {
      if ( fp->coded ) {
	int pi= fp->recon_index;
	unsigned char *src = &SrcReconPtr[ pi ];
	unsigned char *dst = &DestReconPtr[ pi ];
	dsp_copy8x8 (cpi->pb.dsp, src, dst, PlaneLineStep);
      }
    }
  }
}

static void CopyNotRecon( CP_INSTANCE *cpi, unsigned char * DestReconPtr,
			  unsigned char * SrcReconPtr ) {
  ogg_uint32_t  i,plane;
  fragment_t *fp = cpi->frag[0];
  
  /* Copy over only updated blocks.*/
  for(plane=0;plane<3;plane++){  
    int PlaneLineStep = cpi->recon_stride[plane];
    for ( i = 0; i < cpi->frag_n[plane]; i++,fp++ ) {
      if ( !fp->coded ) {
	int pi= fp->recon_index;
	unsigned char *src = &SrcReconPtr[ pi ];
	unsigned char *dst = &DestReconPtr[ pi ];
	dsp_copy8x8 (cpi->pb.dsp, src, dst, PlaneLineStep);
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
  PB_INSTANCE   *pbi = &cpi->pb;

  ogg_int16_t *BoundingValuePtr=pbi->FiltBoundingValue+127;
  ogg_int32_t FLimit = cpi->pb.quant_info.loop_filter_limits[cpi->BaseQ]; // temp
  int j,m,n;
  fragment_t *fp;

  if ( FLimit == 0 ) return;
  SetupBoundingValueArray_Generic(pbi, FLimit);

  for ( j = 0; j < 3 ; j++){
    ogg_int32_t LineFragments = cpi->frag_h[j];
    ogg_int32_t LineLength = cpi->recon_stride[j];
    fp = cpi->frag[j];

    /**************************************************************
     First Row
    **************************************************************/
    /* first column conditions */
    /* only do 2 prediction if fragment coded and on non intra or if
       all fragments are intra */
    if( fp->coded){
      /* Filter right hand border only if the block to the right is
         not coded */
      if ( !fp[1].coded ){
        dsp_FilterHoriz(pbi->dsp,cpi->LastFrameRecon+
			fp[0].recon_index+6,
			LineLength,BoundingValuePtr);
      }

      /* Bottom done if next row set */
      if( !fp[LineFragments].coded ){
        dsp_FilterVert(pbi->dsp,cpi->LastFrameRecon+
		       fp[LineFragments].recon_index,
		       LineLength, BoundingValuePtr);
      }
    }
    fp++;

    /***************************************************************/
    /* middle columns  */
    for ( n = 1 ; n < cpi->frag_h[j] - 1 ; n++, fp++) {
      if( fp->coded){
        /* Filter Left edge always */
        dsp_FilterHoriz(pbi->dsp,cpi->LastFrameRecon+
			fp[0].recon_index-2,
			LineLength, BoundingValuePtr);

        /* Filter right hand border only if the block to the right is
           not coded */
        if ( !fp[1].coded ){
          dsp_FilterHoriz(pbi->dsp,cpi->LastFrameRecon+
			  fp[0].recon_index+6,
			  LineLength, BoundingValuePtr);
        }

        /* Bottom done if next row set */
        if( !fp[LineFragments].coded ){
          dsp_FilterVert(pbi->dsp,cpi->LastFrameRecon+
			 fp[LineFragments].recon_index,
			 LineLength, BoundingValuePtr);
        }
	
      }
    }

    /***************************************************************/
    /* Last Column */
    if(fp->coded){
      /* Filter Left edge always */
      dsp_FilterHoriz(pbi->dsp,cpi->LastFrameRecon+
		      fp[0].recon_index - 2 ,
		      LineLength, BoundingValuePtr);
      
      /* Bottom done if next row set */
      if( !fp[LineFragments].coded ){
        dsp_FilterVert(pbi->dsp,cpi->LastFrameRecon+
		       fp[LineFragments].recon_index,
		       LineLength, BoundingValuePtr);
      }
    }
    fp++;

    /***************************************************************/
    /* Middle Rows */
    /***************************************************************/
    for ( m = 1 ; m < cpi->frag_v[j]-1 ; m++) {

      /*****************************************************************/
      /* first column conditions */
      /* only do 2 prediction if fragment coded and on non intra or if
         all fragments are intra */
      if( fp->coded){
        /* TopRow is always done */
        dsp_FilterVert(pbi->dsp,cpi->LastFrameRecon+
		       fp[0].recon_index,
		       LineLength, BoundingValuePtr);

        /* Filter right hand border only if the block to the right is
           not coded */
        if ( !fp[1].coded ){
          dsp_FilterHoriz(pbi->dsp,cpi->LastFrameRecon+
			  fp[0].recon_index + 6,
			  LineLength, BoundingValuePtr);
        }
	
        /* Bottom done if next row set */
        if( !fp[LineFragments].coded ){
          dsp_FilterVert(pbi->dsp,cpi->LastFrameRecon+
			 fp[LineFragments].recon_index,
			 LineLength, BoundingValuePtr);
        }
      }
      fp++;

      /*****************************************************************/
      /* middle columns  */
      for ( n = 1 ; n < cpi->frag_h[j] - 1 ; n++, fp++){
        if( fp->coded){
          /* Filter Left edge always */
          dsp_FilterHoriz(pbi->dsp,cpi->LastFrameRecon+
			  fp[0].recon_index - 2,
			  LineLength, BoundingValuePtr);

          /* TopRow is always done */
          dsp_FilterVert(pbi->dsp,cpi->LastFrameRecon+
			 fp[0].recon_index,
			 LineLength, BoundingValuePtr);
	  
          /* Filter right hand border only if the block to the right
             is not coded */
          if ( !fp[1].coded ){
            dsp_FilterHoriz(pbi->dsp,cpi->LastFrameRecon+
			    fp[0].recon_index + 6,
			    LineLength, BoundingValuePtr);
          }

          /* Bottom done if next row set */
          if( !fp[LineFragments].coded ){
            dsp_FilterVert(pbi->dsp,cpi->LastFrameRecon+
			   fp[LineFragments].recon_index,
			   LineLength, BoundingValuePtr);
          }
        }
      }

      /******************************************************************/
      /* Last Column */
      if(fp->coded){
        /* Filter Left edge always*/
        dsp_FilterHoriz(pbi->dsp,cpi->LastFrameRecon+
			fp[0].recon_index - 2,
			LineLength, BoundingValuePtr);

        /* TopRow is always done */
        dsp_FilterVert(pbi->dsp,cpi->LastFrameRecon+
		       fp[0].recon_index,		       
		       LineLength, BoundingValuePtr);
	
        /* Bottom done if next row set */
        if( !fp[LineFragments].coded ){
          dsp_FilterVert(pbi->dsp,cpi->LastFrameRecon+
			 fp[LineFragments].recon_index,
			 LineLength, BoundingValuePtr);
        }
      }
      fp++;
    }

    /*******************************************************************/
    /* Last Row  */

    /* first column conditions */
    /* only do 2 prediction if fragment coded and on non intra or if
       all fragments are intra */
    if(fp->coded){

      /* TopRow is always done */
      dsp_FilterVert(pbi->dsp,cpi->LastFrameRecon+
		     fp[0].recon_index,
		     LineLength, BoundingValuePtr);

      /* Filter right hand border only if the block to the right is
         not coded */
      if ( !fp[1].coded ){
        dsp_FilterHoriz(pbi->dsp,cpi->LastFrameRecon+
			fp[0].recon_index + 6,
			LineLength, BoundingValuePtr);
      }
    }
    fp++;

    /******************************************************************/
    /* middle columns  */
    for ( n = 1 ; n < cpi->frag_h[j] - 1 ; n++, fp++){
      if( fp->coded){
        /* Filter Left edge always */
        dsp_FilterHoriz(pbi->dsp,cpi->LastFrameRecon+
			fp[0].recon_index - 2,
			LineLength, BoundingValuePtr);
	
        /* TopRow is always done */
        dsp_FilterVert(pbi->dsp,cpi->LastFrameRecon+
		       fp[0].recon_index,
		       LineLength, BoundingValuePtr);

        /* Filter right hand border only if the block to the right is
           not coded */
        if ( !fp[1].coded ){
          dsp_FilterHoriz(pbi->dsp,cpi->LastFrameRecon+
			  fp[0].recon_index + 6,
			  LineLength, BoundingValuePtr);
        }
      }
    }

    /******************************************************************/
    /* Last Column */
    if( fp->coded){
      /* Filter Left edge always */
      dsp_FilterHoriz(pbi->dsp,cpi->LastFrameRecon+
		      fp[0].recon_index - 2,
		      LineLength, BoundingValuePtr);

      /* TopRow is always done */
      dsp_FilterVert(pbi->dsp,cpi->LastFrameRecon+
		     fp[0].recon_index,
		     LineLength, BoundingValuePtr);
      
    }
    fp++;
  }
}

void ReconRefFrames (CP_INSTANCE *cpi){
  PB_INSTANCE *pbi = &cpi->pb;
  ogg_int32_t i;
  unsigned char *SwapReconBuffersTemp;

  /* Inverse DCT and reconstitute buffer in thisframe */
  for(i=0;i<cpi->frag_total;i++)
    ExpandBlock( cpi, cpi->frag[0]+i, i );

  /* Copy the current reconstruction back to the last frame recon buffer. */
  if(cpi->CodedBlockIndex > (ogg_int32_t) (cpi->frag_total >> 1)){
    SwapReconBuffersTemp = cpi->ThisFrameRecon;
    cpi->ThisFrameRecon = cpi->LastFrameRecon;
    cpi->LastFrameRecon = SwapReconBuffersTemp;
    CopyNotRecon( cpi, cpi->LastFrameRecon, cpi->ThisFrameRecon );
  }else{
    CopyRecon( cpi, cpi->LastFrameRecon, cpi->ThisFrameRecon );
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
	    int l = cpi->frag[0][i].recon_index + j*stride;
	    TH_DEBUG("\n   ");
	    for(k=0;k<8;k++,l++)
	      TH_DEBUG("%d ", cpi->LastFrameRecon[l]);
	  }
	  TH_DEBUG(" }\n\n");
	}
      }
    }
  }
#endif
  
  /* We may need to update the UMV border */
  UpdateUMVBorder(cpi, cpi->LastFrameRecon);
  
  /* Reconstruct the golden frame if necessary.
     For VFW codec only on key frames */
  if ( cpi->FrameType == KEY_FRAME ){
    CopyRecon( cpi, cpi->GoldenFrame, cpi->LastFrameRecon );
    /* We may need to update the UMV border */
    UpdateUMVBorder(cpi, cpi->GoldenFrame);
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
