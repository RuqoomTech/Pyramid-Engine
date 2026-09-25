#pragma once

#include <Pyramid/Core/Prerequisites.hpp>

namespace Pyramid
{
    /**
     * @brief Backend-neutral render-target (framebuffer) contract.
     *
     * This header is deliberately free of OpenGL and Win32 types so the
     * neutral boundary never leaks backend tokens. Concrete implementations
     * live in the graphics backend; callers, render passes, and the renderer
     * only ever see this surface.
     *
     * A framebuffer is owned by whoever created it. Binding through this
     * interface never transfers ownership and never mutates the target.
     */
    class IFramebuffer
    {
    public:
        virtual ~IFramebuffer() = default;

        /**
         * @brief Make this target current for rendering.
         *
         * Implementations also establish the viewport that matches their own
         * extents, so a pass that binds a target does not inherit a stale
         * viewport from the previous pass.
         */
        virtual void Bind() const = 0;

        /**
         * @brief Restore the default surface (framebuffer zero).
         */
        virtual void Unbind() const = 0;

        /**
         * @brief Whether this target is currently usable for rendering.
         * @return false for an uninitialized or incomplete target
         */
        virtual bool IsComplete() const = 0;

        /** @brief Target width in pixels. */
        virtual u32 GetWidth() const = 0;

        /** @brief Target height in pixels. */
        virtual u32 GetHeight() const = 0;

        /**
         * @brief Opaque backend identity for this target.
         *
         * The value is meaningful only to the backend that produced it. Zero
         * denotes the default surface. Passes must not interpret it; use it
         * only for identity comparison or for handing the target back to the
         * owning backend.
         */
        virtual u32 GetNativeHandle() const = 0;
    };

} // namespace Pyramid
