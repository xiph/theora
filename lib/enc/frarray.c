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

#include <string.h>
#include "codec_internal.h"
#include <stdio.h>

/* Long run bit string coding */
static ogg_uint32_t FrArrayCodeSBRun( CP_INSTANCE *cpi, ogg_uint32_t value){
  ogg_uint32_t CodedVal = 0;
  ogg_uint32_t CodedBits = 0;

  /* Coding scheme:
        Codeword              RunLength
      0                       1
      10x                     2-3
      110x                    4-5
      1110xx                  6-9
      11110xxx                10-17
      111110xxxx              18-33
      111111xxxxxxxxxxxx      34-4129 */

  if ( value == 1 ){
    CodedVal = 0;
    CodedBits = 1;
  } else if ( value <= 3 ) {
    CodedVal = 0x0004 + (value - 2);
    CodedBits = 3;
  } else if ( value <= 5 ) {
    CodedVal = 0x000C + (value - 4);
    CodedBits = 4;
  } else if ( value <= 9 ) {
    CodedVal = 0x0038 + (value - 6);
    CodedBits = 6;
  } else if ( value <= 17 ) {
    CodedVal = 0x00F0 + (value - 10);
    CodedBits = 8;
  } else if ( value <= 33 ) {
    CodedVal = 0x03E0 + (value - 18);
    CodedBits = 10;
  } else {
    CodedVal = 0x3F000 + (value - 34);
    CodedBits = 18;
  }

  /* Add the bits to the encode holding buffer. */
  oggpackB_write( cpi->oggbuffer, CodedVal, CodedBits );

  return CodedBits;
}

/* Short run bit string coding */
static ogg_uint32_t FrArrayCodeBlockRun( CP_INSTANCE *cpi,
                                         ogg_uint32_t value ) {
  ogg_uint32_t CodedVal = 0;
  ogg_uint32_t CodedBits = 0;

  /* Coding scheme:
        Codeword                                RunLength
        0x                                      1-2
        10x                                     3-4
        110x                                    5-6
        1110xx                                  7-10
        11110xx                                 11-14
        11111xxxx                               15-30 */

  if ( value <= 2 ) {
    CodedVal = value - 1;
    CodedBits = 2;
  } else if ( value <= 4 ) {
    CodedVal = 0x0004 + (value - 3);
    CodedBits = 3;

  } else if ( value <= 6 ) {
    CodedVal = 0x000C + (value - 5);
    CodedBits = 4;

  } else if ( value <= 10 ) {
    CodedVal = 0x0038 + (value - 7);
    CodedBits = 6;

  } else if ( value <= 14 ) {
    CodedVal = 0x0078 + (value - 11);
    CodedBits = 7;
  } else {
    CodedVal = 0x01F0 + (value - 15);
    CodedBits = 9;
 }

  /* Add the bits to the encode holding buffer. */
  oggpackB_write( cpi->oggbuffer, CodedVal, CodedBits );

  return CodedBits;
}

void PackAndWriteDFArray( CP_INSTANCE *cpi ){
  ogg_uint32_t  SB, B;
  int run_last = -1;
  int run_count = 0;
  int run_break = 0;
  int partial=0;
  int fully = 1;
  int invalid_fi = cpi->frag_total;
  unsigned char *cp = cpi->frag_coded;

  /* code the partially coded SB flags */
  for( SB = 0; SB < cpi->super_total; SB++ ) {
    superblock_t *sp = &cpi->super[0][SB];
    int coded = 0;
    fully = 1;

    for ( B=0; B<16; B++ ) {
      int fi = sp->f[B];

      if ( fi != invalid_fi ){
	if ( cp[fi] ) {
	  coded = 1; /* SB at least partly coded */
	}else{
	  fully = 0;
	}
      }
    }
    
    partial = (!fully && coded);
    
    if(run_last == -1){
      oggpackB_write( cpi->oggbuffer, partial, 1);      
      run_last = partial;
    }

    if(run_last == partial && run_count < 4129){
      run_count++;
    }else{
      if(run_break)
	oggpackB_write( cpi->oggbuffer, partial, 1);
  
      run_break=0;
      FrArrayCodeSBRun( cpi, run_count );      
      if(run_count >= 4129) run_break = 1;
      run_count=1;
    }
    run_last=partial;
  }
  if(run_break)
    oggpackB_write( cpi->oggbuffer, partial, 1);
  if(run_count)
    FrArrayCodeSBRun(cpi, run_count);      

  /* code the fully coded/uncoded SB flags */
  run_last = -1;
  run_count = 0;
  run_break = 0;
  for( SB = 0; SB < cpi->super_total; SB++ ) {
    superblock_t *sp = &cpi->super[0][SB];
    int coded = 0;
    fully = 1;
    
    for ( B=0; B<16; B++ ) {
      int fi = sp->f[B];
      if ( fi != invalid_fi ) {
	if ( cp[fi] ) {
	  coded = 1;
	}else{
	  fully = 0;
	}
      }
    }
    
    if(!fully && coded) continue;
    
    if(run_last == -1){
      oggpackB_write( cpi->oggbuffer, fully, 1);      
      run_last = fully;
    }
    
    if(run_last == fully && run_count < 4129){
      run_count++;
    }else{
      if(run_break)
	oggpackB_write( cpi->oggbuffer, fully, 1);
      run_break=0;
      FrArrayCodeSBRun( cpi, run_count );      
      if(run_count >= 4129) run_break = 1;
      run_count=1;
    }
    run_last=fully;
  }
  if(run_break)
    oggpackB_write( cpi->oggbuffer, fully, 1);

  if(run_count)
    FrArrayCodeSBRun(cpi, run_count);      

  /* code the block flags */
  run_last = -1;
  run_count = 0;
  for( SB = 0; SB < cpi->super_total; SB++ ) {
    superblock_t *sp = &cpi->super[0][SB];
    int coded = 0;
    fully = 1;

    for ( B=0; B<16; B++ ) {
      int fi = sp->f[B];      
      if ( fi != invalid_fi ) {
	if ( cp[fi] ) {
	  coded = 1;
	}else{
	  fully = 0; /* SB not fully coded */
	}
      }
    }

    if(fully || !coded) continue;

    for ( B=0; B<16; B++ ) {
      int fi = sp->f[B];      
      if(fi != invalid_fi){
	if(run_last == -1){
	  oggpackB_write( cpi->oggbuffer, cp[fi], 1);      
	  run_last = cp[fi];
	}
	
	if(run_last == cp[fi]){
	  run_count++;
	}else{
	  FrArrayCodeBlockRun( cpi, run_count );
	  run_count=1;
	}
	run_last=cp[fi];
      }
    }
  }
  if(run_count)
    FrArrayCodeBlockRun( cpi, run_count );

}
