/*Inverse DCT transforms.*/
#include <ogg/ogg.h>
#if !defined(_idct_H)
# define _idct_H (1)

void oc_idct8x8_c(ogg_int16_t _y[64],const ogg_int16_t _x[64]);
void oc_idct8x8_10_c(ogg_int16_t _y[64],const ogg_int16_t _x[64]);

#endif
