#include <iostream>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

int main(int argc, char* argv[]) {

    if (argc < 2) {
        std::cerr << "Usage: ./demuxer <input-video>\n";
        return 1;
    }

    const char* input_file = argv[1];

    AVFormatContext* format_context = nullptr;

    // Open container
    int ret = avformat_open_input(
        &format_context,
        input_file,
        nullptr,
        nullptr
    );

    if (ret < 0) {
        std::cerr << "Could not open input file\n";
        return 1;
    }

    // Read stream information
    ret = avformat_find_stream_info(
        format_context,
        nullptr
    );

    if (ret < 0) {
        std::cerr << "Could not find stream information\n";

        avformat_close_input(&format_context);

        return 1;
    }

    int video_stream_index = -1;
    int audio_stream_index = -1;

    // Find video and audio streams
    for (unsigned int i = 0;
         i < format_context->nb_streams;
         i++) {

        AVStream* stream =
            format_context->streams[i];

        AVCodecParameters* codec_params =
            stream->codecpar;

        if (codec_params->codec_type ==
            AVMEDIA_TYPE_VIDEO) {

            if (video_stream_index == -1) {
                video_stream_index = i;
            }

        } else if (
            codec_params->codec_type ==
            AVMEDIA_TYPE_AUDIO) {

            if (audio_stream_index == -1) {
                audio_stream_index = i;
            }
        }
    }

    // Print video information
    if (video_stream_index != -1) {

        AVStream* video_stream =
            format_context->streams[video_stream_index];

        AVCodecParameters* video_params =
            video_stream->codecpar;

        std::cout << "\n===== VIDEO STREAM =====\n";

        std::cout << "Stream Index : "
                  << video_stream_index << '\n';

        std::cout << "Codec        : "
                  << avcodec_get_name(
                         video_params->codec_id)
                  << '\n';

        std::cout << "Resolution   : "
                  << video_params->width
                  << "x"
                  << video_params->height
                  << '\n';

        std::cout << "Bitrate      : "
                  << video_params->bit_rate
                  << " bits/s\n";

        if (video_stream->duration != AV_NOPTS_VALUE) {

            double duration =
                video_stream->duration *
                av_q2d(video_stream->time_base);

            std::cout << "Duration     : "
                      << duration
                      << " sec\n";
        }
    }

    // Print audio information
    if (audio_stream_index != -1) {

        AVStream* audio_stream =
            format_context->streams[audio_stream_index];

        AVCodecParameters* audio_params =
            audio_stream->codecpar;

        std::cout << "\n===== AUDIO STREAM =====\n";

        std::cout << "Stream Index : "
                  << audio_stream_index << '\n';

        std::cout << "Codec        : "
                  << avcodec_get_name(
                         audio_params->codec_id)
                  << '\n';

        std::cout << "Sample Rate  : "
                  << audio_params->sample_rate
                  << " Hz\n";

        std::cout << "Channels     : "
                  << audio_params->ch_layout.nb_channels
                  << '\n';

        std::cout << "Bitrate      : "
                  << audio_params->bit_rate
                  << " bits/s\n";

        if (audio_stream->duration != AV_NOPTS_VALUE) {

            double duration =
                audio_stream->duration *
                av_q2d(audio_stream->time_base);

            std::cout << "Duration     : "
                      << duration
                      << " sec\n";
        }
    }

    avformat_close_input(&format_context);

    return 0;
}