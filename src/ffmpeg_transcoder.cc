/*
 * FFmpeg decoder class source for ffmpegfs
 *
 * Copyright (C) 2017-2018 Norbert Schlia (nschlia@oblivion-software.de)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "ffmpeg_transcoder.h"
#include "transcode.h"
#include "buffer.h"
#include "wave.h"

#include <assert.h>

// Disable annoying warnings outside our code
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#ifdef __GNUC__
#  include <features.h>
#  if __GNUC_PREREQ(5,0) || defined(__clang__)
// GCC >= 5.0
#     pragma GCC diagnostic ignored "-Wfloat-conversion"
#  elif __GNUC_PREREQ(4,8)
// GCC >= 4.8
#  else
#     error("GCC < 4.8 not supported");
#  endif
#endif
#ifdef __cplusplus
extern "C" {
#endif
#include <libswscale/swscale.h>
#if LAVR_DEPRECATE
#include <libswresample/swresample.h>
#else
#include <libavresample/avresample.h>
#endif
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/audio_fifo.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#ifdef __cplusplus
}
#endif
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
FFMPEG_Transcoder::FFMPEG_Transcoder()
    : m_fileio(NULL)
    , m_predicted_size(0)
    , m_is_video(false)
    , m_pAudio_resample_ctx(NULL)
    , m_pAudioFifo(NULL)
    , m_pSws_ctx(NULL)
    , m_pBufferSinkContext(NULL)
    , m_pBufferSourceContext(NULL)
    , m_pFilterGraph(NULL)
    , m_pts(AV_NOPTS_VALUE)
    , m_pos(AV_NOPTS_VALUE)
{
#pragma GCC diagnostic pop
    ffmpegfs_trace(NULL, "FFmpeg trancoder ready to initialise.");

    // Initialise ID3v1.1 tag structure
    init_id3v1(&m_out.m_id3v1);
}

// Free the FFmpeg en/decoder
// after the transcoding process has finished.
FFMPEG_Transcoder::~FFMPEG_Transcoder()
{
    // Close fifo and resample context
    close();

    ffmpegfs_trace(NULL, "FFmpeg trancoder object destroyed.");
}

// FFmpeg handles cover arts like video streams.
// Try to find out if we have a video stream or a cover art.
bool FFMPEG_Transcoder::is_video() const
{
    bool bIsVideo = false;

    if (m_in.m_video.m_pCodec_ctx != NULL && m_in.m_video.m_pStream != NULL)
    {
        if ((m_in.m_video.m_pCodec_ctx->codec_id == AV_CODEC_ID_PNG) || (m_in.m_video.m_pCodec_ctx->codec_id == AV_CODEC_ID_MJPEG))
        {
            bIsVideo = false;

#ifdef USING_LIBAV
            if (m_in.m_video.m_pStream->avg_frame_rate.den)
            {
                double dbFrameRate = (double)m_in.m_video.m_pStream->avg_frame_rate.num / m_in.m_video.m_pStream->avg_frame_rate.den;

                // If frame rate is < 100 fps this should be a video
                if (dbFrameRate < 100)
                {
                    bIsVideo = true;
                }
            }
#else
            if (m_in.m_video.m_pStream->r_frame_rate.den)
            {
                double dbFrameRate = (double)m_in.m_video.m_pStream->r_frame_rate.num / m_in.m_video.m_pStream->r_frame_rate.den;

                // If frame rate is < 100 fps this should be a video
                if (dbFrameRate < 100)
                {
                    bIsVideo = true;
                }
            }
#endif
        }
        else
        {
            // If the source codec is not PNG or JPG we can safely assume it's a video stream
            bIsVideo = true;
        }
    }

    return bIsVideo;
}

bool FFMPEG_Transcoder::is_open() const
{
    return (m_in.m_pFormat_ctx != NULL);
}

// Open the given FFmpeg file and prepare for decoding. After this function,
// the other methods can be used to process the file.
int FFMPEG_Transcoder::open_input_file(LPCVIRTUALFILE virtualfile)
{
    AVDictionary * opt = NULL;
    int ret;

    m_in.m_filename = virtualfile->m_origfile;

    m_mtime = virtualfile->m_st.st_mtime;

    if (is_open())
    {
        ffmpegfs_warning(filename(), "File is already open.");
        return 0;
    }

    //    This allows selecting if the demuxer should consider all streams to be
    //    found after the first PMT and add further streams during decoding or if it rather
    //    should scan all that are within the analyze-duration and other limits

    ret = av_dict_set_with_check(&opt, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);
    if (ret < 0)
    {
        return ret;
    }

    //    ret = av_dict_set_with_check(&opt, "analyzeduration", "100M", 0);    // <<== honored
    //    if (ret < 0)
    //    {
    //        return ret;
    //    }
    //    ret = av_dict_set_with_check(&opt, "probesize", "100M", 0);          // <<== honored;
    //    if (ret < 0)
    //    {
    //        return ret;
    //    }

    // using own I/O
    m_fileio = fileio::alloc(virtualfile->m_type);
    if (m_fileio == NULL)
    {
        ffmpegfs_error(filename(), "Out of memory opening file.");
        return AVERROR(ENOMEM);
    }

    ret = m_fileio->open(virtualfile);
    if (ret)
    {
        return AVERROR(ret);
    }

    m_in.m_pFormat_ctx = avformat_alloc_context();

    AVIOContext * pb = avio_alloc_context(
                (unsigned char *) ::av_malloc(m_fileio->bufsize() + FF_INPUT_BUFFER_PADDING_SIZE),
                m_fileio->bufsize(),
                0,
                (void *)m_fileio,
                input_read,
                NULL,   // input_write
                seek);  // input_seek
    m_in.m_pFormat_ctx->pb = pb;

    // Open the input file to read from it.
    ret = avformat_open_input(&m_in.m_pFormat_ctx, filename(), NULL, &opt);
    if (ret < 0)
    {
        ffmpegfs_error(filename(), "Could not open input file (error '%s').", ffmpeg_geterror(ret).c_str());
        return ret;
    }

    m_in.m_file_type = get_filetype(m_in.m_pFormat_ctx->iformat->name);

    ret = av_dict_set_with_check(&opt, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE, filename());
    if (ret < 0)
    {
        return ret;
    }

    AVDictionaryEntry * t;
    if ((t = av_dict_get(opt, "", NULL, AV_DICT_IGNORE_SUFFIX)))
    {
        ffmpegfs_error(filename(), "Option %s not found.", t->key);
        return -1; // Couldn't open file
    }

#if HAVE_AV_FORMAT_INJECT_GLOBAL_SIDE_DATA
    av_format_inject_global_side_data(m_in.m_pFormat_ctx);
#endif

    // Get information on the input file (number of streams etc.).
    ret = avformat_find_stream_info(m_in.m_pFormat_ctx, NULL);
    if (ret < 0)
    {
        ffmpegfs_error(filename(), "Could not open find stream info (error '%s').", ffmpeg_geterror(ret).c_str());
        return ret;
    }

    // Open best match video codec
    ret = open_bestmatch_codec_context(&m_in.m_video.m_pCodec_ctx, &m_in.m_video.m_nStream_idx, m_in.m_pFormat_ctx, AVMEDIA_TYPE_VIDEO, filename());
    if (ret < 0 && ret != AVERROR_STREAM_NOT_FOUND)    // Not an error
    {
        ffmpegfs_error(filename(), "Failed to open video codec (error '%s').", ffmpeg_geterror(ret).c_str());
        return ret;
    }

    if (m_in.m_video.m_nStream_idx >= 0)
    {
        // We have a video stream
        m_in.m_video.m_pStream = m_in.m_pFormat_ctx->streams[m_in.m_video.m_nStream_idx];

        m_is_video = is_video();

#ifdef AV_CODEC_CAP_TRUNCATED
        if (m_in.m_video.m_pCodec_ctx->codec->capabilities & AV_CODEC_CAP_TRUNCATED)
        {
            m_in.m_video.m_pCodec_ctx->flags|= AV_CODEC_FLAG_TRUNCATED; // we do not send complete frames
        }
#else
#warning "Your FFMPEG distribution is missing AV_CODEC_CAP_TRUNCATED flag. Probably requires fixing!"
#endif
        video_info(false, m_in.m_pFormat_ctx, m_in.m_video.m_pCodec_ctx, m_in.m_video.m_pStream);
    }

    // Open best match audio codec
    ret = open_bestmatch_codec_context(&m_in.m_audio.m_pCodec_ctx, &m_in.m_audio.m_nStream_idx, m_in.m_pFormat_ctx, AVMEDIA_TYPE_AUDIO, filename());
    if (ret < 0 && ret != AVERROR_STREAM_NOT_FOUND)    // Not an error
    {
        ffmpegfs_error(filename(), "Failed to open audio codec (error '%s').", ffmpeg_geterror(ret).c_str());
        return ret;
    }

    if (m_in.m_audio.m_nStream_idx >= 0)
    {
        // We have an audio stream
        m_in.m_audio.m_pStream = m_in.m_pFormat_ctx->streams[m_in.m_audio.m_nStream_idx];

        audio_info(false, m_in.m_pFormat_ctx, m_in.m_audio.m_pCodec_ctx, m_in.m_audio.m_pStream);
    }

    if (m_in.m_audio.m_nStream_idx == -1 && m_in.m_video.m_nStream_idx == -1)
    {
        ffmpegfs_error(filename(), "File contains neither a video nor an audio stream.");
        return AVERROR(EINVAL);
    }

    // Open album art streams if present and supported by both source and target
    if (!params.m_noalbumarts && m_in.m_audio.m_pStream != NULL &&
            supports_albumart(m_in.m_file_type) && supports_albumart(get_filetype(params.m_desttype)))
    {
        ffmpegfs_trace(filename(), "Processing album arts.");

        for (unsigned int stream_idx = 0; stream_idx < m_in.m_pFormat_ctx->nb_streams; stream_idx++)
        {
            AVStream *input_stream = m_in.m_pFormat_ctx->streams[stream_idx];
            AVCodecID codec_id;

            codec_id = CODECPAR(input_stream)->codec_id;

            if (codec_id == AV_CODEC_ID_MJPEG || codec_id == AV_CODEC_ID_PNG || codec_id == AV_CODEC_ID_BMP)
            {
                STREAMREF streamref;
                AVCodecContext * input_codec_ctx;

                ret = open_codec_context(&input_codec_ctx, stream_idx, m_in.m_pFormat_ctx, AVMEDIA_TYPE_VIDEO, filename());
                if (ret < 0)
                {
                    ffmpegfs_error(filename(), "Failed to open album art codec (error '%s').", ffmpeg_geterror(ret).c_str());
                    return ret;
                }

                streamref.m_pCodec_ctx  = input_codec_ctx;
                streamref.m_pStream     = input_stream;
                streamref.m_nStream_idx = input_stream->index;

                m_in.m_aAlbumArt.push_back(streamref);
            }
        }
    }

    return 0;
}

int FFMPEG_Transcoder::open_output_file(Buffer *buffer)
{
    int res = 0;

    get_destname(&m_out.m_filename, m_in.m_filename);

    ffmpegfs_info(destname(), "Opening output file.");

    // Pre-allocate the predicted file size to reduce memory reallocations
    if (!buffer->reserve(predict_filesize()))
    {
        ffmpegfs_error(filename(), "Out of memory pre-allocating buffer.");
        return AVERROR(ENOMEM);
    }

    // Open the output file for writing.
    res = open_output_filestreams(buffer);
    if (res)
    {
        return res;
    }

    if (m_out.m_audio.m_nStream_idx > -1)
    {
        audio_info(true, m_out.m_pFormat_ctx, m_out.m_audio.m_pCodec_ctx, m_out.m_audio.m_pStream);

        // Initialise the resampler to be able to convert audio sample formats.
        res = init_resampler();
        if (res)
        {
            return res;
        }

        // Initialise the FIFO buffer to store audio samples to be encoded.
        res = init_fifo();
        if (res)
        {
            return res;
        }
    }

    if (m_out.m_video.m_nStream_idx > -1)
    {
        video_info(true, m_out.m_pFormat_ctx, m_out.m_video.m_pCodec_ctx, m_out.m_video.m_pStream);
    }

    // Process metadata. The decoder will call the encoder to set appropriate
    // tag values for the output file.
    res = process_metadata();
    if (res)
    {
        return res;
    }

    // Write the header of the output file container.
    res = write_output_file_header();
    if (res)
    {
        return res;
    }

    // Process album arts: copy all from source file to target.
    res = process_albumarts();
    if (res)
    {
        return res;
    }

    return 0;
}

bool FFMPEG_Transcoder::get_output_sample_rate(int input_sample_rate, int max_sample_rate, int *output_sample_rate) const
{
    *output_sample_rate = input_sample_rate;

    if (*output_sample_rate > max_sample_rate)
    {
        *output_sample_rate = max_sample_rate;
        return true;
    }
    else
    {
        return false;
    }
}

bool FFMPEG_Transcoder::get_output_bit_rate(BITRATE input_bit_rate, BITRATE max_bit_rate, BITRATE * output_bit_rate) const
{
    *output_bit_rate = input_bit_rate;

    if (*output_bit_rate > max_bit_rate)
    {
        *output_bit_rate = max_bit_rate;
        return true;
    }
    else
    {
        return false;
    }
}

double FFMPEG_Transcoder::get_aspect_ratio(int width, int height, const AVRational & sample_aspect_ratio) const
{
    double dblAspectRatio = 0;

    // Try to determine display aspect ratio
    AVRational dar;
    ::av_reduce(&dar.num, &dar.den,
                width  * sample_aspect_ratio.num,
                height * sample_aspect_ratio.den,
                1024 * 1024);

    if (dar.num && dar.den)
    {
        dblAspectRatio = ::av_q2d(dar);
    }

    // If that fails, try sample aspect ratio instead
    if (dblAspectRatio <= 0.0 && sample_aspect_ratio.num != 0)
    {
        dblAspectRatio = ::av_q2d(sample_aspect_ratio);
    }

    // If even that fails, try to use video size
    if (dblAspectRatio <= 0.0 && height)
    {
        dblAspectRatio = (double)width / (double)height;
    }

    // Return value or 0 if all above failed
    return dblAspectRatio;
}

void FFMPEG_Transcoder::limit_video_size(AVCodecContext *output_codec_ctx)
{
    int width;
    int height;

    if (params.m_videowidth && params.m_videoheight)
    {
        // Both width/source set. May look strange, but this is an order...
        width                               = params.m_videowidth;
        height                              = params.m_videoheight;
    }
    else if (params.m_videowidth)
    {
        // Only video width
        double ratio = get_aspect_ratio(output_codec_ctx->width, output_codec_ctx->height, output_codec_ctx->sample_aspect_ratio);

        width                               = params.m_videowidth;

        if (!ratio)
        {
            height                          = output_codec_ctx->height;
        }
        else
        {
            height                          = (int)(params.m_videowidth / ratio);
            height &= ~((int)0x1);
        }
    }
    else //if (params.m_videoheight)
    {
        // Only video height
        double ratio = get_aspect_ratio(output_codec_ctx->width, output_codec_ctx->height, output_codec_ctx->sample_aspect_ratio);

        if (!ratio)
        {
            width                           = output_codec_ctx->width;
        }
        else
        {
            width                           = (int)(params.m_videoheight * ratio);
            width &= ~((int)0x1);
        }
        height                              = params.m_videoheight;
    }

    if (output_codec_ctx->width > width || output_codec_ctx->height > height)
    {
        ffmpegfs_trace(destname(), "Changing video size from %i/%i to %i/%i.", output_codec_ctx->width, output_codec_ctx->height, width, height);
        output_codec_ctx->width             = width;
        output_codec_ctx->height            = height;
    }
}

// Prepare codec optimisations
int FFMPEG_Transcoder::update_codec(void *opt, LPCPROFILE_OPTION mp4_opt) const
{
    int ret = 0;

    if (mp4_opt == NULL)
    {
        return 0;
    }

    for (LPCPROFILE_OPTION p = mp4_opt; p->m_key != NULL; p++)
    {
        ffmpegfs_trace(destname(), "Profile codec option -%s%s%s.", p->m_key, *p->m_value ? " " : "", p->m_value);

        ret = av_opt_set_with_check(opt, p->m_key, p->m_value, p->m_flags, destname());
        if (ret < 0)
        {
            break;
        }
    }
    return ret;
}

int FFMPEG_Transcoder::prepare_mp4_codec(void *opt) const
{
    int ret = 0;

    for (int n = 0; m_profile[n].m_profile != PROFILE_INVALID; n++)
    {
        if (m_profile[n].m_filetype == FILETYPE_MP4 && m_profile[n].m_profile == params.m_profile)
        {
            ret = update_codec(opt, m_profile[n].m_option_codec);
            break;
        }
    }

    return ret;
}

int FFMPEG_Transcoder::prepare_webm_codec(void *opt) const
{
    int ret = 0;

    for (int n = 0; m_profile[n].m_profile != PROFILE_INVALID; n++)
    {
        if (m_profile[n].m_filetype == FILETYPE_WEBM && m_profile[n].m_profile == params.m_profile)
        {
            ret = update_codec(opt, m_profile[n].m_option_codec);
            break;
        }
    }

    return ret;
}

int FFMPEG_Transcoder::add_stream(AVCodecID codec_id)
{
    AVCodecContext *output_codec_ctx    = NULL;
    AVStream *      output_stream       = NULL;
    AVCodec *       output_codec        = NULL;
    AVDictionary *  opt                 = NULL;
    int ret;

    // find the encoder
    output_codec = avcodec_find_encoder(codec_id);
    if (!output_codec)
    {
        ffmpegfs_error(destname(), "Could not find encoder '%s'.", avcodec_get_name(codec_id));
        return AVERROR(EINVAL);
    }

    output_stream = avformat_new_stream(m_out.m_pFormat_ctx, output_codec);
    if (!output_stream)
    {
        ffmpegfs_error(destname(), "Could not allocate stream for encoder '%s'.",  avcodec_get_name(codec_id));
        return AVERROR(ENOMEM);
    }
    output_stream->id = m_out.m_pFormat_ctx->nb_streams - 1;

#if FFMPEG_VERSION3 // Check for FFmpeg 3
    output_codec_ctx = avcodec_alloc_context3(output_codec);
    if (!output_codec_ctx)
    {
        ffmpegfs_error(destname(), "Could not alloc an encoding context.");
        return AVERROR(ENOMEM);
    }
#else
    output_codec_ctx = output_stream->codec;
#endif

    switch (output_codec->type)
    {
    case AVMEDIA_TYPE_AUDIO:
    {
        int64_t orig_bit_rate;
        int orig_sample_rate;

        // Set the basic encoder parameters
        orig_bit_rate = (CODECPAR(m_in.m_audio.m_pStream)->bit_rate != 0) ? CODECPAR(m_in.m_audio.m_pStream)->bit_rate : m_in.m_pFormat_ctx->bit_rate;
        if (get_output_bit_rate(orig_bit_rate, params.m_audiobitrate, &output_codec_ctx->bit_rate))
        {
            // Limit bit rate
            ffmpegfs_trace(destname(), "Limiting audio bit rate from %s to %s.",
                           format_bitrate(orig_bit_rate).c_str(),
                           format_bitrate(output_codec_ctx->bit_rate).c_str());
        }

        output_codec_ctx->channels              = 2;
        output_codec_ctx->channel_layout        = av_get_default_channel_layout(output_codec_ctx->channels);
        output_codec_ctx->sample_rate           = m_in.m_audio.m_pCodec_ctx->sample_rate;
        orig_sample_rate                        = m_in.m_audio.m_pCodec_ctx->sample_rate;
        if (get_output_sample_rate(CODECPAR(m_in.m_audio.m_pStream)->sample_rate, params.m_audiosamplerate, &output_codec_ctx->sample_rate))
        {
            // Limit sample rate
            ffmpegfs_trace(destname(), "Limiting audio sample rate from %s to %s.",
                           format_samplerate(orig_sample_rate).c_str(),
                           format_samplerate(output_codec_ctx->sample_rate).c_str());
        }

        if (output_codec->supported_samplerates != NULL)
        {
            // Go through supported sample rates and adjust if necessary
            bool supported = false;

            for (int n = 0; output_codec->supported_samplerates[n] !=0; n++)
            {
                if (output_codec->supported_samplerates[n] == output_codec_ctx->sample_rate)
                {
                    // Is supported
                    supported = true;
                    break;
                }
            }

            if (!supported)
            {
                int min_samplerate = 0;
                int max_samplerate = INT_MAX;

                // Find next lower sample rate in probably unsorted list
                for (int n = 0; output_codec->supported_samplerates[n] != 0; n++)
                {
                    if (min_samplerate <= output_codec->supported_samplerates[n] && output_codec_ctx->sample_rate >= output_codec->supported_samplerates[n])
                    {
                        min_samplerate = output_codec->supported_samplerates[n];
                    }
                }

                // Find next higher sample rate in probably unsorted list
                for (int n = 0; output_codec->supported_samplerates[n] != 0; n++)
                {
                    if (max_samplerate >= output_codec->supported_samplerates[n] && output_codec_ctx->sample_rate <= output_codec->supported_samplerates[n])
                    {
                        max_samplerate = output_codec->supported_samplerates[n];
                    }
                }

                if (min_samplerate != 0 && max_samplerate != INT_MAX)
                {
                    // set to nearest value
                    if (output_codec_ctx->sample_rate - min_samplerate < max_samplerate - output_codec_ctx->sample_rate)
                    {
                        output_codec_ctx->sample_rate = min_samplerate;
                    }
                    else
                    {
                        output_codec_ctx->sample_rate = max_samplerate;
                    }
                }
                else if (min_samplerate != 0)
                {
                    // No higher sample rate, use next lower
                    output_codec_ctx->sample_rate = min_samplerate;
                }
                else if (max_samplerate != INT_MAX)
                {
                    // No lower sample rate, use higher lower
                    output_codec_ctx->sample_rate = max_samplerate;
                }
                else
                {
                    // Should never happen... There must at least be one.
                    ffmpegfs_error(destname(), "Audio sample rate to %s not supported by codec.", format_samplerate(output_codec_ctx->sample_rate).c_str());
                    return AVERROR(EINVAL);
                }

                ffmpegfs_warning(destname(), "Changed audio sample rate from %s to %s because requested value is not supported by codec.",
                                 format_samplerate(orig_sample_rate).c_str(),
                                 format_samplerate(output_codec_ctx->sample_rate).c_str());
            }
        }

        if (output_codec->sample_fmts != NULL)
        {
            // Check if input sample format is supported and if so, use it (avoiding resampling)
            output_codec_ctx->sample_fmt        = AV_SAMPLE_FMT_NONE;

            for (int n = 0; output_codec->sample_fmts[n] != -1; n++)
            {
                if (output_codec->sample_fmts[n] == m_in.m_audio.m_pCodec_ctx->sample_fmt)
                {
                    output_codec_ctx->sample_fmt    = m_in.m_audio.m_pCodec_ctx->sample_fmt;
                    break;
                }
            }

            // If none of the supported formats match use the first supported
            if (output_codec_ctx->sample_fmt == AV_SAMPLE_FMT_NONE)
            {
                output_codec_ctx->sample_fmt        = output_codec->sample_fmts[0];
            }
        }
        else
        {
            // If suppported sample formats are unknown simply take input format and cross our fingers it works...
            output_codec_ctx->sample_fmt        = m_in.m_audio.m_pCodec_ctx->sample_fmt;
        }

        // Set the sample rate for the container.
        output_stream->time_base.den        = output_codec_ctx->sample_rate;
        output_stream->time_base.num        = 1;
        output_codec_ctx->time_base         = output_stream->time_base;

#if !FFMPEG_VERSION3 | defined(USING_LIBAV) // Check for FFmpeg 3
        // set -strict -2 for aac (required for FFmpeg 2)
        av_dict_set_with_check(&opt, "strict", "-2", 0);

        // Allow the use of the experimental AAC encoder
        output_codec_ctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
#endif

        // Set duration as hint for muxer
        output_stream->duration                 = av_rescale_q(m_in.m_audio.m_pStream->duration, m_in.m_audio.m_pStream->time_base, output_stream->time_base);

        // Save the encoder context for easier access later.
        m_out.m_audio.m_pCodec_ctx              = output_codec_ctx;
        // Save the stream index
        m_out.m_audio.m_nStream_idx             = output_stream->index;
        // Save output audio stream for faster reference
        m_out.m_audio.m_pStream                 = output_stream;
        break;
    }
    case AVMEDIA_TYPE_VIDEO:
    {
        int64_t orig_bit_rate;

        output_codec_ctx->codec_id = codec_id;

        // Set the basic encoder parameters
        orig_bit_rate = (CODECPAR(m_in.m_video.m_pStream)->bit_rate != 0) ? CODECPAR(m_in.m_video.m_pStream)->bit_rate : m_in.m_pFormat_ctx->bit_rate;
        if (get_output_bit_rate(orig_bit_rate, params.m_videobitrate, &output_codec_ctx->bit_rate))
        {
            // Limit sample rate
            ffmpegfs_trace(destname(), "Limiting video bit rate FROM %s to %s.",
                           format_bitrate(orig_bit_rate).c_str(),
                           format_bitrate(output_codec_ctx->bit_rate).c_str());
        }

        // Resolution must be a multiple of two.
        output_codec_ctx->width                 = CODECPAR(m_in.m_video.m_pStream)->width;
        output_codec_ctx->height                = CODECPAR(m_in.m_video.m_pStream)->height;

        if (params.m_videowidth || params.m_videoheight)
        {
            // Use command line argument(s)
            limit_video_size(output_codec_ctx);
        }

#if LAVF_DEP_AVSTREAM_CODEC
        video_stream_setup(output_codec_ctx, output_stream, m_in.m_video.m_pStream->avg_frame_rate);
#else
        video_stream_setup(output_codec_ctx, output_stream, m_in.m_video.m_pStream->codec->framerate);
#endif

        AVRational sample_aspect_ratio                  = CODECPAR(m_in.m_video.m_pStream)->sample_aspect_ratio;

        if (output_codec_ctx->codec_id != AV_CODEC_ID_VP9)
        {
            output_codec_ctx->sample_aspect_ratio           = sample_aspect_ratio;
            CODECPAR(output_stream)->sample_aspect_ratio    = sample_aspect_ratio;
        }

        else
        {
            // WebM does not respect the aspect ratio and always uses 1:1 so we need to rescale "manually".
            // TODO: The ffmpeg actually *can* transcode while presevering the SAR. Need to find out what I am doing wrong here...

            output_codec_ctx->sample_aspect_ratio           = { 1, 1 };
            CODECPAR(output_stream)->sample_aspect_ratio    = { 1, 1 };

            output_codec_ctx->width                         = output_codec_ctx->width * sample_aspect_ratio.num / sample_aspect_ratio.den;
            //output_codec_ctx->height                        *= sample_aspect_ratio.den;
        }

        // Set up optimisations

        switch (output_codec_ctx->codec_id)
        {
        case AV_CODEC_ID_H264:
        {
            ret = prepare_mp4_codec(output_codec_ctx->priv_data);
            if (ret < 0)
            {
                ffmpegfs_error(destname(), "Could not set profile for %s output codec %s (error '%s').", get_media_type_string(output_codec->type), get_codec_name(codec_id, 0), ffmpeg_geterror(ret).c_str());
                return ret;
            }
            break;
        }
        case AV_CODEC_ID_VP9:
        {
            ret = prepare_webm_codec(output_codec_ctx->priv_data);
            if (ret < 0)
            {
                ffmpegfs_error(destname(), "Could not set profile for %s output codec %s (error '%s').", get_media_type_string(output_codec->type), get_codec_name(codec_id, 0), ffmpeg_geterror(ret).c_str());
                return ret;
            }
            break;
        }
        default:
        {
            break;
        }
        }

        // Initialise pixel format conversion and rescaling if necessary

#if LAVF_DEP_AVSTREAM_CODEC
        AVPixelFormat pix_fmt = (AVPixelFormat)m_in.m_video.m_pStream->codecpar->format;
#else
        AVPixelFormat pix_fmt = (AVPixelFormat)m_in.m_video.m_pStream->codec->pix_fmt;
#endif
        if (pix_fmt == AV_PIX_FMT_NONE)
        {
            // If input's stream pixel format is unknown, use same as output (may not work but at least will not crash FFmpeg)
            pix_fmt = output_codec_ctx->pix_fmt;
        }

        if (pix_fmt != output_codec_ctx->pix_fmt ||
                CODECPAR(m_in.m_video.m_pStream)->width != output_codec_ctx->width ||
                CODECPAR(m_in.m_video.m_pStream)->height != output_codec_ctx->height)
        {
            // Rescale image if required
            const AVPixFmtDescriptor *fmtin = av_pix_fmt_desc_get(pix_fmt);
            const AVPixFmtDescriptor *fmtout = av_pix_fmt_desc_get(output_codec_ctx->pix_fmt);

            if (pix_fmt != output_codec_ctx->pix_fmt)
            {
                ffmpegfs_trace(destname(), "Initialising pixel format conversion from %s to %s.", fmtin != NULL ? fmtin->name : "-", fmtout != NULL ? fmtout->name : "-");
            }

            if (CODECPAR(m_in.m_video.m_pStream)->width != output_codec_ctx->width ||
                    CODECPAR(m_in.m_video.m_pStream)->height != output_codec_ctx->height)

            {
                ffmpegfs_debug(destname(), "Rescaling video size ffrom %i:%i to %i:%i.",
                               CODECPAR(m_in.m_video.m_pStream)->width, CODECPAR(m_in.m_video.m_pStream)->height,
                               output_codec_ctx->width, output_codec_ctx->height);
            }

            m_pSws_ctx = sws_getContext(
                        // Source settings
                        CODECPAR(m_in.m_video.m_pStream)->width,    // width
                        CODECPAR(m_in.m_video.m_pStream)->height,   // height
                        pix_fmt,                                    // format
                        // Target settings
                        output_codec_ctx->width,                    // width
                        output_codec_ctx->height,                   // height
                        output_codec_ctx->pix_fmt,                  // format
                        SWS_FAST_BILINEAR, NULL, NULL, NULL);
            if (!m_pSws_ctx)
            {
                ffmpegfs_error(destname(), "Could not allocate scaling/conversion context.");
                return AVERROR(ENOMEM);
            }
        }

#ifdef _DEBUG
        print_info(output_stream);
#endif // _DEBUG

        // Set duration as hint for muxer
        output_stream->duration                 = av_rescale_q(m_in.m_video.m_pStream->duration, m_in.m_video.m_pStream->time_base, output_stream->time_base);

        // Save the encoder context for easier access later.
        m_out.m_video.m_pCodec_ctx              = output_codec_ctx;
        // Save the stream index
        m_out.m_video.m_nStream_idx             = output_stream->index;
        // Save output video stream for faster reference
        m_out.m_video.m_pStream                 = output_stream;

        break;
    }
    default:
        break;
    }

    // Some formats want stream headers to be separate.
    if (m_out.m_pFormat_ctx->oformat->flags & AVFMT_GLOBALHEADER)
    {
        output_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if (!av_dict_get(opt, "threads", NULL, 0))
    {
        ffmpegfs_trace(destname(), "Setting threads to auto for codec %s.", get_codec_name(output_codec_ctx->codec_id, 0));
        av_dict_set_with_check(&opt, "threads", "auto", 0, destname());
    }

    // Open the encoder for the audio stream to use it later.
    ret = avcodec_open2(output_codec_ctx, output_codec, &opt);
    if (ret < 0)
    {
        ffmpegfs_error(destname(), "Could not open %s output codec %s (error '%s').", get_media_type_string(output_codec->type), get_codec_name(codec_id, 0), ffmpeg_geterror(ret).c_str());
        return ret;
    }

    ffmpegfs_debug(destname(), "Opened %s output codec %s for stream #%u.", get_media_type_string(output_codec->type), get_codec_name(codec_id, 1), output_stream->index);

#if FFMPEG_VERSION3 // Check for FFmpeg 3
    ret = avcodec_parameters_from_context(output_stream->codecpar, output_codec_ctx);
    if (ret < 0)
    {
        ffmpegfs_error(destname(), "Could not initialise stream parameters (error '%s').", ffmpeg_geterror(ret).c_str());
        return ret;
    }
#endif

    //    output_stream->time_base = output_codec_ctx->time_base;

    return 0;
}

int FFMPEG_Transcoder::add_albumart_stream(const AVCodecContext * input_codec_ctx)
{
    AVCodecContext * output_codec_ctx   = NULL;
    AVStream * output_stream            = NULL;
    const AVCodec * input_codec         = input_codec_ctx->codec;
    const AVCodec * output_codec        = NULL;
    AVDictionary *  opt                 = NULL;
    int ret;

    // find the encoder
    output_codec = avcodec_find_encoder(input_codec->id);
    if (!output_codec)
    {
        ffmpegfs_error(destname(), "Could not find encoder '%s'.", avcodec_get_name(input_codec->id));
        return AVERROR(EINVAL);
    }

    // Must be a video codec
    if (output_codec->type != AVMEDIA_TYPE_VIDEO)
    {
        ffmpegfs_error(destname(), "INTERNAL TROUBLE! Encoder '%s' is not a video codec.", avcodec_get_name(input_codec->id));
        return AVERROR(EINVAL);
    }

    output_stream = avformat_new_stream(m_out.m_pFormat_ctx, output_codec);
    if (!output_stream)
    {
        ffmpegfs_error(destname(), "Could not allocate stream for encoder '%s'.", avcodec_get_name(input_codec->id));
        return AVERROR(ENOMEM);
    }
    output_stream->id = m_out.m_pFormat_ctx->nb_streams - 1;

#if FFMPEG_VERSION3 // Check for FFmpeg 3
    output_codec_ctx = avcodec_alloc_context3(output_codec);
    if (!output_codec_ctx)
    {
        ffmpegfs_error(destname(), "Could not alloc an encoding context.");
        return AVERROR(ENOMEM);
    }
#else
    output_codec_ctx = output_stream->codec;
#endif

    // Ignore missing width/height when adding album arts
    m_out.m_pFormat_ctx->oformat->flags |= AVFMT_NODIMENSIONS;

    // This is required for some reason (let encoder decide?)
    // If not set, write header will fail!
    //    output_codec_ctx->codec_tag = 0; //av_codec_get_tag(of->codec_tag, codec->codec_id);

    //    output_stream->codec->framerate = { 1, 0 };

    // TODO: ALBUM ARTS
    // mp4 album arts do not work with ipod profile. Set mp4.
    //    if (m_out.m_pFormat_ctx->oformat->mime_type != NULL && (!strcmp(m_out.m_pFormat_ctx->oformat->mime_type, "application/mp4") || !strcmp(m_out.m_pFormat_ctx->oformat->mime_type, "video/mp4")))
    //    {
    //        m_out.m_pFormat_ctx->oformat->name = "mp4";
    //        m_out.m_pFormat_ctx->oformat->mime_type = "application/mp4";
    //    }

    // copy disposition
    // output_stream->disposition = input_stream->disposition;
    output_stream->disposition = AV_DISPOSITION_ATTACHED_PIC;

    // copy estimated duration as a hint to the muxer
    if (output_stream->duration <= 0 && m_in.m_audio.m_pStream->duration > 0)
    {
        output_stream->duration = av_rescale_q(m_in.m_audio.m_pStream->duration, m_in.m_audio.m_pStream->time_base, output_stream->time_base);
    }

    output_codec_ctx->time_base = { 1, 90000 };
    output_stream->time_base = { 1, 90000 };

    output_codec_ctx->pix_fmt   = input_codec_ctx->pix_fmt;
    output_codec_ctx->width     = input_codec_ctx->width;
    output_codec_ctx->height    = input_codec_ctx->height;

    // Some formats want stream headers to be separate.
    if (m_out.m_pFormat_ctx->oformat->flags & AVFMT_GLOBALHEADER)
    {
        output_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    // Open the encoder for the audio stream to use it later.
    ret = avcodec_open2(output_codec_ctx, output_codec, &opt);
    if (ret < 0)
    {
        ffmpegfs_error(destname(), "Could not open %s output codec %s for stream #%u (error '%s').", get_media_type_string(output_codec->type), get_codec_name(input_codec->id, 0), output_stream->index, ffmpeg_geterror(ret).c_str());
        return ret;
    }

    ffmpegfs_debug(destname(), "Opened album art output codec %s for stream #%u (dimensions %ix%i).", get_codec_name(input_codec->id, 1), output_stream->index, output_codec_ctx->width, output_codec_ctx->height);

#if FFMPEG_VERSION3 // Check for FFmpeg 3
    ret = avcodec_parameters_from_context(output_stream->codecpar, output_codec_ctx);
    if (ret < 0)
    {
        ffmpegfs_error(destname(), "Could not initialise stream parameters stream #%u (error '%s').", output_stream->index, ffmpeg_geterror(ret).c_str());
        return ret;
    }
#endif

    STREAMREF stream;

    stream.m_pCodec_ctx     = output_codec_ctx;
    stream.m_pStream        = output_stream;
    stream.m_nStream_idx    = output_stream->index;

    m_out.m_aAlbumArt.push_back(stream);

    return 0;
}

int FFMPEG_Transcoder::add_albumart_frame(AVStream *output_stream, AVPacket* pkt_in)
{
    AVPacket *tmp_pkt;
    int ret = 0;

#if LAVF_DEP_AV_COPY_PACKET || defined(USING_LIBAV)
    tmp_pkt = av_packet_clone(pkt_in);
    if (tmp_pkt == NULL)
    {
        ret = AVERROR(ENOMEM);
        ffmpegfs_error(destname(), "Could not write album art packet (error '%s').", ffmpeg_geterror(ret).c_str());
        return ret;
    }
#else
    AVPacket pkt;

    tmp_pkt = &pkt;

    ret = av_copy_packet(tmp_pkt, pkt_in);
    if (ret < 0)
    {
        ffmpegfs_error(destname(), "Could not write album art packet (error '%s').", ffmpeg_geterror(ret).c_str());
        return ret;
    }
#endif

    ffmpegfs_trace(destname(), "Adding album art stream #%u.", output_stream->index);

    tmp_pkt->stream_index = output_stream->index;
    tmp_pkt->flags |= AV_PKT_FLAG_KEY;
    tmp_pkt->pos = 0;
    tmp_pkt->dts = 0;

    ret = av_interleaved_write_frame(m_out.m_pFormat_ctx, tmp_pkt);

    if (ret < 0)
    {
        ffmpegfs_error(destname(), "Could not write album art packet (error '%s').", ffmpeg_geterror(ret).c_str());
    }

#if LAVF_DEP_AV_COPY_PACKET
    av_packet_unref(tmp_pkt);
#else
    av_free_packet(tmp_pkt);
#endif

    return ret;
}

// Open an output file and the required encoder.
// Also set some basic encoder parameters.
// Some of these parameters are based on the input file's parameters.
int FFMPEG_Transcoder::open_output_filestreams(Buffer *buffer)
{
    AVCodecID       audio_codec_id = params.m_audio_codecid;
    AVCodecID       video_codec_id = params.m_video_codecid;
    const char *    format = params.m_format;
    int             ret = 0;

    m_out.m_file_type = params.m_filetype;
    
    ffmpegfs_debug(destname(), "Opening format type '%s'.", params.m_desttype);

    // Create a new format context for the output container format.
    avformat_alloc_output_context2(&m_out.m_pFormat_ctx, NULL, format, NULL);
    if (!m_out.m_pFormat_ctx)
    {
        ffmpegfs_error(destname(), "Could not allocate output format context.");
        return AVERROR(ENOMEM);
    }

    if (!m_is_video)
    {
        m_in.m_video.m_nStream_idx = INVALID_STREAM;
    }

    //video_codecid = m_out.m_pFormat_ctx->oformat->video_codec;

    if (m_in.m_video.m_nStream_idx != INVALID_STREAM && video_codec_id != AV_CODEC_ID_NONE)
    {
        ret = add_stream(video_codec_id);
        if (ret < 0)
        {
            return ret;
        }

#ifndef USING_LIBAV
        if (params.m_deinterlace)
        {
            // Init deinterlace filters
            ret = init_filters(m_out.m_video.m_pCodec_ctx, m_out.m_video.m_pStream);
            if (ret < 0)
            {
                return ret;
            }
        }
#endif // !USING_LIBAV
    }

    //audio_codec_id = m_out.m_pFormat_ctx->oformat->audio_codec;

    if (m_in.m_audio.m_nStream_idx != INVALID_STREAM && audio_codec_id != AV_CODEC_ID_NONE)
    {
        ret = add_stream(audio_codec_id);
        if (ret < 0)
        {
            return ret;
        }
    }

    if (!params.m_noalbumarts)
    {
        for (size_t n = 0; n < m_in.m_aAlbumArt.size(); n++)
        {
            //ret = add_albumart_stream(codec_id, m_in.m_aAlbumArt.at(n).m_pCodec_ctx->pix_fmt);
            ret = add_albumart_stream(m_in.m_aAlbumArt.at(n).m_pCodec_ctx);
            if (ret < 0)
            {
                return ret;
            }
        }
    }

    // open the output file
    const size_t buf_size = 1024*1024;
    m_out.m_pFormat_ctx->pb = avio_alloc_context(
                (unsigned char *) av_malloc(buf_size + FF_INPUT_BUFFER_PADDING_SIZE),
                buf_size,
                1,
                (void *)buffer,
                NULL,           // read
                output_write,   // write
                (audio_codec_id != AV_CODEC_ID_OPUS) ? seek : NULL);          // seek

    // Some formats require the time stamps to start at 0, so if there is a difference between
    // the streams we need to drop audio or video until we are in sync.
    if ((m_out.m_video.m_pStream != NULL) && (m_in.m_audio.m_pStream != NULL))
    {
        // Calculate difference
        m_out.m_video_start_pts = av_rescale_q(m_in.m_audio.m_pStream->start_time, m_in.m_audio.m_pStream->time_base, m_out.m_video.m_pStream->time_base);
    }

    return 0;
}

// Initialize the audio resampler based on the input and output codec settings.
// If the input and output sample formats differ, a conversion is required
// libswresample takes care of this, but requires initialization.
int FFMPEG_Transcoder::init_resampler()
{
    // Only initialise the resampler if it is necessary, i.e.,
    // if and only if the sample formats differ.

    if (m_in.m_audio.m_pCodec_ctx->sample_fmt != m_out.m_audio.m_pCodec_ctx->sample_fmt ||
            m_in.m_audio.m_pCodec_ctx->sample_rate != m_out.m_audio.m_pCodec_ctx->sample_rate ||
            m_in.m_audio.m_pCodec_ctx->channels != m_out.m_audio.m_pCodec_ctx->channels)
    {
        string in_sample_format(av_get_sample_fmt_name(m_in.m_audio.m_pCodec_ctx->sample_fmt));
        string out_sample_format(av_get_sample_fmt_name(m_out.m_audio.m_pCodec_ctx->sample_fmt));
        int ret;

        ffmpegfs_info(destname(), "Creating audio resampler: Sample format %s -> %s / bit rate %s -> %s.",
                      in_sample_format.c_str(),
                      out_sample_format.c_str(),
                      format_samplerate(m_in.m_audio.m_pCodec_ctx->sample_rate).c_str(),
                      format_samplerate(m_out.m_audio.m_pCodec_ctx->sample_rate).c_str());

        // Create a resampler context for the conversion.
        // Set the conversion parameters.
#if LAVR_DEPRECATE
        m_pAudio_resample_ctx = swr_alloc_set_opts(NULL,
                                                   av_get_default_channel_layout(m_out.m_audio.m_pCodec_ctx->channels),
                                                   m_out.m_audio.m_pCodec_ctx->sample_fmt,
                                                   m_out.m_audio.m_pCodec_ctx->sample_rate,
                                                   av_get_default_channel_layout(m_in.m_audio.m_pCodec_ctx->channels),
                                                   m_in.m_audio.m_pCodec_ctx->sample_fmt,
                                                   m_in.m_audio.m_pCodec_ctx->sample_rate,
                                                   0, NULL);
        if (!m_pAudio_resample_ctx)
        {
            ffmpegfs_error(destname(), "Could not allocate resample context.");
            return AVERROR(ENOMEM);
        }

        // Open the resampler with the specified parameters.
        ret = swr_init(m_pAudio_resample_ctx);
        if (ret < 0)
        {
            ffmpegfs_error(destname(), "Could not open resampler context (error '%s').", ffmpeg_geterror(ret).c_str());
            swr_free(&m_pAudio_resample_ctx);
            m_pAudio_resample_ctx = NULL;
            return ret;
        }
#else
        // Create a resampler context for the conversion.
        m_pAudio_resample_ctx = avresample_alloc_context();
        if (!m_pAudio_resample_ctx)
        {
            ffmpegfs_error(destname(), "Could not allocate resample context.");
            return AVERROR(ENOMEM);
        }

        // Set the conversion parameters.
        // Default channel layouts based on the number of channels
        // are assumed for simplicity (they are sometimes not detected
        // properly by the demuxer and/or decoder).

        av_opt_set_int(m_pAudio_resample_ctx, "in_channel_layout", av_get_default_channel_layout(m_in.m_audio.m_pCodec_ctx->channels), 0);
        av_opt_set_int(m_pAudio_resample_ctx, "out_channel_layout", av_get_default_channel_layout(m_out.m_audio.m_pCodec_ctx->channels), 0);
        av_opt_set_int(m_pAudio_resample_ctx, "in_sample_rate", m_in.m_audio.m_pCodec_ctx->sample_rate, 0);
        av_opt_set_int(m_pAudio_resample_ctx, "out_sample_rate", m_out.m_audio.m_pCodec_ctx->sample_rate, 0);
        av_opt_set_int(m_pAudio_resample_ctx, "in_sample_fmt", m_in.m_audio.m_pCodec_ctx->sample_fmt, 0);
        av_opt_set_int(m_pAudio_resample_ctx, "out_sample_fmt", m_out.m_audio.m_pCodec_ctx->sample_fmt, 0);

        // Open the resampler with the specified parameters.
        ret = avresample_open(m_pAudio_resample_ctx);
        if (ret < 0)
        {
            ffmpegfs_error(destname(), "Could not open resampler context (error '%s').", ffmpeg_geterror(ret).c_str());
            avresample_free(&m_pAudio_resample_ctx);
            m_pAudio_resample_ctx = NULL;
            return ret;
        }
#endif
    }
    return 0;
}

// Initialise a FIFO buffer for the audio samples to be encoded.
int FFMPEG_Transcoder::init_fifo()
{
    // Create the FIFO buffer based on the specified output sample format.
    if (!(m_pAudioFifo = av_audio_fifo_alloc(m_out.m_audio.m_pCodec_ctx->sample_fmt, m_out.m_audio.m_pCodec_ctx->channels, 1)))
    {
        ffmpegfs_error(destname(), "Could not allocate FIFO.");
        return AVERROR(ENOMEM);
    }
    return 0;
}

// Prepare format optimisations
int FFMPEG_Transcoder::update_format(AVDictionary** dict, LPCPROFILE_OPTION option) const
{
    int ret = 0;

    if (option == NULL)
    {
        return 0;
    }

    for (LPCPROFILE_OPTION p = option; p->m_key != NULL; p++)
    {
        if ((p->m_options & OPT_AUDIO) && m_out.m_video.m_nStream_idx != INVALID_STREAM)
        {
            // Option for audio only, but file contains video stream
            continue;
        }

        if ((p->m_options & OPT_VIDEO) && m_out.m_video.m_nStream_idx == INVALID_STREAM)
        {
            // Option for video, but file contains no video stream
            continue;
        }

        ffmpegfs_trace(destname(), "Profile format option -%s%s%s.",  p->m_key, *p->m_value ? " " : "", p->m_value);

        ret = av_dict_set_with_check(dict, p->m_key, p->m_value, p->m_flags, destname());
        if (ret < 0)
        {
            break;
        }
    }
    return ret;
}

int FFMPEG_Transcoder::prepare_mp4_format(AVDictionary** dict) const
{
    int ret = 0;

    for (int n = 0; m_profile[n].m_profile != PROFILE_INVALID; n++)
    {
        if (m_profile[n].m_filetype == FILETYPE_MP4 && m_profile[n].m_profile == params.m_profile)
        {
            ret = update_format(dict, m_profile[n].m_option_format);
            break;
        }
    }

    // All
    av_dict_set_with_check(dict, "flags:a", "+global_header", 0, destname());
    av_dict_set_with_check(dict, "flags:v", "+global_header", 0, destname());

    return ret;
}

int FFMPEG_Transcoder::prepare_webm_format(AVDictionary **dict) const
{
    int ret = 0;

    for (int n = 0; m_profile[n].m_profile != PROFILE_INVALID; n++)
    {
        if (m_profile[n].m_filetype == FILETYPE_WEBM && m_profile[n].m_profile == params.m_profile)
        {
            ret = update_format(dict, m_profile[n].m_option_format);
            break;
        }
    }

    return ret;
}

// Write the header of the output file container.
int FFMPEG_Transcoder::write_output_file_header()
{
    AVDictionary* dict = NULL;
    int ret;

    switch (m_out.m_file_type)
    {
    case FILETYPE_MP4:
    case FILETYPE_MOV:
    {
        ret = prepare_mp4_format(&dict);
        if (ret < 0)
        {
            return ret;
        }
        break;
    }
    case FILETYPE_WEBM:
    {
        ret = prepare_webm_format(&dict);
        if (ret < 0)
        {
            return ret;
        }
        break;
    }
    default:
    {
        break;
    }
    }

    ret = avformat_write_header(m_out.m_pFormat_ctx, &dict);
    if (ret < 0)
    {
        ffmpegfs_error(destname(), "Could not write output file header (error '%s').", ffmpeg_geterror(ret).c_str());
        return ret;
    }

    if (m_out.m_file_type == FILETYPE_WAV)
    {
        // Insert fake WAV header (fill in size fields with estimated values instead of setting to -1)
        AVIOContext * output_io_context = (AVIOContext *)m_out.m_pFormat_ctx->pb;
        Buffer *buffer = (Buffer *)output_io_context->opaque;
        size_t pos = buffer->tell();
        WAV_HEADER wav_header;
        LIST_HEADER list_header;
        DATA_HEADER data_header;

        buffer->copy((uint8_t*)&wav_header, 0, sizeof(WAV_HEADER));
        buffer->copy((uint8_t*)&list_header, sizeof(WAV_HEADER), sizeof(LIST_HEADER));
        buffer->copy((uint8_t*)&data_header, sizeof(WAV_HEADER) + sizeof(LIST_HEADER) + list_header.data_bytes - 4, sizeof(DATA_HEADER));

        wav_header.wav_size = (int)(m_predicted_size - 8);
        data_header.data_bytes = (int)(m_predicted_size - (sizeof(WAV_HEADER) + sizeof(LIST_HEADER) + sizeof(DATA_HEADER) + list_header.data_bytes - 4));

        buffer->seek(0, SEEK_SET);
        buffer->write((uint8_t*)&wav_header, sizeof(WAV_HEADER));
        buffer->seek(sizeof(WAV_HEADER) + sizeof(LIST_HEADER) + list_header.data_bytes - 4, SEEK_SET);
        buffer->write((uint8_t*)&data_header, sizeof(DATA_HEADER));
        buffer->seek(pos, SEEK_SET);
    }

    return 0;
}

AVFrame *FFMPEG_Transcoder::alloc_picture(AVPixelFormat pix_fmt, int width, int height)
{
    AVFrame *picture;
    int ret;

    picture = av_frame_alloc();
    if (!picture)
    {
        return NULL;
    }

    picture->format = pix_fmt;
    picture->width  = width;
    picture->height = height;

    // allocate the buffers for the frame data
    ret = av_frame_get_buffer(picture, 32);
    if (ret < 0)
    {
        ffmpegfs_error(destname(), "Could not allocate frame data.");
        av_frame_free(&picture);
        return NULL;
    }

    return picture;
}

#if LAVC_NEW_PACKET_INTERFACE
// This does not quite work like avcodec_decode_audio4/avcodec_decode_video2.
// There is the following difference: if you got a frame, you must call
// it again with pkt=NULL. pkt==NULL is treated differently from pkt->size==0
// (pkt==NULL means get more output, pkt->size==0 is a flush/drain packet)
int FFMPEG_Transcoder::decode(AVCodecContext *avctx, AVFrame *frame, int *got_frame, AVPacket *pkt) const
{
    int ret;

    *got_frame = 0;

    if (pkt)
    {
        ret = avcodec_send_packet(avctx, pkt);
        // In particular, we don't expect AVERROR(EAGAIN), because we read all
        // decoded frames with avcodec_receive_frame() until done.
        if (ret < 0 && ret != AVERROR_EOF)
        {
            ffmpegfs_error(filename(), "Could not send packet to decoder (error '%s').", ffmpeg_geterror(ret).c_str());
            return ret;
        }
    }

    ret = avcodec_receive_frame(avctx, frame);
    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
    {
        ffmpegfs_error(filename(), "Could not receive packet from decoder (error '%s').", ffmpeg_geterror(ret).c_str());
    }

    *got_frame = (ret >= 0) ? 1 : 0;

    return ret;
}
#endif

int FFMPEG_Transcoder::decode_audio_frame(AVPacket *pkt, int *decoded)
{
    int data_present = 0;
    int ret = 0;

    *decoded = 0;

    // Decode the audio frame stored in the temporary packet.
    // The input audio stream decoder is used to do this.
    // If we are at the end of the file, pass an empty packet to the decoder
    // to flush it.

    // Since FFMpeg version >= 3.2 this is deprecated
#if  !LAVC_NEW_PACKET_INTERFACE
    // Temporary storage of the input samples of the frame read from the file.
    AVFrame *frame = NULL;

    // Initialise temporary storage for one input frame.
    ret = init_frame(&frame, filename());
    if (ret < 0)
    {
        return ret;
    }

    ret = avcodec_decode_audio4(m_in.m_audio.m_pCodec_ctx, frame, &data_present, pkt);

    if (ret < 0 && ret != AVERROR(EINVAL))
    {
        ffmpegfs_error(filename(), "Could not decode audio frame (error '%s').", ffmpeg_geterror(ret).c_str());
        // unused frame
        av_frame_free(&frame);
        return ret;
    }

    *decoded = ret;
    ret = 0;

    {
#else
    bool again = false;

    data_present = 0;

    // read all the output frames (in general there may be any number of them)
    while (ret >= 0)
    {
        AVFrame *frame = NULL;

        // Initialise temporary storage for one input frame.
        ret = init_frame(&frame, filename());
        if (ret < 0)
        {
            return ret;
        }

        ret = decode(m_in.m_audio.m_pCodec_ctx, frame, &data_present, again ? NULL : pkt);
        if (!data_present)
        {
            // unused frame
            av_frame_free(&frame);
            break;
        }
        if (ret < 0)
        {
            // Anything else is an error, report it!
            ffmpegfs_error(filename(), "Could not decode audio frame (error '%s').", ffmpeg_geterror(ret).c_str());
            // unused frame
            av_frame_free(&frame);
            break;
        }

        again = true;

        *decoded += pkt->size;
#endif
        // If there is decoded data, convert and store it
        if (data_present && frame->nb_samples)
        {
            // Temporary storage for the converted input samples.
            uint8_t **converted_input_samples = NULL;
            int nb_output_samples;
#if LAVR_DEPRECATE
            nb_output_samples = (m_pAudio_resample_ctx != NULL) ? swr_get_out_samples(m_pAudio_resample_ctx, frame->nb_samples) : frame->nb_samples;
#else
            nb_output_samples = (m_pAudio_resample_ctx != NULL) ? avresample_get_out_samples(m_pAudio_resample_ctx, frame->nb_samples) : frame->nb_samples;
#endif

            try
            {
                // Store audio frame
                // Initialise the temporary storage for the converted input samples.
                ret = init_converted_samples(&converted_input_samples, nb_output_samples);
                if (ret < 0)
                {
                    throw ret;
                }

                // Convert the input samples to the desired output sample format.
                // This requires a temporary storage provided by converted_input_samples.

                ret = convert_samples(frame->extended_data, frame->nb_samples, converted_input_samples, &nb_output_samples);
                if (ret < 0)
                {
                    throw ret;
                }

                // Add the converted input samples to the FIFO buffer for later processing.
                ret = add_samples_to_fifo(converted_input_samples, nb_output_samples);
                if (ret < 0)
                {
                    throw ret;
                }
                ret = 0;
            }
            catch (int _ret)
            {
                ret = _ret;
            }

            if (converted_input_samples)
            {
                av_freep(&converted_input_samples[0]);
                free(converted_input_samples);
            }
        }
        av_frame_free(&frame);
    }
    return ret;
}

int FFMPEG_Transcoder::decode_video_frame(AVPacket *pkt, int *decoded)
{
    int data_present;
    int ret = 0;

    *decoded = 0;

    // NOTE1: some codecs are stream based (mpegvideo, mpegaudio)
    // and this is the only method to use them because you cannot
    // know the compressed data size before analysing it.

    // BUT some other codecs (msmpeg4, mpeg4) are inherently frame
    // based, so you must call them with all the data for one
    // frame exactly. You must also initialise 'width' and
    // 'height' before initialising them.

    // NOTE2: some codecs allow the raw parameters (frame size,
    // sample rate) to be changed at any frame. We handle this, so
    // you should also take care of it

    // Since FFMpeg version >= 3.2 this is deprecated
#if !LAVC_NEW_PACKET_INTERFACE
    // Temporary storage of the input samples of the frame read from the file.
    AVFrame *frame = NULL;

    // Initialise temporary storage for one input frame.
    ret = init_frame(&frame, filename());
    if (ret < 0)
    {
        return ret;
    }

    ret = avcodec_decode_video2(m_in.m_video.m_pCodec_ctx, frame, &data_present, pkt);

    if (ret < 0 && ret != AVERROR(EINVAL))
    {
        ffmpegfs_error(filename(), "Could not decode video frame (error '%s').", ffmpeg_geterror(ret).c_str());
        // unused frame
        av_frame_free(&frame);
        return ret;
    }

    *decoded = ret;
    ret = 0;

    {
#else
    bool again = false;

    data_present = 0;

    // read all the output frames (in general there may be any number of them)
    while (ret >= 0)
    {
        AVFrame *frame = NULL;

        // Initialise temporary storage for one input frame.
        ret = init_frame(&frame, filename());
        if (ret < 0)
        {
            return ret;
        }

        ret = decode(m_in.m_video.m_pCodec_ctx, frame, &data_present, again ? NULL : pkt);
        if (!data_present)
        {
            // unused frame
            av_frame_free(&frame);
            break;
        }
        if (ret < 0)
        {
            // Anything else is an error, report it!
            ffmpegfs_error(filename(), "Could not decode audio frame (error '%s').", ffmpeg_geterror(ret).c_str());
            // unused frame
            av_frame_free(&frame);
            break;
        }

        again = true;
        *decoded += pkt->size;
#endif

        // Sometimes only a few packets contain valid dts/pts/pos data, so we keep it
        if (pkt->dts != (int64_t)AV_NOPTS_VALUE)
        {
            int64_t pts = pkt->dts;
            if (pts > m_pts)
            {
                m_pts = pts;
            }
        }
        else if (pkt->pts != (int64_t)AV_NOPTS_VALUE)
        {
            int64_t pts = pkt->pts;
            if (pts > m_pts)
            {
                m_pts = pts;
            }
        }

        if (pkt->pos > -1)
        {
            m_pos = pkt->pos;
        }

        if (data_present)
        {
            if (m_pSws_ctx != NULL)
            {
                AVCodecContext *codec_ctx = m_out.m_video.m_pCodec_ctx;

                AVFrame * tmp_frame = alloc_picture(codec_ctx->pix_fmt, codec_ctx->width, codec_ctx->height);
                if (!tmp_frame)
                {
                    return AVERROR(ENOMEM);
                }

                sws_scale(m_pSws_ctx,
                          (const uint8_t * const *)frame->data, frame->linesize,
                          0, frame->height,
                          tmp_frame->data, tmp_frame->linesize);

                tmp_frame->pts = frame->pts;
#ifndef USING_LIBAV
                tmp_frame->best_effort_timestamp = frame->best_effort_timestamp;
#endif

                av_frame_free(&frame);

                frame = tmp_frame;
            }

#ifndef USING_LIBAV
#if LAVF_DEP_AVSTREAM_CODEC
            int64_t best_effort_timestamp = frame->best_effort_timestamp;
#else
            int64_t best_effort_timestamp = av_frame_get_best_effort_timestamp(frame);
#endif

            if (best_effort_timestamp != (int64_t)AV_NOPTS_VALUE)
            {
                frame->pts = best_effort_timestamp;
            }
#endif

            if (frame->pts == (int64_t)AV_NOPTS_VALUE)
            {
                frame->pts = m_pts;
            }

            // Rescale to our time base, but only of nessessary
            if (frame->pts != (int64_t)AV_NOPTS_VALUE && (m_in.m_video.m_pStream->time_base.den != m_out.m_video.m_pStream->time_base.den || m_in.m_video.m_pStream->time_base.num != m_out.m_video.m_pStream->time_base.num))
            {
                frame->pts = av_rescale_q_rnd(frame->pts, m_in.m_video.m_pStream->time_base, m_out.m_video.m_pStream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            }

            frame->quality = m_out.m_video.m_pCodec_ctx->global_quality;
#ifndef USING_LIBAV
            frame->pict_type = AV_PICTURE_TYPE_NONE;	// other than AV_PICTURE_TYPE_NONE causes warnings
            m_VideoFifo.push(send_filters(frame, ret));
#else
            frame->pict_type = (AVPictureType)0;        // other than 0 causes warnings
            m_VideoFifo.push(frame);
#endif
        }
        else
        {
            // unused frame
            av_frame_free(&frame);
        }
    }

    return ret;
}

int FFMPEG_Transcoder::decode_frame(AVPacket *pkt)
{
    int ret = 0;

    if (pkt->stream_index == m_in.m_audio.m_nStream_idx && m_out.m_audio.m_nStream_idx > -1)
    {
        int decoded = 0;
        ret = decode_audio_frame(pkt, &decoded);
    }
    else if (pkt->stream_index == m_in.m_video.m_nStream_idx && m_out.m_video.m_nStream_idx > -1)
    {
        int decoded = 0;
#if LAVC_NEW_PACKET_INTERFACE
        int lastret = 0;
#endif

        // Can someone tell me why this seems required??? If this is not done some videos become garbled.
        do
        {
            // Decode one frame.
            ret = decode_video_frame(pkt, &decoded);

#if LAVC_NEW_PACKET_INTERFACE
            if ((ret == AVERROR(EAGAIN) && ret == lastret) || ret == AVERROR_EOF)
            {
                // If EAGAIN reported twice or stream at EOF
                // quit loop, but this is not an error
                // (must process all streams).
                break;
            }

            if (ret < 0 && ret != AVERROR(EAGAIN))
            {
                ffmpegfs_error(filename(), "Could not decode frame (error '%s').", ffmpeg_geterror(ret).c_str());
                return ret;
            }

            lastret = ret;
#else
            if (ret < 0)
            {
                ffmpegfs_error(filename(), "Could not decode frame (error '%s').", ffmpeg_geterror(ret).c_str());
                return ret;
            }
#endif
            pkt->data += decoded;
            pkt->size -= decoded;
        }
#if LAVC_NEW_PACKET_INTERFACE
        while (pkt->size > 0 && (ret == 0 || ret == AVERROR(EAGAIN)));
#else
        while (pkt->size > 0);
#endif
        ret = 0;
    }
    else
    {
        for (size_t n = 0; n < m_in.m_aAlbumArt.size(); n++)
        {
            AVStream *input_stream = m_in.m_aAlbumArt.at(n).m_pStream;

            // AV_DISPOSITION_ATTACHED_PIC streams already processed in process_albumarts()
            if (pkt->stream_index == input_stream->index && !(input_stream->disposition & AV_DISPOSITION_ATTACHED_PIC))
            {
                AVStream *output_stream = m_out.m_aAlbumArt.at(n).m_pStream;

                ret = add_albumart_frame(output_stream, pkt);
                break;
            }
        }
    }

    if (!params.m_decoding_errors && ret < 0 && ret != AVERROR(EAGAIN))
    {
        ret = 0;
    }

    return ret;
}

// Initialise a temporary storage for the specified number of audio samples.
// The conversion requires temporary storage due to the different format.
// The number of audio samples to be allocated is specified in frame_size.

int FFMPEG_Transcoder::init_converted_samples(uint8_t ***converted_input_samples, int frame_size)
{
    int ret;

    // Allocate as many pointers as there are audio channels.
    // Each pointer will later point to the audio samples of the corresponding
    // channels (although it may be NULL for interleaved formats).

    if (!(*converted_input_samples = (uint8_t **)calloc(m_out.m_audio.m_pCodec_ctx->channels, sizeof(**converted_input_samples))))
    {
        ffmpegfs_error(destname(), "Could not allocate converted input sample pointers.");
        return AVERROR(ENOMEM);
    }

    // Allocate memory for the samples of all channels in one consecutive
    // block for convenience.
    ret = av_samples_alloc(*converted_input_samples, NULL,
                           m_out.m_audio.m_pCodec_ctx->channels,
                           frame_size,
                           m_out.m_audio.m_pCodec_ctx->sample_fmt, 0);
    if (ret < 0)
    {
        ffmpegfs_error(destname(), "Could not allocate converted input samples (error '%s').", ffmpeg_geterror(ret).c_str());
        av_freep(&(*converted_input_samples)[0]);
        free(*converted_input_samples);
        return ret;
    }
    return 0;
}

#if LAVR_DEPRECATE
// Convert the input audio samples into the output sample format.
// The conversion happens on a per-frame basis, the size of which is
// specified by frame_size.
int FFMPEG_Transcoder::convert_samples(uint8_t **input_data, const int in_samples, uint8_t **converted_data, int *out_samples)
{
    if (m_pAudio_resample_ctx != NULL)
    {
        int ret;

        // Convert the samples using the resampler.
        ret = swr_convert(m_pAudio_resample_ctx, converted_data, *out_samples, (const uint8_t **)input_data, in_samples);
        if (ret  < 0)
        {
            ffmpegfs_error(destname(), "Could not convert input samples (error '%s').", ffmpeg_geterror(ret).c_str());
            return ret;
        }

        *out_samples = ret;
    }
    else
    {
        // No resampling, just copy samples
        if (!av_sample_fmt_is_planar(m_in.m_audio.m_pCodec_ctx->sample_fmt))
        {
            memcpy(converted_data[0], input_data[0], in_samples * av_get_bytes_per_sample(m_out.m_audio.m_pCodec_ctx->sample_fmt) * m_in.m_audio.m_pCodec_ctx->channels);
        }
        else
        {
            for (int n = 0; n < m_in.m_audio.m_pCodec_ctx->channels; n++)
            {
                memcpy(converted_data[n], input_data[n], in_samples * av_get_bytes_per_sample(m_out.m_audio.m_pCodec_ctx->sample_fmt));
            }
        }
    }
    return 0;
}
#else
// Convert the input audio samples into the output sample format.
// The conversion happens on a per-frame basis, the size of which is specified
// by frame_size.
int FFMPEG_Transcoder::convert_samples(uint8_t **input_data, const int in_samples, uint8_t **converted_data, int *out_samples)
{
    if (m_pAudio_resample_ctx != NULL)
    {
        int ret;

        // Convert the samples using the resampler.
        ret = avresample_convert(m_pAudio_resample_ctx, converted_data, 0, *out_samples, input_data, 0, in_samples);
        if (ret < 0)
        {
            ffmpegfs_error(destname(), "Could not convert input samples (error '%s').", ffmpeg_geterror(ret).c_str());
            return ret;
        }

        *out_samples = ret;

        // Perform a sanity check so that the number of converted samples is
        // not greater than the number of samples to be converted.
        // If the sample rates differ, this case has to be handled differently

        if (avresample_available(m_pAudio_resample_ctx))
        {
            ffmpegfs_error(destname(), "Converted samples left over.");
            return AVERROR_EXIT;
        }
    }
    else
    {
        // No resampling, just copy samples
        if (!av_sample_fmt_is_planar(m_in.m_audio.m_pCodec_ctx->sample_fmt))
        {
            memcpy(converted_data[0], input_data[0], in_samples * av_get_bytes_per_sample(m_out.m_audio.m_pCodec_ctx->sample_fmt) * m_in.m_audio.m_pCodec_ctx->channels);
        }
        else
        {
            for (int n = 0; n < m_in.m_audio.m_pCodec_ctx->channels; n++)
            {
                memcpy(converted_data[n], input_data[n], in_samples * av_get_bytes_per_sample(m_out.m_audio.m_pCodec_ctx->sample_fmt));
            }
        }
    }
    return 0;
}
#endif

// Add converted input audio samples to the FIFO buffer for later processing.
int FFMPEG_Transcoder::add_samples_to_fifo(uint8_t **converted_input_samples, const int frame_size)
{
    int ret;

    // Make the FIFO as large as it needs to be to hold both,
    // the old and the new samples.

    ret = av_audio_fifo_realloc(m_pAudioFifo, av_audio_fifo_size(m_pAudioFifo) + frame_size);
    if (ret < 0)
    {
        ffmpegfs_error(destname(), "Could not reallocate FIFO.");
        return ret;
    }

    // Store the new samples in the FIFO buffer.
    ret = av_audio_fifo_write(m_pAudioFifo, (void **)converted_input_samples, frame_size);
    if (ret < frame_size)
    {
        if (ret < 0)
        {
            ffmpegfs_error(destname(), "Could not write data to FIFO (error '%s').", ffmpeg_geterror(ret).c_str());
        }
        else
        {
            ffmpegfs_error(destname(), "Could not write data to FIFO.");
            ret = AVERROR_EXIT;
        }
        return AVERROR_EXIT;
    }

    return 0;
}

// Flush the remaining frames
int FFMPEG_Transcoder::flush_frames(int stream_index)
{
    int ret = 0;

    if (stream_index > INVALID_STREAM)
    {
        int (FFMPEG_Transcoder::*decode_frame_ptr)(AVPacket *pkt, int *decoded) = NULL;

        if (stream_index == m_in.m_audio.m_nStream_idx && m_out.m_audio.m_nStream_idx > -1)
        {
            decode_frame_ptr = &FFMPEG_Transcoder::decode_audio_frame;
        }
        else if (stream_index == m_in.m_video.m_nStream_idx && m_out.m_video.m_nStream_idx > -1)
        {
            decode_frame_ptr = &FFMPEG_Transcoder::decode_video_frame;
        }

        if (decode_frame_ptr != NULL)
        {
            AVPacket flush_packet;
            int decoded = 0;

            init_packet(&flush_packet);

            flush_packet.data = NULL;
            flush_packet.size = 0;
            flush_packet.stream_index = stream_index;

            do
            {
                ret = (this->*decode_frame_ptr)(&flush_packet, &decoded);
                if (ret < 0 && ret != AVERROR(EAGAIN))
                {
                    break;
                }
            }
            while (decoded);

            av_packet_unref(&flush_packet);
        }
    }

    return ret;
}

int FFMPEG_Transcoder::read_decode_convert_and_store(int *finished)
{
    // Packet used for temporary storage.
    AVPacket pkt;
    int ret = 0;

    try
    {
        // Read one audio frame from the input file into a temporary packet.
        ret = av_read_frame(m_in.m_pFormat_ctx, &pkt);

        if (ret < 0)
        {
            if (ret == AVERROR_EOF)
            {
                // If we are the the end of the file, flush the decoder below.
                *finished = 1;
            }
            else
            {
                ffmpegfs_error(destname(), "Could not read frame (error '%s').", ffmpeg_geterror(ret).c_str());
                throw ret;
            }
        }

        if (!*finished)
        {
            // Decode one packet, at least with the old API (!LAV_NEW_PACKET_INTERFACE)
            // it seems a packet can contain more than one frame so loop around it
            // if necessary...
            ret = decode_frame(&pkt);

            if (ret < 0 && ret != AVERROR(EAGAIN))
            {
                throw ret;
            }
        }
        else
        {
            // Flush cached frames, ignoring any errors
            if (m_in.m_audio.m_pCodec_ctx != NULL)
            {
                flush_frames(m_in.m_audio.m_nStream_idx);
            }

            if (m_in.m_video.m_pCodec_ctx != NULL)
            {
                flush_frames(m_in.m_video.m_nStream_idx);
            }
        }

        ret = 0;    // Errors will be reported by exception
    }
    catch (int _ret)
    {
        ret = _ret;
    }

    av_packet_unref(&pkt);

    return ret;
}

// Initialise one input frame for writing to the output file.
// The frame will be exactly frame_size samples large.

int FFMPEG_Transcoder::init_audio_output_frame(AVFrame **frame, int frame_size)
{
    int ret;

    // Create a new frame to store the audio samples.
    if (!(*frame = av_frame_alloc()))
    {
        ffmpegfs_error(destname(), "Could not allocate output frame.");
        return AVERROR_EXIT;
    }

    //
    // Set the frame's parameters, especially its size and format.
    // av_frame_get_buffer needs this to allocate memory for the
    // audio samples of the frame.
    // Default channel layouts based on the number of channels
    // are assumed for simplicity.

    (*frame)->nb_samples     = frame_size;
    (*frame)->channel_layout = m_out.m_audio.m_pCodec_ctx->channel_layout;
    (*frame)->format         = m_out.m_audio.m_pCodec_ctx->sample_fmt;

    // Allocate the samples of the created frame. This call will make
    // sure that the audio frame can hold as many samples as specified.

    ret = av_frame_get_buffer(*frame, 0);
    if (ret < 0)
    {
        ffmpegfs_error(destname(), "Could allocate output frame samples (error '%s').", ffmpeg_geterror(ret).c_str());
        av_frame_free(frame);
        return ret;
    }

    return 0;
}

void FFMPEG_Transcoder::produce_audio_dts(AVPacket *pkt, int64_t *pts)
{
    //    if ((pkt->pts == 0 || pkt->pts == AV_NOPTS_VALUE) && pkt->dts == AV_NOPTS_VALUE)
    {
        int64_t duration;
        // Some encoders to not produce dts/pts.
        // So we make some up.
        if (pkt->duration)
        {
            duration = pkt->duration;

            if (m_out.m_audio.m_pCodec_ctx->codec_id == AV_CODEC_ID_OPUS)
            {
                // OPUS is a bit strange. Whatever we feed into the encoder, the result will always be floating point planar
                // at 48 K sampling rate.
                // For some reason the duration calculated by the FFMpeg API is wrong. We have to rescale it to the correct value
                // TODO: Is this a FFmpeg bug or am I too stupid?
                if (duration > 0 && CODECPAR(m_out.m_audio.m_pStream)->sample_rate > 0)
                {
                    pkt->duration = duration = av_rescale(duration, (int64_t)m_out.m_audio.m_pStream->time_base.den * m_out.m_audio.m_pCodec_ctx->ticks_per_frame, CODECPAR(m_out.m_audio.m_pStream)->sample_rate * (int64_t)m_out.m_audio.m_pStream->time_base.num);
                }
            }

        }
        else
        {
            duration = 1;
        }

        pkt->dts = *pts;
        pkt->pts = *pts + duration;

        *pts += duration;
    }
}

// Encode one frame worth of audio to the output file.
int FFMPEG_Transcoder:: encode_audio_frame(AVFrame *frame, int *data_present)
{
    // Packet used for temporary storage.
    AVPacket pkt;
    int ret;

    init_packet(&pkt);

    // Encode the audio frame and store it in the temporary packet.
    // The output audio stream encoder is used to do this.
#if !LAVC_NEW_PACKET_INTERFACE
    ret = avcodec_encode_audio2(m_out.m_audio.m_pCodec_ctx, &pkt, frame, data_present);

    if (ret < 0)
    {
        ffmpegfs_error(destname(), "Could not encode audio frame (error '%s').", ffmpeg_geterror(ret).c_str());
        av_packet_unref(&pkt);
        return ret;
    }

    {
#else
    *data_present = 0;

    // send the frame for encoding
    ret = avcodec_send_frame(m_out.m_audio.m_pCodec_ctx, frame);
    if (ret < 0 && ret != AVERROR_EOF)
    {
        ffmpegfs_error(destname(), "Could not encode audio frame (error '%s').", ffmpeg_geterror(ret).c_str());
        av_packet_unref(&pkt);
        return ret;
    }

    // read all the available output packets (in general there may be any number of them)
    while (ret >= 0)
    {
        *data_present = 0;

        ret = avcodec_receive_packet(m_out.m_audio.m_pCodec_ctx, &pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            av_packet_unref(&pkt);
            return ret;
        }
        else if (ret < 0)
        {
            ffmpegfs_error(destname(), "Could not encode audio frame (error '%s').", ffmpeg_geterror(ret).c_str());
            av_packet_unref(&pkt);
            return ret;
        }

        *data_present = 1;
#endif
        // Write one audio frame from the temporary packet to the output file.
        if (*data_present)
        {
            pkt.stream_index = m_out.m_audio.m_nStream_idx;

            produce_audio_dts(&pkt, &m_out.m_nAudio_pts);

            ret = av_interleaved_write_frame(m_out.m_pFormat_ctx, &pkt);
            if (ret < 0)
            {
                ffmpegfs_error(destname(), "Could not write audio frame (error '%s').", ffmpeg_geterror(ret).c_str());
                av_packet_unref(&pkt);
                return ret;
            }
        }

        av_packet_unref(&pkt);
    }

    return 0;
}

// Encode one frame worth of audio to the output file.
int FFMPEG_Transcoder::encode_video_frame(AVFrame *frame, int *data_present)
{
    // Packet used for temporary storage.
    AVPacket pkt;
    int ret;

    init_packet(&pkt);

    if (frame != NULL)
    {
#if LAVF_DEP_AVSTREAM_CODEC
        if (frame->interlaced_frame)
        {
            if (m_out.m_video.m_pCodec_ctx->codec->id == AV_CODEC_ID_MJPEG)
            {
                m_out.m_video.m_pStream->codecpar->field_order = frame->top_field_first ? AV_FIELD_TT:AV_FIELD_BB;
            }
            else
            {
                m_out.m_video.m_pStream->codecpar->field_order = frame->top_field_first ? AV_FIELD_TB:AV_FIELD_BT;
            }
        }
        else
        {
            m_out.m_video.m_pStream->codecpar->field_order = AV_FIELD_PROGRESSIVE;
        }
#endif
    }

    // Encode the video frame and store it in the temporary packet.
    // The output video stream encoder is used to do this.
#if !LAVC_NEW_PACKET_INTERFACE
    ret = avcodec_encode_video2(m_out.m_video.m_pCodec_ctx, &pkt, frame, data_present);

    if (ret < 0)
    {
        ffmpegfs_error(destname(), "Could not encode video frame (error '%s').", ffmpeg_geterror(ret).c_str());
        av_packet_unref(&pkt);
        return ret;
    }

    {
#else
    *data_present = 0;

    // send the frame for encoding
    ret = avcodec_send_frame(m_out.m_video.m_pCodec_ctx, frame);
    if (ret < 0 && ret != AVERROR_EOF)
    {
        ffmpegfs_error(destname(), "Could not encode video frame (error '%s').", ffmpeg_geterror(ret).c_str());
        av_packet_unref(&pkt);
        return ret;
    }

    // read all the available output packets (in general there may be any number of them
    while (ret >= 0)
    {
        *data_present = 0;

        ret = avcodec_receive_packet(m_out.m_video.m_pCodec_ctx, &pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            av_packet_unref(&pkt);
            return ret;
        }
        else if (ret < 0)
        {
            ffmpegfs_error(destname(), "Could not encode video frame (error '%s').", ffmpeg_geterror(ret).c_str());
            av_packet_unref(&pkt);
            return ret;
        }

        *data_present = 1;
#endif

        // Write one video frame from the temporary packet to the output file.
        if (*data_present)
        {
            if (pkt.pts != (int64_t)AV_NOPTS_VALUE)
            {
                pkt.pts -=  m_out.m_video_start_pts;
            }

            if (pkt.dts != (int64_t)AV_NOPTS_VALUE)
            {
                pkt.dts -=  m_out.m_video_start_pts;
            }

            if (!(m_out.m_pFormat_ctx->oformat->flags & AVFMT_NOTIMESTAMPS))
            {
                if (pkt.dts != (int64_t)AV_NOPTS_VALUE &&
                        pkt.pts != (int64_t)AV_NOPTS_VALUE &&
                        pkt.dts > pkt.pts)
                {

                    ffmpegfs_warning(destname(), "Invalid DTS: %" PRId64 " PTS: %" PRId64 " in video output, replacing by guess.", pkt.dts, pkt.pts);

                    pkt.pts =
                            pkt.dts = pkt.pts + pkt.dts + m_out.m_last_mux_dts + 1
                            - FFMIN3(pkt.pts, pkt.dts, m_out.m_last_mux_dts + 1)
                            - FFMAX3(pkt.pts, pkt.dts, m_out.m_last_mux_dts + 1);
                }

                if (pkt.dts != (int64_t)AV_NOPTS_VALUE && m_out.m_last_mux_dts != (int64_t)AV_NOPTS_VALUE)
                {
                    int64_t max = m_out.m_last_mux_dts + !(m_out.m_pFormat_ctx->oformat->flags & AVFMT_TS_NONSTRICT);

                    if (pkt.dts < max)
                    {
                        ffmpegfs_warning(destname(), "Non-monotonous DTS in video output stream; previous: %" PRId64 ", current: %" PRId64 "; changing to %" PRId64 ". This may result in incorrect timestamps in the output.", m_out.m_last_mux_dts, pkt.dts, max);

                        if (pkt.pts >= pkt.dts)
                        {
                            pkt.pts = FFMAX(pkt.pts, max);
                        }
                        pkt.dts = max;
                    }
                }
            }

            m_out.m_last_mux_dts = pkt.dts;

            ret = av_interleaved_write_frame(m_out.m_pFormat_ctx, &pkt);
            if (ret < 0)
            {
                ffmpegfs_error(destname(), "Could not write video frame (error '%s').", ffmpeg_geterror(ret).c_str());
                av_packet_unref(&pkt);
                return ret;
            }
        }

        av_packet_unref(&pkt);
    }

    return 0;
}

// Load one audio frame from the FIFO buffer, encode and write it to the
// output file.

int FFMPEG_Transcoder::load_encode_and_write(int frame_size)
{
    // Temporary storage of the output samples of the frame written to the file.
    AVFrame *output_frame;
    int ret = 0;

    // Use the maximum number of possible samples per frame.
    // If there is less than the maximum possible frame size in the FIFO
    // buffer use this number. Otherwise, use the maximum possible frame size

    frame_size = FFMIN(av_audio_fifo_size(m_pAudioFifo), frame_size);
    int data_written;

    // Initialise temporary storage for one output frame.
    ret = init_audio_output_frame(&output_frame, frame_size);
    if (ret < 0)
    {
        return ret;
    }

    // Read as many samples from the FIFO buffer as required to fill the frame.
    // The samples are stored in the frame temporarily.

    ret = av_audio_fifo_read(m_pAudioFifo, (void **)output_frame->data, frame_size);
    if (ret < frame_size)
    {
        if (ret < 0)
        {
            ffmpegfs_error(destname(), "Could not read data from FIFO (error '%s').", ffmpeg_geterror(ret).c_str());
        }
        else
        {
            ffmpegfs_error(destname(), "Could not read data from FIFO.");
            ret = AVERROR_EXIT;
        }
        av_frame_free(&output_frame);
        return ret;
    }

    // Encode one frame worth of audio samples.
    ret = encode_audio_frame(output_frame, &data_written);
#if !LAVC_NEW_PACKET_INTERFACE
    if (ret < 0)
#else
    if (ret < 0 && ret != AVERROR(EAGAIN))
#endif
    {
        av_frame_free(&output_frame);
        return ret;
    }
    av_frame_free(&output_frame);
    return 0;
}

// Write the trailer of the output file container.
int FFMPEG_Transcoder::write_output_file_trailer()
{
    int ret;

    ret = av_write_trailer(m_out.m_pFormat_ctx);
    if (ret < 0)
    {
        ffmpegfs_error(destname(), "Could not write output file trailer (error '%s').", ffmpeg_geterror(ret).c_str());
        return ret;
    }

    return 0;
}

time_t FFMPEG_Transcoder::mtime() const
{
    return m_mtime;
}

// Process the metadata in the FFmpeg file. This should be called at the
// beginning, before reading audio data. The set_text_tag() and
// set_picture_tag() methods of the given Encoder will be used to set the
// metadata, with results going into the given Buffer. This function will also
// read the actual PCM stream parameters.

#define tagcpy(dst, src)    \
    for (char *p1 = (dst), *pend = p1 + sizeof(dst), *p2 = (src); *p2 && p1 < pend; p1++, p2++) \
    *p1 = *p2;

void FFMPEG_Transcoder::copy_metadata(AVDictionary **metadata_out, const AVDictionary *metadata_in)
{
    AVDictionaryEntry *tag = NULL;

    while ((tag = av_dict_get(metadata_in, "", tag, AV_DICT_IGNORE_SUFFIX)))
    {
        av_dict_set_with_check(metadata_out, tag->key, tag->value, 0, destname());

        if (m_out.m_file_type == FILETYPE_MP3)
        {
            // For MP3 fill in ID3v1 structure
            if (!strcasecmp(tag->key, "ARTIST"))
            {
                tagcpy(m_out.m_id3v1.m_artist, tag->value);
            }
            else if (!strcasecmp(tag->key, "TITLE"))
            {
                tagcpy(m_out.m_id3v1.m_title, tag->value);
            }
            else if (!strcasecmp(tag->key, "ALBUM"))
            {
                tagcpy(m_out.m_id3v1.m_album, tag->value);
            }
            else if (!strcasecmp(tag->key, "COMMENT"))
            {
                tagcpy(m_out.m_id3v1.m_comment, tag->value);
            }
            else if (!strcasecmp(tag->key, "YEAR") || !strcasecmp(tag->key, "DATE"))
            {
                tagcpy(m_out.m_id3v1.m_year, tag->value);
            }
            else if (!strcasecmp(tag->key, "TRACK"))
            {
                m_out.m_id3v1.m_title_no = (char)atoi(tag->value);
            }
        }
    }
}

// Copy metadata from source to target
//
// Returns:
//  0   if OK
//  <0  ffmepg error

int FFMPEG_Transcoder::process_metadata()
{
    ffmpegfs_trace(destname(), "Processing metadata.");

    if (m_in.m_audio.m_pStream != NULL && CODECPAR(m_in.m_audio.m_pStream)->codec_id == AV_CODEC_ID_VORBIS)
    {
        // For some formats (namely ogg) FFmpeg returns the tags, odd enough, with streams...
        copy_metadata(&m_out.m_pFormat_ctx->metadata, m_in.m_audio.m_pStream->metadata);
    }

    copy_metadata(&m_out.m_pFormat_ctx->metadata, m_in.m_pFormat_ctx->metadata);

    if (m_out.m_audio.m_pStream != NULL && m_in.m_audio.m_pStream != NULL)
    {
        // Copy audio stream meta data
        copy_metadata(&m_out.m_audio.m_pStream->metadata, m_in.m_audio.m_pStream->metadata);
    }

    if (m_out.m_video.m_pStream != NULL && m_in.m_video.m_pStream != NULL)
    {
        // Copy video stream meta data
        copy_metadata(&m_out.m_video.m_pStream->metadata, m_in.m_video.m_pStream->metadata);
    }

    // Also copy album art meta tags
    for (size_t n = 0; n < m_in.m_aAlbumArt.size(); n++)
    {
        AVStream *input_stream = m_in.m_aAlbumArt.at(n).m_pStream;
        AVStream *output_stream = m_out.m_aAlbumArt.at(n).m_pStream;

        copy_metadata(&output_stream->metadata, input_stream->metadata);
    }

    return 0;
}

int FFMPEG_Transcoder::process_albumarts()
{
    int ret = 0;

    for (size_t n = 0; n < m_in.m_aAlbumArt.size(); n++)
    {
        AVStream *input_stream = m_in.m_aAlbumArt.at(n).m_pStream;

        if (input_stream->disposition & AV_DISPOSITION_ATTACHED_PIC)
        {
            AVStream *output_stream = m_out.m_aAlbumArt.at(n).m_pStream;

            ret = add_albumart_frame(output_stream, &input_stream->attached_pic);
            if (ret < 0)
            {
                break;
            }
        }
    }

    return ret;
}

// Process a single frame of audio data. The encode_pcm_data() method
// of the Encoder will be used to process the resulting audio data, with the
// result going into the given Buffer.
//
// Returns:
//  0   if decoding was OK
//  1   if EOF reached
//  -1  error

int FFMPEG_Transcoder::process_single_fr(int &status)
{
    int finished = 0;
    int ret = 0;

    status = 0;

    try
    {
        if (m_out.m_audio.m_nStream_idx > -1)
        {
            int output_frame_size;

            if (m_out.m_audio.m_pCodec_ctx->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
            {
                // Encode supports variable frame size, use an arbitrary value
                output_frame_size =  10000;
            }
            else
            {
                // Use the encoder's desired frame size for processing.
                output_frame_size = m_out.m_audio.m_pCodec_ctx->frame_size;
            }

            // Make sure that there is one frame worth of samples in the FIFO
            // buffer so that the encoder can do its work.
            // Since the decoder's and the encoder's frame size may differ, we
            // need to FIFO buffer to store as many frames worth of input samples
            // that they make up at least one frame worth of output samples.

            while (av_audio_fifo_size(m_pAudioFifo) < output_frame_size)
            {
                // Decode one frame worth of audio samples, convert it to the
                // output sample format and put it into the FIFO buffer.

                ret = read_decode_convert_and_store(&finished);
                if (ret < 0)
                {
                    throw ret;
                }

                // If we are at the end of the input file, we continue
                // encoding the remaining audio samples to the output file.

                if (finished)
                {
                    break;
                }
            }

            // If we have enough samples for the encoder, we encode them.
            // At the end of the file, we pass the remaining samples to
            // the encoder.

            while (av_audio_fifo_size(m_pAudioFifo) >= output_frame_size || (finished && av_audio_fifo_size(m_pAudioFifo) > 0))
            {
                // Take one frame worth of audio samples from the FIFO buffer,
                // encode it and write it to the output file.

                ret = load_encode_and_write(output_frame_size);
                if (ret < 0)
                {
                    throw ret;
                }
            }

            // If we are at the end of the input file and have encoded
            // all remaining samples, we can exit this loop and finish.

            if (finished)
            {
                if (m_out.m_audio.m_pCodec_ctx != NULL)
                {
                    // Flush the encoder as it may have delayed frames.
                    int data_written = 0;
                    do
                    {
                        ret = encode_audio_frame(NULL, &data_written);
#if LAVC_NEW_PACKET_INTERFACE
                        if (ret == AVERROR_EOF)
                        {
                            // Not an error
                            break;
                        }

                        if (ret < 0 && ret != AVERROR(EAGAIN))
                        {
                            ffmpegfs_error(destname(), "Could not encode audio frame (error '%s').", ffmpeg_geterror(ret).c_str());
                            throw ret;
                        }
#else
                        if (ret < 0)
                        {
                            ffmpegfs_error(destname(), "Could not encode audio frame (error '%s').", ffmpeg_geterror(ret).c_str());
                            throw ret;
                        }
#endif
                    }
                    while (data_written);
                }

                status = 1;
            }
        }
        else
        {
            ret = read_decode_convert_and_store(&finished);
            if (ret < 0)
            {
                throw ret;
            }

            if (finished)
            {
                status = 1;
            }
        }

        while (!m_VideoFifo.empty())
        {
            AVFrame *output_frame = m_VideoFifo.front();
            m_VideoFifo.pop();

            // Encode one video frame.
            int data_written = 0;
            output_frame->key_frame = 0;    // Leave that decision to decoder
            ret = encode_video_frame(output_frame, &data_written);
#if !LAVC_NEW_PACKET_INTERFACE
            if (ret < 0)
#else
            if (ret < 0 && ret != AVERROR(EAGAIN))
#endif
            {
                av_frame_free(&output_frame);
                throw ret;
            }
            av_frame_free(&output_frame);
        }

#if LAVC_NEW_PACKET_INTERFACE
        ret = 0;    // May be AVERROR(EAGAIN)
#endif

        // If we are at the end of the input file and have encoded
        // all remaining samples, we can exit this loop and finish.

        if (finished)
        {
            if (m_out.m_video.m_pCodec_ctx != NULL)
            {
                // Flush the encoder as it may have delayed frames.
                int data_written = 0;
                do
                {
                    ret = encode_video_frame(NULL, &data_written);
#if LAVC_NEW_PACKET_INTERFACE
                    if (ret == AVERROR_EOF)
                    {
                        // Not an error
                        break;
                    }
                    if (ret < 0 && ret != AVERROR(EAGAIN))
                    {
                        ffmpegfs_error(destname(), "Could not encode video frame (error '%s').", ffmpeg_geterror(ret).c_str());
                        throw ret;
                    }
#else
                    if (ret < 0)
                    {
                        throw ret;
                    }
#endif
                }
                while (data_written);
            }

            status = 1;
        }
    }
    catch (int _ret)
    {
        status = -1;
        return _ret;
    }

    return 0;
}

// Try to predict final file size.
size_t FFMPEG_Transcoder::predict_filesize(const char * filename, double duration, BITRATE input_audio_bit_rate, int input_sample_rate, BITRATE input_video_bit_rate, bool is_video) const
{
    AVCodecID audio_codec_id = params.m_audio_codecid;
    AVCodecID video_codec_id = params.m_video_codecid;
    FILETYPE file_type = params.m_filetype;
    size_t size = 0;

    if (input_audio_bit_rate)
    {
        BITRATE output_audio_bit_rate;

        get_output_bit_rate(input_audio_bit_rate, params.m_audiobitrate, &output_audio_bit_rate);

        switch (audio_codec_id)
        {
        case AV_CODEC_ID_AAC:
        {
            // Try to predict the size of the AAC stream (this is fairly accurate, sometimes a bit larger, sometimes a bit too small
            size += (size_t)(duration * 1.025 * (double)output_audio_bit_rate / 8); // add 2.5% for overhead
            break;
        }
        case AV_CODEC_ID_MP3:
        {
            // Kbps = bits per second / 8 = Bytes per second x 60 seconds = Bytes per minute x 60 minutes = Bytes per hour
            // This is the sum of the size of
            // ID3v2, ID3v1, and raw MP3 data. This is theoretically only approximate
            // but in practice gives excellent answers, usually exactly correct.
            // Cast to 64-bit int to avoid overflow.

            size += (size_t)(duration * (double)output_audio_bit_rate / 8) + ID3V1_TAG_LENGTH;
            break;
        }
        case AV_CODEC_ID_PCM_S16LE:
        case AV_CODEC_ID_PCM_S16BE:
        {
            int channels = 2; //m_in.m_audio.m_pCodec_ctx->channels;
            int bytes_per_sample =  2; // av_get_bytes_per_sample(m_in.m_audio.m_pCodec_ctx->sample_fmt);
            int output_sample_rate;

            get_output_sample_rate(input_sample_rate, params.m_audiosamplerate, &output_sample_rate);

            // File size:
            // file duration * sample rate (HZ) * channels * bytes per sample
            // + WAV_HEADER + DATA_HEADER + (with FFMpeg always) LIST_HEADER
            // The real size of the list header is unkown as we don't know the contents (meta tags)
            size += (size_t)(duration * output_sample_rate * channels * bytes_per_sample) + sizeof(WAV_HEADER) + sizeof(LIST_HEADER) + sizeof(DATA_HEADER);
            break;
        }
        case AV_CODEC_ID_VORBIS:
        {
            // Kbps = bits per second / 8 = Bytes per second x 60 seconds = Bytes per minute x 60 minutes = Bytes per hour
            size += (size_t)(duration * (double)output_audio_bit_rate / 8) /*+ ID3V1_TAG_LENGTH*/;// TODO ???
            break;
        }
        case AV_CODEC_ID_OPUS:
        {
            // Kbps = bits per second / 8 = Bytes per second x 60 seconds = Bytes per minute x 60 minutes = Bytes per hour
            size += (size_t)(duration * (double)output_audio_bit_rate / 8) /*+ ID3V1_TAG_LENGTH*/;// TODO ???
            break;
        }
        case AV_CODEC_ID_NONE:
        {
            break;
        }
        default:
        {
            ffmpegfs_error(filename, "Internal error - unsupported audio codec '%s' for format %s.", get_codec_name(audio_codec_id, 0), params.m_desttype);
            break;
        }
        }
    }

    if (input_video_bit_rate)
    {
        if (is_video)
        {
            BITRATE out_video_bit_rate;
            int bitrateoverhead = 0;

            get_output_bit_rate(input_video_bit_rate, params.m_videobitrate, &out_video_bit_rate);

            out_video_bit_rate += bitrateoverhead;

            switch (video_codec_id)
            {
            case AV_CODEC_ID_H264:
            {
                size += (size_t)(duration * 1.025  * (double)out_video_bit_rate / 8); // add 2.5% for overhead
                break;
            }
            case AV_CODEC_ID_MJPEG:
            {
                // TODO... size += ???
                break;
            }
            case AV_CODEC_ID_THEORA:
            {
                size += (size_t)(duration * 1.025  * (double)out_video_bit_rate / 8); // ??? // add 2.5% for overhead
                break;
            }
            case AV_CODEC_ID_VP9:
            {
                size += (size_t)(duration * 1.025  * (double)out_video_bit_rate / 8); // ??? // add 2.5% for overhead
                break;
            }
            case AV_CODEC_ID_NONE:
            {
                break;
            }
            default:
            {
                ffmpegfs_warning(filename, "Unsupported video codec '%s' for format %s.", get_codec_name(video_codec_id, 0), params.m_desttype);
                break;
            }
            }
        }
        // else      // TODO #2260: Add picture size
        // {

        // }
    }

    return size;
}

size_t FFMPEG_Transcoder::predict_filesize()
{
    if (m_predicted_size == 0 && m_in.m_pFormat_ctx != NULL)
    {
        double duration = ffmpeg_cvttime(m_in.m_pFormat_ctx->duration, av_get_time_base_q());
        BITRATE input_audio_bit_rate = 0;
        int input_sample_rate = 0;
        BITRATE input_video_bit_rate = 0;

        if (m_fileio->duration() > -1)
        {
            duration = (double)m_fileio->duration();
        }

        if (m_in.m_audio.m_nStream_idx > -1)
        {
            input_sample_rate = CODECPAR(m_in.m_audio.m_pStream)->sample_rate;
            input_audio_bit_rate = (CODECPAR(m_in.m_audio.m_pStream)->bit_rate != 0) ? CODECPAR(m_in.m_audio.m_pStream)->bit_rate : m_in.m_pFormat_ctx->bit_rate;
        }
        if (m_in.m_video.m_nStream_idx > -1)
        {
            input_video_bit_rate = (CODECPAR(m_in.m_video.m_pStream)->bit_rate != 0) ? CODECPAR(m_in.m_video.m_pStream)->bit_rate : m_in.m_pFormat_ctx->bit_rate;
        }

        m_predicted_size = predict_filesize(filename(), duration, input_audio_bit_rate, input_sample_rate, input_video_bit_rate, m_is_video);
    }

    return m_predicted_size;
}

// Encode any remaining PCM data to the given Buffer. This should be called
// after all input data has already been passed to encode_pcm_data().

int FFMPEG_Transcoder::encode_finish()
{
    int ret = 0;

    // Write the trailer of the output file container.
    ret = write_output_file_trailer();
    if (ret < 0)
    {
        ffmpegfs_error(destname(), "Error writing trailer (error '%s').", ffmpeg_geterror(ret).c_str());
    }

    return ret;
}

const ID3v1 * FFMPEG_Transcoder::id3v1tag() const
{
    return &m_out.m_id3v1;
}

int FFMPEG_Transcoder::input_read(void * opaque, unsigned char * data, int size)
{
    fileio * io = (fileio *)opaque;

    if (io == NULL)
    {
        return AVERROR(EINVAL);
    }

    if (io->eof())
    {
        // At EOF
        return AVERROR_EOF;
    }

    int read = io->read((char *)data, size);

    if (read != size && io->error())
    {
        // Read failed
        return AVERROR(io->error());
    }

    return read;
}

int FFMPEG_Transcoder::output_write(void * opaque, unsigned char * data, int size)
{
    Buffer * buffer = (Buffer *)opaque;

    if (buffer == NULL)
    {
        return AVERROR(EINVAL);
    }

    return (int)buffer->write((const uint8_t*)data, size);
}

int64_t FFMPEG_Transcoder::seek(void * opaque, int64_t offset, int whence)
{
    fileio * io = (fileio *)opaque;
    int64_t res_offset = 0;

    if (io == NULL)
    {
        return AVERROR(EINVAL);
    }

    if (whence & AVSEEK_SIZE)
    {
        // Return file size
        res_offset = io->size();
    }
    else
    {
        whence &= ~(AVSEEK_SIZE | AVSEEK_FORCE);

        if (!io->seek(offset, whence))
        {
            // OK: Return position
            res_offset = offset;
        }
        else
        {
            // Error
            res_offset = AVERROR(errno);
        }
    }

    return res_offset;
}

// Close the open FFmpeg file
void FFMPEG_Transcoder::close()
{
    int nAudioSamplesLeft = 0;
    size_t nVideoFramesLeft = 0;
    string file;
    bool bClosed = false;

    if (m_pAudioFifo)
    {
        nAudioSamplesLeft = av_audio_fifo_size(m_pAudioFifo);
        av_audio_fifo_free(m_pAudioFifo);
        m_pAudioFifo = NULL;
        bClosed = true;
    }

    nVideoFramesLeft = m_VideoFifo.size();
    while (m_VideoFifo.size())
    {
        AVFrame *output_frame = m_VideoFifo.front();
        m_VideoFifo.pop();

        av_frame_free(&output_frame);
        bClosed = true;
    }

    if (m_pAudio_resample_ctx)
    {
#if LAVR_DEPRECATE
        swr_free(&m_pAudio_resample_ctx);
#else
        avresample_close(m_pAudio_resample_ctx);
        avresample_free(&m_pAudio_resample_ctx);
#endif
        m_pAudio_resample_ctx = NULL;
        bClosed = true;
    }

    if (m_pSws_ctx != NULL)
    {
        sws_freeContext(m_pSws_ctx);
        m_pSws_ctx = NULL;
        bClosed = true;
    }

    // Close output file
#if !LAVF_DEP_AVSTREAM_CODEC
    if (m_out.m_audio.m_pCodec_ctx)
    {
        avcodec_close(m_out.m_audio.m_pCodec_ctx);
        m_out.m_audio.m_pCodec_ctx = NULL;
        bClosed = true;
    }

    if (m_out.m_video.m_pCodec_ctx)
    {
        avcodec_close(m_out.m_video.m_pCodec_ctx);
        m_out.m_video.m_pCodec_ctx = NULL;
        bClosed = true;
    }
#else
    if (m_out.m_audio.m_pCodec_ctx)
    {
        avcodec_free_context(&m_out.m_audio.m_pCodec_ctx);
        m_out.m_audio.m_pCodec_ctx = NULL;
        bClosed = true;
    }

    if (m_out.m_video.m_pCodec_ctx)
    {
        avcodec_free_context(&m_out.m_video.m_pCodec_ctx);
        m_out.m_video.m_pCodec_ctx = NULL;
        bClosed = true;
    }
#endif

    while (m_out.m_aAlbumArt.size())
    {
        AVCodecContext *codec_ctx = m_out.m_aAlbumArt.back().m_pCodec_ctx;
        m_out.m_aAlbumArt.pop_back();
        if (codec_ctx != NULL)
        {
#if !LAVF_DEP_AVSTREAM_CODEC
            avcodec_close(codec_ctx);
#else
            avcodec_free_context(&codec_ctx);
#endif
            bClosed = true;
        }
    }

    if (m_out.m_pFormat_ctx != NULL)
    {
#if LAVF_DEP_FILENAME
        if (m_out.m_pFormat_ctx->url != NULL)
        {
            file = m_out.m_pFormat_ctx->url;
        }
#else
        // lavf 58.7.100 - avformat.h - deprecated
        file = m_out.m_pFormat_ctx->filename;
#endif

        if (m_out.m_pFormat_ctx->pb != NULL)
        {
            // 2017-09-01 - xxxxxxx - lavf 57.80.100 / 57.11.0 - avio.h
            //  Add avio_context_free(). From now on it must be used for freeing AVIOContext.
#if (LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 80, 0))
            av_freep(&m_out.m_pFormat_ctx->pb->buffer);
            avio_context_free(&m_out.m_pFormat_ctx->pb);
#else
            av_freep(m_out.m_pFormat_ctx->pb);
#endif
            m_out.m_pFormat_ctx->pb = NULL;
        }

        avformat_free_context(m_out.m_pFormat_ctx);

        m_out.m_pFormat_ctx = NULL;
        bClosed = true;
    }

    // Close input file
#if !LAVF_DEP_AVSTREAM_CODEC
    if (m_in.m_audio.m_pCodec_ctx)
    {
        avcodec_close(m_in.m_audio.m_pCodec_ctx);
        m_in.m_audio.m_pCodec_ctx = NULL;
        bClosed = true;
    }

    if (m_in.m_video.m_pCodec_ctx)
    {
        avcodec_close(m_in.m_video.m_pCodec_ctx);
        m_in.m_video.m_pCodec_ctx = NULL;
        bClosed = true;
    }
#else
    if (m_in.m_audio.m_pCodec_ctx)
    {
        avcodec_free_context(&m_in.m_audio.m_pCodec_ctx);
        m_in.m_audio.m_pCodec_ctx = NULL;
        bClosed = true;
    }

    if (m_in.m_video.m_pCodec_ctx)
    {
        avcodec_free_context(&m_in.m_video.m_pCodec_ctx);
        m_in.m_video.m_pCodec_ctx = NULL;
        bClosed = true;
    }
#endif

    while (m_in.m_aAlbumArt.size())
    {
        AVCodecContext *codec_ctx = m_in.m_aAlbumArt.back().m_pCodec_ctx;
        m_in.m_aAlbumArt.pop_back();
        if (codec_ctx != NULL)
        {
#if !LAVF_DEP_AVSTREAM_CODEC
            avcodec_close(codec_ctx);
#else
            avcodec_free_context(&codec_ctx);
#endif
            bClosed = true;
        }
    }

    if (m_in.m_pFormat_ctx != NULL)
    {
        if (file.empty())
        {
#if LAVF_DEP_FILENAME
            file = m_in.m_pFormat_ctx->url;
#else
            // lavf 58.7.100 - avformat.h - deprecated
            file = m_in.m_pFormat_ctx->filename;
#endif
        }

        //if (!(m_in.m_pFormat_ctx->oformat->flags & AVFMT_NOFILE))
        {
            if (m_fileio != NULL)
            {
                m_fileio->close();
                delete m_fileio;
                m_fileio = NULL;
            }

            if (m_in.m_pFormat_ctx->pb != NULL)
            {
                // 2017-09-01 - xxxxxxx - lavf 57.80.100 / 57.11.0 - avio.h
                //  Add avio_context_free(). From now on it must be used for freeing AVIOContext.
#if (LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 80, 0))
                avio_context_free(&m_in.m_pFormat_ctx->pb);
#else
                av_freep(m_in.m_pFormat_ctx->pb);
#endif
                m_in.m_pFormat_ctx->pb = NULL;
            }
        }

        avformat_close_input(&m_in.m_pFormat_ctx);
        m_in.m_pFormat_ctx = NULL;
        bClosed = true;
    }

#ifndef USING_LIBAV
    free_filters();
#endif  // !USING_LIBAV

    if (bClosed)
    {
        const char *p = file.empty() ? NULL : file.c_str();
        if (nAudioSamplesLeft)
        {
            ffmpegfs_warning(p, "%i audio samples left in buffer and not written to target file!", nAudioSamplesLeft);
        }

        if (nVideoFramesLeft)
        {
            ffmpegfs_warning(p, "%zu video frames left in buffer and not written to target file!", nVideoFramesLeft);
        }

        // Closed anything...
        ffmpegfs_info(p, "FFmpeg transcoder closed.");
    }
}

const char *FFMPEG_Transcoder::filename() const
{
    return m_in.m_filename.c_str();
}

const char *FFMPEG_Transcoder::destname() const
{
    return m_out.m_filename.c_str();
}

#ifndef USING_LIBAV
// create
int FFMPEG_Transcoder::init_filters(AVCodecContext *pCodecContext, AVStream * pStream)
{
    const char * filters;
    char args[1024];
    const AVFilter * pBufferSrc     = avfilter_get_by_name("buffer");
    const AVFilter * pBufferSink    = avfilter_get_by_name("buffersink");
    AVFilterInOut * pOutputs        = avfilter_inout_alloc();
    AVFilterInOut * pInputs         = avfilter_inout_alloc();
    //enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_GRAY8, AV_PIX_FMT_NONE };
    int ret = 0;

    m_pBufferSinkContext = NULL;
    m_pBufferSourceContext = NULL;
    m_pFilterGraph = NULL;

    try
    {
        if (pStream == NULL)
        {
            throw (int)AVERROR(EINVAL);
        }

        if (!pStream->avg_frame_rate.den && !pStream->avg_frame_rate.num)
        {
            // No framerate, so this video "stream" has only one picture
            throw (int)AVERROR(EINVAL);
        }

        m_pFilterGraph = avfilter_graph_alloc();

        AVBufferSinkParams bufferSinkParams;
        enum AVPixelFormat aePixelFormat[3];
#if LAVF_DEP_AVSTREAM_CODEC
        AVPixelFormat pix_fmt = (AVPixelFormat)pStream->codecpar->format;
#else
        AVPixelFormat pix_fmt = (AVPixelFormat)pStream->codec->pix_fmt;
#endif

        if (pOutputs == NULL || pInputs == NULL || m_pFilterGraph == NULL)
        {
            throw (int)AVERROR(ENOMEM);
        }

        // buffer video source: the decoded frames from the decoder will be inserted here.
        sprintf(args, "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                pCodecContext->width, pCodecContext->height, pCodecContext->pix_fmt,
                pStream->time_base.num, pStream->time_base.den,
                pCodecContext->sample_aspect_ratio.num, FFMAX(pCodecContext->sample_aspect_ratio.den, 1));

        //AVRational fr = av_guess_frame_rate(m_m_out.m_pFormat_ctx, m_pVideoStream, NULL);
        //if (fr.num && fr.den)
        //{
        //    av_strlcatf(buffersrc_args, sizeof(buffersrc_args), ":frame_rate=%d/%d", fr.num, fr.den);
        //}
        //
        //args.sprintf("%d:%d:%d:%d:%d", m_pCodecContext->width, m_pCodecContext->height, m_pCodecContext->format, 0, 0); //  0, 0 ok?

        ret = avfilter_graph_create_filter(&m_pBufferSourceContext, pBufferSrc, "in", args, NULL, m_pFilterGraph);

        if (ret < 0)
        {
            ffmpegfs_error(destname(), "Cannot create buffer source (error '%s').", ffmpeg_geterror(ret).c_str());
            throw  ret;
        }

        //av_opt_set(m_pBufferSourceContext, "thread_type", "slice", AV_OPT_SEARCH_CHILDREN);
        //av_opt_set_int(m_pBufferSourceContext, "threads", FFMAX(1, av_cpu_count()), AV_OPT_SEARCH_CHILDREN);
        //av_opt_set_int(m_pBufferSourceContext, "threads", 16, AV_OPT_SEARCH_CHILDREN);

        // buffer video sink: to terminate the filter chain.

        if (pix_fmt == AV_PIX_FMT_NV12)
        {
            aePixelFormat[0] = AV_PIX_FMT_NV12;
            aePixelFormat[1] = AV_PIX_FMT_YUV420P;
        }

        else
        {
            aePixelFormat[0] = pix_fmt;
            aePixelFormat[1] = AV_PIX_FMT_NONE;
        }

        aePixelFormat[2] = AV_PIX_FMT_NONE;

        bufferSinkParams.pixel_fmts = aePixelFormat;
        ret = avfilter_graph_create_filter(&m_pBufferSinkContext, pBufferSink, "out", NULL, &bufferSinkParams, m_pFilterGraph);

        if (ret < 0)
        {
            ffmpegfs_error(destname(), "Cannot create buffer sink (error '%s').", ffmpeg_geterror(ret).c_str());
            throw  ret;
        }

        //ret = av_opt_set_int_list(m_pBufferSinkContext, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
        //if (ret < 0)
        //{
        //    ffmpegfs_error(NULL, "Cannot set output pixel format (error '%s').", ffmpeg_geterror(ret).c_str());
        //    throw  ret;
        //}

        //ret = av_opt_set_bin(m_pBufferSinkContext, "pix_fmts", (uint8_t*)&pCodecContext->pix_fmt, sizeof(pCodecContext->pix_fmt), AV_OPT_SEARCH_CHILDREN);
        //if (ret < 0)
        //{
        //    ffmpegfs_error(NULL, "Cannot set output pixel format (error '%s').", ffmpeg_geterror(ret).c_str());
        //    throw  ret;
        //}

        // Endpoints for the filter graph.
        pOutputs->name          = av_strdup("in");
        pOutputs->filter_ctx    = m_pBufferSourceContext;
        pOutputs->pad_idx       = 0;
        pOutputs->next          = NULL;
        pInputs->name           = av_strdup("out");
        pInputs->filter_ctx     = m_pBufferSinkContext;
        pInputs->pad_idx        = 0;
        pInputs->next           = NULL;

        // args "null"      passthrough (dummy) filter for video
        // args "anull"     passthrough (dummy) filter for audio

        // https://stackoverflow.com/questions/31163120/c-applying-filter-in-ffmpeg
        //filters = "yadif=mode=send_frame:parity=auto:deint=interlaced";
        filters = "yadif=mode=send_frame:parity=auto:deint=all";
        //filters = "yadif=0:-1:0";
        //filters = "bwdif=mode=send_frame:parity=auto:deint=all";
        //filters = "kerndeint=thresh=10:map=0:order=0:sharp=1:twoway=1";

        ret = avfilter_graph_parse_ptr(m_pFilterGraph, filters, &pInputs, &pOutputs, NULL);
        if (ret < 0)
        {
            ffmpegfs_error(destname(), "avfilter_graph_parse_ptr failed (error '%s').", ffmpeg_geterror(ret).c_str());
            throw  ret;
        }

        ret = avfilter_graph_config(m_pFilterGraph, NULL);
        if (ret < 0)
        {
            ffmpegfs_error(destname(), "avfilter_graph_config failed (error '%s').", ffmpeg_geterror(ret).c_str());
            throw  ret;
        }

        ffmpegfs_debug(destname(), "Deinterlacing initialised with filters '%s'.", filters);
    }
    catch (int _ret)
    {
        ret = _ret;
    }

    if (pInputs != NULL)
    {
        avfilter_inout_free(&pInputs);
    }
    if (pOutputs != NULL)
    {
        avfilter_inout_free(&pOutputs);
    }

    return ret;
}

AVFrame *FFMPEG_Transcoder::send_filters(AVFrame * srcframe, int & ret)
{
    AVFrame *tgtframe = srcframe;

    ret = 0;

    if (m_pBufferSourceContext != NULL && srcframe->interlaced_frame)
    {
        try
        {
            AVFrame * filterframe   = NULL;

            //pFrame->pts = av_frame_get_best_effort_timestamp(pFrame);
            // push the decoded frame into the filtergraph

            if ((ret = ::av_buffersrc_add_frame_flags(m_pBufferSourceContext, srcframe, AV_BUFFERSRC_FLAG_KEEP_REF)) < 0)
            {
                ffmpegfs_warning(destname(), "Error while feeding the frame to filtergraph (error '%s').", ffmpeg_geterror(ret).c_str());
                throw ret;
            }

            filterframe = ::av_frame_alloc();
            if (filterframe == NULL)
            {
                ret = AVERROR(ENOMEM);
                ffmpegfs_error(destname(), "Unable to allocate filter frame (error '%s').", ffmpeg_geterror(ret).c_str());
                throw ret;
            }

            // pull filtered frames from the filtergraph
            ret = ::av_buffersink_get_frame(m_pBufferSinkContext, filterframe);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            {
                // Not an error, go on
                ::av_frame_unref(filterframe);
                ret = 0;
            }
            else if (ret < 0)
            {
                ffmpegfs_error(destname(), "Error while getting frame from filtergraph (error '%s').", ffmpeg_geterror(ret).c_str());
                ::av_frame_unref(filterframe);
                throw ret;
            }
            else
            {
                // All OK; copy filtered frame and unref original
                tgtframe = filterframe;

                tgtframe->pts = srcframe->pts;
#if LAVF_DEP_AVSTREAM_CODEC
                tgtframe->best_effort_timestamp = srcframe->best_effort_timestamp;
#else
                tgtframe->best_effort_timestamp = av_frame_get_best_effort_timestamp(srcframe);
#endif
                ::av_frame_unref(srcframe);
            }
        }
        catch (int _ret)
        {
            ret = _ret;
        }
    }

    return tgtframe;
}


// free

void FFMPEG_Transcoder::free_filters()
{
    if (m_pBufferSinkContext != NULL)
    {
        ::avfilter_free(m_pBufferSinkContext);
        m_pBufferSinkContext = NULL;
    }

    if (m_pBufferSourceContext != NULL)
    {
        ::avfilter_free(m_pBufferSourceContext);
        m_pBufferSourceContext = NULL;
    }

    if (m_pFilterGraph != NULL)
    {
        ::avfilter_graph_free(&m_pFilterGraph);
        m_pFilterGraph = NULL;
    }
}
#endif  // !USING_LIBAV
