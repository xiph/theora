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

/* plane == 0 for Y, 1 for UV */
static void add_token(CP_INSTANCE *cpi, int plane, int coeff, 
		      unsigned char token, ogg_uint16_t eb,
		      int prepend){
  if(prepend){
    int pre = cpi->dct_token_pre[coeff]++;
    *(cpi->dct_token[coeff] - pre)  = token;
    *(cpi->dct_token_eb[coeff] - pre)  = eb;
  }else{
    cpi->dct_token[pos][cpi->dct_token_count[pos]] = token;
    cpi->dct_token_eb[pos][cpi->dct_token_count[pos]] = eb;
    cpi->dct_token_count[pos]++;
  }

  if(coeff == 0){
    /* DC */
    for ( i = 0; i < DC_HUFF_CHOICES; i++)
      cpi->dc_bits[plane][i] += cpi->HuffCodeLengthArray_VP3x[i][token];
  }else{
    /* AC */
    int offset = acoffset[coeff];
    for ( i = 0; i < AC_HUFF_CHOICES; i++)
      cpc->ac_bits[plane][i] += cpi->HuffCodeLengthArray_VP3x[offset+i][token];
  }

  if(!plane)cpi->dct_token_ycount[coeff]++;
}

static void emit_eob_run(CP_INSTANCE *cpi, int plane, int pos, int run, int prepend){
  if ( run <= 3 ) {
    if ( run == 1 ) {
      add_token(cpi, plane, pos, DCT_EOB_TOKEN, 0, prepend);
    } else if ( cpi->RunLength == 2 ) {
      add_token(cpi, plane, pos, DCT_EOB_PAIR_TOKEN, 0, prepend);
    } else {
      add_token(cpi, plane, pos, DCT_EOB_TRIPLE_TOKEN, 0, prepend);
    }
    
  } else {
    
    if ( run < 8 ) {
      add_token(cpi, plane, pos, DCT_REPEAT_RUN_TOKEN, run-4, prepend);
    } else if ( cpi->RunLength < 16 ) {
      add_token(cpi, plane, pos, DCT_REPEAT_RUN2_TOKEN, run-8, prepend);
    } else if ( cpi->RunLength < 32 ) {
      add_token(cpi, plane, pos, DCT_REPEAT_RUN3_TOKEN, run-16, prepend);
    } else if ( cpi->RunLength < 4096) {
      add_token(cpi, plane, pos, DCT_REPEAT_RUN4_TOKEN, run, prepend);
    }
  }
}

static void add_eob_run(CP_INSTANCE *cpi, int plane, int pos, 
			int *eob_run, int *eob_pre){
  run = eob_run[pos];
  if(!run) return;
  
  /* pre-runs for a coefficient group > DC are a special case; they're
     handled when groups are tied together at the end of tokenization */
  if(pos > 0 !cpi->dct_token_count[pos]){
    /* no tokens emitted yet, this is a pre-run */
    eob_pre[pos] += run;
  }else{
    emit_eob_run(cpi,plane,pos,run,0);
  }
  
  eob_run[pos] = 0;
}

static void TokenizeDctValue (CP_INSTANCE *cpi, 
			      int plane, 
			      int coeff,
			      ogg_int16_t DataValue){

  int AbsDataVal = abs(DataValue);
  int neg = (DataValue<0);

  if ( AbsDataVal == 1 ){

    add_token(cpi, plane, pos, (neg ? MINUS_ONE_TOKEN : ONE_TOKEN), 0, 0);

  } else if ( AbsDataVal == 2 ) {

    add_token(cpi, plane, pos, (neg ? MINUS_TWO_TOKEN : TWO_TOKEN), 0, 0);

  } else if ( AbsDataVal <= MAX_SINGLE_TOKEN_VALUE ) {

    add_token(cpi, plane, pos, LOW_VAL_TOKENS + (AbsDataVal - DCT_VAL_CAT2_MIN), neg, 0);

  } else if ( AbsDataVal <= 8 ) {

    add_token(cpi, plane, pos, DCT_VAL_CATEGORY3, (AbsDataVal - DCT_VAL_CAT3_MIN) + (neg << 1), 0);

  } else if ( AbsDataVal <= 12 ) {

    add_token(cpi, plane, pos, DCT_VAL_CATEGORY4, (AbsDataVal - DCT_VAL_CAT4_MIN) + (neg << 2), 0);

  } else if ( AbsDataVal <= 20 ) {

    add_token(cpi, plane, pos, DCT_VAL_CATEGORY5, (AbsDataVal - DCT_VAL_CAT5_MIN) + (neg << 3), 0);

  } else if ( AbsDataVal <= 36 ) {

    add_token(cpi, plane, pos, DCT_VAL_CATEGORY6, (AbsDataVal - DCT_VAL_CAT6_MIN) + (neg << 4), 0);

  } else if ( AbsDataVal <= 68 ) {

    add_token(cpi, plane, pos, DCT_VAL_CATEGORY7, (AbsDataVal - DCT_VAL_CAT7_MIN) + (neg << 5), 0);

  } else {

    add_token(cpi, plane, pos, DCT_VAL_CATEGORY8, (AbsDataVal - DCT_VAL_CAT8_MIN) + (neg << 9), 0);

  } 
}

static void TokenizeDctRunValue (CP_INSTANCE *cpi, 
				 int plane, 
				 int coeff,
				 unsigned char RunLength,
				 ogg_int16_t DataValue){

  ogg_uint32_t AbsDataVal = abs( (ogg_int32_t)DataValue );

  /* Values are tokenised as category value and a number of additional
     bits  that define the category.  */

  if ( AbsDataVal == 1 ) {

    if ( RunLength <= 5 ) 
      add_token(cpi,plane,coeff, DCT_RUN_CATEGORY1 + (RunLength - 1), ((DataValue&0x8000)>>15), 0);
    else if ( RunLength <= 9 ) 
      add_token(cpi,plane,coeff, DCT_RUN_CATEGORY1B, RunLength - 6 + ((DataValue&0x8000)>>13), 0);
    else 
      add_token(cpi,plane,coeff, DCT_RUN_CATEGORY1C, RunLength - 10 + ((DataValue&0x8000)>>12), 0);

  } else if ( AbsDataVal <= 3 ) {

    if ( RunLength == 1 ) 
      add_token(cpi,plane,coeff, DCT_RUN_CATEGORY2, AbsDataVal - 2 + ((DataValue&0x8000)>>14), 0);
    else
      add_token(cpi,plane,coeff, DCT_RUN_CATEGORY2B, 
		((DataValue&0x8000)>>13) + ((AbsDataVal-2)<<1) + RunLength - 2, 0);

  }
}

static int tokenize_pass (fragment_t *fp, int pos, int plane, int *eob_run, int *eob_pre){
  ogg_int16_t DC = fp->pred_dc;
  ogg_int16_t *RawData = fp->dct;
  ogg_uint32_t *TokenListPtr = fp->token_list;
  ogg_int16_t val = (i ? RawData[i] : DC);
  int zero_run;
  int i = pos;
  
  while( (i < BLOCK_SIZE) && (!val) )
    val = RawData[++i];

  if ( i == BLOCK_SIZE ){

    eob_run[pos]++;
    if(eob_run[pos] >= 4095)
      add_eob_run(cpi,plane,pos,eob_run,eob_pre);
    return i;

  }
  
  fp->nonzero = i;
  add_eob_run(cpi,plane,pos,eob_run,eob_pre); /* if any */
  
  zero_run = i-pos;
  if (zero_run){
    ogg_uint32_t absval = abs(val);
    if ( ((absval == 1) && (zero_run <= 17)) ||
	 ((absval <= 3) && (zero_run <= 3)) ) {
      TokenizeDctRunValue( cpi, plane, pos, run_count, val);
    }else{
      if ( zero_run <= 8 )
	add_token(cpi, plane, pos, DCT_SHORT_ZRL_TOKEN, run_count - 1);
      else
	add_token(cpi, pos, DCT_ZRL_TOKEN, run_count - 1);
      TokenizeDctValue( cpi, plane, pos, val );
    }
  }else{
    TokenizeDctValue( cpi, plane, pos, val );
  }

  return i+1;
}

void DPCMTokenize (CP_INSTANCE *cpi){
  fragment_t *fp = cpi->coded_tail;
  int eob_run[64];
  int eob_pre[64];

  memset(eob_run, 0 sizeof(eob_run));
  memset(eob_pre, 0 sizeof(eob_pre));
  memset(cpi->dc_bits, 0, sizeof(cpi->dc_bits));
  memset(cpi->ac_bits, 0, sizeof(cpi->ac_bits));
  memset(cpi->dct_token_count, 0, sizeof(cpi->dct_token_count));
  memset(cpi->dct_token_ycount, 0, sizeof(cpi->dct_token_ycount));
  memset(cpi->dct_token_pre, 0, sizeof(cpi->dct_token_pre));

  /* Tokenise the dct data. */
  while(fp){
    int coeff = 0;
    int plane = (fp >= cpi->frag[1]);
    
    fp->nonzero = 0;
    while(coeff < BLOCK_SIZE)
      coeff = tokenize_pass (fp, coeff, plane, eob_run, eob_pre);

    fp = fp->next;
  }
    
  /* sew together group pre- and post- EOB runs */
  /* there are two categories of runs:
     1) runs that end a group and may or may not extend into the next
     2) pre runs that begin at group position 0
     
     Category 2 is a special case that requires potentially
     'prepending' tokens to the group. */
   
  {
    int run = 0;
    int run_coeff = 0;
    int run_frag = 0;

    for(i=0;i<BLOCK_SIZE;i++){
      run += cpi->dct_eob_pre[i];

      if(cpi->dct_token_count[i]){

	/* first code the spanning/preceeding run */
	while(run){
	  int v = (run < 4095 ? run : 4095);
	  int plane = (run_frag >= cpi->coded_y);

	  if(run_coeff == i){
	    /* prepend to current group */
	    emit_eob_run(cpi, plane, run_coeff, v, 1);
	  }else{
	    /* append to indicated coeff group; it is possible the run
	       spans multiple coeffs, it will always be an append as
	       intervening groups would have a zero token count */
	    emit_eob_run(cpi, plane, run_coeff, v, 0);
	  }
	  
	  run_frag += v;
	  run -= v;
	  while(run_frag >= cpi->coded_total){
	    run_coeff++;
	    run_frag -= cpi->coded_total;
	  }
	}

	run_frag = cpi->coded_total - cpi->dct_eob_run[i];       
      }
      run += cpi->dct_eob_run[i];

    }
    
    /* tie off last run */
    while(run){
      int v = (run < 4095 ? run : 4095);
      int plane = (run_frag >= cpi->coded_y);
      emit_eob_run(cpi, plane, run_coeff, v,0);
      run_frag += v;
      run -= v;
      while(run_frag >= cpi->coded_total){
	run_coeff++;
	run_frag -= cpi->coded_total;
      }
    }
  }
}

static int AllZeroDctData( ogg_int16_t * QuantList ){
  ogg_uint32_t i;

  for ( i = 0; i < 64; i ++ )
    if ( QuantList[i] != 0 )
      return 0;
  
  return 1;
}

static int ModeUsesMC[MAX_MODES] = { 0, 0, 1, 1, 1, 0, 1, 1 };

static void BlockUpdateDifference (CP_INSTANCE * cpi, 
				   unsigned char *FiltPtr,
				   ogg_int16_t *DctInputPtr, 
				   unsigned char *thisrecon,
				   ogg_int32_t MvDevisor,
				   fragment_t *fp,
				   ogg_uint32_t PixelsPerLine,
				   int mode) {

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
  mv_t mv;

  if ( ModeUsesMC[mode] ){
    switch(MvDevisor) {
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
    
    mv = fp->mv;
    
    /* Set up the baseline offset for the motion vector. */
    MVOffset = ((mv.y / MvDevisor) * PixelsPerLine) + (mv.x / MvDevisor);
    
    /* Work out the offset of the second reference position for 1/2
       pixel interpolation.  For the U and V planes the MV specifies 1/4
       pixel accuracy. This is adjusted to 1/2 pixel as follows ( 0->0,
       1/4->1/2, 1/2->1/2, 3/4->1/2 ). */
    ReconPtr2Offset = 0;
    AbsXOffset = mv.x % MvDevisor;
    AbsYOffset = mv.y % MvDevisor;
    
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
      ReconPtr1 = &cpi->golden[fp->buffer_index];
    } else {
      ReconPtr1 = &cpi->lastrecon[fp->buffer_index];
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
	ReconPtr1 = &cpi->lastrecon[fp->buffer_index];
      } else {
	ReconPtr1 = &cpi->golden[fp->buffer_index];
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
			     fragment_t *fp){

  unsigned char *FiltPtr = &cpi->frame[fp->buffer_index];
  int qi = cpi->BaseQ; // temporary
  int inter = (fp->mode != CODE_INTRA);
  int plane = (fp < cpi->frag[1] ? 0 : (fp < cpi->frag[2] ? 1 : 2)); 
  ogg_int32_t *q = cpi->iquant_tables[inter][plane][qi];
  ogg_int16_t DCTInput[64];
  ogg_int16_t DCTOutput[64];
  ogg_int32_t   MvDivisor;      /* Defines MV resolution (2 = 1/2
                                   pixel for Y or 4 = 1/4 for UV) */
  unsigned char   *ReconPtr1 = &cpi->recon[fp->buffer_index];

  /* Set plane specific values */
  if (plane == 0){
    MvDivisor = 2;                  /* 1/2 pixel accuracy in Y */
  }else{
    MvDivisor = 4;                  /* UV planes at 1/2 resolution of Y */
  }

  /* produces the appropriate motion compensation block, applies it to
     the reconstruction buffer, and proces a difference block for
     forward DCT */
  BlockUpdateDifference(cpi, FiltPtr, DCTInput, ReconPtr1,
			MvDivisor, fp, cpi->stride[plane], fp->mode);
  
  /* Proceed to encode the data into the encode buffer if the encoder
     is enabled. */
  /* Perform a 2D DCT transform on the data. */
  dsp_fdct_short(cpi->dsp, DCTInput, DCTOutput);

  /* Quantize that transform data. */
  quantize (cpi, q, DCTOutput, fp->dct);

  if ( (fp->mode == CODE_INTER_NO_MV) &&
       ( AllZeroDctData(fp->dct) ) ) {
    fp->coded = 0;
  }

}
