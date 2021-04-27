#include "subtitle_common.h"

#include "libavformat/avio_internal.h"

void ttml_write_time(AVIOContext *pb, uint64_t time, AVRational *tb)
{
    int64_t sec, min, hour, ms;
    ms = 1000 * time * tb->num / tb->den;
    sec = ms / 1000;
    ms = ms % 1000;
    min = sec / 60;
    sec = sec % 60;
    hour = min / 60;
    min = min % 60;
    avio_printf(pb, "%02" PRId64 ":%02" PRId64 ":%02" PRId64 ".%03" PRId64, hour, min, sec, ms);
}

void ttml_write_p_tag_sub_pkt(AVIOContext *pb, AVPacket *pkt, AVRational *tb)
{
    avio_printf(pb, "\t\t\t<p begin=\"");
    ttml_write_time(pb, pkt->pts, tb);
    avio_printf(pb, "\" end=\"");
    ttml_write_time(pb, pkt->pts + pkt->duration, tb);
    avio_printf(pb, "\">\n");
    avio_printf(pb, "\t\t\t\t<span style=\"s1\">");
    avio_write(pb, pkt->data, pkt->size);
    avio_printf(pb, "</span>\n");
    avio_printf(pb, "\t\t\t</p>\n");
}

void ttml_write_header_internal(AVIOContext *pb, const char *lang)
{
    avio_printf(pb, XML_PROLOG);
    avio_printf(pb, TTML_TT_OPEN, lang);
    avio_printf(pb, TTML_HEAD_TAG);
    avio_printf(pb, TTML_BODY_OPEN);
    avio_printf(pb, TTML_DIV_OPEN);
}

void ttml_writer_footer_internal(AVIOContext *pb)
{
    avio_printf(pb, TTML_DIV_CLOSE);
    avio_printf(pb, TTML_BODY_CLOSE);
    avio_printf(pb, TTML_TT_CLOSE);
}

int ttml_write_mdat_sub_pkt(AVPacket *pkt, const char *lang, AVRational *tb)
{
    uint8_t *buf;
    int ret = 0;
    AVIOContext *pb = NULL;

    ret = avio_open_dyn_buf(&pb);
    if (ret < 0)
    {
        return ret;
    }

    ttml_write_header_internal(pb, lang);
    ttml_write_p_tag_sub_pkt(pb, pkt, tb);
    ttml_writer_footer_internal(pb);
    avio_flush(pb);

    ret = avio_close_dyn_buf(pb, &buf);
    av_grow_packet(pkt, ret - pkt->size);
    memcpy(pkt->data, buf, pkt->size);
    av_freep(&buf);

    return ret;
}

int webvtt_write_mdat_sub_pkt(AVPacket *pkt, const char *lang, AVRational *tb)
{
    uint8_t *buf;
    int ret = 0;
    AVIOContext *pb = NULL;

    ret = avio_open_dyn_buf(&pb);
    if (ret < 0)
    {
        return ret;
    }

    webvtt_write_vttc_tag(pb, NULL, pkt->data, NULL);
    webvtt_write_vtte_tag(pb);

    avio_flush(pb);
    ret = avio_close_dyn_buf(pb, &buf);
    av_grow_packet(pkt, ret - pkt->size);
    memcpy(pkt->data, buf, pkt->size);
    av_freep(&buf);

    return ret;
}

void webvtt_write_vttc_tag(AVIOContext *pb, const char *iden, const char *payl, const char *sttg)
{
    int64_t curpos;
    int64_t pos = avio_tell(pb);
    avio_wb32(pb, 0); /* write size */
    ffio_wfourcc(pb, "vttc");

    webvtt_write_vttc_subtag(pb, "iden", iden);
    webvtt_write_vttc_subtag(pb, "payl", payl);
    webvtt_write_vttc_subtag(pb, "sttg", sttg);
    curpos = avio_tell(pb);
    avio_seek(pb, pos, SEEK_SET);
    avio_wb32(pb, curpos - pos); /* rewrite size */
    avio_seek(pb, curpos, SEEK_SET);
}

void webvtt_write_vtte_tag(AVIOContext *pb)
{
    /* write webvtt cue */
    avio_wb32(pb, 8);
    ffio_wfourcc(pb, "vtte");
}

void webvtt_write_vttc_subtag(AVIOContext *pb, const char *tag, const char *data)
{
    if (!data)
        return;
    size_t size = strlen(data);
    avio_wb32(pb, 8 + size);
    ffio_wfourcc(pb, tag);
    avio_write(pb, data, size);
}