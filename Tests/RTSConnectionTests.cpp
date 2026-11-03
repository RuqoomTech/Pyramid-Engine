#include <Pyramid/Examples/RTSReference/RTSConnection.hpp>
#include <Pyramid/Examples/RTSReference/RTSHostSession.hpp>
#include <Pyramid/Examples/RTSReference/RTSSimulation.hpp>
#include <Pyramid/Examples/RTSReference/RTSUnitStore.hpp>
#include <Pyramid/Graphics/Scene.hpp>
#include <Pyramid/Platform/TCP.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <variant>
#include <vector>

namespace
{
    using namespace Pyramid;
    using namespace Pyramid::Examples::RTSReference;
    using namespace Pyramid::Platform;

    void Require(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "RTS connection test failed: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    template <typename Attempt>
    void Until(Attempt attempt, const char* context)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!attempt())
        {
            Require(std::chrono::steady_clock::now() < deadline,
                std::string(context) + " exceeded its five-second loopback deadline");
            std::this_thread::yield();
        }
    }

    bool Terminal(RTSConnectionState state)
    {
        return state == RTSConnectionState::Closed || state == RTSConnectionState::Failed;
    }

    Protocol::ContentId Content()
    {
        Protocol::ContentId content{};
        content[0] = 0x31;
        content[15] = 0xA7;
        return content;
    }

    std::vector<u8> Encode(const Protocol::Message& message)
    {
        Protocol::Error error;
        std::vector<u8> bytes;
        Require(Protocol::Encode(message, bytes, error), "fixture encode: " + error.message);
        return bytes;
    }

    void SendRaw(TcpStream& stream, const u8* bytes, std::size_t count)
    {
        std::size_t sent = 0;
        std::string error;
        Until([&]()
        {
            const auto result = stream.Send(bytes + sent, count - sent, error);
            Require(result.status == TcpStatus::Progress || result.status == TcpStatus::WouldBlock,
                "raw fixture send: " + error);
            sent += result.bytes;
            return sent == count;
        }, "raw fixture send");
    }

    void SendRaw(TcpStream& stream, const std::vector<u8>& bytes)
    {
        SendRaw(stream, bytes.data(), bytes.size());
    }

    template <typename Pump>
    Protocol::Message ReadRaw(TcpStream& stream, Pump pump)
    {
        Protocol::FrameDecoder decoder;
        Protocol::Message message;
        Protocol::Error diagnostic;
        std::string error;
        Until([&]()
        {
            pump();
            u8 buffer[4096];
            const auto result = stream.Receive(buffer,
                (std::min)(sizeof(buffer), decoder.GetBytesNeeded()), error);
            Require(result.status == TcpStatus::Progress || result.status == TcpStatus::WouldBlock,
                "raw fixture receive: " + error);
            if (result.bytes != 0)
            {
                std::size_t consumed = 0;
                Require(decoder.Feed(buffer, result.bytes, consumed, diagnostic) &&
                        consumed == result.bytes, "raw fixture framing: " + diagnostic.message);
            }
            return decoder.TakeMessage(message);
        }, "raw fixture receive");
        return message;
    }

    struct HostFixture
    {
        Scene scene{"TCP Host Fixture"};
        RTSUnitStore units;
        RTSSimulation simulation{units};
        RTSHostSession session{units, simulation};
        RTSHostConnection host{session};
        Entity unit;
        u64 networkUnit = 0;
        std::string error;

        explicit HostFixture(std::size_t count = 1)
        {
            Require(session.Begin(42, Content(), error), "fixture host begin: " + error);
            Protocol::Hello hello;
            hello.content = Content();
            Require(session.AddPeer(11, 1, hello, error), "fixture local peer: " + error);
            RTSUnitState definition;
            definition.owner = 2;
            definition.team = 2;
            for (std::size_t index = 0; index < count; ++index)
            {
                const Entity entity = scene.CreateEntity("Network Unit");
                u64 assigned = 0;
                Require(entity && units.AddUnit(entity, definition) &&
                        session.BindUnit(entity, 1, assigned, error), "fixture unit bind: " + error);
                if (index == 0)
                {
                    unit = entity;
                    networkUnit = assigned;
                }
            }
            Require(host.Listen({"127.0.0.1", 0}, 22, 2, error), "fixture listen: " + error);
            Require(host.GetLocalPort() != 0, "ephemeral host port was not reported");
        }

        void Poll()
        {
            Require(host.Poll(0.0, error), "host poll: " + error);
        }

        void Ready(RTSClientConnection& client)
        {
            Require(client.Connect({"127.0.0.1", host.GetLocalPort()}, Content(), error),
                "fixture client connect: " + error);
            Until([&]()
            {
                Poll();
                Require(client.Poll(0.0, error), "client poll: " + error);
                return host.GetState() == RTSConnectionState::Ready &&
                    client.GetState() == RTSConnectionState::Ready;
            }, "host/client handshake");
        }

        TcpStream Raw(bool completeHandshake)
        {
            TcpStream raw;
            Require(raw.Connect({"127.0.0.1", host.GetLocalPort()}, error),
                "raw connect: " + error);
            Until([&]()
            {
                Poll();
                if (raw.GetState() == TcpState::Connecting)
                {
                    const auto status = raw.PollConnect(error);
                    Require(status == TcpStatus::Progress || status == TcpStatus::WouldBlock,
                        "raw connect poll: " + error);
                }
                return raw.GetState() == TcpState::Connected &&
                    host.GetState() == RTSConnectionState::Handshaking;
            }, "raw connection acceptance");
            if (completeHandshake)
            {
                const auto greeting = ReadRaw(raw, [&]() { Poll(); });
                Require(greeting.session == 42 && std::holds_alternative<Protocol::Hello>(greeting.body),
                    "host must send the session Hello first");
                SendRaw(raw, Encode(greeting));
                Until([&]()
                {
                    Poll();
                    return host.GetState() == RTSConnectionState::Ready;
                }, "raw Hello reply");
                const auto initial = ReadRaw(raw, [&]() { Poll(); });
                Require(std::holds_alternative<Protocol::FullSnapshot>(initial.body),
                    "host must publish the initial snapshot as handshake acceptance");
            }
            return raw;
        }
    };

    void Pump(HostFixture& fixture, RTSClientConnection& client)
    {
        fixture.Poll();
        Require(client.Poll(0.0, fixture.error), "client pump: " + fixture.error);
    }

    void TestRealFlowAndDisconnect()
    {
        HostFixture fixture;
        RTSClientConnection client;
        fixture.Ready(client);
        Require(client.GetSessionId() == 42 && client.GetLatestSnapshot() &&
                std::get<Protocol::FullSnapshot>(client.GetLatestSnapshot()->body).units.size() == 1,
            "ready client has no initial authoritative snapshot");
        const auto stateBefore = client.GetState();
        for (f64 invalid : {-1.0, std::numeric_limits<f64>::infinity(),
                std::numeric_limits<f64>::quiet_NaN()})
        {
            Require(!client.Poll(invalid, fixture.error) && !fixture.error.empty() &&
                    client.GetState() == stateBefore &&
                    !fixture.host.Poll(invalid, fixture.error) &&
                    fixture.host.GetState() == RTSConnectionState::Ready,
                "invalid elapsed time must not terminate or mutate a live connection");
        }
        Require(client.QueueMove({1, fixture.networkUnit, Math::Vec3(10.0f, 0.0f, 0.0f)}, fixture.error),
            "ready client move enqueue: " + fixture.error);
        Until([&]()
        {
            Pump(fixture, client);
            return fixture.simulation.GetPendingCommandCount() == 1;
        }, "wire command admission");
        Require(!fixture.units.FindUnit(fixture.unit)->movementTarget,
            "network admission executed movement before its fixed tick");
        Require(fixture.simulation.Advance(RTSSimulation::FixedStepSeconds).acceptedCommands == 1,
            "TCP move did not enter real simulation");
        Require(fixture.host.PublishSnapshot(fixture.error), "post-tick snapshot queue: " + fixture.error);
        Until([&]()
        {
            Pump(fixture, client);
            return std::get<Protocol::FullSnapshot>(client.GetLatestSnapshot()->body).tick == 1;
        }, "post-tick authoritative snapshot");
        const auto& snapshot = std::get<Protocol::FullSnapshot>(client.GetLatestSnapshot()->body);
        Require(snapshot.lastQueuedSequence == 1 && snapshot.units.front().unit == fixture.networkUnit &&
                std::fabs(snapshot.units.front().position.x - 0.125f) < 0.0001f,
            "TCP snapshot changed queued acknowledgement, identity, or moved position");
        Require(!fixture.host.PublishSnapshot(fixture.error),
            "unchanged simulation tick should not queue a duplicate snapshot");
        Require(client.QueueMove({2, 999999, Math::Vec3::Zero}, fixture.error),
            "unknown-unit admission fixture did not queue structurally valid intent");
        Until([&]()
        {
            Pump(fixture, client);
            return fixture.host.GetRejectedCommandCount() == 1;
        }, "nonterminal gameplay rejection");
        Require(fixture.host.GetState() == RTSConnectionState::Ready &&
                !fixture.host.GetLastAdmissionError().empty(),
            "a stale/unauthorized gameplay request must be diagnosed without dropping the connection");
        Require(client.QueueMove({3, fixture.networkUnit, Math::Vec3(20.0f, 0.0f, 0.0f)}, fixture.error),
            "sequence gap after gameplay rejection was not queued");
        Until([&]()
        {
            Pump(fixture, client);
            return fixture.simulation.GetPendingCommandCount() == 1;
        }, "pending move before disconnect");
        const Math::Vec3 positionAtClose = fixture.unit.GetWorldPosition();
        client.Stop();
        Until([&]()
        {
            (void)fixture.host.Poll(0.0, fixture.error);
            return Terminal(fixture.host.GetState());
        }, "remote close detection");
        Require(!fixture.session.IsActive() && fixture.simulation.GetPendingCommandCount() == 0 &&
                !fixture.units.FindUnit(fixture.unit)->movementTarget &&
                fixture.simulation.Advance(RTSSimulation::FixedStepSeconds).acceptedCommands == 0 &&
                fixture.unit.GetWorldPosition().x == positionAtClose.x,
            "disconnect must stop queued and admitted bound movement");
    }

    void TestQueueBoundsAndPartialTransfers()
    {
        {
            HostFixture fixture;
            RTSClientConnection client;
            fixture.Ready(client);
            for (u64 sequence = 1; sequence <= RTSConnectionLimits::MaximumQueuedFrames; ++sequence)
            {
                Require(client.QueueMove({sequence, fixture.networkUnit, Math::Vec3(10.0f)}, fixture.error),
                    "client rejected a frame below its queue capacity");
            }
            const auto bytes = client.GetQueuedBytes();
            Require(client.GetQueuedFrameCount() == RTSConnectionLimits::MaximumQueuedFrames &&
                    !client.QueueMove({RTSConnectionLimits::MaximumQueuedFrames + 1,
                        fixture.networkUnit, Math::Vec3(10.0f)}, fixture.error) &&
                    client.GetQueuedBytes() == bytes,
                "frame backpressure must preserve accepted buffers and sequence state");
            Until([&]()
            {
                Pump(fixture, client);
                return client.GetQueuedFrameCount() == 0;
            }, "client queue drain");
            Require(client.QueueMove({RTSConnectionLimits::MaximumQueuedFrames + 1,
                    fixture.networkUnit, Math::Vec3(10.0f)}, fixture.error),
                "failed queue insertion must permit the same sequence after drain");
        }
        {
            HostFixture fixture(2000);
            RTSClientConnection client;
            fixture.Ready(client);
            std::size_t queued = 0;
            for (std::size_t index = 0; index < RTSConnectionLimits::MaximumQueuedFrames; ++index)
            {
                (void)fixture.simulation.Advance(RTSSimulation::FixedStepSeconds);
                if (!fixture.host.PublishSnapshot(fixture.error))
                {
                    break;
                }
                ++queued;
            }
            Require(queued > 1 && queued < RTSConnectionLimits::MaximumQueuedFrames &&
                    fixture.host.GetQueuedBytes() <= RTSConnectionLimits::MaximumQueuedBytes &&
                    fixture.host.GetQueuedFrameCount() == queued,
                "large snapshot queue must hit its byte bound before the frame-count bound");
            fixture.Poll();
            Require(fixture.host.GetQueuedFrameCount() != 0,
                "one poll exceeded its outgoing byte budget");
            Until([&]()
            {
                Pump(fixture, client);
                const auto* latest = client.GetLatestSnapshot();
                return fixture.host.GetQueuedFrameCount() == 0 && latest &&
                    std::get<Protocol::FullSnapshot>(latest->body).tick == queued;
            }, "large ordered snapshot drain");
            const auto& roster = std::get<Protocol::FullSnapshot>(client.GetLatestSnapshot()->body).units;
            Require(roster.size() == 2000 && roster.front().unit < roster.back().unit,
                "partial TCP transfers corrupted the complete bounded roster");
        }
    }

    void TestRawProtocolFailures()
    {
        for (unsigned int variation = 0; variation < 3; ++variation)
        {
            HostFixture fixture;
            TcpStream raw = fixture.Raw(variation != 0);
            Protocol::Message message;
            message.session = 42;
            message.body = Protocol::MoveCommand{1, fixture.networkUnit, Math::Vec3(5.0f)};
            if (variation == 1)
            {
                message.session = 43;
            }
            if (variation == 2)
            {
                message.body = Protocol::FullSnapshot{};
            }
            SendRaw(raw, Encode(message));
            Until([&]()
            {
                (void)fixture.host.Poll(0.0, fixture.error);
                return Terminal(fixture.host.GetState());
            }, "wrong-role/session rejection");
            Require(fixture.host.GetState() == RTSConnectionState::Failed &&
                    !fixture.session.IsActive() && !fixture.error.empty(),
                "wrong-role/session traffic must terminate the host session visibly");
        }
        {
            HostFixture fixture;
            TcpStream raw = fixture.Raw(false);
            Protocol::Hello hello;
            hello.content = Content();
            auto bytes = Encode({42, hello});
            for (std::size_t offset = 8; offset < 12; ++offset)
            {
                bytes[offset] = 0xFF;
            }
            SendRaw(raw, bytes.data(), Protocol::HeaderSize);
            Until([&]()
            {
                (void)fixture.host.Poll(0.0, fixture.error);
                return Terminal(fixture.host.GetState());
            }, "hostile header rejection");
            Require(fixture.host.GetState() == RTSConnectionState::Failed &&
                    !fixture.session.IsActive(), "oversized header must fail before receiving its claimed body");
        }
        {
            HostFixture fixture;
            RTSClientConnection client;
            auto wrong = Content();
            wrong[0] ^= 1;
            Require(client.Connect({"127.0.0.1", fixture.host.GetLocalPort()}, wrong, fixture.error),
                "incompatible client fixture could not connect");
            Until([&]()
            {
                (void)fixture.host.Poll(0.0, fixture.error);
                (void)client.Poll(0.0, fixture.error);
                return Terminal(client.GetState()) && Terminal(fixture.host.GetState());
            }, "content mismatch disconnect");
            Require(client.GetState() == RTSConnectionState::Failed && !fixture.session.IsActive(),
                "incompatible content must never produce a ready client");
        }
    }

    void TestDeadlinesAndKeepalives()
    {
        {
            HostFixture fixture;
            TcpStream raw = fixture.Raw(false);
            Require(!fixture.host.Poll(RTSConnectionLimits::HandshakeTimeoutSeconds + 0.1, fixture.error) &&
                    fixture.host.GetState() == RTSConnectionState::Failed && !fixture.session.IsActive(),
                "a silent handshake must expire without a blocking wait");
        }
        {
            HostFixture fixture;
            TcpStream raw = fixture.Raw(true);
            const auto bytes = Encode({42, Protocol::MoveCommand{1, fixture.networkUnit, Math::Vec3(5.0f)}});
            SendRaw(raw, bytes.data(), 1);
            Until([&]()
            {
                fixture.Poll();
                return fixture.host.GetBufferedByteCount() == 1;
            }, "first partial frame byte");
            Require(fixture.host.Poll(2.0, fixture.error), "partial frame expired before its deadline");
            SendRaw(raw, bytes.data() + 1, 1);
            Until([&]()
            {
                fixture.Poll();
                return fixture.host.GetBufferedByteCount() == 2;
            }, "trickled partial frame byte");
            Require(!fixture.host.Poll(RTSConnectionLimits::FrameTimeoutSeconds - 2.0 + 0.1, fixture.error) &&
                    fixture.host.GetState() == RTSConnectionState::Failed && !fixture.session.IsActive(),
                "trickled bytes must not restart the absolute frame deadline");
        }
        {
            HostFixture fixture;
            TcpStream raw = fixture.Raw(true);
            const u8 prefix = 'P';
            SendRaw(raw, &prefix, 1);
            Until([&]()
            {
                fixture.Poll();
                return fixture.host.GetBufferedByteCount() == 1;
            }, "EOF partial header fixture");
            raw.Close();
            Until([&]()
            {
                (void)fixture.host.Poll(0.0, fixture.error);
                return Terminal(fixture.host.GetState());
            }, "EOF in partial header");
            Require(fixture.host.GetState() == RTSConnectionState::Failed && !fixture.session.IsActive(),
                "EOF must not publish a partial message");
        }
        {
            HostFixture fixture;
            TcpStream raw = fixture.Raw(true);
            Require(!fixture.host.Poll(RTSConnectionLimits::IdleTimeoutSeconds + 0.1, fixture.error) &&
                    fixture.host.GetState() == RTSConnectionState::Failed,
                "a ready peer without keepalives must expire");
        }
        {
            HostFixture fixture;
            RTSClientConnection client;
            fixture.Ready(client);
            for (unsigned int interval = 0; interval < 12; ++interval)
            {
                const u64 hostFrames = fixture.host.GetReceivedFrameCount();
                const u64 clientFrames = client.GetReceivedFrameCount();
                Require(fixture.host.Poll(RTSConnectionLimits::HeartbeatSeconds, fixture.error) &&
                        client.Poll(RTSConnectionLimits::HeartbeatSeconds, fixture.error),
                    "healthy idle connection failed before keepalive exchange");
                Until([&]()
                {
                    Pump(fixture, client);
                    return fixture.host.GetReceivedFrameCount() > hostFrames &&
                        client.GetReceivedFrameCount() > clientFrames;
                }, "ready keepalive exchange");
            }
            Require(fixture.host.GetState() == RTSConnectionState::Ready &&
                    client.GetState() == RTSConnectionState::Ready,
                "stationary clients must remain ready through validated keepalives");
            Require(client.QueueMove({1, fixture.networkUnit, Math::Vec3(10.0f)}, fixture.error),
                "write deadline fixture failed");
            Require(!client.Poll(RTSConnectionLimits::WriteTimeoutSeconds + 0.1, fixture.error) &&
                    client.GetState() == RTSConnectionState::Failed,
                "an unserviced queued frame must have an absolute write deadline");
        }
    }

    struct RawServer
    {
        TcpListener listener;
        TcpStream stream;
        RTSClientConnection client;
        std::string error;

        RawServer()
        {
            Require(listener.Listen({"127.0.0.1", 0}, error), "raw server listen: " + error);
            Require(client.Connect({"127.0.0.1", listener.GetLocalPort()}, Content(), error),
                "raw-server client connect: " + error);
            Until([&]()
            {
                if (stream.GetState() == TcpState::Closed)
                {
                    const auto accepted = listener.Accept(stream, error);
                    Require(accepted == TcpStatus::Progress || accepted == TcpStatus::WouldBlock,
                        "raw server accept: " + error);
                }
                Require(client.Poll(0.0, error), "raw-server client poll: " + error);
                return stream.GetState() == TcpState::Connected &&
                    client.GetState() == RTSConnectionState::Handshaking;
            }, "raw server connection");
            listener.Close();
        }
    };

    void TestClientAcceptanceAndFreshness()
    {
        {
            RawServer server;
            Protocol::Hello hello;
            hello.content = Content();
            SendRaw(server.stream, Encode({77, hello}));
            Until([&]()
            {
                Require(server.client.Poll(0.0, server.error), "client Hello poll: " + server.error);
                return server.client.GetSessionId() == 77;
            }, "client host-Hello validation");
            Require(server.client.GetState() == RTSConnectionState::Handshaking &&
                    !server.client.GetLatestSnapshot() &&
                    !server.client.QueueMove({1, 1, Math::Vec3::Zero}, server.error),
                "sending a Hello reply must not make the client ready before host acceptance");
            const auto reply = ReadRaw(server.stream, [&]()
            {
                Require(server.client.Poll(0.0, server.error), "client Hello send: " + server.error);
            });
            Require(reply.session == 77 && std::holds_alternative<Protocol::Hello>(reply.body),
                "client Hello reply changed session or role");
            const auto bytes = Encode({77, Protocol::FullSnapshot{}});
            SendRaw(server.stream, bytes);
            Until([&]()
            {
                Require(server.client.Poll(0.0, server.error), "client initial snapshot: " + server.error);
                return server.client.GetState() == RTSConnectionState::Ready;
            }, "client initial-snapshot acceptance");
            SendRaw(server.stream, bytes);
            Until([&]()
            {
                (void)server.client.Poll(0.0, server.error);
                return Terminal(server.client.GetState());
            }, "client stale snapshot rejection");
            Require(server.client.GetState() == RTSConnectionState::Failed,
                "client must reject non-increasing snapshot ticks on an ordered stream");
        }
        {
            RawServer server;
            const u8 prefix = 'P';
            SendRaw(server.stream, &prefix, 1);
            Until([&]()
            {
                Require(server.client.Poll(0.0, server.error), "client partial header: " + server.error);
                return server.client.GetBufferedByteCount() == 1;
            }, "client partial-header delivery");
            server.stream.Close();
            Until([&]()
            {
                (void)server.client.Poll(0.0, server.error);
                return Terminal(server.client.GetState());
            }, "client EOF in frame");
            Require(server.client.GetState() == RTSConnectionState::Failed &&
                    !server.client.GetLatestSnapshot(), "client EOF must discard incomplete state");
        }
    }
}

int main()
{
    TestRealFlowAndDisconnect();
    TestQueueBoundsAndPartialTransfers();
    TestRawProtocolFailures();
    TestDeadlinesAndKeepalives();
    TestClientAcceptanceAndFreshness();
    std::cout << "RTS TCP connection tests passed\n";
    return EXIT_SUCCESS;
}
