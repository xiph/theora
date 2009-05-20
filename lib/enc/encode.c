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
#include "codec_internal.h"
#include "encoder_lookup.h"

static int predict_frag(int wpc,
                        ogg_int16_t *dc,
                        ogg_int16_t *down,
                        int *last){

  if(wpc){
    ogg_int16_t DC = 0;

    if(wpc&0x1) DC += pc[wpc][0]* *(dc-1);
    if(wpc&0x2) DC += pc[wpc][1]* *(down-1);
    if(wpc&0x4) DC += pc[wpc][2]* *(down);
    if(wpc&0x8) DC += pc[wpc][3]* *(down+1);

    /* if we need to do a shift */
    if(pc[wpc][4]) {
      /* If negative add in the negative correction factor */
      DC += (HIGHBITDUPPED(DC) & pc[wpc][5]);
      /* Shift in lieu of a divide */
      DC >>= pc[wpc][4];
    }

    /* check for outranging on the two predictors that can outrange */
    if((wpc&(PU|PUL|PL)) == (PU|PUL|PL)){
      if( abs(DC - *down) > 128) {
        DC = *down;
      } else if( abs(DC - *(dc-1)) > 128) {
        DC = *(dc-1);
      } else if( abs(DC - *(down-1)) > 128) {
        DC = *(down-1);
      }
    }

    *last = *dc;
    return *dc - DC;
  }else{
    int ret = *dc - *last;
    *last = *dc;
    return ret;
  }
}

static void PredictDC(CP_INSTANCE *cpi){
  ogg_int32_t pi;
  int last[3];  /* last value used for given frame */
  int y,x,fi = 0;
  unsigned char *cp = cpi->frag_coded;

  /* for y,u,v; handles arbitrary plane subsampling arrangement.  Shouldn't need to be altered for 4:2:2 or 4:4:4 */
  for (pi=0; pi<3; pi++) {
    int v = cpi->frag_v[pi];
    int h = cpi->frag_h[pi];
    int subh = !(pi && cpi->info.pixelformat != OC_PF_444);
    int subv = !(pi && cpi->info.pixelformat == OC_PF_420);
    ogg_int16_t *dc;
    ogg_int16_t *down;
    dc=cpi->frag_dc_tmp;
    down=cpi->frag_dc_tmp+h;

    for(x=0;x<3;x++)last[x]=0;

    for (y=0; y<v ; y++) {
      macroblock_t *mb_row = cpi->macro + (y>>subv)*cpi->macro_h;
      macroblock_t *mb_down = cpi->macro + ((y-1)>>subv)*cpi->macro_h;

      memcpy(down,dc,h*sizeof(*down));
      memcpy(dc,cpi->frag_dc+fi,h*sizeof(*dc));

      for (x=0; x<h; x++, fi++) {
        if(cp[fi]) {
          int wpc=0;
          int wf = Mode2Frame[mb_row[x>>subh].mode];

          if(x>0){
            if(cp[fi-1] && Mode2Frame[mb_row[(x-1)>>subh].mode] == wf) wpc|=1; /* left */
            if(y>0 && cp[fi-h-1] && Mode2Frame[mb_down[(x-1)>>subh].mode] == wf) wpc|=2; /* down left */
          }
          if(y>0){
            if(cp[fi-h] && Mode2Frame[mb_down[x>>subh].mode] == wf) wpc|=4; /* down */
            if(x+1<h && cp[fi-h+1] && Mode2Frame[mb_down[(x+1)>>subh].mode] == wf) wpc|=8; /* down right */
          }
          cpi->frag_dc[fi]=predict_frag(wpc,dc+x,down+x,last+wf);
        }
      }
    }
  }
}

static void ChooseTokenTables (CP_INSTANCE *cpi) {
  int interp = (cpi->FrameType!=KEY_FRAME);
  int i,plane;
  int best;

  for(plane = 0; plane<2; plane++){

    /* Work out which table options are best for DC */
    best = cpi->dc_bits[plane][0];
    cpi->huffchoice[interp][0][plane] = DC_HUFF_OFFSET;
    for ( i = 1; i < DC_HUFF_CHOICES; i++ ) {
      if ( cpi->dc_bits[plane][i] < best ) {
        best = cpi->dc_bits[plane][i];
        cpi->huffchoice[interp][0][plane] = i + DC_HUFF_OFFSET;
      }
    }

    /* Work out which table options are best for AC */
    best = cpi->ac1_bits[plane][0]+cpi->acN_bits[plane][0];
    cpi->huffchoice[interp][1][plane] = AC_HUFF_OFFSET;
    for ( i = 1; i < AC_HUFF_CHOICES; i++ ) {
      int test = cpi->ac1_bits[plane][i] + cpi->acN_bits[plane][i];
      if ( test < best ){
        best = test;
        cpi->huffchoice[interp][1][plane] = i + AC_HUFF_OFFSET;
      }
    }
  }
}

static void EncodeTokenGroup(CP_INSTANCE *cpi,
                             int group,
                             int huffY,
                             int huffC){

  int i;
  oggpack_buffer *opb=cpi->oggbuffer;
  unsigned char *token = cpi->dct_token[group];
  ogg_uint16_t *eb = cpi->dct_token_eb[group];

  for(i=0; i<cpi->dct_token_ycount[group]; i++){
    if(token[i] < DCT_NOOP){
      oggpackB_write(opb,cpi->huff_codes[huffY][token[i]].pattern,
       cpi->huff_codes[huffY][token[i]].nbits);
      if(OC_DCT_TOKEN_EXTRA_BITS[token[i]]>0){
        oggpackB_write(opb,eb[i],OC_DCT_TOKEN_EXTRA_BITS[token[i]]);
      }
    }
  }

  for(; i<cpi->dct_token_count[group]; i++){
    if(token[i] < DCT_NOOP){
      oggpackB_write(opb,cpi->huff_codes[huffC][token[i]].pattern,
       cpi->huff_codes[huffC][token[i]].nbits);
      if (OC_DCT_TOKEN_EXTRA_BITS[token[i]] > 0)
        oggpackB_write( opb, eb[i], OC_DCT_TOKEN_EXTRA_BITS[token[i]] );
    }
  }
}

static long EncodeTokenList (CP_INSTANCE *cpi) {
  int i;
  int interp = (cpi->FrameType!=KEY_FRAME);
  oggpack_buffer *opb=cpi->oggbuffer;
  long bits0,bits1;

  /* DC tokens aren't special, they just come first */
  oggpackB_write( opb, cpi->huffchoice[interp][0][0] - DC_HUFF_OFFSET, DC_HUFF_CHOICE_BITS );
  oggpackB_write( opb, cpi->huffchoice[interp][0][1] - DC_HUFF_OFFSET, DC_HUFF_CHOICE_BITS );

  bits0 = oggpackB_bits(opb);
  EncodeTokenGroup(cpi, 0,  cpi->huffchoice[interp][0][0], cpi->huffchoice[interp][0][1]);
  bits0 = oggpackB_bits(opb)-bits0;

  /* AC tokens */
  oggpackB_write( opb, cpi->huffchoice[interp][1][0] - AC_HUFF_OFFSET, AC_HUFF_CHOICE_BITS );
  oggpackB_write( opb, cpi->huffchoice[interp][1][1] - AC_HUFF_OFFSET, AC_HUFF_CHOICE_BITS );

  bits1 = oggpackB_bits(opb);
  for(i=1;i<=AC_TABLE_2_THRESH;i++)
    EncodeTokenGroup(cpi, i,  cpi->huffchoice[interp][1][0],
                     cpi->huffchoice[interp][1][1]);

  for(;i<=AC_TABLE_3_THRESH;i++)
    EncodeTokenGroup(cpi, i,  cpi->huffchoice[interp][1][0]+AC_HUFF_CHOICES,
                     cpi->huffchoice[interp][1][1]+AC_HUFF_CHOICES);

  for(;i<=AC_TABLE_4_THRESH;i++)
    EncodeTokenGroup(cpi, i,  cpi->huffchoice[interp][1][0]+AC_HUFF_CHOICES*2,
                     cpi->huffchoice[interp][1][1]+AC_HUFF_CHOICES*2);

  for(;i<BLOCK_SIZE;i++)
    EncodeTokenGroup(cpi, i,  cpi->huffchoice[interp][1][0]+AC_HUFF_CHOICES*3,
                     cpi->huffchoice[interp][1][1]+AC_HUFF_CHOICES*3);
  bits1 = oggpackB_bits(opb)-bits1;

  return bits1;
}

static const unsigned char NoOpModeWords[8] = {0,1,2,3,4,5,6,7};
static const unsigned char NoOpModeBits[8] = {3,3,3,3,3,3,3,3};
static const unsigned char NoOpScheme[8] = {0,1,2,3,4,5,6,7};

static void PackModes (CP_INSTANCE *cpi) {
  ogg_uint32_t    j;
  ogg_uint32_t    BestScheme = cpi->chooser.scheme_list[0];

  const unsigned char *ModeWords;
  const unsigned char *ModeBits;
  const unsigned char  *ModeScheme;
  int SB,MB;

  oggpack_buffer *opb=cpi->oggbuffer;

  /* Encode the best scheme. */
  oggpackB_write( opb, BestScheme, (ogg_uint32_t)MODE_METHOD_BITS );

  /* If the chosen scheme is scheme 0 send details of the mode
     frequency order */
  if ( BestScheme == 0 ) {
    for ( j = 0; j < MAX_MODES; j++ ){
      /* Note that the last two entries are implicit */
      oggpackB_write( opb, cpi->chooser.scheme0_ranks[j], (ogg_uint32_t)MODE_BITS );
    }
    ModeScheme = cpi->chooser.scheme0_ranks;
    ModeWords = ModeBitPatterns;
    ModeBits = ModeBitLengths;
  }
  else if ( BestScheme < (MODE_METHODS - 1)) {
    ModeScheme = ModeSchemes[BestScheme-1];
    ModeWords = ModeBitPatterns;
    ModeBits = ModeBitLengths;
  }else{
    ModeScheme = NoOpScheme;
    ModeWords = NoOpModeWords;
    ModeBits = NoOpModeBits;
  }

  /* modes coded in hilbert order; use superblock addressing */
  for ( SB=0 ; SB < cpi->super_n[0]; SB++ ){
    superblock_t *sp = &cpi->super[0][SB];
    for ( MB=0; MB<4; MB++ ) {
      macroblock_t *mbp = &cpi->macro[sp->m[MB]];
      if(mbp->coded){
        /* Add the appropriate mode entropy token. */
        int index = ModeScheme[mbp->mode];
        oggpackB_write( opb, ModeWords[index],
                        (ogg_uint32_t)ModeBits[index] );
      }
    }
  }
}

static void PackMotionVectors (CP_INSTANCE *cpi) {
  const ogg_uint32_t * MvPatternPtr;
  const ogg_uint32_t * MvBitsPtr;

  ogg_uint32_t SB, MB, B;
  oggpack_buffer *opb=cpi->oggbuffer;

  /* Choose the coding method */
  if ( cpi->MVBits_0 < cpi->MVBits_1 ) {
    oggpackB_write( opb, 0, 1 );
    MvBitsPtr = &MvBits[MAX_MV_EXTENT];
    MvPatternPtr = &MvPattern[MAX_MV_EXTENT];
  }else{
    oggpackB_write( opb, 1, 1 );
    MvBitsPtr = &MvBits2[MAX_MV_EXTENT];
    MvPatternPtr = &MvPattern2[MAX_MV_EXTENT];
  }

  /* Pack and encode the motion vectors */
  /* MBs are iterated in Hilbert scan order, but the MVs within the MB are coded in raster order */

  for ( SB=0 ; SB < cpi->super_n[0]; SB++ ){
    superblock_t *sp = &cpi->super[0][SB];
    for ( MB=0; MB<4; MB++ ) {
      macroblock_t *mbp = &cpi->macro[sp->m[MB]];
      if(!mbp->coded) continue;

      if(mbp->mode==CODE_INTER_PLUS_MV || mbp->mode==CODE_GOLDEN_MV){
        /* One MV for the macroblock */
        for(B=0; B<4; B++ ){
          if(mbp->coded & (1<<B)){
            oggpackB_write( opb, MvPatternPtr[mbp->mv[B][0]], MvBitsPtr[mbp->mv[B][0]] );
            oggpackB_write( opb, MvPatternPtr[mbp->mv[B][1]], MvBitsPtr[mbp->mv[B][1]] );
            break;
          }
        }

      }else if (mbp->mode == CODE_INTER_FOURMV){
        /* MV for each codedblock */
        for(B=0; B<4; B++ ){
          if(mbp->coded & (1<<B)){
            oggpackB_write( opb, MvPatternPtr[mbp->mv[B][0]], MvBitsPtr[mbp->mv[B][0]] );
            oggpackB_write( opb, MvPatternPtr[mbp->mv[B][1]], MvBitsPtr[mbp->mv[B][1]] );
          }
        }
      }
    }
  }
}

#include <stdio.h>
void EncodeData(CP_INSTANCE *cpi){
  long modebits=0;
  long mvbits=0;
  long dctbits;
  long bits;

  PredictDC(cpi);
  dct_tokenize_finish(cpi);

  /* Mode and MV data not needed for key frames. */
  if ( cpi->FrameType != KEY_FRAME ){
    int prebits = oggpackB_bits(cpi->oggbuffer);
    PackModes(cpi);
    modebits = oggpackB_bits(cpi->oggbuffer)-prebits;
    prebits = oggpackB_bits(cpi->oggbuffer);
    PackMotionVectors (cpi);
    mvbits = oggpackB_bits(cpi->oggbuffer)-prebits;
  }
  ChooseTokenTables(cpi);
  {
    int prebits = oggpackB_bits(cpi->oggbuffer);
    EncodeTokenList(cpi);
    dctbits = oggpackB_bits(cpi->oggbuffer)-prebits;
  }
  bits = oggpackB_bits(cpi->oggbuffer);
  ReconRefFrames(cpi);

#if defined(OC_COLLECT_METRICS)
  ModeMetrics(cpi);

#if 0
  {
    int total = cpi->frag_total*64;
    int fi=0,pi,x,y;
    ogg_int64_t ssd=0;
    double minimize;

    for(pi=0;pi<3;pi++){
      int bi = cpi->frag_buffer_index[fi];
      unsigned char *frame = cpi->frame+bi;
      unsigned char *recon = cpi->lastrecon+bi;
      int stride = cpi->stride[pi];
      int h = cpi->frag_h[pi]*8;
      int v = cpi->frag_v[pi]*8;

      for(y=0;y<v;y++){
        int lssd=0;
        for(x=0;x<h;x++)
          lssd += (frame[x]-recon[x])*(frame[x]-recon[x]);
        ssd+=lssd;
        frame+=stride;
        recon+=stride;
      }
      fi+=cpi->frag_n[pi];
    }

    minimize = ssd + (float)bits*cpi->token_lambda*16;

    fprintf(stdout,"%d %d %d %d %f %f %f %ld %ld %ld %ld %f %f  %.0f %.0f %.0f %.0f %.0f %.0f %.0f %.0f  %.0f %.0f %.0f %.0f %.0f %.0f %.0f %.0f  \n",
            (int)cpi->CurrentFrame, // 0
            cpi->BaseQ,             // 1
            cpi->token_lambda,      // 2
            cpi->skip_lambda,       // 3
            (double)cpi->rho_count[cpi->BaseQ]/total,           // 4
            (double)cpi->rho_postop/total,                      // 5
            (double)cpi->rho_postop/cpi->rho_count[cpi->BaseQ], // 6
            modebits,               // 7
            mvbits,                 // 8
            dctbits,                // 9
            oggpackB_bits(cpi->oggbuffer), // 10
            (double)ssd,              // 11
            (double)0,
            (double)cpi->dist_dist[0][0],//13
            (double)cpi->dist_dist[0][1],
            (double)cpi->dist_dist[0][2],
            (double)cpi->dist_dist[0][3],
            (double)cpi->dist_dist[0][4],
            (double)cpi->dist_dist[0][5],
            (double)cpi->dist_dist[0][6],
            (double)cpi->dist_dist[0][7],
            (double)(cpi->dist_bits[0][0]>>7),//21
            (double)(cpi->dist_bits[0][1]>>7),
            (double)(cpi->dist_bits[0][2]>>7),
            (double)(cpi->dist_bits[0][3]>>7),
            (double)(cpi->dist_bits[0][4]>>7),
            (double)(cpi->dist_bits[0][5]>>7),
            (double)(cpi->dist_bits[0][6]>>7),
            (double)(cpi->dist_bits[0][7]>>7)


            );
  }
#endif
#endif
  oc_enc_restore_fpu(cpi);
}

void WriteFrameHeader( CP_INSTANCE *cpi) {
  oggpack_buffer *opb=cpi->oggbuffer;

  /* Output the frame type (base/key frame or inter frame) */
  oggpackB_write( opb, cpi->FrameType, 1 );

  /* Write out details of the current value of Q... variable resolution. */
  oggpackB_write( opb, cpi->BaseQ, 6 ); // temporary

  /* we only support one Q index per frame */
  oggpackB_write( opb, 0, 1 );

  /* If the frame was a base frame then write out the frame dimensions. */
  if ( cpi->FrameType == KEY_FRAME ) {
    /* all bits reserved! */
    oggpackB_write( opb, 0, 3 );
  }
}

void oc_enc_dequant_idct8x8(const CP_INSTANCE *_cpi,ogg_int16_t _y[64],
 const ogg_int16_t _x[64],int _last_zzi,int _ncoefs,
 ogg_uint16_t _dc_quant,const ogg_uint16_t _ac_quant[64]){
  (*_cpi->opt_vtable.dequant_idct8x8)(_y,_x,_last_zzi,_ncoefs,
   _dc_quant,_ac_quant);
}

void oc_enc_loop_filter(CP_INSTANCE *_cpi,int _flimit){
  (*_cpi->opt_vtable.enc_loop_filter)(_cpi,_flimit);
}

void oc_enc_vtable_init_c(CP_INSTANCE *_cpi){
  /*The implementations prefixed with oc_enc_ are encoder-specific.
    The rest we re-use from the decoder.*/
  _cpi->opt_vtable.frag_sad=oc_enc_frag_sad_c;
  _cpi->opt_vtable.frag_sad_thresh=oc_enc_frag_sad_thresh_c;
  _cpi->opt_vtable.frag_sad2_thresh=oc_enc_frag_sad2_thresh_c;
  _cpi->opt_vtable.frag_satd_thresh=oc_enc_frag_satd_thresh_c;
  _cpi->opt_vtable.frag_satd2_thresh=oc_enc_frag_satd2_thresh_c;
  _cpi->opt_vtable.frag_intra_satd=oc_enc_frag_intra_satd_c;
  _cpi->opt_vtable.frag_sub=oc_enc_frag_sub_c;
  _cpi->opt_vtable.frag_sub_128=oc_enc_frag_sub_128_c;
  _cpi->opt_vtable.frag_copy=oc_frag_copy_c;
  _cpi->opt_vtable.frag_copy2=oc_enc_frag_copy2_c;
  _cpi->opt_vtable.frag_recon_intra=oc_frag_recon_intra_c;
  _cpi->opt_vtable.frag_recon_inter=oc_frag_recon_inter_c;
  _cpi->opt_vtable.fdct8x8=oc_enc_fdct8x8_c;
  _cpi->opt_vtable.dequant_idct8x8=oc_dequant_idct8x8_c;
  _cpi->opt_vtable.enc_loop_filter=oc_enc_loop_filter_c;
  _cpi->opt_vtable.restore_fpu=oc_restore_fpu_c;
}
