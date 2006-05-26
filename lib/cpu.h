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
  last mod: $Id: mcomp.c,v 1.8 2003/12/03 08:59:41 arc Exp $

 ********************************************************************/

#include "codec_internal.h"

//extern ogg_uint32_t cpu_flags;

#define CPU_X86_MMX	(1<<0)
#define CPU_X86_3DNOW	(1<<1)
#define CPU_X86_MMXEXT	(1<<2)
#define CPU_X86_SSE	(1<<3)
#define CPU_X86_SSE2	(1<<4)
#define CPU_X86_3DNOWEXT (1<<5)

ogg_uint32_t cpu_init (void);

