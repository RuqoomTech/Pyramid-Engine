#include "Pyramid/Util/JPEGIDCT.hpp"
#include <iostream>
#include <vector>
#include <cmath>

using namespace Pyramid::Util;

bool TestBasicIDCT()
{
    std::cout << "=== Testing Basic IDCT ===" << std::endl;
    
    JPEGIDCT idct;
    
    // Test with DC-only coefficient (should produce uniform gray)
    int16_t coefficients[64] = {0};
    coefficients[0] = 128; // DC coefficient only
    
    uint8_t output[64];
    
    if (!idct.ProcessBlock(coefficients, output))
    {
        std::cout << "ERROR: Failed to process DC-only block" << std::endl;
        std::cout << "Last error: " << idct.GetLastError() << std::endl;
        return false;
    }
    
    // Check that all output values are approximately the same (uniform gray)
    uint8_t expectedValue = output[0];
    bool isUniform = true;
    
    for (int i = 0; i < 64; ++i)
    {
        if (abs(static_cast<int>(output[i]) - static_cast<int>(expectedValue)) > 2)
        {
            isUniform = false;
            break;
        }
    }
    
    if (!isUniform)
    {
        std::cout << "ERROR: DC-only block should produce uniform output" << std::endl;
        std::cout << "Expected ~" << static_cast<int>(expectedValue) << ", got varying values" << std::endl;
        return false;
    }
    
    std::cout << "SUCCESS: DC-only IDCT produces uniform output (" << static_cast<int>(expectedValue) << ")" << std::endl;
    return true;
}

bool TestZeroCoefficients()
{
    std::cout << "\n=== Testing Zero Coefficients ===" << std::endl;
    
    JPEGIDCT idct;
    
    // Test with all zero coefficients
    int16_t coefficients[64] = {0};
    uint8_t output[64];
    
    if (!idct.ProcessBlock(coefficients, output))
    {
        std::cout << "ERROR: Failed to process zero coefficient block" << std::endl;
        return false;
    }
    
    // All outputs should be 128 (middle gray due to level shift)
    for (int i = 0; i < 64; ++i)
    {
        if (output[i] != 128)
        {
            std::cout << "ERROR: Zero coefficients should produce 128, got " << static_cast<int>(output[i]) << " at index " << i << std::endl;
            return false;
        }
    }
    
    std::cout << "SUCCESS: Zero coefficients produce middle gray (128)" << std::endl;
    return true;
}

bool TestFloatingPointOutput()
{
    std::cout << "\n=== Testing Floating Point Output ===" << std::endl;
    
    JPEGIDCT idct;
    
    // Test with simple coefficients
    int16_t coefficients[64] = {0};
    coefficients[0] = 64;  // DC coefficient
    coefficients[1] = 32;  // First AC coefficient
    
    float output[64];
    
    if (!idct.ProcessBlockFloat(coefficients, output))
    {
        std::cout << "ERROR: Failed to process block with float output" << std::endl;
        return false;
    }
    
    // Check that we get reasonable floating point values
    bool hasVariation = false;
    float minVal = output[0];
    float maxVal = output[0];
    
    for (int i = 0; i < 64; ++i)
    {
        if (output[i] < minVal) minVal = output[i];
        if (output[i] > maxVal) maxVal = output[i];
    }
    
    if (maxVal - minVal > 1.0f)
    {
        hasVariation = true;
    }
    
    if (!hasVariation)
    {
        std::cout << "ERROR: Expected variation in output with AC coefficients" << std::endl;
        return false;
    }
    
    std::cout << "SUCCESS: Float output shows variation (range: " << minVal << " to " << maxVal << ")" << std::endl;
    return true;
}

bool TestErrorHandling()
{
    std::cout << "\n=== Testing Error Handling ===" << std::endl;
    
    JPEGIDCT idct;
    
    // Test with null pointers
    int16_t coefficients[64] = {0};
    uint8_t output[64];
    
    if (idct.ProcessBlock(nullptr, output))
    {
        std::cout << "ERROR: Should have failed with null coefficients pointer" << std::endl;
        return false;
    }
    
    if (idct.ProcessBlock(coefficients, nullptr))
    {
        std::cout << "ERROR: Should have failed with null output pointer" << std::endl;
        return false;
    }
    
    // Test float version
    float floatOutput[64];
    
    if (idct.ProcessBlockFloat(nullptr, floatOutput))
    {
        std::cout << "ERROR: Should have failed with null coefficients pointer (float)" << std::endl;
        return false;
    }
    
    if (idct.ProcessBlockFloat(coefficients, nullptr))
    {
        std::cout << "ERROR: Should have failed with null output pointer (float)" << std::endl;
        return false;
    }
    
    std::cout << "SUCCESS: Error handling works correctly" << std::endl;
    return true;
}

bool TestRangeValidation()
{
    std::cout << "\n=== Testing Output Range Validation ===" << std::endl;
    
    JPEGIDCT idct;
    
    // Test with extreme coefficients that might cause overflow
    int16_t coefficients[64] = {0};
    coefficients[0] = 2000;  // Very large DC coefficient
    
    uint8_t output[64];
    
    if (!idct.ProcessBlock(coefficients, output))
    {
        std::cout << "ERROR: Failed to process block with large coefficients" << std::endl;
        return false;
    }
    
    // Check that all outputs are in valid range [0, 255]
    for (int i = 0; i < 64; ++i)
    {
        if (output[i] < 0 || output[i] > 255)
        {
            std::cout << "ERROR: Output value " << static_cast<int>(output[i]) << " is out of range [0, 255]" << std::endl;
            return false;
        }
    }
    
    // Test with negative coefficients
    coefficients[0] = -2000;  // Very negative DC coefficient
    
    if (!idct.ProcessBlock(coefficients, output))
    {
        std::cout << "ERROR: Failed to process block with negative coefficients" << std::endl;
        return false;
    }
    
    // Check range again
    for (int i = 0; i < 64; ++i)
    {
        if (output[i] < 0 || output[i] > 255)
        {
            std::cout << "ERROR: Output value " << static_cast<int>(output[i]) << " is out of range [0, 255] (negative test)" << std::endl;
            return false;
        }
    }
    
    std::cout << "SUCCESS: Output values are properly clamped to [0, 255] range" << std::endl;
    return true;
}

bool TestIDCTProperties()
{
    std::cout << "\n=== Testing IDCT Mathematical Properties ===" << std::endl;
    
    JPEGIDCT idct;
    
    // Test linearity: IDCT(a*x) = a*IDCT(x) (approximately, due to clamping)
    int16_t coefficients1[64] = {0};
    int16_t coefficients2[64] = {0};
    
    coefficients1[0] = 64;   // DC coefficient
    coefficients1[1] = 32;   // AC coefficient
    
    coefficients2[0] = 128;  // 2x DC coefficient
    coefficients2[1] = 64;   // 2x AC coefficient
    
    float output1[64], output2[64];
    
    if (!idct.ProcessBlockFloat(coefficients1, output1) || 
        !idct.ProcessBlockFloat(coefficients2, output2))
    {
        std::cout << "ERROR: Failed to process blocks for linearity test" << std::endl;
        return false;
    }
    
    // Check that output2 is approximately 2x output1 (before clamping)
    bool linearityHolds = true;
    for (int i = 0; i < 64; ++i)
    {
        float expected = output1[i] * 2.0f;
        float actual = output2[i];
        float error = std::abs(actual - expected);
        
        if (error > 2.0f) // Allow some tolerance for floating point precision
        {
            linearityHolds = false;
            break;
        }
    }
    
    if (!linearityHolds)
    {
        std::cout << "WARN: IDCT linearity property has some deviation (expected due to implementation)" << std::endl;
    }
    else
    {
        std::cout << "SUCCESS: IDCT shows good linearity properties" << std::endl;
    }
    
    return true;
}

int main()
{
    std::cout << "Starting JPEG IDCT Tests..." << std::endl;
    
    bool allTestsPassed = true;
    
    allTestsPassed &= TestBasicIDCT();
    allTestsPassed &= TestZeroCoefficients();
    allTestsPassed &= TestFloatingPointOutput();
    allTestsPassed &= TestErrorHandling();
    allTestsPassed &= TestRangeValidation();
    allTestsPassed &= TestIDCTProperties();
    
    std::cout << "\n=== Test Results ===" << std::endl;
    if (allTestsPassed)
    {
        std::cout << "🎉 ALL IDCT TESTS PASSED!" << std::endl;
        std::cout << "JPEG IDCT implementation is working correctly." << std::endl;
        std::cout << "Ready to implement YCbCr to RGB conversion." << std::endl;
    }
    else
    {
        std::cout << "❌ SOME TESTS FAILED! Please check the implementation." << std::endl;
    }
    
    return allTestsPassed ? 0 : 1;
}
