#include <iostream>
#include <cstdint>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <cstdio>
#include <cstring>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/pixdesc.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

static void print_error(int err, const char* context) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(err, errbuf, sizeof(errbuf));
    std::cerr << "Error " << context << ": " << errbuf << " (" << err << ")\n";
}

struct CompressResult {
    bool success = false;
    int64_t output_size = 0;
    std::string error_msg;
    int crf_used = 0;
};

struct VideoContext {
    AVCodecContext* dec_ctx = nullptr;
    AVCodecContext* enc_ctx = nullptr;
    AVStream* in_stream = nullptr;
    AVStream* out_stream = nullptr;
    int in_index = -1;
    int out_index = -1;
    SwsContext* sws_ctx = nullptr;
    AVFrame* enc_frame = nullptr;
    int64_t next_pts = 0;
    int frame_count = 0;
};

struct AudioContext {
    AVCodecContext* dec_ctx = nullptr;
    AVStream* in_stream = nullptr;
    AVStream* out_stream = nullptr;
    int in_index = -1;
    int out_index = -1;
    int packet_count = 0;
};

static void cleanup_video(VideoContext& v) {
    av_frame_free(&v.enc_frame);
    sws_freeContext(v.sws_ctx);
    avcodec_free_context(&v.dec_ctx);
    avcodec_free_context(&v.enc_ctx);
}

static void cleanup_audio(AudioContext& a) {
    avcodec_free_context(&a.dec_ctx);
}

static void cleanup_output(AVFormatContext* ofmt_ctx) {
    if (ofmt_ctx) {
        if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE) && ofmt_ctx->pb) {
            avio_closep(&ofmt_ctx->pb);
        }
        avformat_free_context(ofmt_ctx);
    }
}

static int64_t get_file_size(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return st.st_size;
    }
    return -1;
}

static CompressResult compressVideo(const char* input_path, const char* output_path, int crf, const char* preset) {
    CompressResult result;
    result.crf_used = crf;

    AVFormatContext* ifmt_ctx = nullptr;
    AVFormatContext* ofmt_ctx = nullptr;
    AVPacket* packet = nullptr;
    AVFrame* dec_frame = nullptr;

    VideoContext vctx;
    AudioContext actx;
    bool has_audio = false;

    const AVCodec* video_dec = nullptr;
    const AVCodec* video_enc = nullptr;

    int ret = avformat_open_input(&ifmt_ctx, input_path, nullptr, nullptr);
    if (ret < 0) {
        result.error_msg = "Failed to open input";
        goto fail;
    }

    ret = avformat_find_stream_info(ifmt_ctx, nullptr);
    if (ret < 0) {
        result.error_msg = "Failed to find stream info";
        goto fail;
    }

    for (unsigned i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVMediaType type = ifmt_ctx->streams[i]->codecpar->codec_type;
        if (type == AVMEDIA_TYPE_VIDEO && vctx.in_index == -1) {
            vctx.in_index = i;
            vctx.in_stream = ifmt_ctx->streams[i];
        } else if (type == AVMEDIA_TYPE_AUDIO && actx.in_index == -1) {
            actx.in_index = i;
            actx.in_stream = ifmt_ctx->streams[i];
        }
    }

    if (vctx.in_index == -1) {
        result.error_msg = "No video stream found";
        goto fail;
    }

    has_audio = actx.in_index != -1;

    video_dec = avcodec_find_decoder(vctx.in_stream->codecpar->codec_id);
    if (!video_dec) { result.error_msg = "Video decoder not found"; goto fail; }
    vctx.dec_ctx = avcodec_alloc_context3(video_dec);
    if (!vctx.dec_ctx) { result.error_msg = "Failed to alloc video dec ctx"; goto fail; }
    ret = avcodec_parameters_to_context(vctx.dec_ctx, vctx.in_stream->codecpar);
    if (ret < 0) { print_error(ret, "video dec params"); result.error_msg = "Failed video dec params"; goto fail; }
    ret = avcodec_open2(vctx.dec_ctx, video_dec, nullptr);
    if (ret < 0) { print_error(ret, "video dec open"); result.error_msg = "Failed to open video decoder"; goto fail; }

    video_enc = avcodec_find_encoder_by_name("libx264");
    if (!video_enc) { result.error_msg = "libx264 encoder not found"; goto fail; }
    vctx.enc_ctx = avcodec_alloc_context3(video_enc);
    if (!vctx.enc_ctx) { result.error_msg = "Failed to alloc video enc ctx"; goto fail; }

    vctx.enc_ctx->width = vctx.dec_ctx->width;
    vctx.enc_ctx->height = vctx.dec_ctx->height;
    vctx.enc_ctx->pix_fmt = video_enc->pix_fmts ? video_enc->pix_fmts[0] : AV_PIX_FMT_YUV420P;
    vctx.enc_ctx->time_base = (AVRational){1, 30};
    vctx.enc_ctx->framerate = (AVRational){30, 1};
    vctx.enc_ctx->gop_size = 30;
    vctx.enc_ctx->max_b_frames = 2;
    vctx.enc_ctx->thread_count = 0;

    av_opt_set(vctx.enc_ctx->priv_data, "crf", std::to_string(crf).c_str(), 0);
    av_opt_set(vctx.enc_ctx->priv_data, "preset", preset, 0);
    av_opt_set(vctx.enc_ctx->priv_data, "profile", "high", 0);

    ret = avcodec_open2(vctx.enc_ctx, video_enc, nullptr);
    if (ret < 0) { print_error(ret, "video enc open"); result.error_msg = "Failed to open video encoder"; goto fail; }

    if (vctx.dec_ctx->pix_fmt != vctx.enc_ctx->pix_fmt) {
        vctx.sws_ctx = sws_getContext(
            vctx.dec_ctx->width, vctx.dec_ctx->height, vctx.dec_ctx->pix_fmt,
            vctx.enc_ctx->width, vctx.enc_ctx->height, vctx.enc_ctx->pix_fmt,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );
        if (!vctx.sws_ctx) { result.error_msg = "sws_getContext failed"; goto fail; }
    }

    vctx.enc_frame = av_frame_alloc();
    if (!vctx.enc_frame) { result.error_msg = "Failed to alloc enc frame"; goto fail; }
    vctx.enc_frame->format = vctx.enc_ctx->pix_fmt;
    vctx.enc_frame->width = vctx.enc_ctx->width;
    vctx.enc_frame->height = vctx.enc_ctx->height;
    ret = av_frame_get_buffer(vctx.enc_frame, 32);
    if (ret < 0) { print_error(ret, "frame buffer"); result.error_msg = "Failed to alloc frame buffer"; goto fail; }

    if (has_audio) {
        const AVCodec* audio_dec = avcodec_find_decoder(actx.in_stream->codecpar->codec_id);
        if (audio_dec) {
            actx.dec_ctx = avcodec_alloc_context3(audio_dec);
            if (actx.dec_ctx) {
                ret = avcodec_parameters_to_context(actx.dec_ctx, actx.in_stream->codecpar);
                if (ret >= 0) {
                    ret = avcodec_open2(actx.dec_ctx, audio_dec, nullptr);
                    if (ret < 0) {
                        avcodec_free_context(&actx.dec_ctx);
                        actx.dec_ctx = nullptr;
                    }
                } else {
                    avcodec_free_context(&actx.dec_ctx);
                    actx.dec_ctx = nullptr;
                }
            }
        }
    }

    avformat_alloc_output_context2(&ofmt_ctx, nullptr, "mp4", output_path);
    if (!ofmt_ctx) { result.error_msg = "Failed to create output context"; goto fail; }

    vctx.out_stream = avformat_new_stream(ofmt_ctx, nullptr);
    if (!vctx.out_stream) { result.error_msg = "Failed to create output video stream"; goto fail; }
    vctx.out_index = vctx.out_stream->index;

    ret = avcodec_parameters_from_context(vctx.out_stream->codecpar, vctx.enc_ctx);
    if (ret < 0) { print_error(ret, "video out params"); result.error_msg = "Failed video out params"; goto fail; }
    vctx.out_stream->time_base = vctx.enc_ctx->time_base;

    if (has_audio) {
        actx.out_stream = avformat_new_stream(ofmt_ctx, nullptr);
        if (!actx.out_stream) { result.error_msg = "Failed to create output audio stream"; goto fail; }
        actx.out_index = actx.out_stream->index;

        ret = avcodec_parameters_copy(actx.out_stream->codecpar, actx.in_stream->codecpar);
        if (ret < 0) { print_error(ret, "audio copy params"); result.error_msg = "Failed audio copy params"; goto fail; }
        actx.out_stream->codecpar->codec_tag = 0;
        actx.out_stream->time_base = actx.in_stream->time_base;
    }

    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, output_path, AVIO_FLAG_WRITE);
        if (ret < 0) { print_error(ret, "avio_open"); result.error_msg = "Failed to open output file"; goto fail; }
    }

    ret = avformat_write_header(ofmt_ctx, nullptr);
    if (ret < 0) { print_error(ret, "write header"); result.error_msg = "Failed to write header"; goto fail; }

    packet = av_packet_alloc();
    dec_frame = av_frame_alloc();
    if (!packet || !dec_frame) { result.error_msg = "Failed to alloc packet/frame"; goto fail; }

    while (av_read_frame(ifmt_ctx, packet) >= 0) {
        if (packet->stream_index == vctx.in_index) {
            ret = avcodec_send_packet(vctx.dec_ctx, packet);
            if (ret < 0) { print_error(ret, "video send packet"); break; }

            while (ret >= 0) {
                ret = avcodec_receive_frame(vctx.dec_ctx, dec_frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
                if (ret < 0) { print_error(ret, "video recv frame"); break; }

                AVFrame* frame_to_encode = dec_frame;
                if (vctx.sws_ctx) {
                    ret = av_frame_make_writable(vctx.enc_frame);
                    if (ret < 0) { print_error(ret, "make writable"); break; }
                    sws_scale(vctx.sws_ctx, (const uint8_t* const*)dec_frame->data, dec_frame->linesize,
                              0, vctx.dec_ctx->height, vctx.enc_frame->data, vctx.enc_frame->linesize);
                    frame_to_encode = vctx.enc_frame;
                }

                frame_to_encode->pts = vctx.next_pts++;
                ret = avcodec_send_frame(vctx.enc_ctx, frame_to_encode);
                if (ret < 0) { print_error(ret, "video send frame"); break; }

                while (ret >= 0) {
                    ret = avcodec_receive_packet(vctx.enc_ctx, packet);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
                    if (ret < 0) { print_error(ret, "video recv packet"); break; }

                    packet->stream_index = vctx.out_index;
                    av_packet_rescale_ts(packet, vctx.enc_ctx->time_base, vctx.out_stream->time_base);
                    ret = av_interleaved_write_frame(ofmt_ctx, packet);
                    if (ret < 0) { print_error(ret, "write video frame"); break; }
                    vctx.frame_count++;
                    av_packet_unref(packet);
                }
            }
        } else if (has_audio && packet->stream_index == actx.in_index) {
            packet->stream_index = actx.out_index;
            av_packet_rescale_ts(packet, actx.in_stream->time_base, actx.out_stream->time_base);
            ret = av_interleaved_write_frame(ofmt_ctx, packet);
            if (ret < 0) { print_error(ret, "write audio frame"); break; }
            actx.packet_count++;
        }
        av_packet_unref(packet);
    }

    ret = avcodec_send_frame(vctx.enc_ctx, nullptr);
    while (ret >= 0) {
        ret = avcodec_receive_packet(vctx.enc_ctx, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        if (ret < 0) { print_error(ret, "flush video recv"); break; }
        packet->stream_index = vctx.out_index;
        av_packet_rescale_ts(packet, vctx.enc_ctx->time_base, vctx.out_stream->time_base);
        ret = av_interleaved_write_frame(ofmt_ctx, packet);
        if (ret < 0) { print_error(ret, "write flush frame"); break; }
        vctx.frame_count++;
        av_packet_unref(packet);
    }

    av_write_trailer(ofmt_ctx);

    result.success = true;
    std::cout << "  Video frames encoded: " << vctx.frame_count << "\n";
    if (has_audio) {
        std::cout << "  Audio packets copied: " << actx.packet_count << "\n";
    }

fail:
    av_frame_free(&dec_frame);
    av_packet_free(&packet);
    cleanup_video(vctx);
    cleanup_audio(actx);
    cleanup_output(ofmt_ctx);
    avformat_close_input(&ifmt_ctx);

    if (result.success) {
        result.output_size = get_file_size(output_path);
    }
    return result;
}

static bool try_compress(const char* input_path, const char* output_path, int crf, const char* preset, int64_t input_size) {
    std::cout << "\nAttempt with CRF " << crf << " (preset: " << preset << ")\n";
    std::cout << "  Input size: " << (input_size / (1024.0 * 1024.0)) << " MB\n";

    CompressResult result = compressVideo(input_path, output_path, crf, preset);

    if (!result.success) {
        std::cerr << "  Encoding failed: " << result.error_msg << "\n";
        return false;
    }

    double output_mb = result.output_size / (1024.0 * 1024.0);
    double reduction = ((input_size - result.output_size) / (double)input_size) * 100.0;

    std::cout << "  Output size: " << output_mb << " MB\n";
    std::cout << "  Reduction: " << (reduction >= 0 ? "+" : "") << reduction << "%\n";

    if (result.output_size < input_size) {
        std::cout << "  SUCCESS: Output is smaller than input\n";
        return true;
    } else {
        std::cout << "  FAILED: Output is not smaller than input\n";
        return false;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: ./compressor <input> <output> [crf] [preset=medium]\n";
        std::cerr << "  If CRF is omitted, automatic compression strategy is used (CRF 28, 30, 32)\n";
        std::cerr << "  If CRF is provided, single-pass encoding with that CRF is used\n";
        return 1;
    }

    const char* input = argv[1];
    const char* output = argv[2];

    int64_t input_size = get_file_size(input);
    if (input_size < 0) {
        std::cerr << "Failed to get input file size\n";
        return 1;
    }

    bool auto_mode = (argc <= 3);
    int crf = auto_mode ? 28 : std::stoi(argv[3]);
    const char* preset = (argc > 4) ? argv[4] : "medium";

    std::cout << "Input: " << input << "\n";
    std::cout << "Output: " << output << "\n";
    std::cout << "Input size: " << (input_size / (1024.0 * 1024.0)) << " MB\n";

    if (auto_mode) {
        std::cout << "Mode: Automatic compression (CRF 28 -> 30 -> 32)\n";

        std::vector<int> crf_values = {28, 30, 32};
        bool success = false;
        std::string best_temp_output;
        int best_crf = 0;
        int64_t best_size = input_size;

        for (size_t i = 0; i < crf_values.size(); i++) {
            int current_crf = crf_values[i];
            std::string temp_output = std::string(output) + ".tmp" + std::to_string(i);

            bool attempt_success = try_compress(input, temp_output.c_str(), current_crf, preset, input_size);

            int64_t out_size = get_file_size(temp_output.c_str());
            if (attempt_success && out_size >= 0 && out_size < best_size) {
                best_size = out_size;
                best_temp_output = temp_output;
                best_crf = current_crf;
                success = true;
            }

            if (out_size >= 0 && out_size >= input_size) {
                std::remove(temp_output.c_str());
            }
        }

        if (success && !best_temp_output.empty()) {
            if (std::rename(best_temp_output.c_str(), output) != 0) {
                std::cerr << "Failed to move temp file to output\n";
                std::remove(best_temp_output.c_str());
                return 1;
            }
            double reduction = ((input_size - best_size) / (double)input_size) * 100.0;
            std::cout << "\n=== COMPRESSION SUCCESSFUL ===\n";
            std::cout << "Best CRF: " << best_crf << "\n";
            std::cout << "Input size:  " << (input_size / (1024.0 * 1024.0)) << " MB\n";
            std::cout << "Output size: " << (best_size / (1024.0 * 1024.0)) << " MB\n";
            std::cout << "Reduction:   " << reduction << "%\n";
            return 0;
        } else {
            std::cout << "\n=== NO SIZE REDUCTION ACHIEVED ===\n";
            std::cout << "CRF 28 -> " << (get_file_size((std::string(output) + ".tmp0").c_str()) / (1024.0 * 1024.0)) << " MB\n";
            std::cout << "CRF 30 -> " << (get_file_size((std::string(output) + ".tmp1").c_str()) / (1024.0 * 1024.0)) << " MB\n";
            std::cout << "CRF 32 -> " << (get_file_size((std::string(output) + ".tmp2").c_str()) / (1024.0 * 1024.0)) << " MB\n";
            std::cout << "Original file will be kept.\n";
            return 1;
        }
    } else {
        std::cout << "Mode: Single-pass (CRF=" << crf << ", preset=" << preset << ")\n";
        CompressResult result = compressVideo(input, output, crf, preset);

        if (!result.success) {
            std::cerr << "Failed: " << result.error_msg << "\n";
            return 1;
        }

        double output_mb = result.output_size / (1024.0 * 1024.0);
        double reduction = ((input_size - result.output_size) / (double)input_size) * 100.0;

        std::cout << "\n=== ENCODING COMPLETE ===\n";
        std::cout << "CRF: " << crf << "\n";
        std::cout << "Input size:  " << (input_size / (1024.0 * 1024.0)) << " MB\n";
        std::cout << "Output size: " << output_mb << " MB\n";
        std::cout << "Reduction:   " << (reduction >= 0 ? "+" : "") << reduction << "%\n";

        if (result.output_size >= input_size) {
            std::cout << "Warning: Output is not smaller than input\n";
        }
        return 0;
    }
}