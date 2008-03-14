/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2008                *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

  function:
  last mod: $Id$

 ********************************************************************/

#include <stdlib.h>
#include <string.h>
#include "codec_internal.h"


void ClearFrameInfo(CP_INSTANCE *cpi){

  if(cpi->frame) _ogg_free(cpi->frame);
  if(cpi->lastrecon ) _ogg_free(cpi->lastrecon );
  if(cpi->golden) _ogg_free(cpi->golden);
  if(cpi->recon) _ogg_free(cpi->recon);
  if(cpi->dct_token_storage) _ogg_free(cpi->dct_token_storage);
  if(cpi->dct_token_eb_storage) _ogg_free(cpi->dct_token_eb_storage);
  if(cpi->frag_coded) _ogg_free(cpi->frag_coded);
  if(cpi->frag_buffer_index) _ogg_free(cpi->frag_buffer_index);
  if(cpi->frag_nonzero) _ogg_free(cpi->frag_nonzero);
  if(cpi->frag_dct) _ogg_free(cpi->frag_dct);
  if(cpi->frag_dc) _ogg_free(cpi->frag_dc);
#ifdef COLLECT_METRICS
  if(cpi->frag_mbi) _ogg_free(cpi->frag_mbi);
  if(cpi->frag_sad) _ogg_free(cpi->frag_sad);
  if(cpi->dct_token_frag_storage) _ogg_free(cpi->dct_token_frag_storage);
  if(cpi->dct_eob_fi_storage) _ogg_free(cpi->dct_eob_fi_storage);
#endif

  if(cpi->macro) _ogg_free(cpi->macro);
  if(cpi->super[0]) _ogg_free(cpi->super[0]);
}

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

void InitFrameInfo(CP_INSTANCE *cpi){

  cpi->stride[0] = (cpi->info.width + STRIDE_EXTRA);
  cpi->stride[1] = (cpi->info.width + STRIDE_EXTRA) / 2;
  cpi->stride[2] = (cpi->info.width + STRIDE_EXTRA) / 2;

  {
    ogg_uint32_t ry_size = cpi->stride[0] * (cpi->info.height + STRIDE_EXTRA);
    ogg_uint32_t ruv_size = ry_size / 4;

    cpi->frame_size = ry_size + 2 * ruv_size;
    cpi->offset[0] = (cpi->stride[0] * UMV_BORDER) + UMV_BORDER;
    cpi->offset[1] = ry_size + cpi->stride[1] * (UMV_BORDER/2) + (UMV_BORDER/2);
    cpi->offset[2] = ry_size + ruv_size + cpi->stride[2] * (UMV_BORDER/2) + (UMV_BORDER/2);
  }

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

  /* +1; the last entry is the 'invalid' frag, which is always set to not coded as it doesn't really exist */
  cpi->frag_coded = calloc(cpi->frag_total+1, sizeof(*cpi->frag_coded)); 
  cpi->frag_buffer_index = calloc(cpi->frag_total, sizeof(*cpi->frag_buffer_index));
  cpi->frag_nonzero = calloc(cpi->frag_total, sizeof(*cpi->frag_nonzero));
  cpi->frag_dct = calloc(cpi->frag_total, sizeof(*cpi->frag_dct));
  cpi->frag_dc = calloc(cpi->frag_total, sizeof(*cpi->frag_dc));

  /* +1; the last entry is the 'invalid' mb, which contains only 'invalid' frags */
  cpi->macro = calloc(cpi->macro_total+1, sizeof(*cpi->macro));

  cpi->super[0] = calloc(cpi->super_total, sizeof(**cpi->super));
  cpi->super[1] = cpi->super[0] + cpi->super_n[0];
  cpi->super[2] = cpi->super[1] + cpi->super_n[1];

  cpi->dct_token_storage = _ogg_malloc(cpi->frag_total*BLOCK_SIZE*sizeof(*cpi->dct_token_storage));
  cpi->dct_token_eb_storage = _ogg_malloc(cpi->frag_total*BLOCK_SIZE*sizeof(*cpi->dct_token_eb_storage));

#ifdef COLLECT_METRICS
  cpi->frag_mbi = _ogg_calloc(cpi->frag_total+1, sizeof(*cpi->frag_mbi));
  cpi->frag_sad = _ogg_calloc(cpi->frag_total+1, sizeof(*cpi->frag_sad));
  cpi->dct_token_frag_storage = _ogg_malloc(cpi->frag_total*BLOCK_SIZE*sizeof(*cpi->dct_token_frag_storage));
  cpi->dct_eob_fi_storage = _ogg_malloc(cpi->frag_total*BLOCK_SIZE*sizeof(*cpi->dct_eob_fi_storage));
#endif
  

  /* fill in superblock fragment pointers; hilbert order */
  {
    int row,col,frag,mb;
    int fhilbertx[16] = {0,1,1,0,0,0,1,1,2,2,3,3,3,2,2,3};
    int fhilberty[16] = {0,0,1,1,2,3,3,2,2,3,3,2,1,1,0,0};
    int mhilbertx[4] = {0,0,1,1};
    int mhilberty[4] = {0,1,1,0};
    int offset = 0;
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
	      int fragindex = frow*cpi->frag_h[plane] + fcol + offset;
	      cpi->super[plane][superindex].f[frag] = fragindex;
	    }else
	      cpi->super[plane][superindex].f[frag] = cpi->frag_total; /* 'invalid' */
	  }
	}
      }
      offset+=cpi->frag_n[plane];
    }
    
    /* Y */
    for(row=0;row<cpi->super_v[0];row++){
      for(col=0;col<cpi->super_h[0];col++){
	int superindex = row*cpi->super_h[0] + col;
	for(mb=0;mb<4;mb++){
	  /* translate to macroblock index */
	  int mrow = row*2 + mhilberty[mb];
	  int mcol = col*2 + mhilbertx[mb];
	  if(mrow<cpi->macro_v && mcol<cpi->macro_h){
	    int macroindex = mrow*cpi->macro_h + mcol;
	    cpi->super[0][superindex].m[mb] = macroindex;
	  }else
	    cpi->super[0][superindex].m[mb] = cpi->macro_total;

	}
      }
    }

    /* U (assuming 4:2:0 for now) */
    for(row=0;row<cpi->super_v[1];row++){
      for(col=0;col<cpi->super_h[1];col++){
	int superindex = row*cpi->super_h[1] + col;
	for(mb=0;mb<16;mb++){
	  /* translate to macroblock index */
	  int mrow = row*4 + fhilberty[mb];
	  int mcol = col*4 + fhilbertx[mb];
	  if(mrow<cpi->macro_v && mcol<cpi->macro_h){
	    int macroindex = mrow*cpi->macro_h + mcol;
	    cpi->super[1][superindex].m[mb] = macroindex;
	  }else
	    cpi->super[1][superindex].m[mb] = cpi->macro_total;
	}
      }
    }

    /* V (assuming 4:2:0 for now) */
    for(row=0;row<cpi->super_v[2];row++){
      for(col=0;col<cpi->super_h[2];col++){
	int superindex = row*cpi->super_h[2] + col;
	for(mb=0;mb<16;mb++){
	  /* translate to macroblock index */
	  int mrow = row*4 + fhilberty[mb];
	  int mcol = col*4 + fhilbertx[mb];
	  if(mrow<cpi->macro_v && mcol<cpi->macro_h){
	    int macroindex = mrow*cpi->macro_h + mcol;
	    cpi->super[2][superindex].m[mb] = macroindex;
	  }else
	    cpi->super[2][superindex].m[mb] = cpi->macro_total;
	}
      }
    }

  }

  /* fill in macroblock fragment pointers; raster (MV coding) order */
  /* 4:2:0 only for now */
  {
    int row,col,frag;
    int scanx[4][4] = { {0,1,1,0}, {1,0,0,1}, {0,0,1,1}, {0,0,1,1} };
    int scany[4][4] = { {0,0,1,1}, {1,1,0,0}, {0,1,1,0}, {0,1,1,0} };

    for(row=0;row<cpi->macro_v;row++){
      int baserow = row*2;
      for(col=0;col<cpi->macro_h;col++){
	int basecol = col*2;
	int macroindex = row*cpi->macro_h + col;
	int hpos = (col&1) + (row&1)*2;
	int fragindex;
	for(frag=0;frag<4;frag++){
	  /* translate to fragment index */
	  int frow = baserow + scany[hpos][frag];
	  int fcol = basecol + scanx[hpos][frag];
	  if(frow<cpi->frag_v[0] && fcol<cpi->frag_h[0]){
	    fragindex = frow*cpi->frag_h[0] + fcol;	    
#ifdef COLLECT_METRICS
	    cpi->frag_mbi[fragindex] = macroindex;
#endif
	  }else
	    fragindex = cpi->frag_total;
	  cpi->macro[macroindex].Ryuv[frag] = (baserow+((frag&2)>>1))*cpi->frag_h[0]+basecol+(frag&1);
	  cpi->macro[macroindex].Hyuv[0][frag] = fragindex;
	}
	
	if(row<cpi->frag_v[1] && col<cpi->frag_h[1]){
	  fragindex = cpi->frag_n[0] + macroindex;
#ifdef COLLECT_METRICS
	  cpi->frag_mbi[fragindex] = macroindex;
#endif
	}else
	  fragindex = cpi->frag_total;
	cpi->macro[macroindex].Hyuv[1][0] = fragindex;
	
	if(row<cpi->frag_v[2] && col<cpi->frag_h[2]){
	  fragindex = cpi->frag_n[0] + cpi->frag_n[1] + macroindex; 
#ifdef COLLECT_METRICS
	  cpi->frag_mbi[fragindex] = macroindex;
#endif
	}else
	  fragindex = cpi->frag_total;
	cpi->macro[macroindex].Hyuv[2][0] = fragindex;
	
      }
    }
  }

  /* fill in macroblock neighbor information for MC analysis */
  {
    int row,col;

    for(row=0;row<cpi->macro_v;row++){
      for(col=0;col<cpi->macro_h;col++){
	int macroindex = row*cpi->macro_h + col;
	int count=0;

	/* cneighbors are of four possible already-filled-in neighbors from the eight-neighbor square */
	if(col)
	  cpi->macro[macroindex].cneighbors[count++]=macroindex-1;
	if(col && row)
	  cpi->macro[macroindex].cneighbors[count++]=macroindex-cpi->macro_h-1;
	if(row)
	  cpi->macro[macroindex].cneighbors[count++]=macroindex-cpi->macro_h;
	if(row && col+1<cpi->macro_h)
	  cpi->macro[macroindex].cneighbors[count++]=macroindex-cpi->macro_h+1;
	cpi->macro[macroindex].ncneighbors=count;

	/* pneighbors are of the four possible direct neighbors (plus pattern), not the same as cneighbors */
	count=0;
	if(col)
	  cpi->macro[macroindex].pneighbors[count++]=macroindex-1;
	if(row)
	  cpi->macro[macroindex].pneighbors[count++]=macroindex-cpi->macro_h;
	if(col+1<cpi->macro_h)
	  cpi->macro[macroindex].pneighbors[count++]=macroindex+1;
	if(row+1<cpi->macro_v)
	  cpi->macro[macroindex].pneighbors[count++]=macroindex+cpi->macro_h;
	cpi->macro[macroindex].npneighbors=count;
      }
    }
  }

  /* fill in 'invalid' macroblock */
  {
    int p,f;
    for(p=0;p<3;p++)
      for(f=0;f<4;f++){
	cpi->macro[cpi->macro_total].Ryuv[f] = cpi->frag_total;
	cpi->macro[cpi->macro_total].Hyuv[p][f] = cpi->frag_total;
      }
    cpi->macro[cpi->macro_total].ncneighbors=0;
    cpi->macro[cpi->macro_total].npneighbors=0;
#ifdef COLLECT_METRICS
    cpi->frag_mbi[cpi->frag_total] = cpi->macro_total;
#endif
  }

  /* allocate frames */
  cpi->frame = _ogg_calloc(cpi->frame_size,sizeof(*cpi->frame));
  cpi->lastrecon = _ogg_calloc(cpi->frame_size,sizeof(*cpi->lastrecon));
  cpi->golden = _ogg_calloc(cpi->frame_size,sizeof(*cpi->golden));
  cpi->recon = _ogg_calloc(cpi->frame_size,sizeof(*cpi->recon));

  /* Re-initialise the pixel index table. */
  {
    ogg_uint32_t plane,row,col;
    ogg_uint32_t *bp = cpi->frag_buffer_index;
    
    for(plane=0;plane<3;plane++){
      ogg_uint32_t offset = cpi->offset[plane];
      for(row=0;row<cpi->frag_v[plane];row++){
	for(col=0;col<cpi->frag_h[plane];col++,bp++){
	  *bp = offset+col*8;
	}
	offset += cpi->stride[plane]*8;
      }
    }
  }
}

