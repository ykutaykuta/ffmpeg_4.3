#include "webrtcdec.h"
#include "libavutil/avstring.h"
#include "network.h"

#define RECVBUF_SIZE 10240

static int webrtc_read_probe(const AVProbeData *p)
{
    if (av_strstart(p->filename, "webrtc:", NULL))
        return AVPROBE_SCORE_MAX;
    return 0;
}

static int webrtc_read_header(AVFormatContext *s)
{
    int ret;
    AVStream *v_st, *a_st;
    URLContext *in = NULL;
    WebrtcDemuxContext *ctx = s->priv_data;

    if (!ff_network_init())
        return AVERROR(EIO);

    ret = ffurl_open_whitelist(&in, s->url, AVIO_FLAG_READ, &s->interrupt_callback, NULL, s->protocol_whitelist, s->protocol_blacklist, NULL);
    if (ret)
        goto fail;
    ctx->webrtc_hd = in;

    ctx->recvbuf_size = RECVBUF_SIZE;
    ctx->recvbuf = av_malloc(ctx->recvbuf_size);
    if (!ctx->recvbuf)
    {
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    v_st = avformat_new_stream(s, NULL);
    a_st = avformat_new_stream(s, NULL);
    ctx->v_stream_index = 0;
    ctx->a_stream_index = 1;
    if (!v_st || !a_st)
    {
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    v_st->id = 0;
    v_st->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    v_st->codecpar->codec_id = AV_CODEC_ID_H264;

    a_st->id = 1;
    a_st->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
    a_st->codecpar->codec_id = AV_CODEC_ID_OPUS;

    return ret;
fail:
    ffurl_closep(&in);
    ff_network_close();
    av_freep(&ctx->recvbuf);
    return ret;
}

static int webrtc_read_packet(AVFormatContext *s, AVPacket *pkt)
{
    WebrtcDemuxContext *ctx = s->priv_data;
    int ret, info;
    ret = ffurl_read(ctx->webrtc_hd, ctx->recvbuf, ctx->recvbuf_size);
    if (ret < 0)
        return ret;
    info = ctx->recvbuf[0];
    if ((ret = av_new_packet(pkt, ret)) < 0)
        return ret;
    memcpy(pkt->data, ctx->recvbuf + 5, ret);
    if (info == AVMEDIA_TYPE_VIDEO)
    {
        pkt->stream_index = ctx->v_stream_index;
    }
    else if (info == AVMEDIA_TYPE_AUDIO)
    {
        pkt->stream_index = ctx->a_stream_index;
    }

    return ret;
}

static int webrtc_read_close(AVFormatContext *s)
{
    WebrtcDemuxContext *ctx = s->priv_data;
    ffurl_closep(&ctx->webrtc_hd);
    ff_network_close();
    av_freep(&ctx->recvbuf);
    return 0;
}

static const AVClass webrtc_demuxer_class = {
    .class_name = "Webrtc demuxer",
    .item_name = av_default_item_name,
    .version = LIBAVUTIL_VERSION_INT,
};

AVInputFormat ff_webrtc_demuxer = {
    .name = "webrtc",
    .long_name = NULL_IF_CONFIG_SMALL("Webrtc input"),
    .priv_data_size = sizeof(WebrtcDemuxContext),
    .read_probe = webrtc_read_probe,
    .read_header = webrtc_read_header,
    .read_packet = webrtc_read_packet,
    .read_close = webrtc_read_close,
    .flags = AVFMT_NOFILE,
    .priv_class = &webrtc_demuxer_class,
};
