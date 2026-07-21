#include <cstring>
#include "Pyramid/Util/Image.hpp"
#include "Pyramid/Util/PNGLoader.hpp"
#include "Pyramid/Util/JPEGLoader.hpp"
#include "Pyramid/Util/Log.hpp"
#include <fstream>
#include <vector>
#include <algorithm>
#include <cctype>

namespace Pyramid
{
    namespace Util
    {

// TGA file header structure.
// #pragma pack(push, 1) ensures the compiler doesn't add padding bytes,
// so the struct's memory layout matches the file format exactly.
#pragma pack(push, 1)
        struct TGAHeader
        {
            uint8_t idLength;
            uint8_t colorMapType;
            uint8_t imageType;
            uint8_t colorMapSpec[5];
            uint16_t xOrigin;
            uint16_t yOrigin;
            uint16_t width;
            uint16_t height;
            uint8_t bpp; // bits per pixel
            uint8_t imageDescriptor;
        };

        // BMP file header structures
        struct BMPFileHeader
        {
            uint16_t signature;  // "BM" (0x4D42)
            uint32_t fileSize;   // Size of the BMP file
            uint16_t reserved1;  // Reserved, must be 0
            uint16_t reserved2;  // Reserved, must be 0
            uint32_t dataOffset; // Offset to the pixel data
        };

        struct BMPInfoHeader
        {
            uint32_t headerSize;      // Size of this header (40 bytes for BITMAPINFOHEADER)
            int32_t width;            // Width of the image
            int32_t height;           // Height of the image (positive = bottom-up, negative = top-down)
            uint16_t planes;          // Number of color planes (must be 1)
            uint16_t bitsPerPixel;    // Bits per pixel (24 or 32)
            uint32_t compression;     // Compression type (0 = uncompressed)
            uint32_t imageSize;       // Size of the image data (can be 0 for uncompressed)
            int32_t xPixelsPerMeter;  // Horizontal resolution
            int32_t yPixelsPerMeter;  // Vertical resolution
            uint32_t colorsUsed;      // Number of colors in the palette
            uint32_t colorsImportant; // Number of important colors
        };
#pragma pack(pop)

        // Helper function to get file extension in lowercase
        std::string GetFileExtension(const std::string &filepath)
        {
            size_t dotPos = filepath.find_last_of('.');
            if (dotPos == std::string::npos)
                return "";

            std::string ext = filepath.substr(dotPos + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            return ext;
        }

        // Forward declarations for helper functions
        ImageData LoadTGAFromFile(const std::string &filepath);
        ImageData LoadBMPFromFile(const std::string &filepath);
        ImageData LoadPNGFromFile(const std::string &filepath);
        ImageData LoadJPEGFromFile(const std::string &filepath);

        ImageData Image::Load(const std::string &filepath)
        {
            // Determine file format based on extension
            std::string extension = GetFileExtension(filepath);

            if (extension == "tga")
            {
                return LoadTGAFromFile(filepath);
            }
            else if (extension == "bmp")
            {
                return LoadBMPFromFile(filepath);
            }
            else if (extension == "png")
            {
                return LoadPNGFromFile(filepath);
            }
            else if (extension == "jpg" || extension == "jpeg")
            {
                return LoadJPEGFromFile(filepath);
            }
            else
            {
                ImageData result;
                PYRAMID_LOG_ERROR("Unsupported image format: ", extension, ". Supported formats: TGA, BMP, PNG, JPEG. File: ", filepath);
                return result;
            }
        }

        ImageData LoadTGAFromFile(const std::string &filepath)
        {
            ImageData result;
            std::ifstream file(filepath, std::ios::binary);

            if (!file.is_open())
            {
                PYRAMID_LOG_ERROR("Failed to open TGA file: ", filepath);
                return result;
            }

            TGAHeader header;
            file.read(reinterpret_cast<char *>(&header), sizeof(header));

            if (!file)
            {
                PYRAMID_LOG_ERROR("Failed to read TGA header from: ", filepath);
                return result;
            }

            // We only support uncompressed true-color images (type 2)
            if (header.imageType != 2)
            {
                PYRAMID_LOG_ERROR("Unsupported TGA image type: ", (int)header.imageType, ". Only type 2 is supported. File: ", filepath);
                return result;
            }

            // We only support 24 (RGB) and 32 (RGBA) bits per pixel
            if (header.bpp != 24 && header.bpp != 32)
            {
                PYRAMID_LOG_ERROR("Unsupported TGA bpp: ", (int)header.bpp, ". Only 24 or 32 is supported. File: ", filepath);
                return result;
            }

            result.Width = header.width;
            result.Height = header.height;
            result.Channels = header.bpp / 8;

            // Skip the image ID field if it exists
            if (header.idLength > 0)
            {
                file.seekg(header.idLength, std::ios::cur);
            }

            uint32_t imageSize = result.Width * result.Height * result.Channels;
            result.Data = new unsigned char[imageSize];
            file.read(reinterpret_cast<char *>(result.Data), imageSize);

            if (!file)
            {
                PYRAMID_LOG_ERROR("Failed to read TGA image data from: ", filepath);
                delete[] result.Data;
                result.Data = nullptr;
                return result;
            }

            // TGA stores images in BGR(A) format, so we need to swap the B and R channels for OpenGL (which expects RGB(A))
            for (uint32_t i = 0; i < imageSize; i += result.Channels)
            {
                std::swap(result.Data[i], result.Data[i + 2]);
            }

            // TGA can be stored top-down or bottom-up. Bit 5 of imageDescriptor tells us.
            // 0 = bottom-up (OpenGL's preferred format), 1 = top-down.
            bool isTopDown = (header.imageDescriptor & 0x20) != 0;
            if (isTopDown)
            {
                uint32_t bytesPerRow = result.Width * result.Channels;
                unsigned char *tempRow = new unsigned char[bytesPerRow];
                for (int i = 0; i < result.Height / 2; ++i)
                {
                    unsigned char *row1 = result.Data + i * bytesPerRow;
                    unsigned char *row2 = result.Data + (result.Height - 1 - i) * bytesPerRow;
                    memcpy(tempRow, row1, bytesPerRow);
                    memcpy(row1, row2, bytesPerRow);
                    memcpy(row2, tempRow, bytesPerRow);
                }
                delete[] tempRow;
            }

            PYRAMID_LOG_INFO("Successfully loaded TGA image: ", filepath, " (", result.Width, "x", result.Height, ")");
            return result;
        }

        ImageData LoadBMPFromFile(const std::string &filepath)
        {
            ImageData result;
            std::ifstream file(filepath, std::ios::binary);

            if (!file.is_open())
            {
                PYRAMID_LOG_ERROR("Failed to open BMP file: ", filepath);
                return result;
            }

            // Read BMP file header
            BMPFileHeader fileHeader;
            file.read(reinterpret_cast<char *>(&fileHeader), sizeof(fileHeader));

            if (!file)
            {
                PYRAMID_LOG_ERROR("Failed to read BMP file header from: ", filepath);
                return result;
            }

            // Check BMP signature
            if (fileHeader.signature != 0x4D42) // "BM" in little-endian
            {
                PYRAMID_LOG_ERROR("Invalid BMP signature in file: ", filepath);
                return result;
            }

            // Read BMP info header
            BMPInfoHeader infoHeader;
            file.read(reinterpret_cast<char *>(&infoHeader), sizeof(infoHeader));

            if (!file)
            {
                PYRAMID_LOG_ERROR("Failed to read BMP info header from: ", filepath);
                return result;
            }

            // Validate header size (we only support BITMAPINFOHEADER)
            if (infoHeader.headerSize != 40)
            {
                PYRAMID_LOG_ERROR("Unsupported BMP header size: ", infoHeader.headerSize, ". Only BITMAPINFOHEADER (40 bytes) is supported. File: ", filepath);
                return result;
            }

            // We only support uncompressed BMPs
            if (infoHeader.compression != 0)
            {
                PYRAMID_LOG_ERROR("Unsupported BMP compression: ", infoHeader.compression, ". Only uncompressed BMPs are supported. File: ", filepath);
                return result;
            }

            // We only support 24 (RGB) and 32 (RGBA) bits per pixel
            if (infoHeader.bitsPerPixel != 24 && infoHeader.bitsPerPixel != 32)
            {
                PYRAMID_LOG_ERROR("Unsupported BMP bits per pixel: ", infoHeader.bitsPerPixel, ". Only 24 or 32 is supported. File: ", filepath);
                return result;
            }

            // Handle negative height (top-down BMPs)
            bool isTopDown = infoHeader.height < 0;
            int32_t height = isTopDown ? -infoHeader.height : infoHeader.height;

            result.Width = infoHeader.width;
            result.Height = height;
            result.Channels = infoHeader.bitsPerPixel / 8;

            // Seek to the pixel data
            file.seekg(fileHeader.dataOffset, std::ios::beg);

            // Calculate row padding (BMP rows are padded to 4-byte boundaries)
            uint32_t bytesPerPixel = result.Channels;
            uint32_t rowSizeWithoutPadding = result.Width * bytesPerPixel;
            uint32_t rowSizeWithPadding = ((rowSizeWithoutPadding + 3) / 4) * 4;
            uint32_t padding = rowSizeWithPadding - rowSizeWithoutPadding;

            uint32_t imageSize = result.Width * result.Height * result.Channels;
            result.Data = new unsigned char[imageSize];

            // Read pixel data row by row to handle padding
            for (int32_t row = 0; row < result.Height; ++row)
            {
                unsigned char *rowData = result.Data + row * rowSizeWithoutPadding;
                file.read(reinterpret_cast<char *>(rowData), rowSizeWithoutPadding);

                if (!file)
                {
                    PYRAMID_LOG_ERROR("Failed to read BMP pixel data from: ", filepath);
                    delete[] result.Data;
                    result.Data = nullptr;
                    return result;
                }

                // Skip padding bytes
                if (padding > 0)
                {
                    file.seekg(padding, std::ios::cur);
                }
            }

            // BMP stores images in BGR(A) format, so we need to swap the B and R channels for OpenGL (which expects RGB(A))
            for (uint32_t i = 0; i < imageSize; i += result.Channels)
            {
                std::swap(result.Data[i], result.Data[i + 2]);
            }

            // BMP images are stored bottom-up by default (unless height is negative)
            // We need to flip them to top-down for OpenGL
            if (!isTopDown)
            {
                uint32_t bytesPerRow = result.Width * result.Channels;
                unsigned char *tempRow = new unsigned char[bytesPerRow];
                for (int i = 0; i < result.Height / 2; ++i)
                {
                    unsigned char *row1 = result.Data + i * bytesPerRow;
                    unsigned char *row2 = result.Data + (result.Height - 1 - i) * bytesPerRow;
                    memcpy(tempRow, row1, bytesPerRow);
                    memcpy(row1, row2, bytesPerRow);
                    memcpy(row2, tempRow, bytesPerRow);
                }
                delete[] tempRow;
            }

            PYRAMID_LOG_INFO("Successfully loaded BMP image: ", filepath, " (", result.Width, "x", result.Height, ")");
            return result;
        }

        ImageData LoadPNGFromFile(const std::string &filepath)
        {
            return PNGLoader::LoadFromFile(filepath);
        }

        ImageData LoadJPEGFromFile(const std::string &filepath)
        {
            return JPEGLoader::LoadFromFile(filepath);
        }

        void Image::Free(unsigned char *data)
        {
            if (data)
            {
                delete[] data;
            }
        }

    }
} // namespace Pyramid::Util