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
  last mod: $Id: compglobals.c,v 1.4 2003/12/03 08:59:39 arc Exp $

 ********************************************************************/

#include "codec_internal.h"

/* the Roundup32 silliness is dangerous on non-Intel processors and
   will also choke some C compilers.  Find a non dangerous way later.
   Disabled for now.

   #define ROUNDUP32(X) ( ( ( (unsigned long) X ) + 31 )&( 0xFFFFFFE0 ) ) */

void EDeleteFragmentInfo(CP_INSTANCE * cpi){
  if(cpi->extra_fragmentsAlloc)
    _ogg_free(cpi->extra_fragmentsAlloc);
  if(cpi->FragmentLastQAlloc)
    _ogg_free(cpi->FragmentLastQAlloc);
  if(cpi->FragTokensAlloc)
    _ogg_free(cpi->FragTokensAlloc);
  if(cpi->FragTokenCountsAlloc)
    _ogg_free(cpi->FragTokenCountsAlloc);
  if(cpi->RunHuffIndicesAlloc)
    _ogg_free(cpi->RunHuffIndicesAlloc);
  if(cpi->LastCodedErrorScoreAlloc)
    _ogg_free(cpi->LastCodedErrorScoreAlloc);
  if(cpi->ModeListAlloc)
    _ogg_free(cpi->ModeListAlloc);
  if(cpi->MVListAlloc)
    _ogg_free(cpi->MVListAlloc);
  if(cpi->DCT_codesAlloc )
    _ogg_free( cpi->DCT_codesAlloc );
  if(cpi->DCTDataBufferAlloc )
    _ogg_free( cpi->DCTDataBufferAlloc);
  if(cpi->quantized_listAlloc)
    _ogg_free( cpi->quantized_listAlloc);
  if(cpi->OriginalDCAlloc)
    _ogg_free( cpi->OriginalDCAlloc);
  if(cpi->PartiallyCodedFlags)
    _ogg_free(cpi->PartiallyCodedFlags);
  if(cpi->PartiallyCodedMbPatterns)
    _ogg_free(cpi->PartiallyCodedMbPatterns);
  if(cpi->UncodedMbFlags)
    _ogg_free(cpi->UncodedMbFlags);

  if(cpi->BlockCodedFlagsAlloc)
    _ogg_free(cpi->BlockCodedFlagsAlloc);

  cpi->extra_fragmentsAlloc = 0;
  cpi->FragmentLastQAlloc = 0;
  cpi->FragTokensAlloc = 0;
  cpi->FragTokenCountsAlloc = 0;
  cpi->RunHuffIndicesAlloc = 0;
  cpi->LastCodedErrorScoreAlloc = 0;
  cpi->ModeListAlloc = 0;
  cpi->MVListAlloc = 0;
  cpi->DCT_codesAlloc = 0;
  cpi->DCTDataBufferAlloc = 0;
  cpi->quantized_listAlloc = 0;
  cpi->OriginalDCAlloc = 0;

  cpi->extra_fragments = 0;
  cpi->FragmentLastQ = 0;
  cpi->FragTokens = 0;
  cpi->FragTokenCounts = 0;
  cpi->RunHuffIndices = 0;
  cpi->LastCodedErrorScore = 0;
  cpi->ModeList = 0;
  cpi->MVList = 0;
  cpi->DCT_codes = 0;
  cpi->DCTDataBuffer = 0;
  cpi->quantized_list = 0;
  cpi->OriginalDC = 0;
  cpi->FixedQ = 0;

  cpi->BlockCodedFlagsAlloc = 0;
  cpi->BlockCodedFlags = 0;
}

void EAllocateFragmentInfo(CP_INSTANCE * cpi){

  /* clear any existing info */
  EDeleteFragmentInfo(cpi);

  /* Perform Fragment Allocations */
  cpi->extra_fragments =
    _ogg_malloc(32+cpi->pb.UnitFragments*sizeof(unsigned char));

  /* A note to people reading and wondering why malloc returns aren't
     checked:

     lines like the following that implement a general strategy of
     'check the return of malloc; a zero pointer means we're out of
     memory!'...:

  if(!cpi->extra_fragmentsAlloc) { EDeleteFragmentInfo(cpi); return FALSE; }

     ...are not useful.  It's true that many platforms follow this
     malloc behavior, but many do not.  The more modern malloc
     strategy is only to allocate virtual pages, which are not mapped
     until the memory on that page is touched.  At *that* point, if
     the machine is out of heap, the page fails to be mapped and a
     SEGV is generated.

     That means that is we want to deal with out of memory conditions,
     we *must* be prepared to process a SEGV.  If we implement the
     SEGV handler, there's no reason to to check malloc return; it is
     a waste of code. */

  cpi->FragmentLastQ =
    _ogg_malloc(cpi->pb.UnitFragments*
                sizeof(*cpi->FragmentLastQAlloc));
  cpi->FragTokens =
    _ogg_malloc(cpi->pb.UnitFragments*
                sizeof(*cpi->FragTokensAlloc));
  cpi->OriginalDC =
    _ogg_malloc(cpi->pb.UnitFragments*
                sizeof(*cpi->OriginalDCAlloc));
  cpi->FragTokenCounts =
    _ogg_malloc(cpi->pb.UnitFragments*
                sizeof(*cpi->FragTokenCountsAlloc));
  cpi->RunHuffIndices =
    _ogg_malloc(cpi->pb.UnitFragments*
                sizeof(*cpi->RunHuffIndicesAlloc));
  cpi->LastCodedErrorScore =
    _ogg_malloc(cpi->pb.UnitFragments*
                sizeof(*cpi->LastCodedErrorScoreAlloc));
  cpi->BlockCodedFlags =
    _ogg_malloc(cpi->pb.UnitFragments*
                sizeof(*cpi->BlockCodedFlagsAlloc));
  cpi->ModeList =
    _ogg_malloc(cpi->pb.UnitFragments*
                sizeof(*cpi->ModeListAlloc));
  cpi->MVList =
    _ogg_malloc(cpi->pb.UnitFragments*
                sizeof(cpi->MVListAlloc));
  cpi->DCT_codes =
    _ogg_malloc(64*
                sizeof(*cpi->DCT_codesAlloc));
  cpi->DCTDataBuffer =
    _ogg_malloc(64*
                sizeof(*cpi->DCTDataBufferAlloc));
  cpi->quantized_list =
    _ogg_malloc(64*
                sizeof(*cpi->quantized_listAlloc));
  cpi->PartiallyCodedFlags =
    _ogg_malloc(cpi->pb.MacroBlocks*
                sizeof(*cpi->PartiallyCodedFlags));
  cpi->PartiallyCodedMbPatterns =
    _ogg_malloc(cpi->pb.MacroBlocks*
                sizeof(*cpi->PartiallyCodedMbPatterns));
  cpi->UncodedMbFlags =
    _ogg_malloc(cpi->pb.MacroBlocks*
                sizeof(*cpi->UncodedMbFlags));

}

void EDeleteFrameInfo(CP_INSTANCE * cpi) {
  if(cpi->ConvDestBufferAlloc )
    _ogg_free(cpi->ConvDestBufferAlloc );
  cpi->ConvDestBufferAlloc = 0;
  cpi->ConvDestBuffer = 0;

  if(cpi->yuv0ptrAlloc)
    _ogg_free(cpi->yuv0ptrAlloc);
  cpi->yuv0ptrAlloc = 0;
  cpi->yuv0ptr = 0;

  if(cpi->yuv1ptrAlloc)
    _ogg_free(cpi->yuv1ptrAlloc);
  cpi->yuv1ptrAlloc = 0;
  cpi->yuv1ptr = 0;

  if(cpi->OptimisedTokenListEbAlloc )
    _ogg_free(cpi->OptimisedTokenListEbAlloc);
  cpi->OptimisedTokenListEbAlloc = 0;
  cpi->OptimisedTokenListEb = 0;

  if(cpi->OptimisedTokenListAlloc )
    _ogg_free(cpi->OptimisedTokenListAlloc);
  cpi->OptimisedTokenListAlloc = 0;
  cpi->OptimisedTokenList = 0;

  if(cpi->OptimisedTokenListHiAlloc )
    _ogg_free(cpi->OptimisedTokenListHiAlloc);
  cpi->OptimisedTokenListHiAlloc = 0;
  cpi->OptimisedTokenListHi = 0;

  if(cpi->OptimisedTokenListPlAlloc )
    _ogg_free(cpi->OptimisedTokenListPlAlloc);
  cpi->OptimisedTokenListPlAlloc = 0;
  cpi->OptimisedTokenListPl = 0;

}

void EAllocateFrameInfo(CP_INSTANCE * cpi){
  int FrameSize = cpi->pb.ReconYPlaneSize + 2 * cpi->pb.ReconUVPlaneSize;

  /* clear any existing info */
  EDeleteFrameInfo(cpi);

  /* allocate frames */
  cpi->ConvDestBuffer =
    _ogg_malloc(FrameSize*
                sizeof(*cpi->ConvDestBuffer));
  cpi->yuv0ptr =
    _ogg_malloc(FrameSize*
                sizeof(*cpi->yuv0ptr));
  cpi->yuv1ptr =
    _ogg_malloc(FrameSize*
                sizeof(*cpi->yuv1ptr));
  cpi->OptimisedTokenListEb =
    _ogg_malloc(FrameSize*
                sizeof(*cpi->OptimisedTokenListEb));
  cpi->OptimisedTokenList =
    _ogg_malloc(FrameSize*
                sizeof(*cpi->OptimisedTokenList));
  cpi->OptimisedTokenListHi =
    _ogg_malloc(FrameSize*
                sizeof(*cpi->OptimisedTokenListHi));
  cpi->OptimisedTokenListPl =
    _ogg_malloc(FrameSize*
                sizeof(*cpi->OptimisedTokenListPl));
}

void ClearCPInstance(CP_INSTANCE *cpi){
  if(cpi){
    DeleteTmpBuffers(cpi->pb);
    DeletePPInstance(cpi->pp);
  }
}

void DeleteCPInstance(CP_INSTANCE *cpi){
  if(cpi){
    ClearCPInstance(cpi);
    _ogg_free(cpi);
  }
}

void InitCPInstance(CP_INSTANCE *cpi){
  ogg_uint32_t  i;

  memset((unsigned char *) cpi, 0, sizeof(*cpi));
  AllocateTmpBuffers(&cpi->pb);
  cpi->pp = CreatePPInstance();

  /* Initialise Configuration structure to legal values */
  cpi->Configuration.BaseQ = 32;
  cpi->Configuration.FirstFrameQ = 32;
  cpi->Configuration.MaxQ = 32;
  cpi->Configuration.ActiveMaxQ = 32;
  cpi->Configuration.OutputFrameRate = 30;
  cpi->Configuration.TargetBandwidth = 3000;

  cpi->MVChangeFactor    =    14;
  cpi->FourMvChangeFactor =   8;
  cpi->MinImprovementForNewMV = 25;
  cpi->ExhaustiveSearchThresh = 2500;
  cpi->MinImprovementForFourMV = 100;
  cpi->FourMVThreshold = 10000;
  cpi->BitRateCapFactor = 1.50;
  cpi->InterTripOutThresh = 5000;
  cpi->MVEnabled = TRUE;
  cpi->InterCodeCount = 127;
  cpi->BpbCorrectionFactor = 1.0;
  cpi->GoldenFrameEnabled = TRUE;
  cpi->InterPrediction = TRUE;
  cpi->MotionCompensation = TRUE;
  cpi->ThreshMapThreshold = 5;
  cpi->QuickCompress = TRUE;
  cpi->MaxConsDroppedFrames = 1;
  cpi->Sharpness = 2;

  cpi->PreProcFilterLevel = 2;

  /* Set up default values for QTargetModifier[Q_TABLE_SIZE] table */
  for ( i = 0; i < Q_TABLE_SIZE; i++ )
    cpi->QTargetModifier[Q_TABLE_SIZE] = 1.0;

}
