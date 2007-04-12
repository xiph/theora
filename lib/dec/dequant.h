#if !defined(_dequant_H)
# define _dequant_H (1)
# include "quant.h"

int oc_quant_params_unpack(oggpack_buffer *_opb,
 th_quant_info *_qinfo);
void oc_quant_params_clear(th_quant_info *_qinfo);

#endif
