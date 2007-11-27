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
#include "codec_internal.h"


static void CalcPixelIndexTable( CP_INSTANCE *cpi){
  PB_INSTANCE *pbi = &cpi->pb;
  ogg_uint32_t i;
  ogg_uint32_t * PixelIndexTablePtr;

  /* Calculate the pixel index table for normal image buffers */
  PixelIndexTablePtr = pbi->pixel_index_table;
  for ( i = 0; i < pbi->YPlaneFragments; i++ ) {
    PixelIndexTablePtr[ i ] =
      ((i / pbi->HFragments) * VFRAGPIXELS *
       cpi->info.width);
    PixelIndexTablePtr[ i ] +=
      ((i % pbi->HFragments) * HFRAGPIXELS);
  }

  PixelIndexTablePtr = &pbi->pixel_index_table[pbi->YPlaneFragments];
  for ( i = 0; i < ((pbi->HFragments >> 1) * pbi->VFragments); i++ ) {
    PixelIndexTablePtr[ i ] =
      ((i / (pbi->HFragments / 2) ) *
       (VFRAGPIXELS *
        (cpi->info.width / 2)) );
    PixelIndexTablePtr[ i ] +=
      ((i % (pbi->HFragments / 2) ) *
       HFRAGPIXELS) + pbi->YPlaneSize;
  }

  /************************************************************************/
  /* Now calculate the pixel index table for image reconstruction buffers */

  PixelIndexTablePtr = pbi->recon_pixel_index_table;
  for ( i = 0; i < pbi->YPlaneFragments; i++ ){
    PixelIndexTablePtr[ i ] =
      ((i / pbi->HFragments) * VFRAGPIXELS *
       pbi->YStride);
    PixelIndexTablePtr[ i ] +=
      ((i % pbi->HFragments) * HFRAGPIXELS) +
      pbi->ReconYDataOffset;
  }

  /* U blocks */
  PixelIndexTablePtr = &pbi->recon_pixel_index_table[pbi->YPlaneFragments];
  for ( i = 0; i < pbi->UVPlaneFragments; i++ ) {
    PixelIndexTablePtr[ i ] =
      ((i / (pbi->HFragments / 2) ) *
       (VFRAGPIXELS * (pbi->UVStride)) );
    PixelIndexTablePtr[ i ] +=
      ((i % (pbi->HFragments / 2) ) *
       HFRAGPIXELS) + pbi->ReconUDataOffset;
  }

  /* V blocks */
  PixelIndexTablePtr =
    &pbi->recon_pixel_index_table[pbi->YPlaneFragments +
                                 pbi->UVPlaneFragments];

  for ( i = 0; i < pbi->UVPlaneFragments; i++ ) {
    PixelIndexTablePtr[ i ] =
      ((i / (pbi->HFragments / 2) ) *
       (VFRAGPIXELS * (pbi->UVStride)) );
    PixelIndexTablePtr[ i ] +=
      ((i % (pbi->HFragments / 2) ) * HFRAGPIXELS) +
      pbi->ReconVDataOffset;
  }
}

static void ClearFragmentInfo(PB_INSTANCE * pbi){

  /* free prior allocs if present */
  if(pbi->pixel_index_table) _ogg_free(pbi->pixel_index_table);
  if(pbi->recon_pixel_index_table) _ogg_free(pbi->recon_pixel_index_table);
  if(pbi->CodedBlockList) _ogg_free(pbi->CodedBlockList);
  if(pbi->FragMVect) _ogg_free(pbi->FragMVect);
#ifdef _TH_DEBUG_
  if(pbi->QFragTIME) _ogg_free(pbi->QFragTIME);
  if(pbi->QFragFREQ) _ogg_free(pbi->QFragFREQ);
  if(pbi->QFragQUAN) _ogg_free(pbi->QFragQUAN);
  pbi->QFragTIME = 0;
  pbi->QFragFREQ = 0;
  pbi->QFragQUAN = 0;
#endif
  if(pbi->BlockMap) _ogg_free(pbi->BlockMap);

  if(pbi->SBCodedFlags) _ogg_free(pbi->SBCodedFlags);
  if(pbi->SBFullyFlags) _ogg_free(pbi->SBFullyFlags);
  if(pbi->MBFullyFlags) _ogg_free(pbi->MBFullyFlags);
  if(pbi->MBCodedFlags) _ogg_free(pbi->MBCodedFlags);

  pbi->pixel_index_table = 0;
  pbi->recon_pixel_index_table = 0;
  pbi->CodedBlockList = 0;
  pbi->FragMVect = 0;
  pbi->MBCodedFlags = 0;
  pbi->MBFullyFlags = 0;
  pbi->BlockMap = 0;

  pbi->SBCodedFlags = 0;
  pbi->SBFullyFlags = 0;
}

static void InitFragmentInfo(PB_INSTANCE * pbi){

  /* clear any existing info */
  ClearFragmentInfo(pbi);

  /* Perform Fragment Allocations */
  pbi->pixel_index_table =
    _ogg_malloc(pbi->UnitFragments * sizeof(*pbi->pixel_index_table));

  pbi->recon_pixel_index_table =
    _ogg_malloc(pbi->UnitFragments * sizeof(*pbi->recon_pixel_index_table));

  pbi->CodedBlockList =
    _ogg_malloc(pbi->UnitFragments * sizeof(*pbi->CodedBlockList));

  pbi->FragMVect =
    _ogg_malloc(pbi->UnitFragments * sizeof(*pbi->FragMVect));

#ifdef _TH_DEBUG_

  pbi->QFragTIME =
    _ogg_malloc(pbi->UnitFragments * sizeof(*pbi->QFragTIME));

  pbi->QFragFREQ =
    _ogg_malloc(pbi->UnitFragments * sizeof(*pbi->QFragFREQ));

  pbi->QFragQUAN =
    _ogg_malloc(pbi->UnitFragments * sizeof(*pbi->QFragQUAN));

#endif

  /* Super Block Initialization */
  pbi->SBCodedFlags =
    _ogg_malloc(pbi->SuperBlocks * sizeof(*pbi->SBCodedFlags));

  pbi->SBFullyFlags =
    _ogg_malloc(pbi->SuperBlocks * sizeof(*pbi->SBFullyFlags));

  /* Macro Block Initialization */
  pbi->MBCodedFlags =
    _ogg_malloc(pbi->MacroBlocks * sizeof(*pbi->MBCodedFlags));

  pbi->MBFullyFlags =
    _ogg_malloc(pbi->MacroBlocks * sizeof(*pbi->MBFullyFlags));

  pbi->BlockMap =
    _ogg_malloc(pbi->SuperBlocks * sizeof(*pbi->BlockMap));

}

static void ClearFrameInfo(PB_INSTANCE * pbi){
  if(pbi->ThisFrameRecon )
    _ogg_free(pbi->ThisFrameRecon );
  if(pbi->GoldenFrame)
    _ogg_free(pbi->GoldenFrame);
  if(pbi->LastFrameRecon)
    _ogg_free(pbi->LastFrameRecon);

  pbi->ThisFrameRecon = 0;
  pbi->GoldenFrame = 0;
  pbi->LastFrameRecon = 0;
}

static void InitFrameInfo(PB_INSTANCE * pbi, unsigned int FrameSize){

  /* clear any existing info */
  ClearFrameInfo(pbi);

  /* allocate frames */
  pbi->ThisFrameRecon =
    _ogg_malloc(FrameSize*sizeof(*pbi->ThisFrameRecon));

  pbi->GoldenFrame =
    _ogg_malloc(FrameSize*sizeof(*pbi->GoldenFrame));

  pbi->LastFrameRecon =
    _ogg_malloc(FrameSize*sizeof(*pbi->LastFrameRecon));

}

void InitFrameDetails(CP_INSTANCE *cpi){
  int FrameSize;
  PB_INSTANCE *pbi = &cpi->pb;

    /* Set the frame size etc. */

  pbi->YPlaneSize = cpi->info.width *
    cpi->info.height;
  pbi->UVPlaneSize = pbi->YPlaneSize / 4;
  pbi->HFragments = cpi->info.width / HFRAGPIXELS;
  pbi->VFragments = cpi->info.height / VFRAGPIXELS;
  pbi->UnitFragments = ((pbi->VFragments * pbi->HFragments)*3)/2;
  pbi->YPlaneFragments = pbi->HFragments * pbi->VFragments;
  pbi->UVPlaneFragments = pbi->YPlaneFragments / 4;

  pbi->YStride = (cpi->info.width + STRIDE_EXTRA);
  pbi->UVStride = pbi->YStride / 2;
  pbi->ReconYPlaneSize = pbi->YStride *
    (cpi->info.height + STRIDE_EXTRA);
  pbi->ReconUVPlaneSize = pbi->ReconYPlaneSize / 4;
  FrameSize = pbi->ReconYPlaneSize + 2 * pbi->ReconUVPlaneSize;

  pbi->YDataOffset = 0;
  pbi->UDataOffset = pbi->YPlaneSize;
  pbi->VDataOffset = pbi->YPlaneSize + pbi->UVPlaneSize;
  pbi->ReconYDataOffset =
    (pbi->YStride * UMV_BORDER) + UMV_BORDER;
  pbi->ReconUDataOffset = pbi->ReconYPlaneSize +
    (pbi->UVStride * (UMV_BORDER/2)) + (UMV_BORDER/2);
  pbi->ReconVDataOffset = pbi->ReconYPlaneSize + pbi->ReconUVPlaneSize +
    (pbi->UVStride * (UMV_BORDER/2)) + (UMV_BORDER/2);

  /* Image dimensions in Super-Blocks */
  pbi->YSBRows = (cpi->info.height/32)  +
    ( cpi->info.height%32 ? 1 : 0 );
  pbi->YSBCols = (cpi->info.width/32)  +
    ( cpi->info.width%32 ? 1 : 0 );
  pbi->UVSBRows = ((cpi->info.height/2)/32)  +
    ( (cpi->info.height/2)%32 ? 1 : 0 );
  pbi->UVSBCols = ((cpi->info.width/2)/32)  +
    ( (cpi->info.width/2)%32 ? 1 : 0 );

  /* Super-Blocks per component */
  pbi->YSuperBlocks = pbi->YSBRows * pbi->YSBCols;
  pbi->UVSuperBlocks = pbi->UVSBRows * pbi->UVSBCols;
  pbi->SuperBlocks = pbi->YSuperBlocks+2*pbi->UVSuperBlocks;

  /* Useful externals */
  pbi->MacroBlocks = ((pbi->VFragments+1)/2)*((pbi->HFragments+1)/2);

  InitFragmentInfo(pbi);
  InitFrameInfo(pbi, FrameSize);

  /* Configure mapping between quad-tree and fragments */
  CreateBlockMapping ( pbi->BlockMap, pbi->YSuperBlocks,
                       pbi->UVSuperBlocks, pbi->HFragments, pbi->VFragments);

  /* Re-initialise the pixel index table. */

  CalcPixelIndexTable( cpi );

}

