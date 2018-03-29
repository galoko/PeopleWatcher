#include <stdexcept>

extern "C" {
#include "libavutil/error.h"
}

#include "ffmpegUtils.h"

class ffmpeg_error : public std::runtime_error
{
public:
    ffmpeg_error(int errnum) throw();
};

std::string get_ffmpeg_error_str(int ret) {

    return std::string(av_err2str(ret));
}

ffmpeg_error::ffmpeg_error(int ret) throw()
        : std::runtime_error(get_ffmpeg_error_str(ret)) { }

void av_check_error(int ret) {

    if (ret < 0)
        throw ffmpeg_error(ret);
}