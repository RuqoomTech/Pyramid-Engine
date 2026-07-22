#include "Pyramid/Util/JPEGLoader.hpp"

#include "Pyramid/Util/Image.hpp"
#include "Pyramid/Util/Log.hpp"

#include <jpeglib.h>

#include <climits>
#include <csetjmp>
#include <cstdio>
#include <fstream>
#include <limits>
#include <new>
#include <vector>

namespace Pyramid::Util
{
    namespace
    {
        struct JPEGErrorManager
        {
            jpeg_error_mgr base;
            std::jmp_buf jumpBuffer;
            char message[JMSG_LENGTH_MAX] = {};
        };

        void HandleJPEGError(j_common_ptr info)
        {
            auto* error = reinterpret_cast<JPEGErrorManager*>(info->err);
            (*info->err->format_message)(info, error->message);
            std::longjmp(error->jumpBuffer, 1);
        }

        constexpr std::size_t MaxDecodedImageBytes = 512ULL * 1024ULL * 1024ULL;

        bool IsSafeImageSize(JDIMENSION width, JDIMENSION height, int channels, std::size_t& byteCount)
        {
            if (width == 0 || height == 0 || channels <= 0)
            {
                return false;
            }

            const std::size_t widthValue = static_cast<std::size_t>(width);
            const std::size_t heightValue = static_cast<std::size_t>(height);
            const std::size_t channelValue = static_cast<std::size_t>(channels);

            if (widthValue > std::numeric_limits<std::size_t>::max() / channelValue)
            {
                return false;
            }

            const std::size_t rowBytes = widthValue * channelValue;
            if (heightValue > std::numeric_limits<std::size_t>::max() / rowBytes)
            {
                return false;
            }

            byteCount = rowBytes * heightValue;
            return byteCount <= MaxDecodedImageBytes;
        }
    } // namespace

    thread_local std::string JPEGLoader::s_LastError;

    ImageData JPEGLoader::LoadFromFile(const std::string& filepath)
    {
        s_LastError.clear();

        std::ifstream file(filepath, std::ios::binary | std::ios::ate);
        if (!file)
        {
            SetError("Failed to open JPEG file: " + filepath);
            return {};
        }

        const std::streampos end = file.tellg();
        if (end <= 0)
        {
            SetError("JPEG file is empty: " + filepath);
            return {};
        }

        const auto fileSize = static_cast<std::size_t>(end);
        if (fileSize > static_cast<std::size_t>(ULONG_MAX) || fileSize > MaxDecodedImageBytes)
        {
            SetError("JPEG file is too large to decode: " + filepath);
            return {};
        }

        std::vector<std::uint8_t> bytes(fileSize);
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));

        if (!file)
        {
            SetError("Failed to read JPEG file: " + filepath);
            return {};
        }

        ImageData result = Decode(bytes.data(), bytes.size());
        if (result.Data)
        {
            PYRAMID_LOG_INFO(
                "Loaded JPEG image: ", filepath, " (", result.Width, "x", result.Height,
                ", ", result.Channels, " channels)");
        }
        return result;
    }

    ImageData JPEGLoader::LoadFromMemory(const std::uint8_t* data, std::size_t size)
    {
        s_LastError.clear();
        return Decode(data, size);
    }

    const std::string& JPEGLoader::GetLastError()
    {
        return s_LastError;
    }

    ImageData JPEGLoader::Decode(const std::uint8_t* data, std::size_t size)
    {
        if (!data || size == 0)
        {
            SetError("Invalid or empty JPEG data");
            return {};
        }

        if (size > static_cast<std::size_t>(ULONG_MAX))
        {
            SetError("JPEG memory buffer is too large to decode");
            return {};
        }

        jpeg_decompress_struct decoder = {};
        JPEGErrorManager error = {};
        unsigned char* volatile output = nullptr;
        volatile bool decoderCreated = false;

        decoder.err = jpeg_std_error(&error.base);
        error.base.error_exit = HandleJPEGError;

        if (setjmp(error.jumpBuffer) != 0)
        {
            if (output)
            {
                delete[] const_cast<unsigned char*>(output);
            }
            if (decoderCreated)
            {
                jpeg_destroy_decompress(&decoder);
            }

            SetError(error.message[0] ? error.message : "libjpeg failed to decode the image");
            return {};
        }

        jpeg_create_decompress(&decoder);
        decoderCreated = true;
        jpeg_mem_src(
            &decoder,
            const_cast<unsigned char*>(data),
            static_cast<unsigned long>(size));

        const int headerStatus = jpeg_read_header(&decoder, TRUE);
        if (headerStatus != JPEG_HEADER_OK)
        {
            jpeg_destroy_decompress(&decoder);
            SetError("Invalid JPEG header");
            return {};
        }

        // Normalize grayscale and YCbCr inputs to an RGB texture representation.
        // libjpeg performs the required color conversion.
        decoder.out_color_space = JCS_RGB;

        if (!jpeg_start_decompress(&decoder))
        {
            jpeg_destroy_decompress(&decoder);
            SetError("Failed to start JPEG decompression");
            return {};
        }

        std::size_t byteCount = 0;
        if (decoder.output_components != 3 ||
            !IsSafeImageSize(decoder.output_width, decoder.output_height, 3, byteCount))
        {
            jpeg_abort_decompress(&decoder);
            jpeg_destroy_decompress(&decoder);
            SetError("JPEG dimensions or output format are unsupported");
            return {};
        }

        output = new (std::nothrow) unsigned char[byteCount];
        if (!output)
        {
            jpeg_abort_decompress(&decoder);
            jpeg_destroy_decompress(&decoder);
            SetError("Failed to allocate memory for decoded JPEG pixels");
            return {};
        }

        const std::size_t rowStride = static_cast<std::size_t>(decoder.output_width) * 3U;
        while (decoder.output_scanline < decoder.output_height)
        {
            JSAMPROW row = const_cast<unsigned char*>(output) +
                static_cast<std::size_t>(decoder.output_scanline) * rowStride;
            if (jpeg_read_scanlines(&decoder, &row, 1) != 1)
            {
                delete[] const_cast<unsigned char*>(output);
                output = nullptr;
                jpeg_abort_decompress(&decoder);
                jpeg_destroy_decompress(&decoder);
                SetError("JPEG scanline decoding stopped unexpectedly");
                return {};
            }
        }

        const int width = static_cast<int>(decoder.output_width);
        const int height = static_cast<int>(decoder.output_height);

        if (!jpeg_finish_decompress(&decoder))
        {
            delete[] const_cast<unsigned char*>(output);
            output = nullptr;
            jpeg_destroy_decompress(&decoder);
            SetError("Failed to finish JPEG decompression");
            return {};
        }

        jpeg_destroy_decompress(&decoder);

        ImageData result;
        result.Data = const_cast<unsigned char*>(output);
        result.Width = width;
        result.Height = height;
        result.Channels = 3;
        return result;
    }

    void JPEGLoader::SetError(const std::string& error)
    {
        s_LastError = error;
        PYRAMID_LOG_ERROR("JPEGLoader: ", error);
    }
} // namespace Pyramid::Util
