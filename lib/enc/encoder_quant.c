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

#ifdef _TH_DEBUG_
#include <stdio.h>
extern FILE *debugout;
extern long dframe;
#endif

#define OC_QUANT_MAX        (1024<<2)
//unsigned OC_DC_QUANT_MIN[2]={4<<2,8<<2};
//unsigned OC_AC_QUANT_MIN[2]={2<<2,4<<2};
#define OC_MAXI(_a,_b)      ((_a)<(_b)?(_b):(_a))
#define OC_MINI(_a,_b)      ((_a)>(_b)?(_b):(_a))
#define OC_CLAMPI(_a,_b,_c) (OC_MAXI(_a,OC_MINI(_b,_c)))


void WriteQTables(CP_INSTANCE *cpi,oggpack_buffer* _opb) {
  
  th_quant_info *_qinfo = &cpi->quant_info; 
  
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

/* a copied/reconciled version of derf's theora-exp code; redundancy
   should be eliminated at some point */
void InitQTables( CP_INSTANCE *cpi ){
  int            qti; /* coding mode: intra or inter */
  int            pli; /* Y U V */
  th_quant_info *qinfo = &cpi->quant_info;

  for(qti=0;qti<2;qti++){
    for(pli=0;pli<3;pli++){
      int qi;  /* quality index */
      int qri; /* range iterator */
      
      for(qi=0,qri=0; qri<=qinfo->qi_ranges[qti][pli].nranges; qri++){
	th_quant_base base;
	
	ogg_uint32_t      q;
	int               qi_start;
	int               qi_end;
	int               ci;
	memcpy(base,qinfo->qi_ranges[qti][pli].base_matrices[qri],
	       sizeof(base));
	
	qi_start=qi;
	if(qri==qinfo->qi_ranges[qti][pli].nranges)
	  qi_end=qi+1;
	else 
	  qi_end=qi+qinfo->qi_ranges[qti][pli].sizes[qri];
	
	/* Iterate over quality indicies in this range */
	for(;;){
	  
	  /*Scale DC the coefficient from the proper table.*/
	  q=((ogg_uint32_t)qinfo->dc_scale[qi]*base[0]/100)<<2;
	  q=OC_CLAMPI(OC_DC_QUANT_MIN[qti],q,OC_QUANT_MAX);
	  cpi->quant_tables[qti][pli][qi][0]=(ogg_uint16_t)q;
	  cpi->iquant_tables[qti][pli][qi][0]=(ogg_int32_t)(0.5 + (double)SHIFT16/q);

	  /*Now scale AC coefficients from the proper table.*/
	  for(ci=1;ci<64;ci++){
	    q=((ogg_uint32_t)qinfo->ac_scale[qi]*base[ci]/100)<<2;
	    q=OC_CLAMPI(OC_AC_QUANT_MIN[qti],q,OC_QUANT_MAX);
	    cpi->quant_tables[qti][pli][qi][zigzag_index[ci]]=(ogg_uint16_t)q;
	    cpi->iquant_tables[qti][pli][qi][ci]=(ogg_int32_t)(0.5 + (double)SHIFT16/q);
	  }
	  
	  if(++qi>=qi_end)break;
	  
	  /*Interpolate the next base matrix.*/
	  for(ci=0;ci<64;ci++){
	    base[ci]=(unsigned char)
	      ((2*((qi_end-qi)*qinfo->qi_ranges[qti][pli].base_matrices[qri][ci]+
		   (qi-qi_start)*qinfo->qi_ranges[qti][pli].base_matrices[qri+1][ci])
		+qinfo->qi_ranges[qti][pli].sizes[qri])/
	       (2*qinfo->qi_ranges[qti][pli].sizes[qri]));
	  }
	}
      }
    }
  }

#ifdef _TH_DEBUG_
  int i, j, k, l;

  /* dump the static tables */
  {
    int i, j, k, l, m;
    TH_DEBUG("loop filter limits = {");
    for(i=0;i<64;){
      TH_DEBUG("\n        ");
      for(j=0;j<16;i++,j++)
	TH_DEBUG("%3d ",qinfo->loop_filter_limits[i]);
    }
    TH_DEBUG("\n}\n\n");

    TH_DEBUG("ac scale = {");
    for(i=0;i<64;){
      TH_DEBUG("\n        ");
      for(j=0;j<16;i++,j++)
	TH_DEBUG("%3d ",qinfo->ac_scale[i]);
    }
    TH_DEBUG("\n}\n\n");

    TH_DEBUG("dc scale = {");
    for(i=0;i<64;){
      TH_DEBUG("\n        ");
      for(j=0;j<16;i++,j++)
	TH_DEBUG("%3d ",qinfo->dc_scale[i]);
    }
    TH_DEBUG("\n}\n\n");

    for(k=0;k<2;k++)
      for(l=0;l<3;l++){
	char *name[2][3]={
	  {"intra Y bases","intra U bases", "intra V bases"},
	  {"inter Y bases","inter U bases", "inter V bases"}
	};

	th_quant_ranges *r = &qinfo->qi_ranges[k][l];
	TH_DEBUG("%s = {\n",name[k][l]);
	TH_DEBUG("        ranges = %d\n",r->nranges);
	TH_DEBUG("        intervals = { ");
	for(i=0;i<r->nranges;i++)
	  TH_DEBUG("%3d ",r->sizes[i]);
	TH_DEBUG("}\n");
	TH_DEBUG("\n        matricies = { ");
	for(m=0;m<r->nranges+1;m++){
	  TH_DEBUG("\n          { ");
	  for(i=0;i<64;){
	    TH_DEBUG("\n            ");
	    for(j=0;j<8;i++,j++)
	      TH_DEBUG("%3d ",r->base_matrices[m][i]);
	  }
	  TH_DEBUG("\n          }");
	}
	TH_DEBUG("\n        }\n");
      }
  }

  /* dump the calculated quantizer tables */
  for(i=0;i<2;i++){
    for(j=0;j<3;j++){
      for(k=0;k<64;k++){
	TH_DEBUG("quantizer table [%s][%s][Q%d] = {",
		 (i==0?"intra":"inter"),(j==0?"Y":(j==1?"U":"V")),k);
	for(l=0;l<64;l++){
	  if((l&7)==0)
	    TH_DEBUG("\n   ");
	  TH_DEBUG("%4d ",pbi->quant_tables[i][j][k][l]);
	}
	TH_DEBUG("}\n");
      }
    }
  }
#endif

}

void quantize( CP_INSTANCE *cpi,
	       ogg_int32_t *q,
               ogg_int16_t *in,
               ogg_int16_t *out){
  int i;

  /* Set the quantized_list to default to 0 */
  memset(out, 0, 64 * sizeof(*out) );
  
  /* Note that we add half divisor to effect rounding on positive number */
  for( i = 0; i < 64; i++) {
    int val = ( (q[i] * in[i] + (1<<15)) >> 16 );
    if(val>511)val=511;
    if(val<-511)val=-511;
    out[zigzag_index[i]] = val;
  }
}
