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
#include <limits.h>
#include <ogg/ogg.h>
/*libtheora header.*/
#include <theora/theora.h>
/*theora-exp header.*/
#include "theora/theoradec.h"
/*For oc_ilog et al.*/
#include "../internal.h"

typedef struct th_api_wrapper th_api_wrapper;

/*Generally only one of these pointers will be non-NULL in any given instance.
  Technically we do not even really need this struct, since we should be able
   to figure out which one from "context", but doing it this way makes sure we
   don't flub it up.
  It also means eventually adding encode support will work in the obvious
   manner; theora_clear() does NOT provide enough context to distinguish
   between an encoder and decoder without something like this.*/
struct th_api_wrapper{
  th_setup_info *setup;
  th_dec_ctx    *decode;
};

const char *theora_version_string(void){
  return th_version_string();
}

ogg_uint32_t theora_version_number(void){
  return th_version_number();
}

void theora_info_init(theora_info *_ci){
  th_api_wrapper *api;
  api=(th_api_wrapper *)_ogg_calloc(1,sizeof(*api));
  memset(_ci,0,sizeof(*_ci));
  _ci->codec_setup=api;
}

void theora_info_clear(theora_info *_ci){
  th_api_wrapper *api;
  api=(th_api_wrapper *)_ci->codec_setup;
  if(api->setup)th_setup_free(api->setup);
  if(api->decode)th_decode_free(api->decode);
  free(api);
  memset(_ci,0,sizeof(*_ci));
}

void theora_clear(theora_state *_td){
  if(_td->internal_encode!=NULL){
    (*((oc_enc_dispatch_vtbl *)_td->internal_encode)->clear)(_td);
  }
  else if(_td->i!=NULL){
    theora_info_clear(_td->i);
    _ogg_free(_td->i);
    _td->i=NULL;
  }
}

static void theora_info2th_info(th_info *_info,const theora_info *_ci){
  _info->version_major=_ci->version_major;
  _info->version_minor=_ci->version_minor;
  _info->version_subminor=_ci->version_subminor;
  _info->frame_width=_ci->width;
  _info->frame_height=_ci->height;
  _info->pic_width=_ci->frame_width;
  _info->pic_height=_ci->frame_height;
  _info->pic_x=_ci->offset_x;
  _info->pic_y=_ci->offset_y;
  _info->fps_numerator=_ci->fps_numerator;
  _info->fps_denominator=_ci->fps_denominator;
  _info->aspect_numerator=_ci->aspect_numerator;
  _info->aspect_denominator=_ci->aspect_denominator;
  switch(_ci->colorspace){
    case OC_CS_ITU_REC_470M:_info->colorspace=TH_CS_ITU_REC_470M;break;
    case OC_CS_ITU_REC_470BG:_info->colorspace=TH_CS_ITU_REC_470BG;break;
    default:_info->colorspace=TH_CS_UNSPECIFIED;break;
  }
  switch(_ci->pixelformat){
    case OC_PF_420:_info->pixel_fmt=TH_PF_420;break;
    case OC_PF_422:_info->pixel_fmt=TH_PF_422;break;
    case OC_PF_444:_info->pixel_fmt=TH_PF_444;break;
    default:_info->pixel_fmt=TH_PF_RSVD;
  }
  _info->target_bitrate=_ci->target_bitrate;
  _info->quality=_ci->quality;
  _info->keyframe_granule_shift=_ci->keyframe_frequency_force>0?
   OC_MINI(31,oc_ilog(_ci->keyframe_frequency_force-1)):0;
}

int theora_decode_init(theora_state *_td,theora_info *_ci){
  th_api_wrapper *api;
  th_api_wrapper *dapi;
  th_info         info;
  /*We don't need these two fields.*/
  _td->internal_encode=NULL;
  _td->internal_decode=NULL;
  _td->granulepos=0;
  api=(th_api_wrapper *)_ci->codec_setup;
  /*Make our own copy of the info struct, since its lifetime should be
     independent of the one we were passed in.*/
  _td->i=(theora_info *)_ogg_malloc(sizeof(*_td->i));
  *_td->i=*_ci;
  /*Also make our own copy of the wrapper.*/
  dapi=(th_api_wrapper *)_ogg_calloc(1,sizeof(*dapi));
  _td->i->codec_setup=dapi;
  /*Convert the info struct now instead of saving the the one we decoded with
     theora_decode_header(), since the user might have modified values (i.e.,
     color space, aspect ratio, etc. can be specified from a higher level).
    The user also might be doing something "clever" with the header packets if
     they are not using an Ogg encapsulation.*/
  theora_info2th_info(&info,_ci);
  /*Don't bother to copy the setup info; th_decode_alloc() makes its own copy
     of the stuff it needs.*/
  dapi->decode=th_decode_alloc(&info,api->setup);
  return 0;
}

static void th_info2theora_info(theora_info *_ci,const th_info *_info){
  _ci->version_major=_info->version_major;
  _ci->version_minor=_info->version_minor;
  _ci->version_subminor=_info->version_subminor;
  _ci->width=_info->frame_width;
  _ci->height=_info->frame_height;
  _ci->frame_width=_info->pic_width;
  _ci->frame_height=_info->pic_height;
  _ci->offset_x=_info->pic_x;
  _ci->offset_y=_info->pic_y;
  _ci->fps_numerator=_info->fps_numerator;
  _ci->fps_denominator=_info->fps_denominator;
  _ci->aspect_numerator=_info->aspect_numerator;
  _ci->aspect_denominator=_info->aspect_denominator;
  switch(_info->colorspace){
    case TH_CS_ITU_REC_470M:_ci->colorspace=OC_CS_ITU_REC_470M;break;
    case TH_CS_ITU_REC_470BG:_ci->colorspace=OC_CS_ITU_REC_470BG;break;
    default:_ci->colorspace=OC_CS_UNSPECIFIED;break;
  }
  switch(_info->pixel_fmt){
    case TH_PF_420:_ci->pixelformat=OC_PF_420;break;
    case TH_PF_422:_ci->pixelformat=OC_PF_422;break;
    case TH_PF_444:_ci->pixelformat=OC_PF_444;break;
    default:_ci->pixelformat=OC_PF_RSVD;
  }
  _ci->target_bitrate=_info->target_bitrate;
  _ci->quality=_info->quality;
  _ci->keyframe_frequency_force=1<<_info->keyframe_granule_shift;
}

int theora_decode_header(theora_info *_ci,theora_comment *_cc,ogg_packet *_op){
  th_api_wrapper *api;
  th_info         info;
  int             ret;
  api=(th_api_wrapper *)_ci->codec_setup;
  /*Convert from the theora_info struct instead of saving our own th_info
     struct between calls.
    The user might be doing something "clever" with the header packets if they
     are not using an Ogg encapsulation, and we don't want to break this.*/
  theora_info2th_info(&info,_ci);
  /*We rely on the fact that theora_comment and th_comment structures are
     actually identical.
    Take care not to change this fact unless you change the code here as
     well!*/
  ret=th_decode_headerin(&info,(th_comment *)_cc,&api->setup,_op);
  /*We also rely on the fact that the error return code values are the same,
    and that the implementations of these two functions return the same set of
    them.
   Note that theora_decode_header() really can return OC_NOTFORMAT, even
    though it is not currently documented to do so.*/
  if(ret<0)return ret;
  th_info2theora_info(_ci,&info);
  return 0;
}

int theora_decode_packetin(theora_state *_td,ogg_packet *_op){
  th_api_wrapper *api;
  ogg_int64_t     gp;
  int             ret;
  api=(th_api_wrapper *)_td->i->codec_setup;
  ret=th_decode_packetin(api->decode,_op,&gp);
  if(ret<0)return OC_BADPACKET;
  _td->granulepos=gp;
  return 0;
}

int theora_decode_YUVout(theora_state *_td,yuv_buffer *_yuv){
  th_api_wrapper *api;
  th_ycbcr_buffer buf;
  int             ret;
  api=(th_api_wrapper *)_td->i->codec_setup;
  ret=th_decode_ycbcr_out(api->decode,buf);
  if(ret>=0){
    _yuv->y_width=buf[0].width;
    _yuv->y_height=buf[0].height;
    _yuv->y_stride=buf[0].ystride;
    _yuv->uv_width=buf[1].width;
    _yuv->uv_height=buf[1].height;
    _yuv->uv_stride=buf[1].ystride;
    _yuv->y=buf[0].data;
    _yuv->u=buf[1].data;
    _yuv->v=buf[2].data;
  }
  return ret;
}

int theora_control(theora_state *_td,int _req,void *_buf,size_t _buf_sz){
  if(_td->internal_encode!=NULL){
    return (*((oc_enc_dispatch_vtbl *)_td->internal_encode)->control)(_td,
     _req,_buf,_buf_sz);
  }
  return th_decode_ctl(((th_api_wrapper *)_td->i->codec_setup)->decode,
   _req,_buf,_buf_sz);
}

int theora_packet_isheader(ogg_packet *_op){
  return th_packet_isheader(_op);
}

int theora_packet_iskeyframe(ogg_packet *_op){
  return th_packet_iskeyframe(_op);
}

int theora_granule_shift(theora_info *_ci){
  /*This breaks when keyframe_frequency_force is not positive or is larger than
     2**31 (if your int is more than 32 bits), but that's what the original
     function does.*/
  return oc_ilog(_ci->keyframe_frequency_force-1);
}

ogg_int64_t theora_granule_frame(theora_state *_td,ogg_int64_t _gp){
  if(_td->internal_encode!=NULL){
    return
     (*((oc_enc_dispatch_vtbl *)_td->internal_encode)->granule_frame)(_td,_gp);
  }
  return th_granule_frame(((th_api_wrapper *)_td->i->codec_setup)->decode,_gp);
}

double theora_granule_time(theora_state *_td, ogg_int64_t _gp){
  if(_td->internal_encode!=NULL){
    return
     (*((oc_enc_dispatch_vtbl *)_td->internal_encode)->granule_time)(_td,_gp);
  }
  return th_granule_time(((th_api_wrapper *)_td->i->codec_setup)->decode,_gp);
}

void theora_comment_init(theora_comment *_tc){
  th_comment_init((th_comment *)_tc);
}

char *theora_comment_query(theora_comment *_tc,char *_tag,int _count){
  return th_comment_query((th_comment *)_tc,_tag,_count);
}

int theora_comment_query_count(theora_comment *_tc,char *_tag){
  return th_comment_query_count((th_comment *)_tc,_tag);
}

void theora_comment_clear(theora_comment *_tc){
  th_comment_clear((th_comment *)_tc);
}

void theora_comment_add(theora_comment *_tc,char *_comment){
  th_comment_add((th_comment *)_tc,_comment);
}

void theora_comment_add_tag(theora_comment *_tc, char *_tag, char *_value){
  th_comment_add_tag((th_comment *)_tc,_tag,_value);
}
