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
#include "encint.h"



static int oc_make_eob_token(int _run_count){
  if(_run_count<4)return OC_DCT_EOB1_TOKEN+_run_count-1;
  else{
    int cat;
    cat=OC_ILOGNZ_32(_run_count)-3;
    cat=OC_MINI(cat,3);
    return OC_DCT_REPEAT_RUN0_TOKEN+cat;
  }
}

static int oc_make_eob_token_full(int _run_count,int *_eb){
  if(_run_count<4){
    *_eb=0;
    return OC_DCT_EOB1_TOKEN+_run_count-1;
  }
  else{
    int cat;
    cat=OC_ILOGNZ_32(_run_count)-3;
    cat=OC_MINI(cat,3);
    *_eb=_run_count-OC_BYTE_TABLE32(4,8,16,0,cat);
    return OC_DCT_REPEAT_RUN0_TOKEN+cat;
  }
}

static int oc_decode_eob_token(int _token,int _eb){
  return -oc_dct_token_skip(_token,_eb);
}

static int oc_make_dct_token(int _zzi,int _zzj,int _val){
  int zero_run;
  int token;
  int val;
  val=abs(_val);
  zero_run=_zzj-_zzi;
  if(zero_run>0){
    int adj;
    /*Implement a minor restriction so that we know that extending a combo
       token from stack 1 will never overflow during DC fix-ups.*/
    adj=_zzi!=1;
    if(val<2&&zero_run<17+adj){
      if(zero_run<6)token=OC_DCT_RUN_CAT1A+zero_run-1;
      else if(zero_run<10)token=OC_DCT_RUN_CAT1B;
      else token=OC_DCT_RUN_CAT1C;
    }
    else if(val<4&&zero_run<3+adj){
      if(zero_run<2)token=OC_DCT_RUN_CAT2A;
      else token=OC_DCT_RUN_CAT2B;
    }
    else{
      if(zero_run<9)token=OC_DCT_SHORT_ZRL_TOKEN;
      else token=OC_DCT_ZRL_TOKEN;
    }
  }
  else if(val<3)token=OC_ONE_TOKEN+(val-1<<1)+(_val<0);
  else if(val<7)token=OC_DCT_VAL_CAT2+val-3;
  else if(val<9)token=OC_DCT_VAL_CAT3;
  else if(val<13)token=OC_DCT_VAL_CAT4;
  else if(val<21)token=OC_DCT_VAL_CAT5;
  else if(val<37)token=OC_DCT_VAL_CAT6;
  else if(val<69)token=OC_DCT_VAL_CAT7;
  else token=OC_DCT_VAL_CAT8;
  return token;
}

static int oc_make_dct_token_full(int _zzi,int _zzj,int _val,int *_eb){
  int neg;
  int zero_run;
  int token;
  int eb;
  neg=_val<0;
  _val=abs(_val);
  zero_run=_zzj-_zzi;
  if(zero_run>0){
    int adj;
    /*Implement a minor restriction on stack 1 so that we know during DC fixups
       that extending a dctrun token from stack 1 will never overflow.*/
    adj=_zzi!=1;
    if(_val<2&&zero_run<17+adj){
      if(zero_run<6){
        token=OC_DCT_RUN_CAT1A+zero_run-1;
        eb=neg;
      }
      else if(zero_run<10){
        token=OC_DCT_RUN_CAT1B;
        eb=zero_run-6+(neg<<2);
      }
      else{
        token=OC_DCT_RUN_CAT1C;
        eb=zero_run-10+(neg<<3);
      }
    }
    else if(_val<4&&zero_run<3+adj){
      if(zero_run<2){
        token=OC_DCT_RUN_CAT2A;
        eb=_val-2+(neg<<1);
      }
      else{
        token=OC_DCT_RUN_CAT2B;
        eb=zero_run-2+(_val-2<<1)+(neg<<2);
      }
    }
    else{
      if(zero_run<9)token=OC_DCT_SHORT_ZRL_TOKEN;
      else token=OC_DCT_ZRL_TOKEN;
      eb=zero_run-1;
    }
  }
  else if(_val<3){
    token=OC_ONE_TOKEN+(_val-1<<1)+neg;
    eb=0;
  }
  else if(_val<7){
    token=OC_DCT_VAL_CAT2+_val-3;
    eb=neg;
  }
  else if(_val<9){
    token=OC_DCT_VAL_CAT3;
    eb=_val-7+(neg<<1);
  }
  else if(_val<13){
    token=OC_DCT_VAL_CAT4;
    eb=_val-9+(neg<<2);
  }
  else if(_val<21){
    token=OC_DCT_VAL_CAT5;
    eb=_val-13+(neg<<3);
  }
  else if(_val<37){
    token=OC_DCT_VAL_CAT6;
    eb=_val-21+(neg<<4);
  }
  else if(_val<69){
    token=OC_DCT_VAL_CAT7;
    eb=_val-37+(neg<<5);
  }
  else{
    token=OC_DCT_VAL_CAT8;
    eb=_val-69+(neg<<9);
  }
  *_eb=eb;
  return token;
}

/*Token logging to allow a few fragments of efficient rollback.
  Late SKIP analysis is tied up in the tokenization process, so we need to be
   able to undo a fragment's tokens on a whim.*/

static const unsigned char OC_ZZI_HUFF_OFFSET[64]={
   0,16,16,16,16,16,32,32,
  32,32,32,32,32,32,32,48,
  48,48,48,48,48,48,48,48,
  48,48,48,48,64,64,64,64,
  64,64,64,64,64,64,64,64,
  64,64,64,64,64,64,64,64,
  64,64,64,64,64,64,64,64
};

static int oc_token_bits(oc_enc_ctx *_enc,int _huffi,int _zzi,int _token){
  return _enc->huff_codes[_huffi+OC_ZZI_HUFF_OFFSET[_zzi]][_token].nbits
   +OC_DCT_TOKEN_EXTRA_BITS[_token];
}

static void oc_enc_tokenlog_checkpoint(oc_enc_ctx *_enc,
 oc_token_checkpoint *_cp,int _pli,int _zzi){
  _cp->pli=_pli;
  _cp->zzi=_zzi;
  _cp->eob_run=_enc->eob_run[_pli][_zzi];
  _cp->ndct_tokens=_enc->ndct_tokens[_pli][_zzi];
}

void oc_enc_tokenlog_rollback(oc_enc_ctx *_enc,
 const oc_token_checkpoint *_stack,int _n){
  int i;
  for(i=_n;i-->0;){
    int pli;
    int zzi;
    pli=_stack[i].pli;
    zzi=_stack[i].zzi;
    _enc->eob_run[pli][zzi]=_stack[i].eob_run;
    _enc->ndct_tokens[pli][zzi]=_stack[i].ndct_tokens;
  }
}

static void oc_enc_token_log(oc_enc_ctx *_enc,
 int _pli,int _zzi,int _token,int _eb){
  ptrdiff_t ti;
  ti=_enc->ndct_tokens[_pli][_zzi]++;
  _enc->dct_tokens[_pli][_zzi][ti]=(unsigned char)_token;
  _enc->extra_bits[_pli][_zzi][ti]=(ogg_uint16_t)_eb;
}

static void oc_enc_eob_log(oc_enc_ctx *_enc,
 int _pli,int _zzi,int _run_count){
  int token;
  int eb;
  token=oc_make_eob_token_full(_run_count,&eb);
  oc_enc_token_log(_enc,_pli,_zzi,token,eb);
}

static int oc_enc_tokenize_dctval(oc_enc_ctx *_enc,int _pli,
 int _zzi,int _zzj,int _val){
  int eob_run;
  int token;
  int eb;
  /*Emit pending EOB run if any.*/
  eob_run=_enc->eob_run[_pli][_zzi];
  if(eob_run>0){
    oc_enc_eob_log(_enc,_pli,_zzi,eob_run);
    _enc->eob_run[_pli][_zzi]=0;
  }
  token=oc_make_dct_token_full(_zzi,_zzj,_val,&eb);
  oc_enc_token_log(_enc,_pli,_zzi,token,eb);
  /*Return 0 if we didn't tokenize the value, just the zero run preceding it.*/
  return _val==0||token!=OC_DCT_SHORT_ZRL_TOKEN&&token!=OC_DCT_ZRL_TOKEN;
}

static void oc_enc_tokenize_eobrun(oc_enc_ctx *_enc,int _pli,int _zzi){
  int eob_run;
  eob_run=_enc->eob_run[_pli][_zzi];
  eob_run++;
  if(eob_run>=4095){
    oc_enc_eob_log(_enc,_pli,_zzi,eob_run);
    eob_run=0;
  }
  _enc->eob_run[_pli][_zzi]=eob_run;
}

/*The opportunity cost of a DCT coefficient is the cost to flush any pending
   EOB run plus the cost of the coefficient itself.
  This encourages us to keep long EOB runs going in the higher/chroma
   coefficients.
  Technically this cost should be weighted by the probability that we expect a
   future fragment to continue it, but that's qi- and zzi-dependent.
  Note: Assumes AC coefficients only (_zzi>0).*/
static int oc_enc_tokenize_dctval_bits(oc_enc_ctx *_enc,int _pli,
 int _zzi,int _zzj,int _val){
  int huffi;
  int eob_run;
  int token;
  int bits;
  huffi=_enc->huff_idxs[_enc->state.frame_type][1][_pli+1>>1];
  /*If there was an EOB run pending, count the cost of flushing it.*/
  eob_run=_enc->eob_run[_pli][_zzi];
  if(eob_run)bits=oc_token_bits(_enc,huffi,_zzi,oc_make_eob_token(eob_run));
  else bits=0;
  /*Count the cost of the token.*/
  token=oc_make_dct_token(_zzi,_zzj,_val);
  bits+=oc_token_bits(_enc,huffi,_zzi,token);
  /*If token was a pure zero run, we've not yet coded the value.*/
  if(token==OC_DCT_SHORT_ZRL_TOKEN||token==OC_DCT_ZRL_TOKEN){
    eob_run=_enc->eob_run[_pli][_zzj];
    if(eob_run)bits+=oc_token_bits(_enc,huffi,_zzj,oc_make_eob_token(eob_run));
    bits+=oc_token_bits(_enc,huffi,_zzj,oc_make_dct_token(_zzj,_zzj,_val));
  }
  return bits;
}

/*The opportunity cost of an in-progress EOB run of size N+1 is the cost of
   flushing a run of size N+1 minus the cost of flushing a run of size N.
  Note: Assumes AC coefficients only (_zzi>0).*/
static int oc_enc_tokenize_eobrun_bits(oc_enc_ctx *_enc,int _pli,int _zzi){
  int eob_run;
  int huffi;
  eob_run=_enc->eob_run[_pli][_zzi];
  huffi=_enc->huff_idxs[_enc->state.frame_type][1][_pli+1>>1];
  if(eob_run>0){
    /*Note: We must be able to add another block to this run, or we would have
       flushed it already.*/
    return oc_token_bits(_enc,huffi,_zzi,oc_make_eob_token(eob_run+1))
     -oc_token_bits(_enc,huffi,_zzi,oc_make_eob_token(eob_run));
  }
  else return oc_token_bits(_enc,huffi,_zzi,OC_DCT_EOB1_TOKEN);
}


void oc_enc_tokenize_start(oc_enc_ctx *_enc){
  memset(_enc->ndct_tokens,0,sizeof(_enc->ndct_tokens));
  memset(_enc->eob_run,0,sizeof(_enc->eob_run));
  memset(_enc->dct_token_offs,0,sizeof(_enc->dct_token_offs));
  memset(_enc->dc_pred_last,0,sizeof(_enc->dc_pred_last));
}

/*No final DC to encode yet (DC prediction hasn't been done), so simply assume
   there will be a nonzero DC value and code.
  That's not a true assumption but it can be fixed-up as DC is being tokenized
   later.*/
int oc_enc_tokenize_ac(oc_enc_ctx *_enc,ptrdiff_t _fragi,ogg_int16_t *_qdct,
 const ogg_uint16_t *_dequant,const ogg_int16_t *_dct,int _pli,
 oc_token_checkpoint **_stack,int _acmin){
  oc_token_checkpoint *stack;
  int                  zzi;
  int                  zzj;
  int                  total_bits;
  int                  lambda;
  stack=*_stack;
  lambda=_enc->lambda;
  total_bits=0;
  /*Skip DC for now.*/
  zzi=1;
  for(zzj=zzi;!_qdct[zzj]&&++zzj<64;);
  while(zzj<64){
    int v;
    int d;
    int mask;
    int best_bits;
    int best_d;
    int zzk;
    int k;
    v=_dct[OC_FZIG_ZAG[zzj]];
    d=_qdct[zzj];
    for(zzk=zzj+1;zzk<64&&!_qdct[zzk];zzk++);
    /*Only apply R-D optimizaton if we're past the minimum allowed.*/
    if(zzj>=_acmin){
      int best_cost;
      int bits2;
      if(zzk>=64){
        best_bits=oc_enc_tokenize_eobrun_bits(_enc,_pli,zzi);
        if(zzj+1<64)bits2=oc_enc_tokenize_eobrun_bits(_enc,_pli,zzj+1);
        else bits2=0;
      }
      else{
        best_bits=oc_enc_tokenize_dctval_bits(_enc,_pli,zzi,zzk,_qdct[zzk]);
        bits2=oc_enc_tokenize_dctval_bits(_enc,_pli,zzj+1,zzk,_qdct[zzk]);
      }
      best_cost=v*v+best_bits*lambda;
      best_d=0;
      mask=OC_SIGNMASK(d);
      for(k=abs(d);k>0;k--){
        int dk;
        int dd;
        int bits;
        int cost;
        dk=k+mask^mask;
        dd=dk*_dequant[zzj]-v;
        bits=oc_enc_tokenize_dctval_bits(_enc,_pli,zzi,zzj,dk);
        cost=dd*dd+(bits+bits2)*lambda;
        if(cost<=best_cost){
          best_cost=cost;
          best_bits=bits;
          best_d=dk;
        }
      }
      _qdct[zzj]=best_d;
      if(best_d==0){
        zzj=zzk;
        continue;
      }
    }
    else{
      best_d=d;
      best_bits=oc_enc_tokenize_dctval_bits(_enc,_pli,zzi,zzj,best_d);
    }
    total_bits+=best_bits;
    oc_enc_tokenlog_checkpoint(_enc,stack++,_pli,zzi);
    if(!oc_enc_tokenize_dctval(_enc,_pli,zzi,zzj,best_d)){
      oc_enc_tokenlog_checkpoint(_enc,stack++,_pli,zzj);
      oc_enc_tokenize_dctval(_enc,_pli,zzj,zzj,best_d);
    }
    zzi=zzj+1;
    zzj=zzk;
  }
  if(zzi<64){
    /*We don't include the actual EOB cost for this block.
      It will be paid for by the fragment that terminates the EOB run.
    total_bits+=oc_enc_tokenize_eobrun_bits(_enc,_pli,zzi);*/
    oc_enc_tokenlog_checkpoint(_enc,stack++,_pli,zzi);
    oc_enc_tokenize_eobrun(_enc,_pli,zzi);
  }
  *_stack=stack;
  return total_bits;
}

static void oc_enc_pred_dc_rows(oc_enc_ctx *_enc,int _pli,int _y0,int _yend){
  const oc_fragment_plane *fplane;
  const oc_fragment       *frags;
  ogg_int16_t             *frag_dc;
  ptrdiff_t                fragi;
  int                     *pred_last;
  int                      nhfrags;
  int                      nvfrags;
  int                      fragx;
  int                      fragy;
  fplane=_enc->state.fplanes+_pli;
  frags=_enc->state.frags;
  frag_dc=_enc->frag_dc;
  pred_last=_enc->dc_pred_last[_pli];
  nhfrags=fplane->nhfrags;
  nvfrags=fplane->nvfrags;
  fragi=fplane->froffset+_y0*nhfrags;
  for(fragy=_y0;fragy<_yend;fragy++){
    for(fragx=0;fragx<nhfrags;fragx++,fragi++){
      if(frags[fragi].coded){
        frag_dc[fragi]=frags[fragi].dc
         -oc_frag_pred_dc(frags+fragi,fplane,fragx,fragy,pred_last);
        pred_last[OC_FRAME_FOR_MODE[frags[fragi].mb_mode]]=frags[fragi].dc;
      }
    }
  }
}

static void oc_enc_tokenize_dc(oc_enc_ctx *_enc){
  const ogg_int16_t *frag_dc;
  const ptrdiff_t   *coded_fragis;
  ptrdiff_t          ncoded_fragis;
  ptrdiff_t          fragii;
  int                pli;
  frag_dc=_enc->frag_dc;
  coded_fragis=_enc->state.coded_fragis;
  ncoded_fragis=fragii=0;
  for(pli=0;pli<3;pli++){
    unsigned char *dct_tokens0;
    unsigned char *dct_tokens1;
    ogg_uint16_t  *extra_bits0;
    ogg_uint16_t  *extra_bits1;
    ptrdiff_t      ti0;
    ptrdiff_t      ti1r;
    ptrdiff_t      ti1w;
    int            eob_run0;
    int            eob_run1;
    int            neobs1;
    int            token;
    int            eb;
    int            token1;
    int            eb1;
    /*TODO: Move this inline with reconstruction.*/
    oc_enc_pred_dc_rows(_enc,pli,0,_enc->state.fplanes[pli].nvfrags);
    dct_tokens0=_enc->dct_tokens[pli][0];
    dct_tokens1=_enc->dct_tokens[pli][1];
    extra_bits0=_enc->extra_bits[pli][0];
    extra_bits1=_enc->extra_bits[pli][1];
    ncoded_fragis+=_enc->state.ncoded_fragis[pli];
    ti0=ti1w=ti1r=0;
    eob_run0=eob_run1=neobs1=0;
    for(;fragii<ncoded_fragis;fragii++){
      int val;
      /*All tokens in the 1st AC coefficient stack are regenerated as the DC
         coefficients are produced.
        This can be done in-place; stack 1 cannot get larger.*/
      if(!neobs1){
        /*There's no active EOB run in stack 1; read the next token.*/
        token1=dct_tokens1[ti1r];
        eb1=extra_bits1[ti1r];
        ti1r++;
        if(token1<OC_NDCT_EOB_TOKEN_MAX){
          neobs1=oc_decode_eob_token(token1,eb1);
          /*It's an EOB run; add it to the current (inactive) one.
            Because we may have moved entries to stack 0, we may have an
             opportunity to merge two EOB runs in stack 1.*/
          eob_run1+=neobs1;
        }
      }
      val=frag_dc[coded_fragis[fragii]];
      if(val){
        /*There was a non-zero DC value, so there's no alteration to stack 1
           for this fragment; just code the stack 0 token.*/
        /*Flush any pending EOB run.*/
        if(eob_run0>0){
          token=oc_make_eob_token_full(eob_run0,&eb);
          dct_tokens0[ti0]=(unsigned char)token;
          extra_bits0[ti0]=(ogg_uint16_t)eb;
          ti0++;
          eob_run0=0;
        }
        token=oc_make_dct_token_full(0,0,val,&eb);
        dct_tokens0[ti0]=(unsigned char)token;
        extra_bits0[ti0]=(ogg_uint16_t)eb;
        ti0++;
      }
      else{
        /*Zero DC value; that means the entry in stack 1 might need to be coded
           from stack 0.
          This requires a stack 1 fixup.*/
        if(neobs1){
          /*We're in the middle of an active EOB run in stack 1.
            Move it to stack 0.*/
          if(++eob_run0>=4095){
            token=oc_make_eob_token_full(eob_run0,&eb);
            dct_tokens0[ti0]=(unsigned char)token;
            extra_bits0[ti0]=(ogg_uint16_t)eb;
            ti0++;
            eob_run0=0;
          }
          eob_run1--;
        }
        else{
          /*No active EOB run in stack 1, so we can't extend one in stack 0.
            Flush it if we've got it.*/
          if(eob_run0>0){
            token=oc_make_eob_token_full(eob_run0,&eb);
            dct_tokens0[ti0]=(unsigned char)token;
            extra_bits0[ti0]=(ogg_uint16_t)eb;
            ti0++;
            eob_run0=0;
          }
          /*Stack 1 token is one of: a pure zero run token, a single
             coefficient token, or a zero run/coefficient combo token.
            A zero run token is expanded and moved to token stack 0, and the
             stack 1 entry dropped.
            A single coefficient value may be transformed into combo token that
             is moved to stack 0, or if it cannot be combined, it is left alone
             and a single length-1 zero run is emitted in stack 0.
            A combo token is extended and moved to stack 0.
            During AC coding, we restrict the run lengths on combo tokens for
             stack 1 to guarantee we can extend them.*/
          switch(token1){
            case OC_DCT_SHORT_ZRL_TOKEN:{
              if(eb1<7){
                dct_tokens0[ti0]=OC_DCT_SHORT_ZRL_TOKEN;
                extra_bits0[ti0]=(ogg_uint16_t)(eb1+1);
                ti0++;
                /*Don't write the AC coefficient back out.*/
                continue;
              }
              /*Fall through.*/
            }
            case OC_DCT_ZRL_TOKEN:{
              dct_tokens0[ti0]=OC_DCT_ZRL_TOKEN;
              extra_bits0[ti0]=(ogg_uint16_t)(eb1+1);
              ti0++;
              /*Don't write the AC coefficient back out.*/
            }continue;
            case OC_ONE_TOKEN:
            case OC_MINUS_ONE_TOKEN:{
              dct_tokens0[ti0]=OC_DCT_RUN_CAT1A;
              extra_bits0[ti0]=(ogg_uint16_t)(token1-OC_ONE_TOKEN);
              ti0++;
              /*Don't write the AC coefficient back out.*/
            }continue;
            case OC_TWO_TOKEN:
            case OC_MINUS_TWO_TOKEN:{
              dct_tokens0[ti0]=OC_DCT_RUN_CAT2A;
              extra_bits0[ti0]=(ogg_uint16_t)(token1-OC_TWO_TOKEN<<1);
              ti0++;
              /*Don't write the AC coefficient back out.*/
            }continue;
            case OC_DCT_VAL_CAT2:{
              dct_tokens0[ti0]=OC_DCT_RUN_CAT2A;
              extra_bits0[ti0]=(ogg_uint16_t)((eb1<<1)+1);
              ti0++;
              /*Don't write the AC coefficient back out.*/
            }continue;
            case OC_DCT_RUN_CAT1A:
            case OC_DCT_RUN_CAT1A+1:
            case OC_DCT_RUN_CAT1A+2:
            case OC_DCT_RUN_CAT1A+3:{
              dct_tokens0[ti0]=(unsigned char)(token1+1);
              extra_bits0[ti0]=(ogg_uint16_t)eb1;
              ti0++;
              /*Don't write the AC coefficient back out.*/
            }continue;
            case OC_DCT_RUN_CAT1A+4:{
              dct_tokens0[ti0]=OC_DCT_RUN_CAT1B;
              extra_bits0[ti0]=(ogg_uint16_t)(eb1<<2);
              ti0++;
              /*Don't write the AC coefficient back out.*/
            }continue;
            case OC_DCT_RUN_CAT1B:{
              if((eb1&3)<3){
                dct_tokens0[ti0]=OC_DCT_RUN_CAT1B;
                extra_bits0[ti0]=(ogg_uint16_t)(eb1+1);
                ti0++;
                /*Don't write the AC coefficient back out.*/
                continue;
              }
              eb1=((eb1&4)<<1)-1;
              /*Fall through.*/
            }
            case OC_DCT_RUN_CAT1C:{
              dct_tokens0[ti0]=OC_DCT_RUN_CAT1C;
              extra_bits0[ti0]=(ogg_uint16_t)(eb1+1);
              ti0++;
              /*Don't write the AC coefficient back out.*/
            }continue;
            case OC_DCT_RUN_CAT2A:{
              eb1=(eb1<<1)-1;
              /*Fall through.*/
            }
            case OC_DCT_RUN_CAT2B:{
              dct_tokens0[ti0]=OC_DCT_RUN_CAT2B;
              extra_bits0[ti0]=(ogg_uint16_t)(eb1+1);
              ti0++;
              /*Don't write the AC coefficient back out.*/
            }continue;
          }
          /*We can't merge tokens, write a short zero run and keep going.*/
          dct_tokens0[ti0]=OC_DCT_SHORT_ZRL_TOKEN;
          extra_bits0[ti0]=0;
          ti0++;
        }
      }
      if(!neobs1){
        /*Flush any (inactive) EOB run.*/
        if(eob_run1>0){
          token=oc_make_eob_token_full(eob_run1,&eb);
          dct_tokens1[ti1w]=(unsigned char)token;
          extra_bits1[ti1w]=(ogg_uint16_t)eb;
          ti1w++;
          eob_run1=0;
        }
        /*There's no active EOB run, so log the current token.*/
        dct_tokens1[ti1w]=(unsigned char)token1;
        extra_bits1[ti1w]=(ogg_uint16_t)eb1;
        ti1w++;
      }
      else{
        /*Otherwise consume one EOB from the current run.*/
        neobs1--;
        /*If we have more than 4095 EOBs outstanding in stack1, flush the run.*/
        if(eob_run1-neobs1>=4095){
          token=oc_make_eob_token_full(4095,&eb);
          dct_tokens1[ti1w]=(unsigned char)token;
          extra_bits1[ti1w]=(ogg_uint16_t)eb;
          ti1w++;
          eob_run1-=4095;
        }
      }
    }
    /*Flush the trailing EOB runs.*/
    if(eob_run0>0){
      token=oc_make_eob_token_full(eob_run0,&eb);
      dct_tokens0[ti0]=(unsigned char)token;
      extra_bits0[ti0]=(ogg_uint16_t)eb;
      ti0++;
    }
    if(eob_run1>0){
      token=oc_make_eob_token_full(eob_run1,&eb);
      dct_tokens1[ti1w]=(unsigned char)token;
      extra_bits1[ti1w]=(ogg_uint16_t)eb;
      ti1w++;
    }
    _enc->ndct_tokens[pli][0]=ti0;
    _enc->ndct_tokens[pli][1]=ti1w;
  }
}

/*DC prediction, post-facto DC tokenization (has to be completed after DC
   predict), AC coefficient fix-ups and EOB run welding.*/
void oc_enc_tokenize_finish(oc_enc_ctx *_enc){
  int pli;
  int zzi;
  /*Emit final EOB runs for the AC coefficients.
    This must be done before we tokenize the DC coefficients, so we can
     properly track the 1st AC coefficient to the end of the list.*/
  for(pli=0;pli<3;pli++)for(zzi=1;zzi<64;zzi++){
    int eob_run;
    eob_run=_enc->eob_run[pli][zzi];
    if(eob_run>0)oc_enc_eob_log(_enc,pli,zzi,eob_run);
  }
  /*Fill in the DC token list and fix-up the 1st AC coefficient.*/
  oc_enc_tokenize_dc(_enc);
  /*Merge the final EOB run of one token list with the start of the next, if
     possible.*/
  for(zzi=0;zzi<64;zzi++)for(pli=0;pli<3;pli++){
    int       old_tok1;
    int       old_tok2;
    int       old_eb1;
    int       old_eb2;
    int       new_tok;
    int       new_eb;
    int       zzj;
    int       plj;
    ptrdiff_t ti;
    int       run_count;
    /*Make sure this coefficient has tokens at all.*/
    if(_enc->ndct_tokens[pli][zzi]<=0)continue;
    /*Ensure the first token is an EOB run.*/
    old_tok2=_enc->dct_tokens[pli][zzi][0];
    if(old_tok2>=OC_NDCT_EOB_TOKEN_MAX)continue;
    /*Search for a previous coefficient that has any tokens at all.*/
    old_tok1=OC_NDCT_EOB_TOKEN_MAX;
    for(zzj=zzi,plj=pli;zzj>=0;zzj--){
      while(plj-->0){
        ti=_enc->ndct_tokens[plj][zzj]-1;
        if(ti>=_enc->dct_token_offs[plj][zzj]){
          old_tok1=_enc->dct_tokens[plj][zzj][ti];
          break;
        }
      }
      if(plj>=0)break;
      plj=3;
    }
    /*Ensure its last token was an EOB run.*/
    if(old_tok1>=OC_NDCT_EOB_TOKEN_MAX)continue;
    /*Pull off the associated extra bits, if any, and decode the runs.*/
    /*ti is always initialized; if your compiler thinks otherwise, it is dumb.*/
    old_eb1=_enc->extra_bits[plj][zzj][ti];
    old_eb2=_enc->extra_bits[pli][zzi][0];
    run_count=oc_decode_eob_token(old_tok1,old_eb1)
     +oc_decode_eob_token(old_tok2,old_eb2);
    /*We can't possibly combine these into one run.
      It might be possible to split them more optimally, but we'll just leave
       them as-is.*/
    if(run_count>=4096)continue;
    /*We CAN combine them into one run.*/
    new_tok=oc_make_eob_token_full(run_count,&new_eb);
    _enc->dct_tokens[plj][zzj][ti]=(unsigned char)new_tok;
    _enc->extra_bits[plj][zzj][ti]=(ogg_uint16_t)new_eb;
    _enc->dct_token_offs[pli][zzi]++;
  }
}
