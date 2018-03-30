#pragma once
#include "va/va.h"
#include "ADM_coreLibVA.h"

/**
 * 
 * @param context
 * @param size
 * @return 
 */
class ADM_vaEncodingBuffers
{
public:
        static ADM_vaEncodingBuffers *allocate(VAContextID context, int size);
        
        VABufferID  getId() {return _bufferId;}
                    ~ADM_vaEncodingBuffers();
                    int read(uint8_t *to, int sizeMax); // return # of bytes, <0 on error
protected:
                    ADM_vaEncodingBuffers();
        bool        setup(VAContextID ctx, int size);
        VABufferID _bufferId;
};
// EOF