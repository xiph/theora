/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2005                *
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

/* the *V1 tables are the originals used by the VP3 codec */

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

/* New (6) quant matrices */

static const Q_LIST_ENTRY Y_coeffs[64] ={
  16,  11,  10,  16,  24,  40,  51,  61,
  12,  12,  14,  19,  26,  58,  60,  55,
  14,  13,  16,  24,  40,  57,  69,  56,
  14,  17,  22,  29,  51,  87,  80,  62,
  18,  22,  37,  58,  68, 109, 103,  77,
  24,  35,  55,  64,  81, 104, 113,  92,
  49,  64,  78,  87, 103, 121, 120, 101,
  72,  92,  95,  98, 112, 100, 103,  99
};


static const Q_LIST_ENTRY U_coeffs[64] ={
  17,   18,     24,     47,     99,     99,     99,     99,
  18,   21,     26,     66,     99,     99,     99,     99,
  24,   26,     56,     99,     99,     99,     99,     99,
  47,   66,     99,     99,     99,     99,     99,     99,
  99,   99,     99,     99,     99,     99,     99,     99,
  99,   99,     99,     99,     99,     99,     99,     99,
  99,   99,     99,     99,     99,     99,     99,     99,
  99,   99,     99,     99,     99,     99,     99,     99
};

static const Q_LIST_ENTRY V_coeffs[64] ={
  17,   18,     24,     47,     99,     99,     99,     99,
  18,   21,     26,     66,     99,     99,     99,     99,
  24,   26,     56,     99,     99,     99,     99,     99,
  47,   66,     99,     99,     99,     99,     99,     99,
  99,   99,     99,     99,     99,     99,     99,     99,
  99,   99,     99,     99,     99,     99,     99,     99,
  99,   99,     99,     99,     99,     99,     99,     99,
  99,   99,     99,     99,     99,     99,     99,     99
};


static const Q_LIST_ENTRY Inter_Y_coeffs[64] ={
  16,  16,  16,  20,  24,  28,  32,  40,
  16,  16,  20,  24,  28,  32,  40,  48,
  16,  20,  24,  28,  32,  40,  48,  64,
  20,  24,  28,  32,  40,  48,  64,  64,
  24,  28,  32,  40,  48,  64,  64,  64,
  28,  32,  40,  48,  64,  64,  64,  96,
  32,  40,  48,  64,  64,  64,  96,  128,
  40,  48,  64,  64,  64,  96,  128, 128
};

static const Q_LIST_ENTRY Inter_U_coeffs[64] ={
  17,   18,     24,     47,     99,     99,     99,     99,
  18,   21,     26,     66,     99,     99,     99,     99,
  24,   26,     56,     99,     99,     99,     99,     99,
  47,   66,     99,     99,     99,     99,     99,     99,
  99,   99,     99,     99,     99,     99,     99,     99,
  99,   99,     99,     99,     99,     99,     99,     99,
  99,   99,     99,     99,     99,     99,     99,     99,
  99,   99,     99,     99,     99,     99,     99,     99
};

static const Q_LIST_ENTRY Inter_V_coeffs[64] ={
  17,   18,     24,     47,     99,     99,     99,     99,
  18,   21,     26,     66,     99,     99,     99,     99,
  24,   26,     56,     99,     99,     99,     99,     99,
  47,   66,     99,     99,     99,     99,     99,     99,
  99,   99,     99,     99,     99,     99,     99,     99,
  99,   99,     99,     99,     99,     99,     99,     99,
  99,   99,     99,     99,     99,     99,     99,     99,
  99,   99,     99,     99,     99,     99,     99,     99
};


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
  
  switch(pbi->encoder_profile) {
  	case PROFILE_FULL:
      oggpackB_write(opb, 6 - 1, 9); /* number of base matricies */

      for(x=0; x<64; x++) {
        oggpackB_write(opb, pbi->Y_coeffs[x],8);
      }
      for(x=0; x<64; x++) {
        oggpackB_write(opb, pbi->U_coeffs[x],8);
      }
      for(x=0; x<64; x++) {
        oggpackB_write(opb, pbi->V_coeffs[x],8);
      }
  
      for(x=0; x<64; x++) {
        oggpackB_write(opb, pbi->InterY_coeffs[x],8);
      }
      for(x=0; x<64; x++) {
        oggpackB_write(opb, pbi->InterU_coeffs[x],8);
      }
      for(x=0; x<64; x++) {
        oggpackB_write(opb, pbi->InterV_coeffs[x],8);
      }
      /* table mapping */
      oggpackB_write(opb, 0, 3);  /* matrix 0 for intra Y */
      oggpackB_write(opb, 62, 6); /* used for every q */
      oggpackB_write(opb, 0, 3);
  
      oggpackB_write(opb, 1, 1);
      oggpackB_write(opb, 1, 3);  /* matrix 1 for intra U */
      oggpackB_write(opb, 62, 6); /* used for every q */
      oggpackB_write(opb, 0, 3);
  
      oggpackB_write(opb, 1, 1);
      oggpackB_write(opb, 2, 3);  /* matrix 2 for intra V */
      oggpackB_write(opb, 62, 6); /* used for every q */
      oggpackB_write(opb, 0, 3);
  
      oggpackB_write(opb, 1, 1);
      oggpackB_write(opb, 3, 3);  /* matrix 3 for inter Y */
      oggpackB_write(opb, 62, 6); /* used for every q */
      oggpackB_write(opb, 0, 3);
  
      oggpackB_write(opb, 1, 1);
      oggpackB_write(opb, 4, 3);  /* matrix 4 for inter U */
      oggpackB_write(opb, 62, 6); /* used for every q */
      oggpackB_write(opb, 0, 3);
  
      oggpackB_write(opb, 1, 1);
      oggpackB_write(opb, 5, 3);  /* matrix 5 for inter V */
      oggpackB_write(opb, 62, 6); /* used for every q */
      oggpackB_write(opb, 0, 3);
      break;

    default: /* VP3 */
      oggpackB_write(opb, 3 - 1, 9); /* number of base matricies */
      for(x=0; x<64; x++) {
        oggpackB_write(opb, pbi->Y_coeffs[x],8);
      }
      for(x=0; x<64; x++) {
        oggpackB_write(opb, pbi->U_coeffs[x],8);
      }
      for(x=0; x<64; x++) {
        oggpackB_write(opb, pbi->InterY_coeffs[x],8);
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
      break;
  }
}


static int _read_qtable_range(codec_setup_info *ci, oggpack_buffer* opb,
         int N, int type) 
{
  int index, range;
  int qi = 0;
  int count = 0;
  qmat_range_table table[65];

  theora_read(opb,_ilog(N-1),&index); /* qi=0 index */
  table[count].startqi = 0;
  table[count++].qmat = ci->qmats + index * Q_TABLE_SIZE;
  while(qi<63) {
    theora_read(opb,_ilog(62-qi),&range); /* range to next code q matrix */
    range++;
    if(range<=0) return OC_BADHEADER;
    qi+=range;
    theora_read(opb,_ilog(N-1),&index); /* next index */
    table[count].startqi = qi;
    table[count++].qmat = ci->qmats + index * Q_TABLE_SIZE;
  }

  ci->range_table[type] = _ogg_malloc(count * sizeof(qmat_range_table));
  if (ci->range_table[type] != NULL) {
    memcpy(ci->range_table[type], table, count * sizeof(qmat_range_table)); 
    return 0;
  }
  
  return OC_FAULT; /* allocation failed */
}

int ReadQTables(codec_setup_info *ci, oggpack_buffer* opb) {
  long bits,value;
  int x,y, N;

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
  ci->qmats=_ogg_malloc(N*64*sizeof(Q_LIST_ENTRY));
  ci->MaxQMatrixIndex = N;
  for(y=0; y<N; y++) {
    for(x=0; x<64; x++) {
      theora_read(opb,8,&value);
      if(bits<0)return OC_BADHEADER;
      ci->qmats[(y<<6)+x]=(Q_LIST_ENTRY)value;
    }
  }
  /* table mapping */
  for(x=0; x<6; x++) {
    ci->range_table[x] = NULL;
  }
  {
    int flag, ret;
    /* intra Y */
    if((ret=_read_qtable_range(ci,opb,N,0))<0) return ret;
    /* intra U */
    theora_read(opb,1,&flag);
    if(flag<0) return OC_BADHEADER;
    if(flag) {
      /* explicitly coded */
      if((ret=_read_qtable_range(ci,opb,N,1))<0) return ret;
    } else {
      /* same as previous */
    }
    /* intra V */
    theora_read(opb,1,&flag);
    if(flag<0) return OC_BADHEADER;
    if(flag) {
      /* explicitly coded */
      if((ret=_read_qtable_range(ci,opb,N,2))<0) return ret;
    } else {
       /* same as previous */
    }
    /* inter Y */
    theora_read(opb,1,&flag);
    if(flag<0) return OC_BADHEADER;
    if(flag) {
      /* explicitly coded */
      if((ret=_read_qtable_range(ci,opb,N,3))<0) return ret;
    } else {
      theora_read(opb,1,&flag);
      if(flag<0) return OC_BADHEADER;
      if(flag) {
        /* same as corresponding intra */
      } else {
        /* same as previous */
      }
    }
    /* inter U */
    theora_read(opb,1,&flag);
    if(flag<0) return OC_BADHEADER;
    if(flag) {
      /* explicitly coded */
      if((ret=_read_qtable_range(ci,opb,N,4))<0) return ret;
    } else {
      theora_read(opb,1,&flag);
      if(flag<0) return OC_BADHEADER;
      if(flag) {
        /* same as corresponding intra */
      } else {
        /* same as previous */
      }
    }
    /* inter V */
    theora_read(opb,1,&flag);
    if(flag<0) return OC_BADHEADER;
    if(flag) {
      /* explicitly coded */
      if((ret=_read_qtable_range(ci,opb,N,5))<0) return ret;
    } else {
      theora_read(opb,1,&flag);
      if(flag<0) return OC_BADHEADER;
      if(flag) {
        /* same as corresponding intra */
      } else {
        /* same as previous */
      }
    }
  }
  
  return 0;
}

void CopyQTables(PB_INSTANCE *pbi, codec_setup_info *ci) {
  Q_LIST_ENTRY *qmat;

  memcpy(pbi->QThreshTable, ci->QThreshTable, sizeof(pbi->QThreshTable));
  memcpy(pbi->DcScaleFactorTable, ci->DcScaleFactorTable,
         sizeof(pbi->DcScaleFactorTable));

  /* the decoder only supports 6 different base matricies; do the
     best we can with the range table. We assume the first range
     entry is good for all qi values. A NULL range table entry 
     indicates we fall back to the previous value. */
  qmat = ci->range_table[0]->qmat;
  memcpy(pbi->Y_coeffs, qmat, sizeof(pbi->Y_coeffs));
  if (ci->range_table[1]) qmat = ci->range_table[1]->qmat;
  memcpy(pbi->U_coeffs, qmat, sizeof(pbi->U_coeffs));
  if (ci->range_table[2]) qmat = ci->range_table[2]->qmat;
  memcpy(pbi->V_coeffs, qmat, sizeof(pbi->V_coeffs));
  if (ci->range_table[3]) qmat = ci->range_table[3]->qmat;
  memcpy(pbi->InterY_coeffs, qmat, sizeof(pbi->InterY_coeffs));
  if (ci->range_table[4]) qmat = ci->range_table[4]->qmat;
  memcpy(pbi->InterU_coeffs, qmat, sizeof(pbi->InterU_coeffs));
  if (ci->range_table[5]) qmat = ci->range_table[5]->qmat;
  memcpy(pbi->InterV_coeffs, qmat, sizeof(pbi->InterV_coeffs));
}

/* Initialize custom qtables using the VP31 values.
   Someday we can change the quant tables to be adaptive, or just plain
    better. */
void InitQTables( PB_INSTANCE *pbi ){
  switch(pbi->encoder_profile) {
  	case PROFILE_FULL:
      memcpy(pbi->QThreshTable, QThreshTableV1, sizeof(pbi->QThreshTable));
      memcpy(pbi->DcScaleFactorTable, DcScaleFactorTableV1, sizeof(pbi->DcScaleFactorTable));
      memcpy(pbi->Y_coeffs, Y_coeffs, sizeof(pbi->Y_coeffs));
      memcpy(pbi->U_coeffs, U_coeffs, sizeof(pbi->U_coeffs));
      memcpy(pbi->V_coeffs, V_coeffs, sizeof(pbi->V_coeffs));
      memcpy(pbi->InterY_coeffs, Inter_Y_coeffs, sizeof(pbi->InterY_coeffs));
      memcpy(pbi->InterU_coeffs, Inter_U_coeffs, sizeof(pbi->InterU_coeffs));
      memcpy(pbi->InterV_coeffs, Inter_V_coeffs, sizeof(pbi->InterV_coeffs));
      break;
    default: /* VP3 */
      memcpy(pbi->QThreshTable, QThreshTableV1, sizeof(pbi->QThreshTable));
      memcpy(pbi->DcScaleFactorTable, DcScaleFactorTableV1, sizeof(pbi->DcScaleFactorTable));
      memcpy(pbi->Y_coeffs, Y_coeffsV1, sizeof(pbi->Y_coeffs));
      memcpy(pbi->U_coeffs, UV_coeffsV1, sizeof(pbi->U_coeffs));
      memcpy(pbi->V_coeffs, UV_coeffsV1, sizeof(pbi->V_coeffs));
      memcpy(pbi->InterY_coeffs, Inter_coeffsV1, sizeof(pbi->InterY_coeffs));
      memcpy(pbi->InterU_coeffs, Inter_coeffsV1, sizeof(pbi->InterU_coeffs));
      memcpy(pbi->InterV_coeffs, Inter_coeffsV1, sizeof(pbi->InterV_coeffs));
      break;
  }
}

static void BuildZigZagIndex(PB_INSTANCE *pbi){
  ogg_int32_t i,j;

  /* invert the row to zigzag coeffient order lookup table */
  for ( i = 0; i < BLOCK_SIZE; i++ ){
    j = dezigzag_index[i];
    pbi->zigzag_index[j] = i;
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

    const Q_LIST_ENTRY * temp_Y_coeffs;
    const Q_LIST_ENTRY * temp_U_coeffs;
    const Q_LIST_ENTRY * temp_V_coeffs;
    const Q_LIST_ENTRY * temp_Inter_Y_coeffs;
    const Q_LIST_ENTRY * temp_Inter_U_coeffs;
    const Q_LIST_ENTRY * temp_Inter_V_coeffs;
    const Q_LIST_ENTRY * temp_DcScaleFactorTable;

    /* Notes on setup of quantisers.  The initial multiplication by
     the scale factor is done in the ogg_int32_t domain to insure that the
     precision in the quantiser is the same as in the inverse
     quantiser where all calculations are integer.  The "<< 2" is a
     normalisation factor for the forward DCT transform. */

    /* New version rounding and ZB characteristics. */
    temp_Y_coeffs = cpi->pb.Y_coeffs;
    temp_U_coeffs = cpi->pb.U_coeffs;
    temp_V_coeffs = cpi->pb.V_coeffs;
    temp_Inter_Y_coeffs = cpi->pb.InterY_coeffs;
    temp_Inter_U_coeffs = cpi->pb.InterU_coeffs;
    temp_Inter_V_coeffs = cpi->pb.InterV_coeffs;
    temp_DcScaleFactorTable = cpi->pb.DcScaleFactorTable;
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
      (((ogg_uint32_t)(temp_DcScaleFactorTable[QIndex] * temp_Y_coeffs[0])/100) << 2);
    if ( temp_fp_quant_coeffs < MIN_LEGAL_QUANT_ENTRY * 2 )
      temp_fp_quant_coeffs = MIN_LEGAL_QUANT_ENTRY * 2;

    temp_fp_quant_round = temp_fp_quant_coeffs * RoundingFactor;
    pbi->fp_quant_Y_round[0]    = (ogg_int32_t) (0.5 + temp_fp_quant_round);

    temp_fp_ZeroBinSize = temp_fp_quant_coeffs * ZBinFactor;
    pbi->fp_ZeroBinSize_Y[0]    = (ogg_int32_t) (0.5 + temp_fp_ZeroBinSize);

    temp_fp_quant_coeffs = 1.0 / temp_fp_quant_coeffs;
    pbi->fp_quant_Y_coeffs[0]   = (0.5 + SHIFT16 * temp_fp_quant_coeffs);

    /* Intra U */
    temp_fp_quant_coeffs =
      (((ogg_uint32_t)(temp_DcScaleFactorTable[QIndex] * temp_U_coeffs[0])/100) << 2);
    if ( temp_fp_quant_coeffs < MIN_LEGAL_QUANT_ENTRY * 2)
      temp_fp_quant_coeffs = MIN_LEGAL_QUANT_ENTRY * 2;

    temp_fp_quant_round = temp_fp_quant_coeffs * RoundingFactor;
    pbi->fp_quant_U_round[0]   = (0.5 + temp_fp_quant_round);

    temp_fp_ZeroBinSize = temp_fp_quant_coeffs * ZBinFactor;
    pbi->fp_ZeroBinSize_U[0]   = (0.5 + temp_fp_ZeroBinSize);

    temp_fp_quant_coeffs = 1.0 / temp_fp_quant_coeffs;
    pbi->fp_quant_U_coeffs[0]= (0.5 + SHIFT16 * temp_fp_quant_coeffs);

    /* Intra V */
    temp_fp_quant_coeffs =
      (((ogg_uint32_t)(temp_DcScaleFactorTable[QIndex] * temp_V_coeffs[0])/100) << 2);
    if ( temp_fp_quant_coeffs < MIN_LEGAL_QUANT_ENTRY * 2)
      temp_fp_quant_coeffs = MIN_LEGAL_QUANT_ENTRY * 2;

    temp_fp_quant_round = temp_fp_quant_coeffs * RoundingFactor;
    pbi->fp_quant_V_round[0]   = (0.5 + temp_fp_quant_round);

    temp_fp_ZeroBinSize = temp_fp_quant_coeffs * ZBinFactor;
    pbi->fp_ZeroBinSize_V[0]   = (0.5 + temp_fp_ZeroBinSize);

    temp_fp_quant_coeffs = 1.0 / temp_fp_quant_coeffs;
    pbi->fp_quant_V_coeffs[0]= (0.5 + SHIFT16 * temp_fp_quant_coeffs);


    /* Inter Y */
    temp_fp_quant_coeffs =
      (((ogg_uint32_t)(temp_DcScaleFactorTable[QIndex] * temp_Inter_Y_coeffs[0])/100) << 2);
    if ( temp_fp_quant_coeffs < MIN_LEGAL_QUANT_ENTRY * 4)
      temp_fp_quant_coeffs = MIN_LEGAL_QUANT_ENTRY * 4;

    temp_fp_quant_round = temp_fp_quant_coeffs * RoundingFactor;
    pbi->fp_quant_Inter_Y_round[0]= (0.5 + temp_fp_quant_round);

    temp_fp_ZeroBinSize = temp_fp_quant_coeffs * ZBinFactor;
    pbi->fp_ZeroBinSize_Inter_Y[0]= (0.5 + temp_fp_ZeroBinSize);

    temp_fp_quant_coeffs= 1.0 / temp_fp_quant_coeffs;
    pbi->fp_quant_Inter_Y_coeffs[0]= (0.5 + SHIFT16 * temp_fp_quant_coeffs);

    /* Inter U */
    temp_fp_quant_coeffs =
      (((ogg_uint32_t)(temp_DcScaleFactorTable[QIndex] * temp_Inter_U_coeffs[0])/100) << 2);
    if ( temp_fp_quant_coeffs < MIN_LEGAL_QUANT_ENTRY * 4)
      temp_fp_quant_coeffs = MIN_LEGAL_QUANT_ENTRY * 4;

    temp_fp_quant_round = temp_fp_quant_coeffs * RoundingFactor;
    pbi->fp_quant_Inter_U_round[0]= (0.5 + temp_fp_quant_round);

    temp_fp_ZeroBinSize = temp_fp_quant_coeffs * ZBinFactor;
    pbi->fp_ZeroBinSize_Inter_U[0]= (0.5 + temp_fp_ZeroBinSize);

    temp_fp_quant_coeffs= 1.0 / temp_fp_quant_coeffs;
    pbi->fp_quant_Inter_U_coeffs[0]= (0.5 + SHIFT16 * temp_fp_quant_coeffs);
    
    /* Inter V */
    temp_fp_quant_coeffs =
      (((ogg_uint32_t)(temp_DcScaleFactorTable[QIndex] * temp_Inter_V_coeffs[0])/100) << 2);
    if ( temp_fp_quant_coeffs < MIN_LEGAL_QUANT_ENTRY * 4)
      temp_fp_quant_coeffs = MIN_LEGAL_QUANT_ENTRY * 4;

    temp_fp_quant_round = temp_fp_quant_coeffs * RoundingFactor;
    pbi->fp_quant_Inter_V_round[0]= (0.5 + temp_fp_quant_round);

    temp_fp_ZeroBinSize = temp_fp_quant_coeffs * ZBinFactor;
    pbi->fp_ZeroBinSize_Inter_V[0]= (0.5 + temp_fp_ZeroBinSize);

    temp_fp_quant_coeffs= 1.0 / temp_fp_quant_coeffs;
    pbi->fp_quant_Inter_V_coeffs[0]= (0.5 + SHIFT16 * temp_fp_quant_coeffs);
    

    for ( i = 1; i < 64; i++ ){
      /* now scale coefficients by required compression factor */
      /* Intra Y */
      temp_fp_quant_coeffs =
        (((ogg_uint32_t)(scale_factor * temp_Y_coeffs[i]) / 100 ) << 2 );
      if ( temp_fp_quant_coeffs < (MIN_LEGAL_QUANT_ENTRY) )
        temp_fp_quant_coeffs = (MIN_LEGAL_QUANT_ENTRY);

      temp_fp_quant_round = temp_fp_quant_coeffs * RoundingFactor;
      pbi->fp_quant_Y_round[i]  = (0.5 + temp_fp_quant_round);

      temp_fp_ZeroBinSize = temp_fp_quant_coeffs * ZBinFactor;
      pbi->fp_ZeroBinSize_Y[i]  = (0.5 + temp_fp_ZeroBinSize);

      temp_fp_quant_coeffs = 1.0 / temp_fp_quant_coeffs;
      pbi->fp_quant_Y_coeffs[i] = (0.5 + SHIFT16 * temp_fp_quant_coeffs);

      /* Intra U */
      temp_fp_quant_coeffs =
        (((ogg_uint32_t)(scale_factor * temp_U_coeffs[i]) / 100 ) << 2 );
      if ( temp_fp_quant_coeffs < (MIN_LEGAL_QUANT_ENTRY))
        temp_fp_quant_coeffs = (MIN_LEGAL_QUANT_ENTRY);

      temp_fp_quant_round = temp_fp_quant_coeffs * RoundingFactor;
      pbi->fp_quant_U_round[i] = (0.5 + temp_fp_quant_round);

      temp_fp_ZeroBinSize = temp_fp_quant_coeffs * ZBinFactor;
      pbi->fp_ZeroBinSize_U[i] = (0.5 + temp_fp_ZeroBinSize);

      temp_fp_quant_coeffs = 1.0 / temp_fp_quant_coeffs;
      pbi->fp_quant_U_coeffs[i]= (0.5 + SHIFT16 * temp_fp_quant_coeffs);

      /* Intra V */
      temp_fp_quant_coeffs =
        (((ogg_uint32_t)(scale_factor * temp_V_coeffs[i]) / 100 ) << 2 );
      if ( temp_fp_quant_coeffs < (MIN_LEGAL_QUANT_ENTRY))
        temp_fp_quant_coeffs = (MIN_LEGAL_QUANT_ENTRY);

      temp_fp_quant_round = temp_fp_quant_coeffs * RoundingFactor;
      pbi->fp_quant_V_round[i] = (0.5 + temp_fp_quant_round);

      temp_fp_ZeroBinSize = temp_fp_quant_coeffs * ZBinFactor;
      pbi->fp_ZeroBinSize_V[i] = (0.5 + temp_fp_ZeroBinSize);

      temp_fp_quant_coeffs = 1.0 / temp_fp_quant_coeffs;
      pbi->fp_quant_V_coeffs[i]= (0.5 + SHIFT16 * temp_fp_quant_coeffs);

      /* Inter Y */
      temp_fp_quant_coeffs =
        (((ogg_uint32_t)(scale_factor * temp_Inter_Y_coeffs[i]) / 100 ) << 2 );
      if ( temp_fp_quant_coeffs < (MIN_LEGAL_QUANT_ENTRY * 2) )
        temp_fp_quant_coeffs = (MIN_LEGAL_QUANT_ENTRY * 2);

      temp_fp_quant_round = temp_fp_quant_coeffs * RoundingFactor;
      pbi->fp_quant_Inter_Y_round[i]= (0.5 + temp_fp_quant_round);

      temp_fp_ZeroBinSize = temp_fp_quant_coeffs * ZBinFactor;
      pbi->fp_ZeroBinSize_Inter_Y[i]= (0.5 + temp_fp_ZeroBinSize);

      temp_fp_quant_coeffs = 1.0 / temp_fp_quant_coeffs;
      pbi->fp_quant_Inter_Y_coeffs[i]= (0.5 + SHIFT16 * temp_fp_quant_coeffs);

      /* Inter U */
      temp_fp_quant_coeffs =
        (((ogg_uint32_t)(scale_factor * temp_Inter_U_coeffs[i]) / 100 ) << 2 );
      if ( temp_fp_quant_coeffs < (MIN_LEGAL_QUANT_ENTRY * 2) )
        temp_fp_quant_coeffs = (MIN_LEGAL_QUANT_ENTRY * 2);

      temp_fp_quant_round = temp_fp_quant_coeffs * RoundingFactor;
      pbi->fp_quant_Inter_U_round[i]= (0.5 + temp_fp_quant_round);

      temp_fp_ZeroBinSize = temp_fp_quant_coeffs * ZBinFactor;
      pbi->fp_ZeroBinSize_Inter_U[i]= (0.5 + temp_fp_ZeroBinSize);

      temp_fp_quant_coeffs = 1.0 / temp_fp_quant_coeffs;
      pbi->fp_quant_Inter_U_coeffs[i]= (0.5 + SHIFT16 * temp_fp_quant_coeffs);

      /* Inter V */
      temp_fp_quant_coeffs =
        (((ogg_uint32_t)(scale_factor * temp_Inter_V_coeffs[i]) / 100 ) << 2 );
      if ( temp_fp_quant_coeffs < (MIN_LEGAL_QUANT_ENTRY * 2) )
        temp_fp_quant_coeffs = (MIN_LEGAL_QUANT_ENTRY * 2);

      temp_fp_quant_round = temp_fp_quant_coeffs * RoundingFactor;
      pbi->fp_quant_Inter_V_round[i]= (0.5 + temp_fp_quant_round);

      temp_fp_ZeroBinSize = temp_fp_quant_coeffs * ZBinFactor;
      pbi->fp_ZeroBinSize_Inter_V[i]= (0.5 + temp_fp_ZeroBinSize);

      temp_fp_quant_coeffs = 1.0 / temp_fp_quant_coeffs;
      pbi->fp_quant_Inter_V_coeffs[i]= (0.5 + SHIFT16 * temp_fp_quant_coeffs);


    }

    pbi->fquant_coeffs = pbi->fp_quant_Y_coeffs;

}

void select_quantiser(PB_INSTANCE *pbi, int type) {
  /* select a quantiser according to what plane has to be coded in what
   * mode. Could be extended to a more sophisticated scheme. */
  
  switch(type) {
    case BLOCK_Y:
      pbi->fquant_coeffs = pbi->fp_quant_Y_coeffs;
      pbi->fquant_round = pbi->fp_quant_Y_round;
      pbi->fquant_ZbSize = pbi->fp_ZeroBinSize_Y;
      break;
    case BLOCK_U:
      pbi->fquant_coeffs = pbi->fp_quant_U_coeffs;
      pbi->fquant_round = pbi->fp_quant_U_round;
      pbi->fquant_ZbSize = pbi->fp_ZeroBinSize_U;
      break;
    case BLOCK_V:
      pbi->fquant_coeffs = pbi->fp_quant_V_coeffs;
      pbi->fquant_round = pbi->fp_quant_V_round;
      pbi->fquant_ZbSize = pbi->fp_ZeroBinSize_V;
      break;
    case BLOCK_INTER_Y:
      pbi->fquant_coeffs = pbi->fp_quant_Inter_Y_coeffs;
      pbi->fquant_round = pbi->fp_quant_Inter_Y_round;
      pbi->fquant_ZbSize = pbi->fp_ZeroBinSize_Inter_Y;
      break;
    case BLOCK_INTER_U:
      pbi->fquant_coeffs = pbi->fp_quant_Inter_U_coeffs;
      pbi->fquant_round = pbi->fp_quant_Inter_U_round;
      pbi->fquant_ZbSize = pbi->fp_ZeroBinSize_Inter_U;		
      break;
    case BLOCK_INTER_V:
      pbi->fquant_coeffs = pbi->fp_quant_Inter_V_coeffs;
      pbi->fquant_round = pbi->fp_quant_Inter_V_round;
      pbi->fquant_ZbSize = pbi->fp_ZeroBinSize_Inter_V;		
      break;
  }
}


void quantize( PB_INSTANCE *pbi,
               ogg_int16_t * DCT_block,
               Q_LIST_ENTRY * quantized_list){
  ogg_uint32_t  i;              /* Row index */
  Q_LIST_ENTRY  val;            /* Quantised value. */

  ogg_int32_t * FquantRoundPtr = pbi->fquant_round;
  ogg_int32_t * FquantCoeffsPtr = pbi->fquant_coeffs;
  ogg_int32_t * FquantZBinSizePtr = pbi->fquant_ZbSize;
  ogg_int16_t * DCT_blockPtr = DCT_block;
  ogg_uint32_t * ZigZagPtr = (ogg_uint32_t *)pbi->zigzag_index;
  ogg_int32_t temp;

  /* Set the quantized_list to default to 0 */
  memset( quantized_list, 0, 64 * sizeof(Q_LIST_ENTRY) );

  /* Note that we add half divisor to effect rounding on positive number */
  for( i = 0; i < VFRAGPIXELS; i++) {
    /* Column 0  */
    if ( DCT_blockPtr[0] >= FquantZBinSizePtr[0] ) {
      temp = FquantCoeffsPtr[0] * ( DCT_blockPtr[0] + FquantRoundPtr[0] ) ;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[ZigZagPtr[0]] = ( val > 511 ) ? 511 : val;
    } else if ( DCT_blockPtr[0] <= -FquantZBinSizePtr[0] ) {
      temp = FquantCoeffsPtr[0] *
        ( DCT_blockPtr[0] - FquantRoundPtr[0] ) + MIN16;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[ZigZagPtr[0]] = ( val < -511 ) ? -511 : val;
    }

    /* Column 1 */
    if ( DCT_blockPtr[1] >= FquantZBinSizePtr[1] ) {
      temp = FquantCoeffsPtr[1] *
        ( DCT_blockPtr[1] + FquantRoundPtr[1] ) ;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[ZigZagPtr[1]] = ( val > 511 ) ? 511 : val;
    } else if ( DCT_blockPtr[1] <= -FquantZBinSizePtr[1] ) {
      temp = FquantCoeffsPtr[1] *
        ( DCT_blockPtr[1] - FquantRoundPtr[1] ) + MIN16;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[ZigZagPtr[1]] = ( val < -511 ) ? -511 : val;
    }

    /* Column 2 */
    if ( DCT_blockPtr[2] >= FquantZBinSizePtr[2] ) {
      temp = FquantCoeffsPtr[2] *
        ( DCT_blockPtr[2] + FquantRoundPtr[2] ) ;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[ZigZagPtr[2]] = ( val > 511 ) ? 511 : val;
    } else if ( DCT_blockPtr[2] <= -FquantZBinSizePtr[2] ) {
      temp = FquantCoeffsPtr[2] *
        ( DCT_blockPtr[2] - FquantRoundPtr[2] ) + MIN16;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[ZigZagPtr[2]] = ( val < -511 ) ? -511 : val;
    }

    /* Column 3 */
    if ( DCT_blockPtr[3] >= FquantZBinSizePtr[3] ) {
      temp = FquantCoeffsPtr[3] *
        ( DCT_blockPtr[3] + FquantRoundPtr[3] ) ;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[ZigZagPtr[3]] = ( val > 511 ) ? 511 : val;
    } else if ( DCT_blockPtr[3] <= -FquantZBinSizePtr[3] ) {
      temp = FquantCoeffsPtr[3] *
        ( DCT_blockPtr[3] - FquantRoundPtr[3] ) + MIN16;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[ZigZagPtr[3]] = ( val < -511 ) ? -511 : val;
    }

    /* Column 4 */
    if ( DCT_blockPtr[4] >= FquantZBinSizePtr[4] ) {
      temp = FquantCoeffsPtr[4] *
        ( DCT_blockPtr[4] + FquantRoundPtr[4] ) ;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[ZigZagPtr[4]] = ( val > 511 ) ? 511 : val;
    } else if ( DCT_blockPtr[4] <= -FquantZBinSizePtr[4] ) {
      temp = FquantCoeffsPtr[4] *
        ( DCT_blockPtr[4] - FquantRoundPtr[4] ) + MIN16;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[ZigZagPtr[4]] = ( val < -511 ) ? -511 : val;
    }

    /* Column 5 */
    if ( DCT_blockPtr[5] >= FquantZBinSizePtr[5] ) {
      temp = FquantCoeffsPtr[5] *
        ( DCT_blockPtr[5] + FquantRoundPtr[5] ) ;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[ZigZagPtr[5]] = ( val > 511 ) ? 511 : val;
    } else if ( DCT_blockPtr[5] <= -FquantZBinSizePtr[5] ) {
      temp = FquantCoeffsPtr[5] *
        ( DCT_blockPtr[5] - FquantRoundPtr[5] ) + MIN16;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[ZigZagPtr[5]] = ( val < -511 ) ? -511 : val;
    }

    /* Column 6 */
    if ( DCT_blockPtr[6] >= FquantZBinSizePtr[6] ) {
      temp = FquantCoeffsPtr[6] *
        ( DCT_blockPtr[6] + FquantRoundPtr[6] ) ;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[ZigZagPtr[6]] = ( val > 511 ) ? 511 : val;
    } else if ( DCT_blockPtr[6] <= -FquantZBinSizePtr[6] ) {
      temp = FquantCoeffsPtr[6] *
        ( DCT_blockPtr[6] - FquantRoundPtr[6] ) + MIN16;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[ZigZagPtr[6]] = ( val < -511 ) ? -511 : val;
    }

    /* Column 7 */
    if ( DCT_blockPtr[7] >= FquantZBinSizePtr[7] ) {
      temp = FquantCoeffsPtr[7] *
        ( DCT_blockPtr[7] + FquantRoundPtr[7] ) ;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[ZigZagPtr[7]] = ( val > 511 ) ? 511 : val;
    } else if ( DCT_blockPtr[7] <= -FquantZBinSizePtr[7] ) {
      temp = FquantCoeffsPtr[7] *
        ( DCT_blockPtr[7] - FquantRoundPtr[7] ) + MIN16;
      val = (Q_LIST_ENTRY) (temp>>16);
      quantized_list[ZigZagPtr[7]] = ( val < -511 ) ? -511 : val;
    }

    FquantRoundPtr += 8;
    FquantCoeffsPtr += 8;
    FquantZBinSizePtr += 8;
    DCT_blockPtr += 8;
    ZigZagPtr += 8;
  }
}

static void init_dequantizer ( PB_INSTANCE *pbi,
                        ogg_uint32_t scale_factor,
                        unsigned char  QIndex ){
  int i, j;

  Q_LIST_ENTRY * InterY_coeffs;
  Q_LIST_ENTRY * InterU_coeffs;
  Q_LIST_ENTRY * InterV_coeffs;
  Q_LIST_ENTRY * Y_coeffs;
  Q_LIST_ENTRY * U_coeffs;
  Q_LIST_ENTRY * V_coeffs;
  Q_LIST_ENTRY * DcScaleFactorTable;

  InterY_coeffs = pbi->InterY_coeffs;
  InterU_coeffs = pbi->InterU_coeffs;
  InterV_coeffs = pbi->InterV_coeffs;
  Y_coeffs = pbi->Y_coeffs;
  U_coeffs = pbi->U_coeffs;
  V_coeffs = pbi->V_coeffs;
  DcScaleFactorTable = pbi->DcScaleFactorTable;

  /* invert the dequant index into the quant index
     the dxer has a different order than the cxer. */
  BuildZigZagIndex(pbi);

  /* Reorder dequantisation coefficients into dct zigzag order. */
  for ( i = 0; i < BLOCK_SIZE; i++ ) {
    j = pbi->zigzag_index[i];
    pbi->dequant_Y_coeffs[j] = Y_coeffs[i];
  }
  for ( i = 0; i < BLOCK_SIZE; i++ ) {
    j = pbi->zigzag_index[i];
    pbi->dequant_U_coeffs[j] = U_coeffs[i];
  }
  for ( i = 0; i < BLOCK_SIZE; i++ ) {
    j = pbi->zigzag_index[i];
    pbi->dequant_V_coeffs[j] = V_coeffs[i];
  }
  for ( i = 0; i < BLOCK_SIZE; i++ ){
    j = pbi->zigzag_index[i];
    pbi->dequant_InterY_coeffs[j] = InterY_coeffs[i];
  }
  for ( i = 0; i < BLOCK_SIZE; i++ ){
    j = pbi->zigzag_index[i];
    pbi->dequant_InterU_coeffs[j] = InterU_coeffs[i];
  }
  for ( i = 0; i < BLOCK_SIZE; i++ ){
    j = pbi->zigzag_index[i];
    pbi->dequant_InterV_coeffs[j] = InterV_coeffs[i];
  }

  /* Intra Y DC coeff */
  pbi->dequant_Y_coeffs[0] =
    ((DcScaleFactorTable[QIndex] * pbi->dequant_Y_coeffs[0])/100);
  if ( pbi->dequant_Y_coeffs[0] < MIN_DEQUANT_VAL * 2 )
    pbi->dequant_Y_coeffs[0] = MIN_DEQUANT_VAL * 2;
  pbi->dequant_Y_coeffs[0] =
    pbi->dequant_Y_coeffs[0] << IDCT_SCALE_FACTOR;

  /* Intra UV */
  pbi->dequant_U_coeffs[0] =
    ((DcScaleFactorTable[QIndex] * pbi->dequant_U_coeffs[0])/100);
  if ( pbi->dequant_U_coeffs[0] < MIN_DEQUANT_VAL * 2 )
    pbi->dequant_U_coeffs[0] = MIN_DEQUANT_VAL * 2;
  pbi->dequant_U_coeffs[0] =
    pbi->dequant_U_coeffs[0] << IDCT_SCALE_FACTOR;
  pbi->dequant_V_coeffs[0] =
    ((DcScaleFactorTable[QIndex] * pbi->dequant_V_coeffs[0])/100);
  if ( pbi->dequant_V_coeffs[0] < MIN_DEQUANT_VAL * 2 )
    pbi->dequant_V_coeffs[0] = MIN_DEQUANT_VAL * 2;
  pbi->dequant_V_coeffs[0] =
    pbi->dequant_V_coeffs[0] << IDCT_SCALE_FACTOR;

  /* Inter Y DC coeff */
  pbi->dequant_InterY_coeffs[0] =
    ((DcScaleFactorTable[QIndex] * pbi->dequant_InterY_coeffs[0])/100);
  if ( pbi->dequant_InterY_coeffs[0] < MIN_DEQUANT_VAL * 4 )
    pbi->dequant_InterY_coeffs[0] = MIN_DEQUANT_VAL * 4;
  pbi->dequant_InterY_coeffs[0] =
    pbi->dequant_InterY_coeffs[0] << IDCT_SCALE_FACTOR;

  /* Inter UV */
  pbi->dequant_InterU_coeffs[0] =
    ((DcScaleFactorTable[QIndex] * pbi->dequant_InterU_coeffs[0])/100);
  if ( pbi->dequant_InterU_coeffs[0] < MIN_DEQUANT_VAL * 4 )
    pbi->dequant_InterU_coeffs[0] = MIN_DEQUANT_VAL * 4;
  pbi->dequant_InterU_coeffs[0] =
    pbi->dequant_InterU_coeffs[0] << IDCT_SCALE_FACTOR;
  pbi->dequant_InterV_coeffs[0] =
    ((DcScaleFactorTable[QIndex] * pbi->dequant_InterV_coeffs[0])/100);
  if ( pbi->dequant_InterV_coeffs[0] < MIN_DEQUANT_VAL * 4 )
    pbi->dequant_InterV_coeffs[0] = MIN_DEQUANT_VAL * 4;
  pbi->dequant_InterV_coeffs[0] =
    pbi->dequant_InterV_coeffs[0] << IDCT_SCALE_FACTOR;

  for ( i = 1; i < BLOCK_SIZE; i++ ){
    /* now scale coefficients by required compression factor */
    pbi->dequant_Y_coeffs[i] =
      (( scale_factor * pbi->dequant_Y_coeffs[i] ) / 100);
    if ( pbi->dequant_Y_coeffs[i] < MIN_DEQUANT_VAL )
      pbi->dequant_Y_coeffs[i] = MIN_DEQUANT_VAL;
    pbi->dequant_Y_coeffs[i] =
      pbi->dequant_Y_coeffs[i] << IDCT_SCALE_FACTOR;

    pbi->dequant_U_coeffs[i] =
      (( scale_factor * pbi->dequant_U_coeffs[i] ) / 100);
    if ( pbi->dequant_U_coeffs[i] < MIN_DEQUANT_VAL )
      pbi->dequant_U_coeffs[i] = MIN_DEQUANT_VAL;
    pbi->dequant_U_coeffs[i] =
      pbi->dequant_U_coeffs[i] << IDCT_SCALE_FACTOR;

    pbi->dequant_V_coeffs[i] =
      (( scale_factor * pbi->dequant_V_coeffs[i] ) / 100);
    if ( pbi->dequant_V_coeffs[i] < MIN_DEQUANT_VAL )
      pbi->dequant_V_coeffs[i] = MIN_DEQUANT_VAL;
    pbi->dequant_V_coeffs[i] =
      pbi->dequant_V_coeffs[i] << IDCT_SCALE_FACTOR;

    pbi->dequant_InterY_coeffs[i] =
      (( scale_factor * pbi->dequant_InterY_coeffs[i] ) / 100);
    if ( pbi->dequant_InterY_coeffs[i] < (MIN_DEQUANT_VAL * 2) )
      pbi->dequant_InterY_coeffs[i] = MIN_DEQUANT_VAL * 2;
    pbi->dequant_InterY_coeffs[i] =
      pbi->dequant_InterY_coeffs[i] << IDCT_SCALE_FACTOR;

    pbi->dequant_InterU_coeffs[i] =
      (( scale_factor * pbi->dequant_InterU_coeffs[i] ) / 100);
    if ( pbi->dequant_InterU_coeffs[i] < (MIN_DEQUANT_VAL * 2) )
      pbi->dequant_InterU_coeffs[i] = MIN_DEQUANT_VAL * 2;
    pbi->dequant_InterU_coeffs[i] =
      pbi->dequant_InterU_coeffs[i] << IDCT_SCALE_FACTOR;

    pbi->dequant_InterV_coeffs[i] =
      (( scale_factor * pbi->dequant_InterV_coeffs[i] ) / 100);
    if ( pbi->dequant_InterV_coeffs[i] < (MIN_DEQUANT_VAL * 2) )
      pbi->dequant_InterV_coeffs[i] = MIN_DEQUANT_VAL * 2;
    pbi->dequant_InterV_coeffs[i] =
      pbi->dequant_InterV_coeffs[i] << IDCT_SCALE_FACTOR;
  }

  pbi->dequant_coeffs = pbi->dequant_Y_coeffs;
}

void UpdateQ( PB_INSTANCE *pbi, int NewQIndex ){
  ogg_uint32_t qscale;

  /* clamp to legal bounds */
  if (NewQIndex >= Q_TABLE_SIZE) NewQIndex = Q_TABLE_SIZE - 1;
  else if (NewQIndex < 0) NewQIndex = 0;

  pbi->FrameQIndex = NewQIndex;
  qscale = pbi->QThreshTable[NewQIndex];
  pbi->ThisFrameQualityValue = qscale;

  /* Re-initialise the Q tables for forward and reverse transforms. */
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

  /* Re-initialise the Q tables for forward and reverse transforms. */
  init_quantizer ( cpi, qscale, (unsigned char) pbi->FrameQIndex );
  init_dequantizer ( pbi, qscale, (unsigned char) pbi->FrameQIndex );
}
