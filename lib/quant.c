/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2003                *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

  function:
  last mod: $Id: quant.c,v 1.20 2004/03/19 05:13:56 giles Exp $

 ********************************************************************/

//#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "encoder_internal.h"
#include "quant_lookup.h"

static const ogg_uint32_t QThreshTableV1[Q_TABLE_SIZE] = {
  500,  450,  400,  370,  340,  310, 285, 265,
  245,  225,  210,  195,  185,  180, 170, 160,
  150,  145,  135,  130,  125,  115, 110, 107,
  100,   96,   93,   89,   85,   82,  75,  74,
  70,   68,   64,   60,   57,   56,  52,  50,
  49,   45,   44,   43,   40,   38,  37,  35,
  33,   32,   30,   29,   28,   25,  24,  22,
  21,   19,   18,   17,   15,   13,  12,  10
};

static const Q_LIST_ENTRY DcScaleFactorTableV1[ Q_TABLE_SIZE ] = {
  220, 200, 190, 180, 170, 170, 160, 160,
  150, 150, 140, 140, 130, 130, 120, 120,
  110, 110, 100, 100, 90,  90,  90,  80,
  80,  80,  70,  70,  70,  60,  60,  60,
  60,  50,  50,  50,  50,  40,  40,  40,
  40,  40,  30,  30,  30,  30,  30,  30,
  30,  20,  20,  20,  20,  20,  20,  20,
  20,  10,  10,  10,  10,  10,  10,  10
};

/* dbm -- defined some alternative tables to test header packing */
#define NEW_QTABLES 0
#if NEW_QTABLES

static const Q_LIST_ENTRY Y_coeffsV1[64] =
{
        8,  16,  16,  16,  20,  20,  20,  20,
        16,  16,  16,  16,  20,  20,  20,  20,
        16,  16,  16,  16,  22,  22,  22,  22,
        16,  16,  16,  16,  22,  22,  22,  22,
        20,  20,  22,  22,  24,  24,  24,  24,
        20,  20,  22,  22,  24,  24,  24,  24,
        20,  20,  22,  22,  24,  24,  24,  24,
        20,  20,  22,  22,  24,  24,  24,  24
};

static const Q_LIST_ENTRY UV_coeffsV1[64] =
{       17,     18,     24,     47,     99,     99,     99,     99,
        18,     21,     26,     66,     99,     99,     99,     99,
        24,     26,     56,     99,     99,     99,     99,     99,
        47,     66,     99,     99,     99,     99,     99,     99,
        99,     99,     99,     99,     99,     99,     99,     99,
        99,     99,     99,     99,     99,     99,     99,     99,
        99,     99,     99,     99,     99,     99,     99,     99,
        99,     99,     99,     99,     99,     99,     99,     99
};

/* Different matrices for different encoder versions */
static const Q_LIST_ENTRY Inter_coeffsV1[64] =
{
        12, 16,  16,  16,  20,  20,  20,  20,
        16,  16,  16,  16,  20,  20,  20,  20,
        16,  16,  16,  16,  22,  22,  22,  22,
        16,  16,  16,  16,  22,  22,  22,  22,
        20,  20,  22,  22,  24,  24,  24,  24,
        20,  20,  22,  22,  24,  24,  24,  24,
        20,  20,  22,  22,  24,  24,  24,  24,
        20,  20,  22,  22,  24,  24,  24,  24
};

#else /* these are the old VP3 values: */

static const Q_LIST_ENTRY Y_coeffsV1[64] ={
  16,  11,  10,  16,  24,  40,  51,  61,
  12,  12,  14,  19,  26,  58,  60,  55,
  14,  13,  16,  24,  40,  57,  69,  56,
  14,  17,  22,  29,  51,  87,  80,  62,
  18,  22,  37,  58,  68, 109, 103,  77,
  24,  35,  55,  64,  81, 104, 113,  92,
  49,  64,  78,  87, 103, 121, 120, 101,
  72,  92,  95,  98, 112, 100, 103,  99
};

static const Q_LIST_ENTRY UV_coeffsV1[64] ={
  17,   18,     24,     47,     99,     99,     99,     99,
  18,   21,     26,     66,     99,     99,     99,     99,
  24,   26,     56,     99,     99,     99,     99,     99,
  47,   66,     99,     99,     99,     99,     99,     99,
  99,   99,     99,     99,     99,     99,     99,     99,
  99,   99,     99,     99,     99,     99,     99,     99,
  99,   99,     99,     99,     99,     99,     99,     99,
  99,   99,     99,     99,     99,     99,     99,     99
};

/* Different matrices for different encoder versions */
static const Q_LIST_ENTRY Inter_coeffsV1[64] ={
  16,  16,  16,  20,  24,  28,  32,  40,
  16,  16,  20,  24,  28,  32,  40,  48,
  16,  20,  24,  28,  32,  40,  48,  64,
  20,  24,  28,  32,  40,  48,  64,  64,
  24,  28,  32,  40,  48,  64,  64,  64,
  28,  32,  40,  48,  64,  64,  64,  96,
  32,  40,  48,  64,  64,  64,  96,  128,
  40,  48,  64,  64,  64,  96,  128, 128
};

#endif

static int _ilog(unsigned int v){
  int ret=0;
  while(v){
    ret++;
    v>>=1;
  }
  return(ret);
}

void WriteQTables(PB_INSTANCE *pbi,oggpack_buffer* opb) {
  int x, bits;
  bits=10;
  oggpackB_write(opb, bits-1, 4);
  for(x=0; x<64; x++) {
    oggpackB_write(opb, pbi->QThreshTable[x],bits);
  }
  oggpackB_write(opb, bits-1, 4);
  for(x=0; x<64; x++) {
    oggpackB_write(opb, pbi->DcScaleFactorTable[x],bits);
  }
  oggpackB_write(opb, 3 - 1, 9); /* number of base matricies */
  for(x=0; x<64; x++) {
    oggpackB_write(opb, pbi->Y_coeffs[x],8);
  }
  for(x=0; x<64; x++) {
    oggpackB_write(opb, pbi->UV_coeffs[x],8);
  }
  for(x=0; x<64; x++) {
    oggpackB_write(opb, pbi->Inter_coeffs[x],8);
  }
  /* table mapping */
  oggpackB_write(opb, 0, 2);  /* matrix 0 for intra Y */
  oggpackB_write(opb, 62, 6); /* used for every q */
  oggpackB_write(opb, 0, 2);
  oggpackB_write(opb, 1, 1);  /* next range is explicit */
  oggpackB_write(opb, 1, 2);  /* matrix 1 for intra U */
  oggpackB_write(opb, 62, 6);
  oggpackB_write(opb, 1, 2);
  oggpackB_write(opb, 0, 1);  /* intra V is the same */
  oggpackB_write(opb, 1, 1);  /* next range is explicit */
  oggpackB_write(opb, 2, 2);  /* matrix 2 for inter Y */
  oggpackB_write(opb, 62, 6);
  oggpackB_write(opb, 2, 2);
  oggpackB_write(opb, 0, 2);  /* inter U the same */
  oggpackB_write(opb, 0, 2);  /* inter V the same */
}

static int _read_qtable_range(codec_setup_info *ci, oggpack_buffer* opb, int N) 
{
  int index, range;
  int qi = 0;

  theora_read(opb,_ilog(N-1),&index); /* qi=0 index */
  //fprintf(stderr, " [%d]",index);
  while(qi<63) {
    theora_read(opb,_ilog(62-qi),&range); /* range to next code q matrix */
    range++;
    //fprintf(stderr," %d",range);
    if(range<=0) return OC_BADHEADER;
    qi+=range;
    theora_read(opb,_ilog(N-1),&index); /* next index */
    //fprintf(stderr, " [%d]",index);
  }

  return 0;
}

int ReadQTables(codec_setup_info *ci, oggpack_buffer* opb) {
  long bits,value;
  int x,y, N;
  //fprintf(stderr, "Reading Q tables...\n");
  /* AC scale table */
  theora_read(opb,4,&bits); bits++;
  for(x=0; x<Q_TABLE_SIZE; x++) {
    theora_read(opb,bits,&value);
    if(bits<0)return OC_BADHEADER;
    ci->QThreshTable[x]=value;
  }
  /* DC scale table */
  theora_read(opb,4,&bits); bits++;
  for(x=0; x<Q_TABLE_SIZE; x++) {
    theora_read(opb,bits,&value);
    if(bits<0)return OC_BADHEADER;
    ci->DcScaleFactorTable[x]=(Q_LIST_ENTRY)value;
  }
  /* base matricies */
  theora_read(opb,9,&N); N++;
  //fprintf(stderr, "  max q matrix index %d\n", N);
  if(N!=3)return OC_BADHEADER; /* we only support the VP3 config */
  ci->qmats=_ogg_malloc(N*64*sizeof(Q_LIST_ENTRY));
  ci->MaxQMatrixIndex = N;
  for(y=0; y<N; y++) {
    //fprintf(stderr," q matrix %d:\n  ", y);
    for(x=0; x<64; x++) {
      theora_read(opb,8,&value);
      if(bits<0)return OC_BADHEADER;
      ci->qmats[(y<<6)+x]=(Q_LIST_ENTRY)value;
      //fprintf(stderr," %03d", (Q_LIST_ENTRY)value);
      //if((x+1)%8==0)fprintf(stderr,"\n  ");
    }
    //fprintf(stderr,"\n");
  }
  /* table mapping */
  {
    int flag, ret;
    /* intra Y */
    //fprintf(stderr,"\n Intra Y:");
    if((ret=_read_qtable_range(ci,opb,N))<0) return ret;
    /* intra U */
    //fprintf(stderr, "\n Intra U:");
    theora_read(opb,1,&flag);
    if(flag<0) return OC_BADHEADER;
    if(flag) {
      /* explicitly coded */
      if((ret=_read_qtable_range(ci,opb,N))<0) return ret;
    } else {
      /* same as previous */
      //fprintf(stderr," same as above");
    }
    /* intra V */
    //fprintf(stderr,"\n Intra V:");
    theora_read(opb,1,&flag);
    if(flag<0) return OC_BADHEADER;
    if(flag) {
      /* explicitly coded */
      if((ret=_read_qtable_range(ci,opb,N))<0) return ret;
    } else {
       /* same as previous */
      //fprintf(stderr," same as above");
    }
    /* inter Y */
    //fprintf(stderr,"\n Inter Y:");
    theora_read(opb,1,&flag);
    if(flag<0) return OC_BADHEADER;
    if(flag) {
      /* explicitly coded */
      if((ret=_read_qtable_range(ci,opb,N))<0) return ret;
    } else {
      theora_read(opb,1,&flag);
      if(flag<0) return OC_BADHEADER;
      if(flag) {
        /* same as corresponding intra */
        //fprintf(stderr," same as intra");
      } else {
        /* same as previous */
        //fprintf(stderr," same as above");
      }
    }
    /* inter U */
    //fprintf(stderr,"\n Inter U:");
    theora_read(opb,1,&flag);
    if(flag<0) return OC_BADHEADER;
    if(flag) {
      /* explicitly coded */
      if((ret=_read_qtable_range(ci,opb,N))<0) return ret;
    } else {
      theora_read(opb,1,&flag);
      if(flag<0) return OC_BADHEADER;
      if(flag) {
        /* same as corresponding intra */
        //fprintf(stderr," same as intra");
      } else {
        /* same as previous */
        //fprintf(stderr," same as above");
      }
    }
    /* inter V */
    //fprintf(stderr,"\n Inter V:");
    theora_read(opb,1,&flag);
    if(flag<0) return OC_BADHEADER;
    if(flag) {
      /* explicitly coded */
      if((ret=_read_qtable_range(ci,opb,N))<0) return ret;
    } else {
      theora_read(opb,1,&flag);
      if(flag<0) return OC_BADHEADER;
      if(flag) {
        /* same as corresponding intra */
        //fprintf(stderr," same as intra");
      } else {
        /* same as previous */
        //fprintf(stderr," same as above");
      }
    }
    //fprintf(stderr,"\n");
  }
  
  /* ignore the range table and reference the matricies we use */
  memcpy(ci->Y_coeffs, &ci->qmats[0], sizeof(ci->Y_coeffs));
  memcpy(ci->UV_coeffs, &ci->qmats[64], sizeof(ci->UV_coeffs));
  memcpy(ci->Inter_coeffs, &ci->qmats[2*64], sizeof(ci->Inter_coeffs));

  return 0;
}

void CopyQTables(PB_INSTANCE *pbi, codec_setup_info *ci) {
  memcpy(pbi->QThreshTable, ci->QThreshTable, sizeof(pbi->QThreshTable));
  memcpy(pbi->DcScaleFactorTable, ci->DcScaleFactorTable,
         sizeof(pbi->DcScaleFactorTable));
  memcpy(pbi->Y_coeffs, ci->Y_coeffs, sizeof(pbi->Y_coeffs));
  memcpy(pbi->UV_coeffs, ci->UV_coeffs, sizeof(pbi->UV_coeffs));
  memcpy(pbi->Inter_coeffs, ci->Inter_coeffs, sizeof(pbi->Inter_coeffs));
}

/* Initialize custom qtables using the VP31 values.
   Someday we can change the quant tables to be adaptive, or just plain
    better.*/
void InitQTables( PB_INSTANCE *pbi ){
  memcpy(pbi->QThreshTable, QThreshTableV1, sizeof(pbi->QThreshTable));
  memcpy(pbi->DcScaleFactorTable, DcScaleFactorTableV1,
         sizeof(pbi->DcScaleFactorTable));
  memcpy(pbi->Y_coeffs, Y_coeffsV1, sizeof(pbi->Y_coeffs));
  memcpy(pbi->UV_coeffs, UV_coeffsV1, sizeof(pbi->UV_coeffs));
  memcpy(pbi->Inter_coeffs, Inter_coeffsV1, sizeof(pbi->Inter_coeffs));
}

static void BuildQuantIndex_Generic(PB_INSTANCE *pbi){
  ogg_int32_t i,j;

  /* invert the dequant index into the quant index */
  for ( i = 0; i < BLOCK_SIZE; i++ ){
    j = dequant_index[i];
    pbi->quant_index[j] = i;
  }
}

static void init_quantizer ( CP_INSTANCE *cpi,
                      ogg_uint32_t scale_factor,
                      unsigned char QIndex ){
    int i;
    double ZBinFactor;
    double RoundingFactor;

    double temp_fp_quant_coeffs;
    double temp_fp_quant_round;
    double temp_fp_ZeroBinSize;
    PB_INSTANCE *pbi = &cpi->pb;

    const Q_LIST_ENTRY * Inter_coeffs;
    const Q_LIST_ENTRY * Y_coeffs;
    const Q_LIST_ENTRY * UV_coeffs;
    const Q_LIST_ENTRY * DcScaleFactorTable;
    const Q_LIST_ENTRY * UVDcScaleFactorTable;

    /* Notes on setup of quantisers.  The initial multiplication by
     the scale factor is done in the ogg_int32_t domain to insure that the
     precision in the quantiser is the same as in the inverse
     quantiser where all calculations are integer.  The "<< 2" is a
     normalisation factor for the forward DCT transform. */

    /* New version rounding and ZB characteristics. */
    Inter_coeffs = Inter_coeffsV1;
    Y_coeffs = Y_coeffsV1;
    UV_coeffs = UV_coeffsV1;
    DcScaleFactorTable = DcScaleFactorTableV1;
    UVDcScaleFactorTable = DcScaleFactorTableV1;
    ZBinFactor = 0.9;

    switch(cpi->pb.info.sharpness){
    case 0:
      ZBinFactor = 0.65;
      if ( scale_factor <= 50 )
        RoundingFactor = 0.499;
      else
        RoundingFactor = 0.46;
      break;
    case 1:
      ZBinFactor = 0.75;
      if ( scale_factor <= 50 )
        RoundingFactor = 0.476;
      else
        RoundingFactor = 0.400;
      break;

    default:
      ZBinFactor = 0.9;
      if ( scale_factor <= 50 )
        RoundingFactor = 0.476;
      else
        RoundingFactor = 0.333;
      break;
    }

    /* Use fixed multiplier for intra Y DC */
    temp_fp_quant_coeffs =
      (((ogg_uint32_t)(DcScaleFactorTable[QIndex] * Y_coeffs[0])/100) << 2);
    if ( temp_fp_quant_coeffs < MIN_LEGAL_QUANT_ENTRY * 2 )
      temp_fp_quant_coeffs = MIN_LEGAL_QUANT_ENTRY * 2;

    temp_fp_quant_round = temp_fp_quant_coeffs * RoundingFactor;
    pbi->fp_quant_Y_round[0]    = (ogg_int32_t) (0.5 + temp_fp_quant_round);

    temp_fp_ZeroBinSize = temp_fp_quant_coeffs * ZBinFactor;
    pbi->fp_ZeroBinSize_Y[0]    = (ogg_int32_t) (0.5 + temp_fp_ZeroBinSize);

    temp_fp_quant_coeffs = 1.0 / temp_fp_quant_coeffs;
    pbi->fp_quant_Y_coeffs[0]   = (0.5 + SHIFT16 * temp_fp_quant_coeffs);

    /* Intra UV */
    temp_fp_quant_coeffs =
      (((ogg_uint32_t)(UVDcScaleFactorTable[QIndex] * UV_coeffs[0])/100) << 2);
    if ( temp_fp_quant_coeffs < MIN_LEGAL_QUANT_ENTRY * 2)
      temp_fp_quant_coeffs = MIN_LEGAL_QUANT_ENTRY * 2;

    temp_fp_quant_round = temp_fp_quant_coeffs * RoundingFactor;
    pbi->fp_quant_UV_round[0]   = (0.5 + temp_fp_quant_round);

    temp_fp_ZeroBinSize = temp_fp_quant_coeffs * ZBinFactor;
    pbi->fp_ZeroBinSize_UV[0]   = (0.5 + temp_fp_ZeroBinSize);

    temp_fp_quant_coeffs = 1.0 / temp_fp_quant_coeffs;
    pbi->fp_quant_UV_coeffs[0]= (0.5 + SHIFT16 * temp_fp_quant_coeffs);

    /* Inter Y */
    temp_fp_quant_coeffs =
      (((ogg_uint32_t)(DcScaleFactorTable[QIndex] * Inter_coeffs[0])/100) << 2);
    if ( temp_fp_quant_coeffs < MIN_LEGAL_QUANT_ENTRY * 4)
      temp_fp_quant_coeffs = MIN_LEGAL_QUANT_ENTRY * 4;

    temp_fp_quant_round = temp_fp_quant_coeffs * RoundingFactor;
    pbi->fp_quant_Inter_round[0]= (0.5 + temp_fp_quant_round);

    temp_fp_ZeroBinSize = temp_fp_quant_coeffs * ZBinFactor;
    pbi->fp_ZeroBinSize_Inter[0]= (0.5 + temp_fp_ZeroBinSize);

    temp_fp_quant_coeffs= 1.0 / temp_fp_quant_coeffs;
    pbi->fp_quant_Inter_coeffs[0]= (0.5 + SHIFT16 * temp_fp_quant_coeffs);

    /* Inter UV */
    temp_fp_quant_coeffs =
      (((ogg_uint32_t)(UVDcScaleFactorTable[QIndex] * Inter_coeffs[0])/100) << 2);
    if ( temp_fp_quant_coeffs < MIN_LEGAL_QUANT_ENTRY * 4)
      temp_fp_quant_coeffs = MIN_LEGAL_QUANT_ENTRY * 4;

    temp_fp_quant_round = temp_fp_quant_coeffs * RoundingFactor;
    pbi->fp_quant_InterUV_round[0]= (0.5 + temp_fp_quant_round);

    temp_fp_ZeroBinSize = temp_fp_quant_coeffs * ZBinFactor;
    pbi->fp_ZeroBinSize_InterUV[0]= (0.5 + temp_fp_ZeroBinSize);

    temp_fp_quant_coeffs= 1.0 / temp_fp_quant_coeffs;
    pbi->fp_quant_InterUV_coeffs[0]=
      (0.5 + SHIFT16 * temp_fp_quant_coeffs);

    for ( i = 1; i < 64; i++ ){
      /* now scale coefficients by required compression factor */
      /* Intra Y */
      temp_fp_quant_coeffs =
        (((ogg_uint32_t)(scale_factor * Y_coeffs[i]) / 100 ) << 2 );
      if ( temp_fp_quant_coeffs < (MIN_LEGAL_QUANT_ENTRY) )
        temp_fp_quant_coeffs = (MIN_LEGAL_QUANT_ENTRY);

      temp_fp_quant_round = temp_fp_quant_coeffs * RoundingFactor;
      pbi->fp_quant_Y_round[i]  = (0.5 + temp_fp_quant_round);

      temp_fp_ZeroBinSize = temp_fp_quant_coeffs * ZBinFactor;
      pbi->fp_ZeroBinSize_Y[i]  = (0.5 + temp_fp_ZeroBinSize);

      temp_fp_quant_coeffs = 1.0 / temp_fp_quant_coeffs;
      pbi->fp_quant_Y_coeffs[i] = (0.5 + SHIFT16 * temp_fp_quant_coeffs);

      /* Intra UV */
      temp_fp_quant_coeffs =
        (((ogg_uint32_t)(scale_factor * UV_coeffs[i]) / 100 ) << 2 );
      if ( temp_fp_quant_coeffs < (MIN_LEGAL_QUANT_ENTRY))
        temp_fp_quant_coeffs = (MIN_LEGAL_QUANT_ENTRY);

      temp_fp_quant_round = temp_fp_quant_coeffs * RoundingFactor;
      pbi->fp_quant_UV_round[i] = (0.5 + temp_fp_quant_round);

      temp_fp_ZeroBinSize = temp_fp_quant_coeffs * ZBinFactor;
      pbi->fp_ZeroBinSize_UV[i] = (0.5 + temp_fp_ZeroBinSize);

      temp_fp_quant_coeffs = 1.0 / temp_fp_quant_coeffs;
      pbi->fp_quant_UV_coeffs[i]= (0.5 + SHIFT16 * temp_fp_quant_coeffs);

      /* Inter Y */
      temp_fp_quant_coeffs =
        (((ogg_uint32_t)(scale_factor * Inter_coeffs[i]) / 100 ) << 2 );
      if ( temp_fp_quant_coeffs < (MIN_LEGAL_QUANT_ENTRY * 2) )
        temp_fp_quant_coeffs = (MIN_LEGAL_QUANT_ENTRY * 2);

      temp_fp_quant_round = temp_fp_quant_coeffs * RoundingFactor;
      pbi->fp_quant_Inter_round[i]= (0.5 + temp_fp_quant_round);

      temp_fp_ZeroBinSize = temp_fp_quant_coeffs * ZBinFactor;
      pbi->fp_ZeroBinSize_Inter[i]= (0.5 + temp_fp_ZeroBinSize);

      temp_fp_quant_coeffs = 1.0 / temp_fp_quant_coeffs;
      pbi->fp_quant_Inter_coeffs[i]= (0.5 + SHIFT16 * temp_fp_quant_coeffs);

      /* Inter UV */
      temp_fp_quant_coeffs =
        (((ogg_uint32_t)(scale_factor * Inter_coeffs[i]) / 100 ) << 2 );
      if ( temp_fp_quant_coeffs < (MIN_LEGAL_QUANT_ENTRY * 2) )
        temp_fp_quant_coeffs = (MIN_LEGAL_QUANT_ENTRY * 2);

      temp_fp_quant_round = temp_fp_quant_coeffs * RoundingFactor;
      pbi->fp_quant_InterUV_round[i]= (0.5 + temp_fp_quant_round);

      temp_fp_ZeroBinSize = temp_fp_quant_coeffs * ZBinFactor;
      pbi->fp_ZeroBinSize_InterUV[i]= (0.5 + temp_fp_ZeroBinSize);

      temp_fp_quant_coeffs = 1.0 / temp_fp_quant_coeffs;
      pbi->fp_quant_InterUV_coeffs[i]= (0.5 + SHIFT16 * temp_fp_quant_coeffs);

    }

    pbi->fquant_coeffs = pbi->fp_quant_Y_coeffs;

}

void select_Y_quantiser ( PB_INSTANCE *pbi ){
  pbi->fquant_coeffs = pbi->fp_quant_Y_coeffs;
  pbi->fquant_round = pbi->fp_quant_Y_round;
  pbi->fquant_ZbSize = pbi->fp_ZeroBinSize_Y;
}

void select_Inter_quantiser ( PB_INSTANCE *pbi ){
  pbi->fquant_coeffs = pbi->fp_quant_Inter_coeffs;
  pbi->fquant_round = pbi->fp_quant_Inter_round;
  pbi->fquant_ZbSize = pbi->fp_ZeroBinSize_Inter;
}

void select_UV_quantiser ( PB_INSTANCE *pbi ){
  pbi->fquant_coeffs = pbi->fp_quant_UV_coeffs;
  pbi->fquant_round = pbi->fp_quant_UV_round;
  pbi->fquant_ZbSize = pbi->fp_quant_UV_round;
}

void select_InterUV_quantiser ( PB_INSTANCE *pbi ){
  pbi->fquant_coeffs = pbi->fp_quant_InterUV_coeffs;
  pbi->fquant_round = pbi->fp_quant_InterUV_round;
  pbi->fquant_ZbSize = pbi->fp_ZeroBinSize_InterUV;
}

void quantize( PB_INSTANCE *pbi,
               ogg_int16_t * DCT_block,
               Q_LIST_ENTRY * quantized_list){
  ogg_uint32_t          i;              /*      Row index */
  Q_LIST_ENTRY  val;            /* Quantised value. */

  ogg_int32_t * FquantRoundPtr = pbi->fquant_round;
  ogg_int32_t * FquantCoeffsPtr = pbi->fquant_coeffs;
  ogg_int32_t * FquantZBinSizePtr = pbi->fquant_ZbSize;
  ogg_int16_t * DCT_blockPtr = DCT_block;
  ogg_uint32_t * QIndexPtr = (ogg_uint32_t *)pbi->quant_index;
  ogg_int32_t temp;

  /* Set the quantized_list to default to 0 */
  memset( quantized_list, 0, 64 * sizeof(Q_LIST_ENTRY) );

  /* Note that we add half divisor to effect rounding on positive number */
  for( i = 0; i < VFRAGPIXELS; i++) {
    /* Column 0  */
    if ( DCT_blockPtr[0] >= FquantZBinSizePtr[0] ) {
      temp = FquantCoeffsPtr[0] * ( DCT_blockPtr[0] + FquantRoundPtr[0] ) ;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[QIndexPtr[0]] = ( val > 511 ) ? 511 : val;
    } else if ( DCT_blockPtr[0] <= -FquantZBinSizePtr[0] ) {
      temp = FquantCoeffsPtr[0] *
        ( DCT_blockPtr[0] - FquantRoundPtr[0] ) + MIN16;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[QIndexPtr[0]] = ( val < -511 ) ? -511 : val;
    }

    /* Column 1 */
    if ( DCT_blockPtr[1] >= FquantZBinSizePtr[1] ) {
      temp = FquantCoeffsPtr[1] *
        ( DCT_blockPtr[1] + FquantRoundPtr[1] ) ;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[QIndexPtr[1]] = ( val > 511 ) ? 511 : val;
    } else if ( DCT_blockPtr[1] <= -FquantZBinSizePtr[1] ) {
      temp = FquantCoeffsPtr[1] *
        ( DCT_blockPtr[1] - FquantRoundPtr[1] ) + MIN16;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[QIndexPtr[1]] = ( val < -511 ) ? -511 : val;
    }

    /* Column 2 */
    if ( DCT_blockPtr[2] >= FquantZBinSizePtr[2] ) {
      temp = FquantCoeffsPtr[2] *
        ( DCT_blockPtr[2] + FquantRoundPtr[2] ) ;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[QIndexPtr[2]] = ( val > 511 ) ? 511 : val;
    } else if ( DCT_blockPtr[2] <= -FquantZBinSizePtr[2] ) {
      temp = FquantCoeffsPtr[2] *
        ( DCT_blockPtr[2] - FquantRoundPtr[2] ) + MIN16;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[QIndexPtr[2]] = ( val < -511 ) ? -511 : val;
    }

    /* Column 3 */
    if ( DCT_blockPtr[3] >= FquantZBinSizePtr[3] ) {
      temp = FquantCoeffsPtr[3] *
        ( DCT_blockPtr[3] + FquantRoundPtr[3] ) ;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[QIndexPtr[3]] = ( val > 511 ) ? 511 : val;
    } else if ( DCT_blockPtr[3] <= -FquantZBinSizePtr[3] ) {
      temp = FquantCoeffsPtr[3] *
        ( DCT_blockPtr[3] - FquantRoundPtr[3] ) + MIN16;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[QIndexPtr[3]] = ( val < -511 ) ? -511 : val;
    }

    /* Column 4 */
    if ( DCT_blockPtr[4] >= FquantZBinSizePtr[4] ) {
      temp = FquantCoeffsPtr[4] *
        ( DCT_blockPtr[4] + FquantRoundPtr[4] ) ;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[QIndexPtr[4]] = ( val > 511 ) ? 511 : val;
    } else if ( DCT_blockPtr[4] <= -FquantZBinSizePtr[4] ) {
      temp = FquantCoeffsPtr[4] *
        ( DCT_blockPtr[4] - FquantRoundPtr[4] ) + MIN16;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[QIndexPtr[4]] = ( val < -511 ) ? -511 : val;
    }

    /* Column 5 */
    if ( DCT_blockPtr[5] >= FquantZBinSizePtr[5] ) {
      temp = FquantCoeffsPtr[5] *
        ( DCT_blockPtr[5] + FquantRoundPtr[5] ) ;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[QIndexPtr[5]] = ( val > 511 ) ? 511 : val;
    } else if ( DCT_blockPtr[5] <= -FquantZBinSizePtr[5] ) {
      temp = FquantCoeffsPtr[5] *
        ( DCT_blockPtr[5] - FquantRoundPtr[5] ) + MIN16;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[QIndexPtr[5]] = ( val < -511 ) ? -511 : val;
    }

    /* Column 6 */
    if ( DCT_blockPtr[6] >= FquantZBinSizePtr[6] ) {
      temp = FquantCoeffsPtr[6] *
        ( DCT_blockPtr[6] + FquantRoundPtr[6] ) ;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[QIndexPtr[6]] = ( val > 511 ) ? 511 : val;
    } else if ( DCT_blockPtr[6] <= -FquantZBinSizePtr[6] ) {
      temp = FquantCoeffsPtr[6] *
        ( DCT_blockPtr[6] - FquantRoundPtr[6] ) + MIN16;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[QIndexPtr[6]] = ( val < -511 ) ? -511 : val;
    }

    /* Column 7 */
    if ( DCT_blockPtr[7] >= FquantZBinSizePtr[7] ) {
      temp = FquantCoeffsPtr[7] *
        ( DCT_blockPtr[7] + FquantRoundPtr[7] ) ;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[QIndexPtr[7]] = ( val > 511 ) ? 511 : val;
    } else if ( DCT_blockPtr[7] <= -FquantZBinSizePtr[7] ) {
      temp = FquantCoeffsPtr[7] *
        ( DCT_blockPtr[7] - FquantRoundPtr[7] ) + MIN16;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[QIndexPtr[7]] = ( val < -511 ) ? -511 : val;
    }

    FquantRoundPtr += 8;
    FquantCoeffsPtr += 8;
    FquantZBinSizePtr += 8;
    DCT_blockPtr += 8;
    QIndexPtr += 8;
  }
}

static void init_dequantizer ( PB_INSTANCE *pbi,
                        ogg_uint32_t scale_factor,
                        unsigned char  QIndex ){
  int i, j;

  Q_LIST_ENTRY * Inter_coeffs;
  Q_LIST_ENTRY * Y_coeffs;
  Q_LIST_ENTRY * UV_coeffs;
  Q_LIST_ENTRY * DcScaleFactorTable;
  Q_LIST_ENTRY * UVDcScaleFactorTable;

  Inter_coeffs = pbi->Inter_coeffs;
  Y_coeffs = pbi->Y_coeffs;
  UV_coeffs = pbi->UV_coeffs;
  DcScaleFactorTable = pbi->DcScaleFactorTable;
  UVDcScaleFactorTable = pbi->DcScaleFactorTable;

  /* invert the dequant index into the quant index
     the dxer has a different order than the cxer. */
  BuildQuantIndex_Generic(pbi);

  /* Reorder dequantisation coefficients into dct zigzag order. */
  for ( i = 0; i < BLOCK_SIZE; i++ ) {
    j = pbi->quant_index[i];
    pbi->dequant_Y_coeffs[j] = Y_coeffs[i];
  }
  for ( i = 0; i < BLOCK_SIZE; i++ ){
    j = pbi->quant_index[i];
    pbi->dequant_Inter_coeffs[j] = Inter_coeffs[i];
  }
  for ( i = 0; i < BLOCK_SIZE; i++ ){
    j = pbi->quant_index[i];
    pbi->dequant_UV_coeffs[j] = UV_coeffs[i];
  }
  for ( i = 0; i < BLOCK_SIZE; i++ ){
    j = pbi->quant_index[i];
    pbi->dequant_InterUV_coeffs[j] = Inter_coeffs[i];
  }

  /* Intra Y */
  pbi->dequant_Y_coeffs[0] =
    ((DcScaleFactorTable[QIndex] * pbi->dequant_Y_coeffs[0])/100);
  if ( pbi->dequant_Y_coeffs[0] < MIN_DEQUANT_VAL * 2 )
    pbi->dequant_Y_coeffs[0] = MIN_DEQUANT_VAL * 2;
  pbi->dequant_Y_coeffs[0] =
    pbi->dequant_Y_coeffs[0] << IDCT_SCALE_FACTOR;

  /* Intra UV */
  pbi->dequant_UV_coeffs[0] =
    ((UVDcScaleFactorTable[QIndex] * pbi->dequant_UV_coeffs[0])/100);
  if ( pbi->dequant_UV_coeffs[0] < MIN_DEQUANT_VAL * 2 )
    pbi->dequant_UV_coeffs[0] = MIN_DEQUANT_VAL * 2;
  pbi->dequant_UV_coeffs[0] =
    pbi->dequant_UV_coeffs[0] << IDCT_SCALE_FACTOR;

  /* Inter Y */
  pbi->dequant_Inter_coeffs[0] =
    ((DcScaleFactorTable[QIndex] * pbi->dequant_Inter_coeffs[0])/100);
  if ( pbi->dequant_Inter_coeffs[0] < MIN_DEQUANT_VAL * 4 )
    pbi->dequant_Inter_coeffs[0] = MIN_DEQUANT_VAL * 4;
  pbi->dequant_Inter_coeffs[0] =
    pbi->dequant_Inter_coeffs[0] << IDCT_SCALE_FACTOR;

  /* Inter UV */
  pbi->dequant_InterUV_coeffs[0] =
    ((UVDcScaleFactorTable[QIndex] * pbi->dequant_InterUV_coeffs[0])/100);
  if ( pbi->dequant_InterUV_coeffs[0] < MIN_DEQUANT_VAL * 4 )
    pbi->dequant_InterUV_coeffs[0] = MIN_DEQUANT_VAL * 4;
  pbi->dequant_InterUV_coeffs[0] =
    pbi->dequant_InterUV_coeffs[0] << IDCT_SCALE_FACTOR;

  for ( i = 1; i < 64; i++ ){
    /* now scale coefficients by required compression factor */
    pbi->dequant_Y_coeffs[i] =
      (( scale_factor * pbi->dequant_Y_coeffs[i] ) / 100);
    if ( pbi->dequant_Y_coeffs[i] < MIN_DEQUANT_VAL )
      pbi->dequant_Y_coeffs[i] = MIN_DEQUANT_VAL;
    pbi->dequant_Y_coeffs[i] =
      pbi->dequant_Y_coeffs[i] << IDCT_SCALE_FACTOR;

    pbi->dequant_UV_coeffs[i] =
      (( scale_factor * pbi->dequant_UV_coeffs[i] ) / 100);
    if ( pbi->dequant_UV_coeffs[i] < MIN_DEQUANT_VAL )
      pbi->dequant_UV_coeffs[i] = MIN_DEQUANT_VAL;
    pbi->dequant_UV_coeffs[i] =
      pbi->dequant_UV_coeffs[i] << IDCT_SCALE_FACTOR;

    pbi->dequant_Inter_coeffs[i] =
      (( scale_factor * pbi->dequant_Inter_coeffs[i] ) / 100);
    if ( pbi->dequant_Inter_coeffs[i] < (MIN_DEQUANT_VAL * 2) )
      pbi->dequant_Inter_coeffs[i] = MIN_DEQUANT_VAL * 2;
    pbi->dequant_Inter_coeffs[i] =
      pbi->dequant_Inter_coeffs[i] << IDCT_SCALE_FACTOR;

    pbi->dequant_InterUV_coeffs[i] =
      (( scale_factor * pbi->dequant_InterUV_coeffs[i] ) / 100);
    if ( pbi->dequant_InterUV_coeffs[i] < (MIN_DEQUANT_VAL * 2) )
      pbi->dequant_InterUV_coeffs[i] = MIN_DEQUANT_VAL * 2;
    pbi->dequant_InterUV_coeffs[i] =
      pbi->dequant_InterUV_coeffs[i] << IDCT_SCALE_FACTOR;
  }

  pbi->dequant_coeffs = pbi->dequant_Y_coeffs;
}

void UpdateQ( PB_INSTANCE *pbi, ogg_uint32_t NewQ ){
  ogg_uint32_t qscale;

  /* Do bounds checking and convert to a float. */
  qscale = NewQ;
  if ( qscale < pbi->QThreshTable[Q_TABLE_SIZE-1] )
    qscale = pbi->QThreshTable[Q_TABLE_SIZE-1];
  else if ( qscale > pbi->QThreshTable[0] )
    qscale = pbi->QThreshTable[0];

  /* Set the inter/intra descision control variables. */
  pbi->FrameQIndex = Q_TABLE_SIZE - 1;
  while ( (ogg_int32_t) pbi->FrameQIndex >= 0 ) {
    if ( (pbi->FrameQIndex == 0) ||
         ( pbi->QThreshTable[pbi->FrameQIndex] >= NewQ) )
      break;
    pbi->FrameQIndex --;
  }

  /* Re-initialise the q tables for forward and reverse transforms. */
  init_dequantizer ( pbi, qscale, (unsigned char) pbi->FrameQIndex );
}

void UpdateQC( CP_INSTANCE *cpi, ogg_uint32_t NewQ ){
  ogg_uint32_t qscale;
  PB_INSTANCE *pbi = &cpi->pb;

  /* Do bounds checking and convert to a float.  */
  qscale = NewQ;
  if ( qscale < pbi->QThreshTable[Q_TABLE_SIZE-1] )
    qscale = pbi->QThreshTable[Q_TABLE_SIZE-1];
  else if ( qscale > pbi->QThreshTable[0] )
    qscale = pbi->QThreshTable[0];

  /* Set the inter/intra descision control variables. */
  pbi->FrameQIndex = Q_TABLE_SIZE - 1;
  while ((ogg_int32_t) pbi->FrameQIndex >= 0 ) {
    if ( (pbi->FrameQIndex == 0) ||
         ( pbi->QThreshTable[pbi->FrameQIndex] >= NewQ) )
      break;
    pbi->FrameQIndex --;
  }

  /* Re-initialise the q tables for forward and reverse transforms. */
  init_quantizer ( cpi, qscale, (unsigned char) pbi->FrameQIndex );
  init_dequantizer ( pbi, qscale, (unsigned char) pbi->FrameQIndex );
}
