#include <Pyramid/Platform/TCP.hpp>

#include <algorithm>
#include <utility>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace Pyramid::Platform
{
#ifdef _WIN32
    namespace
    {
        constexpr std::size_t MaximumTransferBytes = 64 * 1024;

        void NativeError(std::string& error, const char* operation, int code)
        {
            error = std::string(operation) + " failed (Winsock " + std::to_string(code) + ").";
        }

        struct WinsockRuntime final
        {
            bool initialized = false;
            ~WinsockRuntime()
            {
                if (initialized)
                {
                    WSACleanup();
                }
            }
        };

        std::shared_ptr<WinsockRuntime> AcquireRuntime(std::string& error)
        {
            auto runtime = std::make_shared<WinsockRuntime>();
            WSADATA data{};
            const int result = WSAStartup(MAKEWORD(2, 2), &data);
            if (result != 0)
            {
                NativeError(error, "WSAStartup", result);
                return {};
            }
            runtime->initialized = true;
            if (data.wVersion != MAKEWORD(2, 2))
            {
                error = "Winsock 2.2 is required.";
                return {};
            }
            return runtime;
        }

        bool ParseEndpoint(const TcpEndpoint& endpoint, bool listener,
            sockaddr_in& address, std::string& error)
        {
            if (endpoint.address.empty() || endpoint.address.size() > 15 ||
                endpoint.address.find('\0') != std::string::npos || (!listener && endpoint.port == 0))
            {
                error = "TCP endpoint requires numeric IPv4 text and a nonzero destination port.";
                return false;
            }
            address = {};
            address.sin_family = AF_INET;
            address.sin_port = htons(endpoint.port);
            if (InetPtonA(AF_INET, endpoint.address.c_str(), &address.sin_addr) != 1)
            {
                error = "TCP address must be numeric IPv4; DNS and IPv6 are unsupported.";
                return false;
            }
            return true;
        }

        bool ConfigureSocket(SOCKET socket, bool stream, std::string& error)
        {
            u_long nonblocking = 1;
            if (ioctlsocket(socket, FIONBIO, &nonblocking) == SOCKET_ERROR)
            {
                NativeError(error, "ioctlsocket(FIONBIO)", WSAGetLastError());
                return false;
            }
            if (stream)
            {
                const int enabled = 1;
                if (setsockopt(socket, IPPROTO_TCP, TCP_NODELAY,
                        reinterpret_cast<const char*>(&enabled), sizeof(enabled)) == SOCKET_ERROR)
                {
                    NativeError(error, "setsockopt(TCP_NODELAY)", WSAGetLastError());
                    return false;
                }
            }
            return true;
        }

        SOCKET CreateSocket(std::string& error)
        {
            const SOCKET socket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP,
                nullptr, 0, WSA_FLAG_NO_HANDLE_INHERIT);
            if (socket == INVALID_SOCKET)
            {
                NativeError(error, "WSASocket", WSAGetLastError());
            }
            return socket;
        }
    }

    struct TcpStream::Impl
    {
        SOCKET socket = INVALID_SOCKET;
        std::shared_ptr<WinsockRuntime> runtime;
        TcpState state = TcpState::Closed;

        ~Impl()
        {
            if (socket != INVALID_SOCKET)
            {
                closesocket(socket);
            }
        }
    };

    struct TcpListener::Impl
    {
        SOCKET socket = INVALID_SOCKET;
        std::shared_ptr<WinsockRuntime> runtime;
        u16 port = 0;

        ~Impl()
        {
            if (socket != INVALID_SOCKET)
            {
                closesocket(socket);
            }
        }
    };
#else
    struct TcpStream::Impl {};
    struct TcpListener::Impl {};
#endif

    TcpStream::TcpStream() = default;
    TcpStream::~TcpStream() = default;
    TcpStream::TcpStream(TcpStream&& other) noexcept = default;
    TcpStream& TcpStream::operator=(TcpStream&& other) noexcept = default;
    TcpListener::TcpListener() = default;
    TcpListener::~TcpListener() = default;
    TcpListener::TcpListener(TcpListener&& other) noexcept = default;
    TcpListener& TcpListener::operator=(TcpListener&& other) noexcept = default;

    void TcpStream::Close()
    {
        m_impl.reset();
    }

    void TcpListener::Close()
    {
        m_impl.reset();
    }

    TcpState TcpStream::GetState() const
    {
#ifdef _WIN32
        return m_impl ? m_impl->state : TcpState::Closed;
#else
        return TcpState::Closed;
#endif
    }

    bool TcpListener::IsListening() const
    {
#ifdef _WIN32
        return m_impl && m_impl->socket != INVALID_SOCKET;
#else
        return false;
#endif
    }

    u16 TcpListener::GetLocalPort() const
    {
#ifdef _WIN32
        return m_impl ? m_impl->port : 0;
#else
        return 0;
#endif
    }

    bool TcpStream::Connect(const TcpEndpoint& endpoint, std::string& error)
    {
#ifdef _WIN32
        if (GetState() != TcpState::Closed)
        {
            error = "Close the existing TCP stream before connecting again.";
            return false;
        }
        auto staged = std::make_unique<Impl>();
        staged->runtime = AcquireRuntime(error);
        if (!staged->runtime)
        {
            return false;
        }
        sockaddr_in address{};
        if (!ParseEndpoint(endpoint, false, address, error))
        {
            return false;
        }
        staged->socket = CreateSocket(error);
        if (staged->socket == INVALID_SOCKET || !ConfigureSocket(staged->socket, true, error))
        {
            return false;
        }
        if (::connect(staged->socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0)
        {
            staged->state = TcpState::Connected;
        }
        else
        {
            const int code = WSAGetLastError();
            if (code != WSAEWOULDBLOCK && code != WSAEINPROGRESS && code != WSAEALREADY)
            {
                NativeError(error, "connect", code);
                return false;
            }
            staged->state = TcpState::Connecting;
        }
        m_impl = std::move(staged);
        error.clear();
        return true;
#else
        (void)endpoint;
        error = "TCP transport is currently supported only on Windows.";
        return false;
#endif
    }

    TcpStatus TcpStream::PollConnect(std::string& error)
    {
#ifdef _WIN32
        if (GetState() == TcpState::Closed)
        {
            error = "TCP stream is closed.";
            return TcpStatus::Closed;
        }
        if (m_impl->state == TcpState::Connected)
        {
            error.clear();
            return TcpStatus::Progress;
        }
        fd_set writable;
        fd_set exceptional;
        FD_ZERO(&writable);
        FD_ZERO(&exceptional);
        FD_SET(m_impl->socket, &writable);
        FD_SET(m_impl->socket, &exceptional);
        timeval timeout{};
        const int ready = select(0, nullptr, &writable, &exceptional, &timeout);
        if (ready == SOCKET_ERROR)
        {
            NativeError(error, "select(connect)", WSAGetLastError());
            Close();
            return TcpStatus::Error;
        }
        if (ready == 0)
        {
            error.clear();
            return TcpStatus::WouldBlock;
        }
        int code = 0;
        int size = sizeof(code);
        if (getsockopt(m_impl->socket, SOL_SOCKET, SO_ERROR,
                reinterpret_cast<char*>(&code), &size) == SOCKET_ERROR)
        {
            NativeError(error, "getsockopt(SO_ERROR)", WSAGetLastError());
            Close();
            return TcpStatus::Error;
        }
        if (code != 0)
        {
            NativeError(error, "connect completion", code);
            Close();
            return TcpStatus::Error;
        }
        m_impl->state = TcpState::Connected;
        error.clear();
        return TcpStatus::Progress;
#else
        error = "TCP transport is currently supported only on Windows.";
        return TcpStatus::Error;
#endif
    }

    TcpTransfer TcpStream::Send(const u8* data, std::size_t size, std::string& error)
    {
        if (!data || size == 0)
        {
            error = "TCP send requires a nonnull, nonempty buffer.";
            return {TcpStatus::Error, 0};
        }
#ifdef _WIN32
        if (GetState() != TcpState::Connected)
        {
            error = "TCP send requires a connected stream.";
            return {GetState() == TcpState::Closed ? TcpStatus::Closed : TcpStatus::Error, 0};
        }
        const int length = static_cast<int>((std::min)(size, MaximumTransferBytes));
        const int transferred = ::send(m_impl->socket, reinterpret_cast<const char*>(data), length, 0);
        if (transferred > 0)
        {
            error.clear();
            return {TcpStatus::Progress, static_cast<std::size_t>(transferred)};
        }
        if (transferred == 0)
        {
            error = "TCP send returned zero; stream closed.";
            Close();
            return {TcpStatus::Closed, 0};
        }
        const int code = WSAGetLastError();
        if (code == WSAEWOULDBLOCK)
        {
            error.clear();
            return {TcpStatus::WouldBlock, 0};
        }
        NativeError(error, "send", code);
        Close();
        return {TcpStatus::Error, 0};
#else
        error = "TCP transport is currently supported only on Windows.";
        return {TcpStatus::Error, 0};
#endif
    }

    TcpTransfer TcpStream::Receive(u8* data, std::size_t capacity, std::string& error)
    {
        if (!data || capacity == 0)
        {
            error = "TCP receive requires a nonnull, nonempty buffer.";
            return {TcpStatus::Error, 0};
        }
#ifdef _WIN32
        if (GetState() != TcpState::Connected)
        {
            error = "TCP receive requires a connected stream.";
            return {GetState() == TcpState::Closed ? TcpStatus::Closed : TcpStatus::Error, 0};
        }
        const int length = static_cast<int>((std::min)(capacity, MaximumTransferBytes));
        const int transferred = ::recv(m_impl->socket, reinterpret_cast<char*>(data), length, 0);
        if (transferred > 0)
        {
            error.clear();
            return {TcpStatus::Progress, static_cast<std::size_t>(transferred)};
        }
        if (transferred == 0)
        {
            error = "Remote TCP peer closed the stream.";
            Close();
            return {TcpStatus::Closed, 0};
        }
        const int code = WSAGetLastError();
        if (code == WSAEWOULDBLOCK)
        {
            error.clear();
            return {TcpStatus::WouldBlock, 0};
        }
        NativeError(error, "recv", code);
        Close();
        return {TcpStatus::Error, 0};
#else
        error = "TCP transport is currently supported only on Windows.";
        return {TcpStatus::Error, 0};
#endif
    }

    bool TcpListener::Listen(const TcpEndpoint& endpoint, std::string& error)
    {
#ifdef _WIN32
        if (IsListening())
        {
            error = "Close the existing TCP listener before listening again.";
            return false;
        }
        auto staged = std::make_unique<Impl>();
        staged->runtime = AcquireRuntime(error);
        if (!staged->runtime)
        {
            return false;
        }
        sockaddr_in address{};
        if (!ParseEndpoint(endpoint, true, address, error))
        {
            return false;
        }
        staged->socket = CreateSocket(error);
        if (staged->socket == INVALID_SOCKET || !ConfigureSocket(staged->socket, false, error))
        {
            return false;
        }
        const int exclusive = 1;
        if (setsockopt(staged->socket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)) == SOCKET_ERROR)
        {
            NativeError(error, "setsockopt(SO_EXCLUSIVEADDRUSE)", WSAGetLastError());
            return false;
        }
        if (::bind(staged->socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR)
        {
            NativeError(error, "bind", WSAGetLastError());
            return false;
        }
        if (::listen(staged->socket, SOMAXCONN) == SOCKET_ERROR)
        {
            NativeError(error, "listen", WSAGetLastError());
            return false;
        }
        int size = sizeof(address);
        if (getsockname(staged->socket, reinterpret_cast<sockaddr*>(&address), &size) == SOCKET_ERROR)
        {
            NativeError(error, "getsockname", WSAGetLastError());
            return false;
        }
        staged->port = ntohs(address.sin_port);
        m_impl = std::move(staged);
        error.clear();
        return true;
#else
        (void)endpoint;
        error = "TCP transport is currently supported only on Windows.";
        return false;
#endif
    }

    TcpStatus TcpListener::Accept(TcpStream& output, std::string& error)
    {
#ifdef _WIN32
        if (!IsListening())
        {
            error = "TCP listener is closed.";
            return TcpStatus::Closed;
        }
        if (output.GetState() != TcpState::Closed)
        {
            error = "TCP accept output must be a closed stream.";
            return TcpStatus::Error;
        }
        auto staged = std::make_unique<TcpStream::Impl>();
        staged->runtime = m_impl->runtime;
        staged->socket = ::accept(m_impl->socket, nullptr, nullptr);
        if (staged->socket == INVALID_SOCKET)
        {
            const int code = WSAGetLastError();
            if (code == WSAEWOULDBLOCK)
            {
                error.clear();
                return TcpStatus::WouldBlock;
            }
            NativeError(error, "accept", code);
            return TcpStatus::Error;
        }
        staged->state = TcpState::Connected;
        if (!ConfigureSocket(staged->socket, true, error))
        {
            return TcpStatus::Error;
        }
        output.m_impl = std::move(staged);
        error.clear();
        return TcpStatus::Progress;
#else
        (void)output;
        error = "TCP transport is currently supported only on Windows.";
        return TcpStatus::Error;
#endif
    }
}
