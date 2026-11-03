#include <Pyramid/Platform/TCP.hpp>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    using namespace Pyramid;
    using namespace Pyramid::Platform;

    void Require(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "TCP test failed: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    void RequireNative(bool condition, const std::string& error, const char* context)
    {
        Require(condition, std::string(context) + ": " + error);
    }

    template <typename Predicate, typename Step>
    void PumpUntil(Predicate predicate, Step step, const char* context)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!predicate())
        {
            step();
            Require(std::chrono::steady_clock::now() < deadline,
                std::string("five-second loopback deadline expired: ") + context);
            std::this_thread::yield();
        }
    }

    struct Pair
    {
        TcpListener listener;
        TcpStream client;
        TcpStream server;
    };

    void ConnectPair(Pair& pair)
    {
        std::string error;
        RequireNative(pair.listener.Listen({"127.0.0.1", 0}, error), error, "loopback listen");
        const u16 port = pair.listener.GetLocalPort();
        Require(port != 0 && pair.listener.IsListening(), "ephemeral listener did not publish its port");
        Require(pair.listener.Accept(pair.server, error) == TcpStatus::WouldBlock &&
                pair.server.GetState() == TcpState::Closed && error.empty(),
            "an idle accept must preserve its closed output and report WouldBlock");
        RequireNative(pair.client.Connect({"127.0.0.1", port}, error), error, "loopback connect");
        PumpUntil(
            [&pair]()
            {
                return pair.client.GetState() == TcpState::Connected &&
                    pair.server.GetState() == TcpState::Connected;
            },
            [&pair, &error]()
            {
                if (pair.client.GetState() == TcpState::Connecting)
                {
                    const TcpStatus status = pair.client.PollConnect(error);
                    Require(status == TcpStatus::Progress || status == TcpStatus::WouldBlock,
                        "connect readiness: " + error);
                }
                if (pair.server.GetState() == TcpState::Closed)
                {
                    const TcpStatus status = pair.listener.Accept(pair.server, error);
                    Require(status == TcpStatus::Progress || status == TcpStatus::WouldBlock,
                        "loopback accept: " + error);
                }
            },
            "connect/accept");
        Require(pair.client.PollConnect(error) == TcpStatus::Progress && error.empty(),
            "polling an established stream should report Progress");
    }

    void Transfer(TcpStream& sender, TcpStream& receiver, const std::vector<u8>& input)
    {
        std::string error;
        std::vector<u8> output(input.size());
        std::size_t sent = 0;
        std::size_t received = 0;
        bool sawCappedPrefix = false;
        PumpUntil(
            [&]() { return sent == input.size() && received == output.size(); },
            [&]()
            {
                if (sent < input.size())
                {
                    const std::size_t remaining = input.size() - sent;
                    const TcpTransfer result = sender.Send(input.data() + sent, remaining, error);
                    Require(result.status == TcpStatus::Progress || result.status == TcpStatus::WouldBlock,
                        "loopback send: " + error);
                    Require(result.bytes <= remaining && result.bytes <= 64 * 1024 &&
                            (result.status != TcpStatus::Progress || result.bytes > 0),
                        "send returned an invalid transfer prefix");
                    if (result.status == TcpStatus::Progress && remaining > 64 * 1024)
                    {
                        sawCappedPrefix = result.bytes < remaining;
                    }
                    sent += result.bytes;
                }
                if (received < output.size())
                {
                    const std::size_t remaining = output.size() - received;
                    const TcpTransfer result = receiver.Receive(output.data() + received, remaining, error);
                    Require(result.status == TcpStatus::Progress || result.status == TcpStatus::WouldBlock,
                        "loopback receive: " + error);
                    Require(result.bytes <= remaining && result.bytes <= 64 * 1024 &&
                            (result.status != TcpStatus::Progress || result.bytes > 0),
                        "receive returned an invalid transfer prefix");
                    received += result.bytes;
                }
            },
            "bidirectional byte transfer");
        Require(output == input, "partial send/receive changed, omitted, or duplicated bytes");
        Require(input.size() <= 64 * 1024 || sawCappedPrefix,
            "large writes must expose a capped prefix without assuming OS segmentation");
        u8 extra = 0;
        const TcpTransfer idle = receiver.Receive(&extra, 1, error);
        Require(idle.status == TcpStatus::WouldBlock && idle.bytes == 0 && error.empty(),
            "drained connected receive should be ordinary idle");
    }

    void AwaitEOF(TcpStream& stream)
    {
        bool closed = false;
        std::string error;
        PumpUntil(
            [&]() { return closed; },
            [&]()
            {
                u8 byte = 0;
                const TcpTransfer result = stream.Receive(&byte, 1, error);
                Require(result.status == TcpStatus::WouldBlock || result.status == TcpStatus::Closed,
                    "orderly peer closure did not produce EOF: " + error);
                closed = result.status == TcpStatus::Closed;
            },
            "orderly EOF");
        Require(stream.GetState() == TcpState::Closed,
            "EOF must release the native stream and publish Closed");
    }

    void TestEndpointValidation()
    {
        TcpStream stream;
        TcpListener listener;
        std::string error;
        Require(stream.GetState() == TcpState::Closed && !listener.IsListening() &&
                listener.GetLocalPort() == 0,
            "default TCP wrappers must be closed");
        const std::string invalidAddresses[] = {
            "", "localhost", "::1", "999.1.1.1", "127.0.0.1 too long",
            std::string("127.0.0.1\0bad", 13)};
        for (const std::string& address : invalidAddresses)
        {
            Require(!stream.Connect({address, 1}, error) && !error.empty() &&
                    stream.GetState() == TcpState::Closed,
                "invalid connect address was accepted or changed ownership");
            Require(!listener.Listen({address, 0}, error) && !error.empty() && !listener.IsListening(),
                "invalid listener address was accepted or changed ownership");
        }
        Require(!stream.Connect({"127.0.0.1", 0}, error) && !error.empty(),
            "destination port zero must be rejected");
        Require(listener.Accept(stream, error) == TcpStatus::Closed &&
                stream.GetState() == TcpState::Closed,
            "closed listener accept should leave output closed");
        Require(stream.PollConnect(error) == TcpStatus::Closed,
            "closed stream connect polling should report Closed");
        stream.Close();
        stream.Close();
        listener.Close();
        listener.Close();
    }

    void TestBytesAndMoves()
    {
        Pair pair;
        ConnectPair(pair);
        std::string error;
        const u16 port = pair.listener.GetLocalPort();
        Require(!pair.listener.Listen({"127.0.0.1", 0}, error) &&
                pair.listener.IsListening() && pair.listener.GetLocalPort() == port,
            "re-listen must not replace a live listener");
        Require(!pair.client.Connect({"127.0.0.1", port}, error) &&
                pair.client.GetState() == TcpState::Connected,
            "re-connect must not replace a live stream");
        Require(pair.listener.Accept(pair.server, error) == TcpStatus::Error &&
                pair.server.GetState() == TcpState::Connected,
            "accept must not replace an already connected output");
        u8 sample = 7;
        Require(pair.client.Send(nullptr, 1, error).status == TcpStatus::Error &&
                pair.client.Send(&sample, 0, error).status == TcpStatus::Error &&
                pair.client.Receive(nullptr, 1, error).status == TcpStatus::Error &&
                pair.client.Receive(&sample, 0, error).status == TcpStatus::Error &&
                pair.client.GetState() == TcpState::Connected,
            "invalid buffer arguments must fail without closing a live stream");
        Transfer(pair.client, pair.server, {0, 1, 2, 3, 254, 255});
        std::vector<u8> large(3 * 64 * 1024 + 17);
        for (std::size_t index = 0; index < large.size(); ++index)
        {
            large[index] = static_cast<u8>((index * 37 + 11) % 251);
        }
        Transfer(pair.server, pair.client, large);

        TcpListener movedListener(std::move(pair.listener));
        Require(!pair.listener.IsListening() && pair.listener.GetLocalPort() == 0 &&
                movedListener.GetLocalPort() == port,
            "moving a listener must transfer ownership and clear the source");
        TcpListener assignedListener;
        assignedListener = std::move(movedListener);
        Require(!movedListener.IsListening() && assignedListener.GetLocalPort() == port,
            "listener move assignment lost ownership");
        assignedListener.Close();
        assignedListener.Close();
        Transfer(pair.client, pair.server, {4, 5, 6});

        TcpStream moved(std::move(pair.server));
        Require(pair.server.GetState() == TcpState::Closed && moved.GetState() == TcpState::Connected,
            "moving an accepted stream must transfer its listener-independent Winsock lease");
        Pair second;
        ConnectPair(second);
        second.server = std::move(moved);
        Require(moved.GetState() == TcpState::Closed && second.server.GetState() == TcpState::Connected,
            "stream move assignment must transfer ownership and clear the source");
        AwaitEOF(second.client);
        second.listener.Close();
        Transfer(pair.client, second.server, {10, 20, 30, 40});
        pair.client.Close();
        pair.client.Close();
        AwaitEOF(second.server);
    }

    void TestIndependentRuntimeAndRefusal()
    {
        Pair survivor;
        ConnectPair(survivor);
        {
            Pair temporary;
            ConnectPair(temporary);
            temporary.client.Close();
            temporary.server.Close();
            temporary.listener.Close();
        }
        Transfer(survivor.client, survivor.server, {99, 100, 101});
        survivor.listener.Close();
        Transfer(survivor.server, survivor.client, {201, 202});

        TcpListener reservation;
        std::string error;
        RequireNative(reservation.Listen({"127.0.0.1", 0}, error), error, "refusal port reservation");
        const u16 closedPort = reservation.GetLocalPort();
        reservation.Close();
        TcpStream refused;
        const bool started = refused.Connect({"127.0.0.1", closedPort}, error);
        if (started)
        {
            bool failed = false;
            PumpUntil(
                [&]() { return failed; },
                [&]()
                {
                    const TcpStatus status = refused.PollConnect(error);
                    Require(status != TcpStatus::Progress,
                        "closed ephemeral port unexpectedly accepted a connection");
                    failed = status == TcpStatus::Error || status == TcpStatus::Closed;
                },
                "connection refusal");
        }
        Require(refused.GetState() == TcpState::Closed && error.find("10061") != std::string::npos,
            "refused connect must close its stream and retain an actionable error");
        Transfer(survivor.client, survivor.server, {17, 18});
    }
}

int main()
{
    static_assert(!std::is_copy_constructible_v<Pyramid::Platform::TcpStream> &&
        std::is_nothrow_move_constructible_v<Pyramid::Platform::TcpStream>);
    static_assert(!std::is_copy_assignable_v<Pyramid::Platform::TcpListener> &&
        std::is_nothrow_move_assignable_v<Pyramid::Platform::TcpListener>);
    TestEndpointValidation();
    TestBytesAndMoves();
    TestIndependentRuntimeAndRefusal();
    std::cout << "TCP tests passed\n";
    return EXIT_SUCCESS;
}
