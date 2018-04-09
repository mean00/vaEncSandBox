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



#define partition(ref, field, key, ascending)   \
    while (i <= j) {                            \
        if (ascending) {                        \
            while (ref[i].field < key)          \
                i++;                            \
            while (ref[j].field > key)          \
                j--;                            \
        } else {                                \
            while (ref[i].field > key)          \
                i++;                            \
            while (ref[j].field < key)          \
                j--;                            \
        }                                       \
        if (i <= j) {                           \
            tmp = ref[i];                       \
            ref[i] = ref[j];                    \
            ref[j] = tmp;                       \
            i++;                                \
            j--;                                \
        }                                       \
    }                                           \


/**
 * 
 * @param ref
 * @param left
 * @param right
 * @param ascending
 * @param frame_idx
 */
static void sort_one(VAPictureH264 ref[], int left, int right,
                     int ascending, int frame_idx)
{
    int i = left, j = right;
    unsigned int key;
    VAPictureH264 tmp;

    if (frame_idx)
    {
        key = ref[(left + right) / 2].frame_idx;
        partition(ref, frame_idx, key, ascending);
    }
    else
    {
        key = ref[(left + right) / 2].TopFieldOrderCnt;
        partition(ref, TopFieldOrderCnt, (signed int) key, ascending);
    }

    /* recursion */
    if (left < j)
        sort_one(ref, left, j, ascending, frame_idx);

    if (i < right)
        sort_one(ref, i, right, ascending, frame_idx);
}

/**
 * 
 * @param ref
 * @param left
 * @param right
 * @param key
 * @param frame_idx
 * @param partition_ascending
 * @param list0_ascending
 * @param list1_ascending
 */
static void sort_two(VAPictureH264 ref[], int left, int right, unsigned int key, unsigned int frame_idx,
                     int partition_ascending, int list0_ascending, int list1_ascending)
{
    int i = left, j = right;
    VAPictureH264 tmp;

    if (frame_idx)
    {
        partition(ref, frame_idx, key, partition_ascending);
    }
    else
    {
        partition(ref, TopFieldOrderCnt, (signed int) key, partition_ascending);
    }


    sort_one(ref, left, i - 1, list0_ascending, frame_idx);
    sort_one(ref, j + 1, right, list1_ascending, frame_idx);
}

/**
 * 
 * @return 
 */
bool ADM_vaEncodingContextH264::update_ReferenceFrames(void)
{
    int i;

    if (current_frame_type == FRAME_B)
        return 0;

    CurrentCurrPic.flags = VA_PICTURE_H264_SHORT_TERM_REFERENCE;
    numShortTerm++;
    if (numShortTerm > num_ref_frames)
        numShortTerm = num_ref_frames;
    for (i = numShortTerm - 1; i > 0; i--)
        ReferenceFrames[i] = ReferenceFrames[i - 1];
    ReferenceFrames[0] = CurrentCurrPic;

    return true;
}

/**
 * 
 * @return 
 */
bool ADM_vaEncodingContextH264::update_RefPicList(void)
{
    unsigned int current_poc = CurrentCurrPic.TopFieldOrderCnt;

    if (current_frame_type == FRAME_P)
    {
        memcpy(RefPicList0_P, ReferenceFrames, numShortTerm * sizeof (VAPictureH264));
        sort_one(RefPicList0_P, 0, numShortTerm - 1, 0, 1);
    }

    if (current_frame_type == FRAME_B)
    {
        memcpy(RefPicList0_B, ReferenceFrames, numShortTerm * sizeof (VAPictureH264));
        sort_two(RefPicList0_B, 0, numShortTerm - 1, current_poc, 0,
                 1, 0, 1);

        memcpy(RefPicList1_B, ReferenceFrames, numShortTerm * sizeof (VAPictureH264));
        sort_two(RefPicList1_B, 0, numShortTerm - 1, current_poc, 0,
                 0, 1, 0);
    }

    return true;
}

/**
 * 
 * @return 
 */
bool ADM_vaEncodingContextH264::render_sequence(void)
{
    VABufferID seq_param_buf, rc_param_buf, misc_param_tmpbuf, render_id[2];
    VAStatus va_status;
    VAEncMiscParameterBuffer *misc_param, *misc_param_tmp;
    VAEncMiscParameterRateControl *misc_rate_ctrl;

    seq_param.level_idc = 41 /*SH_LEVEL_3*/;
    seq_param.picture_width_in_mbs = frame_width_mbaligned / 16;
    seq_param.picture_height_in_mbs = frame_height_mbaligned / 16;
    seq_param.bits_per_second = frame_bitrate;

    seq_param.intra_period = 30; // Hardcoded ? 
#warning fixme
    seq_param.intra_idr_period = intra_idr_period;
    seq_param.ip_period = ip_period;
#warning fixme too
    seq_param.max_num_ref_frames = num_ref_frames;
    seq_param.seq_fields.bits.frame_mbs_only_flag = 1;
    seq_param.time_scale = 900;
    seq_param.num_units_in_tick = 15; /* Tc = num_units_in_tick / time_sacle */
    seq_param.seq_fields.bits.log2_max_pic_order_cnt_lsb_minus4 = Log2MaxPicOrderCntLsb - 4;
    seq_param.seq_fields.bits.log2_max_frame_num_minus4 = Log2MaxFrameNum - 4;
    ;
    seq_param.seq_fields.bits.frame_mbs_only_flag = 1;
    seq_param.seq_fields.bits.chroma_format_idc = 1;
    seq_param.seq_fields.bits.direct_8x8_inference_flag = 1;
    if (frame_width != frame_width_mbaligned ||
            frame_height != frame_height_mbaligned)
    {
        seq_param.frame_cropping_flag = 1;
        seq_param.frame_crop_left_offset = 0;
        seq_param.frame_crop_right_offset = (frame_width_mbaligned - frame_width) / 2;
        seq_param.frame_crop_top_offset = 0;
        seq_param.frame_crop_bottom_offset = (frame_height_mbaligned - frame_height) / 2;
    }
    CHECK_VA_STATUS_BOOL(vaCreateBuffer(admLibVA::getDisplay(), context_id,
                                        VAEncSequenceParameterBufferType,
                                        sizeof (seq_param), 1, &seq_param, &seq_param_buf));


    CHECK_VA_STATUS_BOOL(vaCreateBuffer(admLibVA::getDisplay(), context_id,
                                        VAEncMiscParameterBufferType,
                                        sizeof (VAEncMiscParameterBuffer) + sizeof (VAEncMiscParameterRateControl),
                                        1, NULL, &rc_param_buf));


    vaMapBuffer(admLibVA::getDisplay(), rc_param_buf, (void **) &misc_param);
    misc_param->type = VAEncMiscParameterTypeRateControl;
    misc_rate_ctrl = (VAEncMiscParameterRateControl *) misc_param->data;
    memset(misc_rate_ctrl, 0, sizeof (*misc_rate_ctrl));
    misc_rate_ctrl->bits_per_second = frame_bitrate;
    misc_rate_ctrl->target_percentage = 66;
    misc_rate_ctrl->window_size = 1000;
    misc_rate_ctrl->initial_qp = initial_qp;
    misc_rate_ctrl->min_qp = minimal_qp;
    misc_rate_ctrl->basic_unit_size = 0;
    vaUnmapBuffer(admLibVA::getDisplay(), rc_param_buf);

    render_id[0] = seq_param_buf;
    render_id[1] = rc_param_buf;

    CHECK_VA_STATUS_BOOL(vaRenderPicture(admLibVA::getDisplay(), context_id, &render_id[0], 2));


   

    return true;
}

/**
 * 
 * @param pic_order_cnt_lsb
 * @return 
 */
int ADM_vaEncodingContextH264::calc_poc(int pic_order_cnt_lsb)
{
    static int PicOrderCntMsb_ref = 0, pic_order_cnt_lsb_ref = 0;
    int prevPicOrderCntMsb, prevPicOrderCntLsb;
    int PicOrderCntMsb, TopFieldOrderCnt;

    if (current_frame_type == FRAME_IDR)
        prevPicOrderCntMsb = prevPicOrderCntLsb = 0;
    else
    {
        prevPicOrderCntMsb = PicOrderCntMsb_ref;
        prevPicOrderCntLsb = pic_order_cnt_lsb_ref;
    }

    if ((pic_order_cnt_lsb < prevPicOrderCntLsb) &&
            ((prevPicOrderCntLsb - pic_order_cnt_lsb) >= (int) (MaxPicOrderCntLsb / 2)))
        PicOrderCntMsb = prevPicOrderCntMsb + MaxPicOrderCntLsb;
    else if ((pic_order_cnt_lsb > prevPicOrderCntLsb) &&
            ((pic_order_cnt_lsb - prevPicOrderCntLsb) > (int) (MaxPicOrderCntLsb / 2)))
        PicOrderCntMsb = prevPicOrderCntMsb - MaxPicOrderCntLsb;
    else
        PicOrderCntMsb = prevPicOrderCntMsb;

    TopFieldOrderCnt = PicOrderCntMsb + pic_order_cnt_lsb;

    if (current_frame_type != FRAME_B)
    {
        PicOrderCntMsb_ref = PicOrderCntMsb;
        pic_order_cnt_lsb_ref = pic_order_cnt_lsb;
    }

    return TopFieldOrderCnt;
}

/**
 * 
 * @return 
 */
bool ADM_vaEncodingContextH264::render_picture(void)
{
    VABufferID pic_param_buf;
    VAStatus va_status;
    int i = 0;
    int current_slot = (current_frame_encoding % SURFACE_NUM);
    pic_param.CurrPic.picture_id = vaRefSurface[current_slot]->surface;
    pic_param.CurrPic.frame_idx = current_frame_encoding;
    pic_param.CurrPic.flags = 0;
    pic_param.CurrPic.TopFieldOrderCnt = calc_poc((current_frame_encoding - gop_start) % MaxPicOrderCntLsb);
    pic_param.CurrPic.BottomFieldOrderCnt = pic_param.CurrPic.TopFieldOrderCnt;
    CurrentCurrPic = pic_param.CurrPic;

    if (getenv("TO_DEL"))
    { /* set RefPicList into ReferenceFrames */
        update_RefPicList(); /* calc RefPicList */
        memset(pic_param.ReferenceFrames, 0xff, 16 * sizeof (VAPictureH264)); /* invalid all */
        if (current_frame_type == FRAME_P)
        {
            pic_param.ReferenceFrames[0] = RefPicList0_P[0];
        }
        else if (current_frame_type == FRAME_B)
        {
            pic_param.ReferenceFrames[0] = RefPicList0_B[0];
            pic_param.ReferenceFrames[1] = RefPicList1_B[0];
        }
    }
    else
    {
        memcpy(pic_param.ReferenceFrames, ReferenceFrames, numShortTerm * sizeof (VAPictureH264));
        for (i = numShortTerm; i < SURFACE_NUM; i++)
        {
            pic_param.ReferenceFrames[i].picture_id = VA_INVALID_SURFACE;
            pic_param.ReferenceFrames[i].flags = VA_PICTURE_H264_INVALID;
        }
    }

    pic_param.pic_fields.bits.idr_pic_flag = (current_frame_type == FRAME_IDR);
    pic_param.pic_fields.bits.reference_pic_flag = (current_frame_type != FRAME_B);
    pic_param.pic_fields.bits.entropy_coding_mode_flag = 1;
    pic_param.pic_fields.bits.deblocking_filter_control_present_flag = 1;
    pic_param.frame_num = current_frame_encoding;
    pic_param.coded_buf = vaEncodingBuffers[current_slot]->getId();
    pic_param.last_picture = 0;
    pic_param.pic_init_qp = initial_qp;

    CHECK_VA_STATUS_BOOL(vaCreateBuffer(admLibVA::getDisplay(), context_id, VAEncPictureParameterBufferType,
                                        sizeof (pic_param), 1, &pic_param, &pic_param_buf));


    CHECK_VA_STATUS_BOOL(vaRenderPicture(admLibVA::getDisplay(), context_id, &pic_param_buf, 1));


    return true;
}

/**
 * 
 * @return 
 */
bool ADM_vaEncodingContextH264::render_packedsequence(void)
{
    VAEncPackedHeaderParameterBuffer packedheader_param_buffer;
    VABufferID packedseq_para_bufid, packedseq_data_bufid, render_id[2];
    unsigned int length_in_bits;

    VAStatus va_status;
    vaBitstream bs;

    build_packed_seq_buffer(&bs);
    length_in_bits = bs.lengthInBits();

    packedheader_param_buffer.type = VAEncPackedHeaderSequence;

    packedheader_param_buffer.bit_length = length_in_bits; /*length_in_bits*/
    packedheader_param_buffer.has_emulation_bytes = 0;
    CHECK_VA_STATUS_BOOL(vaCreateBuffer(admLibVA::getDisplay(),
                                        context_id,
                                        VAEncPackedHeaderParameterBufferType,
                                        sizeof (packedheader_param_buffer), 1, &packedheader_param_buffer,
                                        &packedseq_para_bufid));


    CHECK_VA_STATUS_BOOL(vaCreateBuffer(admLibVA::getDisplay(),
                                        context_id,
                                        VAEncPackedHeaderDataBufferType,
                                        (length_in_bits + 7) / 8, 1, bs.getPointer(),
                                        &packedseq_data_bufid));


    render_id[0] = packedseq_para_bufid;
    render_id[1] = packedseq_data_bufid;
    CHECK_VA_STATUS_BOOL(vaRenderPicture(admLibVA::getDisplay(), context_id, render_id, 2));

    return true;
}

/**
 * 
 * @return 
 */
bool ADM_vaEncodingContextH264::render_packedpicture(void)
{
    VAEncPackedHeaderParameterBuffer packedheader_param_buffer;
    VABufferID packedpic_para_bufid, packedpic_data_bufid, render_id[2];
    unsigned int length_in_bits;

    VAStatus va_status;
    vaBitstream bs;


    build_packed_pic_buffer(&bs);
    length_in_bits = bs.lengthInBits();
    packedheader_param_buffer.type = VAEncPackedHeaderPicture;
    packedheader_param_buffer.bit_length = length_in_bits;
    packedheader_param_buffer.has_emulation_bytes = 0;

    CHECK_VA_STATUS_BOOL(vaCreateBuffer(admLibVA::getDisplay(),
                                        context_id,
                                        VAEncPackedHeaderParameterBufferType,
                                        sizeof (packedheader_param_buffer), 1, &packedheader_param_buffer,
                                        &packedpic_para_bufid));


    CHECK_VA_STATUS_BOOL(vaCreateBuffer(admLibVA::getDisplay(),
                                        context_id,
                                        VAEncPackedHeaderDataBufferType,
                                        (length_in_bits + 7) / 8, 1, bs.getPointer(),
                                        &packedpic_data_bufid));


    render_id[0] = packedpic_para_bufid;
    render_id[1] = packedpic_data_bufid;
    CHECK_VA_STATUS_BOOL(vaRenderPicture(admLibVA::getDisplay(), context_id, render_id, 2));


    return true;
}

/**
 * 
 */
bool ADM_vaEncodingContextH264::render_packedsei(void)
{
    VAEncPackedHeaderParameterBuffer packed_header_param_buffer;
    VABufferID packed_sei_header_param_buf_id, packed_sei_buf_id, render_id[2];
    unsigned int length_in_bits /*offset_in_bytes*/;

    VAStatus va_status;
    vaBitstream bs;
    int init_cpb_size, target_bit_rate, i_initial_cpb_removal_delay_length, i_initial_cpb_removal_delay;
    int i_cpb_removal_delay, i_dpb_output_delay_length, i_cpb_removal_delay_length;

    /* it comes for the bps defined in SPS */
    target_bit_rate = frame_bitrate;
    init_cpb_size = (target_bit_rate * 8) >> 10;
    i_initial_cpb_removal_delay = init_cpb_size * 0.5 * 1024 / target_bit_rate * 90000;

    i_cpb_removal_delay = 2;
    i_initial_cpb_removal_delay_length = 24;
    i_cpb_removal_delay_length = 24;
    i_dpb_output_delay_length = 24;


    build_packed_sei_buffer_timing(&bs,
                                   i_initial_cpb_removal_delay_length,
                                   i_initial_cpb_removal_delay,
                                   0,
                                   i_cpb_removal_delay_length,
                                   i_cpb_removal_delay * current_frame_encoding,
                                   i_dpb_output_delay_length,
                                   0);
    length_in_bits = bs.lengthInBits();
    //offset_in_bytes = 0;
    packed_header_param_buffer.type = VAEncPackedHeaderH264_SEI;
    packed_header_param_buffer.bit_length = length_in_bits;
    packed_header_param_buffer.has_emulation_bytes = 0;

    CHECK_VA_STATUS_BOOL(vaCreateBuffer(admLibVA::getDisplay(),
                                        context_id,
                                        VAEncPackedHeaderParameterBufferType,
                                        sizeof (packed_header_param_buffer), 1, &packed_header_param_buffer,
                                        &packed_sei_header_param_buf_id));


    CHECK_VA_STATUS_BOOL(vaCreateBuffer(admLibVA::getDisplay(),
                                        context_id,
                                        VAEncPackedHeaderDataBufferType,
                                        (length_in_bits + 7) / 8, 1, bs.getPointer(),
                                        &packed_sei_buf_id));



    render_id[0] = packed_sei_header_param_buf_id;
    render_id[1] = packed_sei_buf_id;
    CHECK_VA_STATUS_BOOL(vaRenderPicture(admLibVA::getDisplay(), context_id, render_id, 2));

    return true;
}

/**
 * 
 * @return 
 */
bool ADM_vaEncodingContextH264::render_hrd(void)
{
    VABufferID misc_parameter_hrd_buf_id;
    VAStatus va_status;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterHRD *misc_hrd_param;

    CHECK_VA_STATUS_BOOL(vaCreateBuffer(admLibVA::getDisplay(), context_id,
                                        VAEncMiscParameterBufferType,
                                        sizeof (VAEncMiscParameterBuffer) + sizeof (VAEncMiscParameterHRD),
                                        1,
                                        NULL,
                                        &misc_parameter_hrd_buf_id));


    vaMapBuffer(admLibVA::getDisplay(),
                misc_parameter_hrd_buf_id,
                (void **) &misc_param);
    misc_param->type = VAEncMiscParameterTypeHRD;
    misc_hrd_param = (VAEncMiscParameterHRD *) misc_param->data;

    if (frame_bitrate > 0)
    {
        misc_hrd_param->initial_buffer_fullness = frame_bitrate * 1024 * 4;
        misc_hrd_param->buffer_size = frame_bitrate * 1024 * 8;
    }
    else
    {
        misc_hrd_param->initial_buffer_fullness = 0;
        misc_hrd_param->buffer_size = 0;
    }
    vaUnmapBuffer(admLibVA::getDisplay(), misc_parameter_hrd_buf_id);

    CHECK_VA_STATUS_BOOL(vaRenderPicture(admLibVA::getDisplay(), context_id, &misc_parameter_hrd_buf_id, 1));


    return true;
}

/**
 * 
 */
bool ADM_vaEncodingContextH264::render_packedslice()
{
    VAEncPackedHeaderParameterBuffer packedheader_param_buffer;
    VABufferID packedslice_para_bufid, packedslice_data_bufid, render_id[2];
    unsigned int length_in_bits;

    VAStatus va_status;
    vaBitstream bs;
    build_packed_slice_buffer(&bs);
    length_in_bits = bs.lengthInBits();

    packedheader_param_buffer.type = VAEncPackedHeaderSlice;
    packedheader_param_buffer.bit_length = length_in_bits;
    packedheader_param_buffer.has_emulation_bytes = 0;

    CHECK_VA_STATUS_BOOL(vaCreateBuffer(admLibVA::getDisplay(),
                                        context_id,
                                        VAEncPackedHeaderParameterBufferType,
                                        sizeof (packedheader_param_buffer), 1, &packedheader_param_buffer,
                                        &packedslice_para_bufid));


    CHECK_VA_STATUS_BOOL(vaCreateBuffer(admLibVA::getDisplay(),
                                        context_id,
                                        VAEncPackedHeaderDataBufferType,
                                        (length_in_bits + 7) / 8, 1, bs.getPointer(),
                                        &packedslice_data_bufid));


    render_id[0] = packedslice_para_bufid;
    render_id[1] = packedslice_data_bufid;
    CHECK_VA_STATUS_BOOL(vaRenderPicture(admLibVA::getDisplay(), context_id, render_id, 2));

    return true;
}

/**
 * 
 * @return 
 */
bool ADM_vaEncodingContextH264::render_slice(void)
{
    VABufferID slice_param_buf;
    VAStatus va_status;
    int i;

    update_RefPicList();

    /* one frame, one slice */
    slice_param.macroblock_address = 0;
    slice_param.num_macroblocks = frame_width_mbaligned * frame_height_mbaligned / (16 * 16); /* Measured by MB */
    slice_param.slice_type = (current_frame_type == FRAME_IDR) ? 2 : current_frame_type;
    if (current_frame_type == FRAME_IDR)
    {
        if (current_frame_encoding != 0)
            ++slice_param.idr_pic_id;
    }
    else if (current_frame_type == FRAME_P)
    {
        int refpiclist0_max = h264_maxref & 0xffff;
        memcpy(slice_param.RefPicList0, RefPicList0_P, refpiclist0_max * sizeof (VAPictureH264));

        for (i = refpiclist0_max; i < 32; i++)
        {
            slice_param.RefPicList0[i].picture_id = VA_INVALID_SURFACE;
            slice_param.RefPicList0[i].flags = VA_PICTURE_H264_INVALID;
        }
    }
    else if (current_frame_type == FRAME_B)
    {
        int refpiclist0_max = h264_maxref & 0xffff;
        int refpiclist1_max = (h264_maxref >> 16) & 0xffff;

        memcpy(slice_param.RefPicList0, RefPicList0_B, refpiclist0_max * sizeof (VAPictureH264));
        for (i = refpiclist0_max; i < 32; i++)
        {
            slice_param.RefPicList0[i].picture_id = VA_INVALID_SURFACE;
            slice_param.RefPicList0[i].flags = VA_PICTURE_H264_INVALID;
        }

        memcpy(slice_param.RefPicList1, RefPicList1_B, refpiclist1_max * sizeof (VAPictureH264));
        for (i = refpiclist1_max; i < 32; i++)
        {
            slice_param.RefPicList1[i].picture_id = VA_INVALID_SURFACE;
            slice_param.RefPicList1[i].flags = VA_PICTURE_H264_INVALID;
        }
    }

    slice_param.slice_alpha_c0_offset_div2 = 0;
    slice_param.slice_beta_offset_div2 = 0;
    slice_param.direct_spatial_mv_pred_flag = 1;
    slice_param.pic_order_cnt_lsb = (current_frame_encoding - gop_start) % MaxPicOrderCntLsb;


    if (h264_packedheader & VA_ENC_PACKED_HEADER_SLICE)
        render_packedslice();

    CHECK_VA_STATUS_BOOL(vaCreateBuffer(admLibVA::getDisplay(), context_id, VAEncSliceParameterBufferType,
                                        sizeof (slice_param), 1, &slice_param, &slice_param_buf));


    CHECK_VA_STATUS_BOOL(vaRenderPicture(admLibVA::getDisplay(), context_id, &slice_param_buf, 1));


    return true;
}

// EOF
