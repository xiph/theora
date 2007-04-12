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
  last mod: $Id: dsp_mmx.c 12440 2007-02-06 16:36:26Z j $

 ********************************************************************/

#include <stdlib.h>

#include "codec_internal.h"

#if defined(USE_ASM)

/* nothing implemented right now */
void dsp_mmx_dct_decode_init(DspFunctions *funcs)
{
}

#endif /* USE_ASM */
