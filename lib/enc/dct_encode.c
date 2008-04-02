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
#include "dsp.h"
#include "quant_lookup.h"

static int acoffset[64]={
  16,16,16,16,16,16, 32,32,
  32,32,32,32,32,32,32, 48,
  48,48,48,48,48,48,48,48,
  48,48,48,48, 64,64,64,64,
  64,64,64,64,64,64,64,64,
  64,64,64,64,64,64,64,64,
  64,64,64,64,64,64,64,64};

static void add_token(CP_INSTANCE *cpi, int plane, int coeff, 
		      unsigned char token, ogg_uint16_t eb, int fi){

  cpi->dct_token[plane][coeff][cpi->dct_token_count[plane][coeff]] = token;
  cpi->dct_token_eb[plane][coeff][cpi->dct_token_count[plane][coeff]] = eb;
#ifdef COLLECT_METRICS
  cpi->dct_token_frag[plane][coeff][cpi->dct_token_count[plane][coeff]] = fi;
#endif
  cpi->dct_token_count[plane][coeff]++;

  if(coeff == 0){
    /* DC */
    int i;
    for ( i = 0; i < DC_HUFF_CHOICES; i++)
      cpi->dc_bits[plane>0][i] += cpi->HuffCodeLengthArray_VP3x[i][token];
  }else{
    /* AC */
    int i,offset = acoffset[coeff];
    for ( i = 0; i < AC_HUFF_CHOICES; i++)
      cpi->ac_bits[plane>0][i] += cpi->HuffCodeLengthArray_VP3x[offset+i][token];
  }
}

static void prepend_token(CP_INSTANCE *cpi, int plane, int coeff, 
			  unsigned char token, ogg_uint16_t eb, int fi){

  cpi->dct_token[plane][coeff]--;
  cpi->dct_token_eb[plane][coeff]--;
#ifdef COLLECT_METRICS
  cpi->dct_token_frag[plane][coeff]--;
  cpi->dct_token_frag[plane][coeff][0] = fi;
#endif
  cpi->dct_token[plane][coeff][0] = token;
  cpi->dct_token_eb[plane][coeff][0] = eb;
  cpi->dct_token_count[plane][coeff]++;

  if(coeff == 0){
    /* DC */
    int i;
    for ( i = 0; i < DC_HUFF_CHOICES; i++)
      cpi->dc_bits[plane>0][i] += cpi->HuffCodeLengthArray_VP3x[i][token];
  }else{
    /* AC */
    int i,offset = acoffset[coeff];
    for ( i = 0; i < AC_HUFF_CHOICES; i++)
      cpi->ac_bits[plane>0][i] += cpi->HuffCodeLengthArray_VP3x[offset+i][token];
  }
}

static void emit_eob_run(CP_INSTANCE *cpi, int plane, int pos, int run){
  if ( run <= 3 ) {
    if ( run == 1 ) {
      add_token(cpi, plane, pos, DCT_EOB_TOKEN, 0, 0);
    } else if ( run == 2 ) {
      add_token(cpi, plane, pos, DCT_EOB_PAIR_TOKEN, 0, 0);
    } else {
      add_token(cpi, plane, pos, DCT_EOB_TRIPLE_TOKEN, 0, 0);
    }
    
  } else {
    
    if ( run < 8 ) {
      add_token(cpi, plane, pos, DCT_REPEAT_RUN_TOKEN, run-4, 0);
    } else if ( run < 16 ) {
      add_token(cpi, plane, pos, DCT_REPEAT_RUN2_TOKEN, run-8, 0);
    } else if ( run < 32 ) {
      add_token(cpi, plane, pos, DCT_REPEAT_RUN3_TOKEN, run-16, 0);
    } else if ( run < 4096) {
      add_token(cpi, plane, pos, DCT_REPEAT_RUN4_TOKEN, run, 0);
    }
  }
}

static void prepend_eob_run(CP_INSTANCE *cpi, int plane, int pos, int run){
  if ( run <= 3 ) {
    if ( run == 1 ) {
      prepend_token(cpi, plane, pos, DCT_EOB_TOKEN, 0, 0);
    } else if ( run == 2 ) {
      prepend_token(cpi, plane, pos, DCT_EOB_PAIR_TOKEN, 0, 0);
    } else {
      prepend_token(cpi, plane, pos, DCT_EOB_TRIPLE_TOKEN, 0, 0);
    }
    
  } else {
    
    if ( run < 8 ) {
      prepend_token(cpi, plane, pos, DCT_REPEAT_RUN_TOKEN, run-4, 0);
    } else if ( run < 16 ) {
      prepend_token(cpi, plane, pos, DCT_REPEAT_RUN2_TOKEN, run-8, 0);
    } else if ( run < 32 ) {
      prepend_token(cpi, plane, pos, DCT_REPEAT_RUN3_TOKEN, run-16, 0);
    } else if ( run < 4096) {
      prepend_token(cpi, plane, pos, DCT_REPEAT_RUN4_TOKEN, run, 0);
    }
  }
}

static void TokenizeDctValue (CP_INSTANCE *cpi, 
			      int plane, 
			      int coeff,
			      ogg_int16_t DataValue,
			      int fi){

  int AbsDataVal = abs(DataValue);
  int neg = (DataValue<0);

  if ( AbsDataVal == 1 ){

    add_token(cpi, plane, coeff, (neg ? MINUS_ONE_TOKEN : ONE_TOKEN), 0, fi);

  } else if ( AbsDataVal == 2 ) {

    add_token(cpi, plane, coeff, (neg ? MINUS_TWO_TOKEN : TWO_TOKEN), 0, fi);

  } else if ( AbsDataVal <= MAX_SINGLE_TOKEN_VALUE ) {

    add_token(cpi, plane, coeff, LOW_VAL_TOKENS + (AbsDataVal - DCT_VAL_CAT2_MIN), neg, fi);

  } else if ( AbsDataVal <= 8 ) {

    add_token(cpi, plane, coeff, DCT_VAL_CATEGORY3, (AbsDataVal - DCT_VAL_CAT3_MIN) + (neg << 1), fi);

  } else if ( AbsDataVal <= 12 ) {

    add_token(cpi, plane, coeff, DCT_VAL_CATEGORY4, (AbsDataVal - DCT_VAL_CAT4_MIN) + (neg << 2), fi);

  } else if ( AbsDataVal <= 20 ) {

    add_token(cpi, plane, coeff, DCT_VAL_CATEGORY5, (AbsDataVal - DCT_VAL_CAT5_MIN) + (neg << 3), fi);

  } else if ( AbsDataVal <= 36 ) {

    add_token(cpi, plane, coeff, DCT_VAL_CATEGORY6, (AbsDataVal - DCT_VAL_CAT6_MIN) + (neg << 4), fi);

  } else if ( AbsDataVal <= 68 ) {

    add_token(cpi, plane, coeff, DCT_VAL_CATEGORY7, (AbsDataVal - DCT_VAL_CAT7_MIN) + (neg << 5), fi);

  } else {

    add_token(cpi, plane, coeff, DCT_VAL_CATEGORY8, (AbsDataVal - DCT_VAL_CAT8_MIN) + (neg << 9), fi);

  } 
}

static void TokenizeDctRunValue (CP_INSTANCE *cpi, 
				 int plane, 
				 int coeff,
				 unsigned char RunLength,
				 ogg_int16_t DataValue,
				 int fi){

  ogg_uint32_t AbsDataVal = abs( (ogg_int32_t)DataValue );
  int neg = (DataValue<0);

  /* Values are tokenised as category value and a number of additional
     bits  that define the category.  */

  if ( AbsDataVal == 1 ) {

    if ( RunLength <= 5 ) 
      add_token(cpi,plane,coeff, DCT_RUN_CATEGORY1 + RunLength - 1, neg, fi);
    else if ( RunLength <= 9 ) 
      add_token(cpi,plane,coeff, DCT_RUN_CATEGORY1B, RunLength - 6 + (neg<<2), fi);
    else 
      add_token(cpi,plane,coeff, DCT_RUN_CATEGORY1C, RunLength - 10 + (neg<<3), fi);

  } else if ( AbsDataVal <= 3 ) {

    if ( RunLength == 1 ) 
      add_token(cpi,plane,coeff, DCT_RUN_CATEGORY2, AbsDataVal - 2 + (neg<<1), fi);
    else
      add_token(cpi,plane,coeff, DCT_RUN_CATEGORY2B, 
		(neg<<2) + ((AbsDataVal-2)<<1) + RunLength - 2, fi);

  }
}

static void tokenize_block(CP_INSTANCE *cpi, int fi, int plane,
			   int *eob_run, int *eob_pre){
  unsigned char *cp=cpi->frag_coded;
  if ( cp[fi] ) {
    int coeff = 0;
    dct_t *dct = &cpi->frag_dct[fi];
    cpi->frag_nonzero[fi] = 0;
    
    while(coeff < BLOCK_SIZE){
      ogg_int16_t val = dct->data[coeff];
      int zero_run;
      int i = coeff;
      
      cpi->frag_nonzero[fi] = coeff;
      
      while( !val && (++i < BLOCK_SIZE) )
	val = dct->data[i];
	  
      if ( i == BLOCK_SIZE ){
	
	/* if there are no other tokens in this group yet, set up to be
	   prepended later.  Group 0/Plane 0 is the exception (can't be
	   prepended) */
	if(cpi->dct_token_count[plane][coeff] == 0 && (coeff||plane)){
	  /* prepending requires space to do so-- save some at front of token stack */
	  if(eob_pre[coeff]==0 || eob_pre[coeff]&0x8ff){ /* 0xfff is a safe overallocation, 
	                                                    saves a mod 4095 */
	    cpi->dct_token[plane][coeff]++;
	    cpi->dct_token_eb[plane][coeff]++;
#ifdef COLLECT_METRICS
	    cpi->dct_token_frag[plane][coeff]++;
#endif
	  }

	  /* finally, track pre-run */
	  eob_pre[coeff]++;

#ifdef COLLECT_METRICS
	  cpi->dct_eob_fi_stack[plane][coeff][cpi->dct_eob_fi_count[plane][coeff]++]=fi;
#endif
	}else{
	  if(eob_run[coeff] == 4095){
	    emit_eob_run(cpi,plane,coeff,4095);
	    eob_run[coeff] = 0;
	  }
	  
	  eob_run[coeff]++;
#ifdef COLLECT_METRICS
	  cpi->dct_eob_fi_stack[plane][coeff][cpi->dct_eob_fi_count[plane][coeff]++]=fi;
#endif
	}
	coeff = BLOCK_SIZE;
      }else{
	
	if(eob_run[coeff]){
	  emit_eob_run(cpi,plane,coeff,eob_run[coeff]);
	  eob_run[coeff]=0;
	}
	
	zero_run = i-coeff;
	if (zero_run){
	  ogg_uint32_t absval = abs(val);
	  if ( ((absval == 1) && (zero_run <= 17)) ||
	       ((absval <= 3) && (zero_run <= 3)) ) {
	    TokenizeDctRunValue( cpi, plane, coeff, zero_run, val, fi);
	    coeff = i+1;
	  }else{
	    if ( zero_run <= 8 )
	      add_token(cpi, plane, coeff, DCT_SHORT_ZRL_TOKEN, zero_run - 1, fi);
	    else
	      add_token(cpi, plane, coeff, DCT_ZRL_TOKEN, zero_run - 1, fi);
	    coeff = i;
	  }
	}else{
	  TokenizeDctValue(cpi, plane, coeff, val, fi);
	  coeff = i+1;
	}
      }
    }
  }
}

void DPCMTokenize (CP_INSTANCE *cpi){
  int eob_run[3][64];
  int eob_pre[3][64];
  
  int i,j,sbi,mbi;

  memset(eob_run, 0, sizeof(eob_run));
  memset(eob_pre, 0, sizeof(eob_pre));
  memset(cpi->dc_bits, 0, sizeof(cpi->dc_bits));
  memset(cpi->ac_bits, 0, sizeof(cpi->ac_bits));
  memset(cpi->dct_token_count, 0, sizeof(cpi->dct_token_count));
#ifdef COLLECT_METRICS
  memset(cpi->dct_eob_fi_count, 0, sizeof(cpi->dct_eob_fi_count));
#endif

  for(i=0;i<BLOCK_SIZE;i++){
    cpi->dct_token[0][i] = cpi->dct_token_storage+cpi->frag_total*i;
    cpi->dct_token[1][i] = cpi->dct_token[0][i] + cpi->frag_n[0];
    cpi->dct_token[2][i] = cpi->dct_token[1][i] + cpi->frag_n[1];

    cpi->dct_token_eb[0][i] = cpi->dct_token_eb_storage+cpi->frag_total*i;
    cpi->dct_token_eb[1][i] = cpi->dct_token_eb[0][i] + cpi->frag_n[0];
    cpi->dct_token_eb[2][i] = cpi->dct_token_eb[1][i] + cpi->frag_n[1];

#ifdef COLLECT_METRICS
    cpi->dct_eob_fi_stack[0][i] = cpi->dct_eob_fi_storage+cpi->frag_total*i;
    cpi->dct_eob_fi_stack[1][i] = cpi->dct_eob_fi_stack[0][i] + cpi->frag_n[0];
    cpi->dct_eob_fi_stack[2][i] = cpi->dct_eob_fi_stack[1][i] + cpi->frag_n[1];
    cpi->dct_token_frag[0][i] = cpi->dct_token_frag_storage+cpi->frag_total*i;
    cpi->dct_token_frag[1][i] = cpi->dct_token_frag[0][i] + cpi->frag_n[0];
    cpi->dct_token_frag[2][i] = cpi->dct_token_frag[1][i] + cpi->frag_n[1];
#endif
  }

  
  for (sbi=0; sbi < cpi->super_n[0]; sbi++ ){
    superblock_t *sp = &cpi->super[0][sbi];
    for (mbi=0; mbi < 4; mbi++ ){
      int bi;
      macroblock_t *mb = &cpi->macro[sp->m[mbi]];
      for (bi=0; bi<4; bi++ ) {
	int fi = mb->Hyuv[0][bi];
	tokenize_block(cpi, fi, 0, eob_run[0], eob_pre[0]);
      }
    }
  }
  for (sbi=0; sbi < cpi->super_n[1]; sbi++ ){
    superblock_t *sb = &cpi->super[1][sbi];
    int bi;
    for (bi=0; bi<16; bi++ ) {
      int fi = sb->f[bi];
      tokenize_block(cpi, fi, 1, eob_run[1], eob_pre[1]);
    }
  }

  for (sbi=0; sbi < cpi->super_n[2]; sbi++ ){
    superblock_t *sb = &cpi->super[2][sbi];
    int bi;
    for (bi=0; bi<16; bi++ ) {
      int fi = sb->f[bi];
      tokenize_block(cpi, fi, 2, eob_run[2], eob_pre[2]);
    }
  }

  /* tie together eob runs at the beginnings/ends of coeff groups */
  {
    int coeff = 0;
    int run = 0;
    int plane = 0; /* not the current plane; plane of next run token
		      to emit */

    for(i=0;i<BLOCK_SIZE;i++){
      for(j=0;j<3;j++){
	if(eob_pre[j][i]){
	  /* group begins with an EOB run */
	  
	  if(run && run + eob_pre[j][i] >= 4095){
	    emit_eob_run(cpi,plane,coeff,4095);
	    eob_pre[j][i] -= 4095-run; 
	    run = 0;
	    coeff = i;
	    plane = j;
	  }
	
	  if(run){
	    if(cpi->dct_token_count[j][i]){
	      /* group is not only an EOB run; emit the run token */
	      emit_eob_run(cpi,plane,coeff,run + eob_pre[j][i]);
	      eob_pre[j][i] = 0;
	      run = eob_run[j][i];
	      coeff = i;
	      plane = j;
	    }else{
	      /* group consists entirely of EOB run.  Add, iterate */
	      run += eob_pre[j][i];
	      eob_pre[j][i] = 0;
	    }
	  }else{
	    
	    if(cpi->dct_token_count[j][i]){
	      /* there are other tokens in this group; work backwards as we need to prepend */
	      while(eob_pre[j][i] >= 4095){
		prepend_eob_run(cpi,j,i,4095);
		eob_pre[j][i] -= 4095;
	      }
	      if(eob_pre[j][i]){
		prepend_eob_run(cpi,j, i, eob_pre[j][i]);
		eob_pre[j][i] = 0;
	      }
	      run = eob_run[j][i];
	      coeff = i;
	      plane = j;
	    }else{
	      /* group consists entirely of EOB run.  Add, flush overs, iterate */
	      while(eob_pre[j][i] >= 4095){
		emit_eob_run(cpi,j,i,4095);
		eob_pre[j][i] -= 4095;
	      }
	      run = eob_pre[j][i];
	      coeff = i;
	      plane = j;
	    }
	  }
	}else{
	  /* no eob run to begin group */
	  if(cpi->dct_token_count[j][i]){
	    if(run)
	      emit_eob_run(cpi,plane,coeff,run);
	  
	    run = eob_run[j][i];
	    coeff = i;
	    plane = j;
	  }
	}
      }
    }
      
    if(run)
      emit_eob_run(cpi,plane,coeff,run);
    
  }
}

static int ModeUsesMC[MAX_MODES] = { 0, 0, 1, 1, 1, 0, 1, 1 };

static void BlockUpdateDifference (CP_INSTANCE * cpi, 
				   unsigned char *FiltPtr,
				   ogg_int16_t *DctInputPtr, 
				   ogg_int32_t MvDivisor,
				   int fi,
				   ogg_uint32_t PixelsPerLine,
				   int mode,
				   mv_t mv) {

  ogg_int32_t MvShift;
  ogg_int32_t MvModMask;
  ogg_int32_t  AbsRefOffset;
  ogg_int32_t  AbsXOffset;
  ogg_int32_t  AbsYOffset;
  ogg_int32_t  MVOffset;        /* Baseline motion vector offset */
  ogg_int32_t  ReconPtr2Offset; /* Offset for second reconstruction in
                                   half pixel MC */
  unsigned char  *ReconPtr1;    /* DCT reconstructed image pointers */
  unsigned char  *ReconPtr2;    /* Pointer used in half pixel MC */
  int bi = cpi->frag_buffer_index[fi];
  unsigned char *thisrecon = &cpi->recon[bi];

  if ( ModeUsesMC[mode] ){
    switch(MvDivisor) {
    case 2:
      MvShift = 1;
      MvModMask = 1;
      break;
    case 4:
      MvShift = 2;
      MvModMask = 3;
      break;
    default:
      break;
    }
    
    /* Set up the baseline offset for the motion vector. */
    MVOffset = ((mv.y / MvDivisor) * PixelsPerLine) + (mv.x / MvDivisor);
    
    /* Work out the offset of the second reference position for 1/2
       pixel interpolation.  For the U and V planes the MV specifies 1/4
       pixel accuracy. This is adjusted to 1/2 pixel as follows ( 0->0,
       1/4->1/2, 1/2->1/2, 3/4->1/2 ). */
    ReconPtr2Offset = 0;
    AbsXOffset = mv.x % MvDivisor;
    AbsYOffset = mv.y % MvDivisor;
    
    if ( AbsXOffset ) {
      if ( mv.x > 0 )
	ReconPtr2Offset += 1;
      else
	ReconPtr2Offset -= 1;
    }
    
    if ( AbsYOffset ) {
      if ( mv.y > 0 )
	ReconPtr2Offset += PixelsPerLine;
      else
	ReconPtr2Offset -= PixelsPerLine;
    }
    
    if ( mode==CODE_GOLDEN_MV ) {
      ReconPtr1 = &cpi->golden[bi];
    } else {
      ReconPtr1 = &cpi->lastrecon[bi];
    }
    
    ReconPtr1 += MVOffset;
    ReconPtr2 =  ReconPtr1 + ReconPtr2Offset;
    
    AbsRefOffset = abs((int)(ReconPtr1 - ReconPtr2));
    
    /* Is the MV offset exactly pixel alligned */
    if ( AbsRefOffset == 0 ){
      dsp_sub8x8(cpi->dsp, FiltPtr, ReconPtr1, DctInputPtr, PixelsPerLine);
      dsp_copy8x8 (cpi->dsp, ReconPtr1, thisrecon, PixelsPerLine);
    } else {
      /* Fractional pixel MVs. */
      /* Note that we only use two pixel values even for the diagonal */
      dsp_sub8x8avg2(cpi->dsp, FiltPtr, ReconPtr1, ReconPtr2, DctInputPtr, PixelsPerLine);
      dsp_copy8x8_half (cpi->dsp, ReconPtr1, ReconPtr2, thisrecon, PixelsPerLine);
    }

  } else { 
    if ( ( mode==CODE_INTER_NO_MV ) ||
	 ( mode==CODE_USING_GOLDEN ) ) {
      if ( mode==CODE_INTER_NO_MV ) {
	ReconPtr1 = &cpi->lastrecon[bi];
      } else {
	ReconPtr1 = &cpi->golden[bi];
      }
      
      dsp_sub8x8(cpi->dsp, FiltPtr, ReconPtr1, DctInputPtr,PixelsPerLine);
      dsp_copy8x8 (cpi->dsp, ReconPtr1, thisrecon, PixelsPerLine);
    } else if ( mode==CODE_INTRA ) {
      dsp_sub8x8_128(cpi->dsp, FiltPtr, DctInputPtr, PixelsPerLine);
      dsp_set8x8(cpi->dsp, 128, thisrecon, PixelsPerLine);
    }
  }
}

void TransformQuantizeBlock (CP_INSTANCE *cpi, 
			     coding_mode_t mode,
			     int fi,
			     mv_t mv){

  unsigned char *FiltPtr = &cpi->frame[cpi->frag_buffer_index[fi]];
  int qi = cpi->BaseQ; // temporary
  int inter = (mode != CODE_INTRA);
  int plane = (fi < cpi->frag_n[0] ? 0 : 
	       (fi-cpi->frag_n[0] < cpi->frag_n[1] ? 1 : 2)); 
  ogg_int32_t *q = cpi->iquant_tables[inter][plane][qi];
  ogg_int16_t DCTInput[64];
  ogg_int16_t DCTOutput[64];
  ogg_int32_t   MvDivisor;      /* Defines MV resolution (2 = 1/2
                                   pixel for Y or 4 = 1/4 for UV) */
  /* Set plane specific values */
  if (plane == 0){
    MvDivisor = 2;                  /* 1/2 pixel accuracy in Y */
  }else{
    MvDivisor = 4;                  /* UV planes at 1/2 resolution of Y */
  }

  /* produces the appropriate motion compensation block, applies it to
     the reconstruction buffer, and proces a difference block for
     forward DCT */
  BlockUpdateDifference(cpi, FiltPtr, DCTInput, 
			MvDivisor, fi, cpi->stride[plane], mode, mv);

  dsp_fdct_short(cpi->dsp, DCTInput, DCTOutput);

  /* Quantize that transform data. */
  quantize (cpi, q, DCTOutput, cpi->frag_dct[fi].data);
  cpi->frag_dc[fi] = cpi->frag_dct[fi].data[0];

}
