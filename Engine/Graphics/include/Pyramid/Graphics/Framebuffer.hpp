#pragma once

namespace Pyramid
{
    /**
     * @brief Borrowed framebuffer binding interface, independent of native APIs.
     *
     * Implementations belong to the calling device's graphics context. The
     * owner must keep both the framebuffer and that context alive during use.
     */
    class IFramebuffer
    {
    public:
        virtual ~IFramebuffer() = default;

        /** Whether owned framebuffer resources are ready for binding. */
        virtual bool IsInitialized() const = 0;

        /** Bind the framebuffer and its target viewport. Requires initialization. */
        virtual void Bind() const = 0;
    };
}
