#include <iostream>
#include <cstdint>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/samplefmt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/pixdesc.h>
}

static void print_error(int err, const char* context) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(err, errbuf, sizeof(errbuf));
    std::cerr << "Error " << context << ": " << errbuf << " (" << err << ")\n";
}

struct DecoderContext {
    AVCodecContext* video_ctx = nullptr;
    AVCodecContext* audio_ctx = nullptr;
    AVStream* video_stream = nullptr;
    AVStream* audio_stream = nullptr;
    int video_stream_index = -1;
    int audio_stream_index = -1;
    int video_frames = 0;
    int audio_frames = 0;
    int64_t total_audio_samples = 0;
};

int open_decoder(AVFormatContext* fmt_ctx, int stream_index, AVCodecContext** out_ctx) {
    AVStream* stream = fmt_ctx->streams[stream_index];
    AVCodecParameters* params = stream->codecpar;

    const AVCodec* decoder = avcodec_find_decoder(params->codec_id);
    if (!decoder) return -1;

    AVCodecContext* ctx = avcodec_alloc_context3(decoder);
    if (!ctx) return -1;

    int ret = avcodec_parameters_to_context(ctx, params);
    if (ret < 0) {
        avcodec_free_context(&ctx);
        return ret;
    }

    ret = avcodec_open2(ctx, decoder, nullptr);
    if (ret < 0) {
        avcodec_free_context(&ctx);
        return ret;
    }

    *out_ctx = ctx;
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: ./decoder <input-video>\n";
        return 1;
    }

    const char* input_file = argv[1];
    AVFormatContext* format_context = nullptr;

    int ret = avformat_open_input(&format_context, input_file, nullptr, nullptr);
    if (ret < 0) {
        print_error(ret, "avformat_open_input");
        return 1;
    }

    ret = avformat_find_stream_info(format_context, nullptr);
    if (ret < 0) {
        print_error(ret, "avformat_find_stream_info");
        avformat_close_input(&format_context);
        return 1;
    }

    DecoderContext dec;

    for (unsigned int i = 0; i < format_context->nb_streams; i++) {
        AVMediaType type = format_context->streams[i]->codecpar->codec_type;
        if (type == AVMEDIA_TYPE_VIDEO && dec.video_stream_index == -1) {
            dec.video_stream_index = i;
            dec.video_stream = format_context->streams[i];
        } else if (type == AVMEDIA_TYPE_AUDIO && dec.audio_stream_index == -1) {
            dec.audio_stream_index = i;
            dec.audio_stream = format_context->streams[i];
        }
    }

    if (dec.video_stream_index == -1 || dec.audio_stream_index == -1) {
        std::cerr << "Missing video or audio stream\n";
        avformat_close_input(&format_context);
        return 1;
    }

    ret = open_decoder(format_context, dec.video_stream_index, &dec.video_ctx);
    if (ret < 0) {
        print_error(ret, "open video decoder");
        avformat_close_input(&format_context);
        return 1;
    }

    ret = open_decoder(format_context, dec.audio_stream_index, &dec.audio_ctx);
    if (ret < 0) {
        print_error(ret, "open audio decoder");
        avcodec_free_context(&dec.video_ctx);
        avformat_close_input(&format_context);
        return 1;
    }

    std::cout << "\n=== VIDEO DECODER ===\n";
    std::cout << "Codec: " << avcodec_get_name(dec.video_ctx->codec_id) << "\n";
    std::cout << "Resolution: " << dec.video_ctx->width << "x" << dec.video_ctx->height << "\n";
    std::cout << "Pixel format: " << av_get_pix_fmt_name(dec.video_ctx->pix_fmt) << "\n";
    std::cout << "Time base: " << av_q2d(dec.video_stream->time_base) << "\n";

    std::cout << "\n=== AUDIO DECODER ===\n";
    std::cout << "Codec: " << avcodec_get_name(dec.audio_ctx->codec_id) << "\n";
    std::cout << "Sample rate: " << dec.audio_ctx->sample_rate << " Hz\n";
    std::cout << "Channels: " << dec.audio_ctx->ch_layout.nb_channels << "\n";
    std::cout << "Sample format: " << av_get_sample_fmt_name(dec.audio_ctx->sample_fmt) << "\n";
    std::cout << "Time base: " << av_q2d(dec.audio_stream->time_base) << "\n";
    std::cout << "Frame size: " << dec.audio_ctx->frame_size << " samples\n";

    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();

    while (av_read_frame(format_context, packet) >= 0) {
        AVCodecContext* ctx = nullptr;
        AVStream* stream = nullptr;

        if (packet->stream_index == dec.video_stream_index) {
            ctx = dec.video_ctx;
            stream = dec.video_stream;
        } else if (packet->stream_index == dec.audio_stream_index) {
            ctx = dec.audio_ctx;
            stream = dec.audio_stream;
        }

        if (ctx) {
            ret = avcodec_send_packet(ctx, packet);
            if (ret < 0) {
                print_error(ret, "avcodec_send_packet");
            } else {
                while (ret >= 0) {
                    ret = avcodec_receive_frame(ctx, frame);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
                    if (ret < 0) {
                        print_error(ret, "avcodec_receive_frame");
                        break;
                    }

                    if (packet->stream_index == dec.video_stream_index) {
                        dec.video_frames++;
                        if (dec.video_frames <= 3) {
                            std::cout << "  VFrame " << dec.video_frames
                                      << ": pts=" << frame->pts
                                      << ", type=" << av_get_picture_type_char(frame->pict_type) << "\n";
                        }
                    } else {
                        dec.audio_frames++;
                        dec.total_audio_samples += frame->nb_samples;
                        if (dec.audio_frames <= 3) {
                            std::cout << "  AFrame " << dec.audio_frames
                                      << ": pts=" << frame->pts
                                      << ", samples=" << frame->nb_samples
                                      << ", pts_time=" << (frame->pts * av_q2d(stream->time_base)) << "\n";
                        }
                    }
                }
            }
        }
        av_packet_unref(packet);
    }

    std::cout << "\nFlushing decoders...\n";
    for (int pass = 0; pass < 2; pass++) {
        AVCodecContext* ctx = (pass == 0) ? dec.video_ctx : dec.audio_ctx;
        if (!ctx) continue;

        ret = avcodec_send_packet(ctx, nullptr);
        if (ret < 0) continue;

        while (ret >= 0) {
            ret = avcodec_receive_frame(ctx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
            if (ret < 0) break;

            if (pass == 0) dec.video_frames++;
            else {
                dec.audio_frames++;
                dec.total_audio_samples += frame->nb_samples;
            }
        }
    }

    std::cout << "\n=== SUMMARY ===\n";
    std::cout << "Video frames: " << dec.video_frames << "\n";
    std::cout << "Audio frames: " << dec.audio_frames << "\n";
    std::cout << "Total audio samples: " << dec.total_audio_samples << "\n";

    double audio_duration = dec.total_audio_samples / (double)dec.audio_ctx->sample_rate;
    std::cout << "Audio duration: " << audio_duration << " sec\n";

    double video_duration = dec.video_frames * av_q2d(dec.video_stream->time_base) * dec.video_stream->avg_frame_rate.num / dec.video_stream->avg_frame_rate.den;
    std::cout << "Video duration (estimated): " << video_duration << " sec\n";

    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&dec.video_ctx);
    avcodec_free_context(&dec.audio_ctx);
    avformat_close_input(&format_context);

    return 0;
}