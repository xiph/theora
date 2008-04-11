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

static void ChooseTokenTables (CP_INSTANCE *cpi, int huff[4]) {
  int i,plane;
  int best;
  
  for(plane = 0; plane<2; plane++){

    /* Work out which table options are best for DC */
    best = cpi->dc_bits[plane][0];
    huff[plane] = DC_HUFF_OFFSET;
    for ( i = 1; i < DC_HUFF_CHOICES; i++ ) {
      if ( cpi->dc_bits[plane][i] < best ) {
	best = cpi->dc_bits[plane][i];
	huff[plane] = i + DC_HUFF_OFFSET;
      }
    }
  
    /* Work out which table options are best for AC */
    best = cpi->ac_bits[plane][0];
    huff[plane+2] = AC_HUFF_OFFSET;
    for ( i = 1; i < AC_HUFF_CHOICES; i++ ) {
      if ( cpi->ac_bits[plane][i] < best ){
	best = cpi->ac_bits[plane][i];
	huff[plane+2] = i + AC_HUFF_OFFSET;
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
  int y = cpi->dct_token_count[0][group];
  unsigned char *token = cpi->dct_token[0][group];
  ogg_uint16_t *eb = cpi->dct_token_eb[0][group];
 
  for(i=0; i<y; i++){
    oggpackB_write( opb, cpi->HuffCodeArray_VP3x[huffY][token[i]],
		    cpi->HuffCodeLengthArray_VP3x[huffY][token[i]] );
    if (cpi->ExtraBitLengths_VP3x[token[i]] > 0) 
      oggpackB_write( opb, eb[i], cpi->ExtraBitLengths_VP3x[token[i]] );
  }

  token = cpi->dct_token[1][group];
  eb = cpi->dct_token_eb[1][group];
  for(i=0; i<cpi->dct_token_count[1][group]; i++){
    oggpackB_write( opb, cpi->HuffCodeArray_VP3x[huffC][token[i]],
		    cpi->HuffCodeLengthArray_VP3x[huffC][token[i]] );
    if (cpi->ExtraBitLengths_VP3x[token[i]] > 0) 
      oggpackB_write( opb, eb[i], cpi->ExtraBitLengths_VP3x[token[i]] );
  }

  token = cpi->dct_token[2][group];
  eb = cpi->dct_token_eb[2][group];
  for(i=0; i<cpi->dct_token_count[2][group]; i++){
    oggpackB_write( opb, cpi->HuffCodeArray_VP3x[huffC][token[i]],
		    cpi->HuffCodeLengthArray_VP3x[huffC][token[i]] );
    if (cpi->ExtraBitLengths_VP3x[token[i]] > 0) 
      oggpackB_write( opb, eb[i], cpi->ExtraBitLengths_VP3x[token[i]] );
  }
}

static void EncodeTokenList (CP_INSTANCE *cpi, int huff[4]) {
  int i;
  oggpack_buffer *opb=cpi->oggbuffer;

  /* DC tokens aren't special, they just come first */
  oggpackB_write( opb, huff[0] - DC_HUFF_OFFSET, DC_HUFF_CHOICE_BITS );
  oggpackB_write( opb, huff[1] - DC_HUFF_OFFSET, DC_HUFF_CHOICE_BITS );

  EncodeTokenGroup(cpi, 0,  huff[0], huff[1]);

  /* AC tokens */
  oggpackB_write( opb, huff[2] - AC_HUFF_OFFSET, AC_HUFF_CHOICE_BITS );
  oggpackB_write( opb, huff[3] - AC_HUFF_OFFSET, AC_HUFF_CHOICE_BITS );

  for(i=1;i<=AC_TABLE_2_THRESH;i++)
    EncodeTokenGroup(cpi, i,  huff[2], huff[3]);

  for(;i<=AC_TABLE_3_THRESH;i++)
    EncodeTokenGroup(cpi, i,  huff[2]+AC_HUFF_CHOICES, huff[3]+AC_HUFF_CHOICES);

  for(;i<=AC_TABLE_4_THRESH;i++)
    EncodeTokenGroup(cpi, i,  huff[2]+AC_HUFF_CHOICES*2, huff[3]+AC_HUFF_CHOICES*2);

  for(;i<BLOCK_SIZE;i++)
    EncodeTokenGroup(cpi, i,  huff[2]+AC_HUFF_CHOICES*3, huff[3]+AC_HUFF_CHOICES*3);

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
  int tokenhuff[4];
  long bits;

  dsp_save_fpu (cpi->dsp);

  PredictDC(cpi);
  DPCMTokenize(cpi);

  bits = oggpackB_bits(cpi->oggbuffer);

  /* Mode and MV data not needed for key frames. */
  if ( cpi->FrameType != KEY_FRAME ){
    PackModes(cpi);
    bits = oggpackB_bits(cpi->oggbuffer);
    PackMotionVectors (cpi);
    bits = oggpackB_bits(cpi->oggbuffer);
  }

  ChooseTokenTables(cpi, tokenhuff);
#ifdef COLLECT_METRICS
  ModeMetrics(cpi,tokenhuff);
#endif
  EncodeTokenList(cpi, tokenhuff);
  bits = oggpackB_bits(cpi->oggbuffer);
  
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

