#include <string.h>
#include "theora/theoraenc.h"
#include "theora/theora.h"
#include "codec_internal.h"
#include "../dec/ocintrin.h"

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
  _ci->quick_p=1;
}

static int _ilog(unsigned _v){
  int ret;
  for(ret=0;_v;ret++)_v>>=1;
  return ret;
}



struct th_enc_ctx{
  /*This is required at the start of the struct for the common functions to
     work.*/
  th_info        info;
  /*The actual encoder.*/
  theora_state   state;
  /*A temporary buffer for input frames.
    This is needed if the U and V strides differ, or padding is required.*/
  unsigned char *buf;
};


th_enc_ctx *th_encode_alloc(const th_info *_info){
  theora_info  ci;
  th_enc_ctx  *enc;
  th_info2theora_info(&ci,_info);
  /*Do a bunch of checks the new API does, but the old one didn't.*/
  if((_info->frame_width&0xF)||(_info->frame_height&0xF)||
   _info->frame_width>=0x100000||_info->frame_height>=0x100000||
   _info->pic_x+_info->pic_width>_info->frame_width||
   _info->pic_y+_info->pic_height>_info->frame_height||
   _info->pic_x>255||
   _info->frame_height-_info->pic_height-_info->pic_y>255||
   _info->colorspace<0||_info->colorspace>=TH_CS_NSPACES||
   _info->pixel_fmt<0||_info->pixel_fmt>=TH_PF_NFORMATS){
    enc=NULL;
  }
  else{
    enc=(th_enc_ctx *)_ogg_malloc(sizeof(*enc));
    if(theora_encode_init(&enc->state,&ci)<0){
      _ogg_free(enc);
      enc=NULL;
    }
    else{
      if(_info->frame_width>_info->pic_width||
       _info->frame_height>_info->pic_height){
        enc->buf=_ogg_malloc((_info->frame_width*_info->frame_height+
         ((_info->frame_width>>!(_info->pixel_fmt&1))*
         (_info->frame_height>>!(_info->pixel_fmt&2))<<1))*sizeof(*enc->buf));
      }
      else enc->buf=NULL;
      memcpy(&enc->info,_info,sizeof(enc->info));
      /*Overwrite values theora_encode_init() can change; don't trust the user.*/
      enc->info.version_major=ci.version_major;
      enc->info.version_minor=ci.version_minor;
      enc->info.version_subminor=ci.version_subminor;
      enc->info.quality=ci.quality;
      enc->info.target_bitrate=ci.target_bitrate;
      enc->info.fps_numerator=ci.fps_numerator;
      enc->info.fps_denominator=ci.fps_denominator;
      enc->info.keyframe_granule_shift=_ilog(ci.keyframe_frequency_force-1);
    }
  }
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

/*Copies the picture region of the _src image plane into _dst and pads the rest
   of _dst using a diffusion extension method.
  We could do much better (e.g., the DCT-based low frequency extension method
   in theora-exp's fdct.c) if we were to pad after motion compensation, but
   that would require significant changes to the encoder.*/
static unsigned char *th_encode_copy_pad_plane(th_img_plane *_dst,
 unsigned char *_buf,th_img_plane *_src,
 ogg_uint32_t _pic_x,ogg_uint32_t _pic_y,
 ogg_uint32_t _pic_width,ogg_uint32_t _pic_height){
  size_t buf_sz;
  _dst->width=_src->width;
  _dst->height=_src->height;
  _dst->stride=_src->width;
  _dst->data=_buf;
  buf_sz=_dst->width*_dst->height*sizeof(*_dst->data);
  /*If we have _no_ data, just encode a dull green.*/
  if(_pic_width==0||_pic_height==0)memset(_dst->data,0,buf_sz);
  else{
    unsigned char *dst;
    unsigned char *src;
    ogg_uint32_t   x;
    ogg_uint32_t   y;
    int            dstride;
    int            sstride;
    /*Step 1: Copy the data we do have.*/
    dstride=_dst->stride;
    sstride=_src->stride;
    dst=_dst->data+_pic_y*dstride+_pic_x;
    src=_src->data+_pic_y*sstride+_pic_x;
    for(y=0;y<_pic_height;y++){
      memcpy(dst,src,_pic_width);
      dst+=dstride;
      src+=sstride;
    }
    /*Step 2: Copy the border into any blocks that are 100% padding.
      There's probably smarter things we could do than this.*/
    /*Left side.*/
    for(x=_pic_x;x-->0;){
      dst=_dst->data+_pic_y*dstride+x;
      for(y=0;y<_pic_height;y++){
        dst[0]=(dst[1]<<1)+(dst-(dstride&-(y>0)))[1]+
         (dst+(dstride&-(y+1<_pic_height)))[1]+2>>2;
        dst+=dstride;
      }
    }
    /*Right side.*/
    for(x=_pic_x+_pic_width;x<_dst->width;x++){
      dst=_dst->data+_pic_y*dstride+x-1;
      for(y=0;y<_pic_height;y++){
        dst[1]=(dst[0]<<1)+(dst-(dstride&-(y>0)))[0]+
         (dst+(dstride&-(y+1<_pic_height)))[0]+2>>2;
        dst+=dstride;
      }
    }
    /*Top.*/
    dst=_dst->data+_pic_y*dstride;
    for(y=_pic_y;y-->0;){
      for(x=0;x<_dst->width;x++){
        (dst-dstride)[x]=(dst[x]<<1)+dst[x-(x>0)]+dst[x+(x+1<_dst->width)]+2>>2;
      }
      dst-=dstride;
    }
    /*Bottom.*/
    dst=_dst->data+(_pic_y+_pic_height)*dstride;
    for(y=_pic_y+_pic_height;y<_dst->height;y++){
      for(x=0;x<_dst->width;x++){
        dst[x]=((dst-dstride)[x]<<1)+(dst-dstride)[x-(x>0)]+
         (dst-dstride)[x+(x+1<_dst->width)]+2>>2;
      }
      dst+=dstride;
    }
  }
  _buf+=buf_sz;
  return _buf;
}

int th_encode_ycbcr_in(th_enc_ctx *_enc,th_ycbcr_buffer _ycbcr){
  CP_INSTANCE     *cpi;
  theora_state    *te;
  th_img_plane    *pycbcr;
  th_ycbcr_buffer  ycbcr;
  yuv_buffer       yuv;
  ogg_uint32_t     pic_width;
  ogg_uint32_t     pic_height;
  int              hdec;
  int              vdec;
  int              ret;
  if(_enc==NULL||_ycbcr==NULL)return OC_FAULT;
  te=&_enc->state;
  /*theora_encode_YUVin() does not bother to check uv_width and uv_height, and
     then uses them.
    This is arguably okay (it will most likely lead to a crash if they're
     wrong, which will make the developer who passed them fix the problem), but
     our API promises to return an error code instead.*/
  cpi=(CP_INSTANCE *)te->internal_encode;
  hdec=!(cpi->pb.info.pixelformat&1);
  vdec=!(cpi->pb.info.pixelformat&2);
  if(_ycbcr[0].width!=cpi->pb.info.width||
   _ycbcr[0].height!=cpi->pb.info.height||
   _ycbcr[1].width!=_ycbcr[0].width>>hdec||
   _ycbcr[1].height!=_ycbcr[0].height>>vdec||
   _ycbcr[2].width!=_ycbcr[1].width||_ycbcr[2].height!=_ycbcr[1].height){
    return OC_EINVAL;
  }
  pic_width=cpi->pb.info.frame_width;
  pic_height=cpi->pb.info.frame_height;
  /*We can only directly use the input buffer if no padding is required (since
     the new API is documented not to use values outside the picture region)
     and if the strides for the Cb and Cr planes are the same, since the old
     API had no way to specify different ones.*/
  if(_ycbcr[0].width==pic_width&&_ycbcr[0].height==pic_height&&
   _ycbcr[1].stride==_ycbcr[2].stride){
    pycbcr=_ycbcr;
  }
  else{
    unsigned char *buf;
    int            pic_x;
    int            pic_y;
    int            pli;
    pic_x=cpi->pb.info.offset_x;
    pic_y=cpi->pb.info.offset_y;
    if(_ycbcr[0].width>pic_width&&_ycbcr[0].height>pic_height){
      buf=th_encode_copy_pad_plane(ycbcr+0,_enc->buf,_ycbcr+0,
       pic_x,pic_y,pic_width,pic_height);
    }
    else{
      /*If only the strides differ, we can still avoid copying the luma plane.*/
      memcpy(ycbcr+0,_ycbcr+0,sizeof(ycbcr[0]));
      if(_enc->buf==NULL){
        _enc->buf=(unsigned char *)_ogg_malloc(
         (_ycbcr[1].width*_ycbcr[1].height<<1)*sizeof(*_enc->buf));
      }
      buf=_enc->buf;
    }
    for(pli=1;pli<3;pli++){
      int x0;
      int y0;
      x0=pic_x>>hdec;
      y0=pic_x>>hdec;
      buf=th_encode_copy_pad_plane(ycbcr+pli,buf,_ycbcr+pli,
       x0,y0,(pic_x+pic_width+hdec>>hdec)-x0,(pic_y+pic_height+vdec>>vdec)-y0);
    }
    pycbcr=ycbcr;
  }
  yuv.y_width=pycbcr[0].width;
  yuv.y_height=pycbcr[0].height;
  yuv.uv_width=pycbcr[1].width;
  yuv.uv_height=pycbcr[1].height;
  yuv.y_stride=pycbcr[0].stride;
  yuv.y=pycbcr[0].data;
  yuv.uv_stride=pycbcr[1].stride;
  yuv.u=pycbcr[1].data;
  yuv.v=pycbcr[2].data;
  ret=theora_encode_YUVin(te,&yuv);
  return ret;
}

int th_encode_packetout(th_enc_ctx *_enc,int _last,ogg_packet *_op){
  if(_enc==NULL)return OC_FAULT;
  return theora_encode_packetout(&_enc->state,_last,_op);
}

void th_encode_free(th_enc_ctx *_enc){
  if(_enc!=NULL){
    theora_clear(&_enc->state);
    _ogg_free(_enc->buf);
    _ogg_free(_enc);
  }
}
