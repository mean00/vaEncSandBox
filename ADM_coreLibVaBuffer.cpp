
#include "ADM_default.h"
#include "ADM_coreLibVaBuffer.h"

#define CHECK_ERROR_BOOL(x) {xError=x;if(xError) {ADM_warning(#x "%d =<%s>\n",xError,vaErrorStr(xError));return false;}}
#define CHECK_ERROR(x)      {xError=x;if(xError) {ADM_warning(#x "%d =<%s>\n",xError,vaErrorStr(xError));}}

static VAStatus xError; // this is not thread safe !

ADM_vaEncodingBuffers::ADM_vaEncodingBuffers()
{
    _bufferId=VA_INVALID;
}
/**
 * 
 * @param ctx
 * @param size
 * @return 
 */
bool        ADM_vaEncodingBuffers::setup(VAContextID ctx, int size)
{
        CHECK_ERROR_BOOL( vaCreateBuffer(admLibVA::getDisplay(),ctx,VAEncCodedBufferType,size, 1, NULL, &_bufferId));
        return true;
}
/**
 */
ADM_vaEncodingBuffers::~ADM_vaEncodingBuffers()
{
    if(_bufferId!=VA_INVALID)
    {
           CHECK_ERROR(vaDestroyBuffer(admLibVA::getDisplay(),_bufferId));
           _bufferId=VA_INVALID;
    }
}
/**
 */
ADM_vaEncodingBuffers *ADM_vaEncodingBuffers::allocate(VAContextID context, int size)
{
    ADM_vaEncodingBuffers *b=new ADM_vaEncodingBuffers;
    if(!b->setup(context,size))
    {
        ADM_warning("VaEncoder: Buffer creation failed\n");
        delete b;
        return NULL;
    }
    return b;
}
/**
 * 
 * @param to
 * @param sizeMax
 * @return 
 */
int ADM_vaEncodingBuffers::read(uint8_t *data, int sizeMax)
{
    VACodedBufferSegment *buf_list = NULL;    
    xError= vaMapBuffer(admLibVA::getDisplay(),_bufferId,(void **)(&buf_list));
    CHECK_ERROR(xError)
    if(xError)
    {
        return -1;
    }
    int len=0;
    while (buf_list != NULL) 
    {
        int sz=buf_list->size;
        if(sz+len>sizeMax)
        {
            printf("VA buffer readback buffer size exceeded !");
            ADM_assert(0);
        }
        memcpy(data,buf_list->buf,sz);
        data+=sz;
        len+=sz;
        buf_list = (VACodedBufferSegment *) buf_list->next;
    }
    vaUnmapBuffer(admLibVA::getDisplay(),_bufferId);
    return len;
}

// EOF
