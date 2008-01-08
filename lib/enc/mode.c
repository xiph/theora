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

  function: mode selection code
  last mod: $Id$

 ********************************************************************/

#include "codec_internal.h"
#include "mode_select.h"

/* Mode decision is done by exhaustively examining all potential
   choices.  Since we use a minimum-quality encoding strategy, this
   amounts to simply selecting the mode which uses the smallest number
   of bits, since the minimum quality will be met in any mode.
   Obviously, doing the motion compensation, fDCT, tokenization, and
   then counting the bits each token uses is computationally
   expensive.  Theora's EOB runs can also split the cost of these
   tokens across multiple fragments, and naturally we don't know what
   the optimal choice of Huffman codes will be until we know all the
   tokens we're going to encode in all the fragments.

   So we use a simple approach to estimating the bit cost of each mode
   based upon the SAD value of the residual.  The mathematics behind
   the technique are outlined by Kim \cite{Kim03}, but the process is
   very simple.  For each quality index and SAD value, we have a table
   containing the average number of bits needed to code a fragment.
   The SAD values are placed into a small number of bins (currently
   16).  The bit counts are obtained by examining actual encoded
   frames, with optimal Huffman codes selected and EOB bits
   appropriately divided among all the blocks they involve.  A
   separate QIxSAD table is kept for each mode and color plane.  It
   may be possible to combine many of these, but only experimentation
   will tell which ones truly represent the same distribution.

   @ARTICLE{Kim03,
     author="Hyun Mun Kim",
     title="Adaptive Rate Control Using Nonlinear Regression",
     journal="IEEE Transactions on Circuits and Systems for Video
     Technology",
     volume=13,
     number=5,
     pages="432--439",
     month="May",
     year=2003
   }

*/

/* Initialize the mode scheme chooser.
   
   Schemes 0-6 use a highly unbalanced Huffman code to code each of
   the modes.  The same set of Huffman codes is used for each of these
   7 schemes, but the mode assigned to each code varies.

   Schemes 1-6 have a fixed mapping from Huffman code to MB mode,
   while scheme 0 writes a custom mapping to the bitstream before all
   the modes.  Finally, scheme 7 just encodes each mode directly in 3
   bits. 

*/

void oc_mode_scheme_chooser_init(CP_INSTANCE *cpi){
  oc_mode_scheme_chooser *chooser = cpi->mode_scheme_chooser;
  int i;

  for(i=0;i<7;i++)
    chooser->mode_bits[msi] = ModeBitLengths;
  chooser->mode_bits[7] = ModeBitLengthsD[i];
  
  chooser->mode_ranks[0] = chooser->scheme0_ranks;
  for(i=1;i<8;i++)
    chooser->mode_ranks[msi] = ModeSchemes[i];

  memset(chooser->mode_counts,0,sizeof(chooser->mode_counts));
  
  /*Scheme 0 starts with 24 bits to store the mode list in.*/
  chooser->scheme_bits[0] = 24;
  memset(chooser->scheme_bits+1,0,7*sizeof(chooser->scheme_bits[1]));
  for(i=0;i<8;i++){
    /*Scheme 7 should always start first, and scheme 0 should always start
       last.*/
    chooser->scheme_list[i] = 7-i;
    chooser->scheme0_list[i] = chooser->scheme0_ranks[i]=i;
  }
}

/* This is the real purpose of this data structure: not actually
   selecting a mode scheme, but estimating the cost of coding a given
   mode given all the modes selected so far.

   This is done via opportunity cost: the cost is defined as the number of bits
   required to encode all the modes selected so far including the current one
   using the best possible scheme, minus the number of bits required to encode
   all the modes selected so far not including the current one using the best
   possible scheme.
  
   The computational expense of doing this probably makes it overkill.
   Just be happy we take a greedy approach instead of trying to solve the
   global mode-selection problem (which is NP-hard).
   _mode: The mode to determine the cost of.
   Return: The number of bits required to code this mode.*/

int oc_mode_cost(CP_INSTANCE *cpi,
		 int _mode){

  oc_mode_scheme_chooser *chooser = cpi->mode_scheme_chooser;
  int scheme0 = chooser->scheme_list[0];
  int scheme1 = chooser->scheme_list[1];
  int best_bits = chooser->scheme_bits[scheme0];
  int mode_bits = chooser->mode_bits[scheme0][chooser->mode_ranks[scheme0][_mode]];
  int si;
  int scheme_bits;

  /*Typical case: If the difference between the best scheme and the
     next best is greater than 6 bits, then adding just one mode
     cannot change which scheme we use.*/

  if(chooser->scheme_bits[scheme1]-best_bits > 6) return mode_bits;

  /*Otherwise, check to see if adding this mode selects a different scheme as the best.*/
  si = 1;
  best_bits += mode_bits;

  do{
    /* For any scheme except 0, we can just use the bit cost of the mode's rank in that scheme.*/
    if(scheme1!=0){

      scheme_bits = chooser->scheme_bits[scheme1]+
	chooser->mode_bits[scheme1][chooser->mode_ranks[scheme1][_mode]];

    }else{
      int ri;

      /* For scheme 0, incrementing the mode count could potentially
         change the mode's rank.

        Find the index where the mode would be moved to in the optimal
        list, and use its bit cost instead of the one for the mode's
        current position in the list. */

      /* don't recompute scheme bits; this is computing opportunity
	 cost, not an update. */

      for(ri = chooser->scheme0_ranks[_mode] ; ri>0 &&
	    chooser->mode_counts[_mode]>=
	    chooser->mode_counts[chooser->scheme0_list[ri-1]] ; ri--);

      scheme_bits = chooser->scheme_bits[0] + ModeBitLengths[ri];
    }

    if(scheme_bits<best_bits) best_bits = scheme_bits;
    if(++si>=8) break;
    scheme1 = chooser->scheme_list[si];
  } while(chooser->scheme_bits[scheme1] - chooser->scheme_bits[scheme0] <= 6);

  return best_bits - chooser->scheme_bits[scheme0];
}

/* Incrementally update the mode counts and per-scheme bit counts and re-order the scheme
   lists once a mode has been selected.

  _mode: The mode that was chosen.*/

static void oc_mode_set( CP_INSTANCE *cpi,
			 macroblock_t *mb,
			 int _mode){

  oc_mode_scheme_chooser *chooser = cpi->mode_scheme_chooser;
  int ri;
  int si;

  mb->mode = _mode;
  chooser->mode_counts[_mode]++;

  /* Re-order the scheme0 mode list if necessary. */
  for(ri = chooser->scheme0_ranks[_mode]; ri>0; ri--){
    int pmode;
    pmode=chooser->scheme0_list[ri-1];
    if(chooser->mode_counts[pmode] >= chooser->mode_counts[_mode])break;

    /* reorder the mode ranking */
    chooser->scheme0_ranks[pmode]++;
    chooser->scheme0_list[ri]=pmode;

  }
  chooser->scheme0_ranks[_mode]=ri;
  chooser->scheme0_list[ri]=_mode;

  /*Now add the bit cost for the mode to each scheme.*/
  for(si=0;si<8;si++){
    chooser->scheme_bits[si]+=
      chooser->mode_bits[si][chooser->mode_ranks[si][_mode]];
  }

  /* Finally, re-order the list of schemes. */
  for(si=1;si<8;si++){
    int sj = si;
    int scheme0 = chooser->scheme_list[si];
    int bits0 = chooser->scheme_bits[scheme0];
    do{
      int scheme1 = chooser->scheme_list[sj-1];
      if(bits0 >= chooser->scheme_bits[scheme1]) break;
      chooser->scheme_list[sj] = scheme1;
    } while(--sj>0);
    chooser->scheme_list[sj]=scheme0;
  }
}

void oc_mode_unset(CP_INSTANCE *cpi, 
		   macroblock_t *mb){

  oc_mode_scheme_chooser *chooser = cpi->mode_scheme_chooser;
  int ri;
  int si;
  int mode = mb->mode;

  chooser->mode_counts[mode]--;

  /* Re-order the scheme0 mode list if necessary. */
  for(ri = chooser->scheme0_ranks[mode]; ri<7; ri++){
    int pmode;
    pmode=chooser->scheme0_list[ri+1];
    if(chooser->mode_counts[pmode] <= chooser->mode_counts[mode])break;

    /* reorder the mode ranking */
    chooser->scheme0_ranks[pmode]--;
    chooser->scheme0_list[ri]=pmode;

  }
  chooser->scheme0_ranks[mode]=ri;
  chooser->scheme0_list[ri]=mode;

  /* Now remove the bit cost of the mode from each scheme.*/
  for(si=0;si<8;si++){
    chooser->scheme_bits[si]-=
      chooser->mode_bits[si][chooser->mode_ranks[si][mode]];
  }

  /* Finally, re-order the list of schemes. */
  for(si=1;si<8;si++){
    int sj = si;
    int scheme0 = chooser->scheme_list[si];
    int bits0 = chooser->scheme_bits[scheme0];
    do{
      int scheme1 = chooser->scheme_list[sj-1];
      if(bits0 >= chooser->scheme_bits[scheme1]) break;
      chooser->scheme_list[sj] = scheme1;
    } while(--sj>0);
    chooser->scheme_list[sj]=scheme0;
  }
}

static void SetMBMotionVectorsAndMode(CP_INSTANCE *cpi, 
				      macroblock_t *mp,
				      mv_t *mv,
				      int mode){
  oc_mode_set(cpi,mp,mode);
  mp->mv[0] = *mv;
  mp->mv[1] = *mv;
  mp->mv[2] = *mv;
  mp->mv[3] = *mv;
}

static int BIntraSAD420(CP_INSTANCE *cpi, int fi){
  int sad = 0;
  unsigned char *y = frame + cpi->frag_buffer_index[fi];
  ogg_uint32_t acc = 0;
  int j;
  for(j=0;j<8;j++){
    acc += y[0]; 
    acc += y[1]; 
    acc += y[2]; 
    acc += y[3]; 
    acc += y[4]; 
    acc += y[5]; 
    acc += y[6]; 
    acc += y[7]; 
    y += cpi->stride[0];
  }
  
  y = frame + cpi->frag_buffer_index[fi];
  for(j=0;j<8;j++){
    sad += abs ((y[0]<<6)-acc); 
    sad += abs ((y[1]<<6)-acc); 
    sad += abs ((y[2]<<6)-acc); 
    sad += abs ((y[3]<<6)-acc); 
    sad += abs ((y[4]<<6)-acc); 
    sad += abs ((y[5]<<6)-acc); 
    sad += abs ((y[6]<<6)-acc); 
    sad += abs ((y[7]<<6)-acc); 
    y += cpi->stride[0];
  }

  return sad;
}

/* equivalent to adding up the abs values of the AC components of a block */
static int MBIntraSAD420(CP_INSTANCE *cpi, int mbi){
  macroblock_t *mb = &cpi->macro[mbi];
  unsigned char *cp = cpi->frag_coded;
  int sad = 0;

  int fi = mb->yuv[0][0];
  if(cp[fi]) sad += BIntraSAD420(cpi,fi);

  fi = mb->yuv[0][1];
  if(cp[fi]) sad += BIntraSAD420(cpi,fi);

  fi = mb->yuv[0][2];
  if(cp[fi]) sad += BIntraSAD420(cpi,fi);

  fi = mb->yuv[0][3];
  if(cp[fi]) sad += BIntraSAD420(cpi,fi);

  fi = mb->yuv[1][0];
  if(cp[fi]) sad += BIntraSAD420(cpi,fi);

  fi = mb->yuv[2][0];
  if(cp[fi]) sad += BIntraSAD420(cpi,fi);

  return sad;
}

static int InterSad(CP_INSTANCE *cpi, int mbi, mv_t mv){
  macroblock_t *mb = &cpi->macro[mbi];
  unsigned char *cp = cpi->frag_coded;
  int sad = 0;
  int i;
  for(i=0;i<4;i++){
    int fi = mb->y[i];
    if(cp[fi]){
      unsigned char *y = frame + cpi->frag_buffer_index[fi];
      ogg_uint32_t acc = 0;
      int j;
      for(j=0;j<8;j++){
	acc += y[0]; 
	acc += y[1]; 
	acc += y[2]; 
	acc += y[3]; 
	acc += y[4]; 
	acc += y[5]; 
	acc += y[6]; 
	acc += y[7]; 
	y += cpi->stride[0];
      }

      y = frame + cpi->frag_buffer_index[fi];
      for(j=0;j<8;j++){
	sad += abs ((y[0]<<6)-acc); 
	sad += abs ((y[1]<<6)-acc); 
	sad += abs ((y[2]<<6)-acc); 
	sad += abs ((y[3]<<6)-acc); 
	sad += abs ((y[4]<<6)-acc); 
	sad += abs ((y[5]<<6)-acc); 
	sad += abs ((y[6]<<6)-acc); 
	sad += abs ((y[7]<<6)-acc); 
	y += cpi->stride[0];
      }
    }
  }
  return sad;





}

ogg_uint32_t PickModes(CP_INSTANCE *cpi,
                       ogg_uint32_t *InterError, 
		       ogg_uint32_t *IntraError) {
  
  unsigned char qi = cpi->BaseQ; // temporary

  mc_state mcenc;
  mv_t bmvs[4];
  int mbi, bi;
  
  oc_mode_scheme_chooser_init(cpi);
  oc_mcenc_start(cpi, &mcenc); 
  *InterError = 0;
  *IntraError = 0;
  cpi->MVBits_0 = 0;
  cpi->MVBits_1 = 0;
 
  /* Scan through macroblocks. */
  for(mbi = 0; mbi<cpi->macro_total; mbi++){
    ogg_uint32_t  sad[8] = {0,0,0,0, 0,0,0,0};
    macroblock_t *mb     = &cp->macro[mbi];

    /*Move the motion vector predictors back a frame */
    memmove(mb->analysis_mv+1,mb->analysis_mv,2*sizeof(mb->analysis_mv[0]));

    /* basic 1MV search always done for all macroblocks, coded or not, keyframe or not */
    oc_mcenc_search(cpi, &mcenc, mbi, 0, bmvs, &errormv, &error4mv);

    if(mb->coded==0){
      /* Don't bother to do a MV search against the golden frame.
	 Just re-use the last vector, which should match well since the
	 contents of the MB haven't changed much.*/
      mb->analysis_mv[0][1]=mb->analysis_mv[1][1];
      continue;
    }

    /* search golden frame */
    oc_mcenc_search(cpi, &mcenc, mbi, 1, NULL, &errormv_gold, NULL);

    if(cpi->FrameType == KEY_FRAME){
      mb->mode = CODE_INTRA;
      continue;
    }

    /* Count the number of coded blocks that are luma blocks, and replace the
       block MVs for not-coded blocks with (0,0).*/    
    for ( bi=0; bi<4; bi++ ){
      fi = mbp->yuv[0][bi];
      if(!cp[fi]) bmvs[bi]={0,0};
    }
      
    /**************************************************************
     Find the block choice with the lowest estimated coding cost

     NOTE THAT if U or V is coded but no Y from a macro block then the
     mode will be CODE_INTER_NO_MV as this is the default state to
     which the mode data structure is initialised in encoder and
     decoder at the start of each frame. */

    /* calculate INTRA error */
    for(ci=1;ci<64;ci++)
      sad[CODE_INTRA]+=abs(efrag->dct_coeffs[ci]);
   


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

	mbp->mode = CODE_INTER_FOURMV;
	mbp->mv[0] = FourMVect[0];
	mbp->mv[1] = FourMVect[1];
	mbp->mv[2] = FourMVect[2];
	mbp->mv[3] = FourMVect[3];

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


