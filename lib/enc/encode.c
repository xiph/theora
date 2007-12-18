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
  coding_mode_t *mp = cpi->frag_mode[0];
  unsigned char *cp = cpi->frag_coded[0];
  int fi = 0;

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
      for ( n = 0 ; n < cpi->frag_h[plane] ; n++, fp++, fi++) {
        fp->pred_dc = fp->dct[0];

        /* only do 2 prediction if fragment coded and on non intra or
           if all fragments are intra */
        if(cp[fi]) {
          /* Type of Fragment */

          WhichFrame = Mode2Frame[mp[fi]];

          /* Check Borderline Cases */
          WhichCase = (n==0) + ((m==0) << 1) + ((n+1 == cpi->frag_h[plane]) << 2);

          /* fragment valid for prediction use if coded and it comes
             from same frame as the one we are predicting */
          for(k=pcount=wpc=0; k<4; k++) {
            int pflag = 1<<k;
            if((bc_mask[WhichCase]&pflag)){
	      fragment_t *fnp=fp - fn[k];	
	      int fni = fi - fn[k];	
	      if(cp[fni] &&
		 (Mode2Frame[mp[fni]] == WhichFrame)){
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
  coding_mode_t *mp = cpi->frag_mode[0];
  unsigned char *cp = cpi->frag_coded[0];

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
	int fi = sp->f[frag];
	fragment_t *fp = &cpi->frag[0][fi];

	if ( cp[fi] ) {

	  /* transform and quantize block */
	  TransformQuantizeBlock( cpi, fi );
	  
	  /* Has the block got struck off (no MV and no data
	     generated after DCT) If not then mark it and the
	     assosciated MB as coded. */
	  if ( cp[fi] ) {

	    if(cpi->coded_head){
	      cpi->coded_head->next = fp;
	      cpi->coded_head = fp;
	    }else{
	      cpi->coded_head = cpi->coded_tail = fp;
	    }

	    /* MB is still coded */
	    coded = 1;
	    mode = mp[fi];
	    
	  }
	}
      }
     
      /* If the MB is marked as coded and we are in the Y plane */
      /* collect coding metric */
      if ( coded && plane == 0 ) cpi->ModeCount[mode] ++;

    }  
  }
  return 0;
}

static void EncodeDcTokenList (CP_INSTANCE *cpi) {
  int i,plane;
  int best;
  int huff[2];
  oggpack_buffer *opb=cpi->oggbuffer;

  /* Work out which table options are best for DC */
  for(plane = 0; plane<2; plane++){
    best = cpi->dc_bits[plane][0];
    huff[plane] = DC_HUFF_OFFSET;
    for ( i = 1; i < DC_HUFF_CHOICES; i++ ) {
      if ( cpi->dc_bits[plane][i] < best ) {
	best = cpi->dc_bits[plane][i];
	huff[plane] = i + DC_HUFF_OFFSET;
      }
    }
    oggpackB_write( opb, huff[plane] - DC_HUFF_OFFSET, DC_HUFF_CHOICE_BITS );
  }
  
  /* Encode the token list */
  for ( i = 0; i < cpi->dct_token_count[0]; i++){
    int token = cpi->dct_token[0][i];
    int eb = cpi->dct_token_eb[0][i];
    int plane = (i >= cpi->dct_token_ycount[0]);

    oggpackB_write( opb, cpi->HuffCodeArray_VP3x[huff[plane]][token],
                    cpi->HuffCodeLengthArray_VP3x[huff[plane]][token] );
    
    if ( cpi->ExtraBitLengths_VP3x[token] > 0 ) 
      oggpackB_write( opb, eb, cpi->ExtraBitLengths_VP3x[token] );
  }
}

static void EncodeAcGroup(CP_INSTANCE *cpi, int group, int huff_offset, int *huffchoice){
  int i;
  oggpack_buffer *opb=cpi->oggbuffer;
  int c = 0;
  int y = cpi->dct_token_ycount[group];
  
  for(i=0; i<cpi->dct_token_count[group]; i++){
    int token = cpi->dct_token[group][i];
    int eb = cpi->dct_token_eb[group][i];
    int plane = (c >= y);

    int hi = huff_offset + huffchoice[plane];

    oggpackB_write( opb, cpi->HuffCodeArray_VP3x[hi][token],
		    cpi->HuffCodeLengthArray_VP3x[hi][token] );
    
    if (cpi->ExtraBitLengths_VP3x[token] > 0) 
      oggpackB_write( opb, eb,cpi->ExtraBitLengths_VP3x[token] );
    
    c++;
  }
}

static void EncodeAcTokenList (CP_INSTANCE *cpi) {
  int i,plane;
  int best;
  int huff[2];
  oggpack_buffer *opb=cpi->oggbuffer;

  /* Work out which table options are best for AC */
  for(plane = 0; plane<2; plane++){
    best = cpi->ac_bits[plane][0];
    huff[plane] = AC_HUFF_OFFSET;
    for ( i = 1; i < AC_HUFF_CHOICES; i++ ) {
      if ( cpi->ac_bits[plane][i] < best ){
	best = cpi->ac_bits[plane][i];
	huff[plane] = i + AC_HUFF_OFFSET;
      }
    }
    oggpackB_write( opb, huff[plane] - AC_HUFF_OFFSET, AC_HUFF_CHOICE_BITS );
  }
  
  /* encode dct tokens, group 1 through group 63 in the four AC ranges */
  for(i=1;i<=AC_TABLE_2_THRESH;i++)
    EncodeAcGroup(cpi, i, 0, huff);

  for(;i<=AC_TABLE_3_THRESH;i++)
    EncodeAcGroup(cpi, i, AC_HUFF_CHOICES, huff);

  for(;i<=AC_TABLE_4_THRESH;i++)
    EncodeAcGroup(cpi, i, AC_HUFF_CHOICES*2, huff);

  for(;i<BLOCK_SIZE;i++)
    EncodeAcGroup(cpi, i, AC_HUFF_CHOICES*3, huff);

}

static const ogg_uint32_t NoOpModeWords[8] = {0,1,2,3,4,5,6,7};
static const ogg_int32_t NoOpModeBits[8] = {3,3,3,3,3,3,3,3};
static const unsigned char NoOpScheme[8] = {0,1,2,3,4,5,6,7};

static void PackModes (CP_INSTANCE *cpi) {
  ogg_uint32_t    i,j;

  unsigned char   Mode0[MAX_MODES];
  ogg_int32_t     TmpFreq = -1;
  ogg_int32_t     TmpIndex = -1;

  ogg_uint32_t    BestScheme;
  ogg_uint32_t    BestSchemeScore;
  ogg_uint32_t    modes=0;

  const ogg_uint32_t *ModeWords;
  const ogg_int32_t *ModeBits;
  const unsigned char  *ModeScheme;

  unsigned char *cp = cpi->frag_coded[0];
  coding_mode_t *mp = cpi->frag_mode[0];
  int SB,MB,B;

  oggpack_buffer *opb=cpi->oggbuffer;
  for(i=0;i<MAX_MODES;i++)
    modes+=cpi->ModeCount[i];

  /* Default/ fallback scheme uses MODE_BITS bits per mode entry */
  BestScheme = (MODE_METHODS - 1);
  BestSchemeScore = modes * 3;
  /* Get a bit score for the available predefined schemes. */
  for (  j = 1; j < (MODE_METHODS - 1); j++ ) {
    int SchemeScore = 0;
    const unsigned char *SchemeList = ModeSchemes[j-1];

    /* Find the total bits to code using each avaialable scheme */
    for ( i = 0; i < MAX_MODES; i++ )
      SchemeScore += ModeBitLengths[SchemeList[i]] * cpi->ModeCount[i];

    /* Is this the best scheme so far ??? */
    if ( SchemeScore < BestSchemeScore ) {
      BestSchemeScore = SchemeScore;
      BestScheme = j;
    }
  }

  /* Order the modes from most to least frequent.  Store result as
     scheme 0 */
  {
    int SchemeScore = 24;
    
    for ( j = 0; j < MAX_MODES; j++ ) {
      TmpFreq = -1;  /* need to re-initialize for each loop */
      /* Find the most frequent */
      for ( i = 0; i < MAX_MODES; i++ ) {
	/* Is this the best scheme so far ??? */
	if ( cpi->ModeCount[i] > TmpFreq ) {
	  TmpFreq = cpi->ModeCount[i];
	  TmpIndex = i;
	}
      }
      SchemeScore += ModeBitLengths[j] * cpi->ModeCount[TmpIndex];
      cpi->ModeCount[TmpIndex] = -1;
      Mode0[TmpIndex] = j;
    }

    if ( SchemeScore < BestSchemeScore ) {
      BestSchemeScore = SchemeScore;
      BestScheme = 0;
    }
  }

  /* Encode the best scheme. */
  oggpackB_write( opb, BestScheme, (ogg_uint32_t)MODE_METHOD_BITS );

  /* If the chosen scheme is scheme 0 send details of the mode
     frequency order */
  if ( BestScheme == 0 ) {
    for ( j = 0; j < MAX_MODES; j++ ){
      /* Note that the last two entries are implicit */
      oggpackB_write( opb, Mode0[j], (ogg_uint32_t)MODE_BITS );
    }
    ModeScheme = Mode0;
    ModeWords = ModeBitPatterns;
    ModeBits = ModeBitLengths;
  }
  else if ( BestScheme < (MODE_METHODS - 1)) {
    ModeScheme = ModeSchemes[BestScheme-1];
    ModeWords = ModeBitPatterns;
    ModeBits = ModeBitLengths;
  }else{
    ModeScheme = NoOpScheme;
    ModeWords = NoOpModeWords;
    ModeBits = NoOpModeBits;
  }

  for ( SB=0 ; SB < cpi->super_n[0]; SB++ ){
    superblock_t *sp = &cpi->super[0][SB];
    for ( MB=0; MB<4; MB++ ) {
      macroblock_t *mbp = &cpi->macro[sp->m[MB]];
      int fi = mbp->y[0];

      for(B=1; !cp[fi] && B<4; B++ ) fi = mbp->y[B];
  
      if(cp[fi]){
	/* Add the appropriate mode entropy token. */
	int index = ModeScheme[mp[fi]];
	oggpackB_write( opb, ModeWords[index],
			(ogg_uint32_t)ModeBits[index] );
      }
    }
  }
}

static void PackMotionVectors (CP_INSTANCE *cpi) {
  const ogg_uint32_t * MvPatternPtr;
  const ogg_uint32_t * MvBitsPtr;

  ogg_uint32_t SB, MB, B;
  coding_mode_t *mp = cpi->frag_mode[0];
  unsigned char *cp = cpi->frag_coded[0];

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
      macroblock_t *mbp = &cpi->macro[sp->m[MB]];
      int fi = mbp->y[0];
 
      for(B=1; !cp[fi] && B<4; B++ ) fi = mbp->y[B];
      if(B==4) continue;

      if(mp[fi]==CODE_INTER_PLUS_MV || mp[fi]==CODE_GOLDEN_MV){
	fragment_t *fp = &cpi->frag[0][fi];

	/* One MV for the macroblock */
	oggpackB_write( opb, MvPatternPtr[fp->mv.x], MvBitsPtr[fp->mv.x] );
	oggpackB_write( opb, MvPatternPtr[fp->mv.y], MvBitsPtr[fp->mv.y] );
      }else if (mp[fi] == CODE_INTER_FOURMV){
	/* MV for each codedblock */
	for(B=0; B<4; B++ ){
	  fi = mbp->y[B];
	  if(cp[fi]){
	    fragment_t *fp = &cpi->frag[0][fi];
	    oggpackB_write( opb, MvPatternPtr[fp->mv.x], MvBitsPtr[fp->mv.x] );
	    oggpackB_write( opb, MvPatternPtr[fp->mv.y], MvBitsPtr[fp->mv.y] );
	  }
	}
      }
    }
  }
}

void EncodeData(CP_INSTANCE *cpi){

  /* reset all coding metadata  */
  memset(cpi->ModeCount, 0, MAX_MODES*sizeof(*cpi->ModeCount));
  cpi->coded_head = cpi->coded_tail = NULL;

  dsp_save_fpu (cpi->dsp);

  /* Encode and tokenise the Y, U and V components */
  CodePlane(cpi, 0);
  CodePlane(cpi, 1);
  CodePlane(cpi, 2);

  if(cpi->coded_head)
    cpi->coded_head->next = NULL; /* cap off the end of the coded block list */
  
  PredictDC(cpi);
  DPCMTokenize(cpi);

  /* The tree is not needed (implicit) for key frames */
  if ( cpi->FrameType != KEY_FRAME )
    PackAndWriteDFArray( cpi );

  /* Mode and MV data not needed for key frames. */
  if ( cpi->FrameType != KEY_FRAME ){
    PackModes(cpi);
    PackMotionVectors (cpi);
  }

  EncodeDcTokenList(cpi);
  EncodeAcTokenList(cpi);

  ReconRefFrames(cpi);

  dsp_restore_fpu (cpi->dsp);
}

ogg_uint32_t PickIntra( CP_INSTANCE *cpi ){

  int i;
  for(i=0;i<cpi->frag_total;i++){
    cpi->frag_mode[0][i] = CODE_INTRA;
    cpi->frag_coded[0][i] = 1;
  }
  return 0;
}

static void CountMotionVector(CP_INSTANCE *cpi, mv_t *mv) {
  cpi->MVBits_0 += MvBits[mv->x];
  cpi->MVBits_0 += MvBits[mv->y];
  cpi->MVBits_1 += 12; /* Simple six bits per mv component fallback */
}

static void SetFragMotionVectorAndMode(CP_INSTANCE *cpi,
				       int fi,
				       mv_t *mv,
				       int mode){
  fragment_t *fp = &cpi->frag[0][fi];
  fp->mv = *mv;
  cpi->frag_mode[0][fi] = mode;
}

static void SetMBMotionVectorsAndMode(CP_INSTANCE *cpi,
				      macroblock_t *mp,
				      mv_t *mv,
				      int mode){
  SetFragMotionVectorAndMode(cpi, mp->y[0], mv, mode);
  SetFragMotionVectorAndMode(cpi, mp->y[1], mv, mode);
  SetFragMotionVectorAndMode(cpi, mp->y[2], mv, mode);
  SetFragMotionVectorAndMode(cpi, mp->y[3], mv, mode);
  SetFragMotionVectorAndMode(cpi, mp->u, mv, mode);
  SetFragMotionVectorAndMode(cpi, mp->v, mv, mode);
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

  unsigned char QIndex = cpi->BaseQ; // temporary

  unsigned char *cp = cpi->frag_coded[0];

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
      macroblock_t *mbp = &cpi->macro[sp->m[MB]];      
      int fi = mbp->y[0];

      for ( B=1; !cp[fi] && B<4; B++ ) fi = mbp->y[B];
      
      /* This one isn't coded go to the next one */
      if(!cp[fi]) continue;
      
      /**************************************************************
       Find the block choice with the lowest error

       NOTE THAT if U or V is coded but no Y from a macro block then
       the mode will be CODE_INTER_NO_MV as this is the default
       state to which the mode data structure is initialised in
       encoder and decoder at the start of each frame. */

      BestError = HUGE_ERROR;
      
      
      /* Look at the intra coding error. */
      MBIntraError = GetMBIntraError( cpi, mbp );
      BestError = (BestError > MBIntraError) ? MBIntraError : BestError;
      
      /* Get the golden frame error */
      MBGFError = GetMBInterError( cpi, cpi->frame, cpi->golden, 
				   mbp, 0, 0 );
      BestError = (BestError > MBGFError) ? MBGFError : BestError;
      
      /* Calculate the 0,0 case. */
      MBInterError = GetMBInterError( cpi, cpi->frame,
				      cpi->lastrecon,
				      mbp, 0, 0 );
      BestError = (BestError > MBInterError) ? MBInterError : BestError;
      
      /* Measure error for last MV */
      MBLastInterError =  GetMBInterError( cpi, cpi->frame,
					   cpi->lastrecon,
					   mbp, LastInterMVect.x,
					   LastInterMVect.y );
      BestError = (BestError > MBLastInterError) ?
	MBLastInterError : BestError;
      
      /* Measure error for prior last MV */
      MBPriorLastInterError =  GetMBInterError( cpi, cpi->frame,
						cpi->lastrecon,
						mbp, PriorLastInterMVect.x,
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
					      mbp,
					      cpi->MVPixelOffsetY,
					      &InterMVect );

	  /* If we still do not have a good match try an exhaustive
	     MBMV search */
	  if ( (MBInterMVError > cpi->ExhaustiveSearchThresh) &&
	       (BestError > cpi->ExhaustiveSearchThresh) ) {

	    MBInterMVExError =
	      GetMBMVExhaustiveSearch( cpi, cpi->lastrecon,
				       mbp,
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
				     mbp,
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
					  mbp,
					  cpi->MVPixelOffsetY, &GFMVect );

	/* Measure error for last GFMV */
	LastMBGF_MVError =  GetMBInterError( cpi, cpi->frame,
					     cpi->golden,
					     mbp,
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
				     mbp,
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
	SetMBMotionVectorsAndMode(cpi,mbp,&ZeroVect,CODE_INTRA);
      } else if ( BestError == MBInterError ) {
	SetMBMotionVectorsAndMode(cpi,mbp,&ZeroVect,CODE_INTER_NO_MV);
      } else if ( BestError == MBGFError ) {
	SetMBMotionVectorsAndMode(cpi,mbp,&ZeroVect,CODE_USING_GOLDEN);
      } else if ( BestError == MBLastInterError ) {
	SetMBMotionVectorsAndMode(cpi,mbp,&LastInterMVect,CODE_INTER_LAST_MV);
      } else if ( BestError == MBPriorLastInterError ) {
	SetMBMotionVectorsAndMode(cpi,mbp,&PriorLastInterMVect,CODE_INTER_PRIOR_LAST);

	/* Swap the prior and last MV cases over */
	TmpMVect = PriorLastInterMVect;
	PriorLastInterMVect = LastInterMVect;
	LastInterMVect = TmpMVect;

      } else if ( BestError == MBInterMVError ) {

	SetMBMotionVectorsAndMode(cpi,mbp,&InterMVect,CODE_INTER_PLUS_MV);

	/* Update Prior last mv with last mv */
	PriorLastInterMVect.x = LastInterMVect.x;
	PriorLastInterMVect.y = LastInterMVect.y;

	/* Note last inter MV for future use */
	LastInterMVect.x = InterMVect.x;
	LastInterMVect.y = InterMVect.y;

	CountMotionVector( cpi, &InterMVect);

      } else if ( BestError == MBGF_MVError ) {

	SetMBMotionVectorsAndMode(cpi,mbp,&GFMVect,CODE_GOLDEN_MV);

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

	{
	  fragment_t *f0 = cpi->frag[0];
	  SetFragMotionVectorAndMode(cpi,mbp->y[0], &FourMVect[0],CODE_INTER_FOURMV);
	  SetFragMotionVectorAndMode(cpi,mbp->y[1], &FourMVect[1],CODE_INTER_FOURMV);
	  SetFragMotionVectorAndMode(cpi,mbp->y[2], &FourMVect[2],CODE_INTER_FOURMV);
	  SetFragMotionVectorAndMode(cpi,mbp->y[3], &FourMVect[3],CODE_INTER_FOURMV);
	  SetFragMotionVectorAndMode(cpi,mbp->u, &FourMVect[4],CODE_INTER_FOURMV);
	  SetFragMotionVectorAndMode(cpi,mbp->v, &FourMVect[5],CODE_INTER_FOURMV);
	}

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

	SetMBMotionVectorsAndMode(cpi, mbp,&ZeroVect,CODE_INTRA);

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

  /* Output the frame type (base/key frame or inter frame) */
  oggpackB_write( opb, cpi->FrameType, 1 );
  
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

