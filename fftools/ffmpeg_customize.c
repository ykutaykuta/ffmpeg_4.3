// #include "ffmpeg.h"

// int buffer_context_init_filter(void)
// {
//     char args[512];
//     char tmp[32];
//     int i, ret = 0;
//     InputStream *v_ist = NULL;
//     InputStream *a_ist = NULL;
//     const AVFilter *v_src;
//     const AVFilter *scale;
//     const AVFilter *v_sink;
//     const AVFilter *a_src;
//     const AVFilter *aformat;
//     const AVFilter *a_sink;
//     AVFilterContext *scale_ctx = NULL;
//     AVFilterContext *aformat_ctx = NULL;

//     for (i = 0; i < nb_input_streams; i++)
//     {
//         if (input_streams[i]->dec_ctx->codec->type == AVMEDIA_TYPE_VIDEO)
//         {
//             v_ist = input_streams[i];
//         }
//         else if (input_streams[i]->dec_ctx->codec->type == AVMEDIA_TYPE_AUDIO)
//         {
//             a_ist = input_streams[i];
//         }
//     }
//     if (!v_ist || !a_ist)
//         return AVERROR(EIO);

//     /* init filer for video */
//     buf_ctx->v_filter_graph = avfilter_graph_alloc();
//     if (!buf_ctx->v_filter_graph)
//         return AVERROR(ENOMEM);
//     v_src = avfilter_get_by_name("buffer");
//     buf_ctx->v_filter_ctx_src = avfilter_graph_alloc_filter(buf_ctx->v_filter_graph, v_src, "v_src");
//     snprintf(args, sizeof(args), "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
//              buf_ctx->v_dec_ctx->width, buf_ctx->v_dec_ctx->height, buf_ctx->v_dec_ctx->pix_fmt,
//              buf_ctx->fmt_ctx->streams[buf_ctx->v_stream_idx]->time_base.num, buf_ctx->fmt_ctx->streams[buf_ctx->v_stream_idx]->time_base.den,
//              buf_ctx->v_dec_ctx->sample_aspect_ratio.num, buf_ctx->v_dec_ctx->sample_aspect_ratio.den);
//     ret = avfilter_init_str(buf_ctx->v_filter_ctx_src, args);
//     if (ret < 0)
//         return ret;
//     /* rescale buffer */
//     scale = avfilter_get_by_name("scale");
//     scale_ctx = avfilter_graph_alloc_filter(buf_ctx->v_filter_graph, scale, "scale");
//     snprintf(args, sizeof(args), "w=%d:h=%d", v_ist->dec_ctx->width, v_ist->dec_ctx->height);
//     ret = avfilter_init_str(scale_ctx, args);
//     if (ret < 0)
//         return ret;
//     /* sink buffer */
//     v_sink = avfilter_get_by_name("buffersink");
//     buf_ctx->v_filter_ctx_sink = avfilter_graph_alloc_filter(buf_ctx->v_filter_graph, v_sink, "v_sink");
//     avfilter_init_str(buf_ctx->v_filter_ctx_sink, NULL);
//     /* link video filter */
//     ret = avfilter_link(buf_ctx->v_filter_ctx_src, 0, scale_ctx, 0);
//     if (ret < 0)
//         return ret;
//     ret = avfilter_link(scale_ctx, 0, buf_ctx->v_filter_ctx_sink, 0);
//     if (ret < 0)
//         return ret;
//     /* config graph */
//     ret = avfilter_graph_config(buf_ctx->v_filter_graph, NULL);
//     if (ret < 0)
//         return ret;

//     /* init filer for audio */
//     buf_ctx->a_filter_graph = avfilter_graph_alloc();
//     if (!buf_ctx->a_filter_graph)
//         return AVERROR(ENOMEM);
//     /* src buffer */
//     a_src = avfilter_get_by_name("abuffer");
//     buf_ctx->a_filter_ctx_src = avfilter_graph_alloc_filter(buf_ctx->a_filter_graph, a_src, "a_src");
//     av_get_channel_layout_string(tmp, sizeof(tmp), buf_ctx->a_dec_ctx->channels, buf_ctx->a_dec_ctx->channel_layout);
//     snprintf(args, sizeof(args), "sample_rate=%d:sample_fmt=%s:channel_layout=%s",
//              buf_ctx->a_dec_ctx->sample_rate,
//              av_get_sample_fmt_name(buf_ctx->a_dec_ctx->sample_fmt),
//              tmp);
//     ret = avfilter_init_str(buf_ctx->a_filter_ctx_src, args);
//     if (ret < 0)
//         return ret;
//     /* rescale buffer */
//     aformat = avfilter_get_by_name("aformat");
//     aformat_ctx = avfilter_graph_alloc_filter(buf_ctx->a_filter_graph, aformat, "aformat");
//     av_get_channel_layout_string(tmp, sizeof(tmp), a_ist->dec_ctx->channels, a_ist->dec_ctx->channel_layout);
//     snprintf(args, sizeof(args), "sample_rates=%d:sample_fmts=%s:channel_layouts=%s",
//              a_ist->dec_ctx->sample_rate,
//              av_get_sample_fmt_name(a_ist->dec_ctx->sample_fmt),
//              tmp);
//     ret = avfilter_init_str(aformat_ctx, args);
//     if (ret < 0)
//         return ret;
//     /* sink buffer */
//     a_sink = avfilter_get_by_name("abuffersink");
//     buf_ctx->a_filter_ctx_sink = avfilter_graph_alloc_filter(buf_ctx->a_filter_graph, a_sink, "a_sink");
//     avfilter_init_str(buf_ctx->a_filter_ctx_sink, NULL);
//     /* link video filter */
//     ret = avfilter_link(buf_ctx->a_filter_ctx_src, 0, aformat_ctx, 0);
//     if (ret < 0)
//         return ret;
//     ret = avfilter_link(aformat_ctx, 0, buf_ctx->a_filter_ctx_sink, 0);
//     if (ret < 0)
//         return ret;
//     /* config graph */
//     ret = avfilter_graph_config(buf_ctx->a_filter_graph, NULL);
//     if (ret < 0)
//         return ret;

//     /* init queue for frame */
//     ret = fq_init(&buf_ctx->v_queue);
//     if (ret < 0)
//         return ret;
//     ret = fq_init(&buf_ctx->a_queue);
//     if (ret < 0)
//         return ret;

//     buf_ctx->inited = 1;

//     return ret;
// }

// void buffer_context_free(void)
// {
//     avfilter_graph_free(&buf_ctx->v_filter_graph);
//     avfilter_graph_free(&buf_ctx->a_filter_graph);
//     avformat_free_context(buf_ctx->fmt_ctx);
//     avcodec_free_context(&buf_ctx->v_dec_ctx);
//     avcodec_free_context(&buf_ctx->a_dec_ctx);
//     fq_destroy(&buf_ctx->v_queue);
//     fq_destroy(&buf_ctx->a_queue);
//     av_freep(&buf_ctx->buf_file);
//     free(buf_ctx);
//     buf_ctx = NULL;
// }

// int buffer_context_get_packet(AVPacket *pkt)
// {
//     int ret;
//     ret = av_read_frame(buf_ctx->fmt_ctx, pkt);
//     return ret;
// }