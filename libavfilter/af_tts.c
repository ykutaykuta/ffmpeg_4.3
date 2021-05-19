/*
 * Copyright (c) 2013 Paul B Mahol
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

#include "libavutil/avassert.h"
#include "libavutil/avstring.h"
#include "libavutil/opt.h"
#include "libavutil/samplefmt.h"
#include "avfilter.h"
#include "audio.h"
#include "internal.h"
#include "fftools/ffmpeg.h"
#include "af_tts.h"
#include "libavfilter/buffersrc.h"
#include "libavfilter/buffersink.h"
#include "libavutil/bprint.h"
#include "libavcodec/ass_split.h"
#include "libavcodec/ass.h"

typedef struct SigmaTTSContext {
    const AVClass *class;
	AVFilterGraph *graph;                                      /**< filtergraph for subtitle tts */
	FFFrameQueue sub_frame_fifo;                               /**< list of frame info for the first input */
	AVPacket *decoded_pkt;                                     /**< packet for processing */
	int sample_offset_s;                                       /**< offset of current sample in the first frame of queue */
	int sample_offset_a;                                       /**< offset of current sample in the mixing audio frame */
	void (*fc_mix)(void *ctx, AVFrame *frame);                 /**< function mix audio */
	char *server;
	float sub_volume;
	float aud_volume;
	float min_speed;
	float max_speed;
	char *subtitle;
	int discard;
	AVFilterLink *inlink;
	void *subtitle_handler;
    ASSSplitContext *ass_ctx;
    AVBPrint buffer;
    char buf_text[2048];
    char buf_url[2048];
} SigmaTTSContext;

#define OFFSET(x) offsetof(SigmaTTSContext, x)
#define A AV_OPT_FLAG_AUDIO_PARAM|AV_OPT_FLAG_FILTERING_PARAM

static const AVOption tts_options[] = {
    { "subtitle", "subtitle file path or map", OFFSET(subtitle), AV_OPT_TYPE_STRING, {.str=NULL}, 0, 0, A},
    { "server", "server url for api request", OFFSET(server), AV_OPT_TYPE_STRING, {.str=NULL}, 0, 0, A},
    { "min_speed",  "min speed of sub",  OFFSET(min_speed),  AV_OPT_TYPE_FLOAT,  {.dbl=1.0}, 0, 4, A },
    { "max_speed",  "max speed of sub",  OFFSET(max_speed),  AV_OPT_TYPE_FLOAT,  {.dbl=1.4}, 0, 4, A },
    { "sub_volume",  "volume of subtitle",  OFFSET(sub_volume),  AV_OPT_TYPE_FLOAT,  {.dbl=1.0}, 0, 4, A },
    { "aud_volume",  "volume of autio",  OFFSET(aud_volume),  AV_OPT_TYPE_FLOAT,  {.dbl=1.0}, 0, 4, A },
    { NULL }
};

AVFILTER_DEFINE_CLASS(tts);

#define _______ "\0\0\0\0"
static const char uri_encode_tbl[ sizeof(int32_t) * 0x100 ] = {
/*  0       1       2       3       4       5       6       7       8       9       a       b       c       d       e       f                        */
    "%00\0" "%01\0" "%02\0" "%03\0" "%04\0" "%05\0" "%06\0" "%07\0" "%08\0" "%09\0" "%0A\0" "%0B\0" "%0C\0" "%0D\0" "%0E\0" "%0F\0"  /* 0:   0 ~  15 */
    "%10\0" "%11\0" "%12\0" "%13\0" "%14\0" "%15\0" "%16\0" "%17\0" "%18\0" "%19\0" "%1A\0" "%1B\0" "%1C\0" "%1D\0" "%1E\0" "%1F\0"  /* 1:  16 ~  31 */
    "%20\0" "%21\0" "%22\0" "%23\0" "%24\0" "%25\0" "%26\0" "%27\0" "%28\0" "%29\0" "%2A\0" "%2B\0" "%2C\0" _______ _______ "%2F\0"  /* 2:  32 ~  47 */
    _______ _______ _______ _______ _______ _______ _______ _______ _______ _______ "%3A\0" "%3B\0" "%3C\0" "%3D\0" "%3E\0" "%3F\0"  /* 3:  48 ~  63 */
    "%40\0" _______ _______ _______ _______ _______ _______ _______ _______ _______ _______ _______ _______ _______ _______ _______  /* 4:  64 ~  79 */
    _______ _______ _______ _______ _______ _______ _______ _______ _______ _______ _______ "%5B\0" "%5C\0" "%5D\0" "%5E\0" _______  /* 5:  80 ~  95 */
    "%60\0" _______ _______ _______ _______ _______ _______ _______ _______ _______ _______ _______ _______ _______ _______ _______  /* 6:  96 ~ 111 */
    _______ _______ _______ _______ _______ _______ _______ _______ _______ _______ _______ "%7B\0" "%7C\0" "%7D\0" _______ "%7F\0"  /* 7: 112 ~ 127 */
    "%80\0" "%81\0" "%82\0" "%83\0" "%84\0" "%85\0" "%86\0" "%87\0" "%88\0" "%89\0" "%8A\0" "%8B\0" "%8C\0" "%8D\0" "%8E\0" "%8F\0"  /* 8: 128 ~ 143 */
    "%90\0" "%91\0" "%92\0" "%93\0" "%94\0" "%95\0" "%96\0" "%97\0" "%98\0" "%99\0" "%9A\0" "%9B\0" "%9C\0" "%9D\0" "%9E\0" "%9F\0"  /* 9: 144 ~ 159 */
    "%A0\0" "%A1\0" "%A2\0" "%A3\0" "%A4\0" "%A5\0" "%A6\0" "%A7\0" "%A8\0" "%A9\0" "%AA\0" "%AB\0" "%AC\0" "%AD\0" "%AE\0" "%AF\0"  /* A: 160 ~ 175 */
    "%B0\0" "%B1\0" "%B2\0" "%B3\0" "%B4\0" "%B5\0" "%B6\0" "%B7\0" "%B8\0" "%B9\0" "%BA\0" "%BB\0" "%BC\0" "%BD\0" "%BE\0" "%BF\0"  /* B: 176 ~ 191 */
    "%C0\0" "%C1\0" "%C2\0" "%C3\0" "%C4\0" "%C5\0" "%C6\0" "%C7\0" "%C8\0" "%C9\0" "%CA\0" "%CB\0" "%CC\0" "%CD\0" "%CE\0" "%CF\0"  /* C: 192 ~ 207 */
    "%D0\0" "%D1\0" "%D2\0" "%D3\0" "%D4\0" "%D5\0" "%D6\0" "%D7\0" "%D8\0" "%D9\0" "%DA\0" "%DB\0" "%DC\0" "%DD\0" "%DE\0" "%DF\0"  /* D: 208 ~ 223 */
    "%E0\0" "%E1\0" "%E2\0" "%E3\0" "%E4\0" "%E5\0" "%E6\0" "%E7\0" "%E8\0" "%E9\0" "%EA\0" "%EB\0" "%EC\0" "%ED\0" "%EE\0" "%EF\0"  /* E: 224 ~ 239 */
    "%F0\0" "%F1\0" "%F2\0" "%F3\0" "%F4\0" "%F5\0" "%F6\0" "%F7\0" "%F8\0" "%F9\0" "%FA\0" "%FB\0" "%FC\0" "%FD\0" "%FE\0" "%FF"    /* F: 240 ~ 255 */
};
static void tts_url_encode(const char *src, char *dst){
	int len = strlen(src);
	size_t i = 0, j = 0;
	while (i < len){
		const char octet = src[i++];
		const int32_t code = ((int32_t*)uri_encode_tbl)[ (unsigned char)octet ];
		if (code) {
			*((int32_t*)&dst[j]) = code;
			j += 3;
		}
		else dst[j++] = octet;
	}
	dst[j] = '\0';
}
static int tts_init_filter(SigmaTTSContext *tts, AVStream *src, float speed, AVFilterGraph **fg,
		AVFilterContext **fcs, AVFilterContext **fcd)
{
    char args[512];
    int ret = 0;
    const AVFilter *buffersrc = NULL;
    const AVFilter *buffersink = NULL;
    AVFilterContext *buffersrc_ctx = NULL;
    AVFilterContext *buffersink_ctx = NULL;
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    AVFilterGraph *filter_graph = avfilter_graph_alloc();
    AVCodecParameters *codec_src = src->codecpar;
    AVFilterLink *inlink = tts->inlink;
    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
	buffersrc = avfilter_get_by_name("abuffer");
	buffersink = avfilter_get_by_name("abuffersink");
	if (!buffersrc || !buffersink) {
		av_log(NULL, AV_LOG_ERROR, "filtering source or sink element not found\n");
		ret = AVERROR_UNKNOWN;
		goto end;
	}
	if (!codec_src->channel_layout)
		codec_src->channel_layout = av_get_default_channel_layout(codec_src->channels);
	if (!inlink->channel_layout)
		inlink->channel_layout = av_get_default_channel_layout(inlink->channels);
	snprintf(args, sizeof(args),
		"time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%"PRIx64,
		src->time_base.num, src->time_base.den, codec_src->sample_rate,
		av_get_sample_fmt_name(codec_src->format),
		codec_src->channel_layout);
	ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
			args, NULL, filter_graph);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
		goto end;
	}

	ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
			NULL, NULL, filter_graph);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer sink\n");
		goto end;
	}
	ret = av_opt_set_bin(buffersink_ctx, "sample_fmts",
			(uint8_t*)&inlink->format, sizeof(inlink->format),
			AV_OPT_SEARCH_CHILDREN);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot set output sample format\n");
		goto end;
	}
	ret = av_opt_set_bin(buffersink_ctx, "channel_layouts",
		(uint8_t*)&inlink->channel_layout, sizeof(inlink->channel_layout), AV_OPT_SEARCH_CHILDREN);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot set output channel layout\n");
		goto end;
	}
	ret = av_opt_set_bin(buffersink_ctx, "sample_rates",
			(uint8_t*)&inlink->sample_rate, sizeof(inlink->sample_rate),
			AV_OPT_SEARCH_CHILDREN);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot set output sample rate\n");
		goto end;
	}

    /* Endpoints for the filter graph. */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;

    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;

    if (!outputs->name || !inputs->name) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    sprintf(args, "aresample=48000:ocl=mono:osf=3,atempo=%.2f,aresample=%d:ocl=%s:osf=%d,asettb=%d/%d",
    	speed, inlink->sample_rate,
		av_get_default_channel_layout_name(inlink->channels), inlink->format,
		inlink->time_base.num, inlink->time_base.den);

    if ((ret = avfilter_graph_parse_ptr(filter_graph, args,
                    &inputs, &outputs, NULL)) < 0)
        goto end;

    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        goto end;

    /* Fill FilteringContext */
    *fg = filter_graph;
    *fcs = buffersrc_ctx;
    *fcd = buffersink_ctx;

end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    if(ret)
    	avfilter_graph_free(&filter_graph);
    return ret;
}

#define MIX_AUDIO(name, type, type_up, min, max)                                                         \
    static void fc_mix_frame_##name(SigmaTTSContext *ctx, AVFrame *frame)                                  \
    {                                                                                                    \
        int mix_sample, i;                                                                               \
        AVFrame *sub_frame;                                                                              \
        type *audio, *sub;                                                                               \
        type_up sum;                                                                                     \
        sub_frame = ff_framequeue_peek(&ctx->sub_frame_fifo, 0);                                         \
        if ((frame->nb_samples - ctx->sample_offset_a) < (sub_frame->nb_samples - ctx->sample_offset_s)) \
        {                                                                                                \
            mix_sample = frame->nb_samples - ctx->sample_offset_a;                                       \
        }                                                                                                \
        else                                                                                             \
        {                                                                                                \
            mix_sample = sub_frame->nb_samples - ctx->sample_offset_s;                                   \
        }                                                                                                \
        audio = (type *)frame->data[frame->channels * ctx->sample_offset_a];                             \
        sub = (type *)sub_frame->data[sub_frame->channels * ctx->sample_offset_s];                       \
        for (i = 0; i < mix_sample; i++, audio++, sub++)                                                 \
        {                                                                                                \
            sum = (type_up)(*audio*ctx->aud_volume) + (type_up)(*sub*ctx->sub_volume);                   \
            if (sum < min)                                                                               \
                sum = min;                                                                               \
            if (sum > max)                                                                               \
                sum = max;                                                                               \
            *audio = (type)sum;                                                                          \
        }                                                                                                \
        ctx->sample_offset_a += mix_sample;                                                              \
        ctx->sample_offset_s += mix_sample;                                                              \
        if (ctx->sample_offset_a == frame->nb_samples)                                                   \
        {                                                                                                \
            ctx->sample_offset_a = 0;                                                                    \
        }                                                                                                \
        if (ctx->sample_offset_s == sub_frame->nb_samples)                                               \
        {                                                                                                \
            sub_frame = ff_framequeue_take(&ctx->sub_frame_fifo);                                        \
            av_freep(&sub_frame);                                                                        \
            ctx->sample_offset_s = 0;                                                                    \
        }                                                                                                \
    }

MIX_AUDIO(u8, uint8_t, uint16_t, 0, UINT8_MAX)
MIX_AUDIO(s16, int16_t, int32_t, INT16_MIN, INT16_MAX)
MIX_AUDIO(s32, int32_t, int64_t, INT32_MIN, INT32_MAX)
MIX_AUDIO(flt, float, float, -1.0, 1.0)
MIX_AUDIO(dbl, double, double, -1.0, 1.0)

#define MIX_AUDIO_P(name, type, type_up, min, max)                                                       \
    static void fc_mix_frame_##name##p(SigmaTTSContext *ctx, AVFrame *frame)                               \
    {                                                                                                    \
        int mix_sample, chan, i, channels, gap;                                                          \
        AVFrame *sub_frame;                                                                              \
        type **audio, **sub;                                                                             \
        type_up sum;                                                                                     \
        gap = sizeof(type);                                                                              \
        channels = frame->channels;                                                                      \
        sub_frame = ff_framequeue_peek(&ctx->sub_frame_fifo, 0);                                         \
        audio = (type **)malloc(channels * sizeof(type *));                                              \
        sub = (type **)malloc(channels * sizeof(type *));                                                \
        for (chan = 0; chan < channels; chan++)                                                          \
        {                                                                                                \
            audio[chan] = (type *)&frame->extended_data[chan][gap * ctx->sample_offset_a];               \
            sub[chan] = (type *)&sub_frame->extended_data[chan][gap * ctx->sample_offset_s];             \
        }                                                                                                \
        if ((frame->nb_samples - ctx->sample_offset_a) < (sub_frame->nb_samples - ctx->sample_offset_s)) \
        {                                                                                                \
            mix_sample = frame->nb_samples - ctx->sample_offset_a;                                       \
        }                                                                                                \
        else                                                                                             \
        {                                                                                                \
            mix_sample = sub_frame->nb_samples - ctx->sample_offset_s;                                   \
        }                                                                                                \
        for (i = 0; i < mix_sample; i++)                                                                 \
        {                                                                                                \
            for (chan = 0; chan < channels; chan++)                                                      \
            {                                                                                            \
                sum = (type_up)(*audio[chan]*ctx->aud_volume)                                            \
                    + (type_up)(*sub[chan]*ctx->sub_volume);                                             \
                if (sum < min)                                                                           \
                    sum = min;                                                                           \
                if (sum > max)                                                                           \
                    sum = max;                                                                           \
                *audio[chan] = (type)sum;                                                                \
                audio[chan]++;                                                                           \
                sub[chan]++;                                                                             \
            }                                                                                            \
        }                                                                                                \
        ctx->sample_offset_a += mix_sample;                                                              \
        ctx->sample_offset_s += mix_sample;                                                              \
        if (ctx->sample_offset_a == frame->nb_samples)                                                    \
        {                                                                                                \
            ctx->sample_offset_a = 0;                                                                    \
        }                                                                                                \
        if (ctx->sample_offset_s == sub_frame->nb_samples)                                                \
        {                                                                                                \
            sub_frame = ff_framequeue_take(&ctx->sub_frame_fifo);                                        \
            av_freep(&sub_frame);                                                                        \
            ctx->sample_offset_s = 0;                                                                    \
        }                                                                                                \
        free(audio);                                                                                     \
        free(sub);                                                                                       \
    }

MIX_AUDIO_P(u8, uint8_t, uint16_t, 0, UINT8_MAX)
MIX_AUDIO_P(s16, int16_t, int32_t, INT16_MIN, INT16_MAX)
MIX_AUDIO_P(s32, int32_t, int64_t, INT32_MIN, INT32_MAX)
MIX_AUDIO_P(flt, float, float, -1.0, 1.0)
MIX_AUDIO_P(dbl, double, double, -1.0, 1.0)

static void tts_setup(SigmaTTSContext *tts)
{
    switch (tts->inlink->format)
    {
    case AV_SAMPLE_FMT_U8:
    	tts->fc_mix = fc_mix_frame_u8;
        break;
    case AV_SAMPLE_FMT_U8P:
    	tts->fc_mix = fc_mix_frame_u8p;
        break;
    case AV_SAMPLE_FMT_S16:
    	tts->fc_mix = fc_mix_frame_s16;
        break;
    case AV_SAMPLE_FMT_S16P:
    	tts->fc_mix = fc_mix_frame_s16p;
        break;
    case AV_SAMPLE_FMT_S32:
    	tts->fc_mix = fc_mix_frame_s32;
        break;
    case AV_SAMPLE_FMT_S32P:
    	tts->fc_mix = fc_mix_frame_s32p;
        break;
    case AV_SAMPLE_FMT_FLT:
    	tts->fc_mix = fc_mix_frame_flt;
        break;
    case AV_SAMPLE_FMT_FLTP:
    	tts->fc_mix = fc_mix_frame_fltp;
        break;
    case AV_SAMPLE_FMT_DBL:
    	tts->fc_mix = fc_mix_frame_dbl;
        break;
    case AV_SAMPLE_FMT_DBLP:
    	tts->fc_mix = fc_mix_frame_dblp;
        break;
    }
}

static int open_input(AVFormatContext **fmt_ctx, AVCodecContext **dec_ctx, int *stream_index, enum AVMediaType type, const char *filename)
{
    int ret;
    AVCodec *dec;

    if ((ret = avformat_open_input(fmt_ctx, filename, NULL, NULL)) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file '%s'\n", filename);
        return ret;
    }

    if ((ret = avformat_find_stream_info(*fmt_ctx, NULL)) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }

    /* select the stream */
    ret = av_find_best_stream(*fmt_ctx, type, -1, -1, &dec, 0);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream in the input file '%s'\n", filename);
        return ret;
    }
    *stream_index = ret;

    /* create decoding context */
    *dec_ctx = avcodec_alloc_context3(dec);
    if (*dec_ctx == NULL)
        return AVERROR(ENOMEM);
    avcodec_parameters_to_context(*dec_ctx, (*fmt_ctx)->streams[*stream_index]->codecpar);

    /* init the audio decoder */
    if ((ret = avcodec_open2(*dec_ctx, dec, NULL)) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot open decoder\n");
        return ret;
    }

    return 0;
}
static void tts_text_cb(void *priv, const char *text, int len)
{
	SigmaTTSContext *s = priv;
    AVBPrint *buffer = &s->buffer;
    av_bprint_append_data(buffer, text, len);
}

static void tts_new_line_cb(void *priv, int forced)
{
	SigmaTTSContext *s = priv;

    av_bprintf(&s->buffer, "\n");
}

static const ASSCodesCallbacks tts_callbacks = {
    .text = tts_text_cb,
    .new_line = tts_new_line_cb,
};

static int ttstts_parse_frame(AVFilterContext *avctx, uint8_t *buf,
                             int bufsize, const AVSubtitle *sub)
{
    SigmaTTSContext *s = avctx->priv;
    ASSDialog *dialog;
    int i;

    av_bprint_clear(&s->buffer);

    for (i = 0; i < sub->num_rects; i++)
    {
        const char *ass = sub->rects[i]->ass;

        if (sub->rects[i]->type != SUBTITLE_ASS)
        {
            av_log(avctx, AV_LOG_ERROR, "Only SUBTITLE_ASS type supported.\n");
            return AVERROR(ENOSYS);
        }

#if FF_API_ASS_TIMING
        if (!strncmp(ass, "Dialogue: ", 10))
        {
            int num;
            dialog = ff_ass_split_dialog(s->ass_ctx, ass, 0, &num);

            for (; dialog && num--; dialog++)
            {
                ff_ass_split_override_codes(&tts_callbacks, s,
                                            dialog->text);
            }
        }
        else
        {
#endif
            dialog = ff_ass_split_dialog2(s->ass_ctx, ass);
            if (!dialog)
                return AVERROR(ENOMEM);

            ff_ass_split_override_codes(&tts_callbacks, s, dialog->text);
            ff_ass_free_dialog(&dialog);
#if FF_API_ASS_TIMING
        }
#endif
    }

    if (!av_bprint_is_complete(&s->buffer))
        return AVERROR(ENOMEM);
    if (!s->buffer.len)
        return 0;

    if (s->buffer.len > bufsize)
    {
        av_log(avctx, AV_LOG_ERROR, "Buffer too small for ASS event.\n");
        return -1;
    }
    memcpy(buf, s->buffer.str, s->buffer.len);
    buf[s->buffer.len] = 0;

    return 0;
}
static int tts_push(AVFilterContext *ctx, AVSubtitle *subtitle, int pts, int64_t duration)
{
    SigmaTTSContext *tts = ctx->priv;
    AVFormatContext *fmt_ctx;
    AVCodecContext *dec_ctx;
    int stream_index;
    int ret = 0;
    AVPacket *packet;
    int64_t plus;
    AVFilterGraph *fg = NULL;
    AVFilterContext *fcs = NULL;
    AVFilterContext *fcd = NULL;
    float speed;
    char *url = tts->buf_url, text = tts->buf_text;
    fmt_ctx = NULL;
    dec_ctx = NULL;
    stream_index = -1;
    packet = tts->decoded_pkt;
    ret = ttstts_parse_frame(ctx, text, 2048, subtitle);
    if(ret < 0)
    	return ret;
	sprintf(url, "%stext=", tts->server);
	tts_url_encode(text, url + strlen(url));
    if ((ret = open_input(&fmt_ctx, &dec_ctx, &stream_index, AVMEDIA_TYPE_AUDIO, url)) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "%s: Error while open received audio fail\n", __func__);
        goto end;
    }
    if(fmt_ctx->duration < 1 || duration < 1)
    	speed = tts->min_speed;
    else
    	speed = FFMAX(tts->min_speed, FFMIN(tts->max_speed, fmt_ctx->duration / duration));
    if(speed <= 0){
    	goto end;
    }
    if((ret = tts_init_filter(tts, fmt_ctx->streams[stream_index], speed, &fg, &fcs, &fcd))){
    	return ret;
    }
    plus =  av_rescale_q(pts, AV_TIME_BASE_Q, fmt_ctx->streams[stream_index]->time_base);
    while (1)
    {
        if ((ret = av_read_frame(fmt_ctx, packet)) < 0)
            break;
        if (packet->stream_index == stream_index)
        {
            ret = avcodec_send_packet(dec_ctx, packet);
            if (ret < 0)
            {
                av_log(NULL, AV_LOG_ERROR, "%s: Error while sending a packet to the decoder\n", __func__);
                break;
            }

            while (ret >= 0)
            {
                AVFrame *frame = av_frame_alloc();
                if (!frame)
                {
                    ret = AVERROR(ENOMEM);
                    goto end;
                }
                ret = avcodec_receive_frame(dec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                {
                    break;
                }
                else if (ret < 0)
                {
                    av_log(NULL, AV_LOG_ERROR, "%s: Error while receiving a frame from the decoder\n", __func__);
                    goto end;
                }

                if (ret >= 0)
                {
                    frame->pts += plus;
                	ret = av_buffersrc_add_frame(fcs, frame);
                	av_frame_free(&frame);
                	if(ret < 0)
                		goto end;
                }
            }
        }
        av_packet_unref(packet);
    }
	av_buffersrc_add_frame(fcs, NULL);
	while(1){
		AVFrame *frame = av_frame_alloc();
		if (!frame){
			ret = AVERROR(ENOMEM);
			break;
		}
		ret = av_buffersink_get_frame(fcd, frame);
		if (ret < 0) {
			av_frame_free(&frame);
			break;
		}
		ff_framequeue_add(&tts->sub_frame_fifo, frame);
	}

end:
    avcodec_free_context(&dec_ctx);
    avformat_close_input(&fmt_ctx);
    avfilter_graph_free(&fg);
    return ret;
}

static int tts_mix(SigmaTTSContext *tts, AVFrame *decoded_frame, AVRational decoded_frame_tb){
   int ret = 0;
   SubtitleHandler *handler = tts->subtitle_handler;
   SubtitleEntry *entry = handler->entry;
   if(entry->file){
	   ret = subtitle_pull(entry, av_rescale_q(decoded_frame->pts, decoded_frame_tb, AV_TIME_BASE_Q));
	   if(ret < 0)
		   return ret;
   }
   if (!ff_framequeue_queued_frames(&tts->sub_frame_fifo))
	   return 0;
   AVFrame *frame = ff_framequeue_peek(&tts->sub_frame_fifo, 0);
   if (frame->pts != AV_NOPTS_VALUE&&decoded_frame->pts < frame->pts)
	   return 0;
   do{
	   tts->fc_mix(tts, decoded_frame);
   } while (ff_framequeue_queued_frames(&tts->sub_frame_fifo) > 0 && tts->sample_offset_a > 0);
   return 0;
}

static int filter_frame(AVFilterLink *inlink, AVFrame *frame)
{
    AVFilterContext *ctx = inlink->dst;
    SigmaTTSContext *tts = ctx->priv;
    AVFrame *out_frame;
    int ret;
    if (av_frame_is_writable(frame)) {
        out_frame = frame;
    } else {
        out_frame = ff_get_audio_buffer(ctx->outputs[0], frame->nb_samples);
        if (!out_frame) {
            av_frame_free(&frame);
            return AVERROR(ENOMEM);
        }
        av_frame_copy_props(out_frame, frame);
        av_frame_copy(out_frame, frame);
        av_frame_free(&frame);
    }
    ret = tts_mix(tts, out_frame, ctx->outputs[0]->time_base);
    if(ret)
    	return ret;
    return ff_filter_frame(ctx->outputs[0], out_frame);
}
static av_cold void uninit(AVFilterContext *ctx)
{
    SigmaTTSContext *tts = ctx->priv;
    int i;
	avfilter_graph_free(&tts->graph);
	av_packet_free(&tts->decoded_pkt);
	ff_framequeue_free(&tts->sub_frame_fifo);
	if(tts->subtitle_handler)
		subtitle_handle_remove(&tts->subtitle_handler, tts);
	if(tts->ass_ctx)
		ff_ass_split_free(tts->ass_ctx);
	av_bprint_finalize(&tts->buffer, NULL);
}

static av_cold int init(AVFilterContext *ctx)
{
    SigmaTTSContext *tts = ctx->priv;
    int i, ret;
    if(!tts->subtitle || !*tts->subtitle){
    	av_log(tts, AV_LOG_ERROR, "Subtitle source not found\n");
    	return AVERROR(EINVAL);
    }
    if(!tts->server || !*tts->server){
    	av_log(tts, AV_LOG_ERROR, "Server not found\n");
    	return AVERROR(EINVAL);
    }
    return 0;
}

static int query_formats(AVFilterContext *ctx)
{
    AVFilterChannelLayouts *layouts;
    AVFilterFormats *formats;
    static const enum AVSampleFormat sample_fmts[] = {
		 AV_SAMPLE_FMT_U8, AV_SAMPLE_FMT_U8P,
		 AV_SAMPLE_FMT_S16, AV_SAMPLE_FMT_S16P,
		 AV_SAMPLE_FMT_S32, AV_SAMPLE_FMT_S32P,
		 AV_SAMPLE_FMT_FLT, AV_SAMPLE_FMT_FLTP,
		 AV_SAMPLE_FMT_DBL, AV_SAMPLE_FMT_DBLP,
		 AV_SAMPLE_FMT_NONE
	};
    int ret;

    layouts = ff_all_channel_counts();
    if (!layouts)
        return AVERROR(ENOMEM);
    ret = ff_set_common_channel_layouts(ctx, layouts);
    if (ret < 0)
        return ret;

    formats = ff_make_format_list(sample_fmts);
    if (!formats)
        return AVERROR(ENOMEM);
    ret = ff_set_common_formats(ctx, formats);
    if (ret < 0)
        return ret;

    formats = ff_all_samplerates();
    if (!formats)
        return AVERROR(ENOMEM);
    return ff_set_common_samplerates(ctx, formats);
}

static int config_props(AVFilterLink *inlink){
    AVFilterContext *ctx = inlink->dst;
    SigmaTTSContext *tts = ctx->priv;
    SubtitleHandler *handler;
    SubtitleEntry *entry;
    int ret;
	tts->inlink = inlink;
	tts->decoded_pkt = av_packet_alloc();
	if (!tts->decoded_pkt)
		return AVERROR(ENOMEM);
	tts_setup(tts);
	tts->graph = avfilter_graph_alloc();
	if (!tts->graph)
		return AVERROR(ENOMEM);
	ff_framequeue_init(&tts->sub_frame_fifo, &tts->graph->internal->frame_queues);
	ret = subtitle_handle_add(&tts->subtitle_handler, ctx, tts->subtitle, tts_push);
	handler = tts->subtitle_handler;
	entry = handler->entry;
    tts->ass_ctx = ff_ass_split(entry->decoder->subtitle_header);
    av_bprint_init(&tts->buffer, 0, AV_BPRINT_SIZE_UNLIMITED);
	return ret;
}
static const AVFilterPad tts_inputs[] = {
    {
        .name         = "default",
        .type         = AVMEDIA_TYPE_AUDIO,
        .filter_frame = filter_frame,
		.config_props = config_props
    },
    { NULL }
};

static const AVFilterPad tts_outputs[] = {
    {
        .name          = "default",
        .type          = AVMEDIA_TYPE_AUDIO,
    },
    { NULL }
};

AVFilter ff_af_tts = {
    .name          = "tts",
    .description   = NULL_IF_CONFIG_SMALL("Convert subtitle to audio and merge to this"),
    .query_formats = query_formats,
    .priv_size     = sizeof(SigmaTTSContext),
    .priv_class    = &tts_class,
    .init          = init,
    .uninit        = uninit,
    .inputs        = tts_inputs,
    .outputs       = tts_outputs,
};
