#include "ttml_common.h"

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