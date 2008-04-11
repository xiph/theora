#include <string.h>
#include "theora/theoraenc.h"
#include "theora/theora.h"
#include "codec_internal.h"

/*Wrapper to translate the new API into the old API.
  Eventually we need to convert the old functions to support the new API
   natively and do the translation the other way.
  theora-exp already the necessary code to do so.*/

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
  _ci->codec_setup=NULL;
  /*Defaults from old encoder_example... eventually most of these should go
     away when we make the encoder no longer use them.*/
  _ci->dropframes_p=0;
  _ci->keyframe_auto_p=1;
  _ci->keyframe_frequency=1<<_info->keyframe_granule_shift;
  _ci->keyframe_frequency_force=1<<_info->keyframe_granule_shift;
  _ci->keyframe_data_target_bitrate=
   _info->target_bitrate+(_info->target_bitrate>>1);
  _ci->keyframe_auto_threshold=80;
  _ci->keyframe_mindistance=8;
  _ci->noise_sensitivity=1;
  _ci->sharpness=0;
}

typedef struct th_enc_ctx{
  /*This is required at the start of the struct for the common functions to
     work.*/
  th_info      info;
  /*The actual encoder.*/
  theora_state state;
};


th_enc_ctx *th_encode_alloc(const th_info *_info){
  theora_info  ci;
  th_enc_ctx  *enc;
  th_info2theora_info(&ci,_info);
  enc=(th_enc_ctx *)_ogg_malloc(sizeof(*enc));
  if(theora_encode_init(&enc->state,&ci)<0){
    _ogg_free(enc);
    enc=NULL;
  }
  else memcpy(&enc->info,_info,sizeof(enc->info));
  return enc;
}

int th_encode_ctl(th_enc_ctx *_enc,int _req,void *_buf,size_t _buf_sz){
  return theora_control(&_enc->state,_req,_buf,_buf_sz);
}

int th_encode_flushheader(th_enc_ctx *_enc,th_comment *_comments,
 ogg_packet *_op){
  theora_state *te;
  CP_INSTANCE  *cpi;
  if(_enc==NULL||_op==NULL)return OC_FAULT;
  te=&_enc->state;
  cpi=(CP_INSTANCE *)te->internal_encode;
  switch(cpi->doneflag){
    case -3:{
      theora_encode_header(te,_op);
      return -cpi->doneflag++;
    }break;
    case -2:{
      if(_comments==NULL)return OC_FAULT;
      theora_encode_comment((theora_comment *)_comments,_op);
      /*The old API does not require a theora_state struct when writing the
         comment header, so it can't use its internal buffer and relies on the
         application to free it.
        The old documentation is wrong on this subject, and this breaks on
         Windows when linking against multiple versions of libc (which is
         almost always done when, e.g., using DLLs built with mingw32).
        The new API _does_ require a th_enc_ctx, and states that libtheora owns
         the memory.
        Thus we move the contents of this packet into our internal
         oggpack_buffer so it can be properly reclaimed.*/
      oggpackB_reset(cpi->oggbuffer);
      oggpackB_writecopy(cpi->oggbuffer,_op->packet,_op->bytes*8);
      _ogg_free(_op->packet);
      _op->packet=oggpackB_get_buffer(cpi->oggbuffer);
      return -cpi->doneflag++;
    }break;
    case -1:{
      theora_encode_tables(te,_op);
      return -cpi->doneflag++;
    }break;
    case 0:return 0;
    default:return OC_EINVAL;
  }
}

int th_encode_ycbcr_in(th_enc_ctx *_enc,th_ycbcr_buffer _ycbcr){
  CP_INSTANCE   *cpi;
  theora_state  *te;
  yuv_buffer     yuv;
  unsigned char *tmpbuf;
  int            ret;
  if(_enc==NULL||_ycbcr==NULL)return OC_FAULT;
  te=&_enc->state;
  /*theora_encode_YUVin() does not bother to check uv_width and uv_height, and
     then uses them.
    This is arguably okay (it will most likely lead to a crash if they're
     wrong, which will make the developer who passed them fix the problem), but
     our API promises to return an error code instead.*/
  cpi=(CP_INSTANCE *)te->internal_encode;
  if(_ycbcr[1].width!=_ycbcr[0].width>>!(cpi->pb.info.pixelformat&1)||
   _ycbcr[1].height!=_ycbcr[0].height>>!(cpi->pb.info.pixelformat&2)||
   _ycbcr[2].width!=_ycbcr[1].width||_ycbcr[2].height!=_ycbcr[1].height){
    return OC_EINVAL;
  }
  yuv.y_width=_ycbcr[0].width;
  yuv.y_height=_ycbcr[0].height;
  yuv.y_stride=_ycbcr[0].stride;
  yuv.y=_ycbcr[0].data;
  yuv.uv_width=_ycbcr[1].width;
  yuv.uv_height=_ycbcr[1].height;
  if(_ycbcr[1].stride==_ycbcr[2].stride){
    yuv.uv_stride=_ycbcr[1].stride;
    yuv.u=_ycbcr[1].data;
    yuv.v=_ycbcr[2].data;
    tmpbuf=NULL;
  }
  else{
    unsigned char *src;
    unsigned char *dst;
    int            i;
    /*There's no way to signal different strides for the u and v components
       when we pass them to theora_encode_YUVin().
      Therefore we have to allocate a temporary buffer and copy them.*/
    tmpbuf=(unsigned char *)_ogg_malloc(
     (yuv.uv_width*yuv.uv_height<<1)*sizeof(*tmpbuf));
    dst=tmpbuf;
    yuv.u=dst;
    src=_ycbcr[1].data;
    for(i=0;i<yuv.uv_height;i++){
      memcpy(dst,src,yuv.uv_width);
      dst+=yuv.uv_width;
      src+=_ycbcr[1].stride;
    }
    yuv.v=dst;
    src=_ycbcr[2].data;
    for(i=0;i<yuv.uv_height;i++){
      memcpy(dst,src,yuv.uv_width);
      dst+=yuv.uv_width;
      src+=_ycbcr[2].stride;
    }
    yuv.uv_stride=yuv.uv_width;
  }
  ret=theora_encode_YUVin(te,&yuv);
  _ogg_free(tmpbuf);
  return ret;
}

int th_encode_packetout(th_enc_ctx *_enc,int _last,ogg_packet *_op){
  if(_enc==NULL)return OC_FAULT;
  return theora_encode_packetout(&_enc->state,_last,_op);
}

void th_encode_free(th_enc_ctx *_enc){
  if(_enc!=NULL){
    theora_clear(&_enc->state);
    _ogg_free(_enc);
  }
}
