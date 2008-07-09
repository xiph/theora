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

typedef struct {
  int coeff;
  int count;
  int chroma;
  int pre;
  int run;
} token_checkpoint_t;

void tokenize_rollback(CP_INSTANCE *cpi, token_checkpoint_t *stack,int n){
  int i;
  for(i=n-1;i>=0;i--){
    int coeff = stack[i].coeff;
    cpi->dct_token_count[coeff] = stack[i].count; 
    cpi->eob_run[coeff] = stack[i].run;
    cpi->eob_pre[coeff] = stack[i].pre;
  }
}

void tokenize_commit(CP_INSTANCE *cpi, token_checkpoint_t *stack, int n){
  int i;
  for(i=0;i<n;i++){
    int coeff = stack[i].coeff;
    int pos = stack[i].count;
    int token = cpi->dct_token[coeff][pos];
    int chroma = stack[i].chroma;

    if (coeff == 1){
      /* AC == 1*/
    int i,offset = acoffset[1];
    for ( i = 0; i < AC_HUFF_CHOICES; i++)
      cpi->ac1_bits[chroma][i] += cpi->HuffCodeLengthArray_VP3x[offset+i][token];
    }else{
      /* AC > 1*/
      int i,offset = acoffset[coeff];
      for ( i = 0; i < AC_HUFF_CHOICES; i++)
	cpi->acN_bits[chroma][i] += cpi->HuffCodeLengthArray_VP3x[offset+i][token];
    }
  }
}

static void add_token(CP_INSTANCE *cpi, int chroma, int coeff, 
		      unsigned char token, ogg_uint16_t eb){
  cpi->dct_token[coeff][cpi->dct_token_count[coeff]] = token;
  cpi->dct_token_eb[coeff][cpi->dct_token_count[coeff]] = eb;
  cpi->dct_token_count[coeff]++;

  /* coeff 0 (DC) is always undconditionally/immediately committed */
  if(coeff == 0){
    /* DC */
    int i;
    for ( i = 0; i < DC_HUFF_CHOICES; i++)
      cpi->dc_bits[chroma][i] += cpi->HuffCodeLengthArray_VP3x[i][token];
  }else if (coeff == 1){
    /* AC == 1*/
    int i,offset = acoffset[1];
    for ( i = 0; i < AC_HUFF_CHOICES; i++)
      cpi->ac1_bits[chroma][i] += cpi->HuffCodeLengthArray_VP3x[offset+i][token];
  }else{
    /* AC > 1*/
    int i,offset = acoffset[coeff];
    for ( i = 0; i < AC_HUFF_CHOICES; i++)
      cpi->acN_bits[chroma][i] += cpi->HuffCodeLengthArray_VP3x[offset+i][token];
  }

}

static void prepend_token(CP_INSTANCE *cpi, int chroma, int coeff, 
			  unsigned char token, ogg_uint16_t eb){

  cpi->dct_token[coeff]--;
  cpi->dct_token_eb[coeff]--;
#ifdef COLLECT_METRICS
  cpi->dct_token_frag[coeff]--;
#endif
  cpi->dct_token[coeff][0] = token;
  cpi->dct_token_eb[coeff][0] = eb;
  cpi->dct_token_count[coeff]++;

  if(coeff == 0){
    /* DC */
    int i;
    for ( i = 0; i < DC_HUFF_CHOICES; i++)
      cpi->dc_bits[chroma][i] += cpi->HuffCodeLengthArray_VP3x[i][token];
  }else if (coeff == 1){
    /* AC == 1*/
    int i,offset = acoffset[1];
    for ( i = 0; i < AC_HUFF_CHOICES; i++)
      cpi->ac1_bits[chroma][i] += cpi->HuffCodeLengthArray_VP3x[offset+i][token];
  }else{
    /* AC > 1*/
    int i,offset = acoffset[coeff];
    for ( i = 0; i < AC_HUFF_CHOICES; i++)
      cpi->acN_bits[chroma][i] += cpi->HuffCodeLengthArray_VP3x[offset+i][token];
  }
}

static void tokenize_eob_run(int run, int *token, int *eb){
  if ( run <= 3 ) {
    if ( run == 1 ) {
      *token = DCT_EOB_TOKEN;
    } else if ( run == 2 ) {
      *token = DCT_EOB_PAIR_TOKEN;
    } else {
      *token = DCT_EOB_TRIPLE_TOKEN;
    }
    *eb=0;
    
  } else {
    
    if ( run < 8 ) {
      *token = DCT_REPEAT_RUN_TOKEN;
      *eb = run-4;
    } else if ( run < 16 ) {
      *token = DCT_REPEAT_RUN2_TOKEN;
      *eb = run-8;
    } else if ( run < 32 ) {
      *token = DCT_REPEAT_RUN3_TOKEN;
      *eb = run-16;
    } else if ( run < 4096) {
      *token = DCT_REPEAT_RUN4_TOKEN;
      *eb = run;
    }
  }
}

static void add_eob_run(CP_INSTANCE *cpi, int pos, int run){
  int token=0,eb=0;
  int chroma = !(run&0x8000);
  tokenize_eob_run(run&0x7fff, &token, &eb);
  add_token(cpi, chroma, pos, token, eb);
}

static void prepend_eob_run(CP_INSTANCE *cpi, int chroma, int pos, int run){
  int token=0,eb=0;
  tokenize_eob_run(run, &token, &eb);
  prepend_token(cpi, chroma, pos, token, eb);
}

static void emit_raw(CP_INSTANCE *cpi, 
		     int chroma,
		     int fi,
		     int coeff,
		     int token,
		     int eb){
  
  /* Emit pending EOB run if any */
  if(cpi->eob_run[coeff]){
    add_eob_run(cpi,coeff,cpi->eob_run[coeff]);
    cpi->eob_run[coeff]=0;
  }
#ifdef COLLECT_METRICS
  cpi->dct_token_frag[coeff][cpi->dct_token_count[coeff]] = fi;
#endif
  add_token(cpi,chroma,coeff,token,eb);

}

static int emit_token(CP_INSTANCE *cpi, 
		      int chroma,
		      int fi,
		      int coeff,
		      int coeff2,
		      int val){
  
  
  ogg_uint32_t absval = abs(val);
  int neg = (val<0);
  int zero_run = coeff2-coeff;
  int ret = 1;
  int token;
  int eb=0;

  /* Emit pending EOB run if any */
  if(cpi->eob_run[coeff]){
    add_eob_run(cpi,coeff,cpi->eob_run[coeff]);
    cpi->eob_run[coeff]=0;
  }
#ifdef COLLECT_METRICS
  cpi->dct_token_frag[coeff][cpi->dct_token_count[coeff]] = fi;
#endif

  if (zero_run){
    int adj = (coeff!=1); /* implement a minor restriction on
			     stack 1 so that we know during DC
			     fixups that extended a dctrun token
			     from stack 1 will never overflow */
    if ((absval==1) && (zero_run<17+adj)){
      if ( zero_run <= 5 ) {
	token = DCT_RUN_CATEGORY1+zero_run-1; 
	eb    = neg;
      }else if ( zero_run <= 9 ) {
	token = DCT_RUN_CATEGORY1B; 
	eb    = zero_run-6+(neg<<2);
      }else{
	token = DCT_RUN_CATEGORY1C;
	eb    = zero_run-10+(neg<<3);
      }
    }else if((absval==2 || absval==3) && (zero_run < 3+adj)){
      if ( zero_run == 1 ) {
	token = DCT_RUN_CATEGORY2;
	eb    = absval-2+(neg<<1);
      }else{
	token = DCT_RUN_CATEGORY2B;
	eb    = (neg<<2)+((absval-2)<<1)+zero_run-2;
      }
    }else{
      if ( zero_run <= 8 )
	token = DCT_SHORT_ZRL_TOKEN;
      else
	token = DCT_ZRL_TOKEN;
      eb = zero_run-1;
      if(val) ret=0;
    }
  } else if ( absval == 1 ){
    token = (neg ? MINUS_ONE_TOKEN : ONE_TOKEN);
  } else if ( absval == 2 ) {
    token = (neg ? MINUS_TWO_TOKEN : TWO_TOKEN);
  } else if ( absval <= MAX_SINGLE_TOKEN_VALUE ) {
    token = LOW_VAL_TOKENS + (absval - DCT_VAL_CAT2_MIN);
    eb    = neg;
  } else if ( absval <= 8 ) {
    token = DCT_VAL_CATEGORY3;
    eb    = (absval - DCT_VAL_CAT3_MIN) + (neg << 1);
  } else if ( absval <= 12 ) {
    token = DCT_VAL_CATEGORY4;
    eb    = (absval - DCT_VAL_CAT4_MIN) + (neg << 2);
  } else if ( absval <= 20 ) {
    token = DCT_VAL_CATEGORY5;
    eb    = (absval - DCT_VAL_CAT5_MIN) + (neg << 3);
  } else if ( absval <= 36 ) {
    token = DCT_VAL_CATEGORY6;
    eb    = (absval - DCT_VAL_CAT6_MIN) + (neg << 4);
  } else if ( absval <= 68 ) {
    token = DCT_VAL_CATEGORY7;
    eb    = (absval - DCT_VAL_CAT7_MIN) + (neg << 5);
  } else {
    token = DCT_VAL_CATEGORY8;
    eb    = (absval - DCT_VAL_CAT8_MIN) + (neg << 9);
  } 

  add_token(cpi,chroma,coeff,token,eb);

  return ret;
}

static void increment_run(CP_INSTANCE *cpi, 
			  int chroma,
			  int fi,
			  int pre,
			  int coeff){

  if(pre && cpi->dct_token_count[coeff] == 0){
    /* track pre-run */
    cpi->eob_pre[coeff]++;
  }else{
    if((cpi->eob_run[coeff]&0x7fff) == 4095){
      add_eob_run(cpi,coeff,cpi->eob_run[coeff]);
      cpi->eob_run[coeff] = 0;
    }
    
    cpi->eob_run[coeff]++;
    cpi->eob_run[coeff]|= !chroma<<15;
  }	  
#ifdef COLLECT_METRICS
  cpi->dct_eob_fi_stack[coeff][cpi->dct_eob_fi_count[coeff]++]=fi;
#endif
}



static int decode_eob_token(int token, int eb){
  switch(token){
  case DCT_EOB_TOKEN:
    return 1;
  case DCT_EOB_PAIR_TOKEN:
    return 2; 
  case DCT_EOB_TRIPLE_TOKEN:
    return 3;
  case DCT_REPEAT_RUN_TOKEN:
    return eb+4;
  case DCT_REPEAT_RUN2_TOKEN:
    return eb+8;
  case DCT_REPEAT_RUN3_TOKEN:
    return eb+16;	
  case DCT_REPEAT_RUN4_TOKEN:
    return eb;
  default:
    return 0;
  }
}

static int decode_token(int token, int eb, int *val){
  switch(token){
  case DCT_SHORT_ZRL_TOKEN:
  case DCT_ZRL_TOKEN:
    *val=0;
    return eb+1;
  case ONE_TOKEN:
    *val = 1;
    return 0;
  case MINUS_ONE_TOKEN:
    *val = -1;
    return 0;
  case TWO_TOKEN:
    *val = 2;
    return 0;
  case MINUS_TWO_TOKEN:
    *val = -2;
    return 0;
  case LOW_VAL_TOKENS:
  case LOW_VAL_TOKENS+1:
  case LOW_VAL_TOKENS+2:
  case LOW_VAL_TOKENS+3:
    *val = (eb ? -(DCT_VAL_CAT2_MIN+token-LOW_VAL_TOKENS) : DCT_VAL_CAT2_MIN+token-LOW_VAL_TOKENS);
    return 0;
  case DCT_VAL_CATEGORY3:
    *val = ((eb & 0x2) ? -(DCT_VAL_CAT3_MIN+(eb&0x1)) : DCT_VAL_CAT3_MIN+(eb&0x1));
    return 0;
  case DCT_VAL_CATEGORY4:
    *val = ((eb & 0x4) ? -(DCT_VAL_CAT4_MIN+(eb&0x3)) : DCT_VAL_CAT4_MIN+(eb&0x3));
    return 0;
  case DCT_VAL_CATEGORY5:
    *val = ((eb & 0x8) ? -(DCT_VAL_CAT5_MIN+(eb&0x7)) : DCT_VAL_CAT5_MIN+(eb&0x7));
    return 0;
  case DCT_VAL_CATEGORY6:
    *val = ((eb & 0x10) ? -(DCT_VAL_CAT6_MIN+(eb&0xf)) : DCT_VAL_CAT6_MIN+(eb&0xf));
    return 0;
  case DCT_VAL_CATEGORY7:
    *val = ((eb & 0x20) ? -(DCT_VAL_CAT7_MIN+(eb&0x1f)) : DCT_VAL_CAT7_MIN+(eb&0x1f));
    return 0;
  case DCT_VAL_CATEGORY8:
    *val = ((eb & 0x200) ? -(DCT_VAL_CAT8_MIN+(eb&0x1ff)) : DCT_VAL_CAT8_MIN+(eb&0x1ff));
    return 0;
  case DCT_RUN_CATEGORY1:
  case DCT_RUN_CATEGORY1+1:
  case DCT_RUN_CATEGORY1+2:
  case DCT_RUN_CATEGORY1+3:
  case DCT_RUN_CATEGORY1+4:
    *val = (eb ? -1 : 1);
    return token - DCT_RUN_CATEGORY1 + 1;
  case DCT_RUN_CATEGORY1B:
    *val = ((eb&0x4) ? -1 : 1);
    return (eb&0x3)+6;
  case DCT_RUN_CATEGORY1C:
    *val = ((eb&0x8) ? -1 : 1);
    return (eb&0x7)+10;
  case DCT_RUN_CATEGORY2:
    *val = ( (eb&0x2) ? -((eb&0x1)+2) : (eb&0x1)+2 );
    return 1;
  case DCT_RUN_CATEGORY2B:
    *val = ( (eb&0x4) ? -(((eb&0x2)>>1)+2) : ((eb&0x2)>>1)+2);
    return (eb&0x1)+2;
  default:
    *val = 0;
    return 0;
  }
}

/* No final DC to encode yet (DC prediction hasn't been done) So
   simply assume there will be a nonzero DC value and code.  That's
   not a true assumption but it can be fixed-up as DC is tokenized
   later */

void dct_tokenize_AC(CP_INSTANCE *cpi, int fi, ogg_int16_t *dct, int chroma){
  int coeff = 1; /* skip DC for now */
  while(coeff < BLOCK_SIZE){
    ogg_int16_t val = dct[coeff];
    int i = coeff;
    
    while( !val && (++i < BLOCK_SIZE) )
      val = dct[i];
    
    if ( i == BLOCK_SIZE ){
      
      /* if there are no other tokens in this group yet, set up to be
	 prepended later.  */
      increment_run(cpi,chroma,fi,coeff>1,coeff);
      coeff = BLOCK_SIZE;
    }else{
      
      coeff = i+emit_token(cpi, chroma, fi, coeff, i, val);
    }
  }
}

/* called after AC tokenization is complete, because DC coding has to
   happen after DC predict, which has to happen after the
   Hilbert-ordered TQT loop */
/* Convention: All EOB runs in the coeff1 stack are regenerated as the
   runs are tracked.  Other tokens are adjusted in-place (potentially
   replaced with NOOP tokens.  The size of the coeff 1 stack is not
   altered */
static void tokenize_DC(CP_INSTANCE *cpi, int fi, int chroma,
			int *idx1, int *run1){
  
  unsigned char *cp=cpi->frag_coded;

  if ( cp[fi] ) {
    int val = cpi->frag_dc[fi];
    int token1 = cpi->dct_token[1][*idx1];
    int eb1 = cpi->dct_token_eb[1][*idx1];

    if(!*run1) *run1 = decode_eob_token(token1, eb1);

    if(val){
      /* nonzero DC val, no coeff 1 stack 'fixup'. */

      emit_token(cpi,chroma,fi,0,0,val);

      /* there was a nonzero DC value, so there's no alteration to the
	 track1 stack for this fragment; track/regenerate stack 1
	 state unchanged */
      if(*run1){
	/* in the midst of an EOB run in stack 1 */
	increment_run(cpi,chroma,fi,1,1);
	(*run1)--;

      }else{

	/* non-EOB run token to emit for stack 1 */
	emit_raw(cpi,chroma,fi,1,token1,eb1);

      }

    }else{

      /* zero DC value; that means the entry in coeff position 1
	 should have been coded from the DC coeff position. This
	 requires a stack 1 fixup. */
      
      if(*run1){

	/* current stack 1 token an EOB run; conceptually move this fragment's EOBness to stack 0 */
	increment_run(cpi,chroma,fi,0,0);
	      
	/* decrement current EOB run for coeff 1 without adding to coded run */
	(*run1)--;

      }else{
	int run,val=0;

	/* stack 1 token is one of: zerorun, dctrun or dctval */
	/* A zero-run token is expanded and moved to token stack 0 (stack 1 entry dropped) */
	/* A dctval may be transformed into a single dctrun that is moved to stack 0,
	   or if it does not fit in a dctrun, we leave the stack 1 entry alone and emit 
	   a single length-1 zerorun token for stack 0 */
	/* A dctrun is extended and moved to stack 0.  During AC
	   coding, we restrict the run lengths on dctruns for stack 1
	   so we know there's no chance of overrunning the
	   representable range */

	run = decode_token(token1,eb1,&val)+1;

	if(!emit_token(cpi,chroma,fi,0,run,val)){
	  emit_raw(cpi,chroma,fi,1,token1,eb1);
	}
      }
    }
    
    /* update token counter if not in a run */
    if (!*run1) (*idx1)++;
    
  }
}

void dct_tokenize_init (CP_INSTANCE *cpi){
  int i;

  memset(cpi->eob_run, 0, sizeof(cpi->eob_run));
  memset(cpi->eob_pre, 0, sizeof(cpi->eob_pre));
  memset(cpi->dc_bits, 0, sizeof(cpi->dc_bits));
  memset(cpi->ac1_bits, 0, sizeof(cpi->ac1_bits));
  memset(cpi->acN_bits, 0, sizeof(cpi->acN_bits));
  memset(cpi->dct_token_count, 0, sizeof(cpi->dct_token_count));
#ifdef COLLECT_METRICS
  memset(cpi->dct_eob_fi_count, 0, sizeof(cpi->dct_eob_fi_count));
#endif

  for(i=0;i<BLOCK_SIZE;i++){
    cpi->dct_token[i] = cpi->dct_token_storage + cpi->stack_offset*i;
    cpi->dct_token_eb[i] = cpi->dct_token_eb_storage + cpi->stack_offset*i;

#ifdef COLLECT_METRICS
    cpi->dct_eob_fi_stack[i] = cpi->dct_eob_fi_storage + cpi->frag_total*i;
    cpi->dct_token_frag[i] = cpi->dct_token_frag_storage + cpi->stack_offset*i;
#endif
  }
}

void dct_tokenize_ac_chroma (CP_INSTANCE *cpi){
  int i;
  for(i=1;i<64;i++){
    cpi->dct_token_ycount[i]=cpi->dct_token_count[i];
    if(cpi->eob_run[i])
      cpi->dct_token_ycount[i]++; /* there will be another y plane token after welding */
    cpi->eob_ypre[i]=cpi->eob_pre[i];
  }
}

/* post-facto DC tokenization (has to be completed after DC predict)
   coeff 1 fixups and eobrun welding */
void dct_tokenize_finish (CP_INSTANCE *cpi){
  int i,sbi;
  int idx1=0,run1=0;

  /* we parse the token stack for coeff1 to stay in sync, and re-use
     the token stack counters to track */
  /* emit an eob run for the end run of stack 1; this is used to
     reparse the stack in the DC code loop.  The current state will be
     recreated by the end of DC encode */

  if(cpi->eob_run[1]) add_eob_run(cpi,1,cpi->eob_run[1]);
  memset(cpi->ac1_bits, 0, sizeof(cpi->ac1_bits));
  cpi->dct_token_count[1]=0;
  cpi->eob_pre[1]=cpi->eob_run[1]=0;
#ifdef COLLECT_METRICS
  /* reset and reuse as a counter */
  cpi->dct_eob_fi_count[1]=0;
#endif
  
  for (sbi=0; sbi < cpi->super_n[0]; sbi++ ){
    superblock_t *sb = &cpi->super[0][sbi];
    int bi;
    for (bi=0; bi<16; bi++, i++ ) {
      int fi = sb->f[bi];
      tokenize_DC(cpi, fi, 0, &idx1, &run1);
    }
  }

  for(i=0;i<2;i++){
    cpi->dct_token_ycount[i]=cpi->dct_token_count[i];
    if(cpi->eob_run[i])
      cpi->dct_token_ycount[i]++; /* there will be another y plane token after welding */
    cpi->eob_ypre[i]=cpi->eob_pre[i];
  }

  for (; sbi < cpi->super_total; sbi++ ){
    superblock_t *sb = &cpi->super[0][sbi];
    int bi;
    for (bi=0; bi<16; bi++,i++ ) {
      int fi = sb->f[bi];
      tokenize_DC(cpi, fi, 1, &idx1, &run1);
    }
  }

  /* DC coded, AC coeff 1 state fixed up/regenerated */

  /* tie together eob runs at the beginnings/ends of coeff groups */
  {
    int coeff = 0;
    int run = 0;
    
    for(i=0;i<BLOCK_SIZE;i++){
      if(cpi->eob_pre[i]){
	/* group begins with an EOB run */
	
	/* special case the ongoing run + eob is at or over the max run size;
	   we know the ongoing run is < 4095 or it would have been flushed already. */
	if(run && (run&0x7fff) + cpi->eob_pre[i] >= 4095){ /* 1 */
	  add_eob_run(cpi,coeff,4095 | (run&0x8000));
	  cpi->eob_pre[i] -= 4095-(run&0x7fff); 
	  cpi->eob_ypre[i] -= 4095-(run&0x7fff); 
	  run = 0;
	  coeff = i;
	}
	
	if(run){
	  if(cpi->dct_token_count[i]){ /* 2 */
	    /* group is not only an EOB run; emit the run token */
	    add_eob_run(cpi,coeff,run + cpi->eob_pre[i]);
	    cpi->eob_ypre[i] = 0;
	    cpi->eob_pre[i] = 0;
	    run = cpi->eob_run[i];
	    coeff = i;
	  }else{ /* 3 */
	    /* group consists entirely of EOB run.  Add, iterate */
	    run += cpi->eob_pre[i];
	    cpi->eob_pre[i] = 0;
	    cpi->eob_ypre[i] = 0;
	  }
	}else{
	    
	  if(cpi->dct_token_count[i]){
	    /* there are other tokens in this group; work backwards as we need to prepend */
	    while(cpi->eob_pre[i] >= 4095){ /* 4 */
	      int lchroma = (cpi->eob_pre[i]-4095 >= cpi->eob_ypre[i]);
	      prepend_eob_run(cpi,lchroma,i,4095);
	      if(!lchroma)cpi->dct_token_ycount[i]++;
	      cpi->eob_pre[i] -= 4095;
	    }
	    if(cpi->eob_pre[i]){ /* 5 */
	      int lchroma = (cpi->eob_ypre[i]<=0); /* possible when case 1 triggered */
	      prepend_eob_run(cpi, lchroma, i, cpi->eob_pre[i]);
	      if(!lchroma)cpi->dct_token_ycount[i]++;
	      cpi->eob_pre[i] = 0;
	      cpi->eob_ypre[i] = 0;
	    }
	    run = cpi->eob_run[i];
	    coeff = i;
	  }else{
	    /* group consists entirely of EOB run.  Add, flush overs, iterate */
	    int lchroma = (cpi->eob_ypre[i]<=0);
	    while(cpi->eob_pre[i] >= 4095){
	      add_eob_run(cpi,i,4095|(!lchroma<<15));
	      if(!lchroma)cpi->dct_token_ycount[i]++;
	      cpi->eob_pre[i] -= 4095;
	      cpi->eob_ypre[i] -= 4095;
	      lchroma = (cpi->eob_ypre[i]<=0);
	    }
	    run = cpi->eob_pre[i] | (!lchroma<<15);
	    coeff = i;
	    /* source is pre-run, so the eventual eob_emit_run also needs to increment ycount if coded into Y plane */
	    if(!lchroma)cpi->dct_token_ycount[i]++;
	  }
	}
      }else{
	/* no eob run to begin group */
	if(i==0 || cpi->dct_token_count[i]){
	  if(run)
	    add_eob_run(cpi,coeff,run);
	  
	  run = cpi->eob_run[i];
	  coeff = i;
	}
      }
    }
    
    if(run)
      add_eob_run(cpi,coeff,run);
    
  }
}
