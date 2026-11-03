#include <Pyramid/Examples/RTSReference/RTSProtocol.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

namespace Pyramid::Examples::RTSReference::Protocol
{
    namespace
    {
        static_assert(sizeof(f32) == sizeof(u32) && std::numeric_limits<f32>::is_iec559,
            "RTS protocol requires IEEE-754 binary32 floats");
        constexpr std::size_t MinimumUnitBytes = 73;
        constexpr std::size_t TargetBytes = 12;

        bool Fail(Error& error, ErrorCode code, std::size_t offset, const char* message)
        {
            error = {code, offset, message};
            return false;
        }

        bool HasContent(const ContentId& content)
        {
            return std::any_of(content.begin(), content.end(), [](u8 byte) { return byte != 0; });
        }

        bool Bounded(const Math::Vec3& value, f32 maximum)
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) &&
                std::fabs(value.x) <= maximum && std::fabs(value.y) <= maximum &&
                std::fabs(value.z) <= maximum;
        }

        bool ValidateHello(const Hello& hello, Error& error)
        {
            if (hello.gameplayVersion == 0 || hello.ticksPerSecond == 0)
            {
                return Fail(error, ErrorCode::InvalidValue, HeaderSize,
                    "Hello rules version and tick rate must be nonzero");
            }
            if (!HasContent(hello.content))
            {
                return Fail(error, ErrorCode::InvalidValue, HeaderSize + 8,
                    "Hello content fingerprint must be nonzero");
            }
            return true;
        }

        bool ValidateUnit(const UnitSnapshot& unit, std::size_t offset, Error& error)
        {
            if (unit.unit == 0 || unit.catalog == 0)
            {
                return Fail(error, ErrorCode::InvalidValue, offset,
                    "Snapshot unit and catalog identifiers must be nonzero");
            }
            if (unit.maximumHealth == 0 || unit.health > unit.maximumHealth)
            {
                return Fail(error, ErrorCode::InvalidValue, offset + 20,
                    "Snapshot health must be within a nonzero maximum");
            }
            if (!std::isfinite(unit.movementSpeed) || unit.movementSpeed < 0.0f ||
                unit.movementSpeed > MaximumSpeed)
            {
                return Fail(error, ErrorCode::InvalidValue, offset + 28,
                    "Snapshot movement speed exceeds the finite protocol range");
            }
            if (!Bounded(unit.position, MaximumCoordinate))
            {
                return Fail(error, ErrorCode::InvalidValue, offset + 32,
                    "Snapshot position exceeds the finite protocol range");
            }
            const Math::Quat& rotation = unit.rotation;
            const f64 lengthSquared = static_cast<f64>(rotation.x) * rotation.x +
                static_cast<f64>(rotation.y) * rotation.y +
                static_cast<f64>(rotation.z) * rotation.z +
                static_cast<f64>(rotation.w) * rotation.w;
            if (!std::isfinite(lengthSquared) || std::fabs(lengthSquared - 1.0) > 0.001)
            {
                return Fail(error, ErrorCode::InvalidValue, offset + 44,
                    "Snapshot rotation must be a finite normalized quaternion");
            }
            if (!Bounded(unit.scale, MaximumScale))
            {
                return Fail(error, ErrorCode::InvalidValue, offset + 60,
                    "Snapshot scale exceeds the finite protocol range");
            }
            if (unit.movementTarget &&
                (unit.owner == 0 || unit.health == 0 ||
                    !Bounded(*unit.movementTarget, MaximumCoordinate)))
            {
                return Fail(error, ErrorCode::InvalidValue, offset + 72,
                    "Snapshot target requires a live owned unit and bounded coordinates");
            }
            return true;
        }

        class Writer final
        {
        public:
            explicit Writer(std::size_t size) { m_bytes.reserve(size); }
            void Byte(u8 value) { m_bytes.push_back(value); }
            void U16(u16 value)
            {
                for (unsigned shift = 0; shift < 16; shift += 8)
                {
                    Byte(static_cast<u8>(value >> shift));
                }
            }
            void U32(u32 value)
            {
                for (unsigned shift = 0; shift < 32; shift += 8)
                {
                    Byte(static_cast<u8>(value >> shift));
                }
            }
            void U64(u64 value)
            {
                for (unsigned shift = 0; shift < 64; shift += 8)
                {
                    Byte(static_cast<u8>(value >> shift));
                }
            }
            void Float(f32 value)
            {
                u32 bits = 0;
                std::memcpy(&bits, &value, sizeof(bits));
                U32(bits);
            }
            void Vector(const Math::Vec3& value)
            {
                Float(value.x);
                Float(value.y);
                Float(value.z);
            }
            void Rotation(const Math::Quat& value)
            {
                Float(value.x);
                Float(value.y);
                Float(value.z);
                Float(value.w);
            }
            void Publish(std::vector<u8>& output) { output.swap(m_bytes); }

        private:
            std::vector<u8> m_bytes;
        };

        class Reader final
        {
        public:
            Reader(const u8* data, std::size_t size) : m_data(data), m_size(size) {}
            std::size_t Position() const { return m_position; }
            std::size_t Remaining() const { return m_size - m_position; }
            bool Byte(u8& output, Error& error)
            {
                if (!Require(1, error)) return false;
                output = m_data[m_position++];
                return true;
            }
            bool U16(u16& output, Error& error)
            {
                if (!Require(2, error)) return false;
                output = static_cast<u16>(m_data[m_position]) |
                    static_cast<u16>(static_cast<u16>(m_data[m_position + 1]) << 8U);
                m_position += 2;
                return true;
            }
            bool U32(u32& output, Error& error)
            {
                if (!Require(4, error)) return false;
                output = 0;
                for (unsigned shift = 0; shift < 32; shift += 8)
                {
                    output |= static_cast<u32>(m_data[m_position++]) << shift;
                }
                return true;
            }
            bool U64(u64& output, Error& error)
            {
                if (!Require(8, error)) return false;
                output = 0;
                for (unsigned shift = 0; shift < 64; shift += 8)
                {
                    output |= static_cast<u64>(m_data[m_position++]) << shift;
                }
                return true;
            }
            bool Float(f32& output, Error& error)
            {
                u32 bits = 0;
                if (!U32(bits, error)) return false;
                std::memcpy(&output, &bits, sizeof(output));
                return true;
            }
            bool Vector(Math::Vec3& output, Error& error)
            {
                return Float(output.x, error) && Float(output.y, error) && Float(output.z, error);
            }
            bool Rotation(Math::Quat& output, Error& error)
            {
                return Float(output.x, error) && Float(output.y, error) &&
                    Float(output.z, error) && Float(output.w, error);
            }

        private:
            bool Require(std::size_t bytes, Error& error) const
            {
                return bytes <= Remaining() || Fail(error, ErrorCode::Truncated, m_position,
                    "Protocol field is truncated");
            }

            const u8* m_data;
            std::size_t m_size;
            std::size_t m_position = 0;
        };

        struct FrameHeader
        {
            u8 kind = 0;
            u32 payloadSize = 0;
            u64 session = 0;
        };

        bool ReadHeader(Reader& reader, FrameHeader& header, Error& error)
        {
            u32 magic = 0;
            u16 version = 0;
            u8 reserved = 0;
            if (!reader.U32(magic, error) || !reader.U16(version, error) ||
                !reader.Byte(header.kind, error) || !reader.Byte(reserved, error) ||
                !reader.U32(header.payloadSize, error) || !reader.U64(header.session, error))
            {
                return false;
            }
            if (magic != 0x53545250U)
            {
                return Fail(error, ErrorCode::BadMagic, 0, "Protocol magic must be PRTS");
            }
            if (version != Version)
            {
                return Fail(error, ErrorCode::UnsupportedVersion, 4, "Unsupported protocol version");
            }
            if (header.kind < static_cast<u8>(MessageKind::Hello) ||
                header.kind > static_cast<u8>(MessageKind::FullSnapshot))
            {
                return Fail(error, ErrorCode::UnsupportedMessage, 6, "Unsupported protocol message kind");
            }
            if (reserved != 0)
            {
                return Fail(error, ErrorCode::InvalidValue, 7, "Reserved header flags must be zero");
            }
            if (header.payloadSize > MaximumFrameBytes - HeaderSize)
            {
                return Fail(error, ErrorCode::SizeLimit, 8, "Declared payload exceeds the protocol limit");
            }
            if (header.session == 0)
            {
                return Fail(error, ErrorCode::InvalidValue, 12, "Session identifier must be nonzero");
            }
            return true;
        }

        std::size_t PayloadSize(const Message& message)
        {
            if (std::holds_alternative<Hello>(message.body)) return 24;
            if (std::holds_alternative<MoveCommand>(message.body)) return 28;
            std::size_t size = 20;
            for (const UnitSnapshot& unit : std::get<FullSnapshot>(message.body).units)
            {
                size += MinimumUnitBytes + (unit.movementTarget ? TargetBytes : 0);
            }
            return size;
        }
    }

    bool Validate(const Message& message, Error& error)
    {
        if (message.session == 0)
        {
            return Fail(error, ErrorCode::InvalidValue, 12, "Session identifier must be nonzero");
        }
        if (message.body.valueless_by_exception())
        {
            return Fail(error, ErrorCode::InvalidValue, 6, "Message has no active payload");
        }
        if (const auto* hello = std::get_if<Hello>(&message.body))
        {
            if (!ValidateHello(*hello, error)) return false;
        }
        else if (const auto* command = std::get_if<MoveCommand>(&message.body))
        {
            if (command->sequence == 0 || command->unit == 0)
            {
                return Fail(error, ErrorCode::InvalidValue, HeaderSize,
                    "Move sequence and unit identifiers must be nonzero");
            }
            if (!Bounded(command->target, MaximumCoordinate))
            {
                return Fail(error, ErrorCode::InvalidValue, HeaderSize + 16,
                    "Move target exceeds the finite protocol range");
            }
        }
        else
        {
            const auto& snapshot = std::get<FullSnapshot>(message.body);
            if (snapshot.units.size() > MaximumUnits)
            {
                return Fail(error, ErrorCode::SizeLimit, HeaderSize + 16,
                    "Snapshot unit count exceeds the protocol limit");
            }
            u64 previous = 0;
            std::size_t offset = HeaderSize + 20;
            for (const UnitSnapshot& unit : snapshot.units)
            {
                if (unit.unit <= previous)
                {
                    return Fail(error, ErrorCode::NonCanonical, offset,
                        "Snapshot unit identifiers must be strictly increasing and nonzero");
                }
                if (!ValidateUnit(unit, offset, error)) return false;
                previous = unit.unit;
                offset += MinimumUnitBytes + (unit.movementTarget ? TargetBytes : 0);
            }
        }
        if (PayloadSize(message) > MaximumFrameBytes - HeaderSize)
        {
            return Fail(error, ErrorCode::SizeLimit, 8, "Encoded frame exceeds the protocol limit");
        }
        error = {};
        return true;
    }

    bool Encode(const Message& message, std::vector<u8>& output, Error& error)
    {
        if (!Validate(message, error)) return false;
        const std::size_t payloadSize = PayloadSize(message);
        Writer writer(HeaderSize + payloadSize);
        writer.Byte('P');
        writer.Byte('R');
        writer.Byte('T');
        writer.Byte('S');
        writer.U16(Version);
        writer.Byte(static_cast<u8>(message.body.index() + 1));
        writer.Byte(0);
        writer.U32(static_cast<u32>(payloadSize));
        writer.U64(message.session);
        if (const auto* hello = std::get_if<Hello>(&message.body))
        {
            writer.U32(hello->gameplayVersion);
            writer.U32(hello->ticksPerSecond);
            for (u8 byte : hello->content) writer.Byte(byte);
        }
        else if (const auto* command = std::get_if<MoveCommand>(&message.body))
        {
            writer.U64(command->sequence);
            writer.U64(command->unit);
            writer.Vector(command->target);
        }
        else
        {
            const auto& snapshot = std::get<FullSnapshot>(message.body);
            writer.U64(snapshot.tick);
            writer.U64(snapshot.lastQueuedSequence);
            writer.U32(static_cast<u32>(snapshot.units.size()));
            for (const UnitSnapshot& unit : snapshot.units)
            {
                writer.U64(unit.unit);
                writer.U32(unit.catalog);
                writer.U32(unit.owner);
                writer.U32(unit.team);
                writer.U32(unit.maximumHealth);
                writer.U32(unit.health);
                writer.Float(unit.movementSpeed);
                writer.Vector(unit.position);
                writer.Rotation(unit.rotation);
                writer.Vector(unit.scale);
                writer.Byte(static_cast<u8>((unit.selectable ? 1U : 0U) |
                    (unit.movementTarget ? 2U : 0U)));
                if (unit.movementTarget) writer.Vector(*unit.movementTarget);
            }
        }
        writer.Publish(output);
        error = {};
        return true;
    }

    bool Decode(const u8* data, std::size_t size, Message& output, Error& error)
    {
        if (size > MaximumFrameBytes)
        {
            return Fail(error, ErrorCode::SizeLimit, 0, "Input frame exceeds the protocol limit");
        }
        if (!data)
        {
            return Fail(error, ErrorCode::InvalidArgument, 0, "Input frame pointer is null");
        }
        if (size < HeaderSize)
        {
            return Fail(error, ErrorCode::Truncated, size, "Protocol header is truncated");
        }
        Reader reader(data, size);
        FrameHeader header;
        if (!ReadHeader(reader, header, error)) return false;
        if (header.payloadSize > reader.Remaining())
        {
            return Fail(error, ErrorCode::Truncated, size, "Declared protocol payload is truncated");
        }
        if (header.payloadSize < reader.Remaining())
        {
            return Fail(error, ErrorCode::InvalidValue, HeaderSize + header.payloadSize,
                "Trailing bytes after protocol frame");
        }
        const u8 kind = header.kind;
        Message parsed;
        parsed.session = header.session;

        if (kind == static_cast<u8>(MessageKind::Hello))
        {
            Hello hello;
            if (!reader.U32(hello.gameplayVersion, error) ||
                !reader.U32(hello.ticksPerSecond, error)) return false;
            for (u8& byte : hello.content)
            {
                if (!reader.Byte(byte, error)) return false;
            }
            parsed.body = hello;
        }
        else if (kind == static_cast<u8>(MessageKind::MoveCommand))
        {
            MoveCommand command;
            if (!reader.U64(command.sequence, error) || !reader.U64(command.unit, error) ||
                !reader.Vector(command.target, error)) return false;
            parsed.body = command;
        }
        else
        {
            FullSnapshot snapshot;
            u32 count = 0;
            if (!reader.U64(snapshot.tick, error) ||
                !reader.U64(snapshot.lastQueuedSequence, error) || !reader.U32(count, error))
            {
                return false;
            }
            if (count > MaximumUnits)
            {
                return Fail(error, ErrorCode::SizeLimit, HeaderSize + 16,
                    "Snapshot unit count exceeds the protocol limit");
            }
            if (count > reader.Remaining() / MinimumUnitBytes)
            {
                return Fail(error, ErrorCode::Truncated, reader.Position(),
                    "Snapshot records are truncated before allocation");
            }
            snapshot.units.reserve(count);
            for (u32 index = 0; index < count; ++index)
            {
                UnitSnapshot unit;
                u8 flags = 0;
                if (!reader.U64(unit.unit, error) || !reader.U32(unit.catalog, error) ||
                    !reader.U32(unit.owner, error) || !reader.U32(unit.team, error) ||
                    !reader.U32(unit.maximumHealth, error) || !reader.U32(unit.health, error) ||
                    !reader.Float(unit.movementSpeed, error) || !reader.Vector(unit.position, error) ||
                    !reader.Rotation(unit.rotation, error) || !reader.Vector(unit.scale, error) ||
                    !reader.Byte(flags, error))
                {
                    return false;
                }
                if (flags > 3)
                {
                    return Fail(error, ErrorCode::InvalidValue, reader.Position() - 1,
                        "Reserved unit flags must be zero");
                }
                unit.selectable = (flags & 1U) != 0;
                if ((flags & 2U) != 0)
                {
                    Math::Vec3 target;
                    if (!reader.Vector(target, error)) return false;
                    unit.movementTarget = target;
                }
                snapshot.units.push_back(unit);
            }
            parsed.body = std::move(snapshot);
        }
        if (reader.Remaining() != 0)
        {
            return Fail(error, ErrorCode::InvalidValue, reader.Position(),
                "Trailing bytes inside protocol payload");
        }
        if (!Validate(parsed, error)) return false;
        output = std::move(parsed);
        error = {};
        return true;
    }

    bool FrameDecoder::Feed(
        const u8* data, std::size_t size, std::size_t& consumed, Error& error)
    {
        consumed = 0;
        if (m_failure.code != ErrorCode::None)
        {
            error = m_failure;
            return false;
        }
        if (!data && size != 0)
        {
            m_message.reset();
            Fail(m_failure, ErrorCode::InvalidArgument, m_bytes.size(),
                "Incremental frame input pointer is null");
            error = m_failure;
            return false;
        }
        if (m_message)
        {
            error = {};
            return true;
        }
        while (consumed < size)
        {
            const std::size_t amount = (std::min)(GetBytesNeeded(), size - consumed);
            m_bytes.insert(m_bytes.end(), data + consumed, data + consumed + amount);
            consumed += amount;
            if (m_expected == HeaderSize && m_bytes.size() == HeaderSize)
            {
                Reader reader(m_bytes.data(), m_bytes.size());
                FrameHeader header;
                if (!ReadHeader(reader, header, m_failure))
                {
                    error = m_failure;
                    return false;
                }
                m_expected = HeaderSize + header.payloadSize;
                // Allocate only after the complete header passed the shared bounds.
                m_bytes.reserve(m_expected);
            }
            if (m_bytes.size() == m_expected)
            {
                Message parsed;
                if (!Decode(m_bytes.data(), m_bytes.size(), parsed, m_failure))
                {
                    error = m_failure;
                    return false;
                }
                m_message = std::move(parsed);
                error = {};
                return true;
            }
        }
        error = {};
        return true;
    }

    bool FrameDecoder::TakeMessage(Message& output)
    {
        if (!m_message)
        {
            return false;
        }
        output = std::move(*m_message);
        m_message.reset();
        m_bytes.clear();
        m_expected = HeaderSize;
        return true;
    }

    std::size_t FrameDecoder::GetBytesNeeded() const
    {
        return m_message || m_failure.code != ErrorCode::None
            ? 0
            : m_expected - m_bytes.size();
    }

    void FrameDecoder::Reset()
    {
        m_bytes.clear();
        m_message.reset();
        m_expected = HeaderSize;
        m_failure = {};
    }

    bool ValidateCompatibility(const Hello& hello, const ContentId& expected, Error& error)
    {
        if (!HasContent(expected))
        {
            return Fail(error, ErrorCode::InvalidArgument, 0,
                "Expected content fingerprint must be nonzero");
        }
        if (!ValidateHello(hello, error)) return false;
        if (hello.gameplayVersion != GameplayVersion || hello.ticksPerSecond != TicksPerSecond ||
            hello.content != expected)
        {
            return Fail(error, ErrorCode::CompatibilityMismatch, HeaderSize,
                "Peer rules version, tick rate, or content fingerprint is incompatible");
        }
        error = {};
        return true;
    }
}
