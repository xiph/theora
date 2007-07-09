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

/*
 * th_quant_info for VP3 
 */
 
/*The default quantization parameters used by VP3.1.*/
static const int OC_VP31_RANGE_SIZES[1]={63};
static const th_quant_base OC_VP31_BASES_INTRA_Y[2]={
  {
     16, 11, 10, 16, 24,  40, 51, 61,
     12, 12, 14, 19, 26,  58, 60, 55,
     14, 13, 16, 24, 40,  57, 69, 56,
     14, 17, 22, 29, 51,  87, 80, 62,
     18, 22, 37, 58, 68, 109,103, 77,
     24, 35, 55, 64, 81, 104,113, 92,
     49, 64, 78, 87,103, 121,120,101,
     72, 92, 95, 98,112, 100,103, 99
  },
  {
     16, 11, 10, 16, 24,  40, 51, 61,
     12, 12, 14, 19, 26,  58, 60, 55,
     14, 13, 16, 24, 40,  57, 69, 56,
     14, 17, 22, 29, 51,  87, 80, 62,
     18, 22, 37, 58, 68, 109,103, 77,
     24, 35, 55, 64, 81, 104,113, 92,
     49, 64, 78, 87,103, 121,120,101,
     72, 92, 95, 98,112, 100,103, 99
  }
};
static const th_quant_base OC_VP31_BASES_INTRA_C[2]={
  {
     17, 18, 24, 47, 99, 99, 99, 99,
     18, 21, 26, 66, 99, 99, 99, 99,
     24, 26, 56, 99, 99, 99, 99, 99,
     47, 66, 99, 99, 99, 99, 99, 99,
     99, 99, 99, 99, 99, 99, 99, 99,
     99, 99, 99, 99, 99, 99, 99, 99,
     99, 99, 99, 99, 99, 99, 99, 99,
     99, 99, 99, 99, 99, 99, 99, 99
  },
  {
     17, 18, 24, 47, 99, 99, 99, 99,
     18, 21, 26, 66, 99, 99, 99, 99,
     24, 26, 56, 99, 99, 99, 99, 99,
     47, 66, 99, 99, 99, 99, 99, 99,
     99, 99, 99, 99, 99, 99, 99, 99,
     99, 99, 99, 99, 99, 99, 99, 99,
     99, 99, 99, 99, 99, 99, 99, 99,
     99, 99, 99, 99, 99, 99, 99, 99
  }
};
static const th_quant_base OC_VP31_BASES_INTER[2]={
  {
     16, 16, 16, 20, 24, 28, 32, 40,
     16, 16, 20, 24, 28, 32, 40, 48,
     16, 20, 24, 28, 32, 40, 48, 64,
     20, 24, 28, 32, 40, 48, 64, 64,
     24, 28, 32, 40, 48, 64, 64, 64,
     28, 32, 40, 48, 64, 64, 64, 96,
     32, 40, 48, 64, 64, 64, 96,128,
     40, 48, 64, 64, 64, 96,128,128
  },
  {
     16, 16, 16, 20, 24, 28, 32, 40,
     16, 16, 20, 24, 28, 32, 40, 48,
     16, 20, 24, 28, 32, 40, 48, 64,
     20, 24, 28, 32, 40, 48, 64, 64,
     24, 28, 32, 40, 48, 64, 64, 64,
     28, 32, 40, 48, 64, 64, 64, 96,
     32, 40, 48, 64, 64, 64, 96,128,
     40, 48, 64, 64, 64, 96,128,128
  }
};

const th_quant_info TH_VP31_QUANT_INFO={
  {
    220,200,190,180,170,170,160,160,
    150,150,140,140,130,130,120,120,
    110,110,100,100, 90, 90, 90, 80,
     80, 80, 70, 70, 70, 60, 60, 60,
     60, 50, 50, 50, 50, 40, 40, 40,
     40, 40, 30, 30, 30, 30, 30, 30,
     30, 20, 20, 20, 20, 20, 20, 20,
     20, 10, 10, 10, 10, 10, 10, 10
  },
  {
    500,450,400,370,340,310,285,265,
    245,225,210,195,185,180,170,160,
    150,145,135,130,125,115,110,107,
    100, 96, 93, 89, 85, 82, 75, 74,
     70, 68, 64, 60, 57, 56, 52, 50,
     49, 45, 44, 43, 40, 38, 37, 35,
     33, 32, 30, 29, 28, 25, 24, 22,
     21, 19, 18, 17, 15, 13, 12, 10
  },
  {
    30,25,20,20,15,15,14,14,
    13,13,12,12,11,11,10,10,
     9, 9, 8, 8, 7, 7, 7, 7,
     6, 6, 6, 6, 5, 5, 5, 5,
     4, 4, 4, 4, 3, 3, 3, 3,
     2, 2, 2, 2, 2, 2, 2, 2,
     0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0
  },
  {
    {
      {1,OC_VP31_RANGE_SIZES,OC_VP31_BASES_INTRA_Y},
      {1,OC_VP31_RANGE_SIZES,OC_VP31_BASES_INTRA_C},
      {1,OC_VP31_RANGE_SIZES,OC_VP31_BASES_INTRA_C}
    },
    {
      {1,OC_VP31_RANGE_SIZES,OC_VP31_BASES_INTER},
      {1,OC_VP31_RANGE_SIZES,OC_VP31_BASES_INTER},
      {1,OC_VP31_RANGE_SIZES,OC_VP31_BASES_INTER}
    }
  }
};

void WriteQTables(PB_INSTANCE *pbi,oggpack_buffer* _opb) {
  
  th_quant_info *_qinfo = pbi->quant_info; 
  
  const th_quant_ranges *qranges;
  const th_quant_base   *base_mats[2*3*64];
  int                    indices[2][3][64];
  int                    nbase_mats;
  int                    nbits;
  int                    ci;
  int                    qi;
  int                    qri;
  int                    qti;
  int                    pli;
  int                    qtj;
  int                    plj;
  int                    bmi;
  int                    i;
  
  /*Unlike the scale tables, we can't assume the maximum value will be in
     index 0, so search for it here.*/
  i=_qinfo->loop_filter_limits[0];
  for(qi=1;qi<64;qi++)i=OC_MAXI(i,_qinfo->loop_filter_limits[qi]);
  nbits=oc_ilog(i);
  oggpackB_write(_opb,nbits,3);
  for(qi=0;qi<64;qi++){
    oggpackB_write(_opb,_qinfo->loop_filter_limits[qi],nbits);
  }
  /* 580 bits for VP3.*/
  nbits=OC_MAXI(oc_ilog(_qinfo->ac_scale[0]),1);
  oggpackB_write(_opb,nbits-1,4);
  for(qi=0;qi<64;qi++)oggpackB_write(_opb,_qinfo->ac_scale[qi],nbits);
  /* 516 bits for VP3.*/
  nbits=OC_MAXI(oc_ilog(_qinfo->dc_scale[0]),1);
  oggpackB_write(_opb,nbits-1,4);
  for(qi=0;qi<64;qi++)oggpackB_write(_opb,_qinfo->dc_scale[qi],nbits);
  /*Consolidate any duplicate base matrices.*/
  nbase_mats=0;
  for(qti=0;qti<2;qti++)for(pli=0;pli<3;pli++){
    qranges=_qinfo->qi_ranges[qti]+pli;
    for(qri=0;qri<=qranges->nranges;qri++){
      for(bmi=0;;bmi++){
        if(bmi>=nbase_mats){
          base_mats[bmi]=qranges->base_matrices+qri;
          indices[qti][pli][qri]=nbase_mats++;
          break;
        }
        else if(memcmp(base_mats[bmi][0],qranges->base_matrices[qri],
         sizeof(base_mats[bmi][0]))==0){
          indices[qti][pli][qri]=bmi;
          break;
        }
      }
    }
  }
  /*Write out the list of unique base matrices.
    1545 bits for VP3 matrices.*/
  oggpackB_write(_opb,nbase_mats-1,9);
  for(bmi=0;bmi<nbase_mats;bmi++){
    for(ci=0;ci<64;ci++)oggpackB_write(_opb,base_mats[bmi][0][ci],8);
  }
  /*Now store quant ranges and their associated indices into the base matrix
     list.
     46 bits for VP3 matrices.*/
  nbits=oc_ilog(nbase_mats-1);
  for(i=0;i<6;i++){
    qti=i/3;
    pli=i%3;
    qranges=_qinfo->qi_ranges[qti]+pli;
    if(i>0){
      if(qti>0){
        if(qranges->nranges==_qinfo->qi_ranges[qti-1][pli].nranges&&
         memcmp(qranges->sizes,_qinfo->qi_ranges[qti-1][pli].sizes,
         qranges->nranges*sizeof(qranges->sizes[0]))==0&&
         memcmp(indices[qti][pli],indices[qti-1][pli],
         (qranges->nranges+1)*sizeof(indices[qti][pli][0]))==0){
          oggpackB_write(_opb,1,2);
          continue;
        }
      }
      qtj=(i-1)/3;
      plj=(i-1)%3;
      if(qranges->nranges==_qinfo->qi_ranges[qtj][plj].nranges&&
       memcmp(qranges->sizes,_qinfo->qi_ranges[qtj][plj].sizes,
       qranges->nranges*sizeof(qranges->sizes[0]))==0&&
       memcmp(indices[qti][pli],indices[qtj][plj],
       (qranges->nranges+1)*sizeof(indices[qti][pli][0]))==0){
        oggpackB_write(_opb,0,1+(qti>0));
        continue;
      }
      oggpackB_write(_opb,1,1);
    }
    oggpackB_write(_opb,indices[qti][pli][0],nbits);
    for(qi=qri=0;qi<63;qri++){
      oggpackB_write(_opb,qranges->sizes[qri]-1,oc_ilog(62-qi));
      qi+=qranges->sizes[qri];
      oggpackB_write(_opb,indices[qti][pli][qri+1],nbits);
    }
  }
}


/* Initialize custom qtables using the VP31 values.
   Someday we can change the quant tables to be adaptive, or just plain
    better. */
void InitQTables( PB_INSTANCE *pbi ){
    
  switch(pbi->encoder_profile) {
  	case PROFILE_FULL:
  	
  	  pbi->quant_info = (th_quant_info *) &TH_VP31_QUANT_INFO;
      
      break;
    default: /* VP3 */
      
      pbi->quant_info = (th_quant_info *) &TH_VP31_QUANT_INFO;
      
      break;
  }
    
  pbi->QThreshTable = pbi->quant_info->ac_scale;
  
  quant_tables_init(pbi, pbi->quant_info);
}

static void BuildZigZagIndex(PB_INSTANCE *pbi){
  ogg_int32_t i,j;

  /* invert the row to zigzag coeffient order lookup table */
  for ( i = 0; i < BLOCK_SIZE; i++ ){
    j = dezigzag_index[i];
    pbi->zigzag_index[j] = i;
  }
}

/* this is a butchered version of derf's theora-exp code */
void quant_tables_init(PB_INSTANCE *pbi, const th_quant_info *_qinfo){
  int          qti;
  int          pli;
  for(qti=0;qti<2;qti++)for(pli=0;pli<3;pli++){
    int qi;
    int qri;
    /*These simple checks help us improve cache coherency later.*/
    /*if(pli>0&&memcmp(_qinfo->qi_ranges[qti]+pli-1,
     _qinfo->qi_ranges[qti]+pli,sizeof(_qinfo->qi_ranges[qti][pli]))==0){
      pbi->quant_tables[qti][pli]=pbi->quant_tables[qti][pli-1];
      continue;
    }
    if(qti>0&&memcmp(_qinfo->qi_ranges[qti-1]+pli,
     _qinfo->qi_ranges[qti]+pli,sizeof(_qinfo->qi_ranges[qti][pli]))==0){
      pbi->quant_tables[qti][pli]=pbi->quant_tables[qti-1][pli];
      continue;
    }*/
    for(qi=qri=0;qri<=_qinfo->qi_ranges[qti][pli].nranges;qri++){
      int i;
      ogg_uint16_t  base[64];
      int           qi_start;
      int           qi_end;
      int           ci;

      for(i = 0; i < 64; i++)
        base[i] = _qinfo->qi_ranges[qti][pli].base_matrices[qri][i];

      qi_start=qi;
      if(qri==_qinfo->qi_ranges[qti][pli].nranges)qi_end=qi+1;
      else qi_end=qi+_qinfo->qi_ranges[qti][pli].sizes[qri];
      for(;;){
               
        memcpy(pbi->quant_tables[qti][pli][qi], base, sizeof(base));
                
        if(++qi>=qi_end)break;
        /*Interpolate the next base matrix.*/
        for(ci=0;ci<64;ci++){
          base[ci]=(unsigned char)(
           (2*((qi_end-qi)*_qinfo->qi_ranges[qti][pli].base_matrices[qri][ci]+
           (qi-qi_start)*_qinfo->qi_ranges[qti][pli].base_matrices[qri+1][ci])
           +_qinfo->qi_ranges[qti][pli].sizes[qri])/
           (2*_qinfo->qi_ranges[qti][pli].sizes[qri]));
        }
      }
    }
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


    const ogg_uint16_t * temp_Y_coeffs;
    const ogg_uint16_t * temp_U_coeffs;
    const ogg_uint16_t * temp_V_coeffs;
    const ogg_uint16_t * temp_Inter_Y_coeffs;
    const ogg_uint16_t * temp_Inter_U_coeffs;
    const ogg_uint16_t * temp_Inter_V_coeffs;
    const ogg_uint16_t * temp_DcScaleFactorTable;

    /* Notes on setup of quantisers.  The initial multiplication by
     the scale factor is done in the ogg_int32_t domain to insure that the
     precision in the quantiser is the same as in the inverse
     quantiser where all calculations are integer.  The "<< 2" is a
     normalisation factor for the forward DCT transform. */
    
    temp_Y_coeffs = pbi->quant_tables[0][0][QIndex];
    temp_U_coeffs = pbi->quant_tables[0][1][QIndex];
    temp_V_coeffs = pbi->quant_tables[0][2][QIndex];
    temp_Inter_Y_coeffs = pbi->quant_tables[1][0][QIndex];
    temp_Inter_U_coeffs = pbi->quant_tables[1][1][QIndex];
    temp_Inter_V_coeffs = pbi->quant_tables[1][2][QIndex];
    temp_DcScaleFactorTable = pbi->quant_info->dc_scale;
    
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
  
    int col;
    /* Iterate through columns */
    for( col = 0; col < 8; col++) {
      if ( DCT_blockPtr[col] >= FquantZBinSizePtr[col] ) {
        temp = FquantCoeffsPtr[col] * ( DCT_blockPtr[col] + FquantRoundPtr[col] ) ;
        val = (Q_LIST_ENTRY) (temp>>16);
        quantized_list[ZigZagPtr[col]] = ( val > 511 ) ? 511 : val;
      } else if ( DCT_blockPtr[col] <= -FquantZBinSizePtr[col] ) {
        temp = FquantCoeffsPtr[col] *
          ( DCT_blockPtr[col] - FquantRoundPtr[col] ) + MIN16;
        val = (Q_LIST_ENTRY) (temp>>16);
        quantized_list[ZigZagPtr[col]] = ( val < -511 ) ? -511 : val;
      }
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

  ogg_uint16_t * InterY_coeffs;
  ogg_uint16_t * InterU_coeffs;
  ogg_uint16_t * InterV_coeffs;
  ogg_uint16_t * Y_coeffs;
  ogg_uint16_t * U_coeffs;
  ogg_uint16_t * V_coeffs;
  ogg_uint16_t * DcScaleFactorTable;


  Y_coeffs = pbi->quant_tables[0][0][QIndex];
  U_coeffs = pbi->quant_tables[0][1][QIndex];
  V_coeffs = pbi->quant_tables[0][2][QIndex];
  InterY_coeffs = pbi->quant_tables[1][0][QIndex];
  InterU_coeffs = pbi->quant_tables[1][1][QIndex];
  InterV_coeffs = pbi->quant_tables[1][2][QIndex];
  DcScaleFactorTable = pbi->quant_info->dc_scale;

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
  
  qscale = pbi->quant_info->ac_scale[NewQIndex];
  pbi->ThisFrameQualityValue = qscale;

  /* Re-initialise the Q tables for forward and reverse transforms. */
  init_dequantizer ( pbi, qscale, (unsigned char) pbi->FrameQIndex );
}

void UpdateQC( CP_INSTANCE *cpi, ogg_uint32_t NewQ ){
  ogg_uint32_t qscale;
  PB_INSTANCE *pbi = &cpi->pb;

  /* Do bounds checking and convert to a float.  */
  qscale = NewQ;
  if ( qscale < pbi->quant_info->ac_scale[Q_TABLE_SIZE-1] )
    qscale = pbi->quant_info->ac_scale[Q_TABLE_SIZE-1];
  else if ( qscale > pbi->quant_info->ac_scale[0] )
    qscale = pbi->quant_info->ac_scale[0];

  /* Set the inter/intra descision control variables. */
  pbi->FrameQIndex = Q_TABLE_SIZE - 1;
  while ((ogg_int32_t) pbi->FrameQIndex >= 0 ) {
    if ( (pbi->FrameQIndex == 0) ||
         ( pbi->quant_info->ac_scale[pbi->FrameQIndex] >= NewQ) )
      break;
    pbi->FrameQIndex --;
  }

  /* Re-initialise the Q tables for forward and reverse transforms. */
  init_quantizer ( cpi, qscale, (unsigned char) pbi->FrameQIndex );
  init_dequantizer ( pbi, qscale, (unsigned char) pbi->FrameQIndex );
}
