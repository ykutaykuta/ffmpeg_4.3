#ifndef SUBTITLE_COMMON_H
#define SUBTITLE_COMMON_H

#include "avcodec.h"
#include "libavformat/avio.h"
#include "packet.h"

/*
* Definition of server address, format of the http request
*
* POST api HTTP/1.1
* header
*
* body
*/
#define SPACE " "
#define CRLF "\r\n"
#define MAX_HTTP_RESPONSE_SIZE 10240
#define MAX_HTTP_REQUEST_SIZE 1024000
#define SERVER_IP "192.168.3.221"
#define SERVER_PORT 8000
#define POST "POST"
#define HTTP "HTTP/1.1"
#define CONTENT_TYPE_JSON "Content-Type: application/json"
#define CONTENT_LEN "Content-Length: %00007d"

/*
* Definition of subttitle ttml codec
*/
#define TTML_XMLNS_TTML "http://www.w3.org/ns/ttml"
#define TTML_XMLNS_TTP "http://www.w3.org/ns/ttml#parameter"
#define TTML_XMLNS_TTS "http://www.w3.org/ns/ttml#styling"
#define TTML_XMLNS_TTM "http://www.w3.org/ns/ttml#metadata"
#define TTML_XMLNS_EBUTTS "urn:ebu:style"
#define TTML_XMLNS_EBUTTM "urn:ebu:metadata"

#define XML_PROLOG "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"

#define TTML_TT_OPEN                             \
    "<tt\n"                                      \
    "\txmlns=\"" TTML_XMLNS_TTML "\"\n"          \
    "\txmlns:ttm=\"" TTML_XMLNS_TTM "\"\n"       \
    "\txmlns:tts=\"" TTML_XMLNS_TTS "\"\n"       \
    "\txmlns:ttp=\"" TTML_XMLNS_TTP "\"\n"       \
    "\txmlns:ebuttm=\"" TTML_XMLNS_EBUTTM "\"\n" \
    "\txmlns:ebutts=\"" TTML_XMLNS_EBUTTS "\"\n" \
    "\txml:lang=\"%s\" xml:space=\"default\"\n"  \
    "\tttp:timeBase=\"media\" ttp:cellResolution=\"32 15\">\n"
#define TTML_TT_CLOSE "</tt>\n"

#define TTML_METADATA_TAG                                                                           \
    "\t\t<metadata>\n"                                                                              \
    "\t\t\t<ttm:title>DASH-IF Live Simulator</ttm:title>\n"                                         \
    "\t\t\t<ebuttm:documentMetadata>\n"                                                             \
    "\t\t\t\t<ebuttm:conformsToStandard>urn:ebu:distribution:2014-01</ebuttm:conformsToStandard>\n" \
    "\t\t\t\t<ebuttm:authoredFrameRate>30</ebuttm:authoredFrameRate>\n"                             \
    "\t\t\t</ebuttm:documentMetadata>\n"                                                            \
    "\t\t</metadata>\n"

#define TTML_STYLING_TAG                                                                                               \
    "\t\t<styling>\n"                                                                                                  \
    "\t\t\t<style xml:id=\"s0\" tts:fontStyle=\"normal\" tts:fontFamily=\"sansSerif\" tts:fontSize=\"100%\"\n"         \
    "\t\t\t\ttts:lineHeight=\"normal\" tts:color=\"#FFFFFF\" tts:wrapOption=\"noWrap\" tts:textAlign=\"center\"/>\n"   \
    "\t\t\t<style xml:id=\"s1\" tts:color=\"#FFFFFF\" tts:backgroundColor=\"#000000\" ebutts:linePadding=\"0.5c\"/>\n" \
    "\t\t\t<style xml:id=\"s2\" tts:color=\"#ff0000\" tts:backgroundColor=\"#000000\" ebutts:linePadding=\"0.5c\"/>\n" \
    "\t\t</styling>\n"

#define TTML_LAYOUT_TAG                                                                                                                  \
    "\t\t<layout>\n"                                                                                                                     \
    "\t\t\t<region xml:id=\"r0\" tts:origin=\"15% 80%\" tts:extent=\"70% 20%\" tts:overflow=\"visible\" tts:displayAlign=\"before\"/>\n" \
    "\t\t\t<region xml:id=\"r1\" tts:origin=\"15% 20%\" tts:extent=\"70% 20%\" tts:overflow=\"visible\" tts:displayAlign=\"before\"/>\n" \
    "\t\t</layout>\n"

#define TTML_HEAD_TAG                                               \
    "\t<head>\n" TTML_METADATA_TAG TTML_STYLING_TAG TTML_LAYOUT_TAG \
    "\t</head>\n"

#define TTML_BODY_OPEN "\t<body style=\"s0\">\n"
#define TTML_BODY_CLOSE "\t</body>\n"

#define TTML_DIV_OPEN "\t\t<div region=\"r0\">\n"
#define TTML_DIV_CLOSE "\t\t</div>\n"

/*
* Definition of subtitle dvb bitmap to text.
* Send bitmap data to server and receive text
*/
#define API_OCR "/api/ocr"

int server_connect(const char *ip, int port);
int server_disconnect();
int send_request_and_recv_response();

void ttml_write_time(AVIOContext *pb, uint64_t time, AVRational *tb);

void ttml_write_p_tag_sub_pkt(AVIOContext *pb, AVPacket *pkt, AVRational *tb);

int ttml_write_mdat_sub_pkt(AVPacket *pkt, const char *lang, AVRational *tb);

void ttml_write_header_internal(AVIOContext *pb, const char *lang);

void ttml_writer_footer_internal(AVIOContext *pb);

int webvtt_write_mdat_sub_pkt(AVPacket *pkt, const char *lang, AVRational *tb);

void webvtt_write_vttc_tag(AVIOContext *pb, const char *iden, const char *payl, const char *sttg);

/* write vttc subtag: iden, payl, sttg */
void webvtt_write_vttc_subtag(AVIOContext *pb, const char *tag, const char *data);

void webvtt_write_vtte_tag(AVIOContext *pb);

int ocr_subtitle(AVSubtitle *sub);

#endif /* SUBTITLE_COMMON_H */