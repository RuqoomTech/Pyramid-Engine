#pragma once

#include <Pyramid/Examples/RTSReference/RTSProtocol.hpp>
#include <Pyramid/Platform/TCP.hpp>

#include <cstddef>
#include <memory>
#include <string>

namespace Pyramid::Examples::RTSReference
{
    class RTSHostSession;

    enum class RTSConnectionState
    {
        Idle,
        Listening,
        Connecting,
        Handshaking,
        Ready,
        Closed,
        Failed
    };

    /** Snapshot publication separates transient queue pressure from invalid host state. */
    enum class RTSSnapshotPublication
    {
        Queued,
        Backpressure,
        NotReady,
        InvalidState
    };

    struct RTSConnectionLimits
    {
        static constexpr std::size_t MaximumQueuedFrames = 8;
        static constexpr std::size_t MaximumQueuedBytes = 4 * Protocol::MaximumFrameBytes;
        /** Combined send/receive budget, divided equally to avoid starving either direction. */
        static constexpr std::size_t BytesPerPoll = Protocol::MaximumFrameBytes;
        static constexpr std::size_t FramesPerPoll = 16;
        static constexpr f64 ConnectTimeoutSeconds = 5.0;
        static constexpr f64 HandshakeTimeoutSeconds = 5.0;
        static constexpr f64 FrameTimeoutSeconds = 5.0;
        static constexpr f64 WriteTimeoutSeconds = 5.0;
        static constexpr f64 IdleTimeoutSeconds = 10.0;
        static constexpr f64 HeartbeatSeconds = 1.0;
    };

    /**
     * One remote connection for a two-player reference host. The borrowed session
     * must outlive this object. Poll on the game thread with uncapped monotonic
     * elapsed seconds, including while local UI/focus pauses gameplay. Host
     * simulation pause policy remains the caller's responsibility.
     *
     * Successful Listen owns the session lifecycle: Stop, EOF, protocol failure,
     * or timeout calls End and cancels its pending/bound movement. A failed Listen
     * leaves the session alone. No blocking calls, reconnect, encryption, DNS,
     * client scene application, or GUI are supplied by this reference helper.
     */
    class RTSHostConnection final
    {
    public:
        explicit RTSHostConnection(RTSHostSession& session);
        ~RTSHostConnection();
        RTSHostConnection(const RTSHostConnection&) = delete;
        RTSHostConnection& operator=(const RTSHostConnection&) = delete;

        [[nodiscard]] bool Listen(
            const Platform::TcpEndpoint& endpoint, u64 remotePeer, u32 remoteOwner,
            std::string& error);
        /** Invalid elapsed time fails without changing connection state or buffered work. */
        [[nodiscard]] bool Poll(f64 elapsedSeconds, std::string& error);
        /** Ready and newer tick required; only Backpressure is safe to retry unchanged. */
        [[nodiscard]] RTSSnapshotPublication TryPublishSnapshot(std::string& error);
        /** Compatibility wrapper; true only when TryPublishSnapshot returns Queued. */
        [[nodiscard]] bool PublishSnapshot(std::string& error);
        void Stop();

        [[nodiscard]] RTSConnectionState GetState() const;
        [[nodiscard]] u16 GetLocalPort() const;
        [[nodiscard]] std::size_t GetQueuedBytes() const;
        [[nodiscard]] std::size_t GetQueuedFrameCount() const;
        [[nodiscard]] std::size_t GetBufferedByteCount() const;
        [[nodiscard]] u64 GetReceivedFrameCount() const;
        [[nodiscard]] u64 GetRejectedCommandCount() const;
        [[nodiscard]] const std::string& GetLastAdmissionError() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };

    /**
     * A client waits for host Hello, echoes compatible content/session, and becomes
     * Ready only after the initial full snapshot. Ready Hello messages are bounded
     * keepalives. Snapshots are received data, never applied to an engine scene.
     * Poll uses the same uncapped time/deadline contract as the host.
     */
    class RTSClientConnection final
    {
    public:
        RTSClientConnection();
        ~RTSClientConnection();
        RTSClientConnection(const RTSClientConnection&) = delete;
        RTSClientConnection& operator=(const RTSClientConnection&) = delete;

        [[nodiscard]] bool Connect(
            const Platform::TcpEndpoint& endpoint, const Protocol::ContentId& content,
            std::string& error);
        [[nodiscard]] bool Poll(f64 elapsedSeconds, std::string& error);
        /** Sequence advances locally only after enqueue; queue admission is not execution. */
        [[nodiscard]] bool QueueMove(const Protocol::MoveCommand& command, std::string& error);
        void Stop();

        [[nodiscard]] RTSConnectionState GetState() const;
        [[nodiscard]] u64 GetSessionId() const;
        /** Borrowed until the next snapshot or terminal closure/Stop. */
        [[nodiscard]] const Protocol::Message* GetLatestSnapshot() const;
        [[nodiscard]] std::size_t GetQueuedBytes() const;
        [[nodiscard]] std::size_t GetQueuedFrameCount() const;
        [[nodiscard]] std::size_t GetBufferedByteCount() const;
        [[nodiscard]] u64 GetReceivedFrameCount() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
