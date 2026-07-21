#include "Pyramid/Util/Image.hpp"
#include "Pyramid/Util/PNGLoader.hpp"
#include <iostream>
#include <fstream>
#include <vector>

using namespace Pyramid::Util;

// Simple test PNG data (1x1 pixel red PNG)
// This is a minimal valid PNG file for testing
const uint8_t TEST_PNG_DATA[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D,
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53, 0xDE, 0x00, 0x00, 0x00,
    0x0F, 0x49, 0x44, 0x41, 0x54, 0x78, 0x01, 0x01, 0x04, 0x00, 0xFB, 0xFF,
    0x00, 0xFF, 0x00, 0x00, 0x03, 0x01, 0x01, 0x00, 0x8D, 0x1D, 0xE5, 0x82,
    0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82,
};

bool TestBasicPNGLoading()
{
    std::cout << "=== Testing Basic PNG Loading ===" << std::endl;

    // Create a temporary PNG file for testing
    std::string testFile = "test_image.png";
    std::ofstream file(testFile, std::ios::binary);
    if (!file.is_open())
    {
        std::cout << "ERROR: Could not create test PNG file" << std::endl;
        return false;
    }

    file.write(reinterpret_cast<const char *>(TEST_PNG_DATA), sizeof(TEST_PNG_DATA));
    file.close();

    // Test loading through main Image::Load function
    ImageData result = Image::Load(testFile);

    // Clean up test file
    std::remove(testFile.c_str());

    if (!result.Data)
    {
        std::cout << "ERROR: Failed to load test PNG file" << std::endl;
        return false;
    }

    // Verify image properties
    if (result.Width != 1 || result.Height != 1 || result.Channels != 3)
    {
        std::cout << "ERROR: Incorrect image dimensions or channels" << std::endl;
        std::cout << "Expected: 1x1x3, Got: " << result.Width << "x" << result.Height << "x" << result.Channels << std::endl;
        Image::Free(result.Data);
        return false;
    }

    // Verify pixel data (should be red: 255, 0, 0)
    if (result.Data[0] != 255 || result.Data[1] != 0 || result.Data[2] != 0)
    {
        std::cout << "ERROR: Incorrect pixel data" << std::endl;
        std::cout << "Expected: (255,0,0), Got: (" << (int)result.Data[0] << "," << (int)result.Data[1] << "," << (int)result.Data[2] << ")" << std::endl;
        Image::Free(result.Data);
        return false;
    }

    Image::Free(result.Data);
    std::cout << "SUCCESS: Basic PNG loading test passed!" << std::endl;
    return true;
}

bool TestInvalidPNGHandling()
{
    std::cout << "\n=== Testing Invalid PNG Handling ===" << std::endl;

    // Test with invalid PNG signature
    uint8_t invalidPNG[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    std::string testFile = "invalid_test.png";
    std::ofstream file(testFile, std::ios::binary);
    if (!file.is_open())
    {
        std::cout << "ERROR: Could not create invalid test PNG file" << std::endl;
        return false;
    }

    file.write(reinterpret_cast<const char *>(invalidPNG), sizeof(invalidPNG));
    file.close();

    // Test loading invalid PNG
    ImageData result = Image::Load(testFile);

    // Clean up test file
    std::remove(testFile.c_str());

    if (result.Data != nullptr)
    {
        std::cout << "ERROR: Invalid PNG was loaded successfully (should have failed)" << std::endl;
        Image::Free(result.Data);
        return false;
    }

    std::cout << "SUCCESS: Invalid PNG handling test passed!" << std::endl;
    return true;
}

bool TestNonExistentFile()
{
    std::cout << "\n=== Testing Non-Existent File Handling ===" << std::endl;

    ImageData result = Image::Load("non_existent_file.png");

    if (result.Data != nullptr)
    {
        std::cout << "ERROR: Non-existent file was loaded successfully (should have failed)" << std::endl;
        Image::Free(result.Data);
        return false;
    }

    std::cout << "SUCCESS: Non-existent file handling test passed!" << std::endl;
    return true;
}

bool TestPNGLoaderDirectly()
{
    std::cout << "\n=== Testing PNG Loader Directly ===" << std::endl;

    // Test loading from memory
    ImageData result = PNGLoader::LoadFromMemory(TEST_PNG_DATA, sizeof(TEST_PNG_DATA));

    if (!result.Data)
    {
        std::cout << "ERROR: Failed to load PNG from memory" << std::endl;
        std::cout << "Last error: " << PNGLoader::GetLastError() << std::endl;
        return false;
    }

    // Verify image properties
    if (result.Width != 1 || result.Height != 1 || result.Channels != 3)
    {
        std::cout << "ERROR: Incorrect image dimensions or channels in direct test" << std::endl;
        Image::Free(result.Data);
        return false;
    }

    Image::Free(result.Data);
    std::cout << "SUCCESS: Direct PNG loader test passed!" << std::endl;
    return true;
}

int main()
{
    std::cout << "Starting PNG Loader Tests..." << std::endl;

    bool allTestsPassed = true;

    allTestsPassed &= TestBasicPNGLoading();
    allTestsPassed &= TestInvalidPNGHandling();
    allTestsPassed &= TestNonExistentFile();
    allTestsPassed &= TestPNGLoaderDirectly();

    std::cout << "\n=== Test Results ===" << std::endl;
    if (allTestsPassed)
    {
        std::cout << "🎉 ALL TESTS PASSED! PNG loading functionality is working correctly." << std::endl;
    }
    else
    {
        std::cout << "❌ SOME TESTS FAILED! Please check the implementation." << std::endl;
    }

    return allTestsPassed ? 0 : 1;
}
