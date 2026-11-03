#include <Pyramid/Examples/RTSReference/RTSProtocol.hpp>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <variant>
#include <vector>

namespace
{
    using namespace Pyramid;
    namespace Protocol = Pyramid::Examples::RTSReference::Protocol;

    void Require(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "RTS protocol test failed: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    u32 FloatBits(f32 value)
    {
        u32 bits = 0;
        static_assert(sizeof(bits) == sizeof(value));
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }

    bool SameVector(const Math::Vec3& left, const Math::Vec3& right)
    {
        return FloatBits(left.x) == FloatBits(right.x) &&
            FloatBits(left.y) == FloatBits(right.y) && FloatBits(left.z) == FloatBits(right.z);
    }

    Protocol::ContentId Content()
    {
        Protocol::ContentId result{};
        for (std::size_t index = 0; index < result.size(); ++index)
        {
            result[index] = static_cast<u8>(index + 1);
        }
        return result;
    }

    Protocol::Message MoveMessage()
    {
        Protocol::Message message;
        message.session = 0x0102030405060708ULL;
        message.body = Protocol::MoveCommand{
            0x1112131415161718ULL, 0x2122232425262728ULL, Math::Vec3(1.0f, -2.0f, 0.5f)};
        return message;
    }

    Protocol::Message HelloMessage()
    {
        Protocol::Message message;
        message.session = 0x0102030405060708ULL;
        Protocol::Hello hello;
        hello.content = Content();
        message.body = hello;
        return message;
    }

    Protocol::UnitSnapshot Unit(u64 id)
    {
        Protocol::UnitSnapshot unit;
        unit.unit = id;
        unit.catalog = 2;
        unit.owner = 1;
        unit.team = 8;
        unit.maximumHealth = 100;
        unit.health = 75;
        unit.movementSpeed = 4.0f;
        unit.position = Math::Vec3(1.0f, 2.0f, 3.0f);
        unit.scale = Math::Vec3(1.0f, -2.0f, 0.0f);
        unit.movementTarget = Math::Vec3(9.0f, 8.0f, 7.0f);
        return unit;
    }

    Protocol::Message SnapshotMessage()
    {
        Protocol::Message message;
        message.session = 7;
        Protocol::FullSnapshot snapshot;
        snapshot.tick = 123;
        snapshot.lastQueuedSequence = 18;
        snapshot.units.push_back(Unit(1));
        message.body = snapshot;
        return message;
    }

    void RequireEqual(const Protocol::Message& expected, const Protocol::Message& actual)
    {
        Require(expected.session == actual.session && expected.body.index() == actual.body.index(),
            "round trip changed session or message kind");
        if (const auto* hello = std::get_if<Protocol::Hello>(&expected.body))
        {
            const auto& result = std::get<Protocol::Hello>(actual.body);
            Require(hello->gameplayVersion == result.gameplayVersion &&
                    hello->ticksPerSecond == result.ticksPerSecond && hello->content == result.content,
                "round trip changed compatibility information");
        }
        else if (const auto* move = std::get_if<Protocol::MoveCommand>(&expected.body))
        {
            const auto& result = std::get<Protocol::MoveCommand>(actual.body);
            Require(move->sequence == result.sequence && move->unit == result.unit &&
                    SameVector(move->target, result.target), "round trip changed a move command");
        }
        else
        {
            const auto& snapshot = std::get<Protocol::FullSnapshot>(expected.body);
            const auto& result = std::get<Protocol::FullSnapshot>(actual.body);
            Require(snapshot.tick == result.tick && snapshot.lastQueuedSequence == result.lastQueuedSequence &&
                    snapshot.units.size() == result.units.size(), "round trip changed snapshot metadata");
            for (std::size_t index = 0; index < snapshot.units.size(); ++index)
            {
                const auto& unit = snapshot.units[index];
                const auto& decoded = result.units[index];
                Require(unit.unit == decoded.unit && unit.catalog == decoded.catalog &&
                        unit.owner == decoded.owner && unit.team == decoded.team &&
                        unit.maximumHealth == decoded.maximumHealth && unit.health == decoded.health &&
                        FloatBits(unit.movementSpeed) == FloatBits(decoded.movementSpeed) &&
                        SameVector(unit.position, decoded.position) && SameVector(unit.scale, decoded.scale) &&
                        FloatBits(unit.rotation.x) == FloatBits(decoded.rotation.x) &&
                        FloatBits(unit.rotation.y) == FloatBits(decoded.rotation.y) &&
                        FloatBits(unit.rotation.z) == FloatBits(decoded.rotation.z) &&
                        FloatBits(unit.rotation.w) == FloatBits(decoded.rotation.w) &&
                        unit.selectable == decoded.selectable &&
                        unit.movementTarget.has_value() == decoded.movementTarget.has_value(),
                    "round trip changed snapshot unit " + std::to_string(index));
                if (unit.movementTarget)
                {
                    Require(SameVector(*unit.movementTarget, *decoded.movementTarget),
                        "round trip changed optional movement target");
                }
            }
        }
    }

    std::vector<u8> Encode(const Protocol::Message& message)
    {
        std::vector<u8> result;
        Protocol::Error error{Protocol::ErrorCode::InvalidValue, 99, "old error"};
        const bool encoded = Protocol::Encode(message, result, error);
        Require(encoded, "valid encoding failed: " + error.message);
        Require(error.code == Protocol::ErrorCode::None && error.message.empty(),
            "successful encode retained an old diagnostic");
        return result;
    }

    void RoundTrip(const Protocol::Message& message)
    {
        const auto bytes = Encode(message);
        Protocol::Message decoded = MoveMessage();
        Protocol::Error error{Protocol::ErrorCode::InvalidValue, 99, "old error"};
        const bool decodedSuccessfully = Protocol::Decode(bytes.data(), bytes.size(), decoded, error);
        Require(decodedSuccessfully, "valid decoding failed: " + error.message);
        Require(error.code == Protocol::ErrorCode::None && error.message.empty(),
            "successful decode retained an old diagnostic");
        RequireEqual(message, decoded);
        Require(Encode(decoded) == bytes, "canonical round trip changed encoded bytes");
    }

    void RequireDecodeFailure(const u8* data, std::size_t size, const std::string& context)
    {
        Protocol::Message output = MoveMessage();
        Protocol::Error error;
        Require(!Protocol::Decode(data, size, output, error), context + " was accepted");
        Require(error.code != Protocol::ErrorCode::None && !error.message.empty(),
            context + " failed without an actionable diagnostic");
        RequireEqual(MoveMessage(), output);
    }

    void RequireDecodeFailure(const std::vector<u8>& bytes, const std::string& context)
    {
        RequireDecodeFailure(bytes.data(), bytes.size(), context);
    }

    void RequireEncodeFailure(const Protocol::Message& message, const std::string& context)
    {
        const std::vector<u8> sentinel{0xA5, 0x5A, 0xCC};
        std::vector<u8> output = sentinel;
        Protocol::Error error;
        Require(!Protocol::Encode(message, output, error) && output == sentinel,
            context + " was encoded or changed the caller's output");
        Require(error.code != Protocol::ErrorCode::None && !error.message.empty(),
            context + " encoding failed without a diagnostic");
        Require(!Protocol::Validate(message, error) && error.code != Protocol::ErrorCode::None &&
                !error.message.empty(), context + " escaped standalone validation");
    }

    void SetU32(std::vector<u8>& bytes, std::size_t offset, u32 value)
    {
        Require(offset + 4 <= bytes.size(), "malformed fixture write exceeded its buffer");
        for (std::size_t index = 0; index < 4; ++index)
        {
            bytes[offset + index] = static_cast<u8>(value >> (index * 8));
        }
    }

    void TestGoldenBytesAndCompatibility()
    {
        const std::vector<u8> expectedMove{
            0x50, 0x52, 0x54, 0x53, 0x01, 0x00, 0x02, 0x00, 0x1C, 0x00, 0x00, 0x00,
            0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
            0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11,
            0x28, 0x27, 0x26, 0x25, 0x24, 0x23, 0x22, 0x21,
            0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x3F};
        const std::vector<u8> expectedHello{
            0x50, 0x52, 0x54, 0x53, 0x01, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00,
            0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
            0x01, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
            0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
        Require(Encode(MoveMessage()) == expectedMove && Encode(HelloMessage()) == expectedHello,
            "wire bytes differ from independent little-endian fixtures");
        Require(Encode(MoveMessage()) == Encode(MoveMessage()), "repeat encoding is not stable");
        RoundTrip(MoveMessage());
        RoundTrip(HelloMessage());
        RoundTrip(SnapshotMessage());
        Protocol::Message empty;
        empty.session = 1;
        empty.body = Protocol::FullSnapshot{};
        RoundTrip(empty);
        Protocol::Error error{Protocol::ErrorCode::InvalidValue, 99, "old error"};
        Require(Protocol::Validate(HelloMessage(), error) && error.code == Protocol::ErrorCode::None &&
                error.message.empty(), "successful standalone validation retained a diagnostic");
        auto hello = std::get<Protocol::Hello>(HelloMessage().body);
        Require(Protocol::ValidateCompatibility(hello, Content(), error) &&
                error.code == Protocol::ErrorCode::None && error.message.empty(),
            "matching compatibility identity was rejected");
        for (unsigned int variation = 0; variation < 3; ++variation)
        {
            auto mismatch = hello;
            if (variation == 0)
            {
                ++mismatch.gameplayVersion;
            }
            if (variation == 1)
            {
                ++mismatch.ticksPerSecond;
            }
            if (variation == 2)
            {
                mismatch.content[3] ^= 0x80;
            }
            Require(!Protocol::ValidateCompatibility(mismatch, Content(), error) &&
                    error.code == Protocol::ErrorCode::CompatibilityMismatch && !error.message.empty(),
                "incompatible gameplay, tick rate, or content was accepted");
        }
        Require(!Protocol::ValidateCompatibility(hello, Protocol::ContentId{}, error) &&
                error.code != Protocol::ErrorCode::None && !error.message.empty(),
            "an empty local content identity was accepted");
        auto invalid = HelloMessage();
        std::get<Protocol::Hello>(invalid.body).content = {};
        RequireEncodeFailure(invalid, "empty content fingerprint");
        invalid = HelloMessage();
        std::get<Protocol::Hello>(invalid.body).gameplayVersion = 0;
        RequireEncodeFailure(invalid, "zero gameplay version");
        invalid = HelloMessage();
        std::get<Protocol::Hello>(invalid.body).ticksPerSecond = 0;
        RequireEncodeFailure(invalid, "zero tick rate");
    }

    void TestFramingFailures()
    {
        for (const auto& message : {HelloMessage(), MoveMessage(), SnapshotMessage()})
        {
            const auto bytes = Encode(message);
            for (std::size_t prefix = 0; prefix < bytes.size(); ++prefix)
            {
                RequireDecodeFailure(bytes.data(), prefix, "truncated prefix " + std::to_string(prefix));
            }
        }
        RequireDecodeFailure(nullptr, 0, "null empty frame");
        RequireDecodeFailure(nullptr, Protocol::HeaderSize, "null nonempty frame");
        RequireDecodeFailure(std::vector<u8>(Protocol::MaximumFrameBytes + 1), "oversized physical frame");
        const auto original = Encode(MoveMessage());
        auto bad = original;
        bad[0] = 'X';
        RequireDecodeFailure(bad, "wrong magic");
        bad = original;
        bad[4] = 2;
        RequireDecodeFailure(bad, "unsupported protocol version");
        bad = original;
        bad[6] = 99;
        RequireDecodeFailure(bad, "unknown message kind");
        bad = original;
        bad[7] = 1;
        RequireDecodeFailure(bad, "reserved header bits");
        bad = original;
        SetU32(bad, 8, 0);
        RequireDecodeFailure(bad, "short advertised payload");
        bad = original;
        SetU32(bad, 8, std::numeric_limits<u32>::max());
        RequireDecodeFailure(bad, "overflowing advertised payload");
        bad = original;
        bad.push_back(0);
        RequireDecodeFailure(bad, "trailing bytes outside payload");
        SetU32(bad, 8, 29);
        RequireDecodeFailure(bad, "trailing bytes inside move payload");
        bad = original;
        for (std::size_t index = 12; index < 20; ++index)
        {
            bad[index] = 0;
        }
        RequireDecodeFailure(bad, "zero session in frame");
        auto invalid = MoveMessage();
        invalid.session = 0;
        RequireEncodeFailure(invalid, "zero session");
        invalid = MoveMessage();
        std::get<Protocol::MoveCommand>(invalid.body).sequence = 0;
        RequireEncodeFailure(invalid, "zero command sequence");
        invalid = MoveMessage();
        std::get<Protocol::MoveCommand>(invalid.body).unit = 0;
        RequireEncodeFailure(invalid, "zero network unit");
        Protocol::Message empty;
        empty.session = 1;
        empty.body = Protocol::FullSnapshot{};
        bad = Encode(empty);
        SetU32(bad, 36, std::numeric_limits<u32>::max());
        RequireDecodeFailure(bad, "malicious snapshot count");
        bad = Encode(SnapshotMessage());
        bad[112] = 0x80;
        RequireDecodeFailure(bad, "unknown unit flag");
        bad = Encode(SnapshotMessage());
        SetU32(bad, 96, 0); // A zero quaternion is not a valid orientation.
        RequireDecodeFailure(bad, "zero quaternion from wire");
    }

    void TestStateValidation()
    {
        const f32 invalidNumbers[] = {
            std::numeric_limits<f32>::quiet_NaN(), std::numeric_limits<f32>::infinity(),
            -std::numeric_limits<f32>::infinity()};
        for (f32 value : invalidNumbers)
        {
            for (std::size_t axis = 0; axis < 3; ++axis)
            {
                auto invalid = MoveMessage();
                auto& target = std::get<Protocol::MoveCommand>(invalid.body).target;
                if (axis == 0)
                {
                    target.x = value;
                }
                if (axis == 1)
                {
                    target.y = value;
                }
                if (axis == 2)
                {
                    target.z = value;
                }
                RequireEncodeFailure(invalid, "nonfinite move target");
                auto wire = Encode(MoveMessage());
                SetU32(wire, 36 + axis * 4, FloatBits(value));
                RequireDecodeFailure(wire, "nonfinite wire move target");
                invalid = SnapshotMessage();
                auto& unit = std::get<Protocol::FullSnapshot>(invalid.body).units.front();
                if (axis == 0)
                {
                    unit.position.x = value;
                }
                if (axis == 1)
                {
                    unit.position.y = value;
                }
                if (axis == 2)
                {
                    unit.position.z = value;
                }
                RequireEncodeFailure(invalid, "nonfinite snapshot position");
                invalid = SnapshotMessage();
                auto& scale = std::get<Protocol::FullSnapshot>(invalid.body).units.front().scale;
                if (axis == 0)
                {
                    scale.x = value;
                }
                if (axis == 1)
                {
                    scale.y = value;
                }
                if (axis == 2)
                {
                    scale.z = value;
                }
                RequireEncodeFailure(invalid, "nonfinite snapshot scale");
                invalid = SnapshotMessage();
                auto& snapshotTarget = *std::get<Protocol::FullSnapshot>(invalid.body).units.front().movementTarget;
                if (axis == 0)
                {
                    snapshotTarget.x = value;
                }
                if (axis == 1)
                {
                    snapshotTarget.y = value;
                }
                if (axis == 2)
                {
                    snapshotTarget.z = value;
                }
                RequireEncodeFailure(invalid, "nonfinite snapshot movement target");
            }
        }
        for (unsigned int variation = 0; variation < 14; ++variation)
        {
            auto invalid = SnapshotMessage();
            auto& unit = std::get<Protocol::FullSnapshot>(invalid.body).units.front();
            switch (variation)
            {
                case 0: unit.unit = 0; break;
                case 1: unit.catalog = 0; break;
                case 2: unit.maximumHealth = 0; break;
                case 3: unit.health = unit.maximumHealth + 1; break;
                case 4: unit.health = 0; break;
                case 5: unit.owner = 0; break;
                case 6: unit.movementSpeed = -1.0f; break;
                case 7: unit.movementSpeed = Protocol::MaximumSpeed + 1.0f; break;
                case 8: unit.movementSpeed = std::numeric_limits<f32>::quiet_NaN(); break;
                case 9: unit.position.x = Protocol::MaximumCoordinate + 1.0f; break;
                case 10: unit.movementTarget->z = -Protocol::MaximumCoordinate - 1.0f; break;
                case 11: unit.scale.z = Protocol::MaximumScale + 1.0f; break;
                case 12: unit.rotation.w = 2.0f; break;
                case 13: unit.rotation.x = std::numeric_limits<f32>::infinity(); break;
            }
            RequireEncodeFailure(invalid, "invalid snapshot state case " + std::to_string(variation));
        }
        auto invalid = SnapshotMessage();
        auto& units = std::get<Protocol::FullSnapshot>(invalid.body).units;
        units.push_back(units.front());
        RequireEncodeFailure(invalid, "duplicate network IDs");
        units.front().unit = 2;
        RequireEncodeFailure(invalid, "out-of-order network IDs");
        auto boundary = SnapshotMessage();
        auto& unit = std::get<Protocol::FullSnapshot>(boundary.body).units.front();
        unit.position = Math::Vec3(Protocol::MaximumCoordinate, -Protocol::MaximumCoordinate, -0.0f);
        unit.scale = Math::Vec3(Protocol::MaximumScale, -Protocol::MaximumScale, -0.0f);
        unit.movementSpeed = Protocol::MaximumSpeed;
        unit.movementTarget = Math::Vec3(-Protocol::MaximumCoordinate, Protocol::MaximumCoordinate, 0.0f);
        RoundTrip(boundary);
        Protocol::Message decoded;
        Protocol::Error error;
        const auto bytes = Encode(boundary);
        Require(Protocol::Decode(bytes.data(), bytes.size(), decoded, error) &&
                std::signbit(std::get<Protocol::FullSnapshot>(decoded.body).units.front().position.z) &&
                std::signbit(std::get<Protocol::FullSnapshot>(decoded.body).units.front().scale.z),
            "snapshot encoding must preserve signed-zero bits");
        unit.health = 0;
        unit.owner = 0;
        unit.movementTarget.reset();
        RoundTrip(boundary);
    }

    void TestIncrementalFraming()
    {
        Protocol::FrameDecoder decoder;
        Protocol::Error error{Protocol::ErrorCode::InvalidValue, 99, "old error"};
        std::size_t consumed = 99;
        Protocol::Message output = MoveMessage();
        Require(!decoder.HasMessage() && decoder.GetBufferedByteCount() == 0 &&
                decoder.GetBytesNeeded() == Protocol::HeaderSize && !decoder.TakeMessage(output),
            "new incremental decoder must await one header and preserve absent output");
        RequireEqual(MoveMessage(), output);
        Require(decoder.Feed(nullptr, 0, consumed, error) && consumed == 0 &&
                error.code == Protocol::ErrorCode::None && error.message.empty(),
            "empty incremental input must consume nothing and clear old diagnostics");

        for (const auto& message : {HelloMessage(), MoveMessage(), SnapshotMessage()})
        {
            const auto bytes = Encode(message);
            const std::size_t splits[] = {1, 19, 20, 21, bytes.size() - 1};
            for (std::size_t split : splits)
            {
                decoder.Reset();
                Require(decoder.Feed(bytes.data(), split, consumed, error) && consumed == split &&
                        !decoder.HasMessage() && decoder.GetBufferedByteCount() == split &&
                        decoder.GetBytesNeeded() ==
                            (split < Protocol::HeaderSize ? Protocol::HeaderSize - split : bytes.size() - split),
                    "fragmented header/body did not retain its exact bounded progress");
                Require(decoder.Feed(bytes.data() + split, bytes.size() - split, consumed, error) &&
                        consumed == bytes.size() - split && decoder.HasMessage() &&
                        decoder.GetBytesNeeded() == 0 && decoder.GetBufferedByteCount() == bytes.size(),
                    "fragmented frame did not become ready after its final byte");
                Require(decoder.Feed(bytes.data(), bytes.size(), consumed, error) && consumed == 0 &&
                        decoder.HasMessage(), "pending message must not consume following frame bytes");
                Require(decoder.TakeMessage(output), "complete incremental message could not be taken");
                RequireEqual(message, output);
                Require(!decoder.HasMessage() && decoder.GetBufferedByteCount() == 0 &&
                        decoder.GetBytesNeeded() == Protocol::HeaderSize && !decoder.TakeMessage(output),
                    "taking a message must reset framing and preserve output on the next empty take");
                RequireEqual(message, output);
            }
            decoder.Reset();
            for (std::size_t offset = 0; offset < bytes.size(); ++offset)
            {
                Require(decoder.Feed(bytes.data() + offset, 1, consumed, error) && consumed == 1 &&
                        decoder.HasMessage() == (offset + 1 == bytes.size()),
                    "one-byte framing emitted early, stalled, or discarded input");
            }
            Require(decoder.TakeMessage(output), "one-byte framed message was not published");
            RequireEqual(message, output);
        }

        const auto first = Encode(HelloMessage());
        const auto second = Encode(MoveMessage());
        const auto third = Encode(SnapshotMessage());
        std::vector<u8> coalesced = first;
        coalesced.insert(coalesced.end(), second.begin(), second.end());
        coalesced.insert(coalesced.end(), third.begin(), third.end());
        decoder.Reset();
        Require(decoder.Feed(coalesced.data(), coalesced.size(), consumed, error) &&
                consumed == first.size() && decoder.TakeMessage(output),
            "coalesced input must stop at its first exact frame boundary");
        RequireEqual(HelloMessage(), output);
        std::size_t offset = consumed;
        Require(decoder.Feed(coalesced.data() + offset, coalesced.size() - offset, consumed, error) &&
                consumed == second.size() && decoder.TakeMessage(output),
            "coalesced unread tail must preserve its next complete frame");
        RequireEqual(MoveMessage(), output);
        offset += consumed;
        Require(decoder.Feed(coalesced.data() + offset, 19, consumed, error) && consumed == 19 &&
                !decoder.HasMessage() && decoder.GetBufferedByteCount() == 19 &&
                decoder.GetBytesNeeded() == 1,
            "complete frames followed by an incomplete header lost their suffix");
        offset += consumed;
        Require(decoder.Feed(coalesced.data() + offset, coalesced.size() - offset, consumed, error) &&
                consumed == third.size() - 19 && decoder.TakeMessage(output),
            "coalesced partial suffix did not recover its complete frame");
        RequireEqual(SnapshotMessage(), output);
    }

    void TestIncrementalFailures()
    {
        const auto valid = Encode(MoveMessage());
        for (unsigned int variation = 0; variation < 7; ++variation)
        {
            auto header = valid;
            header.resize(Protocol::HeaderSize);
            switch (variation)
            {
                case 0: header[0] = 'X'; break;
                case 1: header[4] = 2; break;
                case 2: header[6] = 99; break;
                case 3: header[7] = 1; break;
                case 4:
                    for (std::size_t index = 12; index < 20; ++index)
                    {
                        header[index] = 0;
                    }
                    break;
                case 5: SetU32(header, 8, std::numeric_limits<u32>::max()); break;
                case 6: SetU32(header, 8, 0); break;
            }
            Protocol::FrameDecoder decoder;
            Protocol::Error error;
            std::size_t consumed = 99;
            Protocol::Message output = MoveMessage();
            Require(!decoder.Feed(header.data(), header.size(), consumed, error) &&
                    consumed == Protocol::HeaderSize && error.code != Protocol::ErrorCode::None &&
                    !error.message.empty() && !decoder.HasMessage() &&
                    decoder.GetBufferedByteCount() == Protocol::HeaderSize &&
                    decoder.GetBytesNeeded() == 0 && !decoder.TakeMessage(output),
                "hostile header must fail before payload buffering and without partial publication");
            RequireEqual(MoveMessage(), output);
            const auto code = error.code;
            Require(!decoder.Feed(valid.data(), valid.size(), consumed, error) && consumed == 0 &&
                    error.code == code && !error.message.empty(),
                "failed decoder must remain terminal until explicit reset");
            decoder.Reset();
            Require(decoder.GetBufferedByteCount() == 0 &&
                    decoder.GetBytesNeeded() == Protocol::HeaderSize &&
                    decoder.Feed(valid.data(), valid.size(), consumed, error) &&
                    consumed == valid.size() && decoder.TakeMessage(output),
                "reset must discard failed frame state and accept a fresh valid frame");
            RequireEqual(MoveMessage(), output);
        }

        Protocol::FrameDecoder decoder;
        Protocol::Error error;
        std::size_t consumed = 99;
        Require(!decoder.Feed(nullptr, 1, consumed, error) && consumed == 0 &&
                error.code == Protocol::ErrorCode::InvalidArgument && !error.message.empty() &&
                !decoder.HasMessage() && decoder.GetBytesNeeded() == 0,
            "nonempty null input must fail without reading memory or publishing a message");
        decoder.Reset();
        auto malformed = valid;
        SetU32(malformed, 36, FloatBits(std::numeric_limits<f32>::quiet_NaN()));
        Require(decoder.Feed(malformed.data(), Protocol::HeaderSize, consumed, error) &&
                consumed == Protocol::HeaderSize && !decoder.HasMessage() &&
                decoder.GetBytesNeeded() == malformed.size() - Protocol::HeaderSize,
            "valid header must wait for complete bounded payload validation");
        Require(!decoder.Feed(malformed.data() + Protocol::HeaderSize,
                    malformed.size() - Protocol::HeaderSize, consumed, error) &&
                error.code == Protocol::ErrorCode::InvalidValue && !error.message.empty() &&
                !decoder.HasMessage() && decoder.GetBytesNeeded() == 0,
            "invalid complete payload must not publish a stale or partial message");
        decoder.Reset();
        auto maximumHeader = valid;
        maximumHeader.resize(Protocol::HeaderSize);
        SetU32(maximumHeader, 8, static_cast<u32>(Protocol::MaximumFrameBytes - Protocol::HeaderSize));
        Require(decoder.Feed(maximumHeader.data(), maximumHeader.size(), consumed, error) &&
                consumed == Protocol::HeaderSize && !decoder.HasMessage() &&
                decoder.GetBufferedByteCount() == Protocol::HeaderSize &&
                decoder.GetBytesNeeded() == Protocol::MaximumFrameBytes - Protocol::HeaderSize,
            "maximum bounded header must retain only actual bytes while awaiting its payload");
        decoder.Reset();
        Require(decoder.Feed(nullptr, 0, consumed, error) && consumed == 0 &&
                decoder.GetBufferedByteCount() == 0 && decoder.GetBytesNeeded() == Protocol::HeaderSize,
            "reset must recover from an abandoned maximum-sized partial frame");
    }

    void TestSnapshotVolumeLimits()
    {
        Protocol::Message message;
        message.session = 17;
        Protocol::FullSnapshot snapshot;
        snapshot.tick = 123456;
        snapshot.lastQueuedSequence = 789;
        for (u64 index = 0; index < 2000; ++index)
        {
            auto unit = Unit(index + 1);
            unit.position = Math::Vec3(static_cast<f32>(index) * 0.5f, 0.0f, static_cast<f32>(index));
            unit.health = static_cast<u32>(index % 101);
            unit.selectable = index % 2 == 0;
            if (unit.health == 0 || index % 3 == 0)
            {
                unit.movementTarget.reset();
            }
            snapshot.units.push_back(unit);
        }
        message.body = snapshot;
        RoundTrip(message);
        Require(Encode(message).size() <= Protocol::MaximumFrameBytes,
            "2,000-unit corpus exceeded the declared message bound");
        auto& units = std::get<Protocol::FullSnapshot>(message.body).units;
        for (std::size_t index = units.size(); index < Protocol::MaximumUnits; ++index)
        {
            units.push_back(Unit(static_cast<u64>(index + 1)));
        }
        RoundTrip(message);
        Require(Encode(message).size() <= Protocol::MaximumFrameBytes,
            "maximum snapshot roster exceeded the declared frame bound");
        units.push_back(Unit(static_cast<u64>(Protocol::MaximumUnits + 1)));
        RequireEncodeFailure(message, "snapshot roster above maximum");
    }
}

int main()
{
    TestGoldenBytesAndCompatibility();
    TestFramingFailures();
    TestStateValidation();
    TestIncrementalFraming();
    TestIncrementalFailures();
    TestSnapshotVolumeLimits();
    std::cout << "RTS protocol tests passed\n";
    return EXIT_SUCCESS;
}