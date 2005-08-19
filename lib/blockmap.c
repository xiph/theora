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
  last mod: $Id: blockmap.c,v 1.5 2003/12/03 08:59:39 arc Exp $

 ********************************************************************/

#include "codec_internal.h"

static void CreateMapping ( ogg_int32_t (*BlockMap)[4][4],
                            ogg_uint32_t FirstSB,
                            ogg_uint32_t FirstFrag, ogg_uint32_t HFrags,
                            ogg_uint32_t VFrags ){
  ogg_uint32_t i, j = 0;
  ogg_uint32_t xpos;
  ogg_uint32_t ypos;
  ogg_uint32_t SBrow, SBcol;
  ogg_uint32_t SBRows, SBCols;
  ogg_uint32_t MB, B;

  ogg_uint32_t SB=FirstSB;
  ogg_uint32_t FragIndex=FirstFrag;

  /* Set Super-Block dimensions */
  SBRows = (VFrags>>2) + ( VFrags&0x03 ? 1 : 0 );
  SBCols = (HFrags>>2) + ( HFrags&0x03 ? 1 : 0 );

  /* Map each Super-Block */
  for ( SBrow=0; SBrow<SBRows; SBrow++ ){
    for ( SBcol=0; SBcol<SBCols; SBcol++ ){
      /* Y co-ordinate of Super-Block in Block units */
      ypos = SBrow<<2;

      /* Map Blocks within this Super-Block */
      for ( i=0; (i<4) && (ypos<VFrags); i++, ypos++ ){
        /* X co-ordinate of Super-Block in Block units */
        xpos = SBcol<<2;

        for ( j=0; (j<4) && (xpos<HFrags); j++, xpos++ ){
          MB = (i & 2) + ((j & 2) >> 1);
          B = ((i & 1) << 1) + (j & 1);

          /* Set mapping and move to next fragment */
          BlockMap[SB][MB][B] = FragIndex++;
        }

        /* Move to first fragment in next row in Super-Block */
        FragIndex += HFrags-j;
      }

      /* Move on to next Super-Block */
      SB++;
      FragIndex -= i*HFrags-j;
    }

    /* Move to first Super-Block in next row */
    FragIndex += 3*HFrags;
  }
}

void CreateBlockMapping ( ogg_int32_t  (*BlockMap)[4][4],
                          ogg_uint32_t YSuperBlocks,
                          ogg_uint32_t UVSuperBlocks,
                          ogg_uint32_t HFrags, ogg_uint32_t VFrags ) {
  ogg_uint32_t i, j;

  for ( i=0; i<YSuperBlocks + UVSuperBlocks * 2; i++ ){
    for ( j=0; j<4; j++ ) {
      BlockMap[i][j][0] = -1;
      BlockMap[i][j][1] = -1;
      BlockMap[i][j][2] = -1;
      BlockMap[i][j][3] = -1;
    }
  }

  CreateMapping ( BlockMap, 0, 0, HFrags, VFrags );
  CreateMapping ( BlockMap, YSuperBlocks, HFrags*VFrags, HFrags/2, VFrags/2 );
  CreateMapping ( BlockMap, YSuperBlocks + UVSuperBlocks, (HFrags*VFrags*5)/4,
                  HFrags/2, VFrags/2 );
}

