#pragma once
/**
 * 
 */
#define VA_ENC_NB_SURFACE 16
#include "ADM_libVaEncodingContext.h"
class ADM_vaEncodingContextH264 : public ADM_vaEncodingContext
{
public:
                    ADM_vaEncodingContextH264()   ;             
                    ~ADM_vaEncodingContextH264();
            bool    setup( int width, int height, std::vector<ADM_vaSurface *>knownSurfaces);
    virtual bool    encode(ADMImage *in, ADMBitstream *out);
};