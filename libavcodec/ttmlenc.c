/*
 * TTML subtitle encoder
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

/**
 * @file
 * TTML subtitle encoder
 * @see https://www.w3.org/TR/ttml1/
 * @see https://www.w3.org/TR/ttml2/
 * @see https://www.w3.org/TR/ttml-imsc/rec
 */

#include <stdarg.h>
#include "avcodec.h"
#include "libavutil/avstring.h"
#include "libavutil/bprint.h"
#include "libavutil/libattopng.h"
#include "libavutil/base64.h"
#include "libavutil/intreadwrite.h"
#include "ass_split.h"
#include "ass.h"
#include <time.h>


typedef struct
{
    AVCodecContext *avctx;
    ASSSplitContext *ass_ctx;
    AVBPrint buffer;
} TTMLContext;

static void ttml_text_cb(void *priv, const char *text, int len)
{
    TTMLContext *s = priv;
    AVBPrint cur_line = {0};
    AVBPrint *buffer = &s->buffer;

    av_bprint_init(&cur_line, len, AV_BPRINT_SIZE_UNLIMITED);

    av_bprint_append_data(&cur_line, text, len);
    if (!av_bprint_is_complete(&cur_line))
    {
        av_log(s->avctx, AV_LOG_ERROR,
               "Failed to move the current subtitle dialog to AVBPrint!\n");
        av_bprint_finalize(&cur_line, NULL);
        return;
    }

    av_bprint_escape(buffer, cur_line.str, NULL, AV_ESCAPE_MODE_XML, 0);

    av_bprint_finalize(&cur_line, NULL);
}

static void ttml_new_line_cb(void *priv, int forced)
{
    TTMLContext *s = priv;

    av_bprintf(&s->buffer, "<br/>");
}

static const ASSCodesCallbacks ttml_callbacks = {
    .text = ttml_text_cb,
    .new_line = ttml_new_line_cb,
};

static int ttml_encode_frame(AVCodecContext *avctx, uint8_t *buf,
                             int bufsize, const AVSubtitle *sub)
{
    av_log(NULL, AV_LOG_INFO, "ykuta %s\n", __func__);
    TTMLContext *s = avctx->priv_data;
    ASSDialog *dialog;
    int i;

    // for bitmap subtitle
    int has_bitmap = 0;
    int x_min = INT_MAX, y_min = INT_MAX, x_max = 0, y_max = 0;

    av_bprint_clear(&s->buffer);

    for (i = 0; i < sub->num_rects; i++)
    {
        if (sub->rects[i]->type == SUBTITLE_ASS)
        {
            if (!s->ass_ctx)
                return AVERROR(ENOEXEC);
            const char *ass = sub->rects[i]->ass;
#if FF_API_ASS_TIMING
            if (!strncmp(ass, "Dialogue: ", 10))
            {
                int num;
                dialog = ff_ass_split_dialog(s->ass_ctx, ass, 0, &num);

                for (; dialog && num--; dialog++)
                {
                    ff_ass_split_override_codes(&ttml_callbacks, s,
                                                dialog->text);
                }
            }
            else
            {
#endif
                dialog = ff_ass_split_dialog2(s->ass_ctx, ass);
                if (!dialog)
                    return AVERROR(ENOMEM);

                ff_ass_split_override_codes(&ttml_callbacks, s, dialog->text);
                ff_ass_free_dialog(&dialog);
#if FF_API_ASS_TIMING
            }
#endif
        }
        else if (sub->rects[i]->type == SUBTITLE_BITMAP)
        {
            has_bitmap = 1;
            x_min = FFMIN(x_min, sub->rects[i]->x);
            y_min = FFMIN(y_min, sub->rects[i]->y);
            x_max = FFMAX(x_max, sub->rects[i]->x + sub->rects[i]->w - 1);
            y_max = FFMAX(y_max, sub->rects[i]->y + sub->rects[i]->h - 1);
        }
        else if (sub->rects[i]->type == SUBTITLE_TEXT)
        {
            ttml_text_cb(s, sub->rects[i]->text, strlen(sub->rects[i]->text));
        }
        else
        {
            av_log(avctx, AV_LOG_ERROR, "SUBTITLE_NONE was detected and this type was supported.\n");
            return AVERROR(ENOSYS);
        }
    }

    bufsize -= 1;   // first byte for specify profile text or image
    if (has_bitmap) // when subtitle bitmap
    {
        bufsize -= 16;  // the next 16 bytes for position of image
        /**
         * If video dimesions was not specified, set default to FullHD 1920*1080
         */ 
        int video_width = sub->video_width;
        if (video_width <= 0)
            video_width = 1920;
        int video_height = sub->video_height;
        if (video_height <= 0)
            video_height = 1080;
        clock_t begin = clock();
        int count, b64_size;
        uint8_t *data;
        libattopng_t *png = libattopng_new(x_max - x_min + 1, y_max - y_min + 1, PNG_GRAYSCALE);

        for (i = 0; i < sub->num_rects; i++)
        {
            int _x, _y;
            int delta_x = sub->rects[i]->x - x_min;
            int delta_y = sub->rects[i]->y - y_min;
            int color_mark = 256 / sub->rects[i]->nb_colors;
            count = 0;
            data = sub->rects[i]->data[0];
            for (_y = 0; _y < sub->rects[i]->h; _y++)
            {
                for (_x = 0; _x < sub->rects[i]->w; _x++)
                {
                    libattopng_set_pixel(png, _x + delta_x, _y + delta_y, data[count] * color_mark);
                    count++;
                }
            }
        }
        data = libattopng_get_data(png, &count);
        b64_size = AV_BASE64_SIZE(count);
        if (b64_size > bufsize)
        {
            av_log(avctx, AV_LOG_ERROR, "Buffer too small for Bitmap subtitle event.\n");
            libattopng_destroy(png);
            return -1;
        }
        
        AV_WB8(buf, SUBTITLE_BITMAP);
        AV_WB32(buf + 1, (int32_t)(100 * x_min / video_width));
        AV_WB32(buf + 5, (int32_t)(100 * y_min / video_height));
        AV_WB32(buf + 9, (int32_t)(100 * (x_max - x_min + 1) / video_width));
        AV_WB32(buf + 13, (int32_t)(100 * (y_max - y_min + 1) / video_height));
        av_base64_encode(buf + 17, b64_size, data, count);
        libattopng_destroy(png);
        clock_t end = clock();
        av_log(NULL,AV_LOG_INFO, "ykuta time convert bitmap to png: %lfms\n", (double)(end - begin) * 1000 / CLOCKS_PER_SEC);
        return b64_size + 16; // ignore NULL terminator
    }
    else // case SUBTITLE_ASS or SUBTITLE_TEXT
    {
        if (!av_bprint_is_complete(&s->buffer))
            return AVERROR(ENOMEM);
        if (!s->buffer.len)
            return 0;

        if (s->buffer.len > bufsize)
        {
            av_log(avctx, AV_LOG_ERROR, "Buffer too small for ASS event.\n");
            return -1;
        }
        AV_WB8(buf, SUBTITLE_TEXT);
        memcpy(buf + 1, s->buffer.str, s->buffer.len);

        return s->buffer.len + 1;
    }
}

static av_cold int ttml_encode_close(AVCodecContext *avctx)
{
    TTMLContext *s = avctx->priv_data;
    ff_ass_split_free(s->ass_ctx);
    av_bprint_finalize(&s->buffer, NULL);
    return 0;
}

static av_cold int ttml_encode_init(AVCodecContext *avctx)
{
    av_log(NULL, AV_LOG_INFO, "ykuta %s\n", __func__);
    TTMLContext *s = avctx->priv_data;
    s->avctx = avctx;
    if (avctx->subtitle_header)
        s->ass_ctx = ff_ass_split(avctx->subtitle_header);
    av_bprint_init(&s->buffer, 0, AV_BPRINT_SIZE_UNLIMITED);
    return 0;
}

AVCodec ff_ttml_encoder = {
    .name           = "ttml",
    .long_name      = NULL_IF_CONFIG_SMALL("TTML subtitle"),
    .type           = AVMEDIA_TYPE_SUBTITLE,
    .id             = AV_CODEC_ID_TTML,
    .priv_data_size = sizeof(TTMLContext),
    .init           = ttml_encode_init,
    .encode_sub     = ttml_encode_frame,
    .close          = ttml_encode_close,
};