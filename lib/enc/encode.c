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
#include "encoder_lookup.h"

static void PredictDC(CP_INSTANCE *cpi){
  ogg_int32_t plane;
  int k,m,n;

  /* value left value up-left, value up, value up-right, missing
      values skipped. */
  int v[4];

  /* fragment number left, up-left, up, up-right */
  int fn[4];

  /* predictor count. */
  int pcount;

  /*which predictor constants to use */
  ogg_int16_t wpc;

  /* last used inter predictor (Raster Order) */
  ogg_int16_t Last[3];  /* last value used for given frame */

  int WhichFrame;
  int WhichCase;

  /* for y,u,v */
  for ( plane = 0; plane < 3 ; plane++) {

    fragment_t *fp = cpi->frag[plane];

    /* initialize our array of last used DC Components */
    for(k=0;k<3;k++)Last[k]=0;

    fn[0]=1;
    fn[1]=cpi->frag_h[plane]+1;
    fn[2]=cpi->frag_h[plane];
    fn[3]=cpi->frag_h[plane]-1;
    

    /* do prediction on all of Y, U or V */
    for ( m = 0 ; m < cpi->frag_v[plane] ; m++) {
      for ( n = 0 ; n < cpi->frag_h[plane] ; n++, fp++) {
        fp->pred_dc = fp->dct[0];

        /* only do 2 prediction if fragment coded and on non intra or
           if all fragments are intra */
        if( fp->coded ) {
          /* Type of Fragment */

          WhichFrame = Mode2Frame[fp->mode];

          /* Check Borderline Cases */
          WhichCase = (n==0) + ((m==0) << 1) + ((n+1 == cpi->frag_h[plane]) << 2);

          /* fragment valid for prediction use if coded and it comes
             from same frame as the one we are predicting */
          for(k=pcount=wpc=0; k<4; k++) {
            int pflag = 1<<k;
            if((bc_mask[WhichCase]&pflag)){
	      fragment_t *fnp=fp - fn[k];	      
	      if(fnp->coded &&
		 (Mode2Frame[fnp->mode] == WhichFrame)){
		v[pcount]=fnp->dct[0];
		wpc|=pflag;
		pcount++;
	      }
	    }
          }

          if(wpc==0) {

            /* fall back to the last coded fragment */
            fp->pred_dc -= Last[WhichFrame];

          } else {

            /* don't do divide if divisor is 1 or 0 */
            ogg_int16_t DC = pc[wpc][0]*v[0];
            for(k=1; k<pcount; k++)
              DC += pc[wpc][k]*v[k];
	    
            /* if we need to do a shift */
            if(pc[wpc][4] != 0 ) {
	      
              /* If negative add in the negative correction factor */
              DC += (HIGHBITDUPPED(DC) & pc[wpc][5]);
              /* Shift in lieu of a divide */
              DC >>= pc[wpc][4];

            }

            /* check for outranging on the two predictors that can outrange */
            if((wpc&(PU|PUL|PL)) == (PU|PUL|PL)){
              if( abs(DC - v[2]) > 128) {
                DC = v[2];
              } else if( abs(DC - v[0]) > 128) {
                DC = v[0];
              } else if( abs(DC - v[1]) > 128) {
                DC = v[1];
              }
            }

            fp->pred_dc -= DC;
          }

          /* Save the last fragment coded for whatever frame we are
             predicting from */

          Last[WhichFrame] = fp->dct[0];

        }
      }
    }
  }
}


static ogg_uint32_t CodePlane ( CP_INSTANCE *cpi,
				int plane ){

  ogg_uint32_t SBs = cpi->super_n[plane];
  ogg_uint32_t SB, MB, B;
  
  /* actually transform and quantize the image now that we've decided
     on the modes Parse in quad-tree ordering */

  for ( SB=0; SB<SBs; SB++ ){
    superblock_t *sp = &cpi->super[plane][SB];
    int frag=0;

    int SBi = SB;
    if(plane>0)SBi+=cpi->super_n[0];
    if(plane>1)SBi+=cpi->super_n[1];

    for ( MB=0; MB<4; MB++ ) {
      int coded = 0;
      int mode = 0;

      for ( B=0; B<4; B++, frag++ ) {
	fragment_t *fp = sp->f[frag];

	/* Does Block lie in frame: */
	if ( fp && fp->coded ) {
	  
	  /* transform and quantize block */
	  TransformQuantizeBlock( cpi, fp );
	  
	  /* Has the block got struck off (no MV and no data
	     generated after DCT) If not then mark it and the
	     assosciated MB as coded. */
	  if ( fp->coded ) {
	    /* Create linear list of coded block indices */
	    cpi->CodedBlockList[cpi->CodedBlockIndex++] = fp;
	    
	    /* MB is still coded */
	    coded = 1;
	    mode = fp->mode;
	    
	  }
	}
      }
     
      /* If the MB is marked as coded and we are in the Y plane then */
      /* the mode list needs to be updated. */
      if ( coded && plane == 0 ){
	/* Make a note of the selected mode in the mode list */
	cpi->ModeList[cpi->ModeListCount++] = mode;
      }
    }  
  }
  return 0;
}

static void EncodeDcTokenList (CP_INSTANCE *cpi) {
  ogg_int32_t   i,j;
  ogg_uint32_t  Token;
  ogg_uint32_t  ExtraBitsToken;
  ogg_uint32_t  HuffIndex;

  ogg_uint32_t  BestDcBits;
  ogg_uint32_t  DcHuffChoice[2];
  ogg_uint32_t  EntropyTableBits[2][DC_HUFF_CHOICES];

  oggpack_buffer *opb=cpi->oggbuffer;

  /* Clear table data structure */
  memset ( EntropyTableBits, 0, sizeof(ogg_uint32_t)*DC_HUFF_CHOICES*2 );

  /* Analyse token list to see which is the best entropy table to use */
  for ( i = 0; i < cpi->OptimisedTokenCount; i++ ) {
    /* Count number of bits for each table option */
    Token = (ogg_uint32_t)cpi->OptimisedTokenList[i];
    for ( j = 0; j < DC_HUFF_CHOICES; j++ ){
      EntropyTableBits[cpi->OptimisedTokenListPl[i]][j] +=
        cpi->HuffCodeLengthArray_VP3x[DC_HUFF_OFFSET + j][Token];
    }
  }

  /* Work out which table option is best for Y */
  BestDcBits = EntropyTableBits[0][0];
  DcHuffChoice[0] = 0;
  for ( j = 1; j < DC_HUFF_CHOICES; j++ ) {
    if ( EntropyTableBits[0][j] < BestDcBits ) {
      BestDcBits = EntropyTableBits[0][j];
      DcHuffChoice[0] = j;
    }
  }

  /* Add the DC huffman table choice to the bitstream */
  oggpackB_write( opb, DcHuffChoice[0], DC_HUFF_CHOICE_BITS );

  /* Work out which table option is best for UV */
  BestDcBits = EntropyTableBits[1][0];
  DcHuffChoice[1] = 0;
  for ( j = 1; j < DC_HUFF_CHOICES; j++ ) {
    if ( EntropyTableBits[1][j] < BestDcBits ) {
      BestDcBits = EntropyTableBits[1][j];
      DcHuffChoice[1] = j;
    }
  }

  /* Add the DC huffman table choice to the bitstream */
  oggpackB_write( opb, DcHuffChoice[1], DC_HUFF_CHOICE_BITS );

  /* Encode the token list */
  for ( i = 0; i < cpi->OptimisedTokenCount; i++ ) {

    /* Get the token and extra bits */
    Token = (ogg_uint32_t)cpi->OptimisedTokenList[i];
    ExtraBitsToken = (ogg_uint32_t)cpi->OptimisedTokenListEb[i];

    /* Select the huffman table */
    if ( cpi->OptimisedTokenListPl[i] == 0)
      HuffIndex = (ogg_uint32_t)DC_HUFF_OFFSET + (ogg_uint32_t)DcHuffChoice[0];
    else
      HuffIndex = (ogg_uint32_t)DC_HUFF_OFFSET + (ogg_uint32_t)DcHuffChoice[1];

    /* Add the bits to the encode holding buffer. */
    oggpackB_write( opb, cpi->HuffCodeArray_VP3x[HuffIndex][Token],
                     (ogg_uint32_t)cpi->
                     HuffCodeLengthArray_VP3x[HuffIndex][Token] );

    /* If the token is followed by an extra bits token then code it */
    if ( cpi->ExtraBitLengths_VP3x[Token] > 0 ) {
      /* Add the bits to the encode holding buffer.  */
      oggpackB_write( opb, ExtraBitsToken,
                       (ogg_uint32_t)cpi->ExtraBitLengths_VP3x[Token] );
    }

  }

  /* Reset the count of second order optimised tokens */
  cpi->OptimisedTokenCount = 0;
}

static void EncodeAcTokenList (CP_INSTANCE *cpi) {
  ogg_int32_t   i,j;
  ogg_uint32_t  Token;
  ogg_uint32_t  ExtraBitsToken;
  ogg_uint32_t  HuffIndex;

  ogg_uint32_t  BestAcBits;
  ogg_uint32_t  AcHuffChoice[2];
  ogg_uint32_t  EntropyTableBits[2][AC_HUFF_CHOICES];

  oggpack_buffer *opb=cpi->oggbuffer;

  memset ( EntropyTableBits, 0, sizeof(ogg_uint32_t)*AC_HUFF_CHOICES*2 );

  /* Analyse token list to see which is the best entropy table to use */
  for ( i = 0; i < cpi->OptimisedTokenCount; i++ ) {
    /* Count number of bits for each table option */
    Token = (ogg_uint32_t)cpi->OptimisedTokenList[i];
    HuffIndex = cpi->OptimisedTokenListHi[i];
    for ( j = 0; j < AC_HUFF_CHOICES; j++ ) {
      EntropyTableBits[cpi->OptimisedTokenListPl[i]][j] +=
        cpi->HuffCodeLengthArray_VP3x[HuffIndex + j][Token];
    }
  }

  /* Select the best set of AC tables for Y */
  BestAcBits = EntropyTableBits[0][0];
  AcHuffChoice[0] = 0;
  for ( j = 1; j < AC_HUFF_CHOICES; j++ ) {
    if ( EntropyTableBits[0][j] < BestAcBits ) {
      BestAcBits = EntropyTableBits[0][j];
      AcHuffChoice[0] = j;
    }
  }

  /* Add the AC-Y huffman table choice to the bitstream */
  oggpackB_write( opb, AcHuffChoice[0], AC_HUFF_CHOICE_BITS );

  /* Select the best set of AC tables for UV */
  BestAcBits = EntropyTableBits[1][0];
  AcHuffChoice[1] = 0;
  for ( j = 1; j < AC_HUFF_CHOICES; j++ ) {
    if ( EntropyTableBits[1][j] < BestAcBits ) {
      BestAcBits = EntropyTableBits[1][j];
      AcHuffChoice[1] = j;
    }
  }

  /* Add the AC-UV huffman table choice to the bitstream */
  oggpackB_write( opb, AcHuffChoice[1], AC_HUFF_CHOICE_BITS );

  /* Encode the token list */
  for ( i = 0; i < cpi->OptimisedTokenCount; i++ ) {
    /* Get the token and extra bits */
    Token = (ogg_uint32_t)cpi->OptimisedTokenList[i];
    ExtraBitsToken = (ogg_uint32_t)cpi->OptimisedTokenListEb[i];

    /* Select the huffman table */
    HuffIndex = (ogg_uint32_t)cpi->OptimisedTokenListHi[i] +
      AcHuffChoice[cpi->OptimisedTokenListPl[i]];

    /* Add the bits to the encode holding buffer. */
    oggpackB_write( opb, cpi->HuffCodeArray_VP3x[HuffIndex][Token],
		    (ogg_uint32_t)cpi->
		    HuffCodeLengthArray_VP3x[HuffIndex][Token] );

    /* If the token is followed by an extra bits token then code it */
    if ( cpi->ExtraBitLengths_VP3x[Token] > 0 ) {
      /* Add the bits to the encode holding buffer. */
      oggpackB_write( opb, ExtraBitsToken,
		      (ogg_uint32_t)cpi->ExtraBitLengths_VP3x[Token] );
    }
  }

  /* Reset the count of second order optimised tokens */
  cpi->OptimisedTokenCount = 0;
}

static void PackModes (CP_INSTANCE *cpi) {
  ogg_uint32_t    i,j;
  unsigned char   ModeIndex;
  const unsigned char  *SchemeList;

  unsigned char   BestModeSchemes[MAX_MODES];
  ogg_int32_t     ModeCount[MAX_MODES];
  ogg_int32_t     TmpFreq = -1;
  ogg_int32_t     TmpIndex = -1;

  ogg_uint32_t    BestScheme;
  ogg_uint32_t    BestSchemeScore;
  ogg_uint32_t    SchemeScore;

  oggpack_buffer *opb=cpi->oggbuffer;

  /* Build a frequency map for the modes in this frame */
  memset( ModeCount, 0, MAX_MODES*sizeof(ogg_int32_t) );
  for ( i = 0; i < cpi->ModeListCount; i++ )
    ModeCount[cpi->ModeList[i]] ++;

  /* Order the modes from most to least frequent.  Store result as
     scheme 0 */
  for ( j = 0; j < MAX_MODES; j++ ) {
    TmpFreq = -1;  /* need to re-initialize for each loop */
    /* Find the most frequent */
    for ( i = 0; i < MAX_MODES; i++ ) {
      /* Is this the best scheme so far ??? */
      if ( ModeCount[i] > TmpFreq ) {
        TmpFreq = ModeCount[i];
        TmpIndex = i;
      }
    }
    /* I don't know if the above loop ever fails to match, but it's
       better safe than sorry.  Plus this takes care of gcc warning */
    if ( TmpIndex != -1 ) {
      ModeCount[TmpIndex] = -1;
      BestModeSchemes[TmpIndex] = (unsigned char)j;
    }
  }

  /* Default/ fallback scheme uses MODE_BITS bits per mode entry */
  BestScheme = (MODE_METHODS - 1);
  BestSchemeScore = cpi->ModeListCount * 3;
  /* Get a bit score for the available schemes. */
  for (  j = 0; j < (MODE_METHODS - 1); j++ ) {

    /* Reset the scheme score */
    if ( j == 0 ){
      /* Scheme 0 additional cost of sending frequency order */
      SchemeScore = 24;
      SchemeList = BestModeSchemes;
    } else {
      SchemeScore = 0;
      SchemeList = ModeSchemes[j-1];
    }

    /* Find the total bits to code using each avaialable scheme */
    for ( i = 0; i < cpi->ModeListCount; i++ )
      SchemeScore += ModeBitLengths[SchemeList[cpi->ModeList[i]]];

    /* Is this the best scheme so far ??? */
    if ( SchemeScore < BestSchemeScore ) {
      BestSchemeScore = SchemeScore;
      BestScheme = j;
    }
  }

  /* Encode the best scheme. */
  oggpackB_write( opb, BestScheme, (ogg_uint32_t)MODE_METHOD_BITS );

  /* If the chosen schems is scheme 0 send details of the mode
     frequency order */
  if ( BestScheme == 0 ) {
    for ( j = 0; j < MAX_MODES; j++ ){
      /* Note that the last two entries are implicit */
      oggpackB_write( opb, BestModeSchemes[j], (ogg_uint32_t)MODE_BITS );
    }
    SchemeList = BestModeSchemes;
  }
  else {
    SchemeList = ModeSchemes[BestScheme-1];
  }

  /* Are we using one of the alphabet based schemes or the fallback scheme */
  if ( BestScheme < (MODE_METHODS - 1)) {
    /* Pack and encode the Mode list */
    for ( i = 0; i < cpi->ModeListCount; i++) {
      /* Add the appropriate mode entropy token. */
      ModeIndex = SchemeList[cpi->ModeList[i]];
      oggpackB_write( opb, ModeBitPatterns[ModeIndex],
		      (ogg_uint32_t)ModeBitLengths[ModeIndex] );
    }
  }else{
    /* Fall back to MODE_BITS per entry */
    for ( i = 0; i < cpi->ModeListCount; i++)
      /* Add the appropriate mode entropy token. */
      oggpackB_write( opb, cpi->ModeList[i], MODE_BITS  );  
  }
  
}

static void PackMotionVectors (CP_INSTANCE *cpi) {
  const ogg_uint32_t * MvPatternPtr;
  const ogg_uint32_t * MvBitsPtr;

  ogg_uint32_t SB, MB, B;

  oggpack_buffer *opb=cpi->oggbuffer;

  /* Choose the coding method */
  if ( cpi->MVBits_0 < cpi->MVBits_1 ) {
    oggpackB_write( opb, 0, 1 );
    MvBitsPtr = &MvBits[MAX_MV_EXTENT];
    MvPatternPtr = &MvPattern[MAX_MV_EXTENT];
  }else{
    oggpackB_write( opb, 1, 1 );
    MvBitsPtr = &MvBits2[MAX_MV_EXTENT];
    MvPatternPtr = &MvPattern2[MAX_MV_EXTENT];
  }

  /* Pack and encode the motion vectors */
  /* iterate through MB list */
  for ( SB=0 ; SB < cpi->super_n[0]; SB++ ){
    superblock_t *sp = &cpi->super[0][SB];
    for ( MB=0; MB<4; MB++ ) {
      macroblock_t *mp = sp->m[MB];
      fragment_t *fp;
      if(!mp) continue;
      fp = mp->y[0];
      for(B=1; !fp && B<4; B++ ) fp = mp->y[B];
      if(!fp) continue;
      if(fp->mode==CODE_INTER_PLUS_MV || fp->mode==CODE_GOLDEN_MV){
	/* One MV for the macroblock */
	oggpackB_write( opb, MvPatternPtr[fp->mv.x], MvBitsPtr[fp->mv.x] );
	oggpackB_write( opb, MvPatternPtr[fp->mv.y], MvBitsPtr[fp->mv.y] );
      }else if (fp->mode == CODE_INTER_FOURMV){
	/* MV for each codedblock */
	for(B=0; B<4; B++ ){
	  fp = mp->y[B];
	  if(fp){
	    oggpackB_write( opb, MvPatternPtr[fp->mv.x], MvBitsPtr[fp->mv.x] );
	    oggpackB_write( opb, MvPatternPtr[fp->mv.y], MvBitsPtr[fp->mv.y] );
	  }
	}
      }
    }
  }
}

static void PackEOBRun( CP_INSTANCE *cpi) {
  if(cpi->RunLength == 0)
        return;

  /* Note the appropriate EOB or EOB run token and any extra bits in
     the optimised token list.  Use the huffman index assosciated with
     the first token in the run */

  /* Mark out which plane the block belonged to */
  cpi->OptimisedTokenListPl[cpi->OptimisedTokenCount] =
    (unsigned char)cpi->RunPlaneIndex;

  /* Note the huffman index to be used */
  cpi->OptimisedTokenListHi[cpi->OptimisedTokenCount] =
    (unsigned char)cpi->RunHuffIndex;

  if ( cpi->RunLength <= 3 ) {
    if ( cpi->RunLength == 1 ) {
      cpi->OptimisedTokenList[cpi->OptimisedTokenCount] = DCT_EOB_TOKEN;
    } else if ( cpi->RunLength == 2 ) {
      cpi->OptimisedTokenList[cpi->OptimisedTokenCount] = DCT_EOB_PAIR_TOKEN;
    } else {
      cpi->OptimisedTokenList[cpi->OptimisedTokenCount] = DCT_EOB_TRIPLE_TOKEN;
    }

    cpi->RunLength = 0;

  } else {

    /* Choose a token appropriate to the run length. */
    if ( cpi->RunLength < 8 ) {
      cpi->OptimisedTokenList[cpi->OptimisedTokenCount] =
        DCT_REPEAT_RUN_TOKEN;
      cpi->OptimisedTokenListEb[cpi->OptimisedTokenCount] =
        cpi->RunLength - 4;
      cpi->RunLength = 0;
    } else if ( cpi->RunLength < 16 ) {
      cpi->OptimisedTokenList[cpi->OptimisedTokenCount] =
        DCT_REPEAT_RUN2_TOKEN;
      cpi->OptimisedTokenListEb[cpi->OptimisedTokenCount] =
        cpi->RunLength - 8;
      cpi->RunLength = 0;
    } else if ( cpi->RunLength < 32 ) {
      cpi->OptimisedTokenList[cpi->OptimisedTokenCount] =
        DCT_REPEAT_RUN3_TOKEN;
      cpi->OptimisedTokenListEb[cpi->OptimisedTokenCount] =
        cpi->RunLength - 16;
      cpi->RunLength = 0;
    } else if ( cpi->RunLength < 4096) {
      cpi->OptimisedTokenList[cpi->OptimisedTokenCount] =
        DCT_REPEAT_RUN4_TOKEN;
      cpi->OptimisedTokenListEb[cpi->OptimisedTokenCount] =
        cpi->RunLength;
      cpi->RunLength = 0;
    }

  }

  cpi->OptimisedTokenCount++;
  /* Reset run EOB length */
  cpi->RunLength = 0;
}

static int TokenCoeffs( ogg_uint32_t Token,
		 ogg_int32_t ExtraBits ){
  if ( Token == DCT_EOB_TOKEN )
    return BLOCK_SIZE;

  /* Is the token is a combination run and value token. */
  if ( Token >= DCT_RUN_CATEGORY1 ){
    /* Expand the token and additional bits to a zero run length and
       data value.  */
    if ( Token < DCT_RUN_CATEGORY2 ) {
      /* Decoding method depends on token */
      if ( Token < DCT_RUN_CATEGORY1B ) {
        /* Step on by the zero run length */
        return(Token - DCT_RUN_CATEGORY1) + 2;
      } else if ( Token == DCT_RUN_CATEGORY1B ) {
        /* Bits 0-1 determines the zero run length */
        return 7 + (ExtraBits & 0x03);
      }else{
        /* Bits 0-2 determines the zero run length */
        return 11 + (ExtraBits & 0x07);
      }
    }else{
      /* If token == DCT_RUN_CATEGORY2 we have a single 0 followed by
         a value */
      if ( Token == DCT_RUN_CATEGORY2 ){
        /* Step on by the zero run length */
        return  2;
      }else{
        /* else we have 2->3 zeros followed by a value */
        /* Bit 0 determines the zero run length */
        return 3 + (ExtraBits & 0x01);
      }
    }
  } 

  if ( Token == DCT_SHORT_ZRL_TOKEN ||  Token == DCT_ZRL_TOKEN ) 
    /* Token is a ZRL token so step on by the appropriate number of zeros */
    return ExtraBits + 1;

  return 1;
}

static void PackToken ( CP_INSTANCE *cpi, 
			fragment_t *fp,
			ogg_uint32_t HuffIndex ) {
  ogg_uint32_t Token = fp->token_list[fp->tokens_packed];
  ogg_uint32_t ExtraBitsToken = fp->token_list[fp->tokens_packed+1];
  ogg_uint32_t OneOrTwo;
  ogg_uint32_t OneOrZero;

  /* Update the record of what coefficient we have got up to for this
     block and unpack the encoded token back into the quantised data
     array. */
  fp->coeffs_packed += TokenCoeffs ( Token, ExtraBitsToken );

  /* Update record of tokens coded and where we are in this fragment. */
  /* Is there an extra bits token */
  OneOrTwo = 1 + ( cpi->ExtraBitLengths_VP3x[Token] > 0 );

  /* Advance to the next real token. */
  fp->tokens_packed += (unsigned char)OneOrTwo;

  OneOrZero = ( fp < cpi->frag[1] );

  if ( Token == DCT_EOB_TOKEN ) {
    if ( cpi->RunLength == 0 ) {
      cpi->RunHuffIndex = HuffIndex;
      cpi->RunPlaneIndex = 1 -  OneOrZero;
    }
    cpi->RunLength++;

    /* we have exceeded our longest run length  xmit an eob run token; */
    if ( cpi->RunLength == 4095 ) PackEOBRun(cpi);

  }else{

    /* If we have an EOB run then code it up first */
    if ( cpi->RunLength > 0 ) PackEOBRun( cpi);

    /* Mark out which plane the block belonged to */
    cpi->OptimisedTokenListPl[cpi->OptimisedTokenCount] =
      (unsigned char)(1 - OneOrZero);

    /* Note the token, extra bits and hufman table in the optimised
       token list */
    cpi->OptimisedTokenList[cpi->OptimisedTokenCount] =
      (unsigned char)Token;
    cpi->OptimisedTokenListEb[cpi->OptimisedTokenCount] =
      ExtraBitsToken;
    cpi->OptimisedTokenListHi[cpi->OptimisedTokenCount] =
      (unsigned char)HuffIndex;

    cpi->OptimisedTokenCount++;
  }
}

static void PackCodedVideo (CP_INSTANCE *cpi) {
  ogg_int32_t i;
  ogg_int32_t EncodedCoeffs = 1;
  ogg_uint32_t HuffIndex; /* Index to group of tables used to code a token */

  /* Reset the count of second order optimised tokens */
  cpi->OptimisedTokenCount = 0;

  /* Blank the various fragment data structures before we start. */
  for ( i = 0; i < cpi->CodedBlockIndex; i++ ) {
    fragment_t *fp = cpi->CodedBlockList[i];
    fp->coeffs_packed = 0;
    fp->tokens_packed = 0;
  }

  /* The tree is not needed (implicit) for key frames */
  if ( cpi->FrameType != KEY_FRAME ){
    /* Pack the quad tree fragment mapping. */
    PackAndWriteDFArray( cpi );
  }

  /* Mode and MV data not needed for key frames. */
  if ( cpi->FrameType != KEY_FRAME ){
    /* Pack and code the mode list. */
    PackModes(cpi);
    /* Pack the motion vectors */
    PackMotionVectors (cpi);
  }

  /* Optimise the DC tokens */
  for ( i = 0; i < cpi->CodedBlockIndex; i++ ) {
    /* Get the linear index for the current fragment. */
    fragment_t *fp = cpi->CodedBlockList[i];
    fp->nonzero = EncodedCoeffs;
    PackToken(cpi, fp, DC_HUFF_OFFSET );
  }
  
  /* Pack any outstanding EOB tokens */
  PackEOBRun(cpi);

  /* Now output the optimised DC token list using the appropriate
     entropy tables. */
  EncodeDcTokenList(cpi);

  /* Work out the number of DC bits coded */

  /* Optimise the AC tokens */
  while ( EncodedCoeffs < 64 ) {
    /* Huffman table adjustment based upon coefficient number. */
    if ( EncodedCoeffs <= AC_TABLE_2_THRESH )
      HuffIndex = AC_HUFF_OFFSET;
    else if ( EncodedCoeffs <= AC_TABLE_3_THRESH )
      HuffIndex = AC_HUFF_OFFSET + AC_HUFF_CHOICES;
    else if ( EncodedCoeffs <= AC_TABLE_4_THRESH )
      HuffIndex = AC_HUFF_OFFSET + (AC_HUFF_CHOICES * 2);
    else
      HuffIndex = AC_HUFF_OFFSET + (AC_HUFF_CHOICES * 3);

    /* Repeatedly scan through the list of blocks. */
    for ( i = 0; i < cpi->CodedBlockIndex; i++ ) {
      /* Get the linear index for the current fragment. */
      fragment_t *fp = cpi->CodedBlockList[i];

      /* Should we code a token for this block on this pass. */
      if ( fp->tokens_packed < fp->tokens_coded &&
           fp->coeffs_packed <= EncodedCoeffs ) {
        /* Bit pack and a token for this block */
        fp->nonzero = EncodedCoeffs;
        PackToken( cpi, fp, HuffIndex );
      }
    }

    EncodedCoeffs ++;
  }

  /* Pack any outstanding EOB tokens */
  PackEOBRun(cpi);

  /* Now output the optimised AC token list using the appropriate
     entropy tables. */
  EncodeAcTokenList(cpi);

}

void EncodeData(CP_INSTANCE *cpi){
  ogg_int32_t   i;

  /* Zero the mode and MV list indices. */
  cpi->ModeListCount = 0;
  
  /* Initialise the coded block indices variables. These allow
     subsequent linear access to the quad tree ordered list of coded
     blocks */
  cpi->CodedBlockIndex = 0;

  dsp_save_fpu (cpi->dsp);

  /* Encode and tokenise the Y, U and V components */
  CodePlane(cpi, 0);
  CodePlane(cpi, 1);
  CodePlane(cpi, 2);
  
  PredictDC(cpi);

  /* Pack DCT tokens */
  for ( i = 0; i < cpi->CodedBlockIndex; i++ ) 
    DPCMTokenizeBlock ( cpi, cpi->CodedBlockList[i] );

  /* Bit pack the video data data */
  PackCodedVideo(cpi);

  /* Reconstruct the reference frames */
  ReconRefFrames(cpi);

  dsp_restore_fpu (cpi->dsp);
}

ogg_uint32_t PickIntra( CP_INSTANCE *cpi ){

  int i;
  for(i=0;i<cpi->frag_total;i++){
    cpi->frag[0][i].mode = CODE_INTRA;
    cpi->frag[0][i].coded = 1;
  }
  return 0;
}

static void CountMotionVector(CP_INSTANCE *cpi, mv_t *mv) {
  cpi->MVBits_0 += MvBits[mv->x];
  cpi->MVBits_0 += MvBits[mv->y];
  cpi->MVBits_1 += 12; /* Simple six bits per mv component fallback */
}

static void SetFragMotionVectorAndMode(fragment_t *fp,
				       mv_t *mv,
				       int mode){
  fp->mv = *mv;
  fp->mode = mode;
}

static void SetMBMotionVectorsAndMode(macroblock_t *mp,
				      mv_t *mv,
				      int mode){
  SetFragMotionVectorAndMode(mp->y[0], mv, mode);
  SetFragMotionVectorAndMode(mp->y[1], mv, mode);
  SetFragMotionVectorAndMode(mp->y[2], mv, mode);
  SetFragMotionVectorAndMode(mp->y[3], mv, mode);
  SetFragMotionVectorAndMode(mp->u, mv, mode);
  SetFragMotionVectorAndMode(mp->v, mv, mode);
}

ogg_uint32_t PickModes(CP_INSTANCE *cpi,
                       ogg_uint32_t *InterError, ogg_uint32_t *IntraError) {

  ogg_uint32_t  SB, MB, B; 
  ogg_uint32_t  MBIntraError;           /* Intra error for macro block */
  ogg_uint32_t  MBGFError;              /* Golden frame macro block error */
  ogg_uint32_t  MBGF_MVError;           /* Golden frame plus MV error */
  ogg_uint32_t  LastMBGF_MVError;       /* Golden frame error with
                                           last used GF motion
                                           vector. */
  ogg_uint32_t  MBInterError;           /* Inter no MV macro block error */
  ogg_uint32_t  MBLastInterError;       /* Inter with last used MV */
  ogg_uint32_t  MBPriorLastInterError;  /* Inter with prior last MV */
  ogg_uint32_t  MBInterMVError;         /* Inter MV macro block error */
  ogg_uint32_t  MBInterMVExError;       /* Inter MV (exhaustive
                                           search) macro block error */
  ogg_uint32_t  MBInterFOURMVError;     /* Inter MV error when using 4
                                           motion vectors per macro
                                           block */
  ogg_uint32_t  BestError;              /* Best error so far. */

  mv_t          FourMVect[6] = {{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}};
  mv_t          LastInterMVect = {0,0};
  mv_t          PriorLastInterMVect = {0,0};
  mv_t          TmpMVect = {0,0};  
  mv_t          LastGFMVect = {0,0};
  mv_t          InterMVect = {0,0};
  mv_t          InterMVectEx = {0,0};
  mv_t          GFMVect = {0,0};
  mv_t          ZeroVect = {0,0};

  int           MBCodedFlag;
  unsigned char QIndex = cpi->BaseQ; // temporary

  /* initialize error scores */
  *InterError = 0;
  *IntraError = 0;
  cpi->MVBits_0 = 0;
  cpi->MVBits_1 = 0;
  
  /* change the quatization matrix to the one at best Q to compute the
     new error score */
  cpi->MinImprovementForNewMV = (MvThreshTable[QIndex] << 12);
  cpi->InterTripOutThresh = (5000<<12);
  cpi->MVChangeFactor = MVChangeFactorTable[QIndex]; /* 0.9 */

  if ( cpi->info.quick_p ) {
    cpi->ExhaustiveSearchThresh = (1000<<12);
    cpi->FourMVThreshold = (2500<<12);
  } else {
    cpi->ExhaustiveSearchThresh = (250<<12);
    cpi->FourMVThreshold = (500<<12);
  }
  cpi->MinImprovementForFourMV = cpi->MinImprovementForNewMV * 4;

  if(cpi->MinImprovementForFourMV < (40<<12))
    cpi->MinImprovementForFourMV = (40<<12);

  cpi->FourMvChangeFactor = 8; /* cpi->MVChangeFactor - 0.05;  */

  /* decide what block type and motion vectors to use on all of the frames */
  for ( SB=0 ; SB < cpi->super_n[0]; SB++ ){
    superblock_t *sp = &cpi->super[0][SB];
    /* Check its four Macro-Blocks */
    for ( MB=0; MB<4; MB++ ) {
      macroblock_t *mp = sp->m[MB];
      
      if(!mp) continue;
      
      /* Is the current macro block coded (in part or in whole) */
      MBCodedFlag = 0;
      for ( B=0; B<4; B++ ) {
	fragment_t *fp = mp->y[B];
	if ( fp && fp->coded ){
	  MBCodedFlag = 1;
	  break;
	}
      }
      
      /* This one isn't coded go to the next one */
      if(!MBCodedFlag) continue;
      
      /**************************************************************
       Find the block choice with the lowest error

       NOTE THAT if U or V is coded but no Y from a macro block then
       the mode will be CODE_INTER_NO_MV as this is the default
       state to which the mode data structure is initialised in
       encoder and decoder at the start of each frame. */

      BestError = HUGE_ERROR;
      
      
      /* Look at the intra coding error. */
      MBIntraError = GetMBIntraError( cpi, mp );
      BestError = (BestError > MBIntraError) ? MBIntraError : BestError;
      
      /* Get the golden frame error */
      MBGFError = GetMBInterError( cpi, cpi->frame, cpi->golden, 
				   mp, 0, 0 );
      BestError = (BestError > MBGFError) ? MBGFError : BestError;
      
      /* Calculate the 0,0 case. */
      MBInterError = GetMBInterError( cpi, cpi->frame,
				      cpi->lastrecon,
				      mp, 0, 0 );
      BestError = (BestError > MBInterError) ? MBInterError : BestError;
      
      /* Measure error for last MV */
      MBLastInterError =  GetMBInterError( cpi, cpi->frame,
					   cpi->lastrecon,
					   mp, LastInterMVect.x,
					   LastInterMVect.y );
      BestError = (BestError > MBLastInterError) ?
	MBLastInterError : BestError;
      
      /* Measure error for prior last MV */
      MBPriorLastInterError =  GetMBInterError( cpi, cpi->frame,
						cpi->lastrecon,
						mp, PriorLastInterMVect.x,
						PriorLastInterMVect.y );
      BestError = (BestError > MBPriorLastInterError) ?
	MBPriorLastInterError : BestError;

      /* Temporarily force usage of no motionvector blocks */
      MBInterMVError = HUGE_ERROR;
      InterMVect.x = 0;  /* Set 0,0 motion vector */
      InterMVect.y = 0;

      /* If the best error is above the required threshold search
	 for a new inter MV */
      if ( BestError > cpi->MinImprovementForNewMV && cpi->MotionCompensation) {
	/* Use a mix of heirachical and exhaustive searches for
	   quick mode. */
	if ( cpi->info.quick_p ) {
	  MBInterMVError = GetMBMVInterError( cpi, cpi->lastrecon,
					      mp,
					      cpi->MVPixelOffsetY,
					      &InterMVect );

	  /* If we still do not have a good match try an exhaustive
	     MBMV search */
	  if ( (MBInterMVError > cpi->ExhaustiveSearchThresh) &&
	       (BestError > cpi->ExhaustiveSearchThresh) ) {

	    MBInterMVExError =
	      GetMBMVExhaustiveSearch( cpi, cpi->lastrecon,
				       mp,
				       &InterMVectEx );

	    /* Is the Variance measure for the EX search
	       better... If so then use it. */
	    if ( MBInterMVExError < MBInterMVError ) {
	      MBInterMVError = MBInterMVExError;
	      InterMVect.x = InterMVectEx.x;
	      InterMVect.y = InterMVectEx.y;
	    }
	  }
	}else{
	  /* Use an exhaustive search */
	  MBInterMVError =
	    GetMBMVExhaustiveSearch( cpi, cpi->lastrecon,
				     mp,
				     &InterMVect );
	}


	/* Is the improvement, if any, good enough to justify a new MV */
	if ( (16 * MBInterMVError < (BestError * cpi->MVChangeFactor)) &&
	     ((MBInterMVError + cpi->MinImprovementForNewMV) < BestError) ){
	  BestError = MBInterMVError;
	}

      }

      /* If the best error is still above the required threshold
	 search for a golden frame MV */
      MBGF_MVError = HUGE_ERROR;
      GFMVect.x = 0; /* Set 0,0 motion vector */
      GFMVect.y = 0;
      if ( BestError > cpi->MinImprovementForNewMV && cpi->MotionCompensation) {
	/* Do an MV search in the golden reference frame */
	MBGF_MVError = GetMBMVInterError( cpi, cpi->golden,
					  mp,
					  cpi->MVPixelOffsetY, &GFMVect );

	/* Measure error for last GFMV */
	LastMBGF_MVError =  GetMBInterError( cpi, cpi->frame,
					     cpi->golden,
					     mp,
					     LastGFMVect.x,
					     LastGFMVect.y );

	/* Check against last GF motion vector and reset if the
	   search has thrown a worse result. */
	if ( LastMBGF_MVError < MBGF_MVError ) {
	  GFMVect.x = LastGFMVect.x;
	  GFMVect.y = LastGFMVect.y;
	  MBGF_MVError = LastMBGF_MVError;
	}else{
	  LastGFMVect.x = GFMVect.x;
	  LastGFMVect.y = GFMVect.y;
	}

	/* Is the improvement, if any, good enough to justify a new MV */
	if ( (16 * MBGF_MVError < (BestError * cpi->MVChangeFactor)) &&
	     ((MBGF_MVError + cpi->MinImprovementForNewMV) < BestError) ) {
	  BestError = MBGF_MVError;
	}
      }

      /* Finally... If the best error is still to high then consider
	 the 4MV mode */
      MBInterFOURMVError = HUGE_ERROR;
      if ( BestError > cpi->FourMVThreshold && cpi->MotionCompensation) {
	/* Get the 4MV error. */
	MBInterFOURMVError =
	  GetFOURMVExhaustiveSearch( cpi, cpi->lastrecon,
				     mp,
				     FourMVect );

	/* If the improvement is great enough then use the four MV mode */
	if ( ((MBInterFOURMVError + cpi->MinImprovementForFourMV) <
	      BestError) && (16 * MBInterFOURMVError <
			     (BestError * cpi->FourMvChangeFactor))) {
	  BestError = MBInterFOURMVError;
	}
      }

      /********************************************************
         end finding the best error
         *******************************************************

         Figure out what to do with the block we chose

         Over-ride and force intra if error high and Intra error similar
         Now choose a mode based on lowest error (with bias towards no MV) */

      if ( (BestError > cpi->InterTripOutThresh) &&
	   (10 * BestError > MBIntraError * 7 ) ) {
	SetMBMotionVectorsAndMode(mp,&ZeroVect,CODE_INTRA);
      } else if ( BestError == MBInterError ) {
	SetMBMotionVectorsAndMode(mp,&ZeroVect,CODE_INTER_NO_MV);
      } else if ( BestError == MBGFError ) {
	SetMBMotionVectorsAndMode(mp,&ZeroVect,CODE_USING_GOLDEN);
      } else if ( BestError == MBLastInterError ) {
	SetMBMotionVectorsAndMode(mp,&LastInterMVect,CODE_INTER_LAST_MV);
      } else if ( BestError == MBPriorLastInterError ) {
	SetMBMotionVectorsAndMode(mp,&PriorLastInterMVect,CODE_INTER_PRIOR_LAST);

	/* Swap the prior and last MV cases over */
	TmpMVect = PriorLastInterMVect;
	PriorLastInterMVect = LastInterMVect;
	LastInterMVect = TmpMVect;

      } else if ( BestError == MBInterMVError ) {

	SetMBMotionVectorsAndMode(mp,&InterMVect,CODE_INTER_PLUS_MV);

	/* Update Prior last mv with last mv */
	PriorLastInterMVect.x = LastInterMVect.x;
	PriorLastInterMVect.y = LastInterMVect.y;

	/* Note last inter MV for future use */
	LastInterMVect.x = InterMVect.x;
	LastInterMVect.y = InterMVect.y;

	CountMotionVector( cpi, &InterMVect);

      } else if ( BestError == MBGF_MVError ) {

	SetMBMotionVectorsAndMode(mp,&GFMVect,CODE_GOLDEN_MV);

	/* Note last inter GF MV for future use */
	LastGFMVect.x = GFMVect.x;
	LastGFMVect.y = GFMVect.y;

	CountMotionVector( cpi, &GFMVect);
      } else if ( BestError == MBInterFOURMVError ) {

	/* Calculate the UV vectors as the average of the Y plane ones. */
	/* First .x component */
	FourMVect[4].x = FourMVect[0].x + FourMVect[1].x +
	  FourMVect[2].x + FourMVect[3].x;
	if ( FourMVect[4].x >= 0 )
	  FourMVect[4].x = (FourMVect[4].x + 2) / 4;
	else
	  FourMVect[4].x = (FourMVect[4].x - 2) / 4;
	FourMVect[5].x = FourMVect[4].x;

	/* Then .y component */
	FourMVect[4].y = FourMVect[0].y + FourMVect[1].y +
	  FourMVect[2].y + FourMVect[3].y;
	if ( FourMVect[4].y >= 0 )
	  FourMVect[4].y = (FourMVect[4].y + 2) / 4;
	else
	  FourMVect[4].y = (FourMVect[4].y - 2) / 4;
	FourMVect[5].y = FourMVect[4].y;

	SetFragMotionVectorAndMode(mp->y[0], &FourMVect[0],CODE_INTER_FOURMV);
	SetFragMotionVectorAndMode(mp->y[1], &FourMVect[1],CODE_INTER_FOURMV);
	SetFragMotionVectorAndMode(mp->y[2], &FourMVect[2],CODE_INTER_FOURMV);
	SetFragMotionVectorAndMode(mp->y[3], &FourMVect[3],CODE_INTER_FOURMV);
	SetFragMotionVectorAndMode(mp->u, &FourMVect[4],CODE_INTER_FOURMV);
	SetFragMotionVectorAndMode(mp->v, &FourMVect[5],CODE_INTER_FOURMV);

	/* Note the four MVs values for current macro-block. */
	CountMotionVector( cpi, &FourMVect[0]);
	CountMotionVector( cpi, &FourMVect[1]);
	CountMotionVector( cpi, &FourMVect[2]);
	CountMotionVector( cpi, &FourMVect[3]);

	/* Update Prior last mv with last mv */
	PriorLastInterMVect = LastInterMVect;

	/* Note last inter MV for future use */
	LastInterMVect = FourMVect[3];

      } else {

	SetMBMotionVectorsAndMode(mp,&ZeroVect,CODE_INTRA);

      }

      /* setting up mode specific block types
         *******************************************************/

      *InterError += (BestError>>8);
      *IntraError += (MBIntraError>>8);

    }
  }

  /* Return number of pixels coded */
  return 0;
}

void WriteFrameHeader( CP_INSTANCE *cpi) {
  oggpack_buffer *opb=cpi->oggbuffer;

  TH_DEBUG("\n>>>> beginning frame %ld\n\n",dframe);

  /* Output the frame type (base/key frame or inter frame) */
  oggpackB_write( opb, cpi->FrameType, 1 );
  TH_DEBUG("frame type = video, %s\n",cpi->pb.FrameType?"predicted":"key");
  
  /* Write out details of the current value of Q... variable resolution. */
  oggpackB_write( opb, cpi->BaseQ, 6 ); // temporary

  /* we only support one Q index per frame */
  oggpackB_write( opb, 0, 1 );

  /* If the frame was a base frame then write out the frame dimensions. */
  if ( cpi->FrameType == KEY_FRAME ) {
    /* all bits reserved! */
    oggpackB_write( opb, 0, 3 );
  }
}

