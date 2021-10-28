#include "libavutil/avassert.h"
#include "libavutil/mathematics.h"
#include "libavutil/parseutils.h"
#include "libavutil/avstring.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/random_seed.h"
#include "libavutil/opt.h"
#include "libavutil/log.h"
#include "libavutil/time.h"
#include "libavutil/time_internal.h"

#include "avformat.h"
#include "avio_internal.h"
#include "integer.h"

typedef struct MSSContext
{
    /* data */
} MSSContext;

static int mss_init(AVFormatContext *s)
{

}

static int mss_write_header(AVFormatContext *s)
{

}

static int mss_write_packet(AVFormatContext *s, AVPacket *pkt)
{

}

static int mss_write_trailer(AVFormatContext *s)
{

}

static void mss_deinit(AVFormatContext *s)
{

}

#define OFFSET(x) offsetof(MSSContext, x)
#define E AV_OPT_FLAG_ENCODING_PARAM
static const AVOption options[] = {

};

static const AVClass mss_class = {
    .class_name = "mss_muxer",
    .item_name = av_default_item_name,
    .option = options,
    .version = LIBAVUTIL_VERSION_INT,
};

AVOutputFormat ff_mss_muxer = {
    .name = "mss",
    .long_name = NULL_IF_CONFIG_SMALL("Microsoft Smooth Streaming"),
    .extensions = "ismc",
    .priv_data_size = sizeof(MSSContext),
    .audio_codec = AV_CODEC_ID_AAC,
    .video_codec = AV_CODEC_ID_H264,
    .flags = AVFMT_GLOBALHEADER,
    .init = mss_init,
    .write_header = mss_write_header,
    .write_packet = mss_write_packet,
    .write_trailer = mss_write_trailer,
    .deinit = mss_deinit,
    .priv_class = &mss_class,
};