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
#define TTML_XMLNS "http://www.w3.org/ns/ttml"
#define TTML_XMLNS_TTP "http://www.w3.org/ns/ttml#parameter"
#define TTML_XMLNS_TTS "http://www.w3.org/ns/ttml#styling"
#define TTML_XMLNS_TTM "http://www.w3.org/ns/ttml#metadata"
#define TTML_XMLNS_EBUTTS "urn:ebu:style"
#define TTML_XMLNS_EBUTTM "urn:ebu:metadata"
#define TTML_XMLNS_SMPTE "http://www.smpte-ra.org/schemas/2052-1/2010/smpte-tt"

#define XML_PROLOG "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"

#define TTML_TT_OPEN_TEXT                        \
    "<tt\n"                                      \
    "\txmlns=\"" TTML_XMLNS "\"\n"               \
    "\txmlns:ttm=\"" TTML_XMLNS_TTM "\"\n"       \
    "\txmlns:tts=\"" TTML_XMLNS_TTS "\"\n"       \
    "\txmlns:ttp=\"" TTML_XMLNS_TTP "\"\n"       \
    "\txmlns:ebuttm=\"" TTML_XMLNS_EBUTTM "\"\n" \
    "\txmlns:ebutts=\"" TTML_XMLNS_EBUTTS "\"\n" \
    "\txml:lang=\"%s\">\n"

#define TTML_TT_OPEN_IMAGE                     \
    "<tt\n"                                    \
    "\txmlns=\"" TTML_XMLNS "\"\n"             \
    "\txmlns:ttm=\"" TTML_XMLNS_TTM "\"\n"     \
    "\txmlns:tts=\"" TTML_XMLNS_TTS "\"\n"     \
    "\txmlns:smpte=\"" TTML_XMLNS_SMPTE "\"\n" \
    "\tlang=\"%s\">\n"

#define TTML_TT_CLOSE "</tt>\n"

#define TTML_METADATA_TAG                                                                           \
    "\t\t<metadata>\n"                                                                              \
    "\t\t\t<ttm:title>Sigmal</ttm:title>\n"                                                         \
    "\t\t\t<ebuttm:documentMetadata>\n"                                                             \
    "\t\t\t\t<ebuttm:conformsToStandard>urn:ebu:distribution:2014-01</ebuttm:conformsToStandard>\n" \
    "\t\t\t</ebuttm:documentMetadata>\n"                                                            \
    "\t\t</metadata>\n"

#define TTML_STYLING_TAG_TEXT                                                                                                                                                                                                                                                    \
    "\t\t<styling>\n"                                                                                                                                                                                                                                                            \
    "\t\t\t<style xml:id=\"s0\" tts:color=\"#FFFFFF\" tts:backgroundColor=\"#000000\" tts:fontStyle=\"normal\" tts:fontFamily=\"sansSerif\" tts:fontSize=\"100%%\" tts:lineHeight=\"normal\" ebutts:linePadding=\"0.5c\" tts:textAlign=\"center\" tts:wrapOption=\"noWrap\"/>\n" \
    "\t\t</styling>\n"

#define TTML_LAYOUT_TAG_TEXT                                                                                                                \
    "\t\t<layout>\n"                                                                                                                        \
    "\t\t\t<region xml:id=\"r0\" tts:origin=\"5%% 80%%\" tts:extent=\"90%% 20%%\" tts:overflow=\"visible\" tts:displayAlign=\"before\"/>\n" \
    "\t\t</layout>\n"

#define TTML_LAYOUT_TAG_IMAGE                                                            \
    "\t\t<layout>\n"                                                                     \
    "\t\t\t<region xml:id=\"r%d\" tts:origin=\"%dpx %dpx\" tts:extent=\"%dpx %dpx\"/>\n" \
    "\t\t</layout>\n"

/*
* Definition of subtitle dvb bitmap to text.
* Send bitmap data to server and receive text
*/
#define API_OCR "/api/ocr"

int server_connect(const char *ip, int port);
int server_disconnect();
int send_request_and_recv_response();

void ttml_write_pkt(AVIOContext *pb, AVPacket *pkt, const char *lang, AVRational tb);

void ttml_write_head_open(AVIOContext *pb);
void ttml_write_head_close(AVIOContext *pb);

void ttml_write_body_open(AVIOContext *pb);
void ttml_write_body_close(AVIOContext *pb);

void ttml_write_layout_open(AVIOContext *pb);
void ttml_write_layout_close(AVIOContext *pb);

void ttml_write_metadata_open(AVIOContext *pb);
void ttml_write_metadata_close(AVIOContext *pb);

void ttml_image_write_tt_open(AVIOContext *pb, const char *lang);
void ttml_image_write_smpte(AVIOContext *pb, AVPacket *pkt, int index);
void ttml_image_wirte_region(AVIOContext *pb, int index, int32_t origin_x, int32_t origin_y, int32_t extent_x, int32_t extent_y);
void ttml_image_write_div(AVIOContext *pb, int index, uint64_t pts, uint64_t duration, AVRational tb);

void ttml_text_write_tt_open(AVIOContext *pb, const char *lang);
void ttml_text_write_head(AVIOContext *pb);
void ttml_text_write_p(AVIOContext *pb, AVPacket *pkt, AVRational tb);
void ttml_text_write_div_open(AVIOContext *pb);
void ttml_text_write_div_close(AVIOContext *pb);


void ttml_write_tt_close(AVIOContext *pb);

int ttml_prepare_for_mdat(AVPacket *pkt, const char *lang, AVRational tb);

int webvtt_write_mdat_sub_pkt(AVPacket *pkt, const char *lang, AVRational tb);

void webvtt_write_vttc_tag(AVIOContext *pb, const char *iden, const char *payl, const char *sttg);

/* write vttc subtag: iden, payl, sttg */
void webvtt_write_vttc_subtag(AVIOContext *pb, const char *tag, const char *data);

void webvtt_write_vtte_tag(AVIOContext *pb);

int ocr_subtitle(AVSubtitle *sub);

#endif /* SUBTITLE_COMMON_H */