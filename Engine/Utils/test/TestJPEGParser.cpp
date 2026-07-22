#include "Pyramid/Util/Image.hpp"
#include "Pyramid/Util/JPEGLoader.hpp"
#include "Fixtures/JPEGFixtures.hpp"

#include <cstdlib>
#include <iostream>

using Pyramid::Tests::Fixtures::ProgressiveGrayJPEG;
using Pyramid::Tests::Fixtures::ProgressiveGrayJPEGSize;
using namespace Pyramid::Util;

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "TestJPEGParser failure: " << message << '\n';
        return EXIT_FAILURE;
    }
}

int main()
{
    ImageData image = JPEGLoader::LoadFromMemory(ProgressiveGrayJPEG, ProgressiveGrayJPEGSize);
    if (!image.Data)
    {
        std::cerr << JPEGLoader::GetLastError() << '\n';
        return Fail("valid progressive grayscale JPEG did not decode");
    }

    if (image.Width != 8 || image.Height != 8 || image.Channels != 3)
    {
        Image::Free(image.Data);
        return Fail("progressive JPEG was not normalized to RGB");
    }

    const int red = image.Data[0];
    const int green = image.Data[1];
    const int blue = image.Data[2];
    if (red < 115 || red > 140 || red != green || green != blue)
    {
        Image::Free(image.Data);
        return Fail("grayscale JPEG RGB conversion is incorrect");
    }

    Image::Free(image.Data);
    std::cout << "Progressive JPEG decode tests passed\n";
    return EXIT_SUCCESS;
}
