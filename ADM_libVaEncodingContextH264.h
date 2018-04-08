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
#pragma once
/**
 * 
 */
#define VA_ENC_NB_SURFACE 16
#include "ADM_libVaEncodingContext.h"
#include "vaBitstream.h"
#include "vaDefines.h"


//-- Global configuration --
namespace ADM_VA_Global
{
  extern int            ip_period;
  extern VAProfile      h264_profile ;
  extern VAConfigAttrib attrib[VAConfigAttribTypeMax];
  extern VAConfigAttrib config_attrib[VAConfigAttribTypeMax];
  extern int config_attrib_num ;
  extern int constraint_set_flag ;
  extern int h264_packedheader ; /* support pack header? */
  extern int h264_maxref ;
  extern int h264_entropy_mode ; /* cabac */
  
#warning FIXME
  extern  int enc_packed_header_idx; // FIXME!
#warning FIXME  
};
using namespace ADM_VA_Global;



class ADM_vaEncodingContextH264 : public ADM_vaEncodingContext
{
public:
                    ADM_vaEncodingContextH264()   ;             
                    ~ADM_vaEncodingContextH264();
            bool    setup( int width, int height, std::vector<ADM_vaSurface *>knownSurfaces);
    virtual bool    encode(ADMImage *in, ADMBitstream *out);
    
protected:    
    

//-- Per instance configuration --
          VAConfigID config_id;
          VAContextID context_id;

          unsigned long long current_IDR_display;

          int current_frame_type;
          int intra_idr_period;
          VAEncSequenceParameterBufferH264 seq_param;
          VAEncPictureParameterBufferH264 pic_param;
          VAEncSliceParameterBufferH264 slice_param;
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
          int frame_rate;
          unsigned int frame_count;
          unsigned int frame_bitrate;
          unsigned int frame_slices;
          double frame_size = 0;
          int initial_qp;
          int minimal_qp;
          int rc_mode;
          uint64_t current_frame_encoding,current_frame_display,current_frame_num;
          int intra_period;

          int misc_priv_type ;
          int misc_priv_value;

          ADM_vaEncodingBuffers *vaEncodingBuffers[VA_ENC_NB_SURFACE];
          ADM_vaSurface *vaSurface[VA_ENC_NB_SURFACE];
          ADM_vaSurface *vaRefSurface[VA_ENC_NB_SURFACE];

         void sps_rbsp(vaBitstream *bs);
         void pps_rbsp(vaBitstream *bs);
         void slice_header(vaBitstream *bs);
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
       
        bool build_packed_slice_buffer(vaBitstream *bs);
         int build_packed_slice_buffer(unsigned char **header_buffer);
         void encoding2display_order(    uint64_t encoding_order,int intra_period,    int intra_idr_period,int ip_period,    uint64_t *displaying_order,    int *frame_type);
        int update_ReferenceFrames(void);
        int update_RefPicList(void);
        int render_sequence(void);
        int calc_poc(int pic_order_cnt_lsb);
        int render_picture(void);
        int render_packedsequence(void);
        int render_packedpicture(void);
        void render_packedsei(void);
        int render_hrd(void);
        void render_packedslice(void);
        int render_slice(void);
};
 
