/*
 * TTML subtitle muxer
 * Copyright (c) 2020 24i
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * TTML subtitle muxer
 * @see https://www.w3.org/TR/ttml1/
 * @see https://www.w3.org/TR/ttml2/
 * @see https://www.w3.org/TR/ttml-imsc/rec
 */

#include "avformat.h"
#include "internal.h"
#include "libavcodec/subtitle_common.h"
#include "libavutil/intreadwrite.h"

typedef struct ImageParam
{
    int index;
    uint64_t pts;
    uint64_t duration;
    // present definitely in percent unit
    int origin_x;
    int origin_y;
    int extent_x;
    int extent_y;
    struct ImageParam *next;
} ImageParam;

typedef struct TTMLContext
{
    int is_single_file;
    int is_image;
    int first_pkt;
    ImageParam *image_param_head;
} TTMLContext;

static ImageParam *add_new_image_param(TTMLContext *ttml, AVPacket *pkt)
{
    ImageParam *ret = (ImageParam *)av_malloc(sizeof(ImageParam));
    ret->pts = pkt->pts;
    ret->duration = pkt->duration;
    ret->origin_x = AV_RB32(pkt->data + 1);
    ret->origin_y = AV_RB32(pkt->data + 5);
    ret->extent_x = AV_RB32(pkt->data + 9);
    ret->extent_y = AV_RB32(pkt->data + 13);
    ret->next = NULL;
    if (ttml->image_param_head)
    {
        ImageParam *curr = ttml->image_param_head;
        while (curr->next)
        {
            curr = curr->next;
        }
        ret->index = curr->index + 1;
        curr->next = ret;
    }
    else
    {
        ret->index = 0;
        ttml->image_param_head = ret;
    }
    return ret;
}

static void release_image_param(TTMLContext *ttml)
{
    ImageParam *curr = ttml->image_param_head;
    while (curr)
    {
        ImageParam *tmp = curr;
        curr = curr->next;
        av_freep(&tmp);
    }
    ttml->image_param_head = NULL;
}

static int ttml_write_header(AVFormatContext *ctx)
{
    TTMLContext *ttml = ctx->priv_data;
    av_log(NULL, AV_LOG_INFO, "ykuta %s\n", __func__);
    if (ctx->nb_streams != 1 ||
        ctx->streams[0]->codecpar->codec_id != AV_CODEC_ID_TTML)
    {
        av_log(ctx, AV_LOG_ERROR, "Exactly one TTML stream is required!\n");
        return AVERROR(EINVAL);
    }

    if (strlen(ctx->url))
        ttml->is_single_file = 1;
    else
        ttml->is_single_file = 0;

    if (ttml->is_single_file)
    {
        ttml->first_pkt = 1;
        ttml->image_param_head = NULL;
    }

    return 0;
}

static int ttml_write_packet(AVFormatContext *ctx, AVPacket *pkt)
{
    av_log(NULL, AV_LOG_INFO, "ykuta %s\n", __func__);
    TTMLContext *ttml = ctx->priv_data;
    AVStream *s = ctx->streams[0];
    AVIOContext *pb = ctx->pb;
    AVDictionaryEntry *lang = av_dict_get(s->metadata, "language", NULL, 0);
    const char *printed_lang = (lang && lang->value) ? lang->value : NULL;

    if (ttml->is_single_file)
    {
        if (ttml->first_pkt)
        {
            int type = AV_RB8(pkt->data);
            ttml->is_image = (type == SUBTITLE_BITMAP) ? 1 : 0;
            if (ttml->is_image)
            {
                ttml_image_write_tt_open(pb, printed_lang);
                ttml_write_head_open(pb);
                ttml_write_metadata_open(pb);
            }
            else
            {
                ttml_text_write_tt_open(pb, printed_lang);
                ttml_text_write_head(pb);
                ttml_write_body_open(pb);
                ttml_text_write_div_open(pb);
            }
            ttml->first_pkt = 0;
        }
        if (ttml->is_image)
        {
            ImageParam *image_param = add_new_image_param(ttml, pkt);
            ttml_image_write_smpte(pb, pkt, image_param->index);
        }
        else
        {
            ttml_text_write_p(pb, pkt, s->time_base);
        }
    }
    else
    {
        ttml_write_pkt(pb, pkt, printed_lang, s->time_base);
    }

    return 0;
}

static int ttml_write_trailer(AVFormatContext *ctx)
{
    av_log(NULL, AV_LOG_INFO, "ykuta %s\n", __func__);
    AVIOContext *pb = ctx->pb;
    AVStream *s = ctx->streams[0];
    TTMLContext *ttml = ctx->priv_data;
    if (ttml->is_single_file)
    {
        if (ttml->is_image)
        {
            ImageParam *curr = ttml->image_param_head;
            ttml_write_metadata_close(pb);
            ttml_write_layout_open(pb);
            while (curr)
            {
                ttml_image_wirte_region(pb, curr->index, curr->origin_x, curr->origin_y, curr->extent_x, curr->extent_y);
                curr = curr->next;
            }
            ttml_write_layout_close(pb);
            ttml_write_head_close(pb);
            ttml_write_body_open(pb);
            curr = ttml->image_param_head;
            while (curr)
            {
                ttml_image_write_div(pb, curr->index, curr->pts, curr->duration, s->time_base);
                curr = curr->next;
            }
            ttml_write_body_close(pb);
            ttml_write_tt_close(pb);
            release_image_param(ttml);
        }
        else
        {
            ttml_text_write_div_close(pb);
            ttml_write_body_close(pb);
            ttml_write_tt_close(pb);
        }
    }

    return 0;
}

AVOutputFormat ff_ttml_muxer = {
    .name = "ttml",
    .long_name = NULL_IF_CONFIG_SMALL("TTML subtitle"),
    .extensions = "xml",
    .mime_type = "application/ttml+xml",
    .priv_data_size = sizeof(TTMLContext),
    .flags = AVFMT_GLOBALHEADER | AVFMT_VARIABLE_FPS | AVFMT_TS_NONSTRICT,
    .subtitle_codec = AV_CODEC_ID_TTML,
    .write_header = ttml_write_header,
    .write_packet = ttml_write_packet,
    .write_trailer = ttml_write_trailer,
};