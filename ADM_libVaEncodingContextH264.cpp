/***************************************************************************
                          \fn     libvaEnc_plugin
                          \brief  Plugin to use libva hw encoder (intel mostly)
                             -------------------

    copyright            : (C) 2018 by mean
    email                : fixounet@free.fr
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
 /***************************************************************************/
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
#include "va/va.h"
#include "va/va_enc_h264.h"
#include "ADM_coreLibVaBuffer.h"
#include "ADM_libVaEncodingContextH264.h"
#include "ADM_bitstream.h"
#include "ADM_coreVideoEncoder.h"

#define aprintf printf
/**
 * 
 */
ADM_vaEncodingContextH264::ADM_vaEncodingContextH264()
{
    context_id=VA_INVALID;
    config_id=VA_INVALID;
    
    current_frame_encoding=current_frame_display=current_frame_num=0;

    for(int i=0;i<VA_ENC_NB_SURFACE;i++)
        vaEncodingBuffers[i]=NULL;;
    for(int i=0;i<VA_ENC_NB_SURFACE;i++)    
    {
        vaSurface[i]=NULL;
        vaRefSurface[i]=NULL;
    }
    memset(&seq_param, 0, sizeof(seq_param));
    memset(&pic_param, 0, sizeof(pic_param));
    memset(&slice_param, 0, sizeof(slice_param));
    
    intra_idr_period = 60;
    num_ref_frames = 2;
    
    
    current_IDR_display = 0;    
    numShortTerm = 0;
    intra_period=30;
    MaxPicOrderCntLsb = (2<<8);
    Log2MaxFrameNum = 16;
    Log2MaxPicOrderCntLsb = 8;



    frame_rate = 30;
    frame_count = 60;
    frame_bitrate = 0;
    frame_slices = 1;
    frame_size = 0;
    initial_qp = 26;
    minimal_qp = 0;
    rc_mode = VA_RC_CQP;
    misc_priv_type = 0;
    misc_priv_value = 0;   
}
/**
 * 
 */
ADM_vaEncodingContextH264::~ADM_vaEncodingContextH264()
{
    if(context_id!=VA_INVALID)
    {
        vaDestroyContext(admLibVA::getDisplay(),context_id);
        context_id=VA_INVALID;
    }
    if(config_id!=VA_INVALID)
    {
        vaDestroyConfig(admLibVA::getDisplay(),config_id);
        config_id=VA_INVALID;
    }
    for(int i=0;i<VA_ENC_NB_SURFACE;i++)
    {
        if(vaSurface[i])
        {
            delete vaSurface[i];
            vaSurface[i]=NULL;
        }
        if(vaRefSurface[i])
        {
            delete vaRefSurface[i];
            vaRefSurface[i]=NULL;
        }

    }
}
/**
 * 
 * @param width
 * @param height
 * @param knownSurfaces
 * @return 
 */
bool ADM_vaEncodingContextH264::setup( int width, int height, std::vector<ADM_vaSurface *>knownSurfaces)
{

        VAStatus va_status;
        frame_width=width;
        frame_height=height;
        frame_width_mbaligned=(width+15)&~15;
        frame_height_mbaligned=(height+15)&~15;
        int  i;

        va_status = vaCreateConfig(admLibVA::getDisplay(), h264_profile, VAEntrypointEncSlice,
                &config_attrib[0], config_attrib_num, &config_id);
        CHECK_VASTATUS(va_status, "vaCreateConfig");

        int n=knownSurfaces.size();                    
        VASurfaceID *tmp_surfaceId = new VASurfaceID[n];
        for(int i=0;i<n;i++)
        {
            tmp_surfaceId[i]=knownSurfaces[i]->surface;
        }

        /* Create a context for this encode pipe */
        va_status = vaCreateContext(admLibVA::getDisplay(), config_id,
                                    frame_width_mbaligned, frame_height_mbaligned,
                                    VA_PROGRESSIVE,
                                    tmp_surfaceId, n,
                                    &context_id);
        CHECK_VASTATUS(va_status, "vaCreateContext");    

        delete [] tmp_surfaceId;
        tmp_surfaceId=NULL;

        int codedbuf_size = (frame_width_mbaligned * frame_height_mbaligned * 400) / (16*16);

        for (i = 0; i < SURFACE_NUM; i++) 
        {
            vaEncodingBuffers[i]= ADM_vaEncodingBuffers::allocate(context_id,codedbuf_size);
            if(!vaEncodingBuffers[i])
            {
                ADM_warning("Cannot create encoding buffer %d\n",i);
                return false;;
            }
        }

        // Allocate VAImage

        for(int i=0;i<VA_ENC_NB_SURFACE;i++)
        {
            vaSurface[i]=ADM_vaSurface::allocateWithSurface(width,height);
            if(!vaSurface[i]) 
            {
                ADM_warning("Cannot allocate surface\n");
                return false;
            }

            vaRefSurface[i]=ADM_vaSurface::allocateWithSurface(width,height);
            if(!vaRefSurface[i]) 
            {
                ADM_warning("Cannot allocate ref surface\n");
                return false;
            }
        }
        return true;                
}
/**
 * 
 * @param in
 * @param out
 * @return 
 */
bool ADM_vaEncodingContextH264::encode(ADMImage *in, ADMBitstream *out)
    {
    if(!vaSurface[current_frame_encoding%SURFACE_NUM]->fromAdmImage(in))
    {
        ADM_warning("Failed to upload image to vaSurface\n");
        return false;
    }

    encoding2display_order(current_frame_encoding, intra_period, intra_idr_period, ip_period,
                           &current_frame_display, &current_frame_type);
    aprintf("Encoding order = %d, display order=%d, frame type=%d\n",current_frame_encoding,current_frame_display,current_frame_type);
    if (current_frame_type == FRAME_IDR) 
    {
        numShortTerm = 0;
        current_frame_num = 0;
        current_IDR_display = current_frame_display;
    }
    int current_slot= (current_frame_display % SURFACE_NUM);

    VAStatus va_status = vaBeginPicture(admLibVA::getDisplay(), context_id, vaSurface[current_slot]->surface);
    CHECK_VASTATUS(va_status,"vaBeginPicture");
    if (current_frame_type == FRAME_IDR) 
    {
        render_sequence();
        render_picture();            
        if (h264_packedheader) 
        {
            render_packedsequence();
            render_packedpicture();
        }
        out->flags = AVI_KEY_FRAME;
    }
    else 
    {
        out->flags = AVI_P_FRAME;
        render_picture();
    }
    render_slice();
    va_status = vaEndPicture(admLibVA::getDisplay(),context_id);
    CHECK_VASTATUS(va_status,"vaEndPicture");;

    //--    
    int display_order=current_frame_display;
    unsigned long long encode_order=current_frame_encoding;
    va_status = vaSyncSurface(admLibVA::getDisplay(), vaSurface[display_order % SURFACE_NUM]->surface);
    CHECK_VASTATUS(va_status,"vaSyncSurface");

    out->len=vaEncodingBuffers[display_order % SURFACE_NUM]->read(out->data, out->bufferSize);
    ADM_assert(out->len>=0);

    /* reload a new frame data */

    update_ReferenceFrames();        
    current_frame_encoding++;
    out->pts=in->Pts;
    out->dts=out->pts;
    return true;
}
// EOF
