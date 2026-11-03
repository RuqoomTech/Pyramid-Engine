#include <Pyramid/Examples/RTSReference/RTSConnection.hpp>
#include <Pyramid/Examples/RTSReference/RTSHostSession.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <optional>
#include <utility>
#include <vector>

namespace Pyramid::Examples::RTSReference
{
    namespace
    {
        using Platform::TcpStatus;

        bool Fail(std::string& error, const char* message)
        {
            error = message;
            return false;
        }

        bool ValidElapsed(f64 elapsedSeconds, std::string& error)
        {
            return (std::isfinite(elapsedSeconds) && elapsedSeconds >= 0.0) ||
                Fail(error, "Connection elapsed time must be finite and nonnegative.");
        }

        f64 AddAge(f64 age, f64 elapsedSeconds)
        {
            // All reference deadlines expire by ten seconds. Saturation also
            // makes huge finite elapsed input safe without overflowing timers.
            return (std::min)(11.0, age + (std::min)(11.0, elapsedSeconds));
        }

        Protocol::Message HelloMessage(u64 session, const Protocol::ContentId& content)
        {
            Protocol::Hello hello;
            hello.content = content;
            return {session, hello};
        }

        bool CompatibleHello(
            const Protocol::Message& message, const Protocol::ContentId& content,
            std::string& error)
        {
            const auto* hello = std::get_if<Protocol::Hello>(&message.body);
            if (!hello)
            {
                return Fail(error, "Expected a Hello message for connection compatibility.");
            }
            Protocol::Error diagnostic;
            if (!Protocol::ValidateCompatibility(*hello, content, diagnostic))
            {
                error = "Connection compatibility rejected: " + diagnostic.message;
                return false;
            }
            return true;
        }

        enum class ChannelResult
        {
            Continue,
            Closed,
            Failed
        };

        enum class QueueResult
        {
            Queued,
            Backpressure,
            InvalidMessage
        };

        struct QueuedFrame
        {
            std::vector<u8> bytes;
            std::size_t offset = 0;
            f64 age = 0.0;
        };

        /** Framing and bounded byte work stay game-owned, above the native stream. */
        class FramedChannel final
        {
        public:
            Platform::TcpStream stream;
            Protocol::FrameDecoder decoder;
            std::deque<QueuedFrame> outgoing;
            std::size_t queuedBytes = 0;
            u64 receivedFrames = 0;
            f64 frameAge = 0.0;
            f64 idleAge = 0.0;
            f64 heartbeatAge = 0.0;

            QueueResult TryQueue(const Protocol::Message& message, std::string& error)
            {
                // Validate before queue admission so malformed state cannot hide
                // indefinitely behind an unrelated full queue.
                std::vector<u8> bytes;
                Protocol::Error diagnostic;
                if (!Protocol::Encode(message, bytes, diagnostic))
                {
                    error = "Cannot queue protocol message: " + diagnostic.message;
                    return QueueResult::InvalidMessage;
                }
                if (outgoing.size() >= RTSConnectionLimits::MaximumQueuedFrames)
                {
                    error = "Connection outgoing frame queue is full; retry after polling.";
                    return QueueResult::Backpressure;
                }
                if (bytes.size() > RTSConnectionLimits::MaximumQueuedBytes - queuedBytes)
                {
                    error = "Connection outgoing byte budget is full; retry after polling.";
                    return QueueResult::Backpressure;
                }
                queuedBytes += bytes.size();
                outgoing.push_back({std::move(bytes), 0, 0.0});
                error.clear();
                return QueueResult::Queued;
            }

            bool Queue(const Protocol::Message& message, std::string& error)
            {
                return TryQueue(message, error) == QueueResult::Queued;
            }

            bool AdvanceTimers(f64 elapsedSeconds, bool ready, std::string& error)
            {
                if (decoder.GetBufferedByteCount() > 0 && !decoder.HasMessage())
                {
                    frameAge = AddAge(frameAge, elapsedSeconds);
                    if (frameAge >= RTSConnectionLimits::FrameTimeoutSeconds)
                    {
                        return Fail(error, "Incomplete protocol frame exceeded its absolute assembly deadline.");
                    }
                }
                for (auto& frame : outgoing)
                {
                    frame.age = AddAge(frame.age, elapsedSeconds);
                }
                if (!outgoing.empty() && outgoing.front().age >= RTSConnectionLimits::WriteTimeoutSeconds)
                {
                    return Fail(error, "Outgoing protocol frame exceeded its absolute write deadline.");
                }
                if (ready)
                {
                    idleAge = AddAge(idleAge, elapsedSeconds);
                    heartbeatAge = AddAge(heartbeatAge, elapsedSeconds);
                    if (idleAge >= RTSConnectionLimits::IdleTimeoutSeconds)
                    {
                        return Fail(error, "Connection received no complete valid message before its idle deadline.");
                    }
                }
                return true;
            }

            ChannelResult Flush(std::size_t& budget, std::string& error)
            {
                while (!outgoing.empty() && budget > 0)
                {
                    QueuedFrame& frame = outgoing.front();
                    const std::size_t amount = (std::min)(budget, frame.bytes.size() - frame.offset);
                    const auto transfer = stream.Send(frame.bytes.data() + frame.offset, amount, error);
                    if (transfer.status == TcpStatus::WouldBlock)
                    {
                        return ChannelResult::Continue;
                    }
                    if (transfer.status == TcpStatus::Closed)
                    {
                        error = "Peer closed the TCP connection while writing a protocol frame.";
                        return ChannelResult::Closed;
                    }
                    if (transfer.status == TcpStatus::Error)
                    {
                        return ChannelResult::Failed;
                    }
                    if (transfer.bytes == 0 || transfer.bytes > amount)
                    {
                        error = "TCP send returned an invalid progress count.";
                        return ChannelResult::Failed;
                    }
                    frame.offset += transfer.bytes;
                    budget -= transfer.bytes;
                    if (frame.offset == frame.bytes.size())
                    {
                        queuedBytes -= frame.bytes.size();
                        outgoing.pop_front();
                    }
                }
                return ChannelResult::Continue;
            }

            template <typename Handler>
            ChannelResult Receive(Handler&& handler, std::string& error)
            {
                std::size_t budget = RTSConnectionLimits::BytesPerPoll / 2;
                std::size_t frames = 0;
                std::array<u8, 8192> bytes{};
                while (frames < RTSConnectionLimits::FramesPerPoll)
                {
                    if (decoder.HasMessage())
                    {
                        Protocol::Message message;
                        if (!decoder.TakeMessage(message))
                        {
                            error = "Completed frame could not publish its protocol message.";
                            return ChannelResult::Failed;
                        }
                        frameAge = 0.0;
                        ++receivedFrames;
                        if (!handler(std::move(message), error))
                        {
                            return ChannelResult::Failed;
                        }
                        idleAge = 0.0;
                        ++frames;
                        continue;
                    }
                    if (budget == 0)
                    {
                        break;
                    }
                    const std::size_t needed = decoder.GetBytesNeeded();
                    if (needed == 0)
                    {
                        error = "Protocol frame decoder cannot accept more bytes.";
                        return ChannelResult::Failed;
                    }
                    const std::size_t amount = (std::min)(
                        (std::min)(bytes.size(), budget), needed);
                    const auto transfer = stream.Receive(bytes.data(), amount, error);
                    if (transfer.status == TcpStatus::WouldBlock)
                    {
                        return ChannelResult::Continue;
                    }
                    if (transfer.status == TcpStatus::Closed)
                    {
                        if (decoder.GetBufferedByteCount() != 0)
                        {
                            error = "Peer closed the TCP connection with an incomplete protocol frame.";
                            return ChannelResult::Failed;
                        }
                        error = "Peer closed the TCP connection.";
                        return ChannelResult::Closed;
                    }
                    if (transfer.status == TcpStatus::Error)
                    {
                        return ChannelResult::Failed;
                    }
                    if (transfer.bytes == 0 || transfer.bytes > amount)
                    {
                        error = "TCP receive returned an invalid progress count.";
                        return ChannelResult::Failed;
                    }
                    budget -= transfer.bytes;
                    std::size_t consumed = 0;
                    Protocol::Error diagnostic;
                    if (!decoder.Feed(bytes.data(), transfer.bytes, consumed, diagnostic))
                    {
                        error = "Protocol frame rejected: " + diagnostic.message;
                        return ChannelResult::Failed;
                    }
                    if (consumed != transfer.bytes)
                    {
                        error = "Protocol frame decoder did not consume its bounded receive chunk.";
                        return ChannelResult::Failed;
                    }
                }
                return ChannelResult::Continue;
            }

            void Heartbeat(u64 session, const Protocol::ContentId& content)
            {
                if (heartbeatAge >= RTSConnectionLimits::HeartbeatSeconds)
                {
                    std::string ignored;
                    if (Queue(HelloMessage(session, content), ignored))
                    {
                        heartbeatAge = 0.0;
                    }
                }
            }

            void Reset(bool clearCounters = true)
            {
                stream.Close();
                decoder.Reset();
                outgoing.clear();
                queuedBytes = 0;
                frameAge = 0.0;
                idleAge = 0.0;
                heartbeatAge = 0.0;
                if (clearCounters) receivedFrames = 0;
            }
        };

        bool IsRunning(RTSConnectionState state)
        {
            return state == RTSConnectionState::Listening || state == RTSConnectionState::Connecting ||
                state == RTSConnectionState::Handshaking || state == RTSConnectionState::Ready;
        }
    }

    struct RTSHostConnection::Impl
    {
        explicit Impl(RTSHostSession& value) : session(value) {}

        bool Terminate(RTSConnectionState terminal, std::string& error)
        {
            terminalError = error;
            channel.Reset(false);
            listener.Close();
            localPort = 0;
            if (ownsSession)
            {
                // Never terminate a newer session that the caller replaced externally.
                if (session.GetSessionId() == epoch)
                {
                    session.End();
                }
                ownsSession = false;
            }
            state = terminal;
            return false;
        }

        bool Handle(Protocol::Message message, std::string& error)
        {
            if (message.session != epoch)
            {
                return Fail(error, "Peer message belongs to another host session epoch.");
            }
            if (state == RTSConnectionState::Handshaking)
            {
                if (!CompatibleHello(message, content, error))
                {
                    return false;
                }
                const auto& hello = std::get<Protocol::Hello>(message.body);
                if (!session.AddPeer(remotePeer, remoteOwner, hello, error))
                {
                    return false;
                }
                Protocol::Message initial;
                if (!session.CaptureSnapshot(remotePeer, initial, error) || !channel.Queue(initial, error))
                {
                    return false;
                }
                lastPublishedTick = std::get<Protocol::FullSnapshot>(initial.body).tick;
                state = RTSConnectionState::Ready;
                phaseAge = 0.0;
                return true;
            }
            if (std::holds_alternative<Protocol::Hello>(message.body))
            {
                return CompatibleHello(message, content, error);
            }
            if (!std::holds_alternative<Protocol::MoveCommand>(message.body))
            {
                return Fail(error, "Host connection only accepts client move commands or keepalives.");
            }
            std::string admissionError;
            if (!session.SubmitMove(remotePeer, message, admissionError))
            {
                ++rejectedCommands;
                lastAdmissionError = std::move(admissionError);
            }
            return true;
        }

        RTSHostSession& session;
        Platform::TcpListener listener;
        FramedChannel channel;
        RTSConnectionState state = RTSConnectionState::Idle;
        Protocol::ContentId content{};
        u64 epoch = 0;
        u64 remotePeer = 0;
        u32 remoteOwner = 0;
        u16 localPort = 0;
        u64 lastPublishedTick = 0;
        u64 rejectedCommands = 0;
        f64 phaseAge = 0.0;
        bool ownsSession = false;
        std::string terminalError;
        std::string lastAdmissionError;
    };

    RTSHostConnection::RTSHostConnection(RTSHostSession& session)
        : m_impl(std::make_unique<Impl>(session))
    {
    }

    RTSHostConnection::~RTSHostConnection()
    {
        Stop();
    }

    bool RTSHostConnection::Listen(
        const Platform::TcpEndpoint& endpoint, u64 remotePeer, u32 remoteOwner, std::string& error)
    {
        if (IsRunning(m_impl->state))
        {
            return Fail(error, "Stop the active host connection before listening again.");
        }
        if (!m_impl->session.IsActive() || remotePeer == 0 || remoteOwner == 0)
        {
            return Fail(error, "Listening requires an active host session and a nonzero remote peer/owner assignment.");
        }
        Platform::TcpListener staged;
        if (!staged.Listen(endpoint, error))
        {
            return false;
        }
        m_impl->channel.Reset();
        m_impl->listener = std::move(staged);
        m_impl->epoch = m_impl->session.GetSessionId();
        m_impl->content = m_impl->session.GetContentId();
        m_impl->remotePeer = remotePeer;
        m_impl->remoteOwner = remoteOwner;
        m_impl->localPort = m_impl->listener.GetLocalPort();
        m_impl->phaseAge = 0.0;
        m_impl->rejectedCommands = 0;
        m_impl->lastPublishedTick = 0;
        m_impl->lastAdmissionError.clear();
        m_impl->terminalError.clear();
        m_impl->ownsSession = true;
        m_impl->state = RTSConnectionState::Listening;
        error.clear();
        return true;
    }

    bool RTSHostConnection::Poll(f64 elapsedSeconds, std::string& error)
    {
        if (!ValidElapsed(elapsedSeconds, error))
        {
            return false;
        }
        if (!IsRunning(m_impl->state))
        {
            error = m_impl->terminalError.empty() ? "Host connection is not active." : m_impl->terminalError;
            return false;
        }
        if (!m_impl->session.IsActive() || m_impl->session.GetSessionId() != m_impl->epoch)
        {
            error = "Borrowed host session ended or changed its epoch.";
            return m_impl->Terminate(RTSConnectionState::Failed, error);
        }
        f64 channelElapsed = elapsedSeconds;
        if (m_impl->state == RTSConnectionState::Listening)
        {
            const TcpStatus accepted = m_impl->listener.Accept(m_impl->channel.stream, error);
            if (accepted == TcpStatus::WouldBlock)
            {
                error.clear();
                return true;
            }
            if (accepted != TcpStatus::Progress)
            {
                return m_impl->Terminate(RTSConnectionState::Failed, error);
            }
            m_impl->listener.Close();
            channelElapsed = 0.0;
            m_impl->state = RTSConnectionState::Handshaking;
            m_impl->phaseAge = 0.0;
            if (!m_impl->channel.Queue(HelloMessage(m_impl->epoch, m_impl->content), error))
            {
                return m_impl->Terminate(RTSConnectionState::Failed, error);
            }
        }
        else if (m_impl->state == RTSConnectionState::Handshaking)
        {
            m_impl->phaseAge = AddAge(m_impl->phaseAge, elapsedSeconds);
            if (m_impl->phaseAge >= RTSConnectionLimits::HandshakeTimeoutSeconds)
            {
                error = "Host compatibility handshake exceeded its absolute deadline.";
                return m_impl->Terminate(RTSConnectionState::Failed, error);
            }
        }
        if (!m_impl->channel.AdvanceTimers(
            channelElapsed, m_impl->state == RTSConnectionState::Ready, error))
        {
            return m_impl->Terminate(RTSConnectionState::Failed, error);
        }
        std::size_t sendBudget = RTSConnectionLimits::BytesPerPoll / 2;
        auto result = m_impl->channel.Flush(sendBudget, error);
        if (result == ChannelResult::Continue)
        {
            result = m_impl->channel.Receive(
                [this](Protocol::Message message, std::string& diagnostic)
                {
                    return m_impl->Handle(std::move(message), diagnostic);
                }, error);
        }
        if (result == ChannelResult::Continue)
        {
            if (m_impl->state == RTSConnectionState::Ready)
            {
                m_impl->channel.Heartbeat(m_impl->epoch, m_impl->content);
            }
            result = m_impl->channel.Flush(sendBudget, error);
        }
        if (result != ChannelResult::Continue)
        {
            return m_impl->Terminate(result == ChannelResult::Closed
                ? RTSConnectionState::Closed : RTSConnectionState::Failed, error);
        }
        error.clear();
        return true;
    }

    RTSSnapshotPublication RTSHostConnection::TryPublishSnapshot(std::string& error)
    {
        if (m_impl->state != RTSConnectionState::Ready)
        {
            error = "PublishSnapshot requires a ready host connection.";
            return RTSSnapshotPublication::NotReady;
        }
        if (!m_impl->session.IsActive() || m_impl->session.GetSessionId() != m_impl->epoch)
        {
            error = "Host session is no longer active for snapshot publication.";
            return RTSSnapshotPublication::InvalidState;
        }
        Protocol::Message snapshot;
        if (!m_impl->session.CaptureSnapshot(m_impl->remotePeer, snapshot, error))
        {
            return RTSSnapshotPublication::InvalidState;
        }
        const auto* fullSnapshot = std::get_if<Protocol::FullSnapshot>(&snapshot.body);
        if (!fullSnapshot || snapshot.session != m_impl->epoch)
        {
            error = "Snapshot capture returned an invalid message kind or host session epoch.";
            return RTSSnapshotPublication::InvalidState;
        }
        const u64 tick = fullSnapshot->tick;
        if (tick <= m_impl->lastPublishedTick)
        {
            error = "Snapshot publication requires a newer authoritative simulation tick.";
            return RTSSnapshotPublication::NotReady;
        }
        const QueueResult result = m_impl->channel.TryQueue(snapshot, error);
        if (result == QueueResult::Backpressure)
        {
            return RTSSnapshotPublication::Backpressure;
        }
        if (result != QueueResult::Queued)
        {
            return RTSSnapshotPublication::InvalidState;
        }
        m_impl->lastPublishedTick = tick;
        return RTSSnapshotPublication::Queued;
    }

    bool RTSHostConnection::PublishSnapshot(std::string& error)
    {
        return TryPublishSnapshot(error) == RTSSnapshotPublication::Queued;
    }

    void RTSHostConnection::Stop()
    {
        std::string error = "Host connection stopped.";
        (void)m_impl->Terminate(RTSConnectionState::Closed, error);
    }

    RTSConnectionState RTSHostConnection::GetState() const { return m_impl->state; }
    u16 RTSHostConnection::GetLocalPort() const { return m_impl->localPort; }
    std::size_t RTSHostConnection::GetQueuedBytes() const { return m_impl->channel.queuedBytes; }
    std::size_t RTSHostConnection::GetQueuedFrameCount() const { return m_impl->channel.outgoing.size(); }
    std::size_t RTSHostConnection::GetBufferedByteCount() const { return m_impl->channel.decoder.GetBufferedByteCount(); }
    u64 RTSHostConnection::GetReceivedFrameCount() const { return m_impl->channel.receivedFrames; }
    u64 RTSHostConnection::GetRejectedCommandCount() const { return m_impl->rejectedCommands; }
    const std::string& RTSHostConnection::GetLastAdmissionError() const { return m_impl->lastAdmissionError; }

    struct RTSClientConnection::Impl
    {
        bool Terminate(RTSConnectionState terminal, std::string& error)
        {
            terminalError = error;
            channel.Reset(false);
            latestSnapshot.reset();
            epoch = 0;
            state = terminal;
            return false;
        }

        bool Handle(Protocol::Message message, std::string& error)
        {
            if (state == RTSConnectionState::Handshaking && !receivedHello)
            {
                if (!CompatibleHello(message, content, error))
                {
                    return false;
                }
                epoch = message.session;
                if (!channel.Queue(HelloMessage(epoch, content), error))
                {
                    return false;
                }
                receivedHello = true;
                return true;
            }
            if (message.session != epoch)
            {
                return Fail(error, "Host message belongs to another connection session epoch.");
            }
            if (state == RTSConnectionState::Ready && std::holds_alternative<Protocol::Hello>(message.body))
            {
                return CompatibleHello(message, content, error);
            }
            const auto* snapshot = std::get_if<Protocol::FullSnapshot>(&message.body);
            if (!snapshot)
            {
                return Fail(error, "Client connection expects a full snapshot or a ready-state keepalive.");
            }
            if (latestSnapshot && snapshot->tick <= lastSnapshotTick)
            {
                return Fail(error, "Host snapshot tick did not increase.");
            }
            if (snapshot->lastQueuedSequence < lastAcknowledgedSequence ||
                snapshot->lastQueuedSequence > lastQueuedSequence)
            {
                return Fail(error, "Host queue acknowledgement regressed or exceeds locally queued commands.");
            }
            lastSnapshotTick = snapshot->tick;
            lastAcknowledgedSequence = snapshot->lastQueuedSequence;
            latestSnapshot = std::move(message);
            state = RTSConnectionState::Ready;
            phaseAge = 0.0;
            return true;
        }

        FramedChannel channel;
        RTSConnectionState state = RTSConnectionState::Idle;
        Protocol::ContentId content{};
        std::optional<Protocol::Message> latestSnapshot;
        u64 epoch = 0;
        u64 lastQueuedSequence = 0;
        u64 lastAcknowledgedSequence = 0;
        u64 lastSnapshotTick = 0;
        f64 phaseAge = 0.0;
        bool receivedHello = false;
        std::string terminalError;
    };

    RTSClientConnection::RTSClientConnection() : m_impl(std::make_unique<Impl>()) {}
    RTSClientConnection::~RTSClientConnection() { Stop(); }

    bool RTSClientConnection::Connect(
        const Platform::TcpEndpoint& endpoint, const Protocol::ContentId& content, std::string& error)
    {
        if (IsRunning(m_impl->state))
        {
            return Fail(error, "Stop the active client connection before connecting again.");
        }
        Protocol::Hello hello;
        hello.content = content;
        Protocol::Error diagnostic;
        if (!Protocol::ValidateCompatibility(hello, content, diagnostic))
        {
            error = "Invalid local compatibility configuration: " + diagnostic.message;
            return false;
        }
        Platform::TcpStream staged;
        if (!staged.Connect(endpoint, error))
        {
            return false;
        }
        m_impl->channel.Reset();
        m_impl->channel.stream = std::move(staged);
        m_impl->content = content;
        m_impl->epoch = 0;
        m_impl->lastQueuedSequence = 0;
        m_impl->lastAcknowledgedSequence = 0;
        m_impl->lastSnapshotTick = 0;
        m_impl->phaseAge = 0.0;
        m_impl->receivedHello = false;
        m_impl->latestSnapshot.reset();
        m_impl->terminalError.clear();
        m_impl->state = RTSConnectionState::Connecting;
        error.clear();
        return true;
    }

    bool RTSClientConnection::Poll(f64 elapsedSeconds, std::string& error)
    {
        if (!ValidElapsed(elapsedSeconds, error))
        {
            return false;
        }
        if (!IsRunning(m_impl->state))
        {
            error = m_impl->terminalError.empty() ? "Client connection is not active." : m_impl->terminalError;
            return false;
        }
        f64 channelElapsed = elapsedSeconds;
        if (m_impl->state == RTSConnectionState::Connecting)
        {
            m_impl->phaseAge = AddAge(m_impl->phaseAge, elapsedSeconds);
            if (m_impl->phaseAge >= RTSConnectionLimits::ConnectTimeoutSeconds)
            {
                error = "TCP connection attempt exceeded its absolute deadline.";
                return m_impl->Terminate(RTSConnectionState::Failed, error);
            }
            const auto connected = m_impl->channel.stream.PollConnect(error);
            if (connected == TcpStatus::WouldBlock)
            {
                error.clear();
                return true;
            }
            if (connected != TcpStatus::Progress)
            {
                return m_impl->Terminate(RTSConnectionState::Failed, error);
            }
            m_impl->state = RTSConnectionState::Handshaking;
            m_impl->phaseAge = 0.0;
            channelElapsed = 0.0;
        }
        else if (m_impl->state == RTSConnectionState::Handshaking)
        {
            m_impl->phaseAge = AddAge(m_impl->phaseAge, elapsedSeconds);
            if (m_impl->phaseAge >= RTSConnectionLimits::HandshakeTimeoutSeconds)
            {
                error = "Client compatibility handshake exceeded its absolute deadline.";
                return m_impl->Terminate(RTSConnectionState::Failed, error);
            }
        }
        if (!m_impl->channel.AdvanceTimers(
            channelElapsed, m_impl->state == RTSConnectionState::Ready, error))
        {
            return m_impl->Terminate(RTSConnectionState::Failed, error);
        }
        std::size_t sendBudget = RTSConnectionLimits::BytesPerPoll / 2;
        auto result = m_impl->channel.Flush(sendBudget, error);
        if (result == ChannelResult::Continue)
        {
            result = m_impl->channel.Receive(
                [this](Protocol::Message message, std::string& diagnostic)
                {
                    return m_impl->Handle(std::move(message), diagnostic);
                }, error);
        }
        if (result == ChannelResult::Continue)
        {
            if (m_impl->state == RTSConnectionState::Ready)
            {
                m_impl->channel.Heartbeat(m_impl->epoch, m_impl->content);
            }
            result = m_impl->channel.Flush(sendBudget, error);
        }
        if (result != ChannelResult::Continue)
        {
            return m_impl->Terminate(result == ChannelResult::Closed
                ? RTSConnectionState::Closed : RTSConnectionState::Failed, error);
        }
        error.clear();
        return true;
    }

    bool RTSClientConnection::QueueMove(const Protocol::MoveCommand& command, std::string& error)
    {
        if (m_impl->state != RTSConnectionState::Ready)
        {
            return Fail(error, "QueueMove requires a ready client connection.");
        }
        if (command.sequence <= m_impl->lastQueuedSequence)
        {
            return Fail(error, "Client move sequence must increase without replay or wrap.");
        }
        if (!m_impl->channel.Queue({m_impl->epoch, command}, error))
        {
            return false;
        }
        m_impl->lastQueuedSequence = command.sequence;
        return true;
    }

    void RTSClientConnection::Stop()
    {
        std::string error = "Client connection stopped.";
        (void)m_impl->Terminate(RTSConnectionState::Closed, error);
    }

    RTSConnectionState RTSClientConnection::GetState() const { return m_impl->state; }
    u64 RTSClientConnection::GetSessionId() const { return m_impl->epoch; }
    const Protocol::Message* RTSClientConnection::GetLatestSnapshot() const
    {
        return m_impl->latestSnapshot ? &*m_impl->latestSnapshot : nullptr;
    }
    std::size_t RTSClientConnection::GetQueuedBytes() const { return m_impl->channel.queuedBytes; }
    std::size_t RTSClientConnection::GetQueuedFrameCount() const { return m_impl->channel.outgoing.size(); }
    std::size_t RTSClientConnection::GetBufferedByteCount() const { return m_impl->channel.decoder.GetBufferedByteCount(); }
    u64 RTSClientConnection::GetReceivedFrameCount() const { return m_impl->channel.receivedFrames; }
}
