
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
  last mod: $Id: scan.c 11548 2006-06-09 09:37:51Z illiminable $

 ********************************************************************/

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "codec_internal.h"
#include "dsp.h"
#include "perf_helper.h"


#define MAX_SEARCH_LINE_LEN                   7

#define SET8_0(ptr) \
  ((ogg_uint32_t *)ptr)[0] = 0x00000000; \
  ((ogg_uint32_t *)ptr)[1] = 0x00000000;
#define SET8_1(ptr) \
  ((ogg_uint32_t *)ptr)[0] = 0x01010101; \
  ((ogg_uint32_t *)ptr)[1] = 0x01010101;
#define SET8_8(ptr) \
  ((ogg_uint32_t *)ptr)[0] = 0x08080808; \
  ((ogg_uint32_t *)ptr)[1] = 0x08080808;

static ogg_uint32_t LineLengthScores[ MAX_SEARCH_LINE_LEN + 1 ] = {
  0, 0, 0, 0, 2, 4, 12, 24
};

static ogg_uint32_t BodyNeighbourScore = 8;
static double DiffDevisor = 0.0625;
#define HISTORY_BLOCK_FACTOR    2
#define MIN_STEP_THRESH 6
#define SCORE_MULT_LOW    0.5
#define SCORE_MULT_HIGH   4

#define UP      0
#define DOWN    1
#define LEFT    2
#define RIGHT   3

#define INTERNAL_BLOCK_HEIGHT   8
#define INTERNAL_BLOCK_WIDTH    8

#define BLOCK_NOT_CODED                       0
#define BLOCK_CODED_BAR                       3
#define BLOCK_CODED_SGC                       4
#define BLOCK_CODED_LOW                       4
#define BLOCK_CODED                           5

#define CANDIDATE_BLOCK_LOW                  -2
#define CANDIDATE_BLOCK                      -1

#define FIRST_ROW           0
#define NOT_EDGE_ROW        1
#define LAST_ROW            2

#define YDIFF_CB_ROWS                   (INTERNAL_BLOCK_HEIGHT * 3)
#define CHLOCALS_CB_ROWS                (INTERNAL_BLOCK_HEIGHT * 3)
#define PMAP_CB_ROWS                    (INTERNAL_BLOCK_HEIGHT * 3)


static unsigned __int64 perf_rds_datmf_time = 0;
static unsigned __int64 perf_rds_datmf_count = 0;
static unsigned __int64 perf_rds_datmf_min = -1;

/* This is temporary until all the brances have been vectorise */
static unsigned char ApplyPakLowPass__sse2( PP_INSTANCE *ppi,
                                      unsigned char * SrcPtr ){
  unsigned char * SrcPtr1 = SrcPtr - 1;
  unsigned char * SrcPtr0 = SrcPtr1 - ppi->PlaneStride; /* Note the
                                                           use of
                                                           stride not
                                                           width. */
  unsigned char * SrcPtr2 = SrcPtr1 + ppi->PlaneStride;

  return  (unsigned char)( ( (ogg_uint32_t)SrcPtr0[0] +
              (ogg_uint32_t)SrcPtr0[1] +
              (ogg_uint32_t)SrcPtr0[2] +
              (ogg_uint32_t)SrcPtr1[0] +
              (ogg_uint32_t)SrcPtr1[2] +
              (ogg_uint32_t)SrcPtr2[0] +
              (ogg_uint32_t)SrcPtr2[1] +
              (ogg_uint32_t)SrcPtr2[2]   ) >> 3 );

}

static void ApplyPakLowPass_Vectorised__sse2( PP_INSTANCE *ppi,
                                      unsigned char * SrcPtr,
                                      unsigned short * OutputPtr)
{

#if 0

  int i;
  for (i = 0; i < 8; i++)
  {
      unsigned char * SrcPtr1 = SrcPtr - 1;
      unsigned char * SrcPtr0 = SrcPtr1 - ppi->PlaneStride; /* Note the
                                                               use of
                                                               stride not
                                                               width. */
      unsigned char * SrcPtr2 = SrcPtr1 + ppi->PlaneStride;

      //OutputPtr[i] = ( ( (ogg_uint32_t)SrcPtr[i-1-s] +
      //            (ogg_uint32_t)SrcPtr[i-s] +
      //            (ogg_uint32_t)SrcPtr[i-s+1] +
      //            (ogg_uint32_t)SrcPtr[i-1] +
      //            (ogg_uint32_t)SrcPtr[i+1] +
      //            (ogg_uint32_t)SrcPtr[i+s-1] +
      //            (ogg_uint32_t)SrcPtr[i+s] +
      //            (ogg_uint32_t)SrcPtr[i+s+1]   ) >> 3 );

  OutputPtr[i] =   (unsigned char)( ( (ogg_uint32_t)SrcPtr0[0 + i] +
              (ogg_uint32_t)SrcPtr0[1 + i] +
              (ogg_uint32_t)SrcPtr0[2 + i] +
              (ogg_uint32_t)SrcPtr1[0 + i] +
              (ogg_uint32_t)SrcPtr1[2 + i] +
              (ogg_uint32_t)SrcPtr2[0 + i] +
              (ogg_uint32_t)SrcPtr2[1 + i] +
              (ogg_uint32_t)SrcPtr2[2 + i]   ) >> 3 );
  }


#else

  /*                                                            
            .... .... .... .... XXXX XXXX .... .... .... ....




            .... .... .... ...1 23.. ..ab c... .... .... ....
            .... .... .... ...4 X5.. ..dY e... .... .... ....
            .... .... .... ...6 78.. ..fg h... .... .... ....


            //Different numbering below
            //Showing per row for the top and bottom rows

            1234567abc

            desired,
            1+2+3 = A
            2+3+4 = B
            3+4+5 = C
            4+5+6 = D
            5+6+7 = E
            6+7+a = F
            7+a+b = G
            a+b+c = H

                    1    2    3    4    5    6    7    a  | b    c
                +   _    1    2    3    4    5    6    7  | a    b    c
                -------------------------------------------------------
                    1   1+2  2+3  3+4  4+5  5+6  6+7  7+a |a+b  b+c   c

                +   2    3    4    5    6    7    a  | b    c    _
                -------------------------------------------------------
                   1+2   A    B    C    D    E    F  | G    H



            //Showing per row for the middle row

            1234567abc

            desired,
            1+3 = A
            2+4 = B
            3+5 = C
            4+6 = D
            5+7 = E
            6+a = F
            7+b = G
            a+c = H


                    1    2    3    4    5    6    7    a  | b    c
                +   _    _    1    2    3    4    5    6  | 7    a    b    c
                -------------------------------------------------------
                              A    B    C   D     E    F    G    H


  */

    static __declspec(align(16)) unsigned long Low6WordsMask[4] = { 0xffffffff, 0xffffffff, 0xffffffff, 0x00000000 };
    static unsigned char* Low6WordsMaskPtr = (unsigned char*)Low6WordsMask;
    long    stride = ppi->PlaneStride;
    unsigned char* SrcPtrTopLeft = SrcPtr - stride - 1;


    __asm {
        align           16

        mov         esi, SrcPtrTopLeft
        mov         eax, Low6WordsMaskPtr
        mov         ecx, stride
        mov         edi, OutputPtr

        movdqa      xmm7, [eax]
        pxor        xmm0, xmm0
        pcmpeqw     xmm6, xmm6  /* All 1's */

        /* Create the inverse mask -- xmm6 = ~xmm7 */
        pxor        xmm6, xmm7

        /***************************************/
        /* TOP ROW OF THE 8 SURROUNDING PIXELS */
        /***************************************/

        /* There are 10 bytes, read the first 8 into the first register, after the shifting
            there will be 6 usable results. For the second register start at plus 2
            so it also has 8 but 6 of them overlap, this stops us reading past the block
            we are supposed to be looking at, and since we operate on the whole register
            anyway, it actually doesn't matter if theres 8 or only 2 inside */

        movq        xmm1, QWORD PTR [esi]
        movq        xmm2, QWORD PTR [esi + 2]       /* this one partly overlaps */



        /* Expand to 16 bits */
        punpcklbw   xmm1, xmm0
        punpcklbw   xmm2, xmm0
        movdqa      xmm3, xmm1
        movdqa      xmm4, xmm2

        /* Shift all 8 items right by 1 lot of 16 bits to get the intermediate sums */
        psrldq      xmm1, 2
        psrldq      xmm2, 2
        paddw       xmm1, xmm3
        paddw       xmm2, xmm4

        /* Shift right by 1 lot of 16 to get the intermediate triple sums */
        pslldq      xmm3, 2
        pslldq      xmm4, 2
        paddw       xmm1, xmm3
        paddw       xmm2, xmm4

        /* Now have 6 lots of triple sums in words 1-6 (0, and 7 have junk) 
             in the first regsiter. and in bytes 5 and 6 of the second register
             there is the final 2 triple sums */


        /* Merge the 8 results into 1 register */
            /* Shift words 1-6 to positions 0-5 */
            psrldq      xmm1, 2
            /* Shift words 5 and 6 to positions 6 and 7 - since
                we don't care about any of the other positions in this regsiter
                use the qword 64 bitwise shift which is twice as fast as the 
                dq 128 bitwise one */
            psllq       xmm2, 16

            /* Clear the high 32 bits in the first register */
            pand        xmm1, xmm7

            /* Clear the low 6 bytes of the second register */
            pand        xmm2, xmm6

            /* First register now contains all 8 triple sums ie. the sum of the top 3 pixels
                in each of the eight 3x3 adjacent blocks */
            por         xmm1, xmm2



        /***************************************/
        /* BOTTOM ROW OF THE 8 SURROUNDING PIXELS */
        /***************************************/

        /* Jump down 2 lines */
        lea     esi, [esi + ecx*2]
        
        /* There are 10 bytes, read the first 8 into the first register, after the shifting
            there will be 6 usable results. For the second register start at plus 2
            so it also has 8 but 6 of them overlap, this stops us reading past the block
            we are supposed to be looking at, and since we operate on the whole register
            anyway, it actually doesn't matter if theres 8 or only 2 inside */

        movq        xmm5, QWORD PTR [esi]
        movq        xmm2, QWORD PTR [esi + 2]       /* this one partly overlaps */


        /* Expand to 16 bits */
        punpcklbw   xmm5, xmm0
        punpcklbw   xmm2, xmm0
        movdqa      xmm3, xmm5
        movdqa      xmm4, xmm2

        /* Shift all 8 items right by 1 lot of 16 bits to get the intermediate sums */
        psrldq      xmm5, 2
        psrldq      xmm2, 2
        paddw       xmm5, xmm3
        paddw       xmm2, xmm4

        /* Shift right by 1 lot of 16 to get the intermediate triple sums */
        pslldq      xmm3, 2
        pslldq      xmm4, 2
        paddw       xmm5, xmm3
        paddw       xmm2, xmm4

        /* Now have 6 lots of triple sums in words 1-6 (0, and 7 have junk) 
             in the first regsiter. and in bytes 5 and 6 of the second register
             there is the final 2 triple sums */


        /* Merge the 8 results into 1 register */
            /* Shift words 1-6 to positions 0-5 */
            psrldq      xmm5, 2
            /* Shift words 5 and 6 to positions 6 and 7 - since
                we don't care about any of the other positions in this regsiter
                use the dword 32 bitwise shift which is twice as fast as the 
                dq 128 bitwise one */
            psllq       xmm2, 16

            /* Clear the high 32 bits in the first register */
            pand        xmm5, xmm7

            /* Clear the low 6 bytes of the second register */
            pand        xmm2, xmm6

            /* First register now contains all 8 triple sums */
            por         xmm5, xmm2


        /* xmm1 contains the top rows, and xmm5 the bottom rows 
            now sum the top rows into the bottom rows.
        */
        paddw           xmm5, xmm1



        /***************************************/
        /* MIDDLE ROW OF THE 8 SURROUNDING PIXELS */
        /***************************************/

        /* Go back one row to the middle row */
        sub     esi, ecx

        /* In this row, the middle pixel of each consecutive 3 is not to be summed */


        movq        xmm1, QWORD PTR [esi]
        movq        xmm2, QWORD PTR [esi + 2]       /* this one partly overlaps */
        //movdqa      xmm7, [eax]


        /* Expand to 16 bits */
        punpcklbw   xmm1, xmm0
        punpcklbw   xmm2, xmm0
        movdqa      xmm3, xmm1
        movdqa      xmm4, xmm2

        /* Shift all 8 items right by 2 lot of 16 bits to get the intermediate sums */
        psrldq      xmm1, 4
        psrldq      xmm2, 4
        paddw       xmm1, xmm3
        paddw       xmm2, xmm4
        
        /* Merge the 8 results into 1 register */
            /* First register has words 0-5 filled with sums */

            /* Shift words 4 and 5 to positions 6 and 7 - since
                we don't care about any of the other positions in this regsiter
                use the qword 64 bitwise shift which is twice as fast as the 
                dq 128 bitwise one */
            psllq       xmm2, 32

            /* Clear the high 32 bits in the first register */
            pand        xmm1, xmm7

            /* Clear the low 6 bytes of the second register */
            pand        xmm2, xmm6

            /* First register now contains the sum of the left and right pixel
                for each of the eight 3x3 adjacent blocks */
            por         xmm1, xmm2


        /* ---------------------- */

        /* Final 8 sums */
        paddw           xmm1, xmm5

        /* Divide by 8 */
        psrlw           xmm1, 3

        /* Write it into temp[0..16] */
        movdqa          [edi], xmm1
    }

#endif
}

/* This is a new function factor out of rowdiffscan, maybe needs a better name */
static ogg_int32_t RowDiffScan_DiffAndThresholding__sse2(PP_INSTANCE *ppi,
                         unsigned char * YuvPtr1,
                         unsigned char * YuvPtr2,
                         ogg_int16_t   * YUVDiffsPtr,
                         unsigned char * bits_map_ptr,
                         signed char   * SgcPtr)
{
  ogg_int16_t Diff;     /* Temp local workspace. */
  ogg_int32_t j; 
  ogg_int32_t    FragChangedPixels = 0;

    for ( j = 0; j < HFRAGPIXELS; j++ ){
      /* Take a local copy of the measured difference. */
      Diff = (int)YuvPtr1[j] - (int)YuvPtr2[j];

      /* Store the actual difference value */
      YUVDiffsPtr[j] = Diff;

      /* Test against the Level thresholds and record the results */
      SgcPtr[0] += ppi->SgcThreshTable[Diff+255];

      /* Test against the SRF thresholds */
      bits_map_ptr[j] = ppi->SrfThreshTable[Diff+255];
      FragChangedPixels += ppi->SrfThreshTable[Diff+255];
    }

    return FragChangedPixels;

}

/* This is a new function factor out of rowdiffscan, maybe needs a better name */
static ogg_int32_t RowDiffScan_DiffAndThresholdingFirstFrag__sse2(PP_INSTANCE *ppi,
                         unsigned char * YuvPtr1,
                         unsigned char * YuvPtr2,
                         ogg_int16_t   * YUVDiffsPtr,
                         unsigned char * bits_map_ptr,
                         signed char   * SgcPtr)
{

  ogg_int16_t Diff;     /* Temp local workspace. */
  ogg_int32_t j; 
  ogg_int32_t    FragChangedPixels = 0;

  for ( j = 0; j < HFRAGPIXELS; j++ ){
    /* Take a local copy of the measured difference. */
    Diff = (int)YuvPtr1[j] - (int)YuvPtr2[j];

    /* Store the actual difference value */
    YUVDiffsPtr[j] = Diff;

    /* Test against the Level thresholds and record the results */
    SgcPtr[0] += ppi->SgcThreshTable[Diff+255];

    if (j>0 && ppi->SrfPakThreshTable[Diff+255] )
      Diff = (int)ApplyPakLowPass__sse2( ppi, &YuvPtr1[j] ) -
        (int)ApplyPakLowPass__sse2( ppi, &YuvPtr2[j] );

    /* Test against the SRF thresholds */
    bits_map_ptr[j] = ppi->SrfThreshTable[Diff+255];
    FragChangedPixels += ppi->SrfThreshTable[Diff+255];
  }
  return FragChangedPixels;

}



/* This is a new function factor out of rowdiffscan, maybe needs a better name */
static ogg_int32_t RowDiffScan_DiffAndThresholdingLastFrag__sse2(PP_INSTANCE *ppi,
                         unsigned char * YuvPtr1,
                         unsigned char * YuvPtr2,
                         ogg_int16_t   * YUVDiffsPtr,
                         unsigned char * bits_map_ptr,
                         signed char   * SgcPtr)
{
  ogg_int16_t Diff;     /* Temp local workspace. */
  ogg_int32_t j; 
  ogg_int32_t    FragChangedPixels = 0;

  for ( j = 0; j < HFRAGPIXELS; j++ ){
    /* Take a local copy of the measured difference. */
    Diff = (int)YuvPtr1[j] - (int)YuvPtr2[j];

    /* Store the actual difference value */
    YUVDiffsPtr[j] = Diff;

    /* Test against the Level thresholds and record the results */
    SgcPtr[0] += ppi->SgcThreshTable[Diff+255];

    if (j<7 && ppi->SrfPakThreshTable[Diff+255] )
      Diff = (int)ApplyPakLowPass__sse2( ppi, &YuvPtr1[j] ) -
        (int)ApplyPakLowPass__sse2( ppi, &YuvPtr2[j] );


    /* Test against the SRF thresholds */
    bits_map_ptr[j] = ppi->SrfThreshTable[Diff+255];
    FragChangedPixels += ppi->SrfThreshTable[Diff+255];
  }
  return FragChangedPixels;

}






/* This is a new function factor out of rowdiffscan, maybe needs a better name */
static __inline ogg_int32_t RowDiffScan_DiffAndThresholdingMiddleFrag__sse2(PP_INSTANCE *ppi,
                         unsigned char * YuvPtr1,
                         unsigned char * YuvPtr2,
                         ogg_int16_t   * YUVDiffsPtr,
                         unsigned char * bits_map_ptr,
                         signed char   * SgcPtr)
{
#if 0

    /* 10% of all encode exectution is in this function, most
        heavily used function in alpha 6 */

  ogg_int16_t Diff;     /* Temp local workspace. */
  ogg_int32_t j; 
  ogg_int32_t    FragChangedPixels = 0;

  
    for ( j = 0; j < HFRAGPIXELS; j++ ){
      /* Take a local copy of the measured difference. */
      Diff = (int)YuvPtr1[j] - (int)YuvPtr2[j];

      /* Store the actual difference value */
      YUVDiffsPtr[j] = Diff;

      /* Test against the Level thresholds and record the results */
      SgcPtr[0] += ppi->SgcThreshTable[Diff+255];

      if (ppi->SrfPakThreshTable[Diff+255] )
        Diff = (int)ApplyPakLowPass__sse2( ppi, &YuvPtr1[j] ) -
          (int)ApplyPakLowPass__sse2( ppi, &YuvPtr2[j] );


      /* Test against the SRF thresholds */
      bits_map_ptr[j] = ppi->SrfThreshTable[Diff+255];
      FragChangedPixels += ppi->SrfThreshTable[Diff+255];
    }
    return FragChangedPixels;
  //PERF_BLOCK_END("RowDiffScan_DiffAndThresholdingMiddleFrag", perf_rds_datmf_time, perf_rds_datmf_time,perf_rds_datmf_time, 10000);

#else

    static __declspec(align(16)) unsigned long Some255s[4] = { 0x00ff00ff, 0x00ff00ff, 0x00ff00ff, 0x00ff00ff };
    static __declspec(align(16)) unsigned char temp[48];
    static unsigned short* temp_ptr = (unsigned short*)temp;
    
    static unsigned char* some_255s_ptr = (unsigned char*)Some255s;
    unsigned char* local_sgc_thresh_table = ppi->SgcThreshTable;
    unsigned char* local_srf_thresh_table = ppi->SrfThreshTable;
    unsigned char* local_srf_pak_thresh_table = ppi->SrfPakThreshTable;

    
    unsigned char thresh_val;
    int i, FragChangedPixels = 0;


    __asm {
        align       16
        mov         esi, YuvPtr1
        mov         edx, YuvPtr2
        mov         edi, YUVDiffsPtr        /* Not aligned */
        mov         eax, some_255s_ptr;
        mov         ecx, temp_ptr

        movdqa      xmm7, [eax]
        pxor        xmm0, xmm0

        /* Load yuvptr1[0..7] into low 8 bytes */
        movq        xmm1, QWORD PTR [esi]
        /* Load yuvptr2[0..7] into low 8 bytes */
        movq        xmm2, QWORD PTR [edx]

        /* Unpack to 16 bits */
        punpcklbw   xmm1, xmm0
        punpcklbw   xmm2, xmm0

        /* Subtract the YUV Ptr values */
        psubw      xmm1, xmm2   /*should it be subsw?? */

        /* Write out to YUVDiffs */
        movdqu      [edi], xmm1

        /* Add 255 to them all */
        paddw       xmm1, xmm7

        /* Write them to the temp area */
        movdqa      [ecx], xmm1 

        

    }

    ApplyPakLowPass_Vectorised__sse2(ppi, YuvPtr1, temp_ptr + 8); /* Bytes 16-31 */
    ApplyPakLowPass_Vectorised__sse2(ppi, YuvPtr2, temp_ptr + 16); /* Bytes 32 - 47 */

    __asm {
        align 16

        mov         esi, temp_ptr
        mov         ecx, some_255s_ptr

        movdqa      xmm1, [esi + 16]
        movdqa      xmm2, [esi + 32]
        
        movdqa      xmm6, [ecx]

        /* New diffs after PakLowPass */
        psubw       xmm1, xmm2

        /* Add 255 to the diffs */
        paddw       xmm1, xmm6

        /* Write back out to temp */
        movdqa      [esi +16], xmm1

        /* Now need to process with normal registers ops */



        /* At this point
                temp_ptr[0..15] = 8 lots of Early loop diffs + 255
                temp_ptr[16..31] = 8 lots of late loop diffs + 255
                temp_ptr[32..47] = who cares */

    }


    /* Apply the pak threash_table and write into temp[32..47] */
    temp_ptr[16] = local_srf_pak_thresh_table[temp_ptr[0]];
    temp_ptr[17] = local_srf_pak_thresh_table[temp_ptr[1]];
    temp_ptr[18] = local_srf_pak_thresh_table[temp_ptr[2]];
    temp_ptr[19] = local_srf_pak_thresh_table[temp_ptr[3]];
    temp_ptr[20] = local_srf_pak_thresh_table[temp_ptr[4]];
    temp_ptr[21] = local_srf_pak_thresh_table[temp_ptr[5]];
    temp_ptr[22] = local_srf_pak_thresh_table[temp_ptr[6]];
    temp_ptr[23] = local_srf_pak_thresh_table[temp_ptr[7]];

    __asm {
        align       16

        //mov         edx, YUVDiffsPtr
        mov         esi, temp_ptr

        /* Read back the old diffs+255 */
        movdqu     xmm4, [esi]

        /* Read back the new diffs+255 */
        movdqa     xmm3, [esi + 16]
        
        /* Read back the pak_threshed values used in the if statement */
        movdqa     xmm6, [esi + 32]

        pxor        xmm0, xmm0
        pcmpeqw     xmm7, xmm7      /* All 1's */

        /* Compare the pak_thresh values to 0, any word which was 0, will now be set to all 1's in xmm0 
                the if basically said, if it's zero, leave it alone, otherwise, replace it
                with the new diff */
        pcmpeqw     xmm0, xmm6

        /* On the old diffs, keep all the words where the pak_thresh is zero */
        pand        xmm4, xmm0

        /* Flip the bits so that the places that were 0 are now all zeros */
        pxor        xmm0, xmm7

        /* This zero's out all the words in the new diffs which were 0 in the pak_thresh */
        pand        xmm3, xmm0

        /* Merge the old and new diffs */
        por         xmm3, xmm4

        /* Write back out to temp */
        movdqa      [esi + 32], xmm3
    }

    for (i = 0; i < 8; i++)
    {

        thresh_val = local_srf_thresh_table[temp_ptr[16 + i]];
        SgcPtr[0] += local_sgc_thresh_table[temp_ptr[i]];
        bits_map_ptr[i] = thresh_val;
        FragChangedPixels += thresh_val;

    }

    return FragChangedPixels;
  


#endif
}


static void RowDiffScan__sse2( PP_INSTANCE *ppi,
                         unsigned char * YuvPtr1,
                         unsigned char * YuvPtr2,
                         ogg_int16_t   * YUVDiffsPtr,
                         unsigned char * bits_map_ptr,
                         signed char   * SgcPtr,
                         signed char   * DispFragPtr,
                         unsigned char * FDiffPixels,
                         ogg_int32_t   * RowDiffsPtr,
                         unsigned char * ChLocalsPtr, int EdgeRow ){

  ogg_int32_t    i,j;
  ogg_int32_t    FragChangedPixels;

  ogg_int16_t Diff;     /* Temp local workspace. */
  PERF_BLOCK_START();
  /* Cannot use kernel if at edge or if PAK disabled */
  if ( (!ppi->PAKEnabled) || EdgeRow ){
    for ( i = 0; i < ppi->PlaneWidth; i += HFRAGPIXELS ){
      /* Reset count of pixels changed for the current fragment. */
      FragChangedPixels = 0;

      /* Test for break out conditions to save time. */
      if (*DispFragPtr == CANDIDATE_BLOCK){

        /* Clear down entries in changed locals array */
        SET8_0(ChLocalsPtr);

        FragChangedPixels += RowDiffScan_DiffAndThresholding__sse2(        ppi,
                                                YuvPtr1,
                                                YuvPtr2,
                                                YUVDiffsPtr,
                                                bits_map_ptr,
                                                SgcPtr);
                                                
      }else{
        /* If we are breaking out here mark all pixels as changed. */
        if ( *DispFragPtr > BLOCK_NOT_CODED ){
          SET8_1(bits_map_ptr);
          SET8_8(ChLocalsPtr);
        }else{
          SET8_0(ChLocalsPtr);
        }
      }

      *RowDiffsPtr += FragChangedPixels;
      *FDiffPixels += (unsigned char)FragChangedPixels;

      YuvPtr1 += HFRAGPIXELS;
      YuvPtr2 += HFRAGPIXELS;
      bits_map_ptr += HFRAGPIXELS;
      ChLocalsPtr += HFRAGPIXELS;
      YUVDiffsPtr += HFRAGPIXELS;
      SgcPtr ++;
      FDiffPixels ++;

      /* If we have a lot of changed pixels for this fragment on this
         row then the fragment is almost sure to be picked (e.g. through
         the line search) so we can mark it as selected and then ignore
         it. */
      if (FragChangedPixels >= 7){
        *DispFragPtr = BLOCK_CODED_LOW;
      }
      DispFragPtr++;
    }
  }else{

    /*************************************************************/
    /* First fragment of row !! */

    i = 0;
    /* Reset count of pixels changed for the current fragment. */
    FragChangedPixels = 0;

    /* Test for break out conditions to save time. */
    if (*DispFragPtr == CANDIDATE_BLOCK){
      /* Clear down entries in changed locals array */
      SET8_0(ChLocalsPtr);

      FragChangedPixels += RowDiffScan_DiffAndThresholdingFirstFrag__sse2(        
                                                ppi,
                                                YuvPtr1,
                                                YuvPtr2,
                                                YUVDiffsPtr,
                                                bits_map_ptr,
                                                SgcPtr);

    }else{
      /* If we are breaking out here mark all pixels as changed. */
      if ( *DispFragPtr > BLOCK_NOT_CODED ){
        SET8_1(bits_map_ptr);
        SET8_8(ChLocalsPtr);
      }else{
        SET8_0(ChLocalsPtr);
      }
    }

    *RowDiffsPtr += FragChangedPixels;
    *FDiffPixels += (unsigned char)FragChangedPixels;

    YuvPtr1 += HFRAGPIXELS;
    YuvPtr2 += HFRAGPIXELS;
    bits_map_ptr += HFRAGPIXELS;
    ChLocalsPtr += HFRAGPIXELS;
    YUVDiffsPtr += HFRAGPIXELS;
    SgcPtr ++;
    FDiffPixels ++;

    /* If we have a lot of changed pixels for this fragment on this
       row then the fragment is almost sure to be picked
       (e.g. through the line search) so we can mark it as selected
       and then ignore it. */
    if (FragChangedPixels >= 7){
      *DispFragPtr = BLOCK_CODED_LOW;
    }
    DispFragPtr++;
    /*************************************************************/
    /* Fragment in between!! */

    for ( i = HFRAGPIXELS ; i < ppi->PlaneWidth-HFRAGPIXELS;
          i += HFRAGPIXELS ){
      /* Reset count of pixels changed for the current fragment. */
      FragChangedPixels = 0;

      /* Test for break out conditions to save time. */
      if (*DispFragPtr == CANDIDATE_BLOCK){
        /* Clear down entries in changed locals array */
        SET8_0(ChLocalsPtr);

        FragChangedPixels += RowDiffScan_DiffAndThresholdingMiddleFrag__sse2(        
                                                ppi,
                                                YuvPtr1,
                                                YuvPtr2,
                                                YUVDiffsPtr,
                                                bits_map_ptr,
                                                SgcPtr);

      }else{
        /* If we are breaking out here mark all pixels as changed. */
        if ( *DispFragPtr > BLOCK_NOT_CODED ){
          SET8_1(bits_map_ptr);
          SET8_8(ChLocalsPtr);
        }else{
          SET8_0(ChLocalsPtr);
        }
      }

      *RowDiffsPtr += FragChangedPixels;
      *FDiffPixels += (unsigned char)FragChangedPixels;

      YuvPtr1 += HFRAGPIXELS;
      YuvPtr2 += HFRAGPIXELS;
      bits_map_ptr += HFRAGPIXELS;
      ChLocalsPtr += HFRAGPIXELS;
      YUVDiffsPtr += HFRAGPIXELS;
      SgcPtr ++;
      FDiffPixels ++;

      /* If we have a lot of changed pixels for this fragment on this
         row then the fragment is almost sure to be picked
         (e.g. through the line search) so we can mark it as selected
         and then ignore it. */
      if (FragChangedPixels >= 7){
        *DispFragPtr = BLOCK_CODED_LOW;
      }
      DispFragPtr++;
    }
    /*************************************************************/
    /* Last fragment of row !! */

    /* Reset count of pixels changed for the current fragment. */
    FragChangedPixels = 0;

    /* Test for break out conditions to save time. */
    if (*DispFragPtr == CANDIDATE_BLOCK){
      /* Clear down entries in changed locals array */
      SET8_0(ChLocalsPtr);

      FragChangedPixels += RowDiffScan_DiffAndThresholdingLastFrag__sse2(        
                                                ppi,
                                                YuvPtr1,
                                                YuvPtr2,
                                                YUVDiffsPtr,
                                                bits_map_ptr,
                                                SgcPtr);


    }else{
      /* If we are breaking out here mark all pixels as changed.*/
      if ( *DispFragPtr > BLOCK_NOT_CODED ) {
          SET8_1(bits_map_ptr);
          SET8_8(ChLocalsPtr);
        }else{
          SET8_0(ChLocalsPtr);
        }
    }
    /* If we have a lot of changed pixels for this fragment on this
       row then the fragment is almost sure to be picked (e.g. through
       the line search) so we can mark it as selected and then ignore
       it. */
    *RowDiffsPtr += FragChangedPixels;
    *FDiffPixels += (unsigned char)FragChangedPixels;

    /* If we have a lot of changed pixels for this fragment on this
       row then the fragment is almost sure to be picked (e.g. through
       the line search) so we can mark it as selected and then ignore
       it. */
    if (FragChangedPixels >= 7){
      *DispFragPtr = BLOCK_CODED_LOW;
    }
    DispFragPtr++;

  }

  PERF_BLOCK_END("RowDiffScan ", perf_rds_datmf_time, perf_rds_datmf_count, perf_rds_datmf_min, 10000);
}


void dsp_sse2_scan_init(DspFunctions *funcs)
{
  TH_DEBUG("enabling accelerated x86_32 sse2 scan functions.\n");
  funcs->RowDiffScan = RowDiffScan__sse2;
  //funcs->copy8x8 = copy8x8__sse2;
  //funcs->recon_intra8x8 = recon_intra8x8__sse2;
  //funcs->recon_inter8x8 = recon_inter8x8__sse2;
  //funcs->recon_inter8x8_half = recon_inter8x8_half__sse2;
}