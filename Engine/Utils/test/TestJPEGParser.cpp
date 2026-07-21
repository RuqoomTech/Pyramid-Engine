#include "Pyramid/Util/Image.hpp"
#include "Pyramid/Util/JPEGLoader.hpp"
#include <iostream>
#include <vector>

using namespace Pyramid::Util;

bool TestJPEGMarkerParsing()
{
    std::cout << "=== Testing JPEG Marker Parsing ===" << std::endl;

    // Minimal JPEG structure for testing marker parsing
    // This creates a valid JPEG header structure without actual image data
    std::vector<uint8_t> jpegData = {
        // SOI (Start of Image)
        0xFF, 0xD8,

        // APP0 (JFIF Application Segment)
        0xFF, 0xE0,
        0x00, 0x10,               // Length: 16 bytes
        'J', 'F', 'I', 'F', 0x00, // JFIF identifier
        0x01, 0x01,               // Version 1.1
        0x01,                     // Units (1 = dots per inch)
        0x00, 0x48,               // X density: 72 dpi
        0x00, 0x48,               // Y density: 72 dpi
        0x00,                     // Thumbnail width: 0
        0x00,                     // Thumbnail height: 0

        // DQT (Define Quantization Table) - Simplified
        0xFF, 0xDB,
        0x00, 0x43, // Length: 67 bytes (64 + 3)
        0x00,       // Precision: 0 (8-bit), Table ID: 0
        // 64 quantization values (simplified - all set to 16 for testing)
        16, 16, 16, 16, 16, 16, 16, 16,
        16, 16, 16, 16, 16, 16, 16, 16,
        16, 16, 16, 16, 16, 16, 16, 16,
        16, 16, 16, 16, 16, 16, 16, 16,
        16, 16, 16, 16, 16, 16, 16, 16,
        16, 16, 16, 16, 16, 16, 16, 16,
        16, 16, 16, 16, 16, 16, 16, 16,
        16, 16, 16, 16, 16, 16, 16, 16,

        // SOF0 (Start of Frame - Baseline DCT)
        0xFF, 0xC0,
        0x00, 0x11, // Length: 17 bytes
        0x08,       // Precision: 8 bits
        0x00, 0x10, // Height: 16 pixels
        0x00, 0x10, // Width: 16 pixels
        0x03,       // Number of components: 3 (YCbCr)
        // Component 1 (Y)
        0x01, // Component ID: 1
        0x22, // Sampling factors: 2x2
        0x00, // Quantization table ID: 0
        // Component 2 (Cb)
        0x02, // Component ID: 2
        0x11, // Sampling factors: 1x1
        0x00, // Quantization table ID: 0
        // Component 3 (Cr)
        0x03, // Component ID: 3
        0x11, // Sampling factors: 1x1
        0x00, // Quantization table ID: 0

        // DHT (Define Huffman Table) - Minimal DC table
        0xFF, 0xC4,
        0x00, 0x14, // Length: 20 bytes (2 + 1 + 16 + 1)
        0x00,       // Table class: 0 (DC), Table ID: 0
        // Code lengths (16 bytes) - simplified: 1 code of length 1
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // Symbols (1 byte)
        0x00,

        // SOS (Start of Scan)
        0xFF, 0xDA,
        0x00, 0x0C, // Length: 12 bytes
        0x03,       // Number of components: 3
        // Component 1
        0x01, // Component ID: 1
        0x00, // DC table: 0, AC table: 0
        // Component 2
        0x02, // Component ID: 2
        0x00, // DC table: 0, AC table: 0
        // Component 3
        0x03, // Component ID: 3
        0x00, // DC table: 0, AC table: 0
        0x00, // Spectral start: 0
        0x3F, // Spectral end: 63
        0x00, // Successive approximation: 0

        // Minimal image data (just a few bytes to represent compressed data)
        0xFF, 0x00, // Escaped 0xFF
        0x12, 0x34, // Some data

        // EOI (End of Image)
        0xFF, 0xD9};

    // Debug: Print the JPEG data size and some key offsets
    std::cout << "DEBUG: JPEG test data size: " << jpegData.size() << " bytes" << std::endl;

    // Test loading from memory
    ImageData result = JPEGLoader::LoadFromMemory(jpegData.data(), jpegData.size());

    if (!result.Data)
    {
        std::cout << "ERROR: Failed to parse JPEG markers" << std::endl;
        std::cout << "Last error: " << JPEGLoader::GetLastError() << std::endl;

        // Let's try a simpler test - just check if we can recognize the SOI marker
        std::cout << "DEBUG: First 4 bytes: 0x" << std::hex
                  << (int)jpegData[0] << (int)jpegData[1] << " 0x"
                  << (int)jpegData[2] << (int)jpegData[3] << std::dec << std::endl;
        return false;
    }

    // Verify basic properties
    if (result.Width != 16 || result.Height != 16 || result.Channels != 3)
    {
        std::cout << "ERROR: Incorrect JPEG properties parsed" << std::endl;
        std::cout << "Expected: 16x16x3, Got: " << result.Width << "x" << result.Height << "x" << result.Channels << std::endl;
        Image::Free(result.Data);
        return false;
    }

    Image::Free(result.Data);
    std::cout << "SUCCESS: JPEG marker parsing test passed!" << std::endl;
    return true;
}

bool TestJPEGExtensionRecognition()
{
    std::cout << "\n=== Testing JPEG Extension Recognition ===" << std::endl;

    // Test .jpg extension
    ImageData result1 = Image::Load("test.jpg");
    if (result1.Data != nullptr)
    {
        std::cout << "ERROR: Non-existent .jpg file was loaded" << std::endl;
        Image::Free(result1.Data);
        return false;
    }

    // Test .jpeg extension
    ImageData result2 = Image::Load("test.jpeg");
    if (result2.Data != nullptr)
    {
        std::cout << "ERROR: Non-existent .jpeg file was loaded" << std::endl;
        Image::Free(result2.Data);
        return false;
    }

    std::cout << "SUCCESS: JPEG extension recognition test passed!" << std::endl;
    return true;
}

int main()
{
    std::cout << "Starting JPEG Parser Tests..." << std::endl;

    bool allTestsPassed = true;

    allTestsPassed &= TestJPEGMarkerParsing();
    allTestsPassed &= TestJPEGExtensionRecognition();

    std::cout << "\n=== Test Results ===" << std::endl;
    if (allTestsPassed)
    {
        std::cout << "🎉 ALL JPEG PARSER TESTS PASSED!" << std::endl;
        std::cout << "JPEG marker parsing is working correctly." << std::endl;
        std::cout << "Ready to implement Huffman decoding, IDCT, and color conversion." << std::endl;
    }
    else
    {
        std::cout << "❌ SOME TESTS FAILED! Please check the implementation." << std::endl;
    }

    return allTestsPassed ? 0 : 1;
}
