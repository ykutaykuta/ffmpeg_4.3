#include "subtitle_common.h"

#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "libavformat/avio_internal.h"
#include "libavutil/base64.h"
#include "libavutil/avstring.h"
#include "ass.h"

static struct sockaddr_in serv_addr;
static int fd = -1;
char *response = NULL;
char *request = NULL;
static int count = 0;

static int process_ocr_response(AVSubtitle *sub)
{
    char *p, *begin, *end, *ass;
    int is_text, count, i;

    // clear old rects
    for (i = 0; i < sub->num_rects; i++)
    {
        av_freep(&sub->rects[i]->data[0]);
        av_freep(&sub->rects[i]->data[1]);
        av_freep(&sub->rects[i]->data[2]);
        av_freep(&sub->rects[i]->data[3]);
        av_freep(&sub->rects[i]->text);
        av_freep(&sub->rects[i]->ass);
        av_freep(&sub->rects[i]);
    }
    av_freep(&sub->rects);
    sub->num_rects = 0;

    begin = strrchr(response, '[');
    if (!begin)
        return NULL;

    end = strrchr(response, ']');
    if (!end)
        return NULL;

    is_text = 0;

    p = begin;
    while (p < end)
    {
        if (*p == '"')
        {
            if (is_text)
            {
                *p = '\0';
                ff_ass_add_rect(sub, begin, count++, 0, NULL, NULL);
            }
            else
            {
                begin = p + 1;
            }
            is_text = !is_text;
        }
        p++;
    }
    return 0;
}

int server_connect(const char *ip, int port)
{
    request = av_malloc(MAX_HTTP_REQUEST_SIZE);
    if (!request)
        return AVERROR(ENOMEM);
    response = av_malloc(MAX_HTTP_RESPONSE_SIZE);
    if (!response)
    {
        av_freep(&request);
        return AVERROR(ENOMEM);
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Socket: error on opening the socket\n");
        return AVERROR(EIO);
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    if (connect(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Socket: error on connect to server\n");
        return AVERROR(EIO);
    }
    return 0;
}

int server_disconnect()
{
    av_freep(&request);
    av_freep(&response);
    if (close(fd) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Socket: error on closing the socket\n");
        fd = -1;
        return AVERROR(EIO);
    }
    return 0;
}

int send_request_and_recv_response()
{
    // const char *test = "POST /api/ocr HTTP/1.1\r\n"
    //                     "Content-Type: application/json\r\n"
    //                     "Content-Length: 112\r\n\r\n"
    //                     "{\"rects\":[{\"w\":926,\"h\":96,\"nb_color\":16,\"image\":\"AAAAAAAAA\"},{\"w\":926,\"h\":84,\"nb_color\":16,\"image\":\"BBBBBBBB\"}]}";
    int transferred, bytes, len;

    /* send the request */
    len = strlen(request);
    av_log(NULL, AV_LOG_INFO, "ykuta len %d\n", len);
    transferred = 0;
    do
    {
        bytes = write(fd, request + transferred, len - transferred);
        // bytes = write(fd,  test, len - transferred);
        if (bytes < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Socket: error on writing message to socket\n");
            return AVERROR(EIO);
        }
        transferred += bytes;
        if (bytes == 0)
            break;
    } while (transferred < len);

    /* receive the response */
    memset(response, 0, MAX_HTTP_RESPONSE_SIZE);
    len = MAX_HTTP_RESPONSE_SIZE - 1;
    transferred = 0;
    do
    {
        bytes = read(fd, response + transferred, len - transferred);
        if (bytes < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Socket: error on read response from socket\n");
            return AVERROR(EIO);
        }
        transferred += bytes;
        if (bytes == 0)
            break;
        if (response[transferred - 1] == '}')
            break;
    } while (transferred < len);

    if (transferred >= len)
    {
        av_log(NULL, AV_LOG_WARNING, "Socket: warning size of response too large!\n");
    }
    return 0;
}

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

void ttml_image_write_tt_open(AVIOContext *pb, const char *lang)
{
    if (!lang)
        lang = "eng";
    avio_printf(pb, XML_PROLOG);
    avio_printf(pb, TTML_TT_OPEN_IMAGE, lang);
}

static void ttml_image_write_head_internal(AVIOContext *pb, AVPacket *pkt)
{
    avio_printf(pb, "\t<head>\n");
    avio_printf(pb, "\t\t<metadata>\n");
    avio_printf(pb, "\t\t\t<smpte:image imagetype=\"PNG\" encoding=\"Base64\" xml:id=\"img_0\">");
    avio_write(pb, pkt->data + 1, pkt->size - 1);
    avio_printf(pb, "</smpte:image>\n");
    avio_printf(pb, "\t\t</metadata>\n");
    avio_printf(pb, TTML_LAYOUT_TAG);
    avio_printf(pb, "\t</head>\n");
}

static void ttml_image_write_body_internal(AVIOContext *pb, AVPacket *pkt, AVRational *tb)
{
    avio_printf(pb, "\t<body>\n");
    avio_printf(pb, "\t\t<div begin=\"");
    ttml_write_time(pb, pkt->pts, tb);
    avio_printf(pb, "\" end=\"");
    ttml_write_time(pb, pkt->pts + pkt->duration, tb);
    avio_printf(pb, "\" region=\"r0\" smpte:backgroundImage=\"#img_0\"/>\n");
    avio_printf(pb, "\t</body>\n");
}

void ttml_image_write_pkt(AVIOContext *pb, AVPacket *pkt, AVRational *tb)
{
    ttml_image_write_head_internal(pb, pkt);
    ttml_image_write_body_internal(pb, pkt, tb);
}

void ttml_text_write_tt_open(AVIOContext *pb, const char *lang)
{
    if (!lang)
        lang = "eng";
    avio_printf(pb, XML_PROLOG);
    avio_printf(pb, TTML_TT_OPEN_TEXT, lang);
}

static void ttml_text_write_head_internal(AVIOContext *pb, AVPacket *pkt)
{
    avio_printf(pb, "\t<head>\n");
    avio_printf(pb, TTML_METADATA_TAG);
    avio_printf(pb, TTML_STYLING_TAG);
    avio_printf(pb, TTML_LAYOUT_TAG);
    avio_printf(pb, "\t</head>\n");
}

static void ttml_text_write_body_internal(AVIOContext *pb, AVPacket *pkt, AVRational *tb)
{
    avio_printf(pb, "\t<body>\n");
    avio_printf(pb, "\t\t<div region=\"r0\">\n");
    avio_printf(pb, "\t\t\t<p begin=\"");
    ttml_write_time(pb, pkt->pts, tb);
    avio_printf(pb, "\" end=\"");
    ttml_write_time(pb, pkt->pts + pkt->duration, tb);
    avio_printf(pb, "\">\n");
    avio_printf(pb, "\t\t\t\t<span style=\"s1\">");
    avio_write(pb, pkt->data + 1, pkt->size - 1);
    avio_printf(pb, "</span>\n");
    avio_printf(pb, "\t\t\t</p>\n");
    avio_printf(pb, "\t\t</div>\n");
    avio_printf(pb, "\t</body>\n");
}

void ttml_text_write_pkt(AVIOContext *pb, AVPacket *pkt, AVRational *tb)
{
    ttml_text_write_head_internal(pb, pkt);
    ttml_text_write_body_internal(pb, pkt, tb);
}

void ttml_write_tt_close(AVIOContext *pb)
{
    avio_printf(pb, TTML_TT_CLOSE);
}

int ttml_prepare_for_mdat(AVPacket *pkt, const char *lang, AVRational *tb)
{
    uint8_t *buf;
    int ret = 0;
    AVIOContext *pb = NULL;

    ret = avio_open_dyn_buf(&pb);
    if (ret < 0)
    {
        return ret;
    }

    if (pkt->data[0] == SUBTITLE_TEXT)
    {
        ttml_text_write_tt_open(pb, lang);
        ttml_text_write_pkt(pb, pkt, tb);
        ttml_write_tt_close(pb);
    }
    else if (pkt->data[0] == SUBTITLE_BITMAP)
    {
        ttml_image_write_tt_open(pb, lang);
        ttml_image_write_pkt(pb, pkt, tb);
        ttml_write_tt_close(pb);
    }
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

int ocr_subtitle(AVSubtitle *sub)
{
    char c;
    char *p;
    int h_len, b_len, i, size, b64_size;
    if (fd < 0)
    {
        av_log(NULL, AV_LOG_INFO, "OCR: init connection to server %s:%d for subtitle ocr\n", SERVER_IP, SERVER_PORT);
        if (server_connect(SERVER_IP, SERVER_PORT) < 0)
        {
            av_log(NULL, AV_LOG_WARNING, "OCR: connect to server faile!\n");
            return 0;
        }
    }

    memset(request, 0, MAX_HTTP_REQUEST_SIZE);
    h_len = strlen(POST) + strlen(SPACE) + strlen(API_OCR) + strlen(SPACE) + strlen(HTTP) + strlen(CRLF) + strlen(CONTENT_TYPE_JSON) + strlen(CRLF) + strlen(CONTENT_LEN) + strlen(CRLF) + strlen(CRLF);

    // make body request
    p = request + h_len;
    sprintf(p, "%s", "{\"rects\":[");
    p += strlen("{\"rects\":[");
    for (i = 0; i < sub->num_rects; i++)
    {
        if (sub->rects[i]->type != SUBTITLE_BITMAP)
            continue;
        sprintf(p, "{\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,\"nb_color\":%d,\"b64\":\"", sub->rects[i]->x, sub->rects[i]->y, sub->rects[i]->w, sub->rects[i]->h, sub->rects[i]->nb_colors);
        while (*p != '\0')
            p++;
        size = sub->rects[i]->w * sub->rects[i]->h;
        b64_size = AV_BASE64_SIZE(size);
        av_base64_encode(p, b64_size, sub->rects[i]->data[0], size);
        p = p + b64_size - 1;
        sprintf(p, "\"},");
        p += 3;
    }
    p--;
    sprintf(p, "]}");
    p += 2;
    b_len = p - request - h_len;

    // make header request
    p = request;
    c = *(p + h_len);
    sprintf(p, POST SPACE API_OCR SPACE HTTP CRLF CONTENT_TYPE_JSON CRLF CONTENT_LEN CRLF CRLF, b_len);
    *(p + h_len) = c;

    av_log(NULL, AV_LOG_INFO, "ykuta h_len %d   b_len %d\n", h_len, b_len);
    if (send_request_and_recv_response() < 0)
        return AVERROR(ENOEXEC);

    if (process_ocr_response(sub) < 0)
        return AVERROR(ENOEXEC);
    return 0;
}