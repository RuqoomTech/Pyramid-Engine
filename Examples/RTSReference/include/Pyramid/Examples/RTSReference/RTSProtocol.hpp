#pragma once

#include <Pyramid/Core/Prerequisites.hpp>
#include <Pyramid/Math/Quat.hpp>
#include <Pyramid/Math/Vec3.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace Pyramid::Examples::RTSReference::Protocol
{
    inline constexpr u16 Version = 1;
    inline constexpr u32 GameplayVersion = 1;
    inline constexpr u32 TicksPerSecond = 32;
    inline constexpr std::size_t HeaderSize = 20;
    inline constexpr std::size_t MaximumFrameBytes = 256 * 1024;
    inline constexpr std::size_t MaximumUnits = 2048;
    inline constexpr f32 MaximumCoordinate = 1000000.0f;
    inline constexpr f32 MaximumSpeed = 10000.0f;
    inline constexpr f32 MaximumScale = 1024.0f;

    using ContentId = std::array<u8, 16>;

    enum class MessageKind : u8
    {
        Hello = 1,
        MoveCommand = 2,
        FullSnapshot = 3
    };

    struct Hello
    {
        u32 gameplayVersion = GameplayVersion;
        u32 ticksPerSecond = TicksPerSecond;
        ContentId content{};
    };

    /** Client intent contains no player claim or authoritative execution time. */
    struct MoveCommand
    {
        u64 sequence = 0;
        u64 unit = 0;
        Math::Vec3 target = Math::Vec3::Zero;
    };

    /** Root-unit presentation and gameplay state; catalog resolves through compatible content. */
    struct UnitSnapshot
    {
        u64 unit = 0;
        u32 catalog = 1;
        u32 owner = 0;
        u32 team = 0;
        u32 maximumHealth = 100;
        u32 health = 100;
        f32 movementSpeed = 4.0f;
        Math::Vec3 position = Math::Vec3::Zero;
        Math::Quat rotation = Math::Quat::Identity;
        Math::Vec3 scale = Math::Vec3::One;
        bool selectable = true;
        std::optional<Math::Vec3> movementTarget;
    };

    struct FullSnapshot
    {
        u64 tick = 0;
        /** Highest sequence queued for the recipient, not an execution acknowledgement. */
        u64 lastQueuedSequence = 0;
        std::vector<UnitSnapshot> units;
    };

    struct Message
    {
        u64 session = 0;
        std::variant<Hello, MoveCommand, FullSnapshot> body;
    };

    enum class ErrorCode
    {
        None,
        InvalidArgument,
        Truncated,
        SizeLimit,
        BadMagic,
        UnsupportedVersion,
        UnsupportedMessage,
        InvalidValue,
        NonCanonical,
        CompatibilityMismatch
    };

    struct Error
    {
        ErrorCode code = ErrorCode::None;
        std::size_t offset = 0;
        std::string message;
    };

    /**
     * Validate a native message against the version-1 bounds and canonical ID order.
     * Error is cleared on success. Content compatibility is a separate admission check.
     */
    [[nodiscard]] bool Validate(const Message& message, Error& error);

    /**
     * Encode one exact framed message in little-endian fixed-width fields.
     * IEEE-754 binary32 values preserve signed-zero bits. Outputs publish only on
     * complete success; an error leaves the previous byte buffer unchanged.
     */
    [[nodiscard]] bool Encode(const Message& message, std::vector<u8>& output, Error& error);

    /**
     * Decode exactly one frame, rejecting trailing bytes, unknown/reserved fields,
     * invalid state and excessive sizes/counts before allocating from input counts.
     * The caller supplies readable memory; failure leaves the previous message intact.
     * This codec owns no sockets, scene references, resource handles, or authentication.
     */
    [[nodiscard]] bool Decode(const u8* data, std::size_t size, Message& output, Error& error);

    /**
     * Bounded incremental decoder for one frame at a time. Feed stops at a complete
     * frame even when input contains more frames; consumed identifies the unread tail.
     * A pending message consumes no further bytes until TakeMessage or Reset.
     * Malformed input is terminal until Reset, without publishing a partial message.
     * The caller owns readable input memory; no input pointer survives a Feed call.
     */
    class FrameDecoder final
    {
    public:
        /** Zero-size input may be null; nonzero null input fails without reading it. */
        [[nodiscard]] bool Feed(const u8* data, std::size_t size, std::size_t& consumed, Error& error);
        [[nodiscard]] bool HasMessage() const { return m_message.has_value(); }
        /** Publishes the complete message and starts the next frame; false preserves output. */
        [[nodiscard]] bool TakeMessage(Message& output);
        [[nodiscard]] std::size_t GetBufferedByteCount() const { return m_bytes.size(); }
        /** Remaining header/body bytes, or zero when a message is pending or decoding failed. */
        [[nodiscard]] std::size_t GetBytesNeeded() const;
        /** Discards partial/ready/error state; retained buffer capacity never exceeds the frame cap. */
        void Reset();

    private:
        std::vector<u8> m_bytes;
        std::optional<Message> m_message;
        std::size_t m_expected = HeaderSize;
        Error m_failure;
    };

    /** Exact rules/tick/content comparison; the opaque fingerprint is not authentication. */
    [[nodiscard]] bool ValidateCompatibility(
        const Hello& hello,
        const ContentId& expected,
        Error& error);
}
