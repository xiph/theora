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
#define OC_YSAD_THRESH2_SCALE_BITS (4)
/*The amount to add to the second maximum Y plane threshold when inflating
   it.*/
#define OC_YSAD_THRESH2_OFFSET     (64)

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


static void oc_mcenc_find_candidates(CP_INSTANCE *cpi,mc_state *_mcenc,
 int _mbi,int _goldenp){
  macroblock_t *nemb;
  macroblock_t *emb;
  ogg_int32_t   mvapw1;
  ogg_int32_t   mvapw2;
  int           a[3][2];
  int           ncandidates;
  int           i;
  emb=cpi->macro+_mbi;
  if(emb->ncneighbors>0){
    /*Fill in the first part of set A: the last motion vectors used and the
       vectors from adjacent blocks.*/
    /*Skip a position to store the median predictor in.*/
    ncandidates=1;
    for(i=0;i<emb->ncneighbors;i++){
      nemb=cpi->macro+emb->cneighbors[i];
      _mcenc->candidates[ncandidates][0]=nemb->analysis_mv[0][_goldenp][0];
      _mcenc->candidates[ncandidates][1]=nemb->analysis_mv[0][_goldenp][1];
      ncandidates++;
    }
    /*Add a few additional vectors to set A: the vector used in the
       previous frame and the (0,0) vector.*/
    _mcenc->candidates[ncandidates][0]=emb->analysis_mv[1][_goldenp][0];
    _mcenc->candidates[ncandidates][1]=emb->analysis_mv[1][_goldenp][1];
    ncandidates++;
    _mcenc->candidates[ncandidates][0]=0;
    _mcenc->candidates[ncandidates][1]=0;
    ncandidates++;
    /*Use the first three vectors of set A to find our best predictor: their
       median.*/
    memcpy(a,_mcenc->candidates+1,sizeof(a));
    OC_SORT2I(a[0][0],a[1][0]);
    OC_SORT2I(a[0][1],a[1][1]);
    OC_SORT2I(a[1][0],a[2][0]);
    OC_SORT2I(a[1][1],a[2][1]);
    OC_SORT2I(a[0][0],a[1][0]);
    OC_SORT2I(a[0][1],a[1][1]);
    _mcenc->candidates[0][0]=a[1][0];
    _mcenc->candidates[0][1]=a[1][1];
  }
  else{
    /*The upper-left most macro block has no neighbors at all
      We just use 0,0 as the median predictor and its previous motion vector
      for set A.*/
    _mcenc->candidates[0][0]=0;
    _mcenc->candidates[0][1]=1;
    _mcenc->candidates[1][0]=emb->analysis_mv[1][_goldenp][0];
    _mcenc->candidates[1][1]=emb->analysis_mv[1][_goldenp][1];
    ncandidates=2;
  }
  /*Fill in set B: accelerated predictors for this and adjacent macro
     blocks.*/
  _mcenc->setb0=ncandidates;
  mvapw1=_mcenc->mvapw1[_goldenp];
  mvapw2=_mcenc->mvapw2[_goldenp];
  /*The first time through the loop use the current macro block.*/
  nemb=emb;
  for(i=0;;i++){
    _mcenc->candidates[ncandidates][0]=OC_CLAMPI(-31,
     OC_DIV_POW2_RE(nemb->analysis_mv[1][_goldenp][0]*mvapw1
     -nemb->analysis_mv[2][_goldenp][0]*mvapw2,16),31);
    _mcenc->candidates[ncandidates][1]=OC_CLAMPI(-31,
     OC_DIV_POW2_RE(nemb->analysis_mv[1][_goldenp][1]*mvapw1
     -nemb->analysis_mv[2][_goldenp][1]*mvapw2,16),31);
    ncandidates++;
    if(i>=emb->npneighbors)break;
    nemb=cpi->macro+emb->pneighbors[i];
  }
  /*Truncate to full-pel positions.*/
  for(i=0;i<ncandidates;i++){
    _mcenc->candidates[i][0]=OC_DIV2(_mcenc->candidates[i][0]);
    _mcenc->candidates[i][1]=OC_DIV2(_mcenc->candidates[i][1]);
  }
  _mcenc->ncandidates=ncandidates;
}

static int oc_sad16_halfpel(CP_INSTANCE *cpi,int mbi,
 int _mvoffset0,int _mvoffset1,int _goldenp,int _best_err){
  macroblock_t *mb;
  int           err;
  int           i;
  mb=cpi->macro+mbi;
  err=0;
  for(i=0;i<4;i++){
    int fi = mb->Ryuv[0][i];
    ogg_uint32_t base_offset = cpi->frag_buffer_index[fi];
    const unsigned char *cur = cpi->frame + base_offset;
    const unsigned char *ref = (_goldenp ? cpi->golden : cpi->lastrecon) + base_offset;
    fi=mb->Ryuv[0][i];
    base_offset=cpi->frag_buffer_index[fi];
    cur=cpi->frame+base_offset;
    ref=(_goldenp?cpi->golden:cpi->lastrecon)+base_offset;
    err+=dsp_sad8x8_xy2_thres(cpi->dsp,cur,
     ref+_mvoffset0,ref+_mvoffset1,cpi->stride[0],_best_err-err);
  }
  return err;
}

static int oc_mcenc_ysad_check_mbcandidate_fullpel(CP_INSTANCE *cpi, 
 mc_state *_mcenc,int _mbi,int _dx,int _dy,int _goldenp,int _block_err[4]){
  int           stride;
  int           mvoffset;
  int           err;
  int           bi;
  macroblock_t *mb;
  mb=cpi->macro+_mbi;
  /*TODO: customize error function for speed/(quality+size) tradeoff.*/
  stride=cpi->stride[0];
  mvoffset=_dx+_dy*stride;
  err=0;
  for(bi=0;bi<4;bi++){
    int fi;
    fi=mb->Ryuv[0][bi];
    /*Only check valid fragments.*/
    if(fi<cpi->frag_total){
      ogg_uint32_t         base_offset;
      const unsigned char *cur;
      const unsigned char *ref;
      base_offset=cpi->frag_buffer_index[fi];
      cur=cpi->frame+base_offset;
      ref=(_goldenp?cpi->golden:cpi->lastrecon)+base_offset;
      _block_err[bi]=dsp_sad8x8_thres(cpi->dsp,cur,ref+mvoffset,stride,16384);
      err+=_block_err[bi];
    }
  }
  return err;
}

static int oc_mcenc_ysad_halfpel_mbrefine(CP_INSTANCE *cpi,int _mbi,
 int _vec[2],int _best_err,int _goldenp){
  int offset_y[9];
  int stride;
  int mvoffset_base;
  int best_site;
  int sitei;
  int err;
  stride=cpi->stride[0];
  mvoffset_base=_vec[0]+_vec[1]*stride;
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
         (_vec[0]<<1)+dx,(_vec[1]<<1)+dy,ref_ystride,0);
      However, it should also be much faster, as it involves no multiplies and
       doesn't have to handle chroma vectors.*/
    xmask=OC_SIGNMASK(((_vec[0]<<1)+dx)^dx);
    ymask=OC_SIGNMASK(((_vec[1]<<1)+dy)^dy);
    mvoffset0=mvoffset_base+(dx&xmask)+(offset_y[site]&ymask);
    mvoffset1=mvoffset_base+(dx&~xmask)+(offset_y[site]&~ymask);
    err=oc_sad16_halfpel(cpi,_mbi,mvoffset0,mvoffset1,_goldenp,_best_err);
    if(err<_best_err){
      _best_err=err;
      best_site=site;
    }
  }
  _vec[0]=(_vec[0]<<1)+OC_SQUARE_DX[best_site];
  _vec[1]=(_vec[1]<<1)+OC_SQUARE_DY[best_site];
  return _best_err;
}

static int oc_mcenc_ysad_halfpel_brefine(CP_INSTANCE *cpi,int _mbi,
 int _bi,int _vec[2],int _best_err,int _goldenp){
  macroblock_t *mb;
  int           offset_y[9];
  int           stride;
  int           mvoffset_base;
  int           best_site;
  int           sitei;
  int           err;
  int           fi;
  mb=cpi->macro+_mbi;
  stride=cpi->stride[0];
  fi=mb->Ryuv[0][_bi];
  if(fi>=cpi->frag_total)return _best_err;
  mvoffset_base=_vec[0]+_vec[1]*stride;
  offset_y[0]=offset_y[1]=offset_y[2]=-stride;
  offset_y[3]=offset_y[5]=0;
  offset_y[6]=offset_y[7]=offset_y[8]=stride;
  err=_best_err;
  best_site=4;
  for(sitei=0;sitei<8;sitei++){
    ogg_uint32_t         base_offset;
    const unsigned char *cur;
    const unsigned char *ref;
    int                  site;
    int                  xmask;
    int                  ymask;
    int                  dx;
    int                  dy;
    int                  mvoffset0;
    int                  mvoffset1;
    base_offset=cpi->frag_buffer_index[fi];
    cur=cpi->frame+base_offset;
    ref=(_goldenp?cpi->golden:cpi->lastrecon)+base_offset;
    site=OC_SQUARE_SITES[0][sitei];
    dx=OC_SQUARE_DX[site];
    dy=OC_SQUARE_DY[site];
    /*The following code SHOULD be equivalent to
        oc_state_get_mv_offsets(&_mcenc->enc.state,&mvoffset0,&mvoffset1,
         (_vec[0]<<1)+dx,(_vec[1]<<1)+dy,ref_ystride,0);
      However, it should also be much faster, as it involves no multiplies and
       doesn't have to handle chroma vectors.*/
    xmask=OC_SIGNMASK(((_vec[0]<<1)+dx)^dx);
    ymask=OC_SIGNMASK(((_vec[1]<<1)+dy)^dy);
    mvoffset0=mvoffset_base+(dx&xmask)+(offset_y[site]&ymask);
    mvoffset1=mvoffset_base+(dx&~xmask)+(offset_y[site]&~ymask);
    err=dsp_sad8x8_xy2_thres(cpi->dsp,cur,
     ref+mvoffset0,ref+mvoffset1,stride,_best_err);
    if(err<_best_err){
      _best_err=err;
      best_site=site;
    }
  }
  _vec[0]=(_vec[0]<<1)+OC_SQUARE_DX[best_site];
  _vec[1]=(_vec[1]<<1)+OC_SQUARE_DY[best_site];
  return _best_err;
}

/*Perform a motion vector search for this macro block against a single
   reference frame.
  As a bonus, individual block motion vectors are computed as well, as much of
   the work can be shared.
  The actual motion vector is stored in the appropriate place in the
   oc_mb_enc_info structure.
  _mcenc:    The motion compensation context.
  _mbi:      The macro block index.
  _frame:    The frame to search, either OC_FRAME_PREV or OC_FRAME_GOLD.
  _bmvs:     Returns the individual block motion vectors.*/
void oc_mcenc_search(CP_INSTANCE *cpi,mc_state *_mcenc,int _mbi,
 int _goldenp,oc_mv _bmvs[4],int *_best_err,int _best_block_err[4]){
  /*Note: Traditionally this search is done using a rate-distortion objective
     function of the form D+lambda*R.
    However, xiphmont tested this and found it produced a small degredation,
     while requiring extra computation.
    This is most likely due to Theora's peculiar MV encoding scheme: MVs are
     not coded relative to a predictor, and the only truly cheap way to use a
     MV is in the LAST or LAST2 MB modes, which are not being considered here.
    Therefore if we use the MV found here, it's only because both LAST and
     LAST2 performed poorly, and therefore the MB is not likely to be uniform
     or suffer from the aperture problem.
    Furthermore we would like to re-use the MV found here for as many MBs as
     possible, so picking a slightly sub-optimal vector to save a bit or two
     may cause increased degredation in many blocks to come.
    We could artificially reduce lambda to compensate, but it's faster to just
     disable it entirely, and use D (the distortion) as the sole criterion.*/
  macroblock_t *mb;
  ogg_int32_t   hit_cache[31];
  ogg_int32_t   hitbit;
  int           block_err[4];
  int           best_vec[2];
  int           best_err;
  int           best_block_vec[4][2];
  int           candx;
  int           candy;
  int           bi;
  mb=cpi->macro+_mbi;
  /*Find some candidate motion vectors.*/
  oc_mcenc_find_candidates(cpi,_mcenc,_mbi,_goldenp);
  /*Clear the cache of locations we've examined.*/
  memset(hit_cache,0,sizeof(hit_cache));
  /*Start with the median predictor.*/
  candx=_mcenc->candidates[0][0];
  candy=_mcenc->candidates[0][1];
  hit_cache[candy+15]|=(ogg_int32_t)1<<candx+15;
  /*TODO: customize error function for speed/(quality+size) tradeoff.*/
  best_err=oc_mcenc_ysad_check_mbcandidate_fullpel(cpi,_mcenc,_mbi,
   candx,candy,_goldenp,block_err);
  best_vec[0]=candx;
  best_vec[1]=candy;
  if(_bmvs){
    for(bi=0;bi<4;bi++){
      _best_block_err[bi]=block_err[bi];
      best_block_vec[bi][0]=candx;
      best_block_vec[bi][1]=candy;
    }
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
    for(ci=0;ci<ncs;ci++)t2=OC_MAXI(t2,cpi->macro[mb->cneighbors[ci]].aerror);
    t2+=(t2>>OC_YSAD_THRESH2_SCALE_BITS)+OC_YSAD_THRESH2_OFFSET;
    /*Examine the candidates in set A.*/
    for(ci=1;ci<_mcenc->setb0;ci++){
      candx=_mcenc->candidates[ci][0];
      candy=_mcenc->candidates[ci][1];
      /*If we've already examined this vector, then we would be using it if it
         was better than what we are using.*/
      hitbit=(ogg_int32_t)1<<candx+15;
      if(hit_cache[candy+15]&hitbit)continue;
      hit_cache[candy+15]|=hitbit;
      err=oc_mcenc_ysad_check_mbcandidate_fullpel(cpi,_mcenc,_mbi,
       candx,candy,_goldenp,block_err);
      if(err<best_err){
        best_err=err;
        best_vec[0]=candx;
        best_vec[1]=candy;
      }
      if(_bmvs){
        for(bi=0;bi<4;bi++)if(block_err[bi]<_best_block_err[bi]){
          _best_block_err[bi]=block_err[bi];
          best_block_vec[bi][0]=candx;
          best_block_vec[bi][1]=candy;
        }
      }
    }
    if(best_err>t2){
      /*Examine the candidates in set B.*/
      for(;ci<_mcenc->ncandidates;ci++){
        candx=_mcenc->candidates[ci][0];
        candy=_mcenc->candidates[ci][1];
        hitbit=(ogg_int32_t)1<<candx+15;
        if(hit_cache[candy+15]&hitbit)continue;
        hit_cache[candy+15]|=hitbit;
        err=oc_mcenc_ysad_check_mbcandidate_fullpel(cpi,_mcenc,_mbi,
         candx,candy,_goldenp,block_err);
        if(err<best_err){
          best_err=err;
          best_vec[0]=candx;
          best_vec[1]=candy;
        }
        if(_bmvs){
          for(bi=0;bi<4;bi++)if(block_err[bi]<_best_block_err[bi]){
            _best_block_err[bi]=block_err[bi];
            best_block_vec[bi][0]=candx;
            best_block_vec[bi][1]=candy;
          }
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
          b=OC_DIV16(-best_vec[0]+1)|OC_DIV16(best_vec[0]+1)<<1|
           OC_DIV16(-best_vec[1]+1)<<2|OC_DIV16(best_vec[1]+1)<<3;
          nsites=OC_SQUARE_NSITES[b];
          for(sitei=0;sitei<nsites;sitei++){
            site=OC_SQUARE_SITES[b][sitei];
            candx=best_vec[0]+OC_SQUARE_DX[site];
            candy=best_vec[1]+OC_SQUARE_DY[site];
            hitbit=(ogg_int32_t)1<<candx+15;
            if(hit_cache[candy+15]&hitbit)continue;
            hit_cache[candy+15]|=hitbit;
            err=oc_mcenc_ysad_check_mbcandidate_fullpel(cpi,_mcenc,_mbi,
             candx,candy,_goldenp,block_err);
            if(err<best_err){
              best_err=err;
              best_site=site;
            }
            if(_bmvs){
              for(bi=0;bi<4;bi++)if(block_err[bi]<_best_block_err[bi]){
                _best_block_err[bi]=block_err[bi];
                best_block_vec[bi][0]=candx;
                best_block_vec[bi][1]=candy;
              }
            }
          }
          if(best_site==4)break;
          best_vec[0]+=OC_SQUARE_DX[best_site];
          best_vec[1]+=OC_SQUARE_DY[best_site];
        }
        /*Final 4-MV search.*/
        /*Simply use 1/4 of the macro block set A and B threshold as the
           individual block threshold.*/
        if(_bmvs){
          t2>>=2;
          for(bi=0;bi<4;bi++){
            if(_best_block_err[bi]>t2){
              /*Square pattern search.
                We do this in a slightly interesting manner.
                We continue to check the SAD of all four blocks in the
                 macro block.
                This gives us two things:
                 1) We can continue to use the hit_cache to avoid duplicate
                     checks.
                    Otherwise we could continue to read it, but not write to it
                     without saving and restoring it for each block.
                    Note that we could still eliminate a large number of
                     duplicate checks by taking into account the site we came
                     from when choosing the site list.
                    We can still do that to avoid extra hit_cache queries, and
                     it might even be a speed win.
                 2) It gives us a slightly better chance of escaping local
                     minima.
                    We would not be here if we weren't doing a fairly bad job
                     in finding a good vector, and checking these vectors can
                     save us from 100 to several thousand points off our SAD 1
                     in 15 times.
                TODO: Is this a good idea?
                Who knows.
                It needs more testing.*/
              for(;;){
                int bestx;
                int besty;
                int bj;
                bestx=best_block_vec[bi][0];
                besty=best_block_vec[bi][1];
                /*Compose the bit flags for boundary conditions.*/
                b=OC_DIV16(-bestx+1)|OC_DIV16(bestx+1)<<1|
                 OC_DIV16(-besty+1)<<2|OC_DIV16(besty+1)<<3;
                nsites=OC_SQUARE_NSITES[b];
                for(sitei=0;sitei<nsites;sitei++){
                  site=OC_SQUARE_SITES[b][sitei];
                  candx=bestx+OC_SQUARE_DX[site];
                  candy=besty+OC_SQUARE_DY[site];
                  hitbit=(ogg_int32_t)1<<candx+15;
                  if(hit_cache[candy+15]&hitbit)continue;
                  hit_cache[candy+15]|=hitbit;
                  err=oc_mcenc_ysad_check_mbcandidate_fullpel(cpi,_mcenc,_mbi,
                   candx,candy,_goldenp,block_err);
                  if(err<best_err){
                    best_err=err;
                    best_vec[0]=candx;
                    best_vec[1]=candy;
                  }
                  for(bj=0;bj<4;bj++)if(block_err[bj]<_best_block_err[bj]){
                    _best_block_err[bj]=block_err[bj];
                    best_block_vec[bj][0]=candx;
                    best_block_vec[bj][1]=candy;
                  }
                }
                if(best_block_vec[bi][0]==bestx&&best_block_vec[bi][1]==besty){
                  break;
                }
              }
            }
          }
        }
      }
    }
  }
  if(!_goldenp)mb->aerror=best_err;
  else mb->gerror=best_err;
  mb->analysis_mv[0][_goldenp][0]=(signed char)(best_vec[0]<<1);
  mb->analysis_mv[0][_goldenp][1]=(signed char)(best_vec[1]<<1);
  if(_bmvs){
    for(bi=0;bi<4;bi++){
      _bmvs[bi][0]=(signed char)(best_block_vec[bi][0]<<1);
      _bmvs[bi][1]=(signed char)(best_block_vec[bi][1]<<1);
    }
  }
  *_best_err=best_err;
}

void oc_mcenc_refine1mv(CP_INSTANCE *cpi,int _mbi,int _goldenp,int _err){
  macroblock_t *mb;
  int           vec[2];
  mb=cpi->macro+_mbi;
  vec[0]=OC_DIV2(mb->analysis_mv[0][_goldenp][0]);
  vec[1]=OC_DIV2(mb->analysis_mv[0][_goldenp][1]);
  _err=oc_mcenc_ysad_halfpel_mbrefine(cpi,_mbi,vec,_err,_goldenp);
  mb->analysis_mv[0][_goldenp][0]=(signed char)vec[0];
  mb->analysis_mv[0][_goldenp][1]=(signed char)vec[1];
  if(!_goldenp)mb->aerror=_err;
  else mb->gerror=_err;
}

void oc_mcenc_refine4mv(CP_INSTANCE *cpi,int _mbi,int _err[4]){
  macroblock_t *mb;
  int           bi;
  mb=cpi->macro+_mbi;
  for(bi=0;bi<4;bi++){
    int vec[2];
    vec[0]=OC_DIV2(mb->block_mv[bi][0]);
    vec[1]=OC_DIV2(mb->block_mv[bi][1]);
    oc_mcenc_ysad_halfpel_brefine(cpi,_mbi,bi,vec,_err[bi],0);
    mb->ref_mv[bi][0]=(signed char)vec[0];
    mb->ref_mv[bi][1]=(signed char)vec[1];
  }
}

void oc_mcenc_start(CP_INSTANCE *cpi,mc_state *_mcenc){
  ogg_int64_t nframes;
  /*Set up the accelerated MV weights for previous frame prediction.*/
  _mcenc->mvapw1[OC_FRAME_PREV]=(ogg_int32_t)1<<17;
  _mcenc->mvapw2[OC_FRAME_PREV]=(ogg_int32_t)1<<16;
  /*Set up the accelerated MV weights for golden frame prediction.*/
  nframes=cpi->LastKeyFrame;
  _mcenc->mvapw1[OC_FRAME_GOLD]=(ogg_int32_t)(
   nframes!=1?(nframes<<17)/(nframes-1):0);
  _mcenc->mvapw2[OC_FRAME_GOLD]=(ogg_int32_t)(
   nframes!=2?(nframes<<16)/(nframes-2):0);
}
