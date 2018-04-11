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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
FFMPEG_Transcoder::FFMPEG_Transcoder()
    : m_nCalculated_size(0)
    , m_bIsVideo(false)
    , m_pAudio_resample_ctx(NULL)
    , m_pAudioFifo(NULL)
    , m_pSws_ctx(NULL)
    , m_pts(AV_NOPTS_VALUE)
    , m_pos(AV_NOPTS_VALUE)
    , m_in(
{
          .m_pFormat_ctx = NULL,
          .m_pAudio_codec_ctx = NULL,
          .m_pVideo_codec_ctx = NULL,
          .m_pAudio_stream = NULL,
          .m_pVideo_stream = NULL,
          .m_nAudio_stream_idx = INVALID_STREAM,
          .m_nVideo_stream_idx = INVALID_STREAM,
          .m_pszFileName = NULL
        })
    , m_out(
{
          .m_output_type = TYPE_UNKNOWN,
          .m_pFormat_ctx = NULL,
          .m_pAudio_codec_ctx = NULL,
          .m_pVideo_codec_ctx = NULL,
          .m_pAudio_stream = NULL,
          .m_pVideo_stream = NULL,
          .m_nAudio_stream_idx = INVALID_STREAM,
          .m_nVideo_stream_idx = INVALID_STREAM,
          .m_nAudio_pts = 0,
          .m_audio_start_pts = 0,
          .m_video_start_pts = 0,
          .m_last_mux_dts = (int64_t)AV_NOPTS_VALUE,
          })
{
#pragma GCC diagnostic pop
    ffmpegfs_trace("FFmpeg trancoder: ready to initialise.");

    // Initialise ID3v1.1 tag structure
    init_id3v1(&m_out.m_id3v1);
}

// Free the FFmpeg en/decoder
// after the transcoding process has finished.
FFMPEG_Transcoder::~FFMPEG_Transcoder()
{

    // Close fifo and resample context
    close();

    ffmpegfs_trace("FFmpeg trancoder: object destroyed.");
}

// FFmpeg handles cover arts like video streams.
// Try to find out if we have a video stream or a cover art.
bool FFMPEG_Transcoder::is_video() const
{
    bool bIsVideo = false;

    if (m_in.m_pVideo_codec_ctx != NULL && m_in.m_pVideo_stream != NULL)
    {
        if ((m_in.m_pVideo_codec_ctx->codec_id == AV_CODEC_ID_PNG) || (m_in.m_pVideo_codec_ctx->codec_id == AV_CODEC_ID_MJPEG))
        {
            bIsVideo = false;

#ifdef USING_LIBAV
            if (m_in.m_pVideo_stream->avg_frame_rate.den)
            {
                double dbFrameRate = (double)m_in.m_pVideo_stream->avg_frame_rate.num / m_in.m_pVideo_stream->avg_frame_rate.den;

                // If frame rate is < 100 fps this should be a video
                if (dbFrameRate < 100)
                {
                    bIsVideo = true;
                }
            }
#else
            if (m_in.m_pVideo_stream->r_frame_rate.den)
            {
                double dbFrameRate = (double)m_in.m_pVideo_stream->r_frame_rate.num / m_in.m_pVideo_stream->r_frame_rate.den;

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
int FFMPEG_Transcoder::open_input_file(const char* filename)
{
    AVDictionary * opt = NULL;
    int ret;

    ffmpegfs_trace("Initialising FFmpeg transcoder for '%s'.", filename);

    struct stat s;
    if (stat(filename, &s) < 0)
    {
        ffmpegfs_error("File stat failed for '%s'.", filename);
        return -1;
    }
    m_mtime = s.st_mtime;

    if (is_open())
    {
        ffmpegfs_warning("File '%s' already open.", filename);
        return 0;
    }

    //    This allows selecting if the demuxer should consider all streams to be
    //    found after the first PMT and add further streams during decoding or if it rather
    //    should scan all that are within the analyze-duration and other limits

    ret = av_dict_set_with_check(&opt, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);

    //    av_dict_set_with_check(&opt, "analyzeduration", "8000000", 0);  // <<== honored
    //    av_dict_set_with_check(&opt, "probesize", "8000000", 0);    //<<== honored

    if (ret < 0)
    {
        return ret;
    }

    // Open the input file to read from it.
    ret = avformat_open_input(&m_in.m_pFormat_ctx, filename, NULL, &opt);
    if (ret < 0)
    {
        ffmpegfs_error("Could not open input file (error '%s') '%s'.", ffmpeg_geterror(ret).c_str(), filename);
        return ret;
    }

#if LAVF_DEP_FILENAME
    m_in.m_pszFileName = m_in.m_pFormat_ctx->url;
#else
    // lavf 58.7.100 - avformat.h - deprecated
    m_in.m_pszFileName = m_in.m_pFormat_ctx->filename;
#endif

    ret = av_dict_set_with_check(&opt, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE);
    if (ret < 0)
    {
        return ret;
    }

    AVDictionaryEntry * t;
    if ((t = av_dict_get(opt, "", NULL, AV_DICT_IGNORE_SUFFIX)))
    {
        ffmpegfs_error("Option %s not found for '%s'.", t->key, filename);
        return -1; // Couldn't open file
    }

#if (LIBAVFORMAT_VERSION_MICRO >= 100 && LIBAVFORMAT_VERSION_INT >= LIBAVCODEC_MIN_VERSION_INT )
    av_format_inject_global_side_data(m_in.m_pFormat_ctx);
#endif

    // Get information on the input file (number of streams etc.).
    ret = avformat_find_stream_info(m_in.m_pFormat_ctx, NULL);
    if (ret < 0)
    {
        ffmpegfs_error("Could not open find stream info (error '%s') for '%s'.", ffmpeg_geterror(ret).c_str(), filename);
        return ret;
    }

    //     av_dump_format(m_in.m_pFormat_ctx, 0, filename, 0);

    // Open best match video codec
    ret = open_codec_context(&m_in.m_nVideo_stream_idx, &m_in.m_pVideo_codec_ctx, m_in.m_pFormat_ctx, AVMEDIA_TYPE_VIDEO, filename);
    if (ret < 0 && ret != AVERROR_STREAM_NOT_FOUND)    // Not an error
    {
        ffmpegfs_error("Failed to open video codec (error '%s') for '%s'.", ffmpeg_geterror(ret).c_str(), filename);
        return ret;
    }

    if (m_in.m_nVideo_stream_idx >= 0)
    {
        // We have a video stream
        m_in.m_pVideo_stream = m_in.m_pFormat_ctx->streams[m_in.m_nVideo_stream_idx];

        m_bIsVideo = is_video();

#ifdef AV_CODEC_CAP_TRUNCATED
        if(m_in.m_pVideo_codec_ctx->codec->capabilities & AV_CODEC_CAP_TRUNCATED)
        {
            m_in.m_pVideo_codec_ctx->flags|= AV_CODEC_FLAG_TRUNCATED; // we do not send complete frames
        }
#else
#warning "Your FFMPEG distribution is missing AV_CODEC_CAP_TRUNCATED flag. Probably requires fixing!"
#endif
    }

    // Open best match audio codec
    // Save the audio decoder context for easier access later.
    ret = open_codec_context(&m_in.m_nAudio_stream_idx, &m_in.m_pAudio_codec_ctx, m_in.m_pFormat_ctx, AVMEDIA_TYPE_AUDIO, filename);
    if (ret < 0 && ret != AVERROR_STREAM_NOT_FOUND)    // Not an error
    {
        ffmpegfs_error("Failed to open audio codec (error '%s') for '%s'.", ffmpeg_geterror(ret).c_str(), filename);
        //        avformat_close_input(&m_in.m_pFormat_ctx);
        //        m_in.m_pFormat_ctx = NULL;
        return ret;
    }

    if (m_in.m_nAudio_stream_idx >= 0)
    {
        // We have an audio stream
        m_in.m_pAudio_stream = m_in.m_pFormat_ctx->streams[m_in.m_nAudio_stream_idx];
    }

    if (m_in.m_nAudio_stream_idx == -1 && m_in.m_nVideo_stream_idx == -1)
    {
        ffmpegfs_error("File contains neither a video nor an audio stream '%s'.", filename);
        return AVERROR(EINVAL);
    }

    return 0;
}

int FFMPEG_Transcoder::open_output_file(Buffer *buffer)
{
    int res = 0;

    // Pre-allocate the predicted file size to reduce memory reallocations
    if (!buffer->reserve(calculate_size()))
    {
        ffmpegfs_error("Out of memory pre-allocating buffer for '%s'.", m_in.m_pszFileName);
        return AVERROR(ENOMEM);
    }

    // Open the output file for writing.
    res = open_output_filestreams(buffer);
    if (res)
    {
        return res;
    }

    if (m_out.m_nAudio_stream_idx > -1)
    {
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

    // Process metadata. The Decoder will call the Encoder to set appropriate
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

    return 0;
}

bool FFMPEG_Transcoder::get_output_sample_rate(AVStream *stream, int max_sample_rate, int *sample_rate) const
{
#if LAVF_DEP_AVSTREAM_CODEC
    *sample_rate = stream->codecpar->sample_rate;
#else
    *sample_rate = stream->codec->sample_rate;
#endif

    if (*sample_rate > max_sample_rate)
    {
        *sample_rate = max_sample_rate;
        return true;
    }

    return false;
}

#if !defined(USING_LIBAV) && (LIBAVUTIL_VERSION_MAJOR > 54)
bool FFMPEG_Transcoder::get_output_bit_rate(AVStream *stream, int64_t max_bit_rate, int64_t * bit_rate) const
#else // USING_LIBAV
bool FFMPEG_Transcoder::get_output_bit_rate(AVStream *stream, int max_bit_rate, int * bit_rate) const
#endif
{
#if LAVF_DEP_AVSTREAM_CODEC
    *bit_rate = stream->codecpar->bit_rate != 0 ? stream->codecpar->bit_rate : m_in.m_pFormat_ctx->bit_rate;
#else
    *bit_rate = stream->codec->bit_rate != 0 ? stream->codec->bit_rate : m_in.m_pFormat_ctx->bit_rate;
#endif

    if (*bit_rate > max_bit_rate)
    {
        *bit_rate = max_bit_rate;
        return true;
    }

    return false;
}

double FFMPEG_Transcoder::get_aspect_ratio(int width, int height, const AVRational & sample_aspect_ratio)
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
        ffmpegfs_error("Could not find encoder '%s' for '%s'.", avcodec_get_name(codec_id), m_in.m_pszFileName);
        return AVERROR(EINVAL);
    }

    output_stream = avformat_new_stream(m_out.m_pFormat_ctx, output_codec);
    if (!output_stream)
    {
        ffmpegfs_error("Could not allocate stream for '%s'.", m_in.m_pszFileName);
        return AVERROR(ENOMEM);
    }
    output_stream->id = m_out.m_pFormat_ctx->nb_streams - 1;
#if (LIBAVCODEC_VERSION_MAJOR > 56) // Check for FFmpeg 3
    output_codec_ctx = avcodec_alloc_context3(output_codec);
    if (!output_codec_ctx)
    {
        ffmpegfs_error("Could not alloc an encoding context for '%s'.", m_in.m_pszFileName);;
        return AVERROR(ENOMEM);
    }
#else
    output_codec_ctx = output_stream->codec;
#endif

    switch (output_codec->type)
    {
    case AVMEDIA_TYPE_AUDIO:
    {
        // Set the basic encoder parameters.
        if (get_output_bit_rate(m_in.m_pAudio_stream, params.m_audiobitrate, &output_codec_ctx->bit_rate))
        {
            // Limit sample rate
            ffmpegfs_info("Limiting audio bit rate to %s for '%s'.", format_bitrate(output_codec_ctx->bit_rate).c_str(), m_in.m_pszFileName);
        }

        output_codec_ctx->channels                 = 2;
        output_codec_ctx->channel_layout           = av_get_default_channel_layout(output_codec_ctx->channels);
        output_codec_ctx->sample_rate              = m_in.m_pAudio_codec_ctx->sample_rate;
        if (get_output_sample_rate(m_in.m_pAudio_stream, params.m_audiosamplerate, &output_codec_ctx->sample_rate))
        {
            // Limit sample rate
            ffmpegfs_info("Limiting audio sample rate to %s for '%s'.", format_samplerate(output_codec_ctx->sample_rate).c_str(), m_in.m_pszFileName);
        }
        output_codec_ctx->sample_fmt               = output_codec->sample_fmts[0];

        // Allow the use of the experimental AAC encoder
        output_codec_ctx->strict_std_compliance    = FF_COMPLIANCE_EXPERIMENTAL;

        // Set the sample rate for the container.
        output_stream->time_base.den     = output_codec_ctx->sample_rate;
        output_stream->time_base.num     = 1;

#if (LIBAVCODEC_VERSION_MAJOR <= 56) // Check for FFmpeg 3
        // set -strict -2 for aac (required for FFmpeg 2)
        av_dict_set_with_check(&opt, "strict", "-2", 0);
        // Allow the use of the experimental AAC encoder
        output_codec_ctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
#endif

        output_stream->duration                     = m_in.m_pAudio_stream->duration;

        // Save the encoder context for easier access later.
        m_out.m_pAudio_codec_ctx            = output_codec_ctx;
        // Save the stream index
        m_out.m_nAudio_stream_idx           = output_stream->index;
        // Save output audio stream for faster reference
        m_out.m_pAudio_stream               = output_stream;
        break;
    }
    case AVMEDIA_TYPE_VIDEO:
    {
        output_codec_ctx->codec_id = codec_id;

        // Set the basic encoder parameters.
        // The input file's sample rate is used to avoid a sample rate conversion.

        //        ret = avcodec_parameters_from_context(stream->codecpar, m_in.m_pVideo_codec_ctx);
        //        if (ret < 0) {
        //            return ret;
        //        }

        if (get_output_bit_rate(m_in.m_pVideo_stream, params.m_videobitrate, &output_codec_ctx->bit_rate))
        {
            // Limit sample rate
            ffmpegfs_info("Limiting video bit rate to %s for '%s'.", format_bitrate(output_codec_ctx->bit_rate).c_str(), m_in.m_pszFileName);
        }

        //codec_ctx->bit_rate_tolerance   = 0;
        // Resolution must be a multiple of two.

#if LAVF_DEP_AVSTREAM_CODEC
        output_codec_ctx->width                     = m_in.m_pVideo_stream->codecpar->width;
        output_codec_ctx->height                    = m_in.m_pVideo_stream->codecpar->height;
#else
        output_codec_ctx->width                     = m_in.m_pVideo_stream->codec->width;
        output_codec_ctx->height                    = m_in.m_pVideo_stream->codec->height;
#endif

        if (params.m_videowidth || params.m_videoheight)
        {
            // Use command line argument(s)
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
                ffmpegfs_info("Changing video size from %i/%i to %i/%i for '%s'.", output_codec_ctx->width, output_codec_ctx->height, width, height, m_in.m_pszFileName);
                output_codec_ctx->width             = width;
                output_codec_ctx->height            = height;
            }
        }

        // timebase: This is the fundamental unit of time (in seconds) in terms
        // of which frame timestamps are represented. For fixed-fps content,
        // timebase should be 1/framerate and timestamp increments should be
        // identical to 1.
        //output_stream->time_base                  = m_in.m_pVideo_stream->time_base;

        AVRational frame_rate;
#if LAVF_DEP_AVSTREAM_CODEC
        frame_rate                                  = m_in.m_pVideo_stream->avg_frame_rate;
#else
        frame_rate                                  = m_in.m_pVideo_stream->codec->framerate;
#endif

        if (!frame_rate.num)
        {
            frame_rate                              = { .num = 25, .den = 1 };
            ffmpegfs_warning("No information about the input framerate is available. Falling back to a default value of 25fps for output stream.");
        }

        // tbn
        if (output_codec_ctx->codec_id == AV_CODEC_ID_THEORA)
        {
            // Strange, but Theora seems to need it this way...
            output_stream->time_base                = av_inv_q(frame_rate);
        }
        else
        {
            output_stream->time_base                = { .num = 1, .den = 90000 };
        }

        // tbc
        output_codec_ctx->time_base                 = output_stream->time_base;

        // tbc
#if LAVF_DEP_AVSTREAM_CODEC
        //output_stream->codecpar->time_base        = output_stream->time_base;
        //output_stream->codecpar->time_base        = m_in.m_pVideo_stream->codec->time_base;
        //output_stream->codecpar->time_base.den    *= output_stream->codec->ticks_per_frame;
#else
        output_stream->codec->time_base             = output_stream->time_base;
        //output_stream->codec->time_base           = m_in.m_pVideo_stream->codec->time_base;
        //output_stream->codec->time_base.den       *= output_stream->codec->ticks_per_frame;
#endif

#ifndef USING_LIBAV
        // tbr
        // output_stream->r_frame_rate              = m_in.m_pVideo_stream->r_frame_rate;
        // output_stream->r_frame_rate              = { .num = 25, .den = 1 };

        // fps
        output_stream->avg_frame_rate               = frame_rate;
        output_codec_ctx->framerate                 = output_stream->avg_frame_rate;
#endif

        output_codec_ctx->gop_size                  = 12;   // emit one intra frame every twelve frames at most

#if LAVF_DEP_AVSTREAM_CODEC
        output_codec_ctx->sample_aspect_ratio       = m_in.m_pVideo_stream->codecpar->sample_aspect_ratio;
#else
        output_codec_ctx->sample_aspect_ratio       = m_in.m_pVideo_stream->codec->sample_aspect_ratio;
#endif

        // At this moment the output format must be AV_PIX_FMT_YUV420P;
        output_codec_ctx->pix_fmt                   = AV_PIX_FMT_YUV420P;

        if (output_codec_ctx->codec_id == AV_CODEC_ID_H264)
        {
            // Ignore missing width/height
            //m_out.m_pFormat_ctx->oformat->flags |= AVFMT_NODIMENSIONS;

            //output_codec_ctx->flags2 |= AV_CODEC_FLAG2_FAST;
            //output_codec_ctx->flags2 = AV_CODEC_FLAG2_FASTPSKIP;
            //output_codec_ctx->profile = FF_PROFILE_H264_HIGH;
            //output_codec_ctx->level = 31;

            // -profile:v baseline -level 3.0
            //av_opt_set(output_codec_ctx->priv_data, "profile", "baseline", 0);
            //av_opt_set(output_codec_ctx->priv_data, "level", "3.0", 0);

            // -profile:v high -level 3.1
            //av_opt_set(output_codec_ctx->priv_data, "profile", "high", 0);
            //av_opt_set(output_codec_ctx->priv_data, "level", "3.1", 0);

            // Set speed (changes profile!)
            av_opt_set(output_codec_ctx->priv_data, "preset", "ultrafast", 0);
            //av_opt_set(output_codec_ctx->priv_data, "preset", "veryfast", 0);
            //av_opt_set(output_codec_ctx->priv_data, "tune", "zerolatency", 0);

            //if (!av_dict_get((AVDictionary*)output_codec_ctx->priv_data, "threads", NULL, 0))
            //{
            //  ffmpegfs_error("Setting threads to auto for codec %s for '%s'.", get_codec_name(codec_id), m_in.m_pszFileName);
            //  av_dict_set_with_check((AVDictionary**)&output_codec_ctx->priv_data, "threads", "auto", 0);
            //}
        }

#if LAVF_DEP_AVSTREAM_CODEC
        if (m_in.m_pVideo_stream->codecpar->format != output_codec_ctx->pix_fmt ||
                m_in.m_pVideo_stream->codecpar->width != output_codec_ctx->width ||
                m_in.m_pVideo_stream->codecpar->height != output_codec_ctx->height)
#else
        if (m_in.m_pVideo_stream->codec->pix_fmt != output_codec_ctx->pix_fmt ||
                m_in.m_pVideo_stream->codec->width != output_codec_ctx->width ||
                m_in.m_pVideo_stream->codec->height != output_codec_ctx->height)
#endif
        {
            // Rescale image if required
#if LAVF_DEP_AVSTREAM_CODEC
            const AVPixFmtDescriptor *fmtin = av_pix_fmt_desc_get((AVPixelFormat)m_in.m_pVideo_stream->codecpar->format);
#else
            const AVPixFmtDescriptor *fmtin = av_pix_fmt_desc_get((AVPixelFormat)m_in.m_pVideo_stream->codec->pix_fmt);
#endif
            const AVPixFmtDescriptor *fmtout = av_pix_fmt_desc_get(output_codec_ctx->pix_fmt);
#if LAVF_DEP_AVSTREAM_CODEC
            if (m_in.m_pVideo_stream->codecpar->format != output_codec_ctx->pix_fmt)
#else
            if (m_in.m_pVideo_stream->codec->pix_fmt != output_codec_ctx->pix_fmt)
#endif
            {
                ffmpegfs_info("Initialising pixel format conversion from %s to %s for '%s'.", fmtin != NULL ? fmtin->name : "-", fmtout != NULL ? fmtout->name : "-", m_in.m_pszFileName);
            }

            m_pSws_ctx = sws_getContext(
                        // Source settings
            #if LAVF_DEP_AVSTREAM_CODEC
                        m_in.m_pVideo_stream->codecpar->width,                  // width
                        m_in.m_pVideo_stream->codecpar->height,                 // height
                        (AVPixelFormat)m_in.m_pVideo_stream->codecpar->format,  // format
            #else
                        m_in.m_pVideo_stream->codec->width,         // width
                        m_in.m_pVideo_stream->codec->height,        // height
                        m_in.m_pVideo_stream->codec->pix_fmt,       // format
            #endif
                        // Target settings
                        output_codec_ctx->width,                    // width
                        output_codec_ctx->height,                   // height
                        output_codec_ctx->pix_fmt,                  // format
                        SWS_FAST_BILINEAR, NULL, NULL, NULL);
            if (!m_pSws_ctx)
            {
                ffmpegfs_error("Could not allocate scaling/conversion context for '%s'.", m_in.m_pszFileName);
                return AVERROR(ENOMEM);
            }
        }

        output_stream->duration                     = m_in.m_pVideo_stream->duration;

        // TODO: ALBUM ARTS
        // mp4 album arts do not work with ipod profile. Set mp4.
        //    if (m_out.m_pFormat_ctx->oformat->mime_type != NULL && (!strcmp(m_out.m_pFormat_ctx->oformat->mime_type, "application/mp4") || !strcmp(m_out.m_pFormat_ctx->oformat->mime_type, "video/mp4")))
        //    {
        //        m_out.m_pFormat_ctx->oformat->name = "mp4";
        //        m_out.m_pFormat_ctx->oformat->mime_type = "application/mp4";
        //    }

        // Save the encoder context for easier access later.
        m_out.m_pVideo_codec_ctx    	= output_codec_ctx;
        // Save the stream index
        m_out.m_nVideo_stream_idx   	= output_stream->index;
        // Save output video stream for faster reference
        m_out.m_pVideo_stream       	= output_stream;

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

    // Open the encoder for the audio stream to use it later.
    if ((ret = avcodec_open2(output_codec_ctx, output_codec, &opt)) < 0)
    {
        ffmpegfs_error("Could not open %s output codec %s (error '%s') for '%s'.", get_media_type_string(output_codec->type), get_codec_name(codec_id), ffmpeg_geterror(ret).c_str(), m_in.m_pszFileName);
        //        avcodec_free_context(&output_codec_ctx);
        return ret;
    }

    ffmpegfs_debug("Successfully opened %s output codec %s for '%s'.", get_media_type_string(output_codec->type), get_codec_name(codec_id), m_in.m_pszFileName);

#if (LIBAVCODEC_VERSION_MAJOR > 56) // Check for FFmpeg 3
    ret = avcodec_parameters_from_context(output_stream->codecpar, output_codec_ctx);
    if (ret < 0)
    {
        ffmpegfs_error("Could not initialise stream parameters (error '%s') for '%s'.", ffmpeg_geterror(ret).c_str(), m_in.m_pszFileName);
        //        avcodec_free_context(&output_codec_ctx);
        return ret;
    }
#endif

    return 0;
}

// Open an output file and the required encoder.
// Also set some basic encoder parameters.
// Some of these parameters are based on the input file's parameters.
int FFMPEG_Transcoder::open_output_filestreams(Buffer *buffer)
{
    AVIOContext *   output_io_context = NULL;
    AVCodecID       audio_codec_id = AV_CODEC_ID_NONE;
    AVCodecID       video_codec_id = AV_CODEC_ID_NONE;
    const char *    format;
    int             ret = 0;

    format = get_codecs(params.m_desttype, &m_out.m_output_type, &audio_codec_id, &video_codec_id);

    if (format == NULL)
    {
        ffmpegfs_error("Unknown format type '%s' for '%s'.", params.m_desttype, m_in.m_pszFileName);
        return -1;
    }

    ffmpegfs_debug("Opening format type '%s' for '%s'.", params.m_desttype, m_in.m_pszFileName);

    // Create a new format context for the output container format.
    avformat_alloc_output_context2(&m_out.m_pFormat_ctx, NULL, format, NULL);
    if (!m_out.m_pFormat_ctx)
    {
        ffmpegfs_error("Could not allocate output format context for '%s'.", m_in.m_pszFileName);
        return AVERROR(ENOMEM);
    }

    if (!m_bIsVideo)
    {
        m_in.m_nVideo_stream_idx = INVALID_STREAM;  // TEST
    }

    //video_codecid = m_out.m_pFormat_ctx->oformat->video_codec;

    if (m_in.m_nVideo_stream_idx != INVALID_STREAM && video_codec_id != AV_CODEC_ID_NONE)
    {
        ret = add_stream(video_codec_id);
        if (ret < 0)
        {
            return ret;
        }
    }

    //    m_in.m_nAudio_stream_idx = INVALID_STREAM;

    //audio_codec_id = m_out.m_pFormat_ctx->oformat->audio_codec;

    if (m_in.m_nAudio_stream_idx != INVALID_STREAM && audio_codec_id != AV_CODEC_ID_NONE)
    {
        ret = add_stream(audio_codec_id);
        if (ret < 0)
        {
            return ret;
        }
    }

    // open the output file
    int nBufSize = 1024*1024;
    output_io_context = ::avio_alloc_context(
                (unsigned char *) ::av_malloc(nBufSize + FF_INPUT_BUFFER_PADDING_SIZE),
                nBufSize,
                1,
                (void *)buffer,
                NULL /*readPacket*/,
                writePacket,
                seek);

    // Associate the output file (pointer) with the container format context.
    m_out.m_pFormat_ctx->pb = output_io_context;

    // Some formats require the time stamps to start at 0, so if there is a difference between
    // the streams we need to drop audio or video until we are in sync.
    if ((m_out.m_pVideo_stream != NULL) && (m_in.m_pAudio_stream != NULL))
    {
        // Calculate difference
        m_out.m_video_start_pts = av_rescale_q(m_in.m_pAudio_stream->start_time, m_in.m_pAudio_stream->time_base, m_out.m_pVideo_stream->time_base);
    }

    return 0;
}

// Initialise the audio resampler based on the input and output codec settings.
// If the input and output sample formats differ, a conversion is required
// libavresample takes care of this, but requires initialisation.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations" // Will replace libavresample soon, cross my heart and hope to die...
int FFMPEG_Transcoder::init_resampler()
{
    // Only initialise the resampler if it is necessary, i.e.,
    // if and only if the sample formats differ.

    if (m_in.m_pAudio_codec_ctx->sample_fmt != m_out.m_pAudio_codec_ctx->sample_fmt ||
            m_in.m_pAudio_codec_ctx->sample_rate != m_out.m_pAudio_codec_ctx->sample_rate ||
            m_in.m_pAudio_codec_ctx->channels != m_out.m_pAudio_codec_ctx->channels)
    {
        int ret;

        // Create a resampler context for the conversion.
        if (!(m_pAudio_resample_ctx = avresample_alloc_context()))
        {
            ffmpegfs_error("Could not allocate resample context for '%s'.", m_in.m_pszFileName);
            return AVERROR(ENOMEM);
        }

        // Set the conversion parameters.
        // Default channel layouts based on the number of channels
        // are assumed for simplicity (they are sometimes not detected
        // properly by the demuxer and/or decoder).

        av_opt_set_int(m_pAudio_resample_ctx, "in_channel_layout", av_get_default_channel_layout(m_in.m_pAudio_codec_ctx->channels), 0);
        av_opt_set_int(m_pAudio_resample_ctx, "out_channel_layout", av_get_default_channel_layout(m_out.m_pAudio_codec_ctx->channels), 0);
        av_opt_set_int(m_pAudio_resample_ctx, "in_sample_rate", m_in.m_pAudio_codec_ctx->sample_rate, 0);
        av_opt_set_int(m_pAudio_resample_ctx, "out_sample_rate", m_out.m_pAudio_codec_ctx->sample_rate, 0);
        av_opt_set_int(m_pAudio_resample_ctx, "in_sample_fmt", m_in.m_pAudio_codec_ctx->sample_fmt, 0);
        av_opt_set_int(m_pAudio_resample_ctx, "out_sample_fmt", m_out.m_pAudio_codec_ctx->sample_fmt, 0);

        // Open the resampler with the specified parameters.
        if ((ret = avresample_open(m_pAudio_resample_ctx)) < 0)
        {
            ffmpegfs_error("Could not open resampler context (error '%s') for '%s'.", ffmpeg_geterror(ret).c_str(), m_in.m_pszFileName);
            avresample_free(&m_pAudio_resample_ctx);
            m_pAudio_resample_ctx = NULL;
            return ret;
        }
    }
    return 0;
}
#pragma GCC diagnostic pop

// Initialise a FIFO buffer for the audio samples to be encoded.
int FFMPEG_Transcoder::init_fifo()
{
    // Create the FIFO buffer based on the specified output sample format.
    if (!(m_pAudioFifo = av_audio_fifo_alloc(m_out.m_pAudio_codec_ctx->sample_fmt, m_out.m_pAudio_codec_ctx->channels, 1)))
    {
        ffmpegfs_error("Could not allocate FIFO for '%s'.", m_in.m_pszFileName);
        return AVERROR(ENOMEM);
    }
    return 0;
}

// Write the header of the output file container.
int FFMPEG_Transcoder::write_output_file_header()
{
    int ret;
    AVDictionary* dict = NULL;

    if (m_out.m_output_type == TYPE_MP4)
    {
        // Settings for fast playback start in HTML5
        av_dict_set_with_check(&dict, "movflags", "+faststart", 0);
        av_dict_set_with_check(&dict, "frag_duration", "1000000", 0); // 1 sec

        av_dict_set_with_check(&dict, "movflags", "+empty_moov", 0);
        //av_dict_set_with_check(&dict, "movflags", "+separate_moof", 0); // MS Edge

        av_dict_set_with_check(&dict, "flags:a", "+global_header", 0);
        av_dict_set_with_check(&dict, "flags:v", "+global_header", 0);
    }

#ifdef AVSTREAM_INIT_IN_WRITE_HEADER
    if (avformat_init_output(m_out.m_pFormat_ctx, &dict) == AVSTREAM_INIT_IN_WRITE_HEADER)
    {
#endif // AVSTREAM_INIT_IN_WRITE_HEADER
        if ((ret = avformat_write_header(m_out.m_pFormat_ctx, &dict)) < 0)
        {
            ffmpegfs_error("Could not write output file header (error '%s') for '%s'.", ffmpeg_geterror(ret).c_str(), m_in.m_pszFileName);
            return ret;
        }
#ifdef AVSTREAM_INIT_IN_WRITE_HEADER
    }
#endif // AVSTREAM_INIT_IN_WRITE_HEADER

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
        ffmpegfs_error("Could not allocate frame data for '%s'.", m_in.m_pszFileName);
        av_frame_free(&picture);
        return NULL;
    }

    return picture;
}

// Decode one audio frame from the input file.
int FFMPEG_Transcoder::decode_frame(AVPacket *pkt, int *decoded)
{
    int data_present = 0;
    int ret = 0;

    *decoded = 0;

    if (pkt->stream_index == m_in.m_nAudio_stream_idx && m_out.m_nAudio_stream_idx > -1)
    {
        // Decode the audio frame stored in the temporary packet.
        // The input audio stream decoder is used to do this.
        // If we are at the end of the file, pass an empty packet to the decoder
        // to flush it.

        // Temporary storage of the input samples of the frame read from the file.
        AVFrame *input_frame = NULL;

        // Initialise temporary storage for one input frame.
        ret = init_frame(&input_frame, m_in.m_pszFileName);
        if (ret < 0)
        {
            return ret;
        }

        ret = avcodec_decode_audio4(m_in.m_pAudio_codec_ctx, input_frame, &data_present, pkt);

        if (ret < 0 && ret != AVERROR(EINVAL))
        {
            ffmpegfs_error("Could not decode audio frame (error '%s') for '%s'.", ffmpeg_geterror(ret).c_str(), m_in.m_pszFileName);
            // unused frame
            av_frame_free(&input_frame);
            return ret;
        }

        *decoded = ret;
        ret = 0;

        {
            // If there is decoded data, convert and store it
            if (data_present && input_frame->nb_samples)
            {
                // Temporary storage for the converted input samples.
                uint8_t **converted_input_samples = NULL;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations" // Will replace libavresample soon, cross my heart and hope to die...
                int nb_output_samples = (m_pAudio_resample_ctx != NULL) ? avresample_get_out_samples(m_pAudio_resample_ctx, input_frame->nb_samples) : input_frame->nb_samples;
#pragma GCC diagnostic pop
                // Store audio frame
                // Initialise the temporary storage for the converted input samples.
                ret = init_converted_samples(&converted_input_samples, nb_output_samples);
                if (ret < 0)
                {
                    goto cleanup2;
                }

                // Convert the input samples to the desired output sample format.
                // This requires a temporary storage provided by converted_input_samples.

                ret = convert_samples(input_frame->extended_data, input_frame->nb_samples, converted_input_samples, &nb_output_samples);
                if (ret < 0)
                {
                    goto cleanup2;
                }

                // Add the converted input samples to the FIFO buffer for later processing.
                ret = add_samples_to_fifo(converted_input_samples, nb_output_samples);
                if (ret < 0)
                {
                    goto cleanup2;
                }
                ret = 0;
cleanup2:
                if (converted_input_samples)
                {
                    av_freep(&converted_input_samples[0]);
                    free(converted_input_samples);
                }
            }
            av_frame_free(&input_frame);
        }

    }
    else if (pkt->stream_index == m_in.m_nVideo_stream_idx && m_out.m_nVideo_stream_idx > -1)
    {
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

        // Temporary storage of the input samples of the frame read from the file.
        AVFrame *input_frame = NULL;

        // Initialise temporary storage for one input frame.
        ret = init_frame(&input_frame, m_in.m_pszFileName);
        if (ret < 0)
        {
            return ret;
        }

        ret = avcodec_decode_video2(m_in.m_pVideo_codec_ctx, input_frame, &data_present, pkt);

        if (ret < 0 && ret != AVERROR(EINVAL))
        {
            ffmpegfs_error("Could not decode video frame (error '%s') for '%s'.", ffmpeg_geterror(ret).c_str(), m_in.m_pszFileName);
            // unused frame
            av_frame_free(&input_frame);
            return ret;
        }

        *decoded = ret;
        ret = 0;

        {

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
                    AVCodecContext *codec_ctx = m_out.m_pVideo_codec_ctx;

                    AVFrame * tmp_frame = alloc_picture(codec_ctx->pix_fmt, codec_ctx->width, codec_ctx->height);
                    if (!tmp_frame)
                    {
                        return AVERROR(ENOMEM);
                    }

                    sws_scale(m_pSws_ctx,
                              (const uint8_t * const *)input_frame->data, input_frame->linesize,
                              0, input_frame->height,
                              tmp_frame->data, tmp_frame->linesize);

                    tmp_frame->pts = input_frame->pts;
#ifndef USING_LIBAV
                    tmp_frame->best_effort_timestamp = input_frame->best_effort_timestamp;
#endif

                    av_frame_free(&input_frame);

                    input_frame = tmp_frame;
                }

#ifndef USING_LIBAV
#if LAVF_DEP_AVSTREAM_CODEC
                int64_t best_effort_timestamp = input_frame->best_effort_timestamp;
#else
                int64_t best_effort_timestamp = ::av_frame_get_best_effort_timestamp(input_frame);
#endif

                if (best_effort_timestamp != (int64_t)AV_NOPTS_VALUE)
                {
                    input_frame->pts = best_effort_timestamp;
                }
#endif

                if (input_frame->pts == (int64_t)AV_NOPTS_VALUE)
                {
                    input_frame->pts = m_pts;
                }

                // Rescale to our time base, but only of nessessary
                if (input_frame->pts != (int64_t)AV_NOPTS_VALUE && (m_in.m_pVideo_stream->time_base.den != m_out.m_pVideo_stream->time_base.den || m_in.m_pVideo_stream->time_base.num != m_out.m_pVideo_stream->time_base.num))
                {
                    input_frame->pts = av_rescale_q_rnd(input_frame->pts, m_in.m_pVideo_stream->time_base, m_out.m_pVideo_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
                }

                m_VideoFifo.push(input_frame);
            }
            else
            {
                // unused frame
                av_frame_free(&input_frame);
            }
        }
    }
    else
    {
        data_present = 0;
        *decoded = pkt->size;    // Ignore
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

    if (!(*converted_input_samples = (uint8_t **)calloc(m_out.m_pAudio_codec_ctx->channels, sizeof(**converted_input_samples))))
    {
        ffmpegfs_error("Could not allocate converted input sample pointers for '%s'.", m_in.m_pszFileName);
        return AVERROR(ENOMEM);
    }

    // Allocate memory for the samples of all channels in one consecutive
    // block for convenience.

    if ((ret = av_samples_alloc(*converted_input_samples, NULL,
                                m_out.m_pAudio_codec_ctx->channels,
                                frame_size,
                                m_out.m_pAudio_codec_ctx->sample_fmt, 0)) < 0)
    {
        ffmpegfs_error("Could not allocate converted input samples (error '%s') for '%s'.", ffmpeg_geterror(ret).c_str(), m_in.m_pszFileName);
        av_freep(&(*converted_input_samples)[0]);
        free(*converted_input_samples);
        return ret;
    }
    return 0;
}

// Convert the input audio samples into the output sample format.
// The conversion happens on a per-frame basis, the size of which is specified
// by frame_size.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations" // Will replace libavresample soon, cross my heart and hope to die...
int FFMPEG_Transcoder::convert_samples(uint8_t **input_data, const int in_samples, uint8_t **converted_data, int *out_samples)
{
    if (m_pAudio_resample_ctx != NULL)
    {
        int ret;

        // Convert the samples using the resampler.
        if ((ret = avresample_convert(m_pAudio_resample_ctx, converted_data, 0, *out_samples, input_data, 0, in_samples)) < 0)
        {
            ffmpegfs_error("Could not convert input samples (error '%s') for '%s'.", ffmpeg_geterror(ret).c_str(), m_in.m_pszFileName);
            return ret;
        }

        *out_samples = ret;

        // Perform a sanity check so that the number of converted samples is
        // not greater than the number of samples to be converted.
        // If the sample rates differ, this case has to be handled differently

        if (avresample_available(m_pAudio_resample_ctx))
        {
            ffmpegfs_error("Converted samples left over for '%s'.", m_in.m_pszFileName);
            return AVERROR_EXIT;
        }
    }
    else
    {
        // No resampling, just copy samples
        if (!av_sample_fmt_is_planar(m_in.m_pAudio_codec_ctx->sample_fmt))
        {
            memcpy(converted_data[0], input_data[0], in_samples * av_get_bytes_per_sample(m_out.m_pAudio_codec_ctx->sample_fmt) * m_in.m_pAudio_codec_ctx->channels);
        }
        else
        {
            for (int n = 0; n < m_in.m_pAudio_codec_ctx->channels; n++)
            {
                memcpy(converted_data[n], input_data[n], in_samples * av_get_bytes_per_sample(m_out.m_pAudio_codec_ctx->sample_fmt));
            }
        }
    }
    return 0;
}
#pragma GCC diagnostic pop

// Add converted input audio samples to the FIFO buffer for later processing.
int FFMPEG_Transcoder::add_samples_to_fifo(uint8_t **converted_input_samples, const int frame_size)
{
    int ret;

    // Make the FIFO as large as it needs to be to hold both,
    // the old and the new samples.

    if ((ret = av_audio_fifo_realloc(m_pAudioFifo, av_audio_fifo_size(m_pAudioFifo) + frame_size)) < 0)
    {
        ffmpegfs_error("Could not reallocate FIFO for '%s'.", m_in.m_pszFileName);
        return ret;
    }

    // Store the new samples in the FIFO buffer.
    ret = av_audio_fifo_write(m_pAudioFifo, (void **)converted_input_samples, frame_size);
    if (ret < frame_size)
    {
        if (ret < 0)
        {
            ffmpegfs_error("Could not write data to FIFO (error '%s') for '%s'.", ffmpeg_geterror(ret).c_str(), m_in.m_pszFileName);
        }
        else
        {
            ffmpegfs_error("Could not write data to FIFO for '%s'.", m_in.m_pszFileName);
            ret = AVERROR_EXIT;
        }
        return AVERROR_EXIT;
    }

    return 0;
}

int FFMPEG_Transcoder::decode_frame(AVPacket *pkt)
{
    int decoded = 0;
    int ret = 0;

    do
    {
        // Decode one frame.
        ret = decode_frame(pkt, &decoded);

        if (ret < 0)
        {
            ffmpegfs_error("Could not decode frame (error '%s') for '%s'.", ffmpeg_geterror(ret).c_str(), m_in.m_pszFileName);
            return ret;
        }

        pkt->data += decoded;
        pkt->size -= decoded;
    }
    while (pkt->size > 0);

    return 0;
}

// Flush the remaining frames
int FFMPEG_Transcoder::flush_input_frames(int stream_index)
{
    int ret = 0;

    if (stream_index > INVALID_STREAM)
    {
        AVPacket flush_packet;
        int decoded = 0;

        init_packet(&flush_packet);

        flush_packet.data = NULL;
        flush_packet.size = 0;
        flush_packet.stream_index = stream_index;

        do
        {
            ret = decode_frame(&flush_packet, &decoded);

            if (ret < 0)
            {
                break;
            }
        }
        while (decoded);

        av_packet_unref(&flush_packet);
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
                ffmpegfs_error("Could not read frame (error '%s') for '%s'.", ffmpeg_geterror(ret).c_str(), m_in.m_pszFileName);
                throw ret;
            }
        }

        if (!*finished)
        {
            // Decode one packet, at least with the old API (!LAV_NEW_PACKET_INTERFACE)
            // it seems a packet can contain more than one frame so loop around it
            // if necessary...
            ret = decode_frame(&pkt);

            if (ret < 0)
            {
                throw ret;
            }
        }
        else
        {
            // Flush cached frames, ignoring any errors
            if (m_in.m_pAudio_codec_ctx != NULL)
            {
                flush_input_frames(m_in.m_nAudio_stream_idx);
            }

            if (m_in.m_pVideo_codec_ctx != NULL)
            {
                flush_input_frames(m_in.m_nVideo_stream_idx);
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
        ffmpegfs_error("Could not allocate output frame for '%s'.", m_in.m_pszFileName);
        return AVERROR_EXIT;
    }

    //
    // Set the frame's parameters, especially its size and format.
    // av_frame_get_buffer needs this to allocate memory for the
    // audio samples of the frame.
    // Default channel layouts based on the number of channels
    // are assumed for simplicity.

    (*frame)->nb_samples     = frame_size;
    (*frame)->channel_layout = m_out.m_pAudio_codec_ctx->channel_layout;
    (*frame)->format         = m_out.m_pAudio_codec_ctx->sample_fmt;
    (*frame)->sample_rate    = m_out.m_pAudio_codec_ctx->sample_rate;

    // Allocate the samples of the created frame. This call will make
    // sure that the audio frame can hold as many samples as specified.

    if ((ret = av_frame_get_buffer(*frame, 0)) < 0)
    {
        ffmpegfs_error("Could allocate output frame samples (error '%s') for '%s'.", ffmpeg_geterror(ret).c_str(), m_in.m_pszFileName);
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
int FFMPEG_Transcoder::encode_audio_frame(AVFrame *frame, int *data_present)
{
    // Packet used for temporary storage.
    AVPacket pkt;
    int ret;

    init_packet(&pkt);

    // Encode the audio frame and store it in the temporary packet.
    // The output audio stream encoder is used to do this.
    ret = avcodec_encode_audio2(m_out.m_pAudio_codec_ctx, &pkt, frame, data_present);

    if (ret < 0)
    {
        ffmpegfs_error("Could not encode audio frame (error '%s') for '%s'.", ffmpeg_geterror(ret).c_str(), m_in.m_pszFileName);
        av_packet_unref(&pkt);
        return ret;
    }

    {
        // Write one audio frame from the temporary packet to the output file.
        if (*data_present)
        {
            pkt.stream_index = m_out.m_nAudio_stream_idx;

            produce_audio_dts(&pkt, &m_out.m_nAudio_pts);

            ret = av_interleaved_write_frame(m_out.m_pFormat_ctx, &pkt);

            if (ret < 0)
            {
                ffmpegfs_error("Could not write audio frame (error '%s') for '%s'.", ffmpeg_geterror(ret).c_str(), m_in.m_pszFileName);
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
#if (LIBAVCODEC_VERSION_MICRO >= 100 && LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 57, 64, 101 ) )

        //if (m_out.m_pVideo_codec_ctx->flags & (AV_CODEC_FLAG_INTERLACED_DCT | AV_CODEC_FLAG_INTERLACED_ME) && ost->top_field_first >= 0)
        //      frame->top_field_first = !!ost->top_field_first;

        if (frame->interlaced_frame)
        {
            if (m_out.m_pVideo_codec_ctx->codec->id == AV_CODEC_ID_MJPEG)
            {
                m_out.m_pVideo_stream->codecpar->field_order = frame->top_field_first ? AV_FIELD_TT:AV_FIELD_BB;
            }
            else
            {
                m_out.m_pVideo_stream->codecpar->field_order = frame->top_field_first ? AV_FIELD_TB:AV_FIELD_BT;
            }
        }
        else
        {
            m_out.m_pVideo_stream->codecpar->field_order = AV_FIELD_PROGRESSIVE;
        }
#endif

        frame->quality = m_out.m_pVideo_codec_ctx->global_quality;
#ifndef USING_LIBAV
        frame->pict_type = AV_PICTURE_TYPE_NONE;	// other than AV_PICTURE_TYPE_NONE causes warnings
#else
        frame->pict_type = (AVPictureType)0;        // other than 0 causes warnings
#endif
    }

    // Encode the video frame and store it in the temporary packet.
    // The output video stream encoder is used to do this.

    ret = avcodec_encode_video2(m_out.m_pVideo_codec_ctx, &pkt, frame, data_present);

    if (ret < 0)
    {
        ffmpegfs_error("Could not encode video frame (error '%s') for '%s'.", ffmpeg_geterror(ret).c_str(), m_in.m_pszFileName);
        av_packet_unref(&pkt);
        return ret;
    }

    {

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

                    ffmpegfs_warning("Invalid DTS: %" PRId64 " PTS: %" PRId64 " in video output, replacing by guess for '%s'.", pkt.dts, pkt.pts, m_in.m_pszFileName);

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
                        ffmpegfs_warning("Non-monotonous DTS in video output stream; previous: %" PRId64 ", current: %" PRId64 "; changing to %" PRId64 ". This may result in incorrect timestamps in the output for '%s'.", m_out.m_last_mux_dts, pkt.dts, max, m_in.m_pszFileName);

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
                ffmpegfs_error("Could not write video frame (error '%s') for '%s'.", ffmpeg_geterror(ret).c_str(), m_in.m_pszFileName);
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
            ffmpegfs_error("Could not read data from FIFO (error '%s') for '%s'.", ffmpeg_geterror(ret).c_str(), m_in.m_pszFileName);
        }
        else
        {
            ffmpegfs_error("Could not read data from FIFO for '%s'.", m_in.m_pszFileName);
            ret = AVERROR_EXIT;
        }
        av_frame_free(&output_frame);
        return ret;
    }

    // Encode one frame worth of audio samples.
    ret = encode_audio_frame(output_frame, &data_written);
    if (ret < 0)
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
        ffmpegfs_error("Could not write output file trailer (error '%s') for '%s'.", ffmpeg_geterror(ret).c_str(), m_in.m_pszFileName);
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
        av_dict_set_with_check(metadata_out, tag->key, tag->value, 0);

        if (m_out.m_output_type == TYPE_MP3)
        {
            // For MP3 fill in ID3v1 structure
            if (!strcasecmp(tag->key, "ARTIST"))
            {
                tagcpy(m_out.m_id3v1.m_sSongArtist, tag->value);
            }
            else if (!strcasecmp(tag->key, "TITLE"))
            {
                tagcpy(m_out.m_id3v1.m_sSongTitle, tag->value);
            }
            else if (!strcasecmp(tag->key, "ALBUM"))
            {
                tagcpy(m_out.m_id3v1.m_sAlbumName, tag->value);
            }
            else if (!strcasecmp(tag->key, "COMMENT"))
            {
                tagcpy(m_out.m_id3v1.m_sComment, tag->value);
            }
            else if (!strcasecmp(tag->key, "YEAR") || !strcasecmp(tag->key, "DATE"))
            {
                tagcpy(m_out.m_id3v1.m_sYear, tag->value);
            }
            else if (!strcasecmp(tag->key, "TRACK"))
            {
                m_out.m_id3v1.m_bTitleNo = (char)atoi(tag->value);
            }
        }
    }
}

int FFMPEG_Transcoder::process_metadata()
{
    ffmpegfs_trace("Processing metadata for '%s'.", m_in.m_pszFileName);

#if (LIBAVCODEC_VERSION_MICRO >= 100 && LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 57, 64, 101 ) )
    if (m_in.m_pAudio_stream != NULL && m_in.m_pAudio_stream->codecpar->codec_id == AV_CODEC_ID_VORBIS)
#else
    if (m_in.m_pAudio_stream != NULL && m_in.m_pAudio_stream->codec->codec_id == AV_CODEC_ID_VORBIS)
#endif
    {
        // For some formats (namely ogg) FFmpeg returns the tags, odd enough, with streams...
        copy_metadata(&m_out.m_pFormat_ctx->metadata, m_in.m_pAudio_stream->metadata);
    }

    copy_metadata(&m_out.m_pFormat_ctx->metadata, m_in.m_pFormat_ctx->metadata);

    if (m_out.m_pAudio_stream != NULL && m_in.m_pAudio_stream != NULL)
    {
        copy_metadata(&m_out.m_pAudio_stream->metadata, m_in.m_pAudio_stream->metadata);
    }

    if (m_out.m_pVideo_stream != NULL && m_in.m_pVideo_stream != NULL)
    {
        copy_metadata(&m_out.m_pVideo_stream->metadata, m_in.m_pVideo_stream->metadata);
    }

    // Pictures later. More complicated...

    return 0;
}

// Process a single frame of audio data. The encode_pcm_data() method
// of the Encoder will be used to process the resulting audio data, with the
// result going into the given Buffer.
//
// Returns:
//  0   if decoding was OK
//  1   if EOF reached
//  -1  error

int FFMPEG_Transcoder::process_single_fr()
{
    int finished = 0;
    int ret = 0;

    if (m_out.m_nAudio_stream_idx > -1)
    {
        int output_frame_size;

        if (m_out.m_pAudio_codec_ctx->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
        {
            // Encode supports variable frame size, use an arbitrary value
            output_frame_size =  10000;
        }
        else
        {
            // Use the encoder's desired frame size for processing.
            output_frame_size = m_out.m_pAudio_codec_ctx->frame_size;
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
                goto cleanup;
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
                goto cleanup;
            }
        }

        // If we are at the end of the input file and have encoded
        // all remaining samples, we can exit this loop and finish.

        if (finished)
        {
            if (m_out.m_pAudio_codec_ctx != NULL)
            {
                // Flush the encoder as it may have delayed frames.
                int data_written = 0;
                do
                {
                    ret = encode_audio_frame(NULL, &data_written);
                    if (ret < 0)
                    {
                        goto cleanup;
                    }
                }
                while (data_written);
            }

            ret = 1;
        }
    }
    else
    {
        ret = read_decode_convert_and_store(&finished);
        if (ret < 0)
        {
            goto cleanup;
        }

        if (finished)
        {
            ret = 1;
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
        if (ret < 0)
        {
            av_frame_free(&output_frame);
            goto cleanup;
        }
        av_frame_free(&output_frame);
    }

    // If we are at the end of the input file and have encoded
    // all remaining samples, we can exit this loop and finish.

    if (finished)
    {
        if (m_out.m_pVideo_codec_ctx != NULL)
        {
            // Flush the encoder as it may have delayed frames.
            int data_written = 0;
            do
            {
                ret = encode_video_frame(NULL, &data_written);
                if (ret < 0)
                {
                    goto cleanup;
                }
            }
            while (data_written);
        }

        ret = 1;
    }

    return ret;

cleanup:
    return -1;
}

// Try to predict final file size.

size_t FFMPEG_Transcoder::calculate_size()
{
    if (m_nCalculated_size == 0 && m_in.m_pFormat_ctx != NULL)
    {
        double duration = ffmpeg_cvttime(m_in.m_pFormat_ctx->duration, AV_TIME_BASE_Q);
        AVCodecID audio_codec_id = AV_CODEC_ID_NONE;
        AVCodecID video_codec_id = AV_CODEC_ID_NONE;
        const char *    format;
        size_t size = 0;

        format = get_codecs(params.m_desttype, &m_out.m_output_type, &audio_codec_id, &video_codec_id);

        if (format == NULL)
        {
            ffmpegfs_error("Unknown format type '%s' for '%s'.", params.m_desttype, m_in.m_pszFileName);
            return 0;
        }

        if (m_in.m_nAudio_stream_idx > -1)
        {
#if !defined(USING_LIBAV) && (LIBAVUTIL_VERSION_MAJOR > 54)
            int64_t audiobitrate = m_in.m_pAudio_codec_ctx->bit_rate;
#else // USING_LIBAV
            int audiobitrate = m_in.m_pAudio_codec_ctx->bit_rate;
#endif

            get_output_bit_rate(m_in.m_pAudio_stream, params.m_audiobitrate, &audiobitrate);

            switch (audio_codec_id)
            {
            case AV_CODEC_ID_AAC:
            {
                // Try to predict the size of the AAC stream (this is fairly accurate, sometimes a bit larger, sometimes a bit too small
                size += (size_t)(duration * 1.025 * (double)audiobitrate / 8); // add 2.5% for overhead
                break;
            }
            case AV_CODEC_ID_MP3:
            {
                // Kbps = bits per second / 8 = Bytes per second x 60 seconds = Bytes per minute x 60 minutes = Bytes per hour
                // This is the sum of the size of
                // ID3v2, ID3v1, and raw MP3 data. This is theoretically only approximate
                // but in practice gives excellent answers, usually exactly correct.
                // Cast to 64-bit int to avoid overflow.

                size += (size_t)(duration * (double)audiobitrate / 8) + ID3V1_TAG_LENGTH;
                break;
            }
            case AV_CODEC_ID_PCM_S16LE:
            {
                int nChannels = 2; //m_in.m_pAudio_codec_ctx->channels;
                int nBytesPerSample =  2; // av_get_bytes_per_sample(m_in.m_pAudio_codec_ctx->sample_fmt);
                int nSampleRate = m_in.m_pAudio_codec_ctx->sample_rate;

                get_output_sample_rate(m_in.m_pAudio_stream, params.m_audiosamplerate, &nSampleRate);

                // File size:
                // file duration * sample rate (HZ) * channels * bytes per sample
                size += (size_t)(duration * nSampleRate * nChannels * nBytesPerSample);
                break;
            }
            case AV_CODEC_ID_VORBIS:
            {
                // Kbps = bits per second / 8 = Bytes per second x 60 seconds = Bytes per minute x 60 minutes = Bytes per hour
                size += (size_t)(duration * (double)audiobitrate / 8) /*+ ID3V1_TAG_LENGTH*/;// TODO ???
                break;
            }
            case AV_CODEC_ID_NONE:
            {
                break;
            }
            default:
            {
                ffmpegfs_error("Internal error - unsupported audio codec '%s' for format %s.", get_codec_name(audio_codec_id), params.m_desttype);
                break;
            }
            }
        }

        if (m_in.m_nVideo_stream_idx > -1)
        {
            if (m_bIsVideo)
            {
#if !defined(USING_LIBAV) && (LIBAVUTIL_VERSION_MAJOR > 54)
                int64_t videobitrate = m_in.m_pVideo_codec_ctx->bit_rate;
#else // USING_LIBAV
                int videobitrate = m_in.m_pVideo_codec_ctx->bit_rate;
#endif
                int bitrateoverhead = 0;

                get_output_bit_rate(m_in.m_pVideo_stream, params.m_videobitrate, &videobitrate);

                videobitrate += bitrateoverhead;

                switch (video_codec_id)
                {
                case AV_CODEC_ID_H264:
                {
                    size += (size_t)(duration * 1.025  * (double)videobitrate / 8); // add 2.5% for overhead
                    break;
                }
                case AV_CODEC_ID_MJPEG:
                {
                    // TODO... size += ???
                    break;
                }
                case AV_CODEC_ID_THEORA:
                {
                    size += (size_t)(duration * 1.025  * (double)videobitrate / 8); // ??? // add 2.5% for overhead
                    break;
                }
                case AV_CODEC_ID_NONE:
                {
                    break;
                }
                default:
                {
                    ffmpegfs_warning("Unsupported video codec '%s' for format %s.", get_codec_name(video_codec_id), params.m_desttype);
                    break;
                }
                }
            }
            // else      // TODO #2260: Add picture size
            // {

            // }
        }

        m_nCalculated_size = size;
    }

    return m_nCalculated_size;
}

// Encode any remaining PCM data in LAME internal buffers to the given
// Buffer. This should be called after all input data has already been
// passed to encode_pcm_data().

int FFMPEG_Transcoder::encode_finish()
{
    int ret = 0;

    // Write the trailer of the output file container.
    ret = write_output_file_trailer();
    if (ret < 0)
    {
        ffmpegfs_error("Error writing trailer (error '%s') for '%s'.", ffmpeg_geterror(ret).c_str(), m_in.m_pszFileName);
    }

    return ret;
}

const ID3v1 * FFMPEG_Transcoder::id3v1tag() const
{
    return &m_out.m_id3v1;
}

int FFMPEG_Transcoder::writePacket(void * pOpaque, unsigned char * pBuffer, int nBufSize)
{
    Buffer * buffer = (Buffer *)pOpaque;

    if (buffer == NULL)
    {
        return -1;
    }

    return (int)buffer->write((const uint8_t*)pBuffer, nBufSize);
}

int64_t FFMPEG_Transcoder::seek(void * pOpaque, int64_t i4Offset, int nWhence)
{
    Buffer * buffer = (Buffer *)pOpaque;
    int64_t i64ResOffset = 0;

    if (buffer != NULL)
    {
        if (nWhence & AVSEEK_SIZE)
        {
            i64ResOffset = buffer->tell();
        }
        else
        {
            nWhence &= ~(AVSEEK_SIZE | AVSEEK_FORCE);

            switch (nWhence)
            {
            case SEEK_CUR:
            {
                i4Offset = buffer->tell() + i4Offset;
                break;
            }

            case SEEK_END:
            {
                i4Offset = buffer->size() - i4Offset;
                break;
            }

            case SEEK_SET:   // SEEK_SET only supported
            {
                break;
            }
            }

            if (i4Offset < 0)
            {
                i4Offset = 0;
            }

            if (buffer->seek(i4Offset))
            {
                i64ResOffset = i4Offset;
            }

            else
            {
                i64ResOffset = 0;
            }
        }
    }

    return i64ResOffset;
}

// Close the open FFmpeg file
void FFMPEG_Transcoder::close()
{
    int nAudioSamplesLeft = 0;
    size_t nVideoFramesLeft = 0;
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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations" // Will replace libavresample soon, cross my heart and hope to die...
        avresample_close(m_pAudio_resample_ctx);
        avresample_free(&m_pAudio_resample_ctx);
#pragma GCC diagnostic pop
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
#if (AV_VERSION_MAJOR < 57)
    if (m_out.m_pAudio_codec_ctx)
    {
        avcodec_close(m_out.m_pAudio_codec_ctx);
        m_out.m_pAudio_codec_ctx = NULL;
        bClosed = true;
    }

    if (m_out.m_pVideo_codec_ctx)
    {
        avcodec_close(m_out.m_pVideo_codec_ctx);
        m_out.m_pVideo_codec_ctx = NULL;
        bClosed = true;
    }
#else
    if (m_out.m_pAudio_codec_ctx)
    {
        avcodec_free_context(&m_out.m_pAudio_codec_ctx);
        m_out.m_pAudio_codec_ctx = NULL;
        bClosed = true;
    }

    if (m_out.m_pVideo_codec_ctx)
    {
        avcodec_free_context(&m_out.m_pVideo_codec_ctx);
        m_out.m_pVideo_codec_ctx = NULL;
        bClosed = true;
    }
#endif

    if (m_out.m_pFormat_ctx != NULL)
    {
        AVIOContext *output_io_context  = (AVIOContext *)m_out.m_pFormat_ctx->pb;

#if (AV_VERSION_MAJOR >= 57)
        if (output_io_context != NULL)
        {
            av_freep(&output_io_context->buffer);
        }
#endif
        //        if (!(m_out.m_pFormat_ctx->oformat->flags & AVFMT_NOFILE))
        {
            av_freep(&output_io_context);
        }

        avformat_free_context(m_out.m_pFormat_ctx);
        m_out.m_pFormat_ctx = NULL;
        bClosed = true;
    }

    // Close input file
#if (AV_VERSION_MAJOR < 57)
    if (m_in.m_pAudio_codec_ctx)
    {
        avcodec_close(m_in.m_pAudio_codec_ctx);
        m_in.m_pAudio_codec_ctx = NULL;
        bClosed = true;
    }

    if (m_in.m_pVideo_codec_ctx)
    {
        avcodec_close(m_in.m_pVideo_codec_ctx);
        m_in.m_pVideo_codec_ctx = NULL;
        bClosed = true;
    }
#else
    if (m_in.m_pAudio_codec_ctx)
    {
        avcodec_free_context(&m_in.m_pAudio_codec_ctx);
        m_in.m_pAudio_codec_ctx = NULL;
        bClosed = true;
    }

    if (m_in.m_pVideo_codec_ctx)
    {
        avcodec_free_context(&m_in.m_pVideo_codec_ctx);
        m_in.m_pVideo_codec_ctx = NULL;
        bClosed = true;
    }
#endif

    if (m_in.m_pFormat_ctx != NULL)
    {
        avformat_close_input(&m_in.m_pFormat_ctx);
        m_in.m_pFormat_ctx = NULL;
        bClosed = true;
    }

    if (bClosed)
    {
        if (nAudioSamplesLeft)
        {
            ffmpegfs_warning("%i audio samples left in buffer and not written to target file!", nAudioSamplesLeft);
        }

        if (nVideoFramesLeft)
        {
            ffmpegfs_warning("%zu video frames left in buffer and not written to target file!", nVideoFramesLeft);
        }

        // Closed anything...
        ffmpegfs_debug("FFmpeg trancoder: closed.");
    }
}

int FFMPEG_Transcoder::av_dict_set_with_check(AVDictionary **pm, const char *key, const char *value, int flags)
{
    int ret = ::av_dict_set(pm, key, value, flags);

    if (ret < 0)
    {
        ffmpegfs_error("Error setting dictionary option key(%s)='%s' (error '%s') for '%s'.", key, value, ffmpeg_geterror(ret).c_str(), m_in.m_pszFileName);
    }

    return ret;
}
