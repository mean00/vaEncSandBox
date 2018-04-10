/***************************************************************************
                          \fn     libvaEnc_plugin
                          \brief  Plugin to use libva hw encoder (intel mostly)
                             -------------------

    copyright            : (C) 2018 by mean
    email                : fixounet@free.fr
 * 
 * 

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
#pragma once

#define ADM_VA_USE_MP4_FORMAT  // i.e. TS/h264 else MP4/AVC1

/**
 * 
 */
#define VA_ENC_NB_SURFACE 16
#include "ADM_libVaEncodingContext.h"
#include "vaBitstream.h"
#include "vaDefines.h"
#include "vaenc_settings.h"
extern vaconf_settings vaH264Settings;

#define VA_BITRATE (vaH264Settings.BitrateKbps*1000)
/**
 */
//-- Global configuration --
namespace ADM_VA_Global
{
  
  extern VAProfile          h264_profile ;
  extern int                ip_period;
  extern vaSetAttributes    newAttributes;
  extern int                h264_packedheader ; /* support pack header? */
  extern int                h264_maxref ;
};
using namespace ADM_VA_Global;


/**
 * 
 */
class ADM_vaEncodingContextH264 : public ADM_vaEncodingContext
{
public:
                    ADM_vaEncodingContextH264()   ;             
                    ~ADM_vaEncodingContextH264();
            bool    setup( int width, int height, std::vector<ADM_vaSurface *>knownSurfaces);
    virtual bool    encode(ADMImage *in, ADMBitstream *out);
    virtual bool    generateExtraData(int *size, uint8_t **data);
    
protected:    
//-- Per instance configuration --
          VAConfigID config_id;
          VAContextID context_id;
          int current_frame_type;
          
          VAEncSequenceParameterBufferH264 seq_param;
          VAEncPictureParameterBufferH264  pic_param;
          VAEncSliceParameterBufferH264    slice_param;
          VAPictureH264 CurrentCurrPic;
          VAPictureH264 ReferenceFrames[16], RefPicList0_P[32], RefPicList0_B[32], RefPicList1_B[32];

          unsigned int num_ref_frames;
          unsigned int numShortTerm;

          unsigned int MaxPicOrderCntLsb;
          unsigned int Log2MaxFrameNum;
          unsigned int Log2MaxPicOrderCntLsb;



          int frame_width;
          int frame_height;
          int frame_width_mbaligned;
          int frame_height_mbaligned;
          int gop_start;
          uint64_t current_frame_encoding;
          
          // -- RC --
          
          int initial_qp;
          int minimal_qp;
          int rc_mode;
          
          
          //--
          ADM_vaEncodingBuffers *vaEncodingBuffers[VA_ENC_NB_SURFACE];
          ADM_vaSurface         *vaSurface[VA_ENC_NB_SURFACE];
          ADM_vaSurface         *vaRefSurface[VA_ENC_NB_SURFACE];
          uint8_t               *tmpBuffer;

         void sps_rbsp(vaBitstream *bs);
         void pps_rbsp(vaBitstream *bs);
         
         bool build_packed_pic_buffer(vaBitstream *bs);
         bool build_packed_seq_buffer(vaBitstream *bs);    
         bool build_packed_sei_buffer_timing(vaBitstream *bs,
                                unsigned int init_cpb_removal_length,
				unsigned int init_cpb_removal_delay,
				unsigned int init_cpb_removal_delay_offset,
				unsigned int cpb_removal_length,
				unsigned int cpb_removal_delay,
				unsigned int dpb_output_length,
				unsigned int dpb_output_delay);
               
        void encoding2display_order(    uint64_t encoding_order, int intra_idr_period,  int *frame_type);
        bool update_ReferenceFrames(void);
        bool update_RefPicList(void);        
        int  calc_poc(int pic_order_cnt_lsb);
        // MP4
        bool render_sequence(void);
        bool render_picture(int frameNumber,int frameType);
        bool render_slice(int frameNumber,int frameType);   
        void slice_header(vaBitstream *bs);
        
        // Annex B
        bool render_packedsequence(void);
        bool render_packedpicture(void);
        bool render_packedsei(void);
        bool render_packedslice(void);
        bool build_packed_slice_buffer(vaBitstream *bs);
        
        bool render_hrd(void);

        //
        void fillSeqParam();
        void fillPPS(int frameNumber, int frameType);
        
};
// EOF 
 
