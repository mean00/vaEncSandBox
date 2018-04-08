/***************************************************************************
                          \fn ADM_VideoEncoders
                          \brief Internal handling of video encoders
                             -------------------
    
    copyright            : (C) 2018 by mean
    email                : fixounet@free.fr
 ***************************************************************************/
/* Derived from libva sample code */
/*
 * Copyright (c) 2007-2013 Intel Corporation. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "ADM_default.h"

#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>


#include "ADM_libvaEncoder.h"
#include "va/va.h"
#include "va/va_enc_h264.h"
#include "ADM_coreLibVaBuffer.h"
#include "ADM_libVaEncodingContext.h"
#include "ADM_libVaEncodingContextH264.h"




static bool  init_va(void);
static bool initDone=false;

namespace ADM_VA_Global
{
  VAProfile      h264_profile = VAProfileNone ;
  int            ip_period = 1;
  vaSetAttributes newAttributes;
  int            constraint_set_flag = 0;
  int            h264_packedheader = 0; /* support pack header? */
  int            h264_maxref = (1<<16|1);
  int            h264_entropy_mode = 1; /* cabac */   

};

using namespace ADM_VA_Global;



/**
 */
ADM_vaEncodingContext *ADM_vaEncodingContext::allocate(int codec, int alignedWidth, int alignedHeight, std::vector<ADM_vaSurface *>knownSurfaces)
{
    if(!initDone)
    {
        if(!init_va())
        {
            return NULL;
        }
        initDone=true;
    }
    // Allocate a new one
    ADM_vaEncodingContextH264 *r=new ADM_vaEncodingContextH264;
    if(!r->setup(alignedWidth,   alignedHeight,  knownSurfaces))
    {
        delete r;
        return NULL;
    }
    return r;
}
/* 
 */

static bool  lookupSupportedFormat(VAProfile profile)
{
    int num_entrypoints, slice_entrypoint;
    
    // query it several times, but way simpler code
    num_entrypoints = vaMaxNumEntrypoints(admLibVA::getDisplay());
    VAEntrypoint *entrypoints = (VAEntrypoint*) alloca(num_entrypoints * sizeof(VAEntrypoint));
    vaQueryConfigEntrypoints(admLibVA::getDisplay(), profile, entrypoints, &num_entrypoints);
    for (int slice_entrypoint = 0; slice_entrypoint < num_entrypoints; slice_entrypoint++) 
    {
        if (entrypoints[slice_entrypoint] == VAEntrypointEncSlice) 
        {
            return true;
        }
    }
    return false;
}
/**
 * 
 * @return 
 */
bool init_va(void)
{
    if(lookupSupportedFormat(VAProfileH264High))
    {
        ADM_info("H264 High is supported\n");
        h264_profile=VAProfileH264High;
        constraint_set_flag |= (1 << 3); /* Annex A.2.4 */
    }else
    if(lookupSupportedFormat(VAProfileH264Main))
    {
        ADM_info("H264 Main is supported\n");
        h264_profile=VAProfileH264Main;
        constraint_set_flag |= (1 << 1); /* Annex A.2.2 */
    }
    else
    {
        ADM_warning("No support for encoding (H264 High or Main)\n");
        return false;
    }
    
    vaAttributes attributes(h264_profile);
    
    
    
    if(!attributes.isSet(VAConfigAttribRTFormat,VA_RT_FORMAT_YUV420))
    {
        ADM_warning("YUV420 not supported, bailing\n");
        return false;
    }
    newAttributes.add(VAConfigAttribRTFormat,VA_RT_FORMAT_YUV420);
    
    uint32_t pack=attributes.get(VAConfigAttribEncPackedHeaders);
    if(pack!=VA_ATTRIB_NOT_SUPPORTED)
    {        
        ADM_info("Support VAConfigAttribEncPackedHeaders\n");
        
        
        uint32_t new_value=VA_ENC_PACKED_HEADER_NONE;
        
        if (pack & VA_ENC_PACKED_HEADER_SEQUENCE) { new_value |=VA_ENC_PACKED_HEADER_SEQUENCE;}
        if (pack & VA_ENC_PACKED_HEADER_PICTURE) { new_value |=VA_ENC_PACKED_HEADER_PICTURE;}
        if (pack & VA_ENC_PACKED_HEADER_SLICE) { new_value |=VA_ENC_PACKED_HEADER_SLICE;}
        if (pack & VA_ENC_PACKED_HEADER_MISC) { new_value |=VA_ENC_PACKED_HEADER_MISC;}
        h264_packedheader = new_value;
        newAttributes.add(VAConfigAttribEncPackedHeaders,new_value);
    }

    int ilaced=attributes.get(VAConfigAttribEncInterlaced);
    if(ilaced!=VA_ATTRIB_NOT_SUPPORTED)
    {
        newAttributes.add(VAConfigAttribEncInterlaced,VA_ENC_INTERLACED_NONE);
    }
    int h264_maxref_tmp=attributes.get(VAConfigAttribEncMaxRefFrames);
    if(h264_maxref_tmp!=VA_ATTRIB_NOT_SUPPORTED)
    {    
        h264_maxref = h264_maxref_tmp;
        ADM_info("Max ref frame is %d\n",h264_maxref);
    }
    return true;
}

