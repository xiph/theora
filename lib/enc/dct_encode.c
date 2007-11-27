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

#include <stdio.h>
static int ModeUsesMC[MAX_MODES] = { 0, 0, 1, 1, 1, 0, 1, 1 };

static unsigned char TokenizeDctValue (ogg_int16_t DataValue,
                                       ogg_uint32_t *TokenListPtr ){
  int AbsDataVal = abs(DataValue);
  int neg = (DataValue<0);
  /* Values are tokenised as category value and a number of additional
     bits that define the position within the category.  */

  if ( AbsDataVal == 0 ) return 0;

  if ( AbsDataVal == 1 ){

    TokenListPtr[0] = (neg ? MINUS_ONE_TOKEN : ONE_TOKEN);
    return 1;

  } else if ( AbsDataVal == 2 ) {

    TokenListPtr[0] = (neg ? MINUS_TWO_TOKEN : TWO_TOKEN);
    return 1;

  } else if ( AbsDataVal <= MAX_SINGLE_TOKEN_VALUE ) {

    TokenListPtr[0] = LOW_VAL_TOKENS + (AbsDataVal - DCT_VAL_CAT2_MIN);
    TokenListPtr[1] = neg;
    return 2;

  } else if ( AbsDataVal <= 8 ) {

    /* Bit 1 determines sign, Bit 0 the value */
    TokenListPtr[0] = DCT_VAL_CATEGORY3;
    TokenListPtr[1] = (AbsDataVal - DCT_VAL_CAT3_MIN) + (neg << 1);
    return 2;

  } else if ( AbsDataVal <= 12 ) {

    /* Bit 2 determines sign, Bit 0-2 the value */
    TokenListPtr[0] = DCT_VAL_CATEGORY4;
    TokenListPtr[1] = (AbsDataVal - DCT_VAL_CAT4_MIN) + (neg << 2);
    return 2;

  } else if ( AbsDataVal <= 20 ) {

    /* Bit 3 determines sign, Bit 0-2 the value */
    TokenListPtr[0] = DCT_VAL_CATEGORY5;
    TokenListPtr[1] = (AbsDataVal - DCT_VAL_CAT5_MIN) + (neg << 3);
    return 2;

  } else if ( AbsDataVal <= 36 ) {

    /* Bit 4 determines sign, Bit 0-3 the value */
    TokenListPtr[0] = DCT_VAL_CATEGORY6;
    TokenListPtr[1] = (AbsDataVal - DCT_VAL_CAT6_MIN) + (neg << 4);
    return 2;

  } else if ( AbsDataVal <= 68 ) {

    /* Bit 5 determines sign, Bit 0-4 the value */
    TokenListPtr[0] = DCT_VAL_CATEGORY7;
    TokenListPtr[1] = (AbsDataVal - DCT_VAL_CAT7_MIN) + (neg << 5);
    return 2;

  } else {

    /* Bit 9 determines sign, Bit 0-8 the value */
    TokenListPtr[0] = DCT_VAL_CATEGORY8;
    TokenListPtr[1] = (AbsDataVal - DCT_VAL_CAT8_MIN) + (neg <<9 );
    return 2;

  } 
}

static unsigned char TokenizeDctRunValue (unsigned char RunLength,
                                          ogg_int16_t DataValue,
                                          ogg_uint32_t *TokenListPtr ){
  unsigned char tokens_added = 0;
  ogg_uint32_t AbsDataVal = abs( (ogg_int32_t)DataValue );

  /* Values are tokenised as category value and a number of additional
     bits  that define the category.  */
  if ( DataValue == 0 ) return 0;
  if ( AbsDataVal == 1 ) {
    /* Zero runs of 1-5 */
    if ( RunLength <= 5 ) {
      TokenListPtr[0] = DCT_RUN_CATEGORY1 + (RunLength - 1);
      if ( DataValue > 0 )
        TokenListPtr[1] = 0;
      else
        TokenListPtr[1] = 1;
    } else if ( RunLength <= 9 ) {
      /* Zero runs of 6-9 */
      TokenListPtr[0] = DCT_RUN_CATEGORY1B;
      if ( DataValue > 0 )
        TokenListPtr[1] = (RunLength - 6);
      else
        TokenListPtr[1] = 0x04 + (RunLength - 6);
    } else {
      /* Zero runs of 10-17 */
      TokenListPtr[0] = DCT_RUN_CATEGORY1C;
      if ( DataValue > 0 )
        TokenListPtr[1] = (RunLength - 10);
      else
        TokenListPtr[1] = 0x08 + (RunLength - 10);
    }
    tokens_added = 2;
  } else if ( AbsDataVal <= 3 ) {
    if ( RunLength == 1 ) {
      TokenListPtr[0] = DCT_RUN_CATEGORY2;

      /* Extra bits token bit 1 indicates sign, bit 0 indicates value */
      if ( DataValue > 0 )
        TokenListPtr[1] = (AbsDataVal - 2);
      else
        TokenListPtr[1] = (0x02) + (AbsDataVal - 2);
      tokens_added = 2;
    }else{
      TokenListPtr[0] = DCT_RUN_CATEGORY2 + 1;

      /* Extra bits token. */
      /* bit 2 indicates sign, bit 1 indicates value, bit 0 indicates
         run length */
      if ( DataValue > 0 )
        TokenListPtr[1] = ((AbsDataVal - 2) << 1) + (RunLength - 2);
      else
        TokenListPtr[1] = (0x04) + ((AbsDataVal - 2) << 1) + (RunLength - 2);
      tokens_added = 2;
    }
  } else  {
    tokens_added = 2;  /* ERROR */
    /*IssueWarning( "Bad Input to TokenizeDctRunValue" );*/
  }

  /* Return the total number of tokens added */
  return tokens_added;
}

static unsigned char TokenizeDctBlock (fragment_t *fp){
  ogg_int16_t DC = fp->pred_dc;
  ogg_int16_t *RawData = fp->dct;
  ogg_uint32_t *TokenListPtr = fp->token_list;
  ogg_uint32_t i;
  unsigned char  run_count;
  unsigned char  token_count = 0;     /* Number of tokens crated. */
  ogg_uint32_t AbsData;
  
  /* Tokenize the block */
  for( i = 0; i < BLOCK_SIZE; i++ ){
    ogg_int16_t val = (i ? RawData[i] : DC);
    run_count = 0;

    /* Look for a zero run.  */
    /* NOTE the use of & instead of && which is faster (and
       equivalent) in this instance. */
    /* NO, NO IT ISN'T --Monty */
    while( (i < BLOCK_SIZE) && (!val) ){
      run_count++;
      i++;
      val = RawData[i];
    }

    /* If we have reached the end of the block then code EOB */
    if ( i == BLOCK_SIZE ){
      TokenListPtr[token_count] = DCT_EOB_TOKEN;
      token_count++;
    }else{
      /* If we have a short zero run followed by a low data value code
         the two as a composite token. */
      if ( run_count ){
        AbsData = abs(val);

        if ( ((AbsData == 1) && (run_count <= 17)) ||
             ((AbsData <= 3) && (run_count <= 3)) ) {
          /* Tokenise the run and subsequent value combination value */
          token_count += TokenizeDctRunValue( run_count, val,
                                              &TokenListPtr[token_count] );
        }else{

        /* Else if we have a long non-EOB run or a run followed by a
           value token > MAX_RUN_VAL then code the run and token
           seperately */
          if ( run_count <= 8 )
            TokenListPtr[token_count] = DCT_SHORT_ZRL_TOKEN;
          else
            TokenListPtr[token_count] = DCT_ZRL_TOKEN;

          token_count++;
          TokenListPtr[token_count] = run_count - 1;
          token_count++;

          /* Now tokenize the value */
          token_count += TokenizeDctValue( val, &TokenListPtr[token_count] );
        }
      }else{
        /* Else there was NO zero run. */
        /* Tokenise the value  */
        token_count += TokenizeDctValue( val, &TokenListPtr[token_count] );
      }
    }
  }

  /* Return the total number of tokens (including additional bits
     tokens) used. */
  return token_count;
}

ogg_uint32_t DPCMTokenizeBlock (CP_INSTANCE *cpi,
				fragment_t *fp){
  /* Tokenise the dct data. */

  int token_count = TokenizeDctBlock(fp);  
  fp->tokens_coded = token_count;
  cpi->TotTokenCount += token_count;

  /* Return number of pixels coded (i.e. 8x8). */
  return BLOCK_SIZE;
}

static int AllZeroDctData( ogg_int16_t * QuantList ){
  ogg_uint32_t i;

  for ( i = 0; i < 64; i ++ )
    if ( QuantList[i] != 0 )
      return 0;
  
  return 1;
}

static void BlockUpdateDifference (CP_INSTANCE * cpi, 
				   unsigned char *FiltPtr,
				   ogg_int16_t *DctInputPtr, 
				   unsigned char *thisrecon,
				   ogg_int32_t MvDevisor,
				   fragment_t *fp,
				   ogg_uint32_t PixelsPerLine,
				   ogg_uint32_t ReconPixelsPerLine,
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
    MVOffset = ((mv.y / MvDevisor) * ReconPixelsPerLine) + (mv.x / MvDevisor);
    
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
	ReconPtr2Offset += ReconPixelsPerLine;
      else
	ReconPtr2Offset -= ReconPixelsPerLine;
    }
    
    if ( mode==CODE_GOLDEN_MV ) {
      ReconPtr1 = &cpi->pb.GoldenFrame[fp->recon_index];
    } else {
      ReconPtr1 = &cpi->pb.LastFrameRecon[fp->recon_index];
    }
    
    ReconPtr1 += MVOffset;
    ReconPtr2 =  ReconPtr1 + ReconPtr2Offset;
    
    AbsRefOffset = abs((int)(ReconPtr1 - ReconPtr2));
    
    /* Is the MV offset exactly pixel alligned */
    if ( AbsRefOffset == 0 ){
      dsp_sub8x8(cpi->dsp, FiltPtr, ReconPtr1, DctInputPtr,
		 PixelsPerLine, ReconPixelsPerLine);
      dsp_copy8x8 (cpi->dsp, ReconPtr1, thisrecon, ReconPixelsPerLine);
    } else {
      /* Fractional pixel MVs. */
      /* Note that we only use two pixel values even for the diagonal */
      dsp_sub8x8avg2(cpi->dsp, FiltPtr, ReconPtr1,ReconPtr2,DctInputPtr,
		     PixelsPerLine, ReconPixelsPerLine);
      dsp_copy8x8_half (cpi->dsp, ReconPtr1, ReconPtr2, thisrecon, ReconPixelsPerLine);
    }

  } else { 
    if ( ( mode==CODE_INTER_NO_MV ) ||
	 ( mode==CODE_USING_GOLDEN ) ) {
      if ( mode==CODE_INTER_NO_MV ) {
	ReconPtr1 = &cpi->pb.LastFrameRecon[fp->recon_index];
      } else {
	ReconPtr1 = &cpi->pb.GoldenFrame[fp->recon_index];
      }
      
      dsp_sub8x8(cpi->dsp, FiltPtr, ReconPtr1, DctInputPtr,
		 PixelsPerLine, ReconPixelsPerLine);
      dsp_copy8x8 (cpi->dsp, ReconPtr1, thisrecon, ReconPixelsPerLine);
    } else if ( mode==CODE_INTRA ) {
      dsp_sub8x8_128(cpi->dsp, FiltPtr, DctInputPtr, PixelsPerLine);
      dsp_set8x8(cpi->dsp, 128, thisrecon, ReconPixelsPerLine);
    }
  }
}

void TransformQuantizeBlock (CP_INSTANCE *cpi, 
			     fragment_t *fp,
                             ogg_uint32_t PixelsPerLine) {
  unsigned char *FiltPtr = &cpi->yuvptr[fp->raw_index];
  int qi = cpi->BaseQ; // temporary
  int inter = (fp->mode != CODE_INTRA);
  int plane = (fp < cpi->frag[1] ? 0 : (fp < cpi->frag[2] ? 1 : 2)); 
  ogg_int32_t *q = cpi->pb.iquant_tables[inter][plane][qi];
  ogg_int16_t DCTInput[64];
  ogg_int16_t DCTOutput[64];
  ogg_uint32_t ReconPixelsPerLine = cpi->recon_stride[plane];
  ogg_int32_t   MvDivisor;      /* Defines MV resolution (2 = 1/2
                                   pixel for Y or 4 = 1/4 for UV) */
  unsigned char   *ReconPtr1 = &cpi->pb.ThisFrameRecon[fp->recon_index];

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
			MvDivisor, fp, PixelsPerLine,
			ReconPixelsPerLine, fp->mode);
  
  /* Proceed to encode the data into the encode buffer if the encoder
     is enabled. */
  /* Perform a 2D DCT transform on the data. */
  dsp_fdct_short(cpi->dsp, DCTInput, DCTOutput);

  /* Quantize that transform data. */
  quantize ( &cpi->pb, q, DCTOutput, fp->dct );

  if ( (fp->mode == CODE_INTER_NO_MV) &&
       ( AllZeroDctData(fp->dct) ) ) {
    fp->coded = 0;
  }

}
