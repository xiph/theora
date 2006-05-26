/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2005                *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

  function:
  last mod: $Id: toplevel.c,v 1.39 2004/03/18 02:00:30 giles Exp $

 ********************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include "theora/theora.h"
#include "toplevel.h"
#include "dsp.h"

static int _ilog(unsigned int v){
  int ret=0;
  while(v){
    ret++;
    v>>=1;
  }
  return(ret);
}

const char *theora_version_string(void){
  return VENDOR_STRING;
}

ogg_uint32_t theora_version_number(void){
  return (VERSION_MAJOR<<16) + (VERSION_MINOR<<8) + (VERSION_SUB);
}


static void _tp_readbuffer(oggpack_buffer *opb, char *buf, const long len)
{
  long i;
  long ret;

  for (i = 0; i < len; i++) {
    theora_read(opb, 8, &ret);
    *buf++=(char)ret;
  }
}

static void _tp_readlsbint(oggpack_buffer *opb, long *value)
{
  int i;
  long ret[4];

  for (i = 0; i < 4; i++) {
    theora_read(opb,8,&ret[i]);
  }
  *value = ret[0]|ret[1]<<8|ret[2]<<16|ret[3]<<24;
}

void theora_info_init(theora_info *c) {
  memset(c,0,sizeof(*c));
  c->codec_setup=_ogg_calloc(1,sizeof(codec_setup_info));
}

void theora_info_clear(theora_info *c) {
  codec_setup_info *ci=c->codec_setup;
  int i;
  if(ci){
    if(ci->qmats) _ogg_free(ci->qmats);
    for(i=0;i<6;i++)
      if(ci->range_table[i]) _ogg_free(ci->range_table[i]);
    ClearHuffmanTrees(ci->HuffRoot);
    _ogg_free(ci);
  }
  memset(c,0,sizeof(*c));
}

void theora_clear(theora_state *t){
  if(t){
    CP_INSTANCE *cpi=(CP_INSTANCE *)(t->internal_encode);
    PB_INSTANCE *pbi=(PB_INSTANCE *)(t->internal_decode);

    if (cpi) theora_encoder_clear (cpi);

    if(pbi){

      theora_info_clear(&pbi->info);
      ClearHuffmanSet(pbi);
      ClearFragmentInfo(pbi);
      ClearFrameInfo(pbi);
      ClearPBInstance(pbi);

      _ogg_free(t->internal_decode);
    }

    t->internal_encode=NULL;
    t->internal_decode=NULL;
  }
}

/********************** The toplevel: decode ***********************/

static int _theora_unpack_info(theora_info *ci, oggpack_buffer *opb){
  long ret;

  theora_read(opb,8,&ret);
  ci->version_major=(unsigned char)ret;
  theora_read(opb,8,&ret);
  ci->version_minor=(unsigned char)ret;
  theora_read(opb,8,&ret);
  ci->version_subminor=(unsigned char)ret;

  if(ci->version_major!=VERSION_MAJOR)return(OC_VERSION);
  if(ci->version_minor>VERSION_MINOR)return(OC_VERSION);

  theora_read(opb,16,&ret);
  ci->width=ret<<4;
  theora_read(opb,16,&ret);
  ci->height=ret<<4;
  theora_read(opb,24,&ret);
  ci->frame_width=ret;
  theora_read(opb,24,&ret);
  ci->frame_height=ret;
  theora_read(opb,8,&ret);
  ci->offset_x=ret;
  theora_read(opb,8,&ret);
  ci->offset_y=ret;
  /* Change offset_y to have the meaning everyone expects it to have */
  ci->offset_y = ci->height - ci->frame_height - ci->offset_y;

  theora_read(opb,32,&ret);
  ci->fps_numerator=ret;
  theora_read(opb,32,&ret);
  ci->fps_denominator=ret;
  theora_read(opb,24,&ret);
  ci->aspect_numerator=ret;
  theora_read(opb,24,&ret);
  ci->aspect_denominator=ret;

  theora_read(opb,8,&ret);
  ci->colorspace=ret;
  theora_read(opb,24,&ret);
  ci->target_bitrate=ret;
  theora_read(opb,6,&ret);
  ci->quality=ret;

  theora_read(opb,5,&ret);
  ci->keyframe_frequency_force=1<<ret;

  theora_read(opb,2,&ret);
  ci->pixelformat=ret;
  if(ci->pixelformat==OC_PF_RSVD)
    return (OC_BADHEADER);
  /* 4:2:2 and 4:4:4 not currently implemented */
  else if(ci->pixelformat != OC_PF_420)
    return (OC_IMPL);

  /* spare configuration bits */
  if ( theora_read(opb,3,&ret) == -1 )
    return (OC_BADHEADER);

  return(0);
}

static int _theora_unpack_comment(theora_comment *tc, oggpack_buffer *opb){
  int i;
  long len;

   _tp_readlsbint(opb,&len);
  if(len<0)return(OC_BADHEADER);
  tc->vendor=_ogg_calloc(1,len+1);
  _tp_readbuffer(opb,tc->vendor, len);
  tc->vendor[len]='\0';

  _tp_readlsbint(opb,(long *) &tc->comments);
  if(tc->comments<0)goto parse_err;
  tc->user_comments=_ogg_calloc(tc->comments,sizeof(*tc->user_comments));
  tc->comment_lengths=_ogg_calloc(tc->comments,sizeof(*tc->comment_lengths));
  for(i=0;i<tc->comments;i++){
    _tp_readlsbint(opb,&len);
    if(len<0)goto parse_err;
    tc->user_comments[i]=_ogg_calloc(1,len+1);
    _tp_readbuffer(opb,tc->user_comments[i],len);
    tc->user_comments[i][len]='\0';
    tc->comment_lengths[i]=len;
  }
  return(0);

parse_err:
  theora_comment_clear(tc);
  return(OC_BADHEADER);
}

static int _theora_unpack_tables(theora_info *c, oggpack_buffer *opb){
  codec_setup_info *ci;
  int ret;

  ci=(codec_setup_info *)c->codec_setup;

  ret=ReadFilterTables(ci, opb);
  if(ret)return ret;
  ret=ReadQTables(ci, opb);
  if(ret)return ret;
  ret=ReadHuffmanTrees(ci, opb);
  if(ret)return ret;

  return ret;
}

int theora_decode_header(theora_info *ci, theora_comment *cc, ogg_packet *op){
  long ret;
  oggpack_buffer *opb;
  
  if(!op)return OC_BADHEADER;
  
#ifndef LIBOGG2
  opb = _ogg_malloc(sizeof(oggpack_buffer));
  oggpackB_readinit(opb,op->packet,op->bytes);
#else
  opb = _ogg_malloc(oggpack_buffersize());
  oggpackB_readinit(opb,op->packet);
#endif
  {
    char id[6];
    int typeflag;
    
    theora_read(opb,8,&ret);
    typeflag = ret;
    if(!(typeflag&0x80)) {
      _ogg_free(opb);
      return(OC_NOTFORMAT);
    }

    _tp_readbuffer(opb,id,6);
    if(memcmp(id,"theora",6)) {
      _ogg_free(opb);
      return(OC_NOTFORMAT);
    }

    switch(typeflag){
    case 0x80:
      if(!op->b_o_s){
        /* Not the initial packet */
        _ogg_free(opb);
        return(OC_BADHEADER);
      }
      if(ci->version_major!=0){
        /* previously initialized info header */
        _ogg_free(opb);
        return OC_BADHEADER;
      }

      ret = _theora_unpack_info(ci,opb);
      _ogg_free(opb);
      return(ret);

    case 0x81:
      if(ci->version_major==0){
        /* um... we didn't get the initial header */
        _ogg_free(opb);
        return(OC_BADHEADER);
      }

      ret = _theora_unpack_comment(cc,opb);
      _ogg_free(opb);
      return(ret);

    case 0x82:
      if(ci->version_major==0 || cc->vendor==NULL){
        /* um... we didn't get the initial header or comments yet */
        _ogg_free(opb);
        return(OC_BADHEADER);
      }

      ret = _theora_unpack_tables(ci,opb);
      _ogg_free(opb);
      return(ret);
    
    default:
      _ogg_free(opb);
      if(ci->version_major==0 || cc->vendor==NULL || 
         ((codec_setup_info *)ci->codec_setup)->HuffRoot[0]==NULL){
        /* we haven't gotten the three required headers */
        return(OC_BADHEADER);
      }
      /* ignore any trailing header packets for forward compatibility */
      return(OC_NEWPACKET);
    }
  }
  /* I don't think it's possible to get this far, but better safe.. */
  _ogg_free(opb);
  return(OC_BADHEADER);
}

int theora_decode_init(theora_state *th, theora_info *c){
  PB_INSTANCE *pbi;
  codec_setup_info *ci;

  ci=(codec_setup_info *)c->codec_setup;
  memset(th, 0, sizeof(*th));
  th->internal_decode=pbi=_ogg_calloc(1,sizeof(*pbi));
  th->internal_encode=NULL;

  InitPBInstance(pbi);

  dsp_static_init (&pbi->dsp);

  memcpy(&pbi->info,c,sizeof(*c));
  pbi->info.codec_setup=NULL;
  th->i=&pbi->info;
  th->granulepos=-1;

  InitFrameDetails(pbi);

  pbi->keyframe_granule_shift=_ilog(c->keyframe_frequency_force-1);

  pbi->LastFrameQualityValue = 0;
  pbi->DecoderErrorCode = 0;

  /* Clear down the YUVtoRGB conversion skipped list. */
  memset(pbi->skipped_display_fragments, 0, pbi->UnitFragments );

  /* Initialise quantiser and in-loop filter */
  CopyQTables(pbi, ci);
  CopyFilterTables(pbi, ci);

  /* Huffman setup */
  InitHuffmanTrees(pbi, ci);

  return(0);

}

int theora_decode_packetin(theora_state *th,ogg_packet *op){
  long ret;
  PB_INSTANCE *pbi=(PB_INSTANCE *)(th->internal_decode);

  pbi->DecoderErrorCode = 0;

#ifndef LIBOGG2
  oggpackB_readinit(pbi->opb,op->packet,op->bytes);
#else
  oggpackB_readinit(pbi->opb,op->packet);
#endif

  if(op->bytes!=0){
    /* verify that this is a video frame */
    theora_read(pbi->opb,1,&ret);

    if (ret==0) {
      ret=LoadAndDecode(pbi);

      if(ret)return ret;

      if(pbi->PostProcessingLevel)
        PostProcess(pbi);

      if(op->granulepos>-1)
        th->granulepos=op->granulepos;
      else{
        if(th->granulepos==-1){
          th->granulepos=0;
        }else{
          if(pbi->FrameType==KEY_FRAME){
            long frames= th->granulepos & ((1<<pbi->keyframe_granule_shift)-1);
            th->granulepos>>=pbi->keyframe_granule_shift;
            th->granulepos+=frames+1;
            th->granulepos<<=pbi->keyframe_granule_shift;
          }else
            th->granulepos++;
        }
      }

      return(0);
    }
  }else{
    th->granulepos++;
    return(OC_DUPFRAME);
  }

  return OC_BADPACKET;
}

int theora_decode_YUVout(theora_state *th,yuv_buffer *yuv){
  PB_INSTANCE *pbi=(PB_INSTANCE *)(th->internal_decode);

  yuv->y_width = pbi->info.width;
  yuv->y_height = pbi->info.height;
  yuv->y_stride = pbi->YStride;

  yuv->uv_width = pbi->info.width / 2;
  yuv->uv_height = pbi->info.height / 2;
  yuv->uv_stride = pbi->UVStride;

  if(pbi->PostProcessingLevel){
    yuv->y = &pbi->PostProcessBuffer[pbi->ReconYDataOffset];
    yuv->u = &pbi->PostProcessBuffer[pbi->ReconUDataOffset];
    yuv->v = &pbi->PostProcessBuffer[pbi->ReconVDataOffset];
  }else{
    yuv->y = &pbi->LastFrameRecon[pbi->ReconYDataOffset];
    yuv->u = &pbi->LastFrameRecon[pbi->ReconUDataOffset];
    yuv->v = &pbi->LastFrameRecon[pbi->ReconVDataOffset];
  }
  
  /* we must flip the internal representation,
     so make the stride negative and start at the end */
  yuv->y += yuv->y_stride * (yuv->y_height - 1);
  yuv->u += yuv->uv_stride * (yuv->uv_height - 1);
  yuv->v += yuv->uv_stride * (yuv->uv_height - 1);
  yuv->y_stride = - yuv->y_stride;
  yuv->uv_stride = - yuv->uv_stride;

  return 0;
}

/* returns, in seconds, absolute time of current packet in given
   logical stream */
double theora_granule_time(theora_state *th,ogg_int64_t granulepos){
#ifndef THEORA_DISABLE_FLOAT
  CP_INSTANCE *cpi=(CP_INSTANCE *)(th->internal_encode);
  PB_INSTANCE *pbi=(PB_INSTANCE *)(th->internal_decode);

  if(cpi)pbi=&cpi->pb;

  if(granulepos>=0){
    ogg_int64_t iframe=granulepos>>pbi->keyframe_granule_shift;
    ogg_int64_t pframe=granulepos-(iframe<<pbi->keyframe_granule_shift);

    return (iframe+pframe)*
      ((double)pbi->info.fps_denominator/pbi->info.fps_numerator);

  }
#endif

  return(-1); /* negative granulepos or float calculations disabled */
}

/* check for header flag */
int theora_packet_isheader(ogg_packet *op)
{
  return (op->packet[0] & 0x80) ? 1 : 0;
}

/* check for keyframe */
int theora_packet_iskeyframe(ogg_packet *op)
{
  if (op->packet[0] & 0x80) return -1; /* not a data packet */
  return (op->packet[0] & 0x40) ? 0 : 1; /* inter or intra */
}

/* returns the shift radix used to split the granulepos into two fields */
int theora_granule_shift(theora_info *ti)
{
  return _ilog(ti->keyframe_frequency_force - 1);
}

/* returns frame number of current packet in given logical stream */
ogg_int64_t theora_granule_frame(theora_state *th,ogg_int64_t granulepos){
  CP_INSTANCE *cpi=(CP_INSTANCE *)(th->internal_encode);
  PB_INSTANCE *pbi=(PB_INSTANCE *)(th->internal_decode);

  if(cpi)pbi=&cpi->pb;

  if(granulepos>=0){
    ogg_int64_t iframe=granulepos>>pbi->keyframe_granule_shift;
    ogg_int64_t pframe=granulepos-(iframe<<pbi->keyframe_granule_shift);

    return (iframe+pframe);
  }

  return(-1);
}
