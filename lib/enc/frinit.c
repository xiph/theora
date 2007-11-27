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


static void CalcPixelIndexTable(CP_INSTANCE *cpi){
  ogg_uint32_t plane,row,col;
  fragment_t *fp = cpi->frag[0];
  ogg_uint32_t raw=0;

  for(plane=0;plane<3;plane++){
    ogg_uint32_t recon = cpi->recon_offset[plane];
    for(row=0;row<cpi->frag_v[plane];row++){
      for(col=0;col<cpi->frag_h[plane];col++,fp++){
	fp->raw_index = raw+col*8;
	fp->recon_index = recon+col*8;
      }
      raw += col*8*8;
      recon += cpi->recon_stride[plane]*8;
    }
  }
}

void ClearFragmentInfo(CP_INSTANCE *cpi){
  PB_INSTANCE *pbi = &cpi->pb;

  /* free prior allocs if present */
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

  pbi->CodedBlockList = 0;
  pbi->FragMVect = 0;
  pbi->MBCodedFlags = 0;
  pbi->MBFullyFlags = 0;
  pbi->BlockMap = 0;

  pbi->SBCodedFlags = 0;
  pbi->SBFullyFlags = 0;

  if(cpi->RunHuffIndices)
    _ogg_free(cpi->RunHuffIndices);
  if(cpi->ModeList)
    _ogg_free(cpi->ModeList);
  if(cpi->MVList)
    _ogg_free(cpi->MVList);
  if(cpi->PartiallyCodedFlags)
    _ogg_free(cpi->PartiallyCodedFlags);
  if(cpi->PartiallyCodedMbPatterns)
    _ogg_free(cpi->PartiallyCodedMbPatterns);
  if(cpi->BlockCodedFlags)
    _ogg_free(cpi->BlockCodedFlags);

  cpi->RunHuffIndices = 0;
  cpi->ModeList = 0;
  cpi->MVList = 0;
  cpi->PartiallyCodedFlags = 0;
  cpi->PartiallyCodedMbPatterns = 0;
  cpi->BlockCodedFlags = 0;

}

static void InitFragmentInfo(CP_INSTANCE * cpi){
  PB_INSTANCE *pbi = &cpi->pb;

  /* clear any existing info */
  ClearFragmentInfo(cpi);

  /* A note to people reading and wondering why malloc returns aren't
     checked:

     lines like the following that implement a general strategy of
     'check the return of malloc; a zero pointer means we're out of
     memory!'...:

     if(!cpi->extra_fragments) { EDeleteFragmentInfo(cpi); return FALSE; }
     
     ...are not useful.  It's true that many platforms follow this
     malloc behavior, but many do not.  The more modern malloc
     strategy is only to allocate virtual pages, which are not mapped
     until the memory on that page is touched.  At *that* point, if
     the machine is out of heap, the page fails to be mapped and a
     SEGV is generated.

     That means that if we want to deal with out of memory conditions,
     we *must* be prepared to process a SEGV.  If we implement the
     SEGV handler, there's no reason to to check malloc return; it is
     a waste of code. */

  cpi->RunHuffIndices =
    _ogg_malloc(cpi->pb.UnitFragments*
                sizeof(*cpi->RunHuffIndices));
  cpi->BlockCodedFlags =
    _ogg_malloc(cpi->pb.UnitFragments*
                sizeof(*cpi->BlockCodedFlags));
  cpi->ModeList =
    _ogg_malloc(cpi->pb.UnitFragments*
                sizeof(*cpi->ModeList));
  cpi->MVList =
    _ogg_malloc(cpi->pb.UnitFragments*
                sizeof(*cpi->MVList));
  cpi->PartiallyCodedFlags =
    _ogg_malloc(cpi->pb.MacroBlocks*
                sizeof(*cpi->PartiallyCodedFlags));
  cpi->PartiallyCodedMbPatterns =
    _ogg_malloc(cpi->pb.MacroBlocks*
                sizeof(*cpi->PartiallyCodedMbPatterns));

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

void ClearFrameInfo(CP_INSTANCE *cpi){
  PB_INSTANCE *pbi = &cpi->pb;

  if(cpi->yuvptr)
    _ogg_free(cpi->yuvptr);
  cpi->yuvptr = 0;

  if(cpi->OptimisedTokenListEb )
    _ogg_free(cpi->OptimisedTokenListEb);
  cpi->OptimisedTokenListEb = 0;

  if(cpi->OptimisedTokenList )
    _ogg_free(cpi->OptimisedTokenList);
  cpi->OptimisedTokenList = 0;

  if(cpi->OptimisedTokenListHi )
    _ogg_free(cpi->OptimisedTokenListHi);
  cpi->OptimisedTokenListHi = 0;

  if(cpi->OptimisedTokenListPl )
    _ogg_free(cpi->OptimisedTokenListPl);
  cpi->OptimisedTokenListPl = 0;

  if(cpi->frag[0])_ogg_free(cpi->frag[0]);
  if(cpi->macro)_ogg_free(cpi->macro);
  if(cpi->super[0])_ogg_free(cpi->super[0]);
  cpi->frag[0] = 0;
  cpi->frag[1] = 0;
  cpi->frag[2] = 0;
  cpi->macro = 0;
  cpi->super[0] = 0;
  cpi->super[1] = 0;
  cpi->super[2] = 0;

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

static void InitFrameInfo(CP_INSTANCE *cpi, unsigned int FrameSize){
  PB_INSTANCE *pbi = &cpi->pb;

  /* clear any existing info */
  ClearFrameInfo(cpi);

  /* allocate frames */
  pbi->ThisFrameRecon =
    _ogg_malloc(FrameSize*sizeof(*pbi->ThisFrameRecon));

  pbi->GoldenFrame =
    _ogg_malloc(FrameSize*sizeof(*pbi->GoldenFrame));

  pbi->LastFrameRecon =
    _ogg_malloc(FrameSize*sizeof(*pbi->LastFrameRecon));

  /* allocate frames */
  cpi->yuvptr =
    _ogg_malloc(FrameSize*
                sizeof(*cpi->yuvptr));
  cpi->OptimisedTokenListEb =
    _ogg_malloc(FrameSize*
                sizeof(*cpi->OptimisedTokenListEb));
  cpi->OptimisedTokenList =
    _ogg_malloc(FrameSize*
                sizeof(*cpi->OptimisedTokenList));
  cpi->OptimisedTokenListHi =
    _ogg_malloc(FrameSize*
                sizeof(*cpi->OptimisedTokenListHi));
  cpi->OptimisedTokenListPl =
    _ogg_malloc(FrameSize*
                sizeof(*cpi->OptimisedTokenListPl));

  /* new block abstraction setup... babysteps... */
  cpi->frag_h[0] = (cpi->info.width >> 3);
  cpi->frag_v[0] = (cpi->info.height >> 3);
  cpi->frag_n[0] = cpi->frag_h[0] * cpi->frag_v[0];
  cpi->frag_h[1] = (cpi->info.width >> 4);
  cpi->frag_v[1] = (cpi->info.height >> 4);
  cpi->frag_n[1] = cpi->frag_h[1] * cpi->frag_v[1];
  cpi->frag_h[2] = (cpi->info.width >> 4);
  cpi->frag_v[2] = (cpi->info.height >> 4);
  cpi->frag_n[2] = cpi->frag_h[2] * cpi->frag_v[2];
  cpi->frag_total = cpi->frag_n[0] + cpi->frag_n[1] + cpi->frag_n[2];

  cpi->macro_h = (cpi->frag_h[0] >> 1);
  cpi->macro_v = (cpi->frag_v[0] >> 1);
  cpi->macro_total = cpi->macro_h * cpi->macro_v;

  cpi->super_h[0] = (cpi->info.width >> 5) + ((cpi->info.width & 0x1f) ? 1 : 0);
  cpi->super_v[0] = (cpi->info.height >> 5) + ((cpi->info.height & 0x1f) ? 1 : 0);
  cpi->super_n[0] = cpi->super_h[0] * cpi->super_v[0];
  cpi->super_h[1] = (cpi->info.width >> 6) + ((cpi->info.width & 0x3f) ? 1 : 0);
  cpi->super_v[1] = (cpi->info.height >> 6) + ((cpi->info.height & 0x3f) ? 1 : 0);
  cpi->super_n[1] = cpi->super_h[1] * cpi->super_v[1];
  cpi->super_h[2] = (cpi->info.width >> 6) + ((cpi->info.width & 0x3f) ? 1 : 0);
  cpi->super_v[2] = (cpi->info.height >> 6) + ((cpi->info.height & 0x3f) ? 1 : 0);
  cpi->super_n[2] = cpi->super_h[2] * cpi->super_v[2];
  cpi->super_total = cpi->super_n[0] + cpi->super_n[1] + cpi->super_n[2];

  cpi->frag[0] = calloc(cpi->frag_total, sizeof(**cpi->frag));
  cpi->frag[1] = cpi->frag[0] + cpi->frag_n[0];
  cpi->frag[2] = cpi->frag[1] + cpi->frag_n[1];

  cpi->macro = calloc(cpi->macro_total, sizeof(*cpi->macro));

  cpi->super[0] = calloc(cpi->super_total, sizeof(**cpi->super));
  cpi->super[1] = cpi->super[0] + cpi->super_n[0];
  cpi->super[2] = cpi->super[1] + cpi->super_n[1];

  /* fill in superblock fragment pointers; hilbert order */
  {
    int row,col,frag,mb;
    int fhilbertx[16] = {0,1,1,0,0,0,1,1,2,2,3,3,3,2,2,3};
    int fhilberty[16] = {0,0,1,1,2,3,3,2,2,3,3,2,1,1,0,0};
    int mhilbertx[4] = {0,0,1,1};
    int mhilberty[4] = {0,1,1,0};
    int plane;

    for(plane=0;plane<3;plane++){

      for(row=0;row<cpi->super_v[plane];row++){
	for(col=0;col<cpi->super_h[plane];col++){
	  int superindex = row*cpi->super_h[plane] + col;
	  for(frag=0;frag<16;frag++){
	    /* translate to fragment index */
	    int frow = row*4 + fhilberty[frag];
	    int fcol = col*4 + fhilbertx[frag];
	    if(frow<cpi->frag_v[plane] && fcol<cpi->frag_h[plane]){
	      int fragindex = frow*cpi->frag_h[plane] + fcol;
	      cpi->super[plane][superindex].f[frag] = &cpi->frag[plane][fragindex];
	    }
	  }
	}
      }
    }

    for(row=0;row<cpi->super_v[0];row++){
      for(col=0;col<cpi->super_h[0];col++){
	int superindex = row*cpi->super_h[0] + col;
	for(mb=0;mb<4;mb++){
	  /* translate to macroblock index */
	  int mrow = row*2 + mhilberty[mb];
	  int mcol = col*2 + mhilbertx[mb];
	  if(mrow<cpi->macro_v && mcol<cpi->macro_h){
	    int macroindex = mrow*cpi->macro_h + mcol;
	    cpi->super[0][superindex].m[mb] = &cpi->macro[macroindex];
	  }
	}
      }
    }
  }

  /* fill in macroblock fragment pointers; raster (MV coding) order */
  {
    int row,col,frag;
    int scanx[4] = {0,1,0,1};
    int scany[4] = {0,1,1,0};

    for(row=0;row<cpi->macro_v;row++){
      int baserow = row*2;
      for(col=0;col<cpi->macro_h;col++){
	int basecol = col*2;
	int macroindex = row*cpi->macro_h + col;
	for(frag=0;frag<4;frag++){
	  /* translate to fragment index */
	  int frow = baserow + scany[frag];
	  int fcol = basecol + scanx[frag];
	  if(frow<cpi->frag_v[0] && fcol<cpi->frag_h[0]){
	    int fragindex = frow*cpi->frag_h[0] + fcol;
	    cpi->macro[macroindex].y[frag] = &cpi->frag[0][fragindex];
	  }
	}

	if(row<cpi->frag_v[1] && col<cpi->frag_h[1])
	  cpi->macro[macroindex].u = &cpi->frag[1][macroindex];
	if(row<cpi->frag_v[2] && col<cpi->frag_h[2])
	  cpi->macro[macroindex].v = &cpi->frag[2][macroindex];

      }
    }
  }

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

  cpi->recon_stride[0] = (cpi->info.width + STRIDE_EXTRA);
  cpi->recon_stride[1] = (cpi->info.width + STRIDE_EXTRA) / 2;
  cpi->recon_stride[2] = (cpi->info.width + STRIDE_EXTRA) / 2;

  {
    ogg_uint32_t ry_size = cpi->recon_stride[0] * (cpi->info.height + STRIDE_EXTRA);
    ogg_uint32_t ruv_size = ry_size / 4;
    FrameSize = ry_size + 2 * ruv_size;

    cpi->recon_offset[0] = (cpi->recon_stride[0] * UMV_BORDER) + UMV_BORDER;
    cpi->recon_offset[1] = ry_size + cpi->recon_stride[1] * (UMV_BORDER/2) + (UMV_BORDER/2);
    cpi->recon_offset[2] = ry_size + ruv_size + cpi->recon_stride[2] * (UMV_BORDER/2) + (UMV_BORDER/2);
  }

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

  InitFragmentInfo(cpi);
  InitFrameInfo(cpi, FrameSize);

  /* Configure mapping between quad-tree and fragments */
  CreateBlockMapping ( pbi->BlockMap, pbi->YSuperBlocks,
                       pbi->UVSuperBlocks, pbi->HFragments, pbi->VFragments);

  /* Re-initialise the pixel index table. */

  CalcPixelIndexTable( cpi );

}

