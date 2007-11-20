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

void ClearTmpBuffers(PB_INSTANCE * pbi){

  if(pbi->ReconDataBuffer)
    _ogg_free(pbi->ReconDataBuffer);

  pbi->ReconDataBuffer=0;
}

void InitTmpBuffers(PB_INSTANCE * pbi){

  /* clear any existing info */
  ClearTmpBuffers(pbi);

  /* Adjust the position of all of our temporary */
  pbi->ReconDataBuffer      =
    _ogg_malloc(64*sizeof(*pbi->ReconDataBuffer));
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
}
