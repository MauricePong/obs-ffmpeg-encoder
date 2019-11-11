// FFMPEG Video Encoder Integration for OBS Studio
// Copyright (c) 2019 Michael Fabian Dirks <info@xaymar.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "tools.hpp"
#include <list>
#include <map>
#include <sstream>
#include <stdexcept>

extern "C" {
#pragma warning(push)
#pragma warning(disable : 4244)
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libavutil/pixdesc.h>
#pragma warning(pop)
}

std::string ffmpeg::tools::translate_encoder_capabilities(int capabilities)
{
	// Sorted by relative importance.
	std::pair<int, std::string> caps[] = {
	    {AV_CODEC_CAP_EXPERIMENTAL, "Experimental"},

	    // Quality
	    {AV_CODEC_CAP_LOSSLESS, "Lossless"},

	    // Features
	    {AV_CODEC_CAP_PARAM_CHANGE, "Dynamic Parameter Change"},
	    {AV_CODEC_CAP_SUBFRAMES, "Sub-Frames"},
	    {AV_CODEC_CAP_VARIABLE_FRAME_SIZE, "Variable Frame Size"},
	    {AV_CODEC_CAP_SMALL_LAST_FRAME, "Small Final Frame"},

	    // Other
	    {AV_CODEC_CAP_TRUNCATED, "Truncated"},
	    {AV_CODEC_CAP_CHANNEL_CONF, "AV_CODEC_CAP_CHANNEL_CONF"},
	    {AV_CODEC_CAP_DRAW_HORIZ_BAND, "AV_CODEC_CAP_DRAW_HORIZ_BAND"},
	    {AV_CODEC_CAP_AVOID_PROBING, "AV_CODEC_CAP_AVOID_PROBING"},
	};

	std::stringstream sstr;
	for (auto const kv : caps) {
		if (capabilities & kv.first) {
			capabilities &= ~kv.first;
			sstr << kv.second;
			if (capabilities != 0) {
				sstr << ", ";
			} else {
				break;
			}
		}
	}

	return sstr.str();
}

const char* ffmpeg::tools::get_pixel_format_name(AVPixelFormat v)
{
	return av_get_pix_fmt_name(v);
}

const char* ffmpeg::tools::get_color_space_name(AVColorSpace v)
{
	switch (v) {
	case AVCOL_SPC_RGB:
		return "RGB";
	case AVCOL_SPC_BT709:
		return "BT.709";
	case AVCOL_SPC_FCC:
		return "FCC Title 47 CoFR 73.682 (a)(20)";
	case AVCOL_SPC_BT470BG:
		return "BT.601 625";
	case AVCOL_SPC_SMPTE170M:
	case AVCOL_SPC_SMPTE240M:
		return "BT.601 525";
	case AVCOL_SPC_YCGCO:
		return "ITU-T SG16";
	case AVCOL_SPC_BT2020_NCL:
		return "BT.2020 NCL";
	case AVCOL_SPC_BT2020_CL:
		return "BT.2020 CL";
	case AVCOL_SPC_SMPTE2085:
		return "SMPTE 2085";
	case AVCOL_SPC_CHROMA_DERIVED_NCL:
		return "Chroma NCL";
	case AVCOL_SPC_CHROMA_DERIVED_CL:
		return "Chroma CL";
	case AVCOL_SPC_ICTCP:
		return "BT.2100";
	case AVCOL_SPC_NB:
		return "Not Part of ABI";
	}
	return "Unknown";
}

const char* ffmpeg::tools::get_error_description(int error)
{
	thread_local char error_buf[AV_ERROR_MAX_STRING_SIZE + 1];
	if (av_strerror(error, error_buf, AV_ERROR_MAX_STRING_SIZE) < 0) {
		snprintf(error_buf, AV_ERROR_MAX_STRING_SIZE, "Unknown Error (%i)", error);
	}
	return error_buf;
}

static std::map<video_format, AVPixelFormat> obs_to_av_format_map = {
    {VIDEO_FORMAT_I420, AV_PIX_FMT_YUV420P},  // YUV 4:2:0
    {VIDEO_FORMAT_NV12, AV_PIX_FMT_NV12},     // NV12 Packed YUV
    {VIDEO_FORMAT_YVYU, AV_PIX_FMT_YVYU422},  // YVYU Packed YUV
    {VIDEO_FORMAT_YUY2, AV_PIX_FMT_YUYV422},  // YUYV Packed YUV
    {VIDEO_FORMAT_UYVY, AV_PIX_FMT_UYVY422},  // UYVY Packed YUV
    {VIDEO_FORMAT_RGBA, AV_PIX_FMT_RGBA},     //
    {VIDEO_FORMAT_BGRA, AV_PIX_FMT_BGRA},     //
    {VIDEO_FORMAT_BGRX, AV_PIX_FMT_BGR0},     //
    {VIDEO_FORMAT_Y800, AV_PIX_FMT_GRAY8},    //
    {VIDEO_FORMAT_I444, AV_PIX_FMT_YUV444P},  //
    {VIDEO_FORMAT_BGR3, AV_PIX_FMT_BGR24},    //
    {VIDEO_FORMAT_I422, AV_PIX_FMT_YUV422P},  //
    {VIDEO_FORMAT_I40A, AV_PIX_FMT_YUVA420P}, //
    {VIDEO_FORMAT_I42A, AV_PIX_FMT_YUVA422P}, //
    {VIDEO_FORMAT_YUVA, AV_PIX_FMT_YUVA444P}, //
                                              //{VIDEO_FORMAT_AYUV, AV_PIX_FMT_AYUV444P}, //
};

AVPixelFormat ffmpeg::tools::obs_videoformat_to_avpixelformat(video_format v)
{
	auto found = obs_to_av_format_map.find(v);
	if (found != obs_to_av_format_map.end()) {
		return found->second;
	}
	return AV_PIX_FMT_NONE;
}

video_format ffmpeg::tools::avpixelformat_to_obs_videoformat(AVPixelFormat v)
{
	for (const auto& kv : obs_to_av_format_map) {
		if (kv.second == v)
			return kv.first;
	}
	return VIDEO_FORMAT_NONE;
}

AVPixelFormat ffmpeg::tools::get_least_lossy_format(const AVPixelFormat* haystack, AVPixelFormat needle)
{
	int data_loss = 0;
	return avcodec_find_best_pix_fmt_of_list(haystack, needle, 0, &data_loss);
}

AVColorSpace ffmpeg::tools::obs_videocolorspace_to_avcolorspace(video_colorspace v)
{
	switch (v) {
	case VIDEO_CS_DEFAULT:
	case VIDEO_CS_709:
		return AVCOL_SPC_BT709;
	case VIDEO_CS_601:
		return AVCOL_SPC_BT470BG;
	}
	throw std::invalid_argument("unknown color space");
}

AVColorRange ffmpeg::tools::obs_videorangetype_to_avcolorrange(video_range_type v)
{
	switch (v) {
	case VIDEO_RANGE_DEFAULT:
	case VIDEO_RANGE_PARTIAL:
		return AVCOL_RANGE_MPEG;
	case VIDEO_RANGE_FULL:
		return AVCOL_RANGE_JPEG;
	}
	throw std::invalid_argument("unknown range");
}

bool ffmpeg::tools::can_hardware_encode(const AVCodec* codec)
{
	AVPixelFormat hardware_formats[] = {AV_PIX_FMT_D3D11};

	for (const AVPixelFormat* fmt = codec->pix_fmts; (fmt != nullptr) && (*fmt != AV_PIX_FMT_NONE); fmt++) {
		for (auto cmp : hardware_formats) {
			if (*fmt == cmp) {
				return true;
			}
		}
	}
	return false;
}

std::vector<AVPixelFormat> ffmpeg::tools::get_software_formats(const AVPixelFormat* list)
{
	AVPixelFormat hardware_formats[] = {
#if FF_API_VAAPI
		AV_PIX_FMT_VAAPI_MOCO,
		AV_PIX_FMT_VAAPI_IDCT,
#endif
		AV_PIX_FMT_VAAPI,
		AV_PIX_FMT_DXVA2_VLD,
		AV_PIX_FMT_VDPAU,
		AV_PIX_FMT_QSV,
		AV_PIX_FMT_MMAL,
		AV_PIX_FMT_D3D11VA_VLD,
		AV_PIX_FMT_CUDA,
		AV_PIX_FMT_XVMC,
		AV_PIX_FMT_VIDEOTOOLBOX,
		AV_PIX_FMT_MEDIACODEC,
		AV_PIX_FMT_D3D11,
		AV_PIX_FMT_OPENCL,
	};

	std::vector<AVPixelFormat> fmts;
	for (auto fmt = list; fmt && (*fmt != AV_PIX_FMT_NONE); fmt++) {
		bool is_blacklisted = false;
		for (auto blacklisted : hardware_formats) {
			if (*fmt == blacklisted)
				is_blacklisted = true;
		}
		if (!is_blacklisted)
			fmts.push_back(*fmt);
	}

	fmts.push_back(AV_PIX_FMT_NONE);

	return std::move(fmts);
}

void ffmpeg::tools::setup_obs_color(video_colorspace colorspace, video_range_type range, AVCodecContext* context)
{
	std::map<video_colorspace, std::tuple<AVColorSpace, AVColorPrimaries, AVColorTransferCharacteristic>>
	    colorspaces = {
	        {VIDEO_CS_DEFAULT, {AVCOL_SPC_BT470BG, AVCOL_PRI_BT470BG, AVCOL_TRC_SMPTE170M}},
	        {VIDEO_CS_601, {AVCOL_SPC_BT470BG, AVCOL_PRI_BT470BG, AVCOL_TRC_SMPTE170M}},
	        {VIDEO_CS_709, {AVCOL_SPC_BT709, AVCOL_PRI_BT709, AVCOL_TRC_BT709}},
	    };
	std::map<video_range_type, AVColorRange> colorranges = {
	    {VIDEO_RANGE_DEFAULT, AVCOL_RANGE_MPEG},
	    {VIDEO_RANGE_PARTIAL, AVCOL_RANGE_MPEG},
	    {VIDEO_RANGE_FULL, AVCOL_RANGE_JPEG},
	};

	{
		auto found = colorspaces.find(colorspace);
		if (found != colorspaces.end()) {
			context->colorspace      = std::get<AVColorSpace>(found->second);
			context->color_primaries = std::get<AVColorPrimaries>(found->second);
			context->color_trc       = std::get<AVColorTransferCharacteristic>(found->second);
		}
	}
	{
		auto found = colorranges.find(range);
		if (found != colorranges.end()) {
			context->color_range = found->second;
		}
	}

	// Downscaling should result in downscaling, not pixelation
	context->chroma_sample_location = AVCHROMA_LOC_CENTER;
}
