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

static void add_token(CP_INSTANCE *cpi, int chroma, int coeff, 
		      unsigned char token, ogg_uint16_t eb, int fi){
  
  cpi->dct_token[coeff][cpi->dct_token_count[coeff]] = token;
  cpi->dct_token_eb[coeff][cpi->dct_token_count[coeff]] = eb;
#ifdef COLLECT_METRICS
  cpi->dct_token_frag[coeff][cpi->dct_token_count[coeff]] = fi;
#endif
  if(!chroma)cpi->dct_token_ycount[coeff]++;
  cpi->dct_token_count[coeff]++;

  if(coeff == 0){
    /* DC */
    int i;
    for ( i = 0; i < DC_HUFF_CHOICES; i++)
      cpi->dc_bits[chroma][i] += cpi->HuffCodeLengthArray_VP3x[i][token];
  }else{
    /* AC */
    int i,offset = acoffset[coeff];
    for ( i = 0; i < AC_HUFF_CHOICES; i++)
      cpi->ac_bits[chroma][i] += cpi->HuffCodeLengthArray_VP3x[offset+i][token];
  }
}

static void prepend_token(CP_INSTANCE *cpi, int chroma, int coeff, 
			  unsigned char token, ogg_uint16_t eb, int fi){

  cpi->dct_token[coeff]--;
  cpi->dct_token_eb[coeff]--;
#ifdef COLLECT_METRICS
  cpi->dct_token_frag[coeff]--;
  cpi->dct_token_frag[coeff][0] = fi;
#endif
  cpi->dct_token[coeff][0] = token;
  cpi->dct_token_eb[coeff][0] = eb;
  cpi->dct_token_count[coeff]++;
  if(!chroma)cpi->dct_token_ycount[coeff]++;

  if(coeff == 0){
    /* DC */
    int i;
    for ( i = 0; i < DC_HUFF_CHOICES; i++)
      cpi->dc_bits[chroma][i] += cpi->HuffCodeLengthArray_VP3x[i][token];
  }else{
    /* AC */
    int i,offset = acoffset[coeff];
    for ( i = 0; i < AC_HUFF_CHOICES; i++)
      cpi->ac_bits[chroma][i] += cpi->HuffCodeLengthArray_VP3x[offset+i][token];
  }
}

static void emit_eob_run(CP_INSTANCE *cpi, int chroma, int pos, int run){
  if ( run <= 3 ) {
    if ( run == 1 ) {
      add_token(cpi, chroma, pos, DCT_EOB_TOKEN, 0, 0);
    } else if ( run == 2 ) {
      add_token(cpi, chroma, pos, DCT_EOB_PAIR_TOKEN, 0, 0);
    } else {
      add_token(cpi, chroma, pos, DCT_EOB_TRIPLE_TOKEN, 0, 0);
    }
    
  } else {
    
    if ( run < 8 ) {
      add_token(cpi, chroma, pos, DCT_REPEAT_RUN_TOKEN, run-4, 0);
    } else if ( run < 16 ) {
      add_token(cpi, chroma, pos, DCT_REPEAT_RUN2_TOKEN, run-8, 0);
    } else if ( run < 32 ) {
      add_token(cpi, chroma, pos, DCT_REPEAT_RUN3_TOKEN, run-16, 0);
    } else if ( run < 4096) {
      add_token(cpi, chroma, pos, DCT_REPEAT_RUN4_TOKEN, run, 0);
    }
  }
}

static void prepend_eob_run(CP_INSTANCE *cpi, int chroma, int pos, int run){
  if ( run <= 3 ) {
    if ( run == 1 ) {
      prepend_token(cpi, chroma, pos, DCT_EOB_TOKEN, 0, 0);
    } else if ( run == 2 ) {
      prepend_token(cpi, chroma, pos, DCT_EOB_PAIR_TOKEN, 0, 0);
    } else {
      prepend_token(cpi, chroma, pos, DCT_EOB_TRIPLE_TOKEN, 0, 0);
    }
    
  } else {
    
    if ( run < 8 ) {
      prepend_token(cpi, chroma, pos, DCT_REPEAT_RUN_TOKEN, run-4, 0);
    } else if ( run < 16 ) {
      prepend_token(cpi, chroma, pos, DCT_REPEAT_RUN2_TOKEN, run-8, 0);
    } else if ( run < 32 ) {
      prepend_token(cpi, chroma, pos, DCT_REPEAT_RUN3_TOKEN, run-16, 0);
    } else if ( run < 4096) {
      prepend_token(cpi, chroma, pos, DCT_REPEAT_RUN4_TOKEN, run, 0);
    }
  }
}

static void TokenizeDctValue (CP_INSTANCE *cpi, 
			      int chroma, 
			      int coeff,
			      ogg_int16_t DataValue,
			      int fi){

  int AbsDataVal = abs(DataValue);
  int neg = (DataValue<0);

  if ( AbsDataVal == 1 ){

    add_token(cpi, chroma, coeff, (neg ? MINUS_ONE_TOKEN : ONE_TOKEN), 0, fi);

  } else if ( AbsDataVal == 2 ) {

    add_token(cpi, chroma, coeff, (neg ? MINUS_TWO_TOKEN : TWO_TOKEN), 0, fi);

  } else if ( AbsDataVal <= MAX_SINGLE_TOKEN_VALUE ) {

    add_token(cpi, chroma, coeff, LOW_VAL_TOKENS + (AbsDataVal - DCT_VAL_CAT2_MIN), neg, fi);

  } else if ( AbsDataVal <= 8 ) {

    add_token(cpi, chroma, coeff, DCT_VAL_CATEGORY3, (AbsDataVal - DCT_VAL_CAT3_MIN) + (neg << 1), fi);

  } else if ( AbsDataVal <= 12 ) {

    add_token(cpi, chroma, coeff, DCT_VAL_CATEGORY4, (AbsDataVal - DCT_VAL_CAT4_MIN) + (neg << 2), fi);

  } else if ( AbsDataVal <= 20 ) {

    add_token(cpi, chroma, coeff, DCT_VAL_CATEGORY5, (AbsDataVal - DCT_VAL_CAT5_MIN) + (neg << 3), fi);

  } else if ( AbsDataVal <= 36 ) {

    add_token(cpi, chroma, coeff, DCT_VAL_CATEGORY6, (AbsDataVal - DCT_VAL_CAT6_MIN) + (neg << 4), fi);

  } else if ( AbsDataVal <= 68 ) {

    add_token(cpi, chroma, coeff, DCT_VAL_CATEGORY7, (AbsDataVal - DCT_VAL_CAT7_MIN) + (neg << 5), fi);

  } else {

    add_token(cpi, chroma, coeff, DCT_VAL_CATEGORY8, (AbsDataVal - DCT_VAL_CAT8_MIN) + (neg << 9), fi);

  } 
}

static void TokenizeDctRunValue (CP_INSTANCE *cpi, 
				 int chroma, 
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
      add_token(cpi,chroma,coeff, DCT_RUN_CATEGORY1 + RunLength - 1, neg, fi);
    else if ( RunLength <= 9 ) 
      add_token(cpi,chroma,coeff, DCT_RUN_CATEGORY1B, RunLength - 6 + (neg<<2), fi);
    else 
      add_token(cpi,chroma,coeff, DCT_RUN_CATEGORY1C, RunLength - 10 + (neg<<3), fi);

  } else if ( AbsDataVal <= 3 ) {

    if ( RunLength == 1 ) 
      add_token(cpi,chroma,coeff, DCT_RUN_CATEGORY2, AbsDataVal - 2 + (neg<<1), fi);
    else
      add_token(cpi,chroma,coeff, DCT_RUN_CATEGORY2B, 
		(neg<<2) + ((AbsDataVal-2)<<1) + RunLength - 2, fi);

  }
}

static void tokenize_block(CP_INSTANCE *cpi, int fi, int chroma,
			   int *eob_ypre, int *eob_pre,
			   int *eob_yrun, int *eob_run){

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
	   prepended later.  Group 0 is the exception (can't be
	   prepended) */
	if(cpi->dct_token_count[coeff] == 0 && coeff){
	  /* prepending requires space to do so-- save some at front of token stack */
	  if(eob_pre[coeff]==0 || (eob_pre[coeff]&0x8ff)==0x8ff){ /* 0xfff is a safe overallocation, 
								     saves a mod 4095 */
	    cpi->dct_token[coeff]++;
	    cpi->dct_token_eb[coeff]++;
#ifdef COLLECT_METRICS
	    cpi->dct_token_frag[coeff]++;
#endif
	  }

	  /* finally, track pre-run */
	  eob_pre[coeff]++;
	  if(!chroma)eob_ypre[coeff]++;

#ifdef COLLECT_METRICS
	  cpi->dct_eob_fi_stack[coeff][cpi->dct_eob_fi_count[coeff]++]=fi;
#endif
	}else{
	  if(eob_run[coeff] == 4095){
	    emit_eob_run(cpi,(eob_yrun[coeff]==0),coeff,4095);
	    eob_run[coeff] = 0;
	    eob_yrun[coeff] = 0;
	  }
	  
	  eob_run[coeff]++;
	  if(!chroma)eob_yrun[coeff]++;

#ifdef COLLECT_METRICS
	  cpi->dct_eob_fi_stack[coeff][cpi->dct_eob_fi_count[coeff]++]=fi;
#endif
	}
	coeff = BLOCK_SIZE;
      }else{
	
	if(eob_run[coeff]){
	  emit_eob_run(cpi,(eob_yrun[coeff]==0),coeff,eob_run[coeff]);
	  eob_run[coeff]=0;
	  eob_yrun[coeff]=0;
	}
	
	zero_run = i-coeff;
	if (zero_run){
	  ogg_uint32_t absval = abs(val);
	  if ( ((absval == 1) && (zero_run <= 17)) ||
	       ((absval <= 3) && (zero_run <= 3)) ) {
	    TokenizeDctRunValue( cpi, chroma, coeff, zero_run, val, fi);
	    coeff = i+1;
	  }else{
	    if ( zero_run <= 8 )
	      add_token(cpi, chroma, coeff, DCT_SHORT_ZRL_TOKEN, zero_run - 1, fi);
	    else
	      add_token(cpi, chroma, coeff, DCT_ZRL_TOKEN, zero_run - 1, fi);
	    coeff = i;
	  }
	}else{
	  TokenizeDctValue(cpi, chroma, coeff, val, fi);
	  coeff = i+1;
	}
      }
    }
  }
}

void DPCMTokenize (CP_INSTANCE *cpi){
  int eob_run[64];
  int eob_pre[64];
  int eob_yrun[64];
  int eob_ypre[64];
  
  int i,sbi;

  memset(eob_run, 0, sizeof(eob_run));
  memset(eob_pre, 0, sizeof(eob_pre));
  memset(eob_yrun, 0, sizeof(eob_yrun));
  memset(eob_ypre, 0, sizeof(eob_ypre));
  memset(cpi->dc_bits, 0, sizeof(cpi->dc_bits));
  memset(cpi->ac_bits, 0, sizeof(cpi->ac_bits));
  memset(cpi->dct_token_count, 0, sizeof(cpi->dct_token_count));
  memset(cpi->dct_token_ycount, 0, sizeof(cpi->dct_token_ycount));
#ifdef COLLECT_METRICS
  memset(cpi->dct_eob_fi_count, 0, sizeof(cpi->dct_eob_fi_count));
#endif

  for(i=0;i<BLOCK_SIZE;i++){
    cpi->dct_token[i] = cpi->dct_token_storage+cpi->frag_total*i;
    cpi->dct_token_eb[i] = cpi->dct_token_eb_storage+cpi->frag_total*i;

#ifdef COLLECT_METRICS
    cpi->dct_eob_fi_stack[i] = cpi->dct_eob_fi_storage+cpi->frag_total*i;
    cpi->dct_token_frag[i] = cpi->dct_token_frag_storage+cpi->frag_total*i;
#endif
  }

  
  for (sbi=0; sbi < cpi->super_n[0]; sbi++ ){
    superblock_t *sb = &cpi->super[0][sbi];
    int bi;
    for (bi=0; bi<16; bi++ ) {
      int fi = sb->f[bi];
      tokenize_block(cpi, fi, 0, eob_ypre, eob_pre, eob_yrun, eob_run);
    }
  }

  for (; sbi < cpi->super_total; sbi++ ){
    superblock_t *sb = &cpi->super[0][sbi];
    int bi;
    for (bi=0; bi<16; bi++ ) {
      int fi = sb->f[bi];
      tokenize_block(cpi, fi, 1, eob_ypre, eob_pre, eob_yrun, eob_run);
    }
  }

  /* tie together eob runs at the beginnings/ends of coeff groups */
  {
    int coeff = 0;
    int run = 0;
    int chroma = 0; /* not the current plane; plane of next run token
		       to emit */

    for(i=0;i<BLOCK_SIZE;i++){
      if(eob_pre[i]){
	/* group begins with an EOB run */
	
	/* special case the ongoing run + eob is at or over the max run size;
	   we know the ongoing run is < 4095 or it would have been flushed already. */
	if(run && run + eob_pre[i] >= 4095){ /* 1 */
	  emit_eob_run(cpi,chroma,coeff,4095);
	  eob_pre[i] -= 4095-run; 
	  eob_ypre[i] -= 4095-run; 
	  run = 0;
	  coeff = i;
	  chroma = (eob_ypre[i]<=0);
	  if(eob_ypre[i]<0)eob_ypre[i]=0;
	}
	
	if(run){
	  if(cpi->dct_token_count[i]){ /* 2 */
	    /* group is not only an EOB run; emit the run token */
	    emit_eob_run(cpi,chroma,coeff,run + eob_pre[i]);
	    eob_ypre[i] = 0;
	    eob_pre[i] = 0;
	    run = eob_run[i];
	    coeff = i;
	    chroma = (eob_yrun[i]<=0);
	  }else{ /* 3 */
	    /* group consists entirely of EOB run.  Add, iterate */
	    run += eob_pre[i];
	    eob_pre[i] = 0;
	    eob_ypre[i] = 0;
	  }
	}else{
	    
	  if(cpi->dct_token_count[i]){
	    /* there are other tokens in this group; work backwards as we need to prepend */
	    while(eob_pre[i] >= 4095){ /* 4 */
	      int lchroma = (eob_pre[i]-4095 >= eob_ypre[i]);
	      prepend_eob_run(cpi,lchroma,i,4095);
	      eob_pre[i] -= 4095;
	    }
	    if(eob_pre[i]){ /* 5 */
	      int lchroma = (eob_ypre[i]<=0); /* possible when case 1 triggered */
	      prepend_eob_run(cpi, lchroma, i, eob_pre[i]);
	      eob_pre[i] = 0;
	      eob_ypre[i] = 0;
	    }
	    run = eob_run[i];
	    coeff = i;
	    chroma = (eob_yrun[i]<=0);
	  }else{
	    /* group consists entirely of EOB run.  Add, flush overs, iterate */
	    while(eob_pre[i] >= 4095){
	      int lchroma = (eob_ypre[i]<=0);
	      emit_eob_run(cpi,lchroma,i,4095);
	      eob_pre[i] -= 4095;
	      eob_ypre[i] -= 4095;
	    }
	    run = eob_pre[i];
	    coeff = i;
	    chroma = (eob_ypre[i]<=0);
	  }
	}
      }else{
	/* no eob run to begin group */
	if(cpi->dct_token_count[i]){
	  if(run)
	    emit_eob_run(cpi,chroma,coeff,run);
	  
	  run = eob_run[i];
	  coeff = i;
	  chroma = (eob_yrun[i]<=0);
	}
      }
    }
    
    if(run)
      emit_eob_run(cpi,chroma,coeff,run);
    
  }
}
