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
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
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
};

int open_decoder(AVFormatContext* fmt_ctx, int stream_index, AVCodecContext** out_ctx) {
    AVStream* stream = fmt_ctx->streams[stream_index];
    AVCodecParameters* params = stream->codecpar;

    const AVCodec* decoder = avcodec_find_decoder(params->codec_id);
    if (!decoder) return -1;

    AVCodecContext* ctx = avcodec_alloc_context3(decoder);
    if (!ctx) return -1;

    int ret = avcodec_parameters_to_context(ctx, params);
    if (ret < 0) { avcodec_free_context(&ctx); return ret; }

    ret = avcodec_open2(ctx, decoder, nullptr);
    if (ret < 0) { avcodec_free_context(&ctx); return ret; }

    *out_ctx = ctx;
    return 0;
}

int open_encoder(AVCodecContext* dec_ctx, AVCodecContext** out_enc_ctx, const char* encoder_name, int crf, const char* preset) {
    const AVCodec* encoder = avcodec_find_encoder_by_name(encoder_name);
    if (!encoder) {
        std::cerr << "Encoder " << encoder_name << " not found\n";
        return -1;
    }

    AVCodecContext* enc_ctx = avcodec_alloc_context3(encoder);
    if (!enc_ctx) return -1;

    enc_ctx->width = dec_ctx->width;
    enc_ctx->height = dec_ctx->height;
    enc_ctx->pix_fmt = encoder->pix_fmts ? encoder->pix_fmts[0] : AV_PIX_FMT_YUV420P;
    enc_ctx->time_base = (AVRational){1, 30};
    enc_ctx->framerate = (AVRational){30, 1};
    enc_ctx->gop_size = 30;
    enc_ctx->max_b_frames = 2;
    enc_ctx->thread_count = 0;

    if (encoder->id == AV_CODEC_ID_H264) {
        av_opt_set(enc_ctx->priv_data, "crf", std::to_string(crf).c_str(), 0);
        av_opt_set(enc_ctx->priv_data, "preset", preset, 0);
        av_opt_set(enc_ctx->priv_data, "profile", "high", 0);
    }

    int ret = avcodec_open2(enc_ctx, encoder, nullptr);
    if (ret < 0) {
        print_error(ret, "avcodec_open2 (encoder)");
        avcodec_free_context(&enc_ctx);
        return ret;
    }

    *out_enc_ctx = enc_ctx;
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: ./encoder <input> <output> [crf=23] [preset=medium]\n";
        return 1;
    }

    const char* input_file = argv[1];
    const char* output_file = argv[2];
    int crf = argc > 3 ? std::stoi(argv[3]) : 23;
    const char* preset = argc > 4 ? argv[4] : "medium";

    AVFormatContext* ifmt_ctx = nullptr;
    int ret = avformat_open_input(&ifmt_ctx, input_file, nullptr, nullptr);
    if (ret < 0) { print_error(ret, "avformat_open_input"); return 1; }

    ret = avformat_find_stream_info(ifmt_ctx, nullptr);
    if (ret < 0) { print_error(ret, "avformat_find_stream_info"); avformat_close_input(&ifmt_ctx); return 1; }

    DecoderContext dec;
    for (unsigned i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVMediaType t = ifmt_ctx->streams[i]->codecpar->codec_type;
        if (t == AVMEDIA_TYPE_VIDEO && dec.video_stream_index == -1) {
            dec.video_stream_index = i; dec.video_stream = ifmt_ctx->streams[i];
        } else if (t == AVMEDIA_TYPE_AUDIO && dec.audio_stream_index == -1) {
            dec.audio_stream_index = i; dec.audio_stream = ifmt_ctx->streams[i];
        }
    }

    if (dec.video_stream_index == -1) { std::cerr << "No video stream\n"; return 1; }

    ret = open_decoder(ifmt_ctx, dec.video_stream_index, &dec.video_ctx);
    if (ret < 0) { print_error(ret, "open video decoder"); return 1; }

    AVCodecContext* video_enc_ctx = nullptr;
    ret = open_encoder(dec.video_ctx, &video_enc_ctx, "libx264", crf, preset);
    if (ret < 0) { return 1; }

    std::cout << "Video Encoder: " << video_enc_ctx->codec->name << "\n";
    std::cout << "Output pix_fmt: " << av_get_pix_fmt_name(video_enc_ctx->pix_fmt) << "\n";
    std::cout << "CRF: " << crf << ", Preset: " << preset << "\n";

    SwsContext* sws_ctx = nullptr;
    if (dec.video_ctx->pix_fmt != video_enc_ctx->pix_fmt) {
        sws_ctx = sws_getContext(
            dec.video_ctx->width, dec.video_ctx->height, dec.video_ctx->pix_fmt,
            video_enc_ctx->width, video_enc_ctx->height, video_enc_ctx->pix_fmt,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );
        if (!sws_ctx) { std::cerr << "sws_getContext failed\n"; return 1; }
        std::cout << "swscale: " << av_get_pix_fmt_name(dec.video_ctx->pix_fmt) << " -> " << av_get_pix_fmt_name(video_enc_ctx->pix_fmt) << "\n";
    }

    AVFormatContext* ofmt_ctx = nullptr;
    avformat_alloc_output_context2(&ofmt_ctx, nullptr, "mp4", output_file);
    if (!ofmt_ctx) { std::cerr << "Could not create output context\n"; return 1; }

    AVStream* out_video_stream = avformat_new_stream(ofmt_ctx, nullptr);
    if (!out_video_stream) { std::cerr << "Could not create output video stream\n"; return 1; }

    ret = avcodec_parameters_from_context(out_video_stream->codecpar, video_enc_ctx);
    if (ret < 0) { print_error(ret, "avcodec_parameters_from_context (video)"); return 1; }
    out_video_stream->time_base = video_enc_ctx->time_base;

    AVStream* out_audio_stream = nullptr;
    int out_audio_stream_index = -1;
    if (dec.audio_stream_index != -1) {
        out_audio_stream = avformat_new_stream(ofmt_ctx, nullptr);
        if (!out_audio_stream) { std::cerr << "Could not create output audio stream\n"; return 1; }

        ret = avcodec_parameters_copy(out_audio_stream->codecpar, dec.audio_stream->codecpar);
        if (ret < 0) { print_error(ret, "avcodec_parameters_copy (audio)"); return 1; }
        out_audio_stream->codecpar->codec_tag = 0;
        out_audio_stream->time_base = dec.audio_stream->time_base;
        out_audio_stream_index = out_audio_stream->index;
        std::cout << "Audio passthrough: " << avcodec_get_name(dec.audio_stream->codecpar->codec_id) << "\n";
    }

    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, output_file, AVIO_FLAG_WRITE);
        if (ret < 0) { print_error(ret, "avio_open"); return 1; }
    }

    ret = avformat_write_header(ofmt_ctx, nullptr);
    if (ret < 0) { print_error(ret, "avformat_write_header"); return 1; }

    AVPacket* packet = av_packet_alloc();
    AVFrame* dec_frame = av_frame_alloc();
    AVFrame* enc_frame = av_frame_alloc();

    enc_frame->format = video_enc_ctx->pix_fmt;
    enc_frame->width = video_enc_ctx->width;
    enc_frame->height = video_enc_ctx->height;
    ret = av_frame_get_buffer(enc_frame, 32);
    if (ret < 0) { print_error(ret, "av_frame_get_buffer"); return 1; }

    int video_frame_count = 0;
    int audio_packet_count = 0;
    int64_t next_pts = 0;

    while (av_read_frame(ifmt_ctx, packet) >= 0) {
        if (packet->stream_index == dec.video_stream_index) {
            ret = avcodec_send_packet(dec.video_ctx, packet);
            if (ret < 0) { print_error(ret, "avcodec_send_packet (dec)"); break; }

            while (ret >= 0) {
                ret = avcodec_receive_frame(dec.video_ctx, dec_frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
                if (ret < 0) { print_error(ret, "avcodec_receive_frame (dec)"); break; }

                if (sws_ctx) {
                    ret = av_frame_make_writable(enc_frame);
                    if (ret < 0) { print_error(ret, "av_frame_make_writable"); break; }
                    sws_scale(sws_ctx, (const uint8_t* const*)dec_frame->data, dec_frame->linesize,
                              0, dec.video_ctx->height, enc_frame->data, enc_frame->linesize);
                } else {
                    av_frame_ref(enc_frame, dec_frame);
                }

                enc_frame->pts = next_pts++;
                ret = avcodec_send_frame(video_enc_ctx, enc_frame);
                if (ret < 0) { print_error(ret, "avcodec_send_frame (enc)"); break; }

                while (ret >= 0) {
                    ret = avcodec_receive_packet(video_enc_ctx, packet);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
                    if (ret < 0) { print_error(ret, "avcodec_receive_packet (enc)"); break; }

                    packet->stream_index = out_video_stream->index;
                    av_packet_rescale_ts(packet, video_enc_ctx->time_base, out_video_stream->time_base);
                    ret = av_interleaved_write_frame(ofmt_ctx, packet);
                    if (ret < 0) { print_error(ret, "av_interleaved_write_frame (video)"); break; }
                    video_frame_count++;
                    av_packet_unref(packet);
                }
            }
        } else if (packet->stream_index == dec.audio_stream_index && out_audio_stream) {
            packet->stream_index = out_audio_stream_index;
            av_packet_rescale_ts(packet, dec.audio_stream->time_base, out_audio_stream->time_base);
            ret = av_interleaved_write_frame(ofmt_ctx, packet);
            if (ret < 0) { print_error(ret, "av_interleaved_write_frame (audio)"); break; }
            audio_packet_count++;
        }
        av_packet_unref(packet);
    }

    std::cout << "Flushing video encoder...\n";
    ret = avcodec_send_frame(video_enc_ctx, nullptr);
    while (ret >= 0) {
        ret = avcodec_receive_packet(video_enc_ctx, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        if (ret < 0) { print_error(ret, "avcodec_receive_packet (flush)"); break; }
        packet->stream_index = out_video_stream->index;
        av_packet_rescale_ts(packet, video_enc_ctx->time_base, out_video_stream->time_base);
        ret = av_interleaved_write_frame(ofmt_ctx, packet);
        if (ret < 0) { print_error(ret, "av_interleaved_write_frame (flush)"); break; }
        video_frame_count++;
        av_packet_unref(packet);
    }

    av_write_trailer(ofmt_ctx);

    std::cout << "Encoded video frames: " << video_frame_count << "\n";
    std::cout << "Passthrough audio packets: " << audio_packet_count << "\n";

    av_frame_free(&dec_frame);
    av_frame_free(&enc_frame);
    av_packet_free(&packet);
    sws_freeContext(sws_ctx);
    avcodec_free_context(&dec.video_ctx);
    avcodec_free_context(&video_enc_ctx);
    if (dec.audio_ctx) avcodec_free_context(&dec.audio_ctx);
    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) avio_closep(&ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);
    avformat_close_input(&ifmt_ctx);

    return 0;
}