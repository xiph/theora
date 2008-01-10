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
#include <limits.h>
#include <string.h>
#include "codec_internal.h"

/*The maximum Y plane SAD value for accepting the median predictor.*/
#define OC_YSAD_THRESH1            (256)
/*The amount to right shift the minimum error by when inflating it for
   computing the second maximum Y plane SAD threshold.*/
#define OC_YSAD_THRESH2_SCALE_BITS (3)
/*The amount to add to the second maximum Y plane threshold when inflating
   it.*/
#define OC_YSAD_THRESH2_OFFSET     (128)

/*The vector offsets in the X direction for each search site in the square
   pattern.*/
static const int OC_SQUARE_DX[9]={-1,0,1,-1,0,1,-1,0,1};
/*The vector offsets in the Y direction for each search site in the square
   pattern.*/
static const int OC_SQUARE_DY[9]={-1,-1,-1,0,0,0,1,1,1};
/*The number of sites to search for each boundary condition in the square
   pattern.
  Bit flags for the boundary conditions are as follows:
  1: -16==dx
  2:      dx==15(.5)
  4: -16==dy
  8:      dy==15(.5)*/
static const int OC_SQUARE_NSITES[11]={8,5,5,0,5,3,3,0,5,3,3};
/*The list of sites to search for each boundary condition in the square
   pattern.*/
static const int OC_SQUARE_SITES[11][8]={
  /* -15.5<dx<31,       -15.5<dy<15(.5)*/
  {0,1,2,3,5,6,7,8},
  /*-15.5==dx,          -15.5<dy<15(.5)*/
  {1,2,5,7,8},
  /*     dx==15(.5),    -15.5<dy<15(.5)*/
  {0,1,3,6,7},
  /*-15.5==dx==15(.5),  -15.5<dy<15(.5)*/
  {-1},
  /* -15.5<dx<15(.5),  -15.5==dy*/
  {3,5,6,7,8},
  /*-15.5==dx,         -15.5==dy*/
  {5,7,8},
  /*     dx==15(.5),   -15.5==dy*/
  {3,6,7},
  /*-15.5==dx==15(.5), -15.5==dy*/
  {-1},
  /*-15.5dx<15(.5),           dy==15(.5)*/
  {0,1,2,3,5},
  /*-15.5==dx,                dy==15(.5)*/
  {1,2,5},
  /*       dx==15(.5),        dy==15(.5)*/
  {0,1,3}
};

/*Swaps two integers _a and _b if _a>_b.*/
#define OC_SORT2I(_a,_b)\
  if((_a)>(_b)){\
    int t__;\
    t__=(_a);\
    (_a)=(_b);\
    (_b)=t__;\
  }


#define OC_MAXI(_a,_b)      ((_a)<(_b)?(_b):(_a))
#define OC_MINI(_a,_b)      ((_a)>(_b)?(_b):(_a))
/*Clamps an integer into the given range.
  If _a>_c, then the lower bound _a is respected over the upper bound _c (this
   behavior is required to meet our documented API behavior).
  _a: The lower bound.
  _b: The value to clamp.
  _c: The upper boud.*/
#define OC_CLAMPI(_a,_b,_c) (OC_MAXI(_a,OC_MINI(_b,_c)))

/*Divides an integer by a power of two, truncating towards 0.
  _dividend: The integer to divide.
  _shift:    The non-negative power of two to divide by.
  _rmask:    (1<<_shift)-1*/
#define OC_DIV_POW2(_dividend,_shift,_rmask)\
  ((_dividend)+(((_dividend)>>sizeof(_dividend)*8-1)&(_rmask))>>(_shift))
/*Divides _x by 65536, truncating towards 0.*/
#define OC_DIV2_16(_x) OC_DIV_POW2(_x,16,0xFFFF)
/*Divides _x by 2, truncating towards 0.*/
#define OC_DIV2(_x) OC_DIV_POW2(_x,1,0x1)

/*Right shifts _dividend by _shift, adding _rval, and subtracting one for
   negative dividends first..
  When _rval is (1<<_shift-1), this is equivalent to division with rounding
   ties towards positive infinity.*/
#define OC_DIV_ROUND_POW2(_dividend,_shift,_rval)\
  ((_dividend)+((_dividend)>>sizeof(_dividend)*8-1)+(_rval)>>(_shift))

static void oc_mcenc_find_candidates(CP_INSTANCE *cpi, 
				     mc_state *_mcenc,
				     int _mbi,
				     int _which_frame){
  macroblock_t *nemb;
  macroblock_t *emb;
  ogg_int32_t   mvapw1;
  ogg_int32_t   mvapw2;
  mv_t          a[3];
  int           ncandidates;
  int           i;
  emb = &cpi->macro[_mbi];
  if(emb->ncneighbors>0){
    /*Fill in the first part of set A: the last motion vectors used and the
       vectors from adjacent blocks.*/
    /*Skip a position to store the median predictor in.*/
    ncandidates=1;
    for(i=0;i<emb->ncneighbors;i++){
      nemb=&cpi->macro[emb->cneighbors[i]];
      _mcenc->candidates[ncandidates].x = nemb->analysis_mv[0][_which_frame].x;
      _mcenc->candidates[ncandidates].y = nemb->analysis_mv[0][_which_frame].y;
      ncandidates++;
    }
    /*Add a few additional vectors to set A: the vector used in the previous
       frame and the (0,0) vector.*/
    _mcenc->candidates[ncandidates].x=emb->analysis_mv[1][_which_frame].x;
    _mcenc->candidates[ncandidates].y=emb->analysis_mv[1][_which_frame].y;
    ncandidates++;
    _mcenc->candidates[ncandidates].x=0;
    _mcenc->candidates[ncandidates].y=0;
    ncandidates++;
    /*Use the first three vectors of set A to find our best predictor: their
       median.*/
    memcpy(a,_mcenc->candidates+1,sizeof(a));
    OC_SORT2I(a[0].x,a[1].x);
    OC_SORT2I(a[0].y,a[1].y);
    OC_SORT2I(a[1].x,a[2].x);
    OC_SORT2I(a[1].y,a[2].y);
    OC_SORT2I(a[0].x,a[1].x);
    OC_SORT2I(a[0].y,a[1].y);
    _mcenc->candidates[0].x=a[1].x;
    _mcenc->candidates[0].y=a[1].y;
  }
  /*The upper-left most macro block has no neighbors at all
    We just use 0,0 as the median predictor and its previous motion vector
     for set A.*/
  else{
    _mcenc->candidates[0].x=0;
    _mcenc->candidates[0].y=0;
    _mcenc->candidates[1].x=emb->analysis_mv[1][_which_frame].x;
    _mcenc->candidates[1].y=emb->analysis_mv[1][_which_frame].y;
    ncandidates=2;
  }
  /*Fill in set B: accelerated predictors for this and adjacent macro
     blocks.*/
  _mcenc->setb0=ncandidates;
  mvapw1=_mcenc->mvapw1[_which_frame];
  mvapw2=_mcenc->mvapw2[_which_frame];
  /*The first time through the loop use the current macro block.*/
  nemb=emb;
  for(i=0;;i++){
    _mcenc->candidates[ncandidates].x =
      OC_DIV_ROUND_POW2(nemb->analysis_mv[1][_which_frame].x*mvapw1-
			nemb->analysis_mv[2][_which_frame].x*mvapw2,16,0x8000);
    _mcenc->candidates[ncandidates].y =
      OC_DIV_ROUND_POW2(nemb->analysis_mv[1][_which_frame].y*mvapw1-
			nemb->analysis_mv[2][_which_frame].y*mvapw2,16,0x8000);
    _mcenc->candidates[ncandidates].x = OC_CLAMPI(-31,_mcenc->candidates[ncandidates].x,31);
    _mcenc->candidates[ncandidates].y = OC_CLAMPI(-31,_mcenc->candidates[ncandidates].y,31);
    ncandidates++;
    if(i>=emb->npneighbors)break;
    nemb=&cpi->macro[emb->pneighbors[i]];
  }
  /*Truncate to full-pel positions.*/
  for(i=0;i<ncandidates;i++){
    _mcenc->candidates[i].x=OC_DIV2(_mcenc->candidates[i].x);
    _mcenc->candidates[i].y=OC_DIV2(_mcenc->candidates[i].y);
  }
  _mcenc->ncandidates=ncandidates;
}

static int oc_sad8_halfpel(const unsigned char *_cur,
			   const unsigned char *_ref0,
			   const unsigned char *_ref1,
			   int _stride){
  int i;
  int j;
  int err;
  err=0;
  for(i=0;i<8;i++){
    for(j=0;j<8;j++)
      err+=abs(_cur[j]-((int)_ref0[j]+_ref1[j]>>1));
    _cur+=_stride;
    _ref0+=_stride;
    _ref1+=_stride;
  }
  return err;
}

static int oc_sad8_fullpel(const unsigned char *_cur,
			   const unsigned char *_ref,
			   int _stride){
  int i;
  int j;
  int err;
  err=0;
  for(i=0;i<8;i++){
    for(j=0;j<8;j++)
      err+=abs(_cur[j]-(int)_ref[j]);
    _cur+=_stride;
    _ref+=_stride;
  }
  return err;
}

static int oc_sad16_halfpel(CP_INSTANCE *cpi, 
			    int mbi,
			    int _mvoffset0,
			    int _mvoffset1,
			    int _goldenp){

  macroblock_t *mb = &cpi->macro[mbi];
  int err;
  int i;
  err=0;
  for(i=0;i<4;i++){
    int fi = mb->yuv[0][i];
    if(fi < cpi->frag_total){ /* last fragment is the 'invalid fragment' */
      ogg_uint32_t base_offset = cpi->frag_buffer_index[fi];
      const unsigned char *cur = cpi->frame + base_offset;
      const unsigned char *ref = (_goldenp ? cpi->golden : cpi->recon) + base_offset;
      
      err+=oc_sad8_halfpel(cur, ref+_mvoffset0, ref+_mvoffset1, cpi->stride[0]);

    }
  }
  
  return err;
}

static int oc_mcenc_ysad_check_mbcandidate_fullpel(CP_INSTANCE *cpi, 
						   mc_state *_mcenc,
						   int _mbi,
						   mv_t _delta,
						   int _goldenp,
						   int _block_err[4]){
  int                      stride;
  int                      mvoffset;
  int                      err;
  int                      bi;
  macroblock_t            *mb = &cpi->macro[_mbi];
  /*TODO: customize error function for speed/(quality+size) tradeoff.*/
  stride=cpi->stride[0];
  mvoffset=_delta.x+_delta.y*stride;
  err=0;
  for(bi=0;bi<4;bi++){
    int fi = mb->yuv[0][bi];
    if(fi < cpi->frag_total){ /* last fragment is the 'invalid fragment' */
      ogg_uint32_t base_offset = cpi->frag_buffer_index[fi];
      const unsigned char *cur = cpi->frame + base_offset;
      const unsigned char *ref = (_goldenp ? cpi->golden : cpi->recon) + base_offset;
      
      _block_err[bi]=oc_sad8_fullpel(cur,ref+mvoffset,stride);
      err+=_block_err[bi];
    }
  }
  return err;
}

static int oc_mcenc_ysad_halfpel_mbrefine(CP_INSTANCE *cpi, 
					  mc_state *_mcenc,
					  int _mbi,
					  mv_t *_vec,
					  int _best_err,
					  int _goldenp){
  int                      offset_y[9];
  int                      stride;
  int                      mvoffset_base;
  int                      best_site;
  int                      sitei;
  int                      err;

  stride=cpi->stride[0];
  mvoffset_base=_vec->x+_vec->y*stride;
  offset_y[0]=offset_y[1]=offset_y[2]=-stride;
  offset_y[3]=offset_y[5]=0;
  offset_y[6]=offset_y[7]=offset_y[8]=stride;
  err=_best_err;
  best_site=4;
  for(sitei=0;sitei<8;sitei++){
    int site;
    int xmask;
    int ymask;
    int dx;
    int dy;
    int mvoffset0;
    int mvoffset1;

    site=OC_SQUARE_SITES[0][sitei];
    dx=OC_SQUARE_DX[site];
    dy=OC_SQUARE_DY[site];
    /*The following code SHOULD be equivalent to
      oc_state_get_mv_offsets(&_mcenc->enc.state,&mvoffset0,&mvoffset1,
       (_vec->x<<1)+dx,(_vec->y<<1)+dy,ref_ystride,0);
      However, it should also be much faster, as it involves no multiplies and
       doesn't have to handle chroma vectors.*/
    xmask=-((((_vec->x<<1)+dx)^dx)<0);
    ymask=-((((_vec->y<<1)+dy)^dy)<0);
    mvoffset0=mvoffset_base+(dx&xmask)+(offset_y[site]&ymask);
    mvoffset1=mvoffset_base+(dx&~xmask)+(offset_y[site]&~ymask);

    err=oc_sad16_halfpel(cpi,_mbi,mvoffset0,mvoffset1,_goldenp);
    if(err<_best_err){
      _best_err=err;
      best_site=site;
    }
  }
  _vec->x=(_vec->x<<1)+OC_SQUARE_DX[best_site];
  _vec->y=(_vec->y<<1)+OC_SQUARE_DY[best_site];
  return _best_err;
}

static int oc_mcenc_ysad_halfpel_brefine(CP_INSTANCE *cpi, 
					 mc_state *_mcenc,
					 int _mbi,
					 int _bi,
					 mv_t *_vec,
					 int _best_err,
					 int _goldenp){
  macroblock_t *mb = &cpi->macro[_mbi];
  int offset_y[9];
  int stride = cpi->stride[0];
  int mvoffset_base;
  int best_site;
  int sitei;
  int err;
  int fi = mb->yuv[0][_bi];

  if(fi == cpi->frag_total) return _best_err;

  mvoffset_base=_vec->x+_vec->y*stride;
  offset_y[0]=offset_y[1]=offset_y[2]=-stride;
  offset_y[3]=offset_y[5]=0;
  offset_y[6]=offset_y[7]=offset_y[8]=stride;
  err=_best_err;
  best_site=4;

  for(sitei=0;sitei<8;sitei++){
    int site;
    int xmask;
    int ymask;
    int dx;
    int dy;
    int mvoffset0;
    int mvoffset1;

    ogg_uint32_t base_offset = cpi->frag_buffer_index[fi];
    const unsigned char *cur = cpi->frame + base_offset;
    const unsigned char *ref = (_goldenp ? cpi->golden : cpi->recon) + base_offset;

    site=OC_SQUARE_SITES[0][sitei];
    dx=OC_SQUARE_DX[site];
    dy=OC_SQUARE_DY[site];
    /*The following code SHOULD be equivalent to
      oc_state_get_mv_offsets(&_mcenc->enc.state,&mvoffset0,&mvoffset1,
       (_vec[0]<<1)+dx,(_vec[1]<<1)+dy,ref_ystride,0);
      However, it should also be much faster, as it involves no multiplies and
       doesn't have to handle chroma vectors.*/
    xmask=-((((_vec->x<<1)+dx)^dx)<0);
    ymask=-((((_vec->y<<1)+dy)^dy)<0);
    mvoffset0=mvoffset_base+(dx&xmask)+(offset_y[site]&ymask);
    mvoffset1=mvoffset_base+(dx&~xmask)+(offset_y[site]&~ymask);

    err=oc_sad8_halfpel(cur, ref+mvoffset0, ref+mvoffset1,stride);

    if(err<_best_err){
      _best_err=err;
      best_site=site;
    }
  }
  _vec->x=(_vec->x<<1)+OC_SQUARE_DX[best_site];
  _vec->y=(_vec->y<<1)+OC_SQUARE_DY[best_site];
  return _best_err;
}

/* Perform a motion vector search for this macro block against a single
   reference frame.
  
   As a bonus, individual block motion vectors are computed as well, as much of
   the work can be shared.
  
   The actual motion vector is stored in the appropriate place in the
   oc_mb_enc_info structure.
  
   _mcenc:    The motion compensation context.
   _mbi:      The macro block index.
   _frame:    The frame to search, either OC_FRAME_PREV or OC_FRAME_GOLD.
   _bmvs:     Returns the individual block motion vectors. */

void oc_mcenc_search(CP_INSTANCE *cpi, 
		     mc_state *_mcenc,
		     int _mbi,
		     int _goldenp,
		     mv_t *_bmvs){
  
  /*TODO: customize error function for speed/(quality+size) tradeoff.*/

  ogg_int32_t     hit_cache[31];
  ogg_int32_t     hitbit;
  int             block_err[4];
  int             best_block_err[4];
  mv_t            best_block_vec[4];
  mv_t            best_vec;
  int             best_err;
  mv_t            cand;
  int             ref_framei;
  int             bi;
  macroblock_t   *mb = &cpi->macro[_mbi];

  /*Find some candidate motion vectors.*/
  oc_mcenc_find_candidates(cpi,_mcenc,_mbi,_goldenp);

  /*Clear the cache of locations we've examined.*/
  memset(hit_cache,0,sizeof(hit_cache));

  /*Start with the median predictor.*/
  cand=_mcenc->candidates[0];
  hit_cache[cand.y+15]|=(ogg_int32_t)1<<cand.x+15;
  best_err=oc_mcenc_ysad_check_mbcandidate_fullpel(cpi,_mcenc,_mbi,cand,
						   _goldenp,block_err);
  best_vec=cand;
  for(bi=0;bi<4;bi++){
    best_block_err[bi]=block_err[bi];
    best_block_vec[bi]=cand;
  }

  /*If this predictor fails, move on to set A.*/
  if(best_err>OC_YSAD_THRESH1){
    int err;
    int ci;
    int ncs;
    int t2;
    /*Compute the early termination threshold for set A.*/
    t2=mb->aerror;
    ncs=OC_MINI(3,mb->ncneighbors);
    for(ci=0;ci<ncs;ci++)
      t2=OC_MAXI(t2,cpi->macro[mb->cneighbors[ci]].aerror);
    t2=t2+(t2>>OC_YSAD_THRESH2_SCALE_BITS)+OC_YSAD_THRESH2_OFFSET;

    /*Examine the candidates in set A.*/
    for(ci=1;ci<_mcenc->setb0;ci++){
      cand=_mcenc->candidates[ci];

      /*If we've already examined this vector, then we would be using it if it
	was better than what we are using.*/
      hitbit=(ogg_int32_t)1<<cand.x+15;
      if(hit_cache[cand.y+15]&hitbit)continue;
      hit_cache[cand.y+15]|=hitbit;
      err=oc_mcenc_ysad_check_mbcandidate_fullpel(cpi,_mcenc,_mbi,cand,_goldenp,block_err);
      if(err<best_err){
        best_err=err;
        best_vec=cand;
      }
      for(bi=0;bi<4;bi++)
	if(block_err[bi]<best_block_err[bi]){
	  best_block_err[bi]=block_err[bi];
	  best_block_vec[bi]=cand;
	}
    }

    if(best_err>t2){
      /*Examine the candidates in set B.*/
      for(;ci<_mcenc->ncandidates;ci++){
        cand=_mcenc->candidates[ci];
        hitbit=(ogg_int32_t)1<<cand.x+15;
        if(hit_cache[cand.y+15]&hitbit)continue;
        hit_cache[cand.y+15]|=hitbit;
        err=oc_mcenc_ysad_check_mbcandidate_fullpel(cpi,_mcenc,_mbi,cand,_goldenp,block_err);
        if(err<best_err){
          best_err=err;
          best_vec=cand;
        }
        for(bi=0;bi<4;bi++)
	  if(block_err[bi]<best_block_err[bi]){
	    best_block_err[bi]=block_err[bi];
	    best_block_vec[bi]=cand;
	  }
      }

      /*Use the same threshold for set B as in set A.*/
      if(best_err>t2){
        int best_site;
        int nsites;
        int sitei;
        int site;
        int b;
        /*Square pattern search.*/
        for(;;){
          best_site=4;
          /*Compose the bit flags for boundary conditions.*/
          b=OC_DIV16(-best_vec.x+1)|OC_DIV16(best_vec.x+1)<<1|
	    OC_DIV16(-best_vec.y+1)<<2|OC_DIV16(best_vec.y+1)<<3;
          nsites=OC_SQUARE_NSITES[b];
          for(sitei=0;sitei<nsites;sitei++){
            site=OC_SQUARE_SITES[b][sitei];
            cand.x=best_vec.x+OC_SQUARE_DX[site];
            cand.y=best_vec.y+OC_SQUARE_DY[site];
            hitbit=(ogg_int32_t)1<<cand.x+15;
            if(hit_cache[cand.y+15]&hitbit)continue;
            hit_cache[cand.y+15]|=hitbit;
            err=oc_mcenc_ysad_check_mbcandidate_fullpel(cpi,_mcenc,_mbi,cand,_goldenp,block_err);
            if(err<best_err){
              best_err=err;
              best_site=site;
            }
            for(bi=0;bi<4;bi++)
	      if(block_err[bi]<best_block_err[bi]){
		best_block_err[bi]=block_err[bi];
		best_block_vec[bi]=cand;
	      }
          }
          if(best_site==4)break;
          best_vec.x+=OC_SQUARE_DX[best_site];
          best_vec.y+=OC_SQUARE_DY[best_site];
        }

        /*Final 4-MV search.*/
        /*Simply use 1/4 of the macro block set A and B threshold as the
           individual block threshold.*/
	if(_bmvs){
	  t2>>=2;
	  for(bi=0;bi<4;bi++)
	    if(best_block_err[bi]>t2){
	      /*Square pattern search. We do this in a slightly interesting manner.
		We continue to check the SAD of all four blocks in the macroblock.
		This gives us two things:
		
 	        1) We can continue to use the hit_cache to avoid
		   duplicate checks.  Otherwise we could continue to
		   read it, but not write to it without saving and
		   restoring it for each block.  Note that we could
		   still eliminate a large number of duplicate checks
		   by taking into account the site we came from when
		   choosing the site list.  We can still do that to
		   avoid extra hit_cache queries, and it might even be
		   a speed win.

		2) It gives us a slightly better chance of escaping local minima.
		   We would not be here if we weren't doing a fairly bad job in
		   finding a good vector, and checking these vectors can save us
		   from 100 to several thousand points off our SAD 1 in 15
		   times.

		TODO: Is this a good idea?
		Who knows. It needs more testing.*/

	      for(;;){
		mv_t best;
		int bj;
		best=best_block_vec[bi];
		/*Compose the bit flags for boundary conditions.*/
		b=OC_DIV16(-best.x+1)|OC_DIV16(best.x+1)<<1|
		  OC_DIV16(-best.y+1)<<2|OC_DIV16(best.y+1)<<3;
		nsites=OC_SQUARE_NSITES[b];
		for(sitei=0;sitei<nsites;sitei++){
		  site=OC_SQUARE_SITES[b][sitei];
		  cand.x=best.x+OC_SQUARE_DX[site];
		  cand.y=best.y+OC_SQUARE_DY[site];
		  hitbit=(ogg_int32_t)1<<cand.x+15;
		  if(hit_cache[cand.y+15]&hitbit)continue;
		  hit_cache[cand.y+15]|=hitbit;
		  err=oc_mcenc_ysad_check_mbcandidate_fullpel(cpi,_mcenc,_mbi,cand,_goldenp,block_err);
		  if(err<best_err){
		    best_err=err;
		    best_vec=cand;
		  }
		  for(bj=0;bj<4;bj++)
		    if(block_err[bj]<best_block_err[bj]){
		      best_block_err[bj]=block_err[bj];
		      best_block_vec[bj]=cand;
		    }
		  
		}
		if(best_block_vec[bi].x==best.x && best_block_vec[bi].y==best.y) break;
	      }
	    }
	}
      }
    }
  }

  {

    int error=oc_mcenc_ysad_halfpel_mbrefine(cpi,_mcenc,_mbi,&best_vec,best_err,ref_framei);
    if(!_goldenp) mb->aerror = error;
    mb->analysis_mv[0][_goldenp]=best_vec;

    if(_bmvs){
      for(bi=0;bi<4;bi++){
	oc_mcenc_ysad_halfpel_brefine(cpi,_mcenc,_mbi,bi,
				      &best_block_vec[bi],
				      best_block_err[bi],
				      _goldenp);
	_bmvs[bi]=best_block_vec[bi];
      }
    }
  }
}


void oc_mcenc_start(CP_INSTANCE *cpi,
                    mc_state *mcenc){

  ogg_int64_t  nframes;

  /*Set up the accelerated MV weights for previous frame prediction.*/
  mcenc->mvapw1[OC_FRAME_PREV]=(ogg_int32_t)1<<17;
  mcenc->mvapw2[OC_FRAME_PREV]=(ogg_int32_t)1<<16;

  /*Set up the accelerated MV weights for golden frame prediction.*/
  nframes=cpi->LastKeyFrame;

  mcenc->mvapw1[OC_FRAME_GOLD]=(ogg_int32_t)(nframes!=1?(nframes<<17)/(nframes-1):0);
  mcenc->mvapw2[OC_FRAME_GOLD]=(ogg_int32_t)(nframes!=2?(nframes<<16)/(nframes-2):0);

}
