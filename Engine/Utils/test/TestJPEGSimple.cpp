#include "Pyramid/Util/Image.hpp"
#include "Pyramid/Util/JPEGLoader.hpp"
#include "Fixtures/JPEGFixtures.hpp"

#include <cstdlib>
#include <iostream>

using Pyramid::Tests::Fixtures::BaselineRGBJPEG;
using Pyramid::Tests::Fixtures::BaselineRGBJPEGSize;
using namespace Pyramid::Util;

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "TestJPEGSimple failure: " << message << '\n';
        return EXIT_FAILURE;
    }
}

int main()
{
    ImageData image = JPEGLoader::LoadFromMemory(BaselineRGBJPEG, BaselineRGBJPEGSize);
    if (!image.Data)
    {
        std::cerr << JPEGLoader::GetLastError() << '\n';
        return Fail("valid baseline JPEG did not decode");
    }

    if (image.Width != 8 || image.Height != 8 || image.Channels != 3)
    {
        Image::Free(image.Data);
        return Fail("decoded baseline JPEG metadata is incorrect");
    }

    // JPEG is lossy, so validate the dominant channel rather than exact bytes.
    if (image.Data[0] < 230 || image.Data[1] > 35 || image.Data[2] > 45)
    {
        Image::Free(image.Data);
        return Fail("decoded baseline JPEG pixel is not approximately red");
    }
    Image::Free(image.Data);

    const std::uint8_t invalid[] = {0xFF, 0xD8, 0xFF, 0xD9};
    image = JPEGLoader::LoadFromMemory(invalid, sizeof(invalid));
    if (image.Data)
    {
        Image::Free(image.Data);
        return Fail("invalid JPEG unexpectedly decoded");
    }
    if (JPEGLoader::GetLastError().empty())
    {
        return Fail("invalid JPEG did not provide an error message");
    }

    image = Image::Load("nonexistent_unique_texture.jpg");
    if (image.Data)
    {
        Image::Free(image.Data);
        return Fail("non-existent JPEG unexpectedly loaded");
    }

    std::cout << "Baseline JPEG decode tests passed\n";
    return EXIT_SUCCESS;
}
