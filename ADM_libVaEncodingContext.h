

#pragma once

#include <vector>
class ADMImage;
class ADM_vaSurface;
class ADMBitstream;

/**
 * 
 * @param codec
 * @param alignedWidth
 * @param alignedHeight
 * @param knownSurfaces
 * @return 
 */
class ADM_vaEncodingContext
{
public:
    virtual      ~ADM_vaEncodingContext() {}
    static       ADM_vaEncodingContext *allocate(int codec, int width, int height, std::vector<ADM_vaSurface *>knownSurfaces);
    virtual bool encode(ADMImage *in, ADMBitstream *out)=0;
};