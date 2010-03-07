/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2009                *
 * by the Xiph.Org Foundation and contributors http://www.xiph.org/ *
 *                                                                  *
 ********************************************************************

  function:
    last mod: $Id: $

 ********************************************************************/

#include "ARMint.h"

#if defined(OC_ARM_ASM)

void oc_state_vtable_init_arm(oc_theora_state *_state){
  _state->opt_vtable.frag_copy=oc_frag_copy_arm;
  _state->opt_vtable.frag_recon_intra=oc_frag_recon_intra_arm;
  _state->opt_vtable.frag_recon_inter=oc_frag_recon_inter_arm;
  _state->opt_vtable.frag_recon_inter2=oc_frag_recon_inter2_arm;
  _state->opt_vtable.idct8x8=oc_idct8x8_arm;
  _state->opt_vtable.state_frag_recon=oc_state_frag_recon_arm;
  _state->opt_vtable.state_frag_copy_list=oc_state_frag_copy_list_arm;
  _state->opt_vtable.state_loop_filter_frag_rows=
   oc_state_loop_filter_frag_rows_arm;
  _state->opt_vtable.restore_fpu=oc_restore_fpu_arm;
  _state->opt_data.dct_fzig_zag=OC_FZIG_ZAG;
}

void oc_restore_fpu_arm(void)
{
}

#endif
