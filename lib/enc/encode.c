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

static void predict_frag(CP_INSTANCE  *cpi,
			 ogg_int16_t  *Last,
			 
			 macroblock_t *mb, 
			 macroblock_t *mb_left, 
			 macroblock_t *mb_downleft,
			 macroblock_t *mb_down,
			 macroblock_t *mb_downright, 
			 
			 int fi,
			 int fi_down){

  unsigned char *cp = cpi->frag_coded;
  ogg_int16_t   *dc = cpi->frag_dc;
  dct_t        *dct = cpi->frag_dct;
  int wpc = 0;

  /* only do 2 prediction if fragment coded and on non intra or
     if all fragments are intra */
  if(cp[fi]) {
    
    int WhichFrame = Mode2Frame[mb->mode];
    
    /* L, DL, D, DR */
    if(mb_left && cp[fi-1] && Mode2Frame[mb_left->mode] == WhichFrame)
      wpc|=1;
    if(mb_downleft && cp[fi_down-1] && Mode2Frame[mb_downleft->mode] == WhichFrame)
      wpc|=2;
    if(mb_down && cp[fi_down] && Mode2Frame[mb_down->mode] == WhichFrame)
      wpc|=4;
    if(mb_downright && cp[fi_down+1] && Mode2Frame[mb_downright->mode] == WhichFrame)
      wpc|=8;

    if(wpc){
      ogg_int16_t DC = 0;

      if(wpc&0x1) DC += pc[wpc][0]*dc[fi-1];
      if(wpc&0x2) DC += pc[wpc][1]*dc[fi_down-1];
      if(wpc&0x4) DC += pc[wpc][2]*dc[fi_down];
      if(wpc&0x8) DC += pc[wpc][3]*dc[fi_down+1];

      /* if we need to do a shift */
      if(pc[wpc][4]) {
	/* If negative add in the negative correction factor */
	DC += (HIGHBITDUPPED(DC) & pc[wpc][5]);
	/* Shift in lieu of a divide */
	DC >>= pc[wpc][4];
      }
      
      /* check for outranging on the two predictors that can outrange */
      if((wpc&(PU|PUL|PL)) == (PU|PUL|PL)){
	if( abs(DC - dc[fi_down]) > 128) {
	  DC = dc[fi_down];
	} else if( abs(DC - dc[fi-1]) > 128) {
	  DC = dc[fi-1];
	} else if( abs(DC - dc[fi_down-1]) > 128) {
	  DC = dc[fi_down-1];
	}
      }
      
      dct[fi].data[0] = dc[fi] - DC;
    
    }else{
      dct[fi].data[0] = dc[fi] - Last[WhichFrame];
    }

    /* Save the last fragment coded for whatever frame we are
       predicting from */
    
    Last[WhichFrame] = dc[fi];
  }
}

static void PredictDC(CP_INSTANCE *cpi){
  ogg_int32_t plane;
  ogg_int16_t Last[3];  /* last value used for given frame */
  int y,y2,x,fi = 0;

  /* for y,u,v; handles arbitrary plane subsampling arrangement.  Shouldn't need to be altered for 4:2:2 or 4:4:4 */
  for ( plane = 0; plane < 3 ; plane++) {
    macroblock_t *mb_row = cpi->macro;
    macroblock_t *mb_down = NULL;
    int fi_stride = cpi->frag_h[plane];
    int v = cpi->macro_v;
    int h = cpi->macro_h;

    for(x=0;x<3;x++)Last[x]=0;

    for ( y = 0 ; y < v ; y++) {
      for ( y2 = 0 ; y2 <= (cpi->macro_v < cpi->frag_v[plane]) ; y2++) {

	macroblock_t *mb = mb_row;
	macroblock_t *mb_left = NULL;
	macroblock_t *mb_downleft = NULL;
	macroblock_t *mb_downright = ((1<h && mb_down) ? mb_down+1: NULL);	    

	if(h < cpi->frag_h[plane]){
	  for ( x = 0 ; x < h ; x++) {
	    predict_frag(cpi,Last,mb, mb_left,mb_downleft,mb_down,mb_down, fi,fi-fi_stride);
	    predict_frag(cpi,Last,mb, mb,mb_down,mb_down,mb_downright, fi+1,fi-fi_stride+1);
	    fi+=2;
	    mb_left = mb;
	    mb_downleft = mb_down;
	    
	    mb++;
	    if(mb_down){
	      mb_down++;
	      mb_downright = (x+2<h ? mb_down+1: NULL);	    
	    }
	  }
	}else{
	  for ( x = 0 ; x < h ; x++) {
	    
	    predict_frag(cpi,Last,mb, mb_left,mb_downleft,mb_down,mb_downright, fi,fi-fi_stride);
	    fi++;
	    mb_left = mb;
	    mb_downleft = mb_down;
	    
	    mb++;
	    if(mb_down){
	      mb_down++;
	      mb_downright = (x+2<h ? mb_down+1: NULL);	    
	    }	    
	  }
	}
	mb_down = mb_row;
      }
      
      mb_row += h;
    }
  }
}

static ogg_uint32_t CodePlane ( CP_INSTANCE *cpi, int plane, int subsample){
  int B;
  unsigned char *cp = cpi->frag_coded;
  macroblock_t *mp = cpi->macro;
  macroblock_t *mp_end = cpi->macro+cpi->macro_total;
  int fi;

  switch(subsample){
  case 1:
    for ( ; mp<mp_end; mp++ ) {
      int *yuv = mp->yuv[plane];

      for ( B=0; B<4; B++) {
	fi = yuv[B];
	if ( cp[fi] ) {
	  TransformQuantizeBlock( cpi, mp->mode, fi, mp->mv[B] );
	  if(!cp[fi] && plane == 0){
	    mp->coded &= ~(1<<B);
	    if(!mp->coded) oc_mode_unset(cpi,mp);
	  }
	}
      }
    }
    return 0;
  case 2:
    /* fill me in when we need to support 4:2:2 */
    return 1;
  case 4:
    for ( ; mp<mp_end; mp++ ) {
      int fi = mp->yuv[plane][0];
      if ( cp[fi] ) {
	
	if(mp->mode == CODE_INTER_FOURMV){
	  mv_t mv;
	  
	  /* Calculate motion vector as the average of the Y plane ones. */
	  /* Uncoded members are 0,0 and not special-cased */
	  mv.x = mp->mv[0].x + mp->mv[1].x + mp->mv[2].x + mp->mv[3].x;
	  mv.y = mp->mv[0].y + mp->mv[1].y + mp->mv[2].y + mp->mv[3].y;
	  
	  mv.x = ( mv.x >= 0 ? (mv.x + 2) / 4 : (mv.x - 2) / 4);
	  mv.y = ( mv.y >= 0 ? (mv.y + 2) / 4 : (mv.y - 2) / 4);

	  TransformQuantizeBlock( cpi, mp->mode, fi, mv );
	}else
	  TransformQuantizeBlock( cpi, mp->mode, fi, mp->mv[0] );
    
      }  
    }
    return 0;
  default:
    return 1;
  }
}

static void EncodeDcTokenList (CP_INSTANCE *cpi) {
  int i,plane;
  int best;
  int huff[2];
  oggpack_buffer *opb=cpi->oggbuffer;

  /* Work out which table options are best for DC */
  for(plane = 0; plane<2; plane++){
    best = cpi->dc_bits[plane][0];
    huff[plane] = DC_HUFF_OFFSET;
    for ( i = 1; i < DC_HUFF_CHOICES; i++ ) {
      if ( cpi->dc_bits[plane][i] < best ) {
	best = cpi->dc_bits[plane][i];
	huff[plane] = i + DC_HUFF_OFFSET;
      }
    }
    oggpackB_write( opb, huff[plane] - DC_HUFF_OFFSET, DC_HUFF_CHOICE_BITS );
  }
  
  /* Encode the token list */
  for ( i = 0; i < cpi->dct_token_count[0]; i++){
    int token = cpi->dct_token[0][i];
    int eb = cpi->dct_token_eb[0][i];
    int plane = (i >= cpi->dct_token_ycount[0]);

    oggpackB_write( opb, cpi->HuffCodeArray_VP3x[huff[plane]][token],
                    cpi->HuffCodeLengthArray_VP3x[huff[plane]][token] );
    
    if ( cpi->ExtraBitLengths_VP3x[token] > 0 ) 
      oggpackB_write( opb, eb, cpi->ExtraBitLengths_VP3x[token] );
  }
}

static void EncodeAcGroup(CP_INSTANCE *cpi, int group, int huff_offset, int *huffchoice){
  int i;
  oggpack_buffer *opb=cpi->oggbuffer;
  int c = 0;
  int y = cpi->dct_token_ycount[group];
  
  for(i=0; i<cpi->dct_token_count[group]; i++){
    int token = cpi->dct_token[group][i];
    int eb = cpi->dct_token_eb[group][i];
    int plane = (c >= y);

    int hi = huff_offset + huffchoice[plane];

    oggpackB_write( opb, cpi->HuffCodeArray_VP3x[hi][token],
		    cpi->HuffCodeLengthArray_VP3x[hi][token] );
    
    if (cpi->ExtraBitLengths_VP3x[token] > 0) 
      oggpackB_write( opb, eb,cpi->ExtraBitLengths_VP3x[token] );
    
    c++;
  }
}

static void EncodeAcTokenList (CP_INSTANCE *cpi) {
  int i,plane;
  int best;
  int huff[2];
  oggpack_buffer *opb=cpi->oggbuffer;

  /* Work out which table options are best for AC */
  for(plane = 0; plane<2; plane++){
    best = cpi->ac_bits[plane][0];
    huff[plane] = AC_HUFF_OFFSET;
    for ( i = 1; i < AC_HUFF_CHOICES; i++ ) {
      if ( cpi->ac_bits[plane][i] < best ){
	best = cpi->ac_bits[plane][i];
	huff[plane] = i + AC_HUFF_OFFSET;
      }
    }
    oggpackB_write( opb, huff[plane] - AC_HUFF_OFFSET, AC_HUFF_CHOICE_BITS );
  }
  
  /* encode dct tokens, group 1 through group 63 in the four AC ranges */
  for(i=1;i<=AC_TABLE_2_THRESH;i++)
    EncodeAcGroup(cpi, i, 0, huff);

  for(;i<=AC_TABLE_3_THRESH;i++)
    EncodeAcGroup(cpi, i, AC_HUFF_CHOICES, huff);

  for(;i<=AC_TABLE_4_THRESH;i++)
    EncodeAcGroup(cpi, i, AC_HUFF_CHOICES*2, huff);

  for(;i<BLOCK_SIZE;i++)
    EncodeAcGroup(cpi, i, AC_HUFF_CHOICES*3, huff);

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
	oggpackB_write( opb, MvPatternPtr[mbp->mv[0].x], MvBitsPtr[mbp->mv[0].x] );
	oggpackB_write( opb, MvPatternPtr[mbp->mv[0].y], MvBitsPtr[mbp->mv[0].y] );
      }else if (mbp->mode == CODE_INTER_FOURMV){
	/* MV for each codedblock */
	for(B=0; B<4; B++ ){
	  if(mbp->coded & (1<<B)){
	    oggpackB_write( opb, MvPatternPtr[mbp->mv[B].x], MvBitsPtr[mbp->mv[B].x] );
	    oggpackB_write( opb, MvPatternPtr[mbp->mv[B].y], MvBitsPtr[mbp->mv[B].y] );
	  }
	}
      }
    }
  }
}

void EncodeData(CP_INSTANCE *cpi){

  dsp_save_fpu (cpi->dsp);

  /* Encode and tokenise the Y, U and V components */
  /* 4:2:0 for now */
  CodePlane(cpi, 0, 1);
  CodePlane(cpi, 1, 4);
  CodePlane(cpi, 2, 4);

  PredictDC(cpi);
  DPCMTokenize(cpi);

  /* The tree is not needed (implicit) for key frames */
  if ( cpi->FrameType != KEY_FRAME )
    PackAndWriteDFArray( cpi );

  /* Mode and MV data not needed for key frames. */
  if ( cpi->FrameType != KEY_FRAME ){
    PackModes(cpi);
    PackMotionVectors (cpi);
  }

  EncodeDcTokenList(cpi);
  EncodeAcTokenList(cpi);

  ReconRefFrames(cpi);

  dsp_restore_fpu (cpi->dsp);
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

