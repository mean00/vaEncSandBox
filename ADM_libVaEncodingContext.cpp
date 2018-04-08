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
  VAProfile      h264_profile ;
  VAConfigAttrib attrib[VAConfigAttribTypeMax];
  VAConfigAttrib config_attrib[VAConfigAttribTypeMax];
  int            ip_period = 1;
  int            config_attrib_num = 0;
  int            constraint_set_flag = 0;
  int            h264_packedheader = 0; /* support pack header? */
  int            h264_maxref = (1<<16|1);
  int            h264_entropy_mode = 1; /* cabac */
  
  
#warning FIXME
  int enc_packed_header_idx; // FIXME!
#warning FIXME  

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
 * Return displaying order with specified periods and encoding order
 * displaying_order: displaying order
 * frame_type: frame type 
 */

bool init_va(void)
{
    VAProfile profile_list[]={VAProfileH264High,VAProfileH264Main,VAProfileH264Baseline,VAProfileH264ConstrainedBaseline};
    VAEntrypoint *entrypoints;
    int num_entrypoints, slice_entrypoint;
    int support_encode = 0;    
    int major_ver, minor_ver;
    VAStatus va_status;
    unsigned int i;

    num_entrypoints = vaMaxNumEntrypoints(admLibVA::getDisplay());
    entrypoints = (VAEntrypoint*) malloc(num_entrypoints * sizeof(*entrypoints));
    if (!entrypoints) {
        ADM_warning( "error: failed to initialize VA entrypoints array\n");
        return false;
    }

    /* use the highest profile */
    for (i = 0; i < sizeof(profile_list)/sizeof(profile_list[0]); i++) 
    {
        h264_profile = profile_list[i];
        vaQueryConfigEntrypoints(admLibVA::getDisplay(), h264_profile, entrypoints, &num_entrypoints);
        for (slice_entrypoint = 0; slice_entrypoint < num_entrypoints; slice_entrypoint++) 
        {
            if (entrypoints[slice_entrypoint] == VAEntrypointEncSlice) 
            {
                support_encode = 1;
                break;
            }
        }
        if (support_encode == 1)
            break;
    }
    
    if (support_encode == 0) {
        ADM_warning("Can't find VAEntrypointEncSlice for H264 profiles\n");
        return false;
    } else {
        switch (h264_profile) {
            case VAProfileH264Baseline:
                printf("Use profile VAProfileH264Baseline\n");
                constraint_set_flag |= (1 << 0); /* Annex A.2.1 */
                h264_entropy_mode = 0;
                break;
            case VAProfileH264ConstrainedBaseline:
                printf("Use profile VAProfileH264ConstrainedBaseline\n");
                constraint_set_flag |= (1 << 0 | 1 << 1); /* Annex A.2.2 */
                break;

            case VAProfileH264Main:
                printf("Use profile VAProfileH264Main\n");
                constraint_set_flag |= (1 << 1); /* Annex A.2.2 */
                break;

            case VAProfileH264High:
                constraint_set_flag |= (1 << 3); /* Annex A.2.4 */
                printf("Use profile VAProfileH264High\n");
                break;
            default:
                printf("unknow profile. Set to Baseline");
                h264_profile = VAProfileH264Baseline;
                constraint_set_flag |= (1 << 0); /* Annex A.2.1 */
                break;
        }
    }

    /* find out the format for the render target, and rate control mode */
    for (i = 0; i < VAConfigAttribTypeMax; i++)
        attrib[i].type = (VAConfigAttribType)i;

    CHECK_VA_STATUS_BOOL(vaGetConfigAttributes(admLibVA::getDisplay(), h264_profile, VAEntrypointEncSlice,
                                      &attrib[0], VAConfigAttribTypeMax));
    
    /* check the interested configattrib */
    if ((attrib[VAConfigAttribRTFormat].value & VA_RT_FORMAT_YUV420) == 0) {
        ADM_warning("Not find desired YUV420 RT format\n");
        return false;
    } else {
        config_attrib[config_attrib_num].type = VAConfigAttribRTFormat;
        config_attrib[config_attrib_num].value = VA_RT_FORMAT_YUV420;
        config_attrib_num++;
    }
    
#if 1

    if (attrib[VAConfigAttribEncPackedHeaders].value != VA_ATTRIB_NOT_SUPPORTED) 
    {
        int tmp = attrib[VAConfigAttribEncPackedHeaders].value;

        printf("Support VAConfigAttribEncPackedHeaders\n");
        
        h264_packedheader = 1;
        config_attrib[config_attrib_num].type = VAConfigAttribEncPackedHeaders;
        config_attrib[config_attrib_num].value = VA_ENC_PACKED_HEADER_NONE;
        
        if (tmp & VA_ENC_PACKED_HEADER_SEQUENCE) {
            printf("Support packed sequence headers\n");
            config_attrib[config_attrib_num].value |= VA_ENC_PACKED_HEADER_SEQUENCE;
        }
        
        if (tmp & VA_ENC_PACKED_HEADER_PICTURE) {
            printf("Support packed picture headers\n");
            config_attrib[config_attrib_num].value |= VA_ENC_PACKED_HEADER_PICTURE;
        }
        
        if (tmp & VA_ENC_PACKED_HEADER_SLICE) {
            printf("Support packed slice headers\n");
            config_attrib[config_attrib_num].value |= VA_ENC_PACKED_HEADER_SLICE;
        }
        
        if (tmp & VA_ENC_PACKED_HEADER_MISC) {
            printf("Support packed misc headers\n");
            config_attrib[config_attrib_num].value |= VA_ENC_PACKED_HEADER_MISC;
        }
        
        enc_packed_header_idx = config_attrib_num;
        config_attrib_num++;
    }
#endif    

    if (attrib[VAConfigAttribEncInterlaced].value != VA_ATTRIB_NOT_SUPPORTED) {
        int tmp = attrib[VAConfigAttribEncInterlaced].value;
        
        printf("Support VAConfigAttribEncInterlaced\n");

        if (tmp & VA_ENC_INTERLACED_FRAME)
            printf("support VA_ENC_INTERLACED_FRAME\n");
        if (tmp & VA_ENC_INTERLACED_FIELD)
            printf("Support VA_ENC_INTERLACED_FIELD\n");
        if (tmp & VA_ENC_INTERLACED_MBAFF)
            printf("Support VA_ENC_INTERLACED_MBAFF\n");
        if (tmp & VA_ENC_INTERLACED_PAFF)
            printf("Support VA_ENC_INTERLACED_PAFF\n");
        
        config_attrib[config_attrib_num].type = VAConfigAttribEncInterlaced;
        config_attrib[config_attrib_num].value = VA_ENC_PACKED_HEADER_NONE;
        config_attrib_num++;
    }
    
    if (attrib[VAConfigAttribEncMaxRefFrames].value != VA_ATTRIB_NOT_SUPPORTED) {
        h264_maxref = attrib[VAConfigAttribEncMaxRefFrames].value;
        
        printf("Support %d RefPicList0 and %d RefPicList1\n",
               h264_maxref & 0xffff, (h264_maxref >> 16) & 0xffff );
    }

    if (attrib[VAConfigAttribEncMaxSlices].value != VA_ATTRIB_NOT_SUPPORTED)
        printf("Support %d slices\n", attrib[VAConfigAttribEncMaxSlices].value);

    if (attrib[VAConfigAttribEncSliceStructure].value != VA_ATTRIB_NOT_SUPPORTED) {
        int tmp = attrib[VAConfigAttribEncSliceStructure].value;
        
        printf("Support VAConfigAttribEncSliceStructure\n");

        if (tmp & VA_ENC_SLICE_STRUCTURE_ARBITRARY_ROWS)
            printf("Support VA_ENC_SLICE_STRUCTURE_ARBITRARY_ROWS\n");
        if (tmp & VA_ENC_SLICE_STRUCTURE_POWER_OF_TWO_ROWS)
            printf("Support VA_ENC_SLICE_STRUCTURE_POWER_OF_TWO_ROWS\n");
        if (tmp & VA_ENC_SLICE_STRUCTURE_ARBITRARY_MACROBLOCKS)
            printf("Support VA_ENC_SLICE_STRUCTURE_ARBITRARY_MACROBLOCKS\n");
    }
    if (attrib[VAConfigAttribEncMacroblockInfo].value != VA_ATTRIB_NOT_SUPPORTED) {
        printf("Support VAConfigAttribEncMacroblockInfo\n");
    }

    free(entrypoints);
    return true;
}
