#include "TestGraphicsDevice.hpp"

#include <Pyramid/Graphics/Renderer/RenderSystem.hpp>

#include <cstdlib>
#include <iostream>
#include <type_traits>
#include <utility>

namespace
{
    template<class T, class = void>
    struct HasDispatchMethod : std::false_type {};

    template<class T>
    struct HasDispatchMethod<T, std::void_t<decltype(std::declval<T&>().Dispatch(
        Pyramid::u32{}, Pyramid::u32{}, Pyramid::u32{}))>> : std::true_type {};

    template<class T, class = void>
    struct HasDispatchCommand : std::false_type {};

    template<class T>
    struct HasDispatchCommand<T, std::void_t<decltype(T::Dispatch)>> : std::true_type {};

    template<class T, class = void>
    struct HasDispatchPayload : std::false_type {};

    template<class T>
    struct HasDispatchPayload<T, std::void_t<decltype(std::declval<T&>().data.dispatch)>>
        : std::true_type {};

    static_assert(!HasDispatchMethod<Pyramid::Renderer::CommandBuffer>::value,
        "CommandBuffer must not advertise unsupported compute dispatch");
    static_assert(!HasDispatchCommand<Pyramid::Renderer::RenderCommandType>::value,
        "RenderCommandType must not contain an unsupported Dispatch command");
    static_assert(!HasDispatchPayload<Pyramid::Renderer::RenderCommand>::value,
        "RenderCommand must not expose unsupported dispatch payload state");
    static_assert(static_cast<int>(Pyramid::Renderer::RenderCommandType::ClearTarget) == 15,
        "Removing Dispatch must preserve the existing ClearTarget command value");
}

int main()
{
    using namespace Pyramid;

    Tests::TestGraphicsDevice device;
    Renderer::CommandBuffer commands;
    commands.Begin();
    commands.ClearTarget(0.1f, 0.2f, 0.3f, 1.0f);
    commands.DrawIndexed(6, 2, PrimitiveTopology::Triangles);
    commands.DrawArrays(4, 3, 1, PrimitiveTopology::TriangleStrip);
    commands.End();

    const Renderer::CommandBufferDrawStats stats = commands.GetDrawStats();
    if (commands.GetCommandCount() != 3 || stats.drawCalls != 2 ||
        stats.vertices != 16 || stats.triangles != 6)
    {
        std::cerr << "CommandBuffer draw statistics counted a clear or lost recorded draws\n";
        return EXIT_FAILURE;
    }

    commands.Execute(&device);
    if (device.clearCalls != 1 || device.lastClearColor.r != 0.1f ||
        device.lastClearColor.g != 0.2f || device.lastClearColor.b != 0.3f ||
        device.lastClearColor.a != 1.0f)
    {
        std::cerr << "CommandBuffer execution lost the clear or forwarded incorrect RGBA values\n";
        return EXIT_FAILURE;
    }
    if (device.drawCalls != 2 || device.lastDrawIndexed || device.lastDrawCount != 4 ||
        device.lastInstanceCount != 1 || device.lastFirstVertex != 3 ||
        device.lastTopology != PrimitiveTopology::TriangleStrip)
    {
        std::cerr << "CommandBuffer execution lost draw order or forwarded incorrect draw arguments\n";
        return EXIT_FAILURE;
    }

    commands.Reset();
    const Renderer::CommandBufferDrawStats empty = commands.GetDrawStats();
    if (empty.drawCalls != 0 || empty.vertices != 0 || empty.triangles != 0)
    {
        std::cerr << "Reset command buffer retained draw statistics\n";
        return EXIT_FAILURE;
    }
    device.ResetDrawState();
    commands.Execute(&device);
    if (commands.GetCommandCount() != 0 || device.drawCalls != 0 || device.clearCalls != 1)
    {
        std::cerr << "Reset command buffer executed previously recorded draws or clears\n";
        return EXIT_FAILURE;
    }

    std::cout << "CommandBuffer statistics tests passed\n";
    return EXIT_SUCCESS;
}
