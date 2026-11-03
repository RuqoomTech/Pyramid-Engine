#pragma once

#include <Pyramid/Core/Prerequisites.hpp>

#include <cstddef>
#include <memory>
#include <string>

namespace Pyramid::Platform
{
    struct TcpEndpoint
    {
        std::string address = "127.0.0.1";
        u16 port = 0;
    };

    enum class TcpState
    {
        Closed,
        Connecting,
        Connected
    };

    enum class TcpStatus
    {
        Progress,
        WouldBlock,
        Closed,
        Error
    };

    struct TcpTransfer
    {
        TcpStatus status = TcpStatus::WouldBlock;
        std::size_t bytes = 0;
    };

    /**
     * Move-only, single-threaded nonblocking TCP byte stream.
     *
     * Numeric IPv4 only; DNS and IPv6 are explicit failures. No native socket
     * types escape this interface. Progress can transfer only a prefix, capped
     * at 64 KiB per call. Callers retain unsent bytes and own framing/deadlines.
     * WouldBlock is ordinary idle. Fatal native I/O and EOF close the stream.
     */
    class TcpStream final
    {
    public:
        TcpStream();
        ~TcpStream();
        TcpStream(TcpStream&& other) noexcept;
        TcpStream& operator=(TcpStream&& other) noexcept;
        TcpStream(const TcpStream&) = delete;
        TcpStream& operator=(const TcpStream&) = delete;

        /** Starts a connection, possibly Connecting; rejects an already open stream. */
        [[nodiscard]] bool Connect(const TcpEndpoint& endpoint, std::string& error);
        /** Polls readiness without blocking; successful completion returns Progress. */
        [[nodiscard]] TcpStatus PollConnect(std::string& error);
        [[nodiscard]] TcpTransfer Send(const u8* data, std::size_t size, std::string& error);
        [[nodiscard]] TcpTransfer Receive(u8* data, std::size_t capacity, std::string& error);
        /** Idempotent; does not guarantee delivery of queued application data. */
        void Close();
        [[nodiscard]] TcpState GetState() const;

    private:
        friend class TcpListener;
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };

    /** Nonblocking numeric-IPv4 listener. Port zero requests an ephemeral port. */
    class TcpListener final
    {
    public:
        TcpListener();
        ~TcpListener();
        TcpListener(TcpListener&& other) noexcept;
        TcpListener& operator=(TcpListener&& other) noexcept;
        TcpListener(const TcpListener&) = delete;
        TcpListener& operator=(const TcpListener&) = delete;

        [[nodiscard]] bool Listen(const TcpEndpoint& endpoint, std::string& error);
        /** Output must be closed; idle/failure leaves it unchanged. */
        [[nodiscard]] TcpStatus Accept(TcpStream& output, std::string& error);
        void Close();
        [[nodiscard]] bool IsListening() const;
        [[nodiscard]] u16 GetLocalPort() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
