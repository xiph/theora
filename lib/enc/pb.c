/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2003                *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

  function:
  last mod: $Id$

 ********************************************************************/

#include <stdlib.h>
#include <string.h>
#include "codec_internal.h"

void ClearTmpBuffers(PB_INSTANCE * pbi){

  if(pbi->ReconDataBuffer)
    _ogg_free(pbi->ReconDataBuffer);
  if(pbi->DequantBuffer)
    _ogg_free(pbi->DequantBuffer);
  if(pbi->TmpDataBuffer)
    _ogg_free(pbi->TmpDataBuffer);
  if(pbi->TmpReconBuffer)
    _ogg_free(pbi->TmpReconBuffer);
  if(pbi->dequant_Y_coeffs)
    _ogg_free(pbi->dequant_Y_coeffs);
  if(pbi->dequant_U_coeffs)
    _ogg_free(pbi->dequant_U_coeffs);
  if(pbi->dequant_V_coeffs)
    _ogg_free(pbi->dequant_V_coeffs);
  if(pbi->dequant_InterY_coeffs)
    _ogg_free(pbi->dequant_InterY_coeffs);
  if(pbi->dequant_InterU_coeffs)
    _ogg_free(pbi->dequant_InterU_coeffs);
  if(pbi->dequant_InterV_coeffs)
    _ogg_free(pbi->dequant_InterV_coeffs);


  pbi->ReconDataBuffer=0;
  pbi->DequantBuffer = 0;
  pbi->TmpDataBuffer = 0;
  pbi->TmpReconBuffer = 0;
  pbi->dequant_Y_coeffs = 0;
  pbi->dequant_U_coeffs = 0;
  pbi->dequant_V_coeffs = 0;
  pbi->dequant_InterY_coeffs = 0;
  pbi->dequant_InterU_coeffs = 0;
  pbi->dequant_InterV_coeffs = 0;

}

void InitTmpBuffers(PB_INSTANCE * pbi){

  /* clear any existing info */
  ClearTmpBuffers(pbi);

  /* Adjust the position of all of our temporary */
  pbi->ReconDataBuffer      =
    _ogg_malloc(64*sizeof(*pbi->ReconDataBuffer));

  pbi->DequantBuffer        =
    _ogg_malloc(64 * sizeof(*pbi->DequantBuffer));

  pbi->TmpDataBuffer        =
    _ogg_malloc(64 * sizeof(*pbi->TmpDataBuffer));

  pbi->TmpReconBuffer       =
    _ogg_malloc(64 * sizeof(*pbi->TmpReconBuffer));

  pbi->dequant_Y_coeffs     =
    _ogg_malloc(64 * sizeof(*pbi->dequant_Y_coeffs));

  pbi->dequant_U_coeffs    =
    _ogg_malloc(64 * sizeof(*pbi->dequant_U_coeffs));

  pbi->dequant_V_coeffs    =
    _ogg_malloc(64 * sizeof(*pbi->dequant_V_coeffs));

  pbi->dequant_InterY_coeffs =
    _ogg_malloc(64 * sizeof(*pbi->dequant_InterY_coeffs));

  pbi->dequant_InterU_coeffs =
    _ogg_malloc(64 * sizeof(*pbi->dequant_InterU_coeffs));

  pbi->dequant_InterV_coeffs =
    _ogg_malloc(64 * sizeof(*pbi->dequant_InterV_coeffs));

}

void ClearPBInstance(PB_INSTANCE *pbi){
  if(pbi){
    ClearTmpBuffers(pbi);
    if (pbi->opb) {
      _ogg_free(pbi->opb);
    }
  }
}

void InitPBInstance(PB_INSTANCE *pbi){
  /* initialize whole structure to 0 */
  memset(pbi, 0, sizeof(*pbi));

  InitTmpBuffers(pbi);

  /* allocate memory for the oggpack_buffer */
#ifndef LIBOGG2
  pbi->opb = _ogg_malloc(sizeof(oggpack_buffer));
#else
  pbi->opb = _ogg_malloc(oggpack_buffersize());
#endif

  /* variables needing initialization (not being set to 0) */

  pbi->ModifierPointer[0] = &pbi->Modifier[0][255];
  pbi->ModifierPointer[1] = &pbi->Modifier[1][255];
  pbi->ModifierPointer[2] = &pbi->Modifier[2][255];
  pbi->ModifierPointer[3] = &pbi->Modifier[3][255];

  pbi->DecoderErrorCode = 0;
  pbi->KeyFrameType = DCT_KEY_FRAME;
  pbi->FramesHaveBeenSkipped = 0;
}
