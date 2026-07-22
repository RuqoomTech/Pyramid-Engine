#include "Pyramid/Util/PNGLoader.hpp"
#include "Pyramid/Util/Image.hpp"
#include "Pyramid/Util/ZLib.hpp"
#include "Pyramid/Util/Log.hpp"
#include <fstream>
#include <algorithm>
#include <cstring>

namespace Pyramid
{
    namespace Util
    {
        std::string PNGLoader::s_LastError;

        // PNG signature: 8 bytes
        static const uint8_t PNG_SIGNATURE[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

        // CRC32 table for PNG chunk verification
        static uint32_t crc_table[256];
        static bool crc_table_computed = false;

        static void make_crc_table()
        {
            uint32_t c;
            for (int n = 0; n < 256; n++)
            {
                c = static_cast<uint32_t>(n);
                for (int k = 0; k < 8; k++)
                {
                    if (c & 1)
                        c = 0xedb88320L ^ (c >> 1);
                    else
                        c = c >> 1;
                }
                crc_table[n] = c;
            }
            crc_table_computed = true;
        }

        ImageData PNGLoader::LoadFromFile(const std::string &filepath)
        {
            std::ifstream file(filepath, std::ios::binary);
            if (!file.is_open())
            {
                SetError("Failed to open PNG file: " + filepath);
                return ImageData{};
            }

            // Get file size
            file.seekg(0, std::ios::end);
            size_t fileSize = file.tellg();
            file.seekg(0, std::ios::beg);

            // Read entire file into memory
            std::vector<uint8_t> fileData(fileSize);
            file.read(reinterpret_cast<char *>(fileData.data()), fileSize);

            if (!file)
            {
                SetError("Failed to read PNG file: " + filepath);
                return ImageData{};
            }

            return LoadFromMemory(fileData.data(), fileSize);
        }

        ImageData PNGLoader::LoadFromMemory(const uint8_t *data, size_t size)
        {
            return LoadPNGInternal(data, size);
        }

        const std::string &PNGLoader::GetLastError()
        {
            return s_LastError;
        }

        ImageData PNGLoader::LoadPNGInternal(const uint8_t *data, size_t size)
        {
            s_LastError.clear();

            if (!VerifySignature(data, size))
            {
                return ImageData{};
            }

            size_t offset = 8; // Skip PNG signature
            PNGHeader header{};
            std::vector<uint8_t> idatData;
            std::vector<uint8_t> palette;
            bool headerParsed = false;

            // Read chunks
            while (offset < size)
            {
                PNGChunk chunk;
                if (!ReadChunk(data, size, offset, chunk))
                {
                    SetError("Failed to read PNG chunk");
                    return ImageData{};
                }

                // Convert chunk type to string for easier comparison
                char chunkType[5] = {0};
                chunkType[0] = (chunk.type >> 24) & 0xFF;
                chunkType[1] = (chunk.type >> 16) & 0xFF;
                chunkType[2] = (chunk.type >> 8) & 0xFF;
                chunkType[3] = chunk.type & 0xFF;

                if (strcmp(chunkType, "IHDR") == 0)
                {
                    if (!ParseIHDR(chunk, header))
                    {
                        return ImageData{};
                    }
                    headerParsed = true;
                }
                else if (strcmp(chunkType, "PLTE") == 0)
                {
                    // Palette chunk
                    palette = chunk.data;
                }
                else if (strcmp(chunkType, "IDAT") == 0)
                {
                    // Image data chunk - concatenate all IDAT chunks
                    idatData.insert(idatData.end(), chunk.data.begin(), chunk.data.end());
                }
                else if (strcmp(chunkType, "IEND") == 0)
                {
                    // End of image
                    break;
                }
                // Ignore other chunks for now
            }

            if (!headerParsed)
            {
                SetError("PNG file missing IHDR chunk");
                return ImageData{};
            }

            if (idatData.empty())
            {
                SetError("PNG file missing IDAT chunks");
                return ImageData{};
            }

            // Process the image data
            std::vector<uint8_t> processedData = ProcessImageData(idatData, header, palette);
            if (processedData.empty())
            {
                return ImageData{};
            }

            // Create ImageData structure
            ImageData result;
            result.Width = header.width;
            result.Height = header.height;

            // Determine number of channels based on color type
            switch (header.colorType)
            {
            case GRAYSCALE:
                result.Channels = 1;
                break;
            case TRUECOLOR:
            case INDEXED:
                result.Channels = 3;
                break;
            case GRAYSCALE_ALPHA:
                result.Channels = 2;
                break;
            case TRUECOLOR_ALPHA:
                result.Channels = 4;
                break;
            default:
                SetError("Unsupported PNG color type");
                return ImageData{};
            }

            // For indexed images, we convert to RGB
            if (header.colorType == INDEXED)
            {
                result.Channels = 3;
            }

            // Allocate and copy data
            size_t dataSize = result.Width * result.Height * result.Channels;
            result.Data = new unsigned char[dataSize];
            std::copy(processedData.begin(), processedData.end(), result.Data);

            PYRAMID_LOG_INFO("Successfully loaded PNG image: ", result.Width, "x", result.Height, " (", result.Channels, " channels)");
            return result;
        }

        bool PNGLoader::VerifySignature(const uint8_t *data, size_t size)
        {
            if (size < 8)
            {
                SetError("File too small to be a PNG");
                return false;
            }

            if (memcmp(data, PNG_SIGNATURE, 8) != 0)
            {
                SetError("Invalid PNG signature");
                return false;
            }

            return true;
        }

        bool PNGLoader::ReadChunk(const uint8_t *data, size_t size, size_t &offset, PNGChunk &chunk)
        {
            if (offset + 8 > size) // Need at least 8 bytes for length and type
            {
                return false;
            }

            // Read length (big-endian)
            chunk.length = (static_cast<uint32_t>(data[offset]) << 24) |
                           (static_cast<uint32_t>(data[offset + 1]) << 16) |
                           (static_cast<uint32_t>(data[offset + 2]) << 8) |
                           static_cast<uint32_t>(data[offset + 3]);
            offset += 4;

            // Read type (big-endian)
            chunk.type = (static_cast<uint32_t>(data[offset]) << 24) |
                         (static_cast<uint32_t>(data[offset + 1]) << 16) |
                         (static_cast<uint32_t>(data[offset + 2]) << 8) |
                         static_cast<uint32_t>(data[offset + 3]);
            offset += 4;

            // Check if we have enough data for the chunk data and CRC
            if (offset + chunk.length + 4 > size)
            {
                return false;
            }

            // Read chunk data
            chunk.data.resize(chunk.length);
            if (chunk.length > 0)
            {
                std::copy(data + offset, data + offset + chunk.length, chunk.data.begin());
                offset += chunk.length;
            }

            // Read CRC (big-endian)
            chunk.crc = (static_cast<uint32_t>(data[offset]) << 24) |
                        (static_cast<uint32_t>(data[offset + 1]) << 16) |
                        (static_cast<uint32_t>(data[offset + 2]) << 8) |
                        static_cast<uint32_t>(data[offset + 3]);
            offset += 4;

            // Verify CRC
            std::vector<uint8_t> crcData(4 + chunk.length);
            crcData[0] = (chunk.type >> 24) & 0xFF;
            crcData[1] = (chunk.type >> 16) & 0xFF;
            crcData[2] = (chunk.type >> 8) & 0xFF;
            crcData[3] = chunk.type & 0xFF;
            if (chunk.length > 0)
            {
                std::copy(chunk.data.begin(), chunk.data.end(), crcData.begin() + 4);
            }

            uint32_t calculatedCRC = CalculateCRC32(crcData.data(), crcData.size());
            if (calculatedCRC != chunk.crc)
            {
                SetError("PNG chunk CRC mismatch");
                return false;
            }

            return true;
        }

        bool PNGLoader::ParseIHDR(const PNGChunk &chunk, PNGHeader &header)
        {
            if (chunk.length != 13)
            {
                SetError("Invalid IHDR chunk length");
                return false;
            }

            const uint8_t *data = chunk.data.data();

            // Read header fields (all big-endian)
            header.width = (static_cast<uint32_t>(data[0]) << 24) |
                           (static_cast<uint32_t>(data[1]) << 16) |
                           (static_cast<uint32_t>(data[2]) << 8) |
                           static_cast<uint32_t>(data[3]);

            header.height = (static_cast<uint32_t>(data[4]) << 24) |
                            (static_cast<uint32_t>(data[5]) << 16) |
                            (static_cast<uint32_t>(data[6]) << 8) |
                            static_cast<uint32_t>(data[7]);

            header.bitDepth = data[8];
            header.colorType = data[9];
            header.compressionMethod = data[10];
            header.filterMethod = data[11];
            header.interlaceMethod = data[12];

            // Validate header
            if (header.width == 0 || header.height == 0)
            {
                SetError("Invalid PNG dimensions");
                return false;
            }

            if (header.compressionMethod != 0)
            {
                SetError("Unsupported PNG compression method");
                return false;
            }

            if (header.filterMethod != 0)
            {
                SetError("Unsupported PNG filter method");
                return false;
            }

            if (header.interlaceMethod != 0)
            {
                SetError("Interlaced PNG images are not yet supported");
                return false;
            }

            // Validate bit depth for color type
            switch (header.colorType)
            {
            case GRAYSCALE:
                if (header.bitDepth != 1 && header.bitDepth != 2 &&
                    header.bitDepth != 4 && header.bitDepth != 8 && header.bitDepth != 16)
                {
                    SetError("Invalid bit depth for grayscale PNG");
                    return false;
                }
                break;
            case TRUECOLOR:
            case GRAYSCALE_ALPHA:
            case TRUECOLOR_ALPHA:
                if (header.bitDepth != 8 && header.bitDepth != 16)
                {
                    SetError("Invalid bit depth for color PNG");
                    return false;
                }
                break;
            case INDEXED:
                if (header.bitDepth != 1 && header.bitDepth != 2 &&
                    header.bitDepth != 4 && header.bitDepth != 8)
                {
                    SetError("Invalid bit depth for indexed PNG");
                    return false;
                }
                break;
            default:
                SetError("Invalid PNG color type");
                return false;
            }

            return true;
        }

        std::vector<uint8_t> PNGLoader::ProcessImageData(const std::vector<uint8_t> &idatData,
                                                         const PNGHeader &header,
                                                         const std::vector<uint8_t> &palette)
        {
            // Decompress IDAT data using ZLIB
            std::vector<uint8_t> decompressedData;
            if (!ZLib::Decompress(idatData.data(), idatData.size(), decompressedData))
            {
                SetError("Failed to decompress PNG image data: " + ZLib::GetLastError());
                return std::vector<uint8_t>();
            }

            // Apply PNG filters
            std::vector<uint8_t> unfilteredData = ApplyFilters(decompressedData, header);
            if (unfilteredData.empty())
            {
                return std::vector<uint8_t>();
            }

            // Convert to final format
            return ConvertToFinalFormat(unfilteredData, header, palette);
        }

        std::vector<uint8_t> PNGLoader::ApplyFilters(const std::vector<uint8_t> &filteredData,
                                                     const PNGHeader &header)
        {
            int bytesPerPixel = GetBytesPerPixel(static_cast<ColorType>(header.colorType), header.bitDepth);
            int bytesPerRow = (header.width * header.bitDepth *
                                   (header.colorType == INDEXED ? 1 : header.colorType == GRAYSCALE     ? 1
                                                                  : header.colorType == GRAYSCALE_ALPHA ? 2
                                                                  : header.colorType == TRUECOLOR       ? 3
                                                                                                        : 4) +
                               7) /
                              8;

            std::vector<uint8_t> unfilteredData;
            unfilteredData.reserve(header.height * bytesPerRow);

            std::vector<uint8_t> prevRow(bytesPerRow, 0);
            size_t dataIndex = 0;

            for (uint32_t y = 0; y < header.height; ++y)
            {
                if (dataIndex >= filteredData.size())
                {
                    SetError("Insufficient PNG image data");
                    return std::vector<uint8_t>();
                }

                uint8_t filterType = filteredData[dataIndex++];

                if (dataIndex + bytesPerRow > filteredData.size())
                {
                    SetError("Insufficient PNG image data for row");
                    return std::vector<uint8_t>();
                }

                std::vector<uint8_t> currentRow(bytesPerRow);
                std::copy(filteredData.begin() + dataIndex,
                          filteredData.begin() + dataIndex + bytesPerRow,
                          currentRow.begin());
                dataIndex += bytesPerRow;

                // Apply filter
                for (int x = 0; x < bytesPerRow; ++x)
                {
                    uint8_t a = (x >= bytesPerPixel) ? currentRow[x - bytesPerPixel] : 0;
                    uint8_t b = prevRow[x];
                    uint8_t c = (x >= bytesPerPixel) ? prevRow[x - bytesPerPixel] : 0;

                    switch (filterType)
                    {
                    case FILTER_NONE:
                        // No change needed
                        break;
                    case FILTER_SUB:
                        currentRow[x] = (currentRow[x] + a) & 0xFF;
                        break;
                    case FILTER_UP:
                        currentRow[x] = (currentRow[x] + b) & 0xFF;
                        break;
                    case FILTER_AVERAGE:
                        currentRow[x] = (currentRow[x] + ((a + b) / 2)) & 0xFF;
                        break;
                    case FILTER_PAETH:
                        currentRow[x] = (currentRow[x] + PaethPredictor(a, b, c)) & 0xFF;
                        break;
                    default:
                        SetError("Invalid PNG filter type");
                        return std::vector<uint8_t>();
                    }
                }

                unfilteredData.insert(unfilteredData.end(), currentRow.begin(), currentRow.end());
                prevRow = currentRow;
            }

            return unfilteredData;
        }

        uint8_t PNGLoader::PaethPredictor(uint8_t a, uint8_t b, uint8_t c)
        {
            int p = a + b - c;
            int pa = abs(p - a);
            int pb = abs(p - b);
            int pc = abs(p - c);

            if (pa <= pb && pa <= pc)
                return a;
            else if (pb <= pc)
                return b;
            else
                return c;
        }

        int PNGLoader::GetBytesPerPixel(ColorType colorType, uint8_t bitDepth)
        {
            switch (colorType)
            {
            case GRAYSCALE:
                return (bitDepth + 7) / 8;
            case TRUECOLOR:
                return 3 * ((bitDepth + 7) / 8);
            case INDEXED:
                return (bitDepth + 7) / 8;
            case GRAYSCALE_ALPHA:
                return 2 * ((bitDepth + 7) / 8);
            case TRUECOLOR_ALPHA:
                return 4 * ((bitDepth + 7) / 8);
            default:
                return 1;
            }
        }

        std::vector<uint8_t> PNGLoader::ConvertToFinalFormat(const std::vector<uint8_t> &imageData,
                                                             const PNGHeader &header,
                                                             const std::vector<uint8_t> &palette)
        {
            std::vector<uint8_t> result;

            // For now, we'll focus on the most common cases: 8-bit RGB and RGBA
            if (header.bitDepth == 8)
            {
                switch (header.colorType)
                {
                case TRUECOLOR:
                    // RGB - already in correct format
                    result = imageData;
                    break;

                case TRUECOLOR_ALPHA:
                    // RGBA - already in correct format
                    result = imageData;
                    break;

                case GRAYSCALE:
                    // Convert grayscale to RGB
                    result.reserve(imageData.size() * 3);
                    for (size_t i = 0; i < imageData.size(); ++i)
                    {
                        uint8_t gray = imageData[i];
                        result.push_back(gray);
                        result.push_back(gray);
                        result.push_back(gray);
                    }
                    break;

                case INDEXED:
                    // Convert indexed to RGB using palette
                    if (palette.size() < 3)
                    {
                        SetError("PNG palette too small");
                        return std::vector<uint8_t>();
                    }

                    result.reserve(imageData.size() * 3);
                    for (size_t i = 0; i < imageData.size(); ++i)
                    {
                        uint8_t index = imageData[i];
                        if (static_cast<std::size_t>(index) * 3U + 2U >= palette.size())
                        {
                            SetError("PNG palette index out of range");
                            return std::vector<uint8_t>();
                        }
                        result.push_back(palette[index * 3]);     // R
                        result.push_back(palette[index * 3 + 1]); // G
                        result.push_back(palette[index * 3 + 2]); // B
                    }
                    break;

                default:
                    SetError("Unsupported PNG color type for conversion");
                    return std::vector<uint8_t>();
                }
            }
            else
            {
                SetError("Unsupported PNG bit depth (only 8-bit supported currently)");
                return std::vector<uint8_t>();
            }

            return result;
        }

        uint32_t PNGLoader::CalculateCRC32(const uint8_t *data, size_t length)
        {
            if (!crc_table_computed)
            {
                make_crc_table();
            }

            uint32_t c = 0xffffffffL;
            for (size_t n = 0; n < length; n++)
            {
                c = crc_table[(c ^ data[n]) & 0xff] ^ (c >> 8);
            }
            return c ^ 0xffffffffL;
        }

        void PNGLoader::SetError(const std::string &error)
        {
            s_LastError = error;
            PYRAMID_LOG_ERROR("PNGLoader: ", error);
        }

    } // namespace Util
} // namespace Pyramid
