#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace Pyramid::Util
{
    struct ImageData;

    /**
     * Decode JPEG images through the platform libjpeg implementation.
     *
     * The loader always returns tightly packed 8-bit RGB pixels. Both baseline
     * and progressive JPEG streams supported by libjpeg are accepted. Memory
     * returned in ImageData is owned by Pyramid and must be released with
     * Image::Free().
     */
    class JPEGLoader
    {
    public:
        static ImageData LoadFromFile(const std::string& filepath);
        static ImageData LoadFromMemory(const std::uint8_t* data, std::size_t size);
        static const std::string& GetLastError();

    private:
        static ImageData Decode(const std::uint8_t* data, std::size_t size);
        static void SetError(const std::string& error);

        static thread_local std::string s_LastError;
    };
} // namespace Pyramid::Util
