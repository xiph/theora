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
#include "encint.h"

/*A rough lookup table for tan(x), 0<=x<pi/2.
  The values are Q12 fixed-point and spaced at 5 degree intervals.
  These decisions are somewhat arbitrary, but sufficient for the 2nd order
   Bessel follower below.
  Values of x larger than 85 degrees are extrapolated from the last inteval,
   which is way off, but "good enough".*/
static unsigned short OC_ROUGH_TAN_LOOKUP[18]={
      0,  358,  722, 1098, 1491, 1910,
   2365, 2868, 3437, 4096, 4881, 5850,
   7094, 8784,11254,15286,23230,46817
};

/*_alpha is Q24 in the range [0,0.5).
  The return values is 5.12.*/
static int oc_warp_alpha(int _alpha){
  int i;
  int d;
  int t0;
  int t1;
  i=_alpha*36>>24;
  if(i>=17)i=16;
  t0=OC_ROUGH_TAN_LOOKUP[i];
  t1=OC_ROUGH_TAN_LOOKUP[i+1];
  d=_alpha*36-(i<<24);
  return (int)(((ogg_int64_t)t0<<32)+(t1-t0<<8)*(ogg_int64_t)d>>32);
}

/*Initialize a 2nd order low-pass Bessel filter with the corresponding delay
   and initial value.
  _value is Q24.*/
void oc_iir_filter_init(oc_iir_filter *_f,int _delay,ogg_int32_t _value){
  int         alpha;
  ogg_int64_t one48;
  ogg_int64_t warp;
  ogg_int64_t k1;
  ogg_int64_t k2;
  ogg_int64_t d;
  ogg_int64_t a;
  ogg_int64_t ik2;
  ogg_int64_t b1;
  ogg_int64_t b2;
  /*This borrows some code from an unreleased version of Postfish.*/
  /*alpha is Q24*/
  alpha=(1<<24)/_delay;
  one48=(ogg_int64_t)1<<48;
  /*warp is 5.12*/
  warp=oc_warp_alpha(alpha);
  /*k1 is 6.12*/
  k1=3*warp;
  /*k2 is 10.24.*/
  k2=k1*warp;
  /*d is 11.24.*/
  d=((1<<12)+k1<<12)+k2;
  /*a is 34.24.*/
  a=(k2<<24)/d;
  /*ik2 is 25.24.*/
  ik2=one48/k2;
  /*b1 is Q48; in practice, the integer part is limited.*/
  b1=2*a*(ik2-(1<<24));
  /*b2 is Q48; in practice, the integer part is limited.*/
  b2=one48-(4*a<<24)-b1;
  /*All of the filter parameters are Q24.*/
  _f->c[0]=(ogg_int32_t)(b1+(1<<23)>>24);
  _f->c[1]=(ogg_int32_t)(b2+(1<<23)>>24);
  _f->g=(ogg_int32_t)a;
  _f->y[1]=_f->y[0]=_f->x[1]=_f->x[0]=_value;
}

static ogg_int64_t oc_iir_filter_update(oc_iir_filter *_f,int _x){
  ogg_int64_t c0;
  ogg_int64_t c1;
  ogg_int64_t g;
  ogg_int64_t x0;
  ogg_int64_t x1;
  ogg_int64_t y0;
  ogg_int64_t y1;
  ogg_int64_t ya;
  c0=_f->c[0];
  c1=_f->c[1];
  g=_f->g;
  x0=_f->x[0];
  x1=_f->x[1];
  y0=_f->y[0];
  y1=_f->y[1];
  ya=(_x+x0*2+x1)*g+y0*c0+y1*c1+(1<<23)>>24;
  _f->x[1]=(ogg_int32_t)x0;
  _f->x[0]=_x;
  _f->y[1]=(ogg_int32_t)y0;
  _f->y[0]=(ogg_int32_t)ya;
  return ya;
}



/*Search for the quantizer that matches the target most closely.
  We don't assume a linear ordering, but when there are ties we pick the
   quantizer closest to the old one.*/
static int oc_enc_find_qi_for_target(oc_enc_ctx *_enc,int _qti,int _qi_old,
 int _qi_min,ogg_int64_t _log_qtarget){
  ogg_int64_t best_qdiff;
  int         best_qi;
  int         qi;
  best_qi=_qi_min;
  best_qdiff=_enc->log_qavg[_qti][best_qi]-_log_qtarget;
  best_qdiff=best_qdiff+OC_SIGNMASK(best_qdiff)^OC_SIGNMASK(best_qdiff);
  for(qi=_qi_min+1;qi<64;qi++){
    ogg_int64_t qdiff;
    qdiff=_enc->log_qavg[_qti][qi]-_log_qtarget;
    qdiff=qdiff+OC_SIGNMASK(qdiff)^OC_SIGNMASK(qdiff);
    if(qdiff<best_qdiff||
     qdiff==best_qdiff&&abs(qi-_qi_old)<abs(best_qi-_qi_old)){
      best_qi=qi;
      best_qdiff=qdiff;
    }
  }
  return best_qi;
}

void oc_enc_calc_lambda(oc_enc_ctx *_enc,int _qti){
  ogg_int64_t lq;
  int         qi;
  int         qi1;
  int         nqis;
  /*For now, lambda is fixed depending on the qi value and frame type:
      lambda=qscale*(qavg[qti][qi]**2),
     where qscale=0.2125.
    This was derived by exhaustively searching for the optimal quantizer for
     the AC coefficients in each block from a number of test sequences for a
     number of fixed lambda values and fitting the peaks of the resulting
     histograms (on the log(qavg) scale).
    The same model applies to both inter and intra frames.
    A more adaptive scheme might perform better.*/
  qi=_enc->state.qis[0];
  /*If rate control is active, use the lambda for the _target_ quantizer.
    This allows us to scale to rates slightly lower than we'd normally be able
     to reach, and give the rate control a semblance of "fractional qi"
     precision.*/
  if(_enc->state.info.target_bitrate>0)lq=_enc->rc.log_qtarget;
  else lq=_enc->log_qavg[_qti][qi];
  /*The resulting lambda value is less than 0x500000.*/
  _enc->lambda=(int)oc_bexp64(2*lq-0x4780BD468D6B62BLL);
  /*Select additional quantizers.
    The R-D optimal block AC quantizer statistics suggest that the distribution
     is roughly Gaussian-like with a slight positive skew.
    K-means clustering on log_qavg to select 3 quantizers produces cluster
     centers of {log_qavg-0.6,log_qavg,log_qavg+0.7}.
    Experiments confirm these are relatively good choices.

    Although we do greedy R-D optimization of the qii flags to avoid switching
     too frequently, this becomes ineffective at low rates, either because we
     do a poor job of predicting the actual R-D cost, or the greedy
     optimization is not sufficient.
    Therefore adaptive quantization is disabled above an (experimentally
     suggested) threshold of log_qavg=7.00 (e.g., below INTRA qi=12 or
     INTER qi=20 with current matrices).
    This may need to be revised if the R-D cost estimation or qii flag
     optimization strategies change.*/
  nqis=1;
  if(lq<(OC_Q57(56)>>3)&&!_enc->vp3_compatible){
    qi1=oc_enc_find_qi_for_target(_enc,_qti,OC_MAXI(qi-1,0),0,
     lq+(OC_Q57(7)+5)/10);
    if(qi1!=qi)_enc->state.qis[nqis++]=qi1;
    qi1=oc_enc_find_qi_for_target(_enc,_qti,OC_MINI(qi+1,63),0,
     lq-(OC_Q57(6)+5)/10);
    if(qi1!=qi&&qi1!=_enc->state.qis[nqis-1])_enc->state.qis[nqis++]=qi1;
  }
  _enc->state.nqis=nqis;
}

/*Binary exponential of _log_scale with 24-bit fractional precision and
   saturation.
  _log_scale: A binary logarithm in Q57 format.
  Return: The binary exponential in Q24 format, saturated to 2**31-1 if
   _log_scale was too large.*/
static ogg_int32_t oc_bexp_q24(ogg_int64_t _log_scale){
  if(_log_scale<OC_Q57(8)){
    ogg_int64_t ret;
    ret=oc_bexp64(_log_scale+OC_Q57(24));
    return ret<0x7FFFFFFF?(ogg_int32_t)ret:0x7FFFFFFF;
  }
  return 0x7FFFFFFF;
}


void oc_rc_state_reset(oc_rc_state *_rc,const oc_enc_ctx *_enc){
  ogg_int64_t npixels;
  ogg_int64_t ibpp;
  /*TODO: These parameters should be exposed in a th_encode_ctl() API.*/
  _rc->bits_per_frame=(_enc->state.info.target_bitrate*
   (ogg_int64_t)_enc->state.info.fps_denominator)/
   _enc->state.info.fps_numerator;
  /*Insane framerates or frame sizes mean insane bitrates.
    Let's not get carried away.*/
  if(_rc->bits_per_frame>0x400000000000LL){
    _rc->bits_per_frame=(ogg_int64_t)0x400000000000LL;
  }
  else if(_rc->bits_per_frame<32)_rc->bits_per_frame=32;
  _rc->buf_delay=OC_MAXI(_rc->buf_delay,12);
  _rc->max=_rc->bits_per_frame*_rc->buf_delay;
  /*Start with a buffer fullness of 75%.
    We can require fully half the buffer for a keyframe, and so this initial
     level gives us maximum flexibility for over/under-shooting in subsequent
     frames.*/
  _rc->target=_rc->fullness=(_rc->max+1>>1)+(_rc->max+2>>2);
  /*Pick exponents and initial scales for quantizer selection.*/
  npixels=_enc->state.info.frame_width*
   (ogg_int64_t)_enc->state.info.frame_height;
  _rc->log_npixels=oc_blog64(npixels);
  ibpp=npixels/_rc->bits_per_frame;
  if(ibpp<1){
    _rc->exp[0]=59;
    _rc->log_scale[0]=oc_blog64(1997)-OC_Q57(8);
  }
  else if(ibpp<2){
    _rc->exp[0]=55;
    _rc->log_scale[0]=oc_blog64(1604)-OC_Q57(8);
  }
  else{
    _rc->exp[0]=48;
    _rc->log_scale[0]=oc_blog64(834)-OC_Q57(8);
  }
  if(ibpp<4){
    _rc->exp[1]=100;
    _rc->log_scale[1]=oc_blog64(2249)-OC_Q57(8);
  }
  else if(ibpp<8){
    _rc->exp[1]=95;
    _rc->log_scale[1]=oc_blog64(1751)-OC_Q57(8);
  }
  else{
    _rc->exp[1]=73;
    _rc->log_scale[1]=oc_blog64(1260)-OC_Q57(8);
  }
  _rc->prev_drop_count=0;
  _rc->log_drop_scale=OC_Q57(0);
  /*Set up second order followers, initialized according to corresponding
     time constants.*/
  oc_iir_filter_init(&_rc->scalefilter[0],2,oc_bexp_q24(_rc->log_scale[0]));
  oc_iir_filter_init(&_rc->scalefilter[1],_rc->buf_delay>>1,
   oc_bexp_q24(_rc->log_scale[1]));
  oc_iir_filter_init(&_rc->vfrfilter,2,oc_bexp_q24(_rc->log_drop_scale));
}


void oc_rc_state_init(oc_rc_state *_rc,const oc_enc_ctx *_enc){
  /*The buffer size is set equal to the keyframe interval, clamped to the range
     [12,256] frames.
    The 12 frame minimum gives us some chance to distribute bit estimation
     errors.
    The 256 frame maximum means we'll require 8-10 seconds of pre-buffering at
     24-30 fps, which is not unreasonable.*/
  _rc->buf_delay=_enc->keyframe_frequency_force>256?
   256:_enc->keyframe_frequency_force;
  /*By default, enforce all buffer constraints.*/
  _rc->drop_frames=1;
  _rc->cap_overflow=1;
  _rc->cap_underflow=0;
  oc_rc_state_reset(_rc,_enc);
}

int oc_enc_update_rc_state(oc_enc_ctx *_enc,
 long _bits,int _qti,int _qi,int _trial,int _droppable){
  ogg_int64_t buf_delta;
  int         dropped;
  dropped=0;
  if(!_enc->rc.drop_frames)_droppable=0;
  buf_delta=_enc->rc.bits_per_frame*(1+_enc->dup_count);
  if(_bits<=0){
    /*We didn't code any blocks in this frame.
      Add it to the previous frame's dup count.*/
    _enc->rc.prev_drop_count+=1+_enc->dup_count;
    /*If this was the first frame of this type, lower the expected scale, but
       don't set it to zero outright.*/
    if(_trial)_enc->rc.log_scale[_qti]>>=1;
    _bits=0;
  }
  else{
    ogg_int64_t log_scale;
    ogg_int64_t log_bits;
    ogg_int64_t log_qexp;
    /*Compute the estimated scale factor for this frame type.*/
    log_bits=oc_blog64(_bits);
    log_qexp=_enc->log_qavg[_qti][_qi]-OC_Q57(2);
    log_qexp=(log_qexp>>6)*(_enc->rc.exp[_qti]);
    log_scale=OC_MINI(log_bits-_enc->rc.log_npixels+log_qexp,OC_Q57(16));
    /*Use it to set that factor directly if this was a trial.*/
    if(_trial)_enc->rc.log_scale[_qti]=log_scale;
    else{
      /*Otherwise update the low-pass scale filter for this frame type,
         regardless of whether or not we dropped this frame.*/
      _enc->rc.log_scale[_qti]=oc_blog64(oc_iir_filter_update(
       _enc->rc.scalefilter+_qti,oc_bexp_q24(log_scale)))-OC_Q57(24);
      /*If this frame busts our budget, it must be dropped.*/
      if(_droppable&&_enc->rc.fullness+buf_delta<_bits){
        _enc->rc.prev_drop_count+=1+_enc->dup_count;
        _bits=0;
        dropped=1;
      }
      else{
        ogg_uint32_t drop_count;
        /*Update a simple exponential moving average to estimate the "real"
           frame rate taking drops and duplicates into account.
          This is only done if the frame is coded, as it needs the final count
           of dropped frames.*/
        drop_count=_enc->rc.prev_drop_count+1;
        if(drop_count>0x7F)drop_count=0x7FFFFFFF;
        else drop_count<<=24;
        _enc->rc.log_drop_scale=oc_blog64(oc_iir_filter_update(
         &_enc->rc.vfrfilter,drop_count))-OC_Q57(24);
        /*Initialize the drop count for this frame to the user-requested dup
           count.
          It will be increased if we drop more frames.*/
        _enc->rc.prev_drop_count=_enc->dup_count;
      }
    }
  }
  if(!_trial){
    /*And update the buffer fullness level.*/
    _enc->rc.fullness+=buf_delta-_bits;
    /*If we're too quick filling the buffer and overflow is capped,
      that rate is lost forever.*/
    if(_enc->rc.cap_overflow&&_enc->rc.fullness>_enc->rc.max){
      _enc->rc.fullness=_enc->rc.max;
    }
    /*If we're too quick draining the buffer and underflow is capped,
      don't try to make up that rate later.*/
    if(_enc->rc.cap_underflow&&_enc->rc.fullness<0){
      _enc->rc.fullness=0;
    }
  }
  return dropped;
}

int oc_enc_select_qi(oc_enc_ctx *_enc,int _qti,int _clamp){
  ogg_int64_t  rate_total;
  ogg_uint32_t next_key_frame;
  int          nframes[2];
  int          buf_delay;
  ogg_int64_t  log_qtarget;
  ogg_int64_t  log_scale0;
  int          old_qi;
  int          qi;
  /*Figure out how to re-distribute bits so that we hit our fullness target
     before the last keyframe in our current buffer window (after the current
     frame), or the end of the buffer window, whichever comes first.*/
  next_key_frame=_qti?_enc->keyframe_frequency_force
   -(_enc->state.curframe_num-_enc->state.keyframe_num):0;
  nframes[0]=(_enc->rc.buf_delay-OC_MINI(next_key_frame,_enc->rc.buf_delay)
   +_enc->keyframe_frequency_force-1)/_enc->keyframe_frequency_force;
  if(nframes[0]+_qti>1){
    nframes[0]--;
    buf_delay=next_key_frame+nframes[0]*_enc->keyframe_frequency_force;
  }
  else buf_delay=_enc->rc.buf_delay;
  nframes[1]=buf_delay-nframes[0];
  rate_total=_enc->rc.fullness-_enc->rc.target
   +buf_delay*_enc->rc.bits_per_frame;
  /*Downgrade the delta frame rate to correspond to the recent drop count
     history.*/
  if(_enc->rc.prev_drop_count>0||_enc->rc.log_drop_scale>OC_Q57(0)){
    ogg_int64_t dup_scale;
    dup_scale=oc_bexp64((_enc->rc.log_drop_scale
     +oc_blog64(_enc->rc.prev_drop_count+1)>>1)+OC_Q57(8));
    if(dup_scale<nframes[1]<<8){
      int dup_scalei;
      dup_scalei=(int)dup_scale;
      if(dup_scalei>0)nframes[1]=((nframes[1]<<8)+dup_scalei-1)/dup_scalei;
    }
    else nframes[1]=!!nframes[1];
  }
  log_scale0=_enc->rc.log_scale[_qti]+_enc->rc.log_npixels;
  /*If there aren't enough bits to achieve our desired fullness level, use the
     minimum quality permitted.*/
  if(rate_total<=buf_delay)log_qtarget=OC_QUANT_MAX_LOG;
  else{
    static const unsigned char KEY_RATIO[2]={32,17};
    ogg_int64_t   log_scale1;
    ogg_int64_t   prevr;
    ogg_int64_t   curr;
    ogg_int64_t   realr;
    int           i;
    log_scale1=_enc->rc.log_scale[1-_qti]+_enc->rc.log_npixels;
    curr=(rate_total+(buf_delay>>1))/buf_delay;
    realr=curr*KEY_RATIO[_qti]+16>>5;
    for(i=0;i<10;i++){
      ogg_int64_t rdiff;
      ogg_int64_t rderiv;
      ogg_int64_t log_rpow;
      ogg_int64_t rscale;
      ogg_int64_t drscale;
      ogg_int64_t bias;
      prevr=curr;
      log_rpow=oc_blog64(prevr)-log_scale0;
      log_rpow=(log_rpow+(_enc->rc.exp[_qti]>>1))/_enc->rc.exp[_qti]*
       _enc->rc.exp[1-_qti];
      rscale=nframes[1-_qti]*KEY_RATIO[1-_qti]*
       oc_bexp64(log_scale1+log_rpow);
      rdiff=nframes[_qti]*KEY_RATIO[_qti]*prevr+rscale-(rate_total<<5);
      drscale=(rscale+(_enc->rc.exp[_qti]>>1))/_enc->rc.exp[_qti]*
       _enc->rc.exp[1-_qti]/prevr;
      rderiv=nframes[_qti]*KEY_RATIO[_qti]+drscale;
      if(rderiv==0)break;
      bias=rderiv+OC_SIGNMASK(rdiff^rderiv)^OC_SIGNMASK(rdiff^rderiv);
      curr=prevr-((rdiff<<1)+bias)/(rderiv<<1);
      realr=curr*KEY_RATIO[_qti]+16>>5;
      if(curr<=0||realr>rate_total||prevr==curr)break;
    }
    log_qtarget=OC_Q57(2)-((oc_blog64(realr)-log_scale0+(_enc->rc.exp[_qti]>>1))/
     _enc->rc.exp[_qti]<<6);
    log_qtarget=OC_MINI(log_qtarget,OC_QUANT_MAX_LOG);
  }
  /*If this was not one of the initial frames, limit the change in quality.*/
  old_qi=_enc->state.qis[0];
  if(_clamp){
    ogg_int64_t log_qmin;
    ogg_int64_t log_qmax;
    /*Clamp the target quantizer to within [0.8*Q,1.2*Q], where Q is the
       current quantizer.
      TODO: With user-specified quant matrices, we need to enlarge these limits
       if they don't actually let us change qi values.*/
    log_qmin=_enc->log_qavg[_qti][old_qi]-0x00A4D3C25E68DC58LL;
    log_qmax=_enc->log_qavg[_qti][old_qi]+0x00A4D3C25E68DC58LL;
    log_qtarget=OC_CLAMPI(log_qmin,log_qtarget,log_qmax);
  }
  /*The above allocation looks only at the total rate we'll accumulate in the
     next buf_delay frames.
    However, we could bust the budget on the very next frame, so check for that
     here, if we're not using a soft target.*/
  if(!_enc->rc.cap_underflow||_enc->rc.drop_frames){
    ogg_int64_t log_hard_limit;
    ogg_int64_t log_qexp;
    int         exp0;
    /*Allow 50% of the rate for a single frame for prediction error.
      This may not be enough for keyframes or sudden changes in complexity.*/
    log_hard_limit=oc_blog64(_enc->rc.fullness+(_enc->rc.bits_per_frame>>1));
    exp0=_enc->rc.exp[_qti];
    log_qexp=log_qtarget-OC_Q57(2);
    log_qexp=(log_qtarget-OC_Q57(2)>>6)*exp0;
    if(log_scale0-log_qexp>log_hard_limit){
      log_qexp=log_scale0-log_hard_limit;
      log_qtarget=((log_qexp+(exp0>>1))/exp0<<6)+OC_Q57(2);
      log_qtarget=OC_MINI(log_qtarget,OC_QUANT_MAX_LOG);
    }
  }
  qi=oc_enc_find_qi_for_target(_enc,_qti,old_qi,
   _enc->state.info.quality,log_qtarget);
  /*Save the quantizer target for lambda calculations.*/
  _enc->rc.log_qtarget=log_qtarget;
  return qi;
}
