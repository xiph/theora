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

/* plane == 0 for Y, 1 for UV */
static void add_token(CP_INSTANCE *cpi, int plane, int coeff, 
		      unsigned char token, ogg_uint16_t eb){

  cpi->dct_token[coeff][cpi->dct_token_count[coeff]] = token;
  cpi->dct_token_eb[coeff][cpi->dct_token_count[coeff]] = eb;
  cpi->dct_token_count[coeff]++;

  if(coeff == 0){
    /* DC */
    int i;
    for ( i = 0; i < DC_HUFF_CHOICES; i++)
      cpi->dc_bits[plane][i] += cpi->HuffCodeLengthArray_VP3x[i][token];
  }else{
    /* AC */
    int i,offset = acoffset[coeff];
    for ( i = 0; i < AC_HUFF_CHOICES; i++)
      cpi->ac_bits[plane][i] += cpi->HuffCodeLengthArray_VP3x[offset+i][token];
  }

  if(!plane)cpi->dct_token_ycount[coeff]++;
}

static void prepend_token(CP_INSTANCE *cpi, int plane, int coeff, 
			  unsigned char token, ogg_uint16_t eb){

  cpi->dct_token[coeff]--;
  cpi->dct_token_eb[coeff]--;
  cpi->dct_token[coeff][0] = token;
  cpi->dct_token_eb[coeff][0] = eb;
  cpi->dct_token_count[coeff]++;

  if(coeff == 0){
    /* DC */
    int i;
    for ( i = 0; i < DC_HUFF_CHOICES; i++)
      cpi->dc_bits[plane][i] += cpi->HuffCodeLengthArray_VP3x[i][token];
  }else{
    /* AC */
    int i,offset = acoffset[coeff];
    for ( i = 0; i < AC_HUFF_CHOICES; i++)
      cpi->ac_bits[plane][i] += cpi->HuffCodeLengthArray_VP3x[offset+i][token];
  }

  if(!plane)cpi->dct_token_ycount[coeff]++;
}

static void emit_eob_run(CP_INSTANCE *cpi, int plane, int pos, int run){
  if ( run <= 3 ) {
    if ( run == 1 ) {
      add_token(cpi, plane, pos, DCT_EOB_TOKEN, 0);
    } else if ( run == 2 ) {
      add_token(cpi, plane, pos, DCT_EOB_PAIR_TOKEN, 0);
    } else {
      add_token(cpi, plane, pos, DCT_EOB_TRIPLE_TOKEN, 0);
    }
    
  } else {
    
    if ( run < 8 ) {
      add_token(cpi, plane, pos, DCT_REPEAT_RUN_TOKEN, run-4);
    } else if ( run < 16 ) {
      add_token(cpi, plane, pos, DCT_REPEAT_RUN2_TOKEN, run-8);
    } else if ( run < 32 ) {
      add_token(cpi, plane, pos, DCT_REPEAT_RUN3_TOKEN, run-16);
    } else if ( run < 4096) {
      add_token(cpi, plane, pos, DCT_REPEAT_RUN4_TOKEN, run);
    }
  }
}

static void prepend_eob_run(CP_INSTANCE *cpi, int plane, int pos, int run){
  if ( run <= 3 ) {
    if ( run == 1 ) {
      prepend_token(cpi, plane, pos, DCT_EOB_TOKEN, 0);
    } else if ( run == 2 ) {
      prepend_token(cpi, plane, pos, DCT_EOB_PAIR_TOKEN, 0);
    } else {
      prepend_token(cpi, plane, pos, DCT_EOB_TRIPLE_TOKEN, 0);
    }
    
  } else {
    
    if ( run < 8 ) {
      prepend_token(cpi, plane, pos, DCT_REPEAT_RUN_TOKEN, run-4);
    } else if ( run < 16 ) {
      prepend_token(cpi, plane, pos, DCT_REPEAT_RUN2_TOKEN, run-8);
    } else if ( run < 32 ) {
      prepend_token(cpi, plane, pos, DCT_REPEAT_RUN3_TOKEN, run-16);
    } else if ( run < 4096) {
      prepend_token(cpi, plane, pos, DCT_REPEAT_RUN4_TOKEN, run);
    }
  }
}

static void TokenizeDctValue (CP_INSTANCE *cpi, 
			      int plane, 
			      int coeff,
			      ogg_int16_t DataValue){

  int AbsDataVal = abs(DataValue);
  int neg = (DataValue<0);

  if ( AbsDataVal == 1 ){

    add_token(cpi, plane, coeff, (neg ? MINUS_ONE_TOKEN : ONE_TOKEN), 0);

  } else if ( AbsDataVal == 2 ) {

    add_token(cpi, plane, coeff, (neg ? MINUS_TWO_TOKEN : TWO_TOKEN), 0);

  } else if ( AbsDataVal <= MAX_SINGLE_TOKEN_VALUE ) {

    add_token(cpi, plane, coeff, LOW_VAL_TOKENS + (AbsDataVal - DCT_VAL_CAT2_MIN), neg);

  } else if ( AbsDataVal <= 8 ) {

    add_token(cpi, plane, coeff, DCT_VAL_CATEGORY3, (AbsDataVal - DCT_VAL_CAT3_MIN) + (neg << 1));

  } else if ( AbsDataVal <= 12 ) {

    add_token(cpi, plane, coeff, DCT_VAL_CATEGORY4, (AbsDataVal - DCT_VAL_CAT4_MIN) + (neg << 2));

  } else if ( AbsDataVal <= 20 ) {

    add_token(cpi, plane, coeff, DCT_VAL_CATEGORY5, (AbsDataVal - DCT_VAL_CAT5_MIN) + (neg << 3));

  } else if ( AbsDataVal <= 36 ) {

    add_token(cpi, plane, coeff, DCT_VAL_CATEGORY6, (AbsDataVal - DCT_VAL_CAT6_MIN) + (neg << 4));

  } else if ( AbsDataVal <= 68 ) {

    add_token(cpi, plane, coeff, DCT_VAL_CATEGORY7, (AbsDataVal - DCT_VAL_CAT7_MIN) + (neg << 5));

  } else {

    add_token(cpi, plane, coeff, DCT_VAL_CATEGORY8, (AbsDataVal - DCT_VAL_CAT8_MIN) + (neg << 9));

  } 
}

static void TokenizeDctRunValue (CP_INSTANCE *cpi, 
				 int plane, 
				 int coeff,
				 unsigned char RunLength,
				 ogg_int16_t DataValue){

  ogg_uint32_t AbsDataVal = abs( (ogg_int32_t)DataValue );
  int neg = (DataValue<0);

  /* Values are tokenised as category value and a number of additional
     bits  that define the category.  */

  if ( AbsDataVal == 1 ) {

    if ( RunLength <= 5 ) 
      add_token(cpi,plane,coeff, DCT_RUN_CATEGORY1 + RunLength - 1, neg);
    else if ( RunLength <= 9 ) 
      add_token(cpi,plane,coeff, DCT_RUN_CATEGORY1B, RunLength - 6 + (neg<<2));
    else 
      add_token(cpi,plane,coeff, DCT_RUN_CATEGORY1C, RunLength - 10 + (neg<<3));

  } else if ( AbsDataVal <= 3 ) {

    if ( RunLength == 1 ) 
      add_token(cpi,plane,coeff, DCT_RUN_CATEGORY2, AbsDataVal - 2 + (neg<<1));
    else
      add_token(cpi,plane,coeff, DCT_RUN_CATEGORY2B, 
		(neg<<2) + ((AbsDataVal-2)<<1) + RunLength - 2);

  }
}

static void tokenize_groups(CP_INSTANCE *cpi,
			    int *eob_run, int *eob_plane, int *eob_ypre, int *eob_uvpre){
  fragment_t *fp = cpi->coded_tail;

  while(fp){
    int coeff = 0;
    int plane = (fp >= cpi->frag[1]);
    fp->nonzero = 0;

    while(coeff < BLOCK_SIZE){
      ogg_int16_t val = (coeff ? fp->dct[coeff] : fp->pred_dc);
      int zero_run;
      int i = coeff;
  
      fp->nonzero = coeff;
      
      while( !val && (++i < BLOCK_SIZE) )
	val = fp->dct[i];
      
      if ( i == BLOCK_SIZE ){

	/* if there are no other tokens in this group yet, set up to be
	   prepended later.  Group 0 is the exception (can't be
	   prepended) */
	if(cpi->dct_token_count[coeff] == 0 && coeff){
	  if(!plane)
	    eob_ypre[coeff]++;
	  else
	    eob_uvpre[coeff]++;
	}else{
	  if(eob_run[coeff] == 4095){
	    emit_eob_run(cpi,eob_plane[coeff],coeff,4095);
	    eob_run[coeff] = 0;
	  }
	  
	  if(eob_run[coeff]==0)
	    eob_plane[coeff]=plane;
	  
	  eob_run[coeff]++;
	}
	coeff = BLOCK_SIZE;
      }else{
	
	if(eob_run[coeff]){
	  emit_eob_run(cpi,eob_plane[coeff],coeff,eob_run[coeff]);
	  eob_run[coeff]=0;
	}
	
	zero_run = i-coeff;
	if (zero_run){
	  ogg_uint32_t absval = abs(val);
	  if ( ((absval == 1) && (zero_run <= 17)) ||
	       ((absval <= 3) && (zero_run <= 3)) ) {
	    TokenizeDctRunValue( cpi, plane, coeff, zero_run, val);
	    coeff = i+1;
	  }else{
	    if ( zero_run <= 8 )
	      add_token(cpi, plane, coeff, DCT_SHORT_ZRL_TOKEN, zero_run - 1);
	    else
	      add_token(cpi, plane, coeff, DCT_ZRL_TOKEN, zero_run - 1);
	    coeff = i;
	  }
	}else{
	  TokenizeDctValue( cpi, plane, coeff, val );
	  coeff = i+1;
	}
      }
    }
    fp=fp->next;
  }
}

void DPCMTokenize (CP_INSTANCE *cpi){
  int eob_run[64];
  int eob_plane[64];

  int eob_ypre[64];
  int eob_uvpre[64];
  
  int i;

  memset(eob_run, 0, sizeof(eob_run));
  memset(eob_ypre, 0, sizeof(eob_ypre));
  memset(eob_uvpre, 0, sizeof(eob_uvpre));
  memset(cpi->dc_bits, 0, sizeof(cpi->dc_bits));
  memset(cpi->ac_bits, 0, sizeof(cpi->ac_bits));
  memset(cpi->dct_token_count, 0, sizeof(cpi->dct_token_count));
  memset(cpi->dct_token_ycount, 0, sizeof(cpi->dct_token_ycount));

  for(i=0;i<BLOCK_SIZE;i++){
    cpi->dct_token[i] = cpi->dct_token_storage+cpi->frag_total*i;
    cpi->dct_token_eb[i] = cpi->dct_token_eb_storage+cpi->frag_total*i;
  }

  /* Tokenize the dct data. */
  tokenize_groups (cpi, eob_run, eob_plane, eob_ypre, eob_uvpre);
  
  /* tie together eob runs at the beginnings/ends of coeff groups */
  {
    int coeff = 0;
    int run = 0;
    int plane = 0;

    for(i=0;i<BLOCK_SIZE;i++){

      if(eob_ypre[i] || eob_uvpre[i]){
	/* group begins with an EOB run */
	
	if(run && run + eob_ypre[i] >= 4095){
	  emit_eob_run(cpi,plane,coeff,4095);
	  eob_ypre[i] -= 4095-run; 
	  run = 0;
	  coeff = i;
	  plane = (eob_ypre[i] ? 0 : 1);
	}
	
	if(run && run + eob_ypre[i] + eob_uvpre[i] >= 4095){
	  emit_eob_run(cpi,plane,coeff,4095);
	  eob_uvpre[i] -= 4095 - eob_ypre[i] - run;
	  eob_ypre[i] = 0;
	  run = 0;
	  coeff = i;
	  plane = 1;
	}
	
	if(run){
	  if(cpi->dct_token_count[i]){
	    /* group is not only an EOB run; emit the run token */
	    emit_eob_run(cpi,plane,coeff,run + eob_ypre[i] + eob_uvpre[i]);
	    eob_ypre[i] = 0;
	    eob_uvpre[i] = 0;
	    run = eob_run[i];
	    coeff = i;
	    plane = eob_plane[i];
	  }else{
	    /* group consists entirely of EOB run.  Add, iterate */
	    run += eob_ypre[i];
	    run += eob_uvpre[i];
	    eob_ypre[i] = 0;
	    eob_uvpre[i] = 0;
	  }
	}else{
	  
	  if(cpi->dct_token_count[i]){
	    /* there are other tokens in this group; work backwards as we need to prepend */
	    while(eob_uvpre[i] >= 4095){
	      prepend_eob_run(cpi,1,i,4095);
	      eob_uvpre[i] -= 4095;
	    }
	    while(eob_uvpre[i] + eob_ypre[i] >= 4095){
	      prepend_eob_run(cpi,0,i,4095);
	      eob_ypre[i] -= 4095 - eob_uvpre[i];
	      eob_uvpre[i] = 0;
	    }
	    if(eob_uvpre[i] + eob_ypre[i]){
	      prepend_eob_run(cpi, (eob_ypre[i] ? 0 : 1), i, eob_ypre[i] + eob_uvpre[i]);
	      eob_ypre[i] = 0;
	      eob_uvpre[i] = 0;
	    }
	    run = eob_run[i];
	    coeff = i;
	    plane = eob_plane[i];
	  }else{
	    /* group consists entirely of EOB run.  Add, flush overs, iterate */
	    while(eob_ypre[i] >= 4095){
	      emit_eob_run(cpi,0,i,4095);
	      eob_ypre[i] -= 4095;
	    }
	    while(eob_uvpre[i] + eob_ypre[i] >= 4095){
	      emit_eob_run(cpi,(eob_ypre[i] ? 0 : 1), i, 4095);
	      eob_uvpre[i] -= 4095 - eob_ypre[i];
	      eob_ypre[i] = 0;
	    }
	    run = eob_uvpre[i]+eob_ypre[i];
	    coeff = i;
	    plane = (eob_ypre[i] ? 0 : 1);
	  }
	}
      }else{
	/* no eob run to begin group */
	if(cpi->dct_token_count[i]){
	  if(run)
	    emit_eob_run(cpi,plane,coeff,run);
	  
	  run = eob_run[i];
	  coeff = i;
	  plane = eob_plane[i];
	}
      }
    }
    
    if(run)
      emit_eob_run(cpi,plane,coeff,run);
    
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
			     int fi){
  
  fragment_t *fp = &cpi->frag[0][fi];
  coding_mode_t mode = cpi->frag_mode[0][fi];
  unsigned char *cp = &cpi->frag_coded[0][fi];

  unsigned char *FiltPtr = &cpi->frame[fp->buffer_index];
  int qi = cpi->BaseQ; // temporary
  int inter = (mode != CODE_INTRA);
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
			MvDivisor, fp, cpi->stride[plane], mode);
  
  /* Proceed to encode the data into the encode buffer if the encoder
     is enabled. */
  /* Perform a 2D DCT transform on the data. */
  dsp_fdct_short(cpi->dsp, DCTInput, DCTOutput);

  /* Quantize that transform data. */
  quantize (cpi, q, DCTOutput, fp->dct);

  if ( (mode == CODE_INTER_NO_MV) &&
       ( AllZeroDctData(fp->dct) ) ) {
    *cp = 0;
  }

}
