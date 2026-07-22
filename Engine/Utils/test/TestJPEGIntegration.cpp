#include "Pyramid/Util/Image.hpp"
#include "Pyramid/Util/JPEGLoader.hpp"
#include "Fixtures/JPEGFixtures.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

using Pyramid::Tests::Fixtures::BaselineRGBJPEG;
using Pyramid::Tests::Fixtures::BaselineRGBJPEGSize;
using namespace Pyramid::Util;

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "TestJPEGIntegration failure: " << message << '\n';
        return EXIT_FAILURE;
    }
}

int main()
{
    const std::string filepath = "pyramid_test_image.jpg";
    {
        std::ofstream file(filepath, std::ios::binary);
        if (!file)
        {
            return Fail("could not create JPEG fixture file");
        }
        file.write(
            reinterpret_cast<const char*>(BaselineRGBJPEG),
            static_cast<std::streamsize>(BaselineRGBJPEGSize));
    }

    ImageData direct = JPEGLoader::LoadFromFile(filepath);
    if (!direct.Data)
    {
        std::remove(filepath.c_str());
        return Fail("JPEGLoader::LoadFromFile failed");
    }

    ImageData generic = Image::Load(filepath);
    std::remove(filepath.c_str());
    if (!generic.Data)
    {
        Image::Free(direct.Data);
        return Fail("Image::Load did not route JPEG files to the decoder");
    }

    const bool matchingMetadata =
        direct.Width == generic.Width && direct.Height == generic.Height &&
        direct.Channels == generic.Channels && direct.Width == 8 && direct.Height == 8;
    const bool matchingPixel =
        direct.Data[0] == generic.Data[0] &&
        direct.Data[1] == generic.Data[1] &&
        direct.Data[2] == generic.Data[2];

    Image::Free(direct.Data);
    Image::Free(generic.Data);

    if (!matchingMetadata || !matchingPixel)
    {
        return Fail("direct and generic JPEG loading paths disagree");
    }

    std::cout << "JPEG file integration tests passed\n";
    return EXIT_SUCCESS;
}
