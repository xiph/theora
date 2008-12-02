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

void fr_clear(CP_INSTANCE *cpi, fr_state_t *fr){
  fr->sb_partial_last = -1;
  fr->sb_partial_count = 0;
  fr->sb_partial_break = 0;

  fr->sb_full_last = -1;
  fr->sb_full_count = 0;
  fr->sb_full_break = 0;

  fr->b_last = -1;
  fr->b_count = 0;
  fr->b_pend = 0;

  fr->sb_partial=0;
  fr->sb_coded=0;

  fr->cost=0;
}

static int BRun( ogg_uint32_t value, ogg_int16_t *token) {
  
  /* Coding scheme:
     Codeword                                RunLength
     0x                                      1-2
     10x                                     3-4
     110x                                    5-6
     1110xx                                  7-10
     11110xx                                 11-14
     11111xxxx                               15-30 */

  if ( value <= 2 ) {
    *token = value - 1;
    return 2;
  } else if ( value <= 4 ) {
    *token = 0x0004 + (value - 3);
    return 3;
  } else if ( value <= 6 ) {
    *token = 0x000C + (value - 5);
    return 4;
  } else if ( value <= 10 ) {
    *token = 0x0038 + (value - 7);
    return 6;
  } else if ( value <= 14 ) {
    *token = 0x0078 + (value - 11);
    return 7;
  } else {
    *token = 0x01F0 + (value - 15);
    return 9;
 }
}

static int BRunCost( ogg_uint32_t value ) {
  
  if ( value <= 0 ) {
    return 0;
  } else if ( value <= 2 ) {
    return 2;
  } else if ( value <= 4 ) {
    return 3;
  } else if ( value <= 6 ) {
    return 4;
  } else if ( value <= 10 ) {
    return 6;
  } else if ( value <= 14 ) {
    return 7;
  } else {
    return 9;
 }
}

static int SBRun(ogg_uint32_t value, int *token){

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
    *token = 0;
    return 1;
  } else if ( value <= 3 ) {
    *token = 0x0004 + (value - 2);
    return 3;
  } else if ( value <= 5 ) {
    *token = 0x000C + (value - 4);
    return 4;
  } else if ( value <= 9 ) {
    *token = 0x0038 + (value - 6);
    return 6;
  } else if ( value <= 17 ) {
    *token = 0x00F0 + (value - 10);
    return 8;
  } else if ( value <= 33 ) {
    *token = 0x03E0 + (value - 18);
    return 10;
  } else {
    *token = 0x3F000 + (value - 34);
    return 18;
  }
}

static int SBRunCost(ogg_uint32_t value){

  if ( value == 0 ){
    return 0;
  } else if ( value == 1 ){
    return 1;
  } else if ( value <= 3 ) {
    return 3;
  } else if ( value <= 5 ) {
    return 4;
  } else if ( value <= 9 ) {
    return 6;
  } else if ( value <= 17 ) {
    return 8;
  } else if ( value <= 33 ) {
    return 10;
  } else {
    return 18;
  }
}

void fr_skipblock(CP_INSTANCE *cpi, fr_state_t *fr){
  if(fr->sb_coded){
    if(!fr->sb_partial){

      /* superblock was previously fully coded */

      if(fr->b_last==-1){
	/* first run of the frame */
	if(cpi){
	  cpi->fr_block[cpi->fr_block_count]=1;
	  cpi->fr_block_bits[cpi->fr_block_count++]=1;
	}
	fr->cost++;
	fr->b_last = 1;
      }

      if(fr->b_last==1){
	/* in-progress run also a coded run */
	fr->b_count += fr->b_pend;
      }else{
	/* in-progress run an uncoded run; flush */
	if(cpi){
	  fr->cost +=
	    cpi->fr_block_bits[cpi->fr_block_count] = 
	    BRun(fr->b_count, cpi->fr_block+cpi->fr_block_count);
	  cpi->fr_block_count++;
	}else
	  fr->cost += BRunCost(fr->b_count);
	  
	fr->b_count=fr->b_pend;
	fr->b_last = 1;
      }
    }

    /* add a skip block */
    if(fr->b_last == 0){
      fr->b_count++;
    }else{
      if(cpi){
	fr->cost+=
	  cpi->fr_block_bits[cpi->fr_block_count] = 
	  BRun(fr->b_count, cpi->fr_block+cpi->fr_block_count);
	cpi->fr_block_count++;
      }else
	fr->cost+=BRunCost(fr->b_count);
      fr->b_count = 1;
      fr->b_last = 0;
    }
  }
   
  fr->b_pend++;
  fr->sb_partial=1;
}

void fr_codeblock(CP_INSTANCE *cpi, fr_state_t *fr){
  if(fr->sb_partial){
    if(!fr->sb_coded){

      /* superblock was previously completely uncoded */

      if(fr->b_last==-1){
	/* first run of the frame */
	if(cpi){
	  cpi->fr_block[cpi->fr_block_count]=0;
	  cpi->fr_block_bits[cpi->fr_block_count++]=1;
	}
	fr->cost++;
	fr->b_last = 0;
      }

      if(fr->b_last==0){
	/* in-progress run also an uncoded run */
	fr->b_count += fr->b_pend;
      }else{
	/* in-progress run a coded run; flush */
	if(cpi){
	  fr->cost+=
	    cpi->fr_block_bits[cpi->fr_block_count] = 
	    BRun(fr->b_count, cpi->fr_block+cpi->fr_block_count);
	  cpi->fr_block_count++;
	}else
	  fr->cost+=BRunCost(fr->b_count);
	fr->b_count=fr->b_pend;
	fr->b_last = 0;
      }
    }
    
    /* add a coded block */
    if(fr->b_last == 1){
      fr->b_count++;
    }else{
      if(cpi){
	fr->cost+=
	  cpi->fr_block_bits[cpi->fr_block_count] = 
	  BRun(fr->b_count, cpi->fr_block+cpi->fr_block_count);
	cpi->fr_block_count++;
      }else
	fr->cost+=BRunCost(fr->b_count);
      fr->b_count = 1;
      fr->b_last = 1;
    }
  }
   
  fr->b_pend++;
  fr->sb_coded=1;
}

void fr_finishsb(CP_INSTANCE *cpi, fr_state_t *fr){
  /* update partial state */
  int partial = (fr->sb_partial & fr->sb_coded); 
  if(fr->sb_partial_last == -1){
    if(cpi){
      cpi->fr_partial[cpi->fr_partial_count] = partial;
      cpi->fr_partial_bits[cpi->fr_partial_count++] = 1;
    }
    fr->cost++;
    fr->sb_partial_last = partial;
  }
    
  if(fr->sb_partial_last == partial && fr->sb_partial_count < 4129){
    fr->sb_partial_count++;
  }else{
    if(fr->sb_partial_break){
      if(cpi){
	cpi->fr_partial[cpi->fr_partial_count] = partial;
	cpi->fr_partial_bits[cpi->fr_partial_count++] = 1;
      }
      fr->cost++;
    }
      
    fr->sb_partial_break=0;
    if(cpi){
      fr->cost+=
	cpi->fr_partial_bits[cpi->fr_partial_count] = 
	SBRun( fr->sb_partial_count, cpi->fr_partial+cpi->fr_partial_count);
      cpi->fr_partial_count++;
    }else
      fr->cost+=SBRunCost(fr->sb_partial_count);
    
    if(fr->sb_partial_count >= 4129) fr->sb_partial_break = 1;
    fr->sb_partial_count=1;
  }
  fr->sb_partial_last=partial;
  
  /* fully coded/uncoded state */
  if(!fr->sb_partial || !fr->sb_coded){
    
    if(fr->sb_full_last == -1){
      if(cpi){
	cpi->fr_full[cpi->fr_full_count] = fr->sb_coded;
	cpi->fr_full_bits[cpi->fr_full_count++] = 1;
      }
      fr->cost++;
      fr->sb_full_last = fr->sb_coded;
    }
    
    if(fr->sb_full_last == fr->sb_coded && fr->sb_full_count < 4129){
      fr->sb_full_count++;
    }else{
      if(fr->sb_full_break){
	if(cpi){
	  cpi->fr_full[cpi->fr_full_count] = fr->sb_coded;
	  cpi->fr_full_bits[cpi->fr_full_count++] = 1;
	}
	fr->cost++;
      }

      fr->sb_full_break=0;
      if(cpi){
	fr->cost+=
	  cpi->fr_full_bits[cpi->fr_full_count] = 
	  SBRun( fr->sb_full_count, cpi->fr_full+cpi->fr_full_count);
	cpi->fr_full_count++;
      }else
	fr->cost+= SBRunCost( fr->sb_full_count);
      if(fr->sb_full_count >= 4129) fr->sb_full_break = 1;
      fr->sb_full_count=1;
    }
    fr->sb_full_last=fr->sb_coded;

  }

  fr->b_pend=0;
  fr->sb_partial=0;
  fr->sb_coded=0;
}

static void fr_flush(CP_INSTANCE *cpi, fr_state_t *fr){
  /* flush any pending partial run */
  if(fr->sb_partial_break){
    if(cpi){
      cpi->fr_partial[cpi->fr_partial_count] = fr->sb_partial_last;
      cpi->fr_partial_bits[cpi->fr_partial_count++] = 1;
    }
    fr->cost++;
  }
  if(fr->sb_partial_count){
    if(cpi){
      fr->cost+=
	cpi->fr_partial_bits[cpi->fr_partial_count] = 
	SBRun( fr->sb_partial_count, cpi->fr_partial+cpi->fr_partial_count);
      cpi->fr_partial_count++;
    }else
      fr->cost+=SBRunCost( fr->sb_partial_count );
  }
  
  /* flush any pending full run */
  if(fr->sb_full_break){
    if(cpi){
      cpi->fr_full[cpi->fr_full_count] = fr->sb_full_last;
      cpi->fr_full_bits[cpi->fr_full_count++] = 1;
    }
    fr->cost++;
  }
  if(fr->sb_full_count){
    if(cpi){
      fr->cost+=
	cpi->fr_full_bits[cpi->fr_full_count] = 
	SBRun( fr->sb_full_count, cpi->fr_full+cpi->fr_full_count);
      cpi->fr_full_count++;
    }else
      fr->cost+=SBRunCost(fr->sb_full_count);
  }
  
  /* flush any pending block run */
  if(fr->b_count){
    if(cpi){
      fr->cost+=
	cpi->fr_block_bits[cpi->fr_block_count] = 
	BRun(fr->b_count, cpi->fr_block+cpi->fr_block_count);
      cpi->fr_block_count++;
    }else
      fr->cost+=BRunCost(fr->b_count);
  }
}

void fr_write(CP_INSTANCE *cpi, fr_state_t *fr){
  int i;

  fr_flush(cpi,fr);

  for(i=0;i<cpi->fr_partial_count;i++)
    oggpackB_write( cpi->oggbuffer, cpi->fr_partial[i], cpi->fr_partial_bits[i]);      
  for(i=0;i<cpi->fr_full_count;i++)
    oggpackB_write( cpi->oggbuffer, cpi->fr_full[i], cpi->fr_full_bits[i]);      
  for(i=0;i<cpi->fr_block_count;i++)
    oggpackB_write( cpi->oggbuffer, cpi->fr_block[i], cpi->fr_block_bits[i]);      
}

int fr_cost1(fr_state_t *fr){
  fr_state_t temp = *fr;
  int cost;

  fr_skipblock(NULL,&temp);
  cost=temp.cost;
  temp=*fr;
  fr_codeblock(NULL,&temp);
  return temp.cost - cost;
}

int fr_cost4(fr_state_t *pre, fr_state_t *post){
  fr_state_t temp = *pre;
  int cost;

  fr_skipblock(NULL,&temp);
  fr_skipblock(NULL,&temp);
  fr_skipblock(NULL,&temp);
  fr_skipblock(NULL,&temp);
  fr_finishsb(NULL,&temp);
  cost=temp.cost;
  temp=*post;
  fr_finishsb(NULL,&temp);
  return temp.cost - cost;
}
