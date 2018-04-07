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

/**
 * 
 * @param bs
 */
 void ADM_vaEncodingContextH264::sps_rbsp(vaBitstream *bs)
{
    int profile_idc = PROFILE_IDC_BASELINE;

    if (h264_profile  == VAProfileH264High)
        profile_idc = PROFILE_IDC_HIGH;
    else if (h264_profile  == VAProfileH264Main)
        profile_idc = PROFILE_IDC_MAIN;

    bs->put_ui( profile_idc, 8);               /* profile_idc */
    bs->put_ui( !!(constraint_set_flag & 1), 1);                         /* constraint_set0_flag */
    bs->put_ui( !!(constraint_set_flag & 2), 1);                         /* constraint_set1_flag */
    bs->put_ui( !!(constraint_set_flag & 4), 1);                         /* constraint_set2_flag */
    bs->put_ui( !!(constraint_set_flag & 8), 1);                         /* constraint_set3_flag */
    bs->put_ui( 0, 4);                         /* reserved_zero_4bits */
    bs->put_ui( seq_param.level_idc, 8);      /* level_idc */
    bs->put_ue( seq_param.seq_parameter_set_id);      /* seq_parameter_set_id */

    if ( profile_idc == PROFILE_IDC_HIGH) {
        bs->put_ue(  1);        /* chroma_format_idc = 1, 4:2:0 */ 
        bs->put_ue(  0);        /* bit_depth_luma_minus8 */
        bs->put_ue(  0);        /* bit_depth_chroma_minus8 */
        bs->put_ui( 0, 1);     /* qpprime_y_zero_transform_bypass_flag */
        bs->put_ui( 0, 1);     /* seq_scaling_matrix_present_flag */
    }

    bs->put_ue(  seq_param.seq_fields.bits.log2_max_frame_num_minus4); /* log2_max_frame_num_minus4 */
    bs->put_ue(  seq_param.seq_fields.bits.pic_order_cnt_type);        /* pic_order_cnt_type */

    if (seq_param.seq_fields.bits.pic_order_cnt_type == 0)
        bs->put_ue(  seq_param.seq_fields.bits.log2_max_pic_order_cnt_lsb_minus4);     /* log2_max_pic_order_cnt_lsb_minus4 */
    else {
        assert(0);
    }

    bs->put_ue(  seq_param.max_num_ref_frames);        /* num_ref_frames */
    bs->put_ui( 0, 1);                                 /* gaps_in_frame_num_value_allowed_flag */

    bs->put_ue( seq_param.picture_width_in_mbs - 1);  /* pic_width_in_mbs_minus1 */
    bs->put_ue(  seq_param.picture_height_in_mbs - 1); /* pic_height_in_map_units_minus1 */
    bs->put_ui( seq_param.seq_fields.bits.frame_mbs_only_flag, 1);    /* frame_mbs_only_flag */

    if (!seq_param.seq_fields.bits.frame_mbs_only_flag) {
        assert(0);
    }

    bs->put_ui( seq_param.seq_fields.bits.direct_8x8_inference_flag, 1);      /* direct_8x8_inference_flag */
    bs->put_ui( seq_param.frame_cropping_flag, 1);            /* frame_cropping_flag */

    if (seq_param.frame_cropping_flag) {
        bs->put_ue(  seq_param.frame_crop_left_offset);        /* frame_crop_left_offset */
        bs->put_ue(  seq_param.frame_crop_right_offset);       /* frame_crop_right_offset */
        bs->put_ue(  seq_param.frame_crop_top_offset);         /* frame_crop_top_offset */
        bs->put_ue(  seq_param.frame_crop_bottom_offset);      /* frame_crop_bottom_offset */
    }
    
    //if ( frame_bit_rate < 0 ) { //TODO EW: the vui header isn't correct
    if ( 1 ) {
        bs->put_ui(  0, 1); /* vui_parameters_present_flag */
    } else {
        bs->put_ui( 1, 1); /* vui_parameters_present_flag */
        bs->put_ui( 0, 1); /* aspect_ratio_info_present_flag */
        bs->put_ui( 0, 1); /* overscan_info_present_flag */
        bs->put_ui( 0, 1); /* video_signal_type_present_flag */
        bs->put_ui( 0, 1); /* chroma_loc_info_present_flag */
        bs->put_ui( 1, 1); /* timing_info_present_flag */
        {
            bs->put_ui( 15, 32);
            bs->put_ui( 900, 32);
            bs->put_ui( 1, 1);
        }
        bs->put_ui( 1, 1); /* nal_hrd_parameters_present_flag */
        {
            // hrd_parameters 
            bs->put_ue(  0);    /* cpb_cnt_minus1 */
            bs->put_ui( 4, 4); /* bit_rate_scale */
            bs->put_ui( 6, 4); /* cpb_size_scale */
           
            bs->put_ue( frame_bitrate - 1); /* bit_rate_value_minus1[0] */
            bs->put_ue( frame_bitrate*8 - 1); /* cpb_size_value_minus1[0] */
            bs->put_ui( 1, 1);  /* cbr_flag[0] */

            bs->put_ui( 23, 5);   /* initial_cpb_removal_delay_length_minus1 */
            bs->put_ui( 23, 5);   /* cpb_removal_delay_length_minus1 */
            bs->put_ui( 23, 5);   /* dpb_output_delay_length_minus1 */
            bs->put_ui( 23, 5);   /* time_offset_length  */
        }
        bs->put_ui( 0, 1);   /* vcl_hrd_parameters_present_flag */
        bs->put_ui( 0, 1);   /* low_delay_hrd_flag */ 

        bs->put_ui( 0, 1); /* pic_struct_present_flag */
        bs->put_ui( 0, 1); /* bitstream_restriction_flag */
    }
    bs->rbspTrailingBits();
         /* rbsp_trailing_bits */
}

/**
 * 
 * @param bs
 */
void ADM_vaEncodingContextH264::pps_rbsp(vaBitstream *bs)
{
    bs->put_ue( pic_param.pic_parameter_set_id);      /* pic_parameter_set_id */
    bs->put_ue( pic_param.seq_parameter_set_id);      /* seq_parameter_set_id */

    bs->put_ui( pic_param.pic_fields.bits.entropy_coding_mode_flag, 1);  /* entropy_coding_mode_flag */

    bs->put_ui(0, 1);                         /* pic_order_present_flag: 0 */

    bs->put_ue( 0);                            /* num_slice_groups_minus1 */

    bs->put_ue(pic_param.num_ref_idx_l0_active_minus1);      /* num_ref_idx_l0_active_minus1 */
    bs->put_ue( pic_param.num_ref_idx_l1_active_minus1);      /* num_ref_idx_l1_active_minus1 1 */

    bs->put_ui(pic_param.pic_fields.bits.weighted_pred_flag, 1);     /* weighted_pred_flag: 0 */
    bs->put_ui(pic_param.pic_fields.bits.weighted_bipred_idc, 2);	/* weighted_bipred_idc: 0 */

    bs->put_se( pic_param.pic_init_qp - 26);  /* pic_init_qp_minus26 */
    bs->put_se( 0);                            /* pic_init_qs_minus26 */
    bs->put_se( 0);                            /* chroma_qp_index_offset */

    bs->put_ui( pic_param.pic_fields.bits.deblocking_filter_control_present_flag, 1); /* deblocking_filter_control_present_flag */
    bs->put_ui( 0, 1);                         /* constrained_intra_pred_flag */
    bs->put_ui( 0, 1);                         /* redundant_pic_cnt_present_flag */
    
    /* more_rbsp_data */
    bs->put_ui( pic_param.pic_fields.bits.transform_8x8_mode_flag, 1);    /*transform_8x8_mode_flag */
    bs->put_ui( 0, 1);                         /* pic_scaling_matrix_present_flag */
    bs->put_se( pic_param.second_chroma_qp_index_offset );    /*second_chroma_qp_index_offset */

    bs->rbspTrailingBits();
}
/**
 * 
 * @param bs
 */
void ADM_vaEncodingContextH264::slice_header(bitstream *bs)
{
    int first_mb_in_slice = slice_param.macroblock_address;

    bitstream_put_ue(bs, first_mb_in_slice);        /* first_mb_in_slice: 0 */
    bitstream_put_ue(bs, slice_param.slice_type);   /* slice_type */
    bitstream_put_ue(bs, slice_param.pic_parameter_set_id);        /* pic_parameter_set_id: 0 */
    bitstream_put_ui(bs, pic_param.frame_num, seq_param.seq_fields.bits.log2_max_frame_num_minus4 + 4); /* frame_num */

    /* frame_mbs_only_flag == 1 */
    if (!seq_param.seq_fields.bits.frame_mbs_only_flag) {
        /* FIXME: */
        assert(0);
    }

    if (pic_param.pic_fields.bits.idr_pic_flag)
        bitstream_put_ue(bs, slice_param.idr_pic_id);		/* idr_pic_id: 0 */

    if (seq_param.seq_fields.bits.pic_order_cnt_type == 0) {
        bitstream_put_ui(bs, pic_param.CurrPic.TopFieldOrderCnt, seq_param.seq_fields.bits.log2_max_pic_order_cnt_lsb_minus4 + 4);
        /* pic_order_present_flag == 0 */
    } else {
        /* FIXME: */
        assert(0);
    }

    /* redundant_pic_cnt_present_flag == 0 */
    /* slice type */
    if (IS_P_SLICE(slice_param.slice_type)) {
        bitstream_put_ui(bs, slice_param.num_ref_idx_active_override_flag, 1);            /* num_ref_idx_active_override_flag: */

        if (slice_param.num_ref_idx_active_override_flag)
            bitstream_put_ue(bs, slice_param.num_ref_idx_l0_active_minus1);

        /* ref_pic_list_reordering */
        bitstream_put_ui(bs, 0, 1);            /* ref_pic_list_reordering_flag_l0: 0 */
    } else if (IS_B_SLICE(slice_param.slice_type)) {
        bitstream_put_ui(bs, slice_param.direct_spatial_mv_pred_flag, 1);            /* direct_spatial_mv_pred: 1 */

        bitstream_put_ui(bs, slice_param.num_ref_idx_active_override_flag, 1);       /* num_ref_idx_active_override_flag: */

        if (slice_param.num_ref_idx_active_override_flag) {
            bitstream_put_ue(bs, slice_param.num_ref_idx_l0_active_minus1);
            bitstream_put_ue(bs, slice_param.num_ref_idx_l1_active_minus1);
        }

        /* ref_pic_list_reordering */
        bitstream_put_ui(bs, 0, 1);            /* ref_pic_list_reordering_flag_l0: 0 */
        bitstream_put_ui(bs, 0, 1);            /* ref_pic_list_reordering_flag_l1: 0 */
    }

    if ((pic_param.pic_fields.bits.weighted_pred_flag &&
         IS_P_SLICE(slice_param.slice_type)) ||
        ((pic_param.pic_fields.bits.weighted_bipred_idc == 1) &&
         IS_B_SLICE(slice_param.slice_type))) {
        /* FIXME: fill weight/offset table */
        assert(0);
    }

    /* dec_ref_pic_marking */
    if (pic_param.pic_fields.bits.reference_pic_flag) {     /* nal_ref_idc != 0 */
        unsigned char no_output_of_prior_pics_flag = 0;
        unsigned char long_term_reference_flag = 0;
        unsigned char adaptive_ref_pic_marking_mode_flag = 0;

        if (pic_param.pic_fields.bits.idr_pic_flag) {
            bitstream_put_ui(bs, no_output_of_prior_pics_flag, 1);            /* no_output_of_prior_pics_flag: 0 */
            bitstream_put_ui(bs, long_term_reference_flag, 1);            /* long_term_reference_flag: 0 */
        } else {
            bitstream_put_ui(bs, adaptive_ref_pic_marking_mode_flag, 1);            /* adaptive_ref_pic_marking_mode_flag: 0 */
        }
    }

    if (pic_param.pic_fields.bits.entropy_coding_mode_flag &&
        !IS_I_SLICE(slice_param.slice_type))
        bitstream_put_ue(bs, slice_param.cabac_init_idc);               /* cabac_init_idc: 0 */

    bitstream_put_se(bs, slice_param.slice_qp_delta);                   /* slice_qp_delta: 0 */

    /* ignore for SP/SI */

    if (pic_param.pic_fields.bits.deblocking_filter_control_present_flag) {
        bitstream_put_ue(bs, slice_param.disable_deblocking_filter_idc);           /* disable_deblocking_filter_idc: 0 */

        if (slice_param.disable_deblocking_filter_idc != 1) {
            bitstream_put_se(bs, slice_param.slice_alpha_c0_offset_div2);          /* slice_alpha_c0_offset_div2: 2 */
            bitstream_put_se(bs, slice_param.slice_beta_offset_div2);              /* slice_beta_offset_div2: 2 */
        }
    }

    if (pic_param.pic_fields.bits.entropy_coding_mode_flag) {
        bitstream_byte_aligning(bs, 1);
    }
}
/**
 * 
 * @param header_buffer
 * @return 
 */
bool ADM_vaEncodingContextH264::build_packed_pic_buffer(vaBitstream *bs)
{
    bs->startCodePrefix();
    bs->nalHeader(NAL_REF_IDC_HIGH, NAL_PPS);
    pps_rbsp(bs);
    bs->startCodePrefix();
    return true; // Offset
}
/**
 * 
 * @param header_buffer
 * @return 
 */
 bool ADM_vaEncodingContextH264::build_packed_seq_buffer(vaBitstream *bs)
{

    bs->startCodePrefix(); 
    
    bs->nalHeader( NAL_REF_IDC_HIGH, NAL_SPS);
    sps_rbsp(bs);
    bs->stop();
    return true;
}

/**
 * 
 * @param init_cpb_removal_length
 * @param init_cpb_removal_delay
 * @param init_cpb_removal_delay_offset
 * @param cpb_removal_length
 * @param cpb_removal_delay
 * @param dpb_output_length
 * @param dpb_output_delay
 * @param sei_buffer
 * @return 
 */  
 int ADM_vaEncodingContextH264::build_packed_sei_buffer_timing(unsigned int init_cpb_removal_length,
				unsigned int init_cpb_removal_delay,
				unsigned int init_cpb_removal_delay_offset,
				unsigned int cpb_removal_length,
				unsigned int cpb_removal_delay,
				unsigned int dpb_output_length,
				unsigned int dpb_output_delay,
				unsigned char **sei_buffer)
{
    unsigned char *byte_buf;
    int bp_byte_size, i, pic_byte_size;

    bitstream nal_bs;
    bitstream sei_bp_bs, sei_pic_bs;

    bitstream_start(&sei_bp_bs);
    bitstream_put_ue(&sei_bp_bs, 0);       /*seq_parameter_set_id*/
    bitstream_put_ui(&sei_bp_bs, init_cpb_removal_delay, cpb_removal_length); 
    bitstream_put_ui(&sei_bp_bs, init_cpb_removal_delay_offset, cpb_removal_length); 
    if ( sei_bp_bs.bit_offset & 0x7) {
        bitstream_put_ui(&sei_bp_bs, 1, 1);
    }
    bitstream_end(&sei_bp_bs);
    bp_byte_size = (sei_bp_bs.bit_offset + 7) / 8;
    
    bitstream_start(&sei_pic_bs);
    bitstream_put_ui(&sei_pic_bs, cpb_removal_delay, cpb_removal_length); 
    bitstream_put_ui(&sei_pic_bs, dpb_output_delay, dpb_output_length); 
    if ( sei_pic_bs.bit_offset & 0x7) {
        bitstream_put_ui(&sei_pic_bs, 1, 1);
    }
    bitstream_end(&sei_pic_bs);
    pic_byte_size = (sei_pic_bs.bit_offset + 7) / 8;
    
    bitstream_start(&nal_bs);
    nal_start_code_prefix(&nal_bs);
    nal_header(&nal_bs, NAL_REF_IDC_NONE, NAL_SEI);

	/* Write the SEI buffer period data */    
    bitstream_put_ui(&nal_bs, 0, 8);
    bitstream_put_ui(&nal_bs, bp_byte_size, 8);
    
    byte_buf = (unsigned char *)sei_bp_bs.buffer;
    for(i = 0; i < bp_byte_size; i++) {
        bitstream_put_ui(&nal_bs, byte_buf[i], 8);
    }
    free(byte_buf);
	/* write the SEI timing data */
    bitstream_put_ui(&nal_bs, 0x01, 8);
    bitstream_put_ui(&nal_bs, pic_byte_size, 8);
    
    byte_buf = (unsigned char *)sei_pic_bs.buffer;
    for(i = 0; i < pic_byte_size; i++) {
        bitstream_put_ui(&nal_bs, byte_buf[i], 8);
    }
    free(byte_buf);

    rbsp_trailing_bits(&nal_bs);
    bitstream_end(&nal_bs);

    *sei_buffer = (unsigned char *)nal_bs.buffer; 
   
    return nal_bs.bit_offset;
}
/**
 * 
 * @param header_buffer
 * @return 
 */
  int ADM_vaEncodingContextH264::build_packed_slice_buffer(unsigned char **header_buffer)
{
    bitstream bs;
    int is_idr = !!pic_param.pic_fields.bits.idr_pic_flag;
    int is_ref = !!pic_param.pic_fields.bits.reference_pic_flag;

    bitstream_start(&bs);
    nal_start_code_prefix(&bs);

    if (IS_I_SLICE(slice_param.slice_type)) {
        nal_header(&bs, NAL_REF_IDC_HIGH, is_idr ? NAL_IDR : NAL_NON_IDR);
    } else if (IS_P_SLICE(slice_param.slice_type)) {
        nal_header(&bs, NAL_REF_IDC_MEDIUM, NAL_NON_IDR);
    } else {
        assert(IS_B_SLICE(slice_param.slice_type));
        nal_header(&bs, is_ref ? NAL_REF_IDC_LOW : NAL_REF_IDC_NONE, NAL_NON_IDR);
    }

    slice_header(&bs);
    bitstream_end(&bs);

    *header_buffer = (unsigned char *)bs.buffer;
    return bs.bit_offset;
}

/*
 * Return displaying order with specified periods and encoding order
 * displaying_order: displaying order
 * frame_type: frame type 
 */
void ADM_vaEncodingContextH264::encoding2display_order(    uint64_t encoding_order,int intra_period,    int intra_idr_period,int ip_period,    uint64_t *displaying_order,    int *frame_type)
{
    int encoding_order_gop = 0;

    if (intra_period == 1) 
    { /* all are I/IDR frames */
        *displaying_order = encoding_order;
        if (intra_idr_period == 0)
            *frame_type = (encoding_order == 0)?FRAME_IDR:FRAME_I;
        else
            *frame_type = (encoding_order % intra_idr_period == 0)?FRAME_IDR:FRAME_I;
        return;
    }

    if (intra_period == 0)
        intra_idr_period = 0;

    /* new sequence like
     * IDR PPPPP IPPPPP
     * IDR (PBB)(PBB)(IBB)(PBB)
     */
    encoding_order_gop = (intra_idr_period == 0)? encoding_order:
        (encoding_order % (intra_idr_period + ((ip_period == 1)?0:1)));
         
    if (encoding_order_gop == 0) { /* the first frame */
        *frame_type = FRAME_IDR;
        *displaying_order = encoding_order;
    } else if (((encoding_order_gop - 1) % ip_period) != 0) { /* B frames */
	*frame_type = FRAME_B;
        *displaying_order = encoding_order - 1;
    } else if ((intra_period != 0) && /* have I frames */
               (encoding_order_gop >= 2) &&
               ((ip_period == 1 && encoding_order_gop % intra_period == 0) || /* for IDR PPPPP IPPPP */
                /* for IDR (PBB)(PBB)(IBB) */
                (ip_period >= 2 && ((encoding_order_gop - 1) / ip_period % (intra_period / ip_period)) == 0))) {
	*frame_type = FRAME_I;
	*displaying_order = encoding_order + ip_period - 1;
    } else {
	*frame_type = FRAME_P;
	*displaying_order = encoding_order + ip_period - 1;
    }
}

// EOF