#include <iostream>

extern "C" {
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

int main() {
    std::cout << "FFmpeg version info:\n";
    std::cout << "  libavutil:      " << AV_STRINGIFY(LIBAVUTIL_VERSION) << " (" << avutil_version() << ")\n";
    std::cout << "  libavcodec:     " << AV_STRINGIFY(LIBAVCODEC_VERSION) << " (" << avcodec_version() << ")\n";
    std::cout << "  libavformat:    " << AV_STRINGIFY(LIBAVFORMAT_VERSION) << " (" << avformat_version() << ")\n";
    std::cout << "  libswscale:     " << AV_STRINGIFY(LIBSWSCALE_VERSION) << " (" << swscale_version() << ")\n";
    std::cout << "  libswresample:  " << AV_STRINGIFY(LIBSWRESAMPLE_VERSION) << " (" << swresample_version() << ")\n";
    std::cout << "\nConfiguration: " << avutil_configuration() << "\n";
    std::cout << "License: " << avutil_license() << "\n";

    const AVCodec* codec = avcodec_find_encoder_by_name("libx264");
    if (codec) {
        std::cout << "\nlibx264 encoder found: " << codec->name << " (id: " << codec->id << ")\n";
    } else {
        std::cout << "\nlibx264 encoder NOT found\n";
    }

    return 0;
}