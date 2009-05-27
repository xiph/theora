/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2008                *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

  function:
  last mod: $Id$

 ********************************************************************/

#include <stdlib.h>
#include <string.h>
#include "codec_internal.h"
#include "quant_lookup.h"

static void make_eobrun_token(int run, int *token, int *eb){
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

static int make_dct_token(CP_INSTANCE *cpi, 
			  int coeff,
			  int coeff2,
			  int val,
			  int *eb){
  
  ogg_uint32_t absval = abs(val);
  int neg = (val<0);
  int zero_run = coeff2-coeff;
  int token;
  *eb=0;

  if (zero_run){
    int adj = (coeff!=1); /* implement a minor restriction on
			     stack 1 so that we know during DC
			     fixups that extended a dctrun token
			     from stack 1 will never overflow */
    if ((absval==1) && (zero_run<17+adj)){
      if ( zero_run <= 5 ) {
	token = DCT_RUN_CATEGORY1+zero_run-1; 
	*eb   = neg;
      }else if ( zero_run <= 9 ) {
	token = DCT_RUN_CATEGORY1B; 
	*eb   = zero_run-6+(neg<<2);
      }else{
	token = DCT_RUN_CATEGORY1C;
	*eb   = zero_run-10+(neg<<3);
      }
    }else if((absval==2 || absval==3) && (zero_run < 3+adj)){
      if ( zero_run == 1 ) {
	token = DCT_RUN_CATEGORY2;
	*eb   = absval-2+(neg<<1);
      }else{
	token = DCT_RUN_CATEGORY2B;
	*eb   = (neg<<2)+((absval-2)<<1)+zero_run-2;
      }
    }else{
      if ( zero_run <= 8 )
	token = DCT_SHORT_ZRL_TOKEN;
      else
	token = DCT_ZRL_TOKEN;
      *eb = zero_run-1;
    }
  } else if ( absval == 1 ){
    token = (neg ? MINUS_ONE_TOKEN : ONE_TOKEN);
  } else if ( absval == 2 ) {
    token = (neg ? MINUS_TWO_TOKEN : TWO_TOKEN);
  } else if ( absval <= MAX_SINGLE_TOKEN_VALUE ) {
    token = LOW_VAL_TOKENS + (absval - DCT_VAL_CAT2_MIN);
    *eb   = neg;
  } else if ( absval <= 8 ) {
    token = DCT_VAL_CATEGORY3;
    *eb   = (absval - DCT_VAL_CAT3_MIN) + (neg << 1);
  } else if ( absval <= 12 ) {
    token = DCT_VAL_CATEGORY4;
    *eb   = (absval - DCT_VAL_CAT4_MIN) + (neg << 2);
  } else if ( absval <= 20 ) {
    token = DCT_VAL_CATEGORY5;
    *eb   = (absval - DCT_VAL_CAT5_MIN) + (neg << 3);
  } else if ( absval <= 36 ) {
    token = DCT_VAL_CATEGORY6;
    *eb   = (absval - DCT_VAL_CAT6_MIN) + (neg << 4);
  } else if ( absval <= 68 ) {
    token = DCT_VAL_CATEGORY7;
    *eb   = (absval - DCT_VAL_CAT7_MIN) + (neg << 5);
  } else {
    token = DCT_VAL_CATEGORY8;
    *eb   = (absval - DCT_VAL_CAT8_MIN) + (neg << 9);
  } 

  return token;
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

/* token logging to allow a few fragments of efficient rollback.  SKIP
   analysis is tied up in the tokenization process, so we need to be
   able to undo a fragment's tokens on a whim */

static int acoffset[64]={
  00,00,00,00,00,00,16,16,
  16,16,16,16,16,16,16,32,
  32,32,32,32,32,32,32,32,
  32,32,32,32,48,48,48,48,
  48,48,48,48,48,48,48,48,
  48,48,48,48,48,48,48,48,
  48,48,48,48,48,48,48,48};

/* only counts bits */
static int tokencost(CP_INSTANCE *cpi, int huff, int coeff, int token){
  huff += acoffset[coeff];
  return cpi->huff_codes[huff][token].nbits+OC_DCT_TOKEN_EXTRA_BITS[token];
}

void tokenlog_rollback(CP_INSTANCE *cpi, token_checkpoint_t *stack,int n){
  int i;
  for(i=n-1;i>=0;i--){
    int coeff = stack[i].coeff;
    if(stack[i].count>=0) cpi->dct_token_count[coeff] = stack[i].count; 
    cpi->eob_run[coeff] = stack[i].run;
    cpi->eob_pre[coeff] = stack[i].pre;
#if defined(OC_COLLECT_METRICS)
    cpi->dct_eob_fi_count[coeff] = stack[i].runstack;
#endif
  }
}

static void tokenlog_metrics(CP_INSTANCE *cpi, int coeff, int chroma, int token){
  if(coeff == 0){
    /* DC */
    int i;
    for ( i = 0; i < DC_HUFF_CHOICES; i++)
      cpi->dc_bits[chroma][i] += cpi->huff_codes[i][token].nbits;
  }else if (coeff == 1){
    /* AC == 1*/
    int i,offset = acoffset[1]+AC_HUFF_OFFSET;
    for ( i = 0; i < AC_HUFF_CHOICES; i++)
      cpi->ac1_bits[chroma][i] += cpi->huff_codes[offset+i][token].nbits;
  }else{
    /* AC > 1*/
    int i,offset = acoffset[coeff]+AC_HUFF_OFFSET;
    for ( i = 0; i < AC_HUFF_CHOICES; i++)
      cpi->acN_bits[chroma][i] += cpi->huff_codes[offset+i][token].nbits;
  }
}

void tokenlog_commit(CP_INSTANCE *cpi, token_checkpoint_t *stack, int n){
  int i;
  for(i=0;i<n;i++){
    int pos = stack[i].count;
    if(pos>=0){
      int coeff = stack[i].coeff;
      int token = cpi->dct_token[coeff][pos];
      int chroma = stack[i].chroma;
      tokenlog_metrics(cpi,coeff,chroma,token);
    }
  }
}

static void tokenlog_mark(CP_INSTANCE *cpi, int coeff, token_checkpoint_t **stack){
  (*stack)->coeff = coeff;
  (*stack)->count = -1;
  (*stack)->run = cpi->eob_run[coeff];
  (*stack)->pre = cpi->eob_pre[coeff];
#if defined(OC_COLLECT_METRICS)
  (*stack)->runstack = cpi->dct_eob_fi_count[coeff];
#endif
  (*stack)++;
}

static void token_add(CP_INSTANCE *cpi, int chroma, int coeff, 
			 unsigned char token, ogg_uint16_t eb,
			 token_checkpoint_t **stack){
  int pos = cpi->dct_token_count[coeff]++;
  cpi->dct_token[coeff][pos] = token;
  cpi->dct_token_eb[coeff][pos] = eb;
  if(stack){
    (*stack)->coeff = coeff;
    (*stack)->count = pos;
    (*stack)->chroma = chroma;
    (*stack)->run = cpi->eob_run[coeff];
    (*stack)->pre = cpi->eob_pre[coeff];
#if defined(OC_COLLECT_METRICS)
    (*stack)->runstack = cpi->dct_eob_fi_count[coeff];
#endif
    (*stack)++;
  }else{
    tokenlog_metrics(cpi,coeff,chroma,token);
  }
}

/* does not offer logging option; only used in nonconditional EOBrun welding */
static void token_prepend(CP_INSTANCE *cpi, int chroma, int coeff, 
			  unsigned char token, ogg_uint16_t eb){
  
  cpi->dct_token[coeff]--;
  cpi->dct_token_eb[coeff]--;
#if defined(OC_COLLECT_METRICS)
  cpi->dct_token_frag[coeff]--;
#endif
  cpi->dct_token[coeff][0] = token;
  cpi->dct_token_eb[coeff][0] = eb;
  cpi->dct_token_count[coeff]++;
  tokenlog_metrics(cpi,coeff,chroma,token);
}

static int tokenize_eobrun(CP_INSTANCE *cpi, int pos, int run, token_checkpoint_t **stack){
  int token=0,eb=0;
  int chroma = !(run&0x8000);
  int huff = cpi->huffchoice[cpi->FrameType!=KEY_FRAME][1][chroma];

  make_eobrun_token(run&0x7fff, &token, &eb);
  token_add(cpi, chroma, pos, token, eb, stack);

  return tokencost(cpi,huff,pos,token);
}


static void tokenize_prepend_eobrun(CP_INSTANCE *cpi, int chroma, int pos, int run){
  int token=0,eb=0;
  make_eobrun_token(run, &token, &eb);
  token_prepend(cpi, chroma, pos, token, eb);
}

/* only used in nonconditional DC/stack1 fixups */
static void token_add_raw(CP_INSTANCE *cpi, 
			  int chroma,
			  int fi,
			  int coeff,
			  int token,
			  int eb){
  
  /* Emit pending EOB run if any */
  if(cpi->eob_run[coeff]){
    tokenize_eobrun(cpi,coeff,cpi->eob_run[coeff],NULL);
    cpi->eob_run[coeff]=0;
  }
#if defined(OC_COLLECT_METRICS)
  cpi->dct_token_frag[coeff][cpi->dct_token_count[coeff]] = fi;
#endif
  token_add(cpi,chroma,coeff,token,eb,NULL);
  
}

/* NULL stack to force commit */
static int tokenize_dctval(CP_INSTANCE *cpi, 
			   int chroma,
			   int fi,
			   int coeff,
			   int coeff2,
			   int val,
			   token_checkpoint_t **stack){
  int eb=0;
  int token=make_dct_token(cpi,coeff,coeff2,val,&eb);

  /* Emit pending EOB run if any */
  if(cpi->eob_run[coeff]){
    tokenize_eobrun(cpi,coeff,cpi->eob_run[coeff],stack);
    cpi->eob_run[coeff]=0;
  }
#if defined(OC_COLLECT_METRICS)
  cpi->dct_token_frag[coeff][cpi->dct_token_count[coeff]] = fi;
#endif
  
  token_add(cpi,chroma,coeff,token,eb,stack);
  
  if( ((token==DCT_SHORT_ZRL_TOKEN) || (token==DCT_ZRL_TOKEN)) && val)
    return 0; /* we only flushed a preceeding zero run, not the value token. */
  
  return 1;
}

static int tokenize_mark_run(CP_INSTANCE *cpi, 
			      int chroma,
			      int fi,
			      int pre,
			      int coeff,
			      token_checkpoint_t **stack){
  int cost = 0;

  if(pre && cpi->dct_token_count[coeff] == 0){
    if(stack)tokenlog_mark(cpi,coeff,stack); /* log an undo without logging a token */
    cpi->eob_pre[coeff]++;
  }else{
    if((cpi->eob_run[coeff]&0x7fff) == 4095){
      cost += tokenize_eobrun(cpi,coeff,cpi->eob_run[coeff],stack);
      cpi->eob_run[coeff] = 0;
    }
    
    if(stack)tokenlog_mark(cpi,coeff,stack); /* log an undo without logging a token */
    cpi->eob_run[coeff]++;
    cpi->eob_run[coeff]|= !chroma<<15;
  }	  
#if defined(OC_COLLECT_METRICS)
  cpi->dct_eob_fi_stack[coeff][cpi->dct_eob_fi_count[coeff]++]=fi;
#endif
  return cost;
}

static int tokenize_dctcost(CP_INSTANCE *cpi,int chroma,
			     int coeff, int coeff2, int val){
  int huff = cpi->huffchoice[cpi->FrameType!=KEY_FRAME][1][chroma];
  int eb=0,token=0;
  int cost = 0;
  
  /* if there was an EOB run pending, count the cost of flushing it */
  if(cpi->eob_run[coeff]){
    int rchroma = !(cpi->eob_run[coeff]&0x8000); 
    int rhuff = cpi->huffchoice[cpi->FrameType!=KEY_FRAME][1][rchroma];
    make_eobrun_token(cpi->eob_run[coeff]&0x7fff,&token,&eb);
    cost += tokencost(cpi,rhuff,coeff,token);
  }

  /* count cost of token */
  token = make_dct_token(cpi,coeff,coeff2,val,&eb);
  cost += tokencost(cpi,huff, coeff, token);
  
  /* if token was a zero run, we've not yet coded up to the value */
  if( (token==DCT_SHORT_ZRL_TOKEN) || (token==DCT_ZRL_TOKEN))
    return cost + tokenize_dctcost(cpi,chroma,coeff2,coeff2,val);
  else
    return cost;
}

/* The opportunity cost of an in-progress EOB run is the cost to flush
   the run up to 'n+1' minus the cost of flushing the run up to 'n' */
static int tokenize_eobcost(CP_INSTANCE *cpi,int chroma, int coeff){
  int n = cpi->eob_run[coeff];
  int eb=0,token=0;
  int cost0=0,cost1;
  
  if(n>0){
    int huff = cpi->huffchoice[cpi->FrameType!=KEY_FRAME][1][!(n&0x8000)];

    make_eobrun_token(n&0x7fff, &token, &eb);
    cost0 = tokencost(cpi,huff,coeff,token);

    make_eobrun_token((n+1)&0x7fff, &token, &eb);
    cost1 = tokencost(cpi,huff,coeff,token);
    
  }else{
    int huff = cpi->huffchoice[cpi->FrameType!=KEY_FRAME][1][chroma];
    cost1 = tokencost(cpi,huff,coeff,DCT_EOB_TOKEN);
  }    

  return cost1-cost0;
}

/* No final DC to encode yet (DC prediction hasn't been done) So
   simply assume there will be a nonzero DC value and code.  That's
   not a true assumption but it can be fixed-up as DC is tokenized
   later */
int dct_tokenize_AC(CP_INSTANCE *cpi, const int fi, 
		    ogg_int16_t *dct, const ogg_int16_t *dequant, 
		    const ogg_int16_t *origdct, const int chroma, 
		    token_checkpoint_t **stack,int _acmin){
  int coeff = 1; /* skip DC for now */
  int i = coeff;
  int retcost = 0;

  while( !dct[i] && (++i < BLOCK_SIZE) );
    
  while(i < BLOCK_SIZE){
    int ret;
    int od = origdct[dezigzag_index[i]];
    int bestd=0,d = dct[i];
    int bestmin;
    int cost,cost2=0,bestcost=0;
    int j=i+1,k;

    while((j < BLOCK_SIZE) && !dct[j] ) j++;

  if(i>=_acmin){
    if(j==BLOCK_SIZE){
      cost = tokenize_eobcost(cpi,chroma,coeff);
      if(i+1<BLOCK_SIZE) 
	cost2 = tokenize_eobcost(cpi,chroma,i+1);
    }else{
      cost = tokenize_dctcost(cpi,chroma,coeff,j,dct[j]);
      cost2 = tokenize_dctcost(cpi,chroma,i+1,j,dct[j]);
    }
    bestmin = od*od+cost*cpi->lambda;
    

    for(k=1;k<=abs(d);k++){
      int dval = (d>0 ? k : -k);
      int dd = dval*dequant[i] - od;
      int min = dd*dd;
      cost = tokenize_dctcost(cpi,chroma,coeff,i,dval);

      min += (cost+cost2)*cpi->lambda;
      if(min<bestmin){
	bestmin=min;
	bestcost=cost;
	bestd=dval;
      }
    }

    dct[i]=bestd;
    if(bestd==0){
      if(j==BLOCK_SIZE) break;
      i=j;
      continue;
    }
  }
  else{
    bestcost = tokenize_dctcost(cpi,chroma,coeff,i,d);
  }
    
    retcost+=bestcost;
	
    ret = tokenize_dctval(cpi, chroma, fi, coeff, i, dct[i], stack);
    if(!ret)
      tokenize_dctval(cpi, chroma, fi, i, i, dct[i], stack);
    coeff=i+1;
    i=j;
    
  }
  if(coeff<BLOCK_SIZE) retcost+=tokenize_mark_run(cpi,chroma,fi,coeff>1,coeff,stack);
  return retcost;
}

/* called after AC tokenization is complete, because DC coding has to
   happen after DC predict, which has to happen after the
   Hilbert-ordered TQT loop */
/* Convention: All tokens and runs in the coeff1 stack are
   'regenerated' as the stack is tracked. This can be done in-place;
   stack 1 can only shrink or stay the same size */
static void tokenize_DC(CP_INSTANCE *cpi, int fi, int chroma,
			int *idx1, int *run1){
  
  int val = cpi->frag_dc[fi];
  int token1 = cpi->dct_token[1][*idx1];
  int eb1 = cpi->dct_token_eb[1][*idx1];
  
  if(!*run1) *run1 = decode_eob_token(token1, eb1);
  
  if(val){
    /* nonzero DC val, no coeff 1 stack 'fixup'. */
    
    tokenize_dctval(cpi,chroma,fi,0,0,val,NULL);
    
    /* there was a nonzero DC value, so there's no alteration to the
       track1 stack for this fragment; track/regenerate stack 1
	 state unchanged */
    if(*run1){
      /* in the midst of an EOB run in stack 1 */
      tokenize_mark_run(cpi,chroma,fi,1,1,NULL);
      (*run1)--;
      
    }else{
      
      /* non-EOB run token to emit for stack 1 */
      token_add_raw(cpi,chroma,fi,1,token1,eb1);
      
    }
    
  }else{

    /* zero DC value; that means the entry in coeff position 1
       should have been coded from the DC coeff position. This
       requires a stack 1 fixup. */
    
    if(*run1){
      
      /* current stack 1 token an EOB run; conceptually move this fragment's EOBness to stack 0 */
      tokenize_mark_run(cpi,chroma,fi,0,0,NULL);
      
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
      
      if(!tokenize_dctval(cpi,chroma,fi,0,run,val,NULL)){
	token_add_raw(cpi,chroma,fi,1,token1,eb1);
      }
    }
  }
  
  /* update token counter if not in a run */
  if (!*run1) (*idx1)++;
}

void dct_tokenize_init (CP_INSTANCE *cpi){
  int i;

  memset(cpi->eob_run, 0, sizeof(cpi->eob_run));
  memset(cpi->eob_pre, 0, sizeof(cpi->eob_pre));
  memset(cpi->dc_bits, 0, sizeof(cpi->dc_bits));
  memset(cpi->ac1_bits, 0, sizeof(cpi->ac1_bits));
  memset(cpi->acN_bits, 0, sizeof(cpi->acN_bits));
  memset(cpi->dct_token_count, 0, sizeof(cpi->dct_token_count));
#if defined(OC_COLLECT_METRICS)
  memset(cpi->dct_eob_fi_count, 0, sizeof(cpi->dct_eob_fi_count));
#endif

  for(i=0;i<BLOCK_SIZE;i++){
    cpi->dct_token[i] = cpi->dct_token_storage + cpi->stack_offset*i;
    cpi->dct_token_eb[i] = cpi->dct_token_eb_storage + cpi->stack_offset*i;

#if defined(OC_COLLECT_METRICS)
    cpi->dct_eob_fi_stack[i] = cpi->dct_eob_fi_storage + cpi->frag_total*i;
    cpi->dct_token_frag[i] = cpi->dct_token_frag_storage + cpi->stack_offset*i;
#endif
  }
}

void dct_tokenize_mark_ac_chroma (CP_INSTANCE *cpi){
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
  unsigned char *cp=cpi->frag_coded;
  
  /* we parse the token stack for coeff1 to stay in sync, and re-use
     the token stack counters to track */
  /* emit an eob run for the end run of stack 1; this is used to
     reparse the stack in the DC code loop.  The current state will be
     recreated by the end of DC encode */

  if(cpi->eob_run[1]) tokenize_eobrun(cpi,1,cpi->eob_run[1],NULL);
  memset(cpi->ac1_bits, 0, sizeof(cpi->ac1_bits));
  cpi->dct_token_count[1]=0;
  cpi->eob_pre[1]=cpi->eob_run[1]=0;
#if defined(OC_COLLECT_METRICS)
  /* reset and reuse as a counter */
  cpi->dct_eob_fi_count[1]=0;
#endif
  
  for (sbi=0; sbi < cpi->super_n[0]; sbi++ ){
    superblock_t *sb = &cpi->super[0][sbi];
    int bi;
    for (bi=0; bi<16; bi++, i++ ) {
      int fi = sb->f[bi];
      if(cp[fi]) 
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
      if(cp[fi]) 
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
	  tokenize_eobrun(cpi,coeff,4095 | (run&0x8000),NULL);
	  cpi->eob_pre[i] -= 4095-(run&0x7fff); 
	  cpi->eob_ypre[i] -= 4095-(run&0x7fff); 
	  run = 0;
	  coeff = i;
	}
	
	if(run){
	  if(cpi->dct_token_count[i]){ /* 2 */
	    /* group is not only an EOB run; emit the run token */
	    tokenize_eobrun(cpi,coeff,run + cpi->eob_pre[i],NULL);
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
	      tokenize_prepend_eobrun(cpi,lchroma,i,4095);
	      if(!lchroma)cpi->dct_token_ycount[i]++;
	      cpi->eob_pre[i] -= 4095;
	    }
	    if(cpi->eob_pre[i]){ /* 5 */
	      int lchroma = (cpi->eob_ypre[i]<=0); /* possible when case 1 triggered */
	      tokenize_prepend_eobrun(cpi, lchroma, i, cpi->eob_pre[i]);
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
	      tokenize_eobrun(cpi,i,4095|(!lchroma<<15),NULL);
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
	    tokenize_eobrun(cpi,coeff,run,NULL);
	  
	  run = cpi->eob_run[i];
	  coeff = i;
	}
      }
    }
    
    if(run)
      tokenize_eobrun(cpi,coeff,run,NULL);
    
  }
}
