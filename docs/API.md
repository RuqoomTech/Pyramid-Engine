# API overview

Public headers under `Engine/*/include` are the signature-level source of truth. `PYRAMID_VERSION_STRING` is exported as a target compile definition and currently evaluates to `0.6.0-pre-alpha`.

## Application

Header: `Pyramid/Core/Game.hpp`

```cpp
class Game
{
public:
    explicit Game(GraphicsAPI api = GraphicsAPI::OpenGL);
    void run();
    void quit();
    bool IsInitialized() const;

protected:
    virtual void onCreate();
    virtual void onUpdate(float deltaTime);
    f64 GetFrameElapsedSeconds() const;
    virtual void onRender();
    virtual void onWindowResize(const WindowResizeEvent& event);
    IGraphicsDevice* GetGraphicsDevice() const;
    const InputState& GetInput() const;
    InputActionSystem& GetInputActions();
    const InputActionSystem& GetInputActions() const;
    void SetActiveCamera(Camera* camera);
    Camera* GetActiveCamera() const;
    void SetRenderSystem(Renderer::RenderSystem* renderSystem);
    Renderer::RenderSystem* GetRenderSystem() const;
    bool IsRenderSurfaceAvailable() const;
};
```

Only `GraphicsAPI::OpenGL` is supported. A derived `onCreate()` must call `Game::onCreate()` first.

The `onUpdate(float)` argument remains capped at 1/30 second for existing camera/UI behavior. Protected `GetFrameElapsedSeconds()` returns the uncapped `steady_clock` sample in double seconds for the current update frame, or zero before the first sample. Simulation consumers define their own pause and overload policy. The accessor adds engine object state; rebuild pre-alpha consumers for the changed `Game` layout.

## TCP byte transport

Header: `Pyramid/Platform/TCP.hpp`, namespace `Pyramid::Platform`. `TcpStream` and `TcpListener` are move-only, single-threaded nonblocking byte handles with native types hidden by Pimpl. Only the Windows implementation is supported. `TcpEndpoint` contains numeric IPv4 text and a port; DNS, IPv6, embedded NULs, and malformed addresses fail explicitly. Destination ports must be nonzero; listeners permit port 0 and report the selected port through `GetLocalPort`. The default address is loopback; LAN listeners may explicitly bind an interface or `0.0.0.0`.

`Connect` starts a stream and may leave it `Connecting`. `PollConnect` uses zero-wait readiness and reports `Progress` only after completion; `GetState` distinguishes Closed/Connecting/Connected. `Listen` and `Accept` likewise never wait. Accept requires a closed output, preserves it on idle/failure, and gives an accepted stream its own retained Winsock lifetime independently of the listener. Reopening a live handle fails without replacing it. Move construction/assignment transfers ownership; moved-from objects are safely closed. Close is idempotent and does not promise delivery of buffered application data.

`Send`/`Receive` return `TcpTransfer {status, bytes}` with Progress/WouldBlock/Closed/Error. One call transfers at most 64 KiB and may transfer a smaller prefix; callers retain unsent bytes. WouldBlock is normal idle. EOF or fatal native I/O closes the stream. Null/empty buffers fail without closing a live stream. Errors include native operation and Winsock code where applicable. The native layer owns no protocol, queue, deadlines, authentication, or game semantics. `ws2_32` is an OS link dependency and is propagated for static installed-engine consumers; no new bundled runtime middleware is required.

## Window

Header: `Pyramid/Platform/Window.hpp`

The window interface requires initialization, presentation, context activation, message processing, close-state reporting, title/size/position/visibility mutation, and minimized/maximized queries. The checked-in implementation is `Win32OpenGLWindow`.

Runtime assets deployed beside an executable should be resolved through the platform path API:

```cpp
#include <Pyramid/Platform/RuntimePath.hpp>

const auto fontPath = Pyramid::Platform::ResolveRuntimePath(
    "Fonts/PyramidArabic-64-sdf.pfont");
```

`GetExecutableDirectory()` returns the running binary's directory. `ResolveRuntimePath()` preserves absolute paths, prefers executable-relative assets, falls back to the current working directory for development inputs, and returns the executable-relative candidate for useful missing-file diagnostics. This prevents terminal, shortcut, IDE, or file-manager launch context from changing which packaged asset is loaded.

`Window::SetResizeCallback()` receives platform-neutral `WindowResizeEvent` values while `ProcessMessages()` dispatches native messages. Each event includes client width, client height, and a `Restored`, `Minimized`, or `Maximized` state. `WindowResizeEvent::HasRenderableArea()` is false for minimized or zero-sized windows. `Game` installs this callback, updates the default graphics viewport, synchronizes the registered active camera and render system, suspends rendering for non-renderable client areas, and then forwards the event to the overridable `onWindowResize()` hook.

```cpp
void MyGame::onWindowResize(const Pyramid::WindowResizeEvent& event)
{
    Game::onWindowResize(event);
    if (!event.HasRenderableArea())
        return;

    // Default viewport, the camera registered with SetActiveCamera(), and the
    // RenderSystem registered with SetRenderSystem() are already synchronized.
    // Resize standalone framebuffers here.
}
```

## Input

Headers:

- `Pyramid/Platform/Input.hpp`
- `Pyramid/Input/InputActions.hpp`
- `Pyramid/Graphics/CameraController.hpp`

`Win32OpenGLWindow` converts native keyboard, mouse-button, pointer, wheel, and focus messages into one platform-neutral `InputState`. `Window::ProcessMessages()` starts a new input frame before dispatching messages, so transitions and deltas are valid during the immediately following `Game::onUpdate()`.

```cpp
void MyGame::onUpdate(float deltaTime)
{
    Game::onUpdate(deltaTime);
    const auto& input = GetInput();

    if (input.WasKeyPressed(Pyramid::Key::Escape))
        quit();

    if (input.IsKeyDown(Pyramid::Key::W))
        MoveForward(deltaTime);

    if (input.IsMouseButtonDown(Pyramid::MouseButton::Right))
    {
        const auto delta = input.GetMouseDelta();
        RotateCamera(delta.x, delta.y);
    }

    ZoomCamera(input.GetMouseWheelDelta());
}
```

Queries include held, pressed-this-frame, and released-this-frame states for keyboard and mouse buttons; `HasMousePosition()` for distinguishing a valid client-space pointer sample from default coordinates; client-space pointer position; aggregated pointer movement; and vertical/horizontal wheel steps. Native key-repeat messages do not retrigger `WasKeyPressed()`. Losing focus releases every held key/button and resets the pointer baseline, preventing stuck controls or a large mouse jump after focus returns.

`InputState` remains the low-level polling layer. `InputActionSystem` evaluates engine-generic named actions from that snapshot once per frame before `Game::onUpdate()`. It supports button, one-dimensional, and two-dimensional actions; prioritized enabled/disabled contexts; optional control consumption; key and mouse-button chords; mouse movement/wheel axes; and runtime binding replacement.

```cpp
void MyGame::onCreate()
{
    Game::onCreate();
    auto* gameplay = GetInputActions().CreateContext("gameplay");
    gameplay->AddAction("Move", Pyramid::InputActionType::Axis2D);
    gameplay->AddBinding(
        "Move",
        Pyramid::InputBinding::KeyBinding(
            Pyramid::Key::W,
            1.0f,
            Pyramid::InputAxisComponent::Y));
}

void MyGame::onUpdate(float deltaTime)
{
    Game::onUpdate(deltaTime);
    const auto move = GetInputActions().GetActionValue2D("gameplay", "Move");
    MovePlayer(move.x, move.y, deltaTime);
}
```

### Camera controllers

`CameraController` is a non-owning update interface over `Camera`. Pyramid provides three optional implementations:

- `FreeFlyCameraController` for six-degree movement;
- `OrbitCameraController` for target-centered orbit, pan, and zoom;
- `RTSCameraController` for XZ-ground-plane movement around a focus point.

Controllers consume configurable `CameraActionReference` values. They do not know which keys, mouse buttons, future gamepads, editor tools, or Baa scripts produce an action. Per-frame delta actions are applied directly; rate actions are multiplied by `deltaTime`.

```cpp
Pyramid::RTSCameraActions controllerActions;
controllerActions.move = {"camera", "Move"};
controllerActions.orbitDelta = {"camera", "OrbitDelta"};
controllerActions.orbitRate = {"camera", "OrbitRate"};
controllerActions.zoomDelta = {"camera", "ZoomDelta"};
controllerActions.reset = {"camera", "Reset"};

Pyramid::RTSCameraController controller(
    Pyramid::Math::Vec3::Zero,
    controllerActions);
controller.CaptureHome(camera);

// Game has already evaluated InputActionSystem for this frame.
controller.Update(camera, GetInputActions(), deltaTime);
```

Call `Synchronize()` after external code changes a camera pose, `CaptureHome()` to define the reset pose, and `SetEnabled(false)` when another mode owns the camera. Physical bindings remain in the game or editor layer. The optional `Examples/RTSReference` target demonstrates edge scrolling, selection, and command requests above these APIs; it is game-side source support, not installed `Pyramid::Engine` API.

The source-only `RTSUnitStore` exposes `AddUnit`, `FindUnit`, `RemoveUnit`, `PruneInvalidEntities`, and `Clear` for one gameplay scene. Each entry retains its original non-owning `Entity`; another scene or a new incarnation with the same numeric ID cannot access it. Duplicate IDs never overwrite a record, including stale records: prune invalid entries before registering replacements. Lookup returns borrowed, read-only state, invalidated by removal/pruning/clearing/store destruction and by a successful snapshot-replica Apply/Clear commit, including pointers to unrelated entries; re-query after these operations or scene lifetime changes. Neither removing state nor clearing the store destroys entities.

`RTSUnitState` initializes owner/team IDs, maximum/current integer health (100/100 by default), selectability, finite nonnegative movement speed (4 world units/second by default), and an optional movement target. Owner 0 is neutral; teams are metadata without diplomacy rules. `AddUnit` rejects invalid entities, zero maximum health, health above maximum, negative/non-finite speed, non-finite targets, and targets on dead, neutral, or parented units. `IsSelectable` requires a registered, live, selectable unit; the interaction controller additionally checks visibility/renderability. `SetSelectable` changes inspection policy independently of ownership. `ApplyDamage` saturates at zero and clears movement on death. `SetOwnership` clears movement whenever owner or team changes; `ClearMovementTarget` cancels an order.

`RTSCommandRequest` retains its numeric `entity`, finite `target`, and process-local `entityRef` facade. `ApplyMoveCommand(issuer, request)` rejects empty/mismatched/stale facades, unregistered/dead units, issuer 0, non-owners, parented entities, non-finite root positions, and non-finite targets without changing accepted state. Success records intent only. Manually constructed requests must populate all three fields; the facade is not a persistent or network identifier. Input selection retains the same lifetime identity and rechecks the selectable predicate and active scene before command generation. Pending requests are one-shot but may become stale before consumption, so admission remains mandatory. These single-threaded APIs return `false` on rejected mutations; `FindUnit` returns null on invalid identity. The store provides no transport, authentication, or gameplay serialization; movement execution is private to the reference simulation coordinator.

`RTSSimulation` borrows the unit store and runs at `FixedStepSeconds = 1/32` second. `QueueMoveCommand` retains a captured request and issuer by value; successful enqueue promises capacity and structural validity, not current authority. The FIFO holds at most 256 requests and rejects the newest on overflow without replacing accepted work. `Advance(elapsedSeconds)` drains requests once before movement on the next due tick, re-runs store admission, and reports `valid`, executed `steps`, accepted/rejected command counts, cancelled moves, and `droppedSeconds`. Zero/subtick frames retain requests. Negative or non-finite elapsed time returns invalid without changing queue, time, or scene state. Each advance executes at most 8 ticks, discards excess whole ticks, and retains the fractional remainder; huge finite inputs remain bounded. These are reference policies, not measured throughput targets.

Each tick visits units in ascending entity-ID order, computes straight-line movement in double precision, and writes only `Entity::SetLocalPosition`, preserving rotation/scale and inherited child transforms. Arrival snaps exactly to the target and clears it without overshoot. Movement supports root entities only: parented admission fails, and reparenting an active unit cancels/reports its order. Malformed positions, failed writes, or float-coordinate precision that prevents progress also cancel/report movement. Speed zero preserves an unfulfilled target; an already reached target clears. No navigation, collision, terrain-following, interpolation, or cross-platform deterministic replay is implied.

Call controller/input code once per frame, enqueue once, then advance ticks from uncapped elapsed time. `Suspend()` clears pending requests and fractional time, preserving tick count and already admitted targets. BasicGame passes local menu/focus/minimize state to `RTSMatch`: offline mode suspends and discards the resumed boundary interval; host networking and simulation continue after remote readiness regardless of local pause, while clients apply snapshots without advancing simulation. Scene/spatial synchronization follows published scene changes before picking. The match must be destroyed before its borrowed simulation, store, scene, and resource registry; the simulation must be destroyed before its store.

Contexts are evaluated from highest to lowest priority. An enabled consuming context blocks only controls that were active during that frame, allowing UI, editor, console, gameplay, and vehicle modes to coexist.

`InputConsumptionMask` reserves physical controls before context evaluation. Use `InputActionSystem::Update(input, mask)` when a higher layer has already handled keys, mouse buttons, pointer deltas, wheel input, or committed text. `Game::RegisterUIContext()` and `UnregisterUIContext()` merge registered UI masks automatically before the normal action update. Gamepad input, raw relative mouse mode, camera blending/collision, and persisted binding files remain future work.

### RTS protocol and host admission

Source-only headers: `RTSProtocol.hpp` and `RTSHostSession.hpp` under `Pyramid/Examples/RTSReference`. `Pyramid::RTSProtocol` is a CPU-only target linked to Foundation/Math; `Pyramid::RTSReference` supplies the scene/simulation adapter. Neither target is installed with the engine. Protocol version 1 is independent of scene-format versions and never serializes native `Entity`/weak pointers or resource handles.

`Protocol::Validate`, `Encode`, `Decode`, and `ValidateCompatibility` return explicit failure with an `ErrorCode`, byte offset, and short diagnostic. Success clears the error. Encode/decode leave their existing output untouched on failure. The caller supplies readable memory; parsing is bounded, not native crash isolation. Decode takes exactly one complete frame, rejecting trailing bytes, unsupported versions/kinds, reserved flags, excessive counts, invalid numeric state, and malformed lengths before publishing data. These are application frames, not UDP datagrams. Whole-frame decoding remains separate from incremental stream framing and transport.

| Header byte offset | Field |
|---|---|
| 0 | Four bytes `PRTS` |
| 4 | Little-endian `u16` protocol version, currently 1 |
| 6 | `u8` kind: Hello=1, MoveCommand=2, FullSnapshot=3 |
| 7 | Reserved `u8`, required zero |
| 8 | Little-endian `u32` payload bytes, exact frame length minus 20 |
| 12 | Little-endian nonzero `u64` session epoch |

Payload fields follow declaration order: Hello has rules version `u32`, tick rate `u32`, and 16 opaque content/catalog compatibility bytes; MoveCommand has increasing sequence `u64`, host-issued network unit ID `u64`, and XYZ target `f32`. It contains no issuer or requested execution time. FullSnapshot has host tick `u64`, recipient `lastQueuedSequence u64`, count `u32`, then strictly increasing unique unit records. Each record is unit ID `u64`; catalog, owner, team, maximum/current health `u32`; speed `f32`; position XYZ, rotation XYZW, scale XYZ `f32`; and one flags byte (bit 0 selectable, bit 1 target present), followed by optional XYZ target. Float encoding copies IEEE-754 binary32 bits, preserving signed zero. An empty roster and tick/queued-sequence zero are valid. Record sizes are 73 bytes without a target and 85 with one.

Version-1 limits are 256 KiB per frame and 2,048 root units per full snapshot, covering the proposed 2,000-unit format workload. Coordinate/target components must be finite with absolute value at most 1,000,000; movement speed is finite in 0..10,000; scale components are finite with absolute value at most 1,024 (zero/mirrored scale allowed). Rotation is finite with squared quaternion length within 0.001 of one. Unit/catalog IDs and maximum health are nonzero, health cannot exceed maximum, and dead/neutral units cannot carry targets. These wire limits are not global engine limits. Hello rules/tick fields and fingerprint must be nonzero; admission requires exact rules-version 1, 32 Hz, and expected fingerprint equality. The caller defines a reproducible fingerprint covering compatible content/catalog/rules. No hash generation, authentication, encryption, or integrity protection is supplied.

`RTSHostSession` borrows the unit store and simulation, exclusively owns the simulation queue while active, and must be destroyed first. `Begin` requires a fresh nonzero epoch greater than earlier epochs used by that object and a nonzero fingerprint; it discards prior queued simulation work. `AddPeer(peer, owner, hello)` validates compatibility and assigns at most two nonzero peer IDs distinct nonzero owners from trusted native connection context. The connection layer validates the handshake envelope/session and supplies that native assignment; a client packet never chooses its owner.

`BindUnit` mints non-reused session-local network IDs tied to captured entity lifetimes and catalog IDs, subject to the roster limit and valid root-unit representation. Failed calls preserve the output ID. `PruneInvalidUnits` removes expired entity bindings without reusing IDs. `SubmitMove` requires the active epoch, known peer/unit, increasing sequence (gaps allowed, wrap forbidden), and a live owned root unit. It advances the peer sequence only after successful queue insertion, allowing the same sequence to retry after queue saturation. Success acknowledges enqueue only; execution can later reject changed ownership, death, or stale identity. `CaptureSnapshot` atomically copies an ID-sorted complete roster and per-peer queue acknowledgement, including dead units and omitting destroyed/unregistered bindings. Invalid/parented host state fails capture without replacing prior output. Client catalog resolution and scene application belong to `RTSSnapshotReplica`, not this host adapter.

`End` (also on destruction) clears all queued simulation requests and movement intent on bound live units, then drops peer/binding state without destroying scene entities or resetting health/tick count. Disconnect must end the whole reference session; peer-specific queue revocation is not supported. New epochs may reuse numeric network IDs, but old-epoch commands cannot address them. `RTSMatch` composes this adapter with connections, passive client replicas, and BasicGame lifecycle controls; the host adapter itself owns no UI or presentation policy.

### Incremental framing and TCP connections

`Protocol::FrameDecoder::Feed(data, size, consumed, error)` consumes at most one complete frame. It accumulates the 20-byte header, validates it before reserving its bounded body, then uses the same typed decoder. The returned consumed count leaves any coalesced tail to the caller. A pending message consumes no further input until `TakeMessage` or `Reset`; failed Take preserves output. `GetBytesNeeded` supports exact bounded reads and returns zero for ready/failed states. `GetBufferedByteCount` exposes partial-frame progress. Malformed input remains terminal until Reset, and no caller input pointer is retained. Zero-length Feed may have a null pointer; nonempty null input fails. The protocol target remains CPU-only.

Source-only header `RTSConnection.hpp` supplies concrete `RTSHostConnection`/`RTSClientConnection` helpers with Idle/Listening/Connecting/Handshaking/Ready/Closed/Failed states. A host connection borrows an already active `RTSHostSession` and accepts one remote stream using a native-assigned peer/owner. `Listen` stages the listener; failure leaves the session unchanged. Once listening succeeds, Stop/destruction, EOF, malformed/wrong-role/wrong-epoch traffic, or timeout closes transport and Ends that original session. An externally replaced newer epoch is not ended by cleanup of the old connection.

The host sends Hello; the client validates compatibility/session and echoes Hello. Host registration/capture publishes the initial full snapshot as implicit acceptance. The client is not Ready until that snapshot arrives. Ready Hello messages with matching compatibility/epoch are keepalives, not another peer registration. Host Ready accepts move commands, and client Ready accepts full snapshots with strictly increasing ticks and nondecreasing queue acknowledgements no greater than locally queued sequences. Wrong-role or stale-snapshot traffic is terminal. A valid move rejected by game admission remains nonterminal and is exposed through `GetRejectedCommandCount`/`GetLastAdmissionError`, avoiding disconnects for ordinary stale/dead/non-owned orders.

`QueueMove` advances the client's sequence only after outgoing enqueue succeeds. `TryPublishSnapshot` returns `RTSSnapshotPublication::Queued`, `Backpressure`, `NotReady`, or `InvalidState`; only Backpressure represents retryable queue saturation. A newer authoritative tick is required, and invalid capture/encoding must not be retried as congestion. The compatibility wrapper `PublishSnapshot` returns true only for Queued. The client retains only the latest complete `Protocol::Message`; its borrowed `GetLatestSnapshot` pointer expires on replacement or terminal closure/Stop. The connection does not mutate scenes; `RTSMatch` passes this data to its replica. `GetQueuedBytes`, `GetQueuedFrameCount`, `GetBufferedByteCount`, and `GetReceivedFrameCount` support diagnostics; decoded-frame counts survive terminal closure and reset on a new start.

| Reference connection limit | Policy |
|---|---|
| Outgoing storage | At most 8 complete frame buffers and 1 MiB total, including partially sent heads; reject newest on overflow |
| Work per Poll | 256 KiB combined, split equally between sending/receiving; at most 16 decoded incoming frames |
| Connect / handshake | Absolute 5 seconds each |
| Partial frame / queued write | Absolute 5 seconds, not restarted by trickled bytes or partial sends |
| Ready receive idle | 10 seconds without a complete valid message |
| Keepalive | Matching Hello every 1 second while Ready, at most once per Poll |

`Poll(elapsedSeconds, error)` never waits; callers use uncapped monotonic elapsed seconds even while menus/focus pause local gameplay. Negative/nonfinite elapsed time fails without changing live state; huge finite values cannot overflow deadline accounting. No network thread is required. Partial reads/writes retain their offsets, and only complete validated frames reach handlers. Poll returns false for a terminal connection with an actionable diagnostic; normal would-block remains true. Ready-state admission rejection does not imply an execution acknowledgement. Host stepping and pause policy belong to `RTSMatch`, which keeps a ready host running during local focus/menu/minimize pauses. DNS/IPv6, reconnect/host migration, encryption/authentication, prediction/interpolation, and two-machine acceptance are not implemented or claimed.
### Client replicas and reference matches

Source-only `RTSSnapshotReplica.hpp` maps host-issued network IDs to automatically allocated client `Entity` lifetimes. It borrows a scene, unit store, and optional registry; all must outlive it. `SetCatalog(id, MeshRendererComponent, error)` registers trusted native render descriptions, including immutable resource handles and bounds. Nonempty handles require a registry and valid generations; every Apply revalidates used catalog resources. A null registry permits only handle-free CPU fixtures. Explicit catalog upserts can refresh generations; catalog meanings must agree with the session content identity.

`Begin(session, error)` requires a nonzero epoch and an inactive replica; call `Clear` before changing sessions. `Apply(message, error)` accepts only a completely validated full snapshot for that epoch, with increasing ticks after the first snapshot and nonregressing queue acknowledgement. Unknown catalogs, stale resources, missing components/state, expired mapped entities, or native hierarchy changes fail before publication. Retained network IDs retain their client entity lifetimes; new IDs allocate fresh local entities, and omitted IDs remove only replica-owned entities/state. Network IDs are not used to allocate local scene IDs. `FindEntity` resolves only live mapped entities; `FindNetworkId` requires the original live facade. Reused numeric local IDs cannot redirect commands or cleanup.

Reconciliation stages gameplay state, new entities, and retained transform/renderer backups before committing membership and freshness. Ordinary validation/setter failures preserve accepted membership/state/freshness; allocation failures and native crashes are not contained. Managed entities must remain root and childless. `Clear(error)` preflights that invariant, preserves unrelated scene props and same-local-ID replacement entities, and keeps catalogs for reuse. Foreign parents/children block cleanup with a diagnostic instead of recursive destruction. Successful Apply/Clear commits can invalidate every borrowed `RTSUnitStore::FindUnit` pointer through store copy/swap; re-query all such pointers. Identity/catalog maps contain no second transform or gameplay model: scene components and the unit store remain authoritative locally, and the host remains gameplay authority.

`RTSMatch.hpp` composes the replica, host session, and connections in `None`, `Offline`, `Host`, and `Client` modes. It borrows the same scene/store/registry plus a simulation, exclusively controls that simulation queue, and runs on the game thread. Register catalog 1 before `StartOffline` or `Host`; it supplies the shared reference-unit renderer. Offline creates one owner/team-1 unit. Host creates two root units at X=-2/+2 assigned owners/teams 1 and 2, waits for remote readiness, then advances 32 Hz ticks and attempts one full snapshot after at least four further ticks (nominally 8 Hz). At most one snapshot is queued per Update; only Backpressure is retried, while capture/encoding failure ends the match. Join creates no local authoritative units: received snapshots alone publish the client scene and gameplay state, without local simulation or interpolation.

Call `Update(uncappedElapsedSeconds, localPaused, error)` every frame, including menus, focus loss, and minimization. Finite nonnegative elapsed time is required. Only offline mode follows localPaused; a ready host continues simulation and both network roles continue polling. Offline resume and the first ready host frame discard their boundary interval. `QueueCommand` accepts only a live root unit owned by `GetLocalOwner` and belonging to that match; client commands resolve through the replica to network IDs, while local host commands enter the simulation queue as owner 1. Native assignment makes the client owner 2. Queue admission is not execution acknowledgement. `IsReady`, `GetMode`, `GetStatus`, `GetTick`, `GetHostPort`, and `GetLastSimulationResult` expose progress without exposing transport internals.

`Stop(error)` closes transport, ends the exclusive host session, and suspends pending simulation work before removing only managed local/replica units. Unrelated props and cached resources survive. Unsafe native hierarchy edits leave mode None with actionable cleanup failure; detach foreign hierarchy and retry Stop. Starts replace the previous match after successful catalog validation where required; a failed later start can therefore leave no active match. Simulation tick count is not reset by Stop. `ParseRTSPort(text, output, error)` accepts a complete one-to-five-digit ASCII value in 1..65535 and preserves output on failure.

BasicGame's shared UI supplies offline start, LAN host/join, disconnect/menu controls, and status/owner/selection/health diagnostics. It registers catalog 1 with the resource registry and uses the explicit 16-byte `DemoContentId` revision `PYRAMID-RTS-0001`; native authors must change it when compatible catalog/content/rules change. This is manually maintained compatibility metadata, not an automatically computed asset hash or authentication. The CPU-only menu test uses the bundled production font at 1280x720 and 640x480; neither it nor loopback match tests establish native window, pixel, two-PC LAN, or performance acceptance.

### Unicode text events and clipboard

Physical key transitions and entered text are separate APIs. The Win32 backend feeds UTF-16 character messages into `InputState`, which combines surrogate pairs and emits Unicode scalar values:

```cpp
for (const Pyramid::TextInputEvent& event : GetInput().GetTextInputEvents())
{
    if (event.type == Pyramid::TextInputEventType::Commit)
    {
        for (char32_t codepoint : event.text)
            HandleCommittedCharacter(codepoint);
    }
}
```

`TextInputEventType` also reserves composition start/update/end events so a later IME implementation does not require replacing the public event model. Focus loss clears pending surrogates and text events. `InputConsumptionMask::ConsumeTextInput()` prevents committed text from reaching lower action layers while leaving unrelated controls available.

Include the clipboard contract with:

```cpp
#include <Pyramid/Platform/Clipboard.hpp>
```

`Game::GetClipboard()` returns the native window clipboard service. It exchanges UTF-32 text through a platform-neutral interface; the Win32 implementation uses `CF_UNICODETEXT`, normalizes clipboard line endings to LF, and bounds conversions. `ClipboardEncoding::Utf32ToUtf16()` and `Utf16ToUtf32()` are available for backend and test code.

## Font assets

Include the owned font pipeline with:

```cpp
#include <Pyramid/Font/Font.hpp>
#include <Pyramid/Platform/RuntimePath.hpp>
#include <Pyramid/Platform/SystemFont.hpp>
```

Development tools may parse a TrueType-outline `.ttf`, rasterize selected Unicode ranges, and save a deterministic processed asset:

```cpp
auto loaded = Pyramid::Font::LoadTrueTypeFile("Fonts/MyFont.ttf");
Pyramid::Font::BakeOptions options;
options.pixelHeight = 64.0f;
options.atlasWidth = 1024;
options.atlasHeight = 1024;
options.mode = Pyramid::Font::RasterMode::SignedDistanceField;
options.distanceRange = 10.0f;

auto baked = Pyramid::Font::BakeFont(loaded.face, options);
Pyramid::Font::SaveProcessedFontFile(
    baked.font, "Fonts/MyFont-64-sdf.pfont");
```

The first parser supports TrueType quadratic `glyf` outlines, compound glyphs, `cmap` formats 4/12, horizontal metrics, classic `kern` format 0, coverage/SDF rasterization, and bounded malformed-input rejection. CFF/OpenType outlines, collections, WOFF/WOFF2, variable/color fonts, bytecode hinting, and complex-script shaping are explicitly outside this version. Runtime applications should prefer checksummed version-2 `.pfont` assets; the loader remains compatible with version-1 coverage files. `PyramidFontCompiler` exposes `--sdf` and `--distance=N` for the same pipeline.

For runtime-provided font bytes, use the content-addressed cache so parsing and SDF generation occur only when the source or bake options change:

```cpp
Pyramid::Font::BakeOptions bake;
bake.pixelHeight = 64.0f;
bake.atlasWidth = 2048;
bake.atlasHeight = 2048;
bake.mode = Pyramid::Font::RasterMode::SignedDistanceField;
bake.distanceRange = 10.0f;
bake.missingGlyphPolicy = Pyramid::Font::MissingGlyphPolicy::Skip;

auto cached = Pyramid::Font::LoadOrBakeProcessedFont(
    fontBytes.data(), fontBytes.size(), bake, cacheDirectory);
if (cached.Succeeded())
    auto atlas = Pyramid::Text::CreateFontAtlas(cached.font);
```

Use separate bounded bake options for Latin, Arabic presentation forms, and symbols instead of assuming one fixed atlas can hold every requested range from a large installed font. Validate the glyphs required by each text role before accepting a candidate into a fallback family.

On Windows, `Pyramid::Platform::LoadSystemFont()` extracts installed TrueType bytes only; it does not delegate parsing or rendering to GDI. Requested names are exact, case-insensitive family matches, so Windows font substitution is rejected:

```cpp
Pyramid::Platform::SystemFontRequest request;
request.preferredFamilies = {"Segoe UI"};
Pyramid::Platform::SystemFontData source;
std::string error;
if (Pyramid::Platform::LoadSystemFont(request, source, &error))
{
    const auto cache = Pyramid::Platform::GetUserCacheDirectory(
        "PyramidEngine/Fonts");
    // Pass source.bytes to LoadOrBakeProcessedFont as above.
}
```

## Text and UI

Include the renderer-independent APIs with:

```cpp
#include <Pyramid/Font/Font.hpp>
#include <Pyramid/Text/Text.hpp>
#include <Pyramid/UI/UI.hpp>
#include <Pyramid/Util/Log.hpp>
```

`Text::CreateDebugFontAtlas()` returns a deterministic embedded ASCII atlas. `Text::LoadFontAtlas()` converts a processed `.pfont` into the same renderer-neutral atlas contract. `Text::BuildFontFamily()` combines an ordered list of atlases into one renderer-ready fallback atlas; earlier fonts win duplicate code points, and `ResolveFontIndex()` reports the selected source. `UI::Context::SetFontAtlas()` selects one atlas, while `SetFontFamily()` enables the merged fallback family.

```cpp
Pyramid::Text::FontAtlas latin;
Pyramid::Text::FontAtlas arabic;
Pyramid::Text::LoadFontAtlas("Fonts/PyramidSans-64-sdf.pfont", latin);
Pyramid::Text::LoadFontAtlas("Fonts/PyramidArabic-64-sdf.pfont", arabic);

Pyramid::Text::FontFamily family;
std::string familyError;
if (Pyramid::Text::BuildFontFamily({latin, arabic}, family, &familyError))
    ui.SetFontFamily(family);
```

`Text::LayoutInternational()` accepts UTF-32 and `LayoutInternationalUtf8()` strictly decodes UTF-8. They resolve paragraph direction, build common Latin/Arabic/numeric visual runs, contextually shape the core Arabic alphabet, mirror common RTL punctuation, wrap at owned international word/CJK opportunities, and emit renderer-neutral glyphs plus logical ranges, visual clusters, caret stops, and selection geometry. `GetCaretLocation()`, `HitTestInternational()`, `MoveCaretVisual()`, and `BuildSelectionSpans()` expose that mapping to UI and future editor code. `Text::Layout()`, `Measure()`, and `BuildGlyphQuads()` remain compact legacy/convenience APIs.

`UI::Theme::typography` defines separate `TextStyle` values for headings, body copy, field labels, buttons, inputs, and captions. Each role controls relative scale and SDF optical weight. `UI::Context::Heading()` and `Caption()` expose explicit semantic roles; existing labels, buttons, panel titles, and editable controls select the appropriate role automatically.

`Text::TextBuffer` stores Unicode scalar values as logical indices, but cursor and selection mutations are constrained to extended-grapheme boundaries independently from UI rendering:

```cpp
Pyramid::Text::TextBuffer buffer(U"Kingdom");
buffer.SetMaximumCharacters(64);
buffer.MoveCursor(Pyramid::Text::CursorMove::DocumentEnd);
buffer.Insert(U" Ω");
buffer.SelectWordAt(buffer.GetCursor() - 1);
const std::string utf8 = buffer.GetUtf8();
```

It supports insertion, cluster-safe backspace/delete and previous/next movement, word/line/document movement, line-up/down movement, selection replacement, single-line normalization, read-only mode, and character limits. `Text::SegmentGraphemes()` and the boundary-navigation helpers expose the same owned segmentation contract. `Text::DecodeUtf8()` and `EncodeUtf8()` provide strict conversion at application boundaries.

A `UI::Context` owns retained widget state and accepts immediate calls between `BeginFrame()` and `EndFrame()`:

```cpp
Pyramid::UI::Context ui;
Pyramid::UI::FrameInfo frame{width, height, dpiScale, deltaTime};
auto& logger = Pyramid::Util::Logger::GetInstance();

ui.BeginFrame(frame, input);
if (ui.BeginPanel("DEBUG"))
{
    (void)ui.CollapsingHeader("PERFORMANCE", performanceOpen);
    if (performanceOpen)
        ui.LabelValue("FPS", "144");
    ui.Checkbox("PAUSED", paused);
    ui.SliderFloat("CAMERA SPEED", speed, 1.0f, 20.0f);
    ui.WrappedLabel("Pointer capture blocks camera and world interaction.");

    Pyramid::UI::ScrollAreaOptions scroll;
    scroll.height = 120.0f;
    scroll.stickToBottom = true;
    if (ui.BeginScrollArea("LOG", scroll))
    {
        for (const auto& entry : logger.GetRecentEntries(100))
            ui.WrappedLabel(entry.message);
        ui.EndScrollArea();
    }
    ui.EndPanel();
}
const Pyramid::UI::DrawList& drawList = ui.EndFrame();
```

Editable controls bind directly to UTF-8 application strings while retaining Unicode editing state internally:

```cpp
ui.SetClipboard(GetClipboard());

Pyramid::UI::TextFieldOptions nameOptions;
nameOptions.placeholder = "Kingdom name";
nameOptions.maximumCharacters = 64;
const auto nameResult = ui.TextField("KINGDOM NAME", kingdomName, nameOptions);

Pyramid::UI::TextAreaOptions notesOptions;
notesOptions.height = 160.0f;
notesOptions.maximumCharacters = 1024;
const auto notesResult = ui.MultilineTextArea("NOTES", notes, notesOptions);
```

`TextField`, `PasswordField`, `SearchField`, and `MultilineTextArea` support pointer caret placement, drag selection, double-click word selection, triple-click line selection, Shift selection, Ctrl+A/C/X/V, word navigation, Home/End, Enter submission, Escape rollback, placeholder/error states, and caret/selection rendering. They use international cluster maps for RTL-aware hit testing, visual Left/Right movement, selection geometry, wrapping, and password masking. A focused editor consumes character-producing and editing controls before gameplay actions, but leaves unrelated function keys available. The current owned subset does not yet implement explicit bidi controls/isolates, complete UAX #14, OpenType GSUB/GPOS, general complex scripts, advanced Arabic ligatures/mark positioning, or native IME composition/candidate presentation.

Persistent game flow uses the retained screen stack:

```cpp
#include <Pyramid/UI/GameUI.hpp>

Pyramid::UI::ScreenStack screens;
screens.Push(mainMenuScreen);
screens.Replace(gameplayHudScreen, {
    Pyramid::UI::ScreenTransitionType::Fade, 0.15f});
screens.Push(pauseModal);

screens.Update(deltaTime);
screens.Build(ui);

const bool gameplayBlocked = screens.BlocksGameplayInput();
```

Screens receive deterministic `OnEnter`, `OnExit`, `Update`, and `Build` callbacks. Push, pop, replace, and clear operations requested during callbacks are deferred until dispatch completes. `ResolveAnchoredRect()` and `ResolveDockedRect()` provide responsive placement, while `DrawList::AddNineSlice()` emits renderer-neutral scalable panel geometry.

Stable widget identity derives from the parent scope and label; use `PushId()`/`PopId()` when repeated labels need independent state. `PrepareInput()` hit-tests the previous retained frame and returns an `InputConsumptionMask` before action evaluation. Current widgets are panels, labels/value rows, colored and wrapped labels, separators, spacers, persistent collapsing headers, clipped vertical scroll areas, buttons, checkboxes, float sliders, progress bars, and images.

The engine graphics adapter is:

```cpp
#include <Pyramid/Graphics/UI/UIRenderer.hpp>

Pyramid::UIRenderer renderer;
renderer.Initialize(device, resources, ui.GetFontAtlas());
renderer.Render(ui.GetDrawList(), frame);
```

`UIRenderer` consumes `UI::DrawList`, supports the active processed or embedded fallback font plus registered `ITexture2D` IDs, binds framebuffer zero, establishes the DPI-scaled physical surface viewport, applies top-left scissor clipping, and restores the engine baseline render state. It should run after world rendering and before presentation. `RenderSystem` independently restores the main framebuffer and viewport after every render pass so shadow-map or off-screen dimensions cannot leak into later passes.

`Util::Logger` maintains an optional bounded in-memory history for diagnostics. `GetRecentEntries(maximum, minimumLevel)` returns the newest matching entries in chronological order, `SetHistoryCapacity()` trims deterministically, and `ClearHistory()` removes only the in-memory history; console/file sinks remain unchanged.

## Graphics device and resources

Primary headers:

- `Pyramid/Graphics/GraphicsDevice.hpp`
- `Pyramid/Graphics/Framebuffer.hpp`
- `Pyramid/Graphics/Buffer/*.hpp`
- `Pyramid/Graphics/Shader/Shader.hpp`
- `Pyramid/Graphics/Shader/ShaderProgram.hpp`
- `Pyramid/Graphics/Shader/ShaderCache.hpp`
- `Pyramid/Graphics/Texture.hpp`
- `Pyramid/Graphics/Texture/TextureResource.hpp`
- `Pyramid/Graphics/Texture/TextureCache.hpp`
- `Pyramid/Graphics/Geometry/Vertex.hpp`
- `Pyramid/Graphics/Geometry/Mesh.hpp`
- `Pyramid/Graphics/Geometry/MeshCache.hpp`
- `Pyramid/Graphics/Material/Material.hpp`
- `Pyramid/Graphics/Material/MaterialCache.hpp`
- `Pyramid/Graphics/Resources/ResourceHandle.hpp`
- `Pyramid/Graphics/Resources/ResourceManifest.hpp`
- `Pyramid/Graphics/Resources/ResourceRegistry.hpp`
- `Pyramid/Graphics/Scene/SceneSerializer.hpp`

### Render-state transitions

`OpenGLStateManager` caches the engine's supported program, vertex-array/generic buffer, texture, framebuffer, viewport, blend, depth, stencil, culling, scissor and clear state. Every tracked state operation refreshes an invalidated cache, including calls through a retained manager reference. External native state changes require `InvalidateState()` before further engine use. Statistics access does not query graphics state; mutation counts describe native state changes emitted by the manager, excluding queries and suppressed repeats.

Buffer lookups report the cached binding, or zero when unknown. Initial/invalidated snapshots seed array and current element bindings; other generic targets become known through explicit `BindBuffer()` calls. Element-buffer bindings follow the current VAO. Native vertex/index/instance buffer and VAO adapters use the same cache for binding and forget deleted names. `ForgetBuffer()` and `ForgetVertexArray()` invalidate references without changing unrelated state. Index-buffer `SetData()` uploads storage through `GL_COPY_WRITE_BUFFER`, leaving element attachment to `Bind()`/`SetIndexBuffer()` with a nonzero VAO. An element-buffer bind with VAO zero is rejected with a diagnostic.

`ResetToDefaults()` applies this baseline: program/VAO/array-buffer/default framebuffer zero; unit-zero 2D/cube textures unbound with active unit zero; blending off with ONE/ZERO factors and ADD equations; depth test/clamp off with LESS and writes enabled; stencil test off with ALWAYS/zero/all-bit comparison and KEEP operations on both faces; culling off with BACK/CCW/FILL; scissor disabled; clear color zero, depth one and stencil zero. It preserves viewport/scissor rectangles, textures on other units/targets and stored element attachments in other VAOs. Indexed UBO/SSBO state and unlisted OpenGL features are outside this reset contract.

UI establishes its physical surface and clip at draw time, then restores its documented UI baseline. A following material reapplies its own blend/depth/cull/polygon state. Deferred fullscreen lighting explicitly selects filled rasterization and disables blending before drawing. Fake-native-state and draw-snapshot tests verify these contracts; native-driver and visual checks remain separate deferred gates.

### Framebuffers

`Pyramid/Graphics/Framebuffer.hpp` defines the renderer-neutral `IFramebuffer` contract: `IsInitialized()` and `Bind()` are required virtual operations. `OpenGLFramebuffer` implements it with its existing resource ownership. Binding a ready target selects both its framebuffer and its target-sized viewport; it does not save previous state.

`IGraphicsDevice::BindFramebuffer(IFramebuffer*)` borrows the object for the call and does not retain it. An uninitialized or invalidated target is rejected with a diagnostic and `GetLastError()`, preserving the current framebuffer and viewport. A successful generic bind clears that error. Implementations must belong to the calling device's current graphics context; this interface does not validate arbitrary native implementations or provide cross-context synchronization.

```cpp
// framebuffer is an initialized OpenGLFramebuffer owned by the caller.
Pyramid::IFramebuffer& target = framebuffer;
device->BindFramebuffer(&target);       // Selects the target and its viewport.
// Submit off-screen graphics work.
device->BindFramebuffer(nullptr);       // Selects the default surface only.
device->SetViewport(0, 0, surfaceWidth, surfaceHeight);
```

Binding `nullptr` does not restore an earlier viewport. `RenderSystem` and the final UI pass explicitly establish the main surface and its viewport. `BindFramebufferHandle()` remains a native escape hatch and also leaves the viewport unchanged. The OpenGL state cache forgets deleted framebuffer names across read/draw bindings, preventing a reused name from suppressing a required native bind.

Creation, attachment specifications, resize, blits, and readback remain OpenGL-specific; there is no generic framebuffer factory. Resources must be released while their owning context is current. `OpenGLFramebuffer::SetDebugLabel()` uses optional KHR/core debug labeling only when the current context and loaded entry point support it; otherwise it emits a debug diagnostic and leaves the resource unchanged. Adding the virtual base changes `OpenGLFramebuffer`'s binary layout: rebuild applications and libraries together; no stable binary ABI is promised during pre-alpha. Fake-GL, linkage, and installed-package checks do not establish native-driver or visual correctness, which remains deferred.

### Deferred cascades

Deferred shadows use the existing separate depth textures with `sampler2D u_ShadowMaps[8]`, not layered texture-array storage. G-buffer slots are 0-4; cascades use slots 5-12. Literal-index shader branches follow the GLSL 3.30 sampler-index restriction; explicit level-zero sampling avoids implicit derivatives across cascade branches. Cascade selection uses view-space depth and returns unshadowed lighting outside the supplied range or when the count is zero.

`DeferredLightingPass::SetShadowData(maps, matrices, splits, bias)` accepts 1-8 initialized, non-multisampled depth textures, one finite matrix per map, exactly one more split than maps, strictly increasing finite splits, and a finite nonnegative bias. It returns `false`, logs a diagnostic and disables shadows for invalid data. Three empty vectors explicitly disable shadows. Readiness is checked again at execution because retained framebuffers can be invalidated. The legacy `SetShadowMaps(maps)` remains source-compatible but clears missing metadata and leaves shadows disabled; use `SetShadowData` to enable them.

`ShadowMapPass` exposes `GetCascadeSplits()`, `GetDepthBias()` and `HasValidShadowData()` alongside its existing maps/matrices. Readiness resets at each execution and becomes true only after every configured cascade is rendered for a shadow-casting primary light. Constructor counts outside 1-8 log and fall back to four; invalid setter counts/bias/splits are rejected. Custom split distances survive frame updates, and failed target recreation preserves the previous target set and configuration while disabling readiness until a successful render.

`SetupDeferredPipeline()` connects complete current-frame shadow data before each lighting pass and refreshes the current G-buffer after recreation/resize. Disabled, absent or unsuccessful shadow passes publish no shadows. Manually assembled pipelines retain explicit connection ownership. Deferred lighting supplies its camera/view uniforms immediately; deferred geometry uses its existing queued view-projection uniform rather than a mismatched camera block. Incomplete G-buffers, failed shader compilation and unavailable fullscreen resources skip the lighting draw. `End()` clears consumed texture units 0-12.

The textures retain their existing framebuffer ownership. This change adds no texture-array allocation/copy path. Real GLSL compilation, native rendering, cascade appearance and visual acceptance remain unverified while those checks are deferred.

### Shader programs

```cpp
Pyramid::ShaderProgramSpecification shaderSpec;
shaderSpec.vertexSource = vertexSource;
shaderSpec.fragmentSource = fragmentSource;
shaderSpec.name = "Player Forward";
shaderSpec.assetId =
    Pyramid::ShaderAssetId::FromString("shaders/player-forward");

Pyramid::ShaderCache shaderCache(*device);
auto shader = shaderCache.GetOrCreate(shaderSpec);
```

`ShaderProgram` is an immutable compiled resource that implements `IShader`, so it can be assigned directly to materials and command buffers. Exact stage-source sets receive a deterministic content identifier and compile only once even when requested through several caller-defined stable aliases. Graphics specifications require vertex and fragment stages; tessellation control/evaluation stages must be paired; compute programs cannot mix with graphics stages. Debug names are excluded from content identity. Renderer command buffers submit graphics commands only: the former log-only `CommandBuffer::Dispatch`, its command tag, and its payload have been removed. Calls to that method no longer compile; there is no queued compute replacement. Lower-level `IShader`/`ShaderProgram` compute APIs are unchanged and require separate capability checks and validation. Supported render-command numeric codes remain unchanged, including `ClearTarget = 15`.

Changing a stable asset uses transactional replacement:

```cpp
Pyramid::ShaderProgramSpecification replacement = shaderSpec;
replacement.fragmentSource = updatedFragmentSource;
replacement.assetId = {};

if (shaderCache.Recompile(shaderSpec.assetId, replacement))
    shader = shaderCache.Find(shaderSpec.assetId);
```

`Recompile()` compiles or resolves the replacement before changing the stable alias. Failure leaves the previous cached program active. Existing external owners of the old program remain valid and must reacquire the stable alias when they want the replacement. Content-derived identifiers are immutable and cannot be recompiled. `RemoveAlias()` removes only a caller-defined non-canonical alias and advances its generation without evicting shared compiled content. `Evict()`, `CollectUnused()`, `Clear()`, and `GetStats()` provide explicit lifetime and diagnostics controls. Destroy the cache before its graphics device/context and use it from the graphics thread.

### Geometry

Typical geometry setup:

```cpp
auto* device = GetGraphicsDevice();

Pyramid::MeshSpecification meshSpec;
meshSpec.vertexData = vertexData;
meshSpec.vertexDataSize = vertexBytes;
meshSpec.vertexCount = vertexCount;
meshSpec.layout = {
    {Pyramid::ShaderDataType::Float3, "a_Position"},
    {Pyramid::ShaderDataType::Float4, "a_Color"}
};
meshSpec.indexData = indexData;
meshSpec.indexCount = indexCount;
meshSpec.topology = Pyramid::PrimitiveTopology::Triangles;
meshSpec.name = "PlayerMesh";
meshSpec.assetId = Pyramid::MeshAssetId::FromString("meshes/player");

Pyramid::MeshCache meshCache(*device);
auto mesh = meshCache.GetOrCreate(meshSpec);
renderObject->mesh = mesh;
```

`Mesh` owns the created vertex array, vertex buffer, and optional index buffer. Its validated layout, vertex/index counts, primitive topology, identifiers, and local bounds are immutable after creation. Indexed and non-indexed meshes are supported for points, lines, line strips, triangles, and triangle strips. Creation rejects mismatched byte counts, missing/invalid position semantics, non-finite positions, incompatible topology counts, and out-of-range indices.

`MeshAssetId::FromString()` creates a deterministic 128-bit identifier from a stable caller-owned name such as an asset path. If `MeshSpecification::assetId` is left invalid, `Mesh::CalculateContentId()` derives the asset identifier from the exact geometry bytes and immutable draw metadata. Debug names are excluded, so renaming a mesh does not create another GPU upload.

`MeshCache` is bound to one graphics device and owns one strong reference per unique content fingerprint. Requests through different stable IDs share the same resident mesh when their geometry is identical. Reusing a resident explicit ID for different geometry fails rather than silently returning the wrong resource. `Find()` resolves either a stable alias or the content ID. `Evict()` removes the mesh and all aliases from the cache while existing external `shared_ptr` owners remain valid; `CollectUnused()` removes resources owned only by the cache, and `Clear()` removes all cache ownership. `GetStats()` reports residency, bytes, hits, misses, conflicts, failures, creations, and evictions. Destroy the cache before its graphics device/context and call it from the graphics thread. Direct `Mesh::Create()` remains available for intentionally uncached one-off geometry.

Model assets can be parsed without an engine or graphics dependency:

```cpp
#include <Pyramid/Model/ObjImporter.hpp>

auto imported = Pyramid::Model::ObjImporter::ImportFile("Assets/Models/tower.obj");
if (!imported.IsValid())
    return;
```

`ImportedModel` contains CPU-side materials, primitives, indexed vertices, bounds, and structured diagnostics. `ObjImportOptions` controls V-coordinate flipping, missing-normal generation, declared-library strictness, and byte/count/diagnostic limits. The parser accepts positive and negative OBJ indices, polygons, object/group/material splits, source or generated normals, quoted paths, and common MTL properties. Unsupported or malformed data is reported explicitly.

Publish imported meshes only when a tool intends to assign materials later:

```cpp
Pyramid::ModelMeshImportOptions options;
options.assetPrefix = "models/tower";

auto uploaded = Pyramid::ModelResourceImporter::UploadMeshes(
    *GetResourceRegistry(),
    imported,
    options);
```

For renderable resources, provide the shader-facing material profile explicitly:

```cpp
Pyramid::ModelResourceImportOptions options;
options.assetPrefix = "models/tower";
options.sourceDirectory = "Assets/Models";
options.materialProfile.shader = towerShader;
options.materialProfile.diffuseTexture.colorSpace =
    Pyramid::TextureColorSpace::SRGB;
options.materialProfile.missingTextureBehavior =
    Pyramid::ModelMissingTextureBehavior::Error;

auto resources = Pyramid::ModelResourceImporter::ImportModel(
    *GetResourceRegistry(),
    imported,
    options);

if (resources.IsSuccess())
{
    auto mesh = GetResourceRegistry()->Resolve(resources.renderables.front().mesh);
    auto material = GetResourceRegistry()->Resolve(
        resources.renderables.front().material);
}
```

`ImportModel()` maps MTL ambient, diffuse, specular, shininess, opacity, illumination-model, and diffuse `map_Kd` data into immutable texture/material resources. Uniform and sampler names, render state, texture color space/filtering/wrapping, fallback material, missing-texture behavior, and opacity-driven alpha blending are configurable. The operation reuses exact resident content, rejects stable-ID conflicts, and rolls back only aliases and resources created by the failed operation. Mesh-only and complete imports preserve source material-slot indices. Non-canonical cache aliases can be removed transactionally through `MeshCache::RemoveAlias()`, `TextureCache::RemoveAlias()`, and `MaterialCache::RemoveAlias()`; canonical content IDs cannot be removed by those methods.

### Textures

```cpp
Pyramid::TextureSpecification spec;
spec.Width = 512;
spec.Height = 512;
spec.Format = Pyramid::TextureFormat::RGBA8;
spec.GenerateMips = false;

auto texture = Pyramid::ITexture2D::Create(spec, pixels);
auto blank = Pyramid::ITexture2D::Create(512, 512);
auto target = Pyramid::ITexture2D::CreateRenderTarget(1280, 720);
auto depth = Pyramid::ITexture2D::CreateDepthTarget(
    1280, 720, Pyramid::TextureFormat::Depth24Stencil8);
auto white = Pyramid::ITexture2D::CreateFromColor(1, 1, Pyramid::Color::White);
```

For shared sampled textures, prefer the immutable cache path:

```cpp
Pyramid::TextureCache textureCache(*device);

Pyramid::TextureFileSpecification fileSpec;
fileSpec.filepath = "Assets/albedo.png";
fileSpec.colorSpace = Pyramid::TextureColorSpace::SRGB;
fileSpec.assetId = Pyramid::TextureAssetId::FromString("textures/player/albedo");

auto albedo = textureCache.GetOrCreate(fileSpec);
```

`TextureCache` fingerprints exact decoded pixels plus dimensions, format, mip policy, sampler state, border color, anisotropy request, and explicit linear/sRGB intent. Identical memory or file requests share one GPU upload across aliases. Reusing one stable ID for different resident content fails. `Reload()` is transactional for caller-defined file aliases: a replacement is decoded and uploaded before the alias changes; failure preserves the previous resource. `Evict()`, `CollectUnused()`, `Clear()`, and `GetStats()` provide explicit residency control. Cached `TextureResource` objects are immutable; reacquire the stable alias after a successful reload. Destroy the cache before the graphics device/context.

The direct `ITexture2D` path remains available for intentionally uncached or mutable textures. Direct and cached memory textures support all uncompressed `TextureFormat` values. The source buffer is tightly packed in native byte order, with no row padding:

| Formats | CPU representation per pixel | Bytes per pixel |
| --- | --- | --- |
| `RGB8`, `SRGB8`; `RGBA8`, `SRGBA8` | RGB or RGBA unsigned bytes | 3; 4 |
| `R8` | One unsigned byte; sampled as red without grayscale swizzling | 1 |
| `R16F`; `RGB16F`; `RGBA16F` | 1, 3, or 4 IEEE binary16 bit patterns in `u16` values | 2; 6; 8 |
| `R32F`; `RGB32F`; `RGBA32F` | 1, 3, or 4 `f32` values | 4; 12; 16 |
| `Depth16`; `Depth24`; `Depth32F` | Normalized `u16`; normalized `u32`; `f32` | 2; 4; 4 |
| `Depth24Stencil8` | Packed `u32`: depth in the high 24 bits, stencil in the low 8 | 4 |
| `Depth32FStencil8` | `f32` depth followed by `u32` whose low 8 bits hold stencil; high 24 bits ignored | 8 |

In particular, `16F` inputs are half-float bits, not `f32` buffers, and `Depth24` requires four transfer bytes, not three. The raw-pointer constructor cannot verify buffer length: callers must supply `Width * Height` complete pixels. `SetData()` requires exactly that byte count; cached memory creation additionally checks its explicit `pixelDataSize`, addressability, and a 512 MiB base-level source limit before copying or uploading. The same limit applies to the cache copy of decoded files; individual file decoders have separate validation limits. Content identity covers the exact input bytes, without floating-point canonicalization. Existing numeric format codes and valid RGB/RGBA content identities are preserved. Residency statistics are estimates, using four bytes per `Depth24` pixel; driver allocation can differ.

For direct textures, `SRGB=true` or an explicit `SRGB8`/`SRGBA8` format selects sRGB storage. Other formats reject the sRGB flag. Cached resources preserve their existing contract: `TextureResourceSpecification::colorSpace` is authoritative, normalizing the embedded `texture.SRGB` flag and sRGB enum aliases; request `TextureColorSpace::SRGB` explicitly. Cached sRGB is accepted only for RGB/RGBA 8-bit formats. `BC1_RGB`, `BC1_RGBA`, `BC3_RGBA`, and `BC7_RGBA` retain their numeric codes as reserved, unsupported formats. They, `None`, and unknown enum values fail with diagnostics before graphics allocation.

`ITexture2D::Create()` failures produce an unloaded object with `GetLastError()`. `CreateDepthTarget()` accepts only depth/depth-stencil formats and defaults to one mip level, nearest filtering, and clamp-to-edge; non-depth formats return `nullptr`. General specifications may generate mipmaps for any supported format. Requested mip storage is also created for null initial data, whose pixels remain undefined until written. Depth/stencil sampling exposes depth in the OpenGL 3.3 baseline; stencil sampling is not implemented, and generated mip values do not promise a min/max depth hierarchy or stencil preservation. `CreateRenderTarget()` allocates texture storage; framebuffer attachment compatibility and completeness remain separate checks.

File decoding still produces RGB8/RGBA8 pixels from JPEG/PNG/TGA/BMP; the new memory formats do not add HDR/depth file codecs. `IsLoaded()` and `GetLastError()` expose state, uploads restore unpack alignment, and mipmapped filters are mapped completely. Cached file specifications can flip decoded rows before upload; direct `TextureSpecification::FlipY` remains unsupported.


Mutable `OpenGLTexture2D` objects implement `SetFilter`, `SetWrap`, `SetBorderColor`, and `SetMaxAnisotropy`. Both creation and mutation reject unknown filter/wrap enum values, non-finite border components, and anisotropy below 1 or non-finite values before native calls. Finite border components outside `[0,1]` remain valid. Known compatibility aliases stay supported: filter `None` means linear, wrap `None` means repeat, and mipmapped magnification enums select their nearest/linear base filter. Direct textures retain the requested minification filter: without mip levels, sampling uses its nearest/linear base fallback; successful `GenerateMipmaps()` activates the requested mip filter. Cache normalization of no-mip specifications remains unchanged to preserve existing content IDs.

Anisotropy is independent of mip generation and applies through the same adapter for direct and cached creation. OpenGL 4.6 core, `ARB_texture_filter_anisotropic`, or `EXT_texture_filter_anisotropic` enables a current-context maximum query and clamping of the requested value. Unsupported contexts use 1x, log a warning for requests above 1, and do not query or write unsupported anisotropy parameters. Requests remain part of cache identity even when different requests clamp to the same native value. Limits are queried during creation/mutation rather than retained across contexts; the owning context and matching GLAD metadata must be current.

Sampler mutations retain the active texture unit and end with its 2D target unbound, using the shared binding cache. Validation failures leave native state untouched. After native errors, the adapter attempts to restore the old sampler and preserves the original error in `GetLastError()`; restoration failure is reported explicitly and means native sampler state is uncertain. Mipmap allocation itself cannot be undone by restoring sampler parameters. Successful operations clear the previous error.

Cached `TextureResource` objects reject mutations. `GetTexture()` now returns `const std::shared_ptr<const ITexture2D>&`, so callers must use the immutable wrapper for device binding or const native operations; code that mutated the unwrapped texture must migrate to a deliberately mutable texture. Native renderer IDs remain an advanced escape for trusted graphics integration and must not be used to mutate cached assets. Game and debug UI bind the immutable wrapper through the existing device adapter.

### Materials

```cpp
Pyramid::MaterialSpecification materialSpec;
materialSpec.shader = shader;
materialSpec.textures = {
    {"u_AlbedoMap", 0, albedo}
};
materialSpec.uniforms = {
    {"u_AlbedoColor", Pyramid::Math::Vec4(1.0f)},
    {"u_Metallic", 0.0f},
    {"u_Roughness", 0.5f}
};
materialSpec.renderState.blendMode = Pyramid::MaterialBlendMode::Opaque;
materialSpec.renderState.depthTest = true;
materialSpec.renderState.cullMode = Pyramid::MaterialCullMode::Back;
materialSpec.assetId =
    Pyramid::MaterialAssetId::FromString("materials/player");

Pyramid::MaterialCache materialCache;
auto material = materialCache.GetOrCreate(materialSpec);
renderObject->material = material;
```

`Material` is immutable and owns exact references to one graphics `ShaderProgram` and zero or more `TextureResource` objects. Its content identity includes shader and texture content IDs, sampler uniform names and slots, typed uniforms, and blend/depth/cull/polygon state while excluding the debug name. Creation rejects compute shaders, duplicate slots or names, unloaded textures, and non-finite values. `CommandBuffer::SetMaterial()` applies the material in draw order; dynamic object/camera matrices are recorded separately with typed `SetUniform*()` commands so they do not alter material identity.

`MaterialCache` keeps one strong resident reference per exact material content fingerprint. Different stable IDs share one `Material` when shader, textures, uniforms, and fixed state are identical. Reusing one stable ID for different resident content is rejected. `Find()`, `Evict()`, `CollectUnused()`, `Clear()`, and `GetStats()` provide explicit lookup, lifetime, and diagnostics control. Destroy the material cache before the shader and texture caches that own its referenced resources.

Stable aliases can be replaced transactionally:

```cpp
Pyramid::MaterialSpecification replacement = materialSpec;
replacement.uniforms = {
    {"u_AlbedoColor", Pyramid::Math::Vec4(0.8f, 0.9f, 1.0f, 1.0f)},
    {"u_Metallic", 0.2f},
    {"u_Roughness", 0.35f}
};
replacement.assetId = {};

if (materialCache.Replace(materialSpec.assetId, replacement))
    material = materialCache.Find(materialSpec.assetId);
```

`Replace()` validates or resolves the complete replacement before changing the stable alias. Failure preserves the previously active material, while existing external owners of older material versions remain valid. Content-derived identifiers are immutable and cannot be replaced.


### Resource registry

`Game` owns one `ResourceRegistry` for its graphics device and destroys it before device shutdown:

```cpp
auto* resources = GetResourceRegistry();
if (!resources)
    return;

auto mesh = resources->Meshes().GetOrCreate(meshSpec);
auto shader = resources->Shaders().GetOrCreate(shaderSpec);
auto texture = resources->Textures().GetOrCreate(textureSpec);
auto material = resources->Materials().GetOrCreate(materialSpec);
```

The registry is the preferred application-level entry point for reusable graphics assets. It guarantees dependency-safe teardown and maintenance ordering: materials first, then textures, shaders, and meshes. `CollectUnused()` removes cache-only resources while preserving dependencies referenced by externally owned materials. `Clear()` removes every cached alias/resource without invalidating external `shared_ptr` owners. Those external owners must still be released before the graphics device/context. `GetStats()` aggregates all four cache snapshots and reports estimated total resident bytes.

For scene, serialization, and long-lived asset references, acquire typed non-owning handles instead of retaining cache identifiers or resource owners:

```cpp
auto meshHandle = resources->AcquireMesh(meshSpec);
auto materialHandle = resources->AcquireMaterial(materialSpec);

if (auto mesh = resources->Resolve(meshHandle))
{
    // The alias still exists and its generation matches the handle.
}

if (!resources->IsAlive(materialHandle))
{
    // The resource was evicted, collected, cleared, or replaced.
}
```

`MeshHandle`, `ShaderHandle`, `TextureHandle`, and `MaterialHandle` contain a stable asset identifier and a non-zero alias generation. They are small serializable value types and do not keep GPU resources alive. Alias creation, remapping, eviction, collection, and clearing advance persistent generation tombstones; therefore a stale handle never resolves to a later resource that reused the same stable ID. `FromParts()` reconstructs a handle from serialized ID/generation data.

Mutations return a new current-generation handle and leave the previous handle stale when content changes:

```cpp
shaderHandle = resources->RecompileShader(shaderHandle, replacementShader);
textureHandle = resources->ReloadTexture(textureHandle);
materialHandle = resources->ReplaceMaterial(materialHandle, replacementMaterial);
```

Handle-backed scene objects avoid long-lived resource ownership while retaining mesh bounds for culling:

```cpp
auto object = std::make_shared<Pyramid::RenderObject>();
object->SetMeshHandle(meshHandle, *resources);
object->SetMaterialHandle(materialHandle, *resources);
```

Register the render system through `Game::SetRenderSystem()` so render passes receive the same registry and resolve handles at submission time. A stale mesh or material handle causes that object to be skipped rather than bound to unrelated replacement content. Assigning a newly issued mesh handle refreshes the cached local bounds.

Standalone cache construction remains available for tooling and focused tests, but a registry must always be destroyed before its graphics device and native context.

### Resource manifests

`ResourceManifest` serializes a named set of typed handles without a JSON dependency. The current format is deterministic and versioned:

```text
PYRAMID_RESOURCE_MANIFEST	1
mesh	player.mesh	0123456789abcdef0123456789abcdef	4
material	player.material	fedcba9876543210fedcba9876543210	2
```

```cpp
Pyramid::ResourceManifest manifest;
manifest.Add("player.mesh", meshHandle);
manifest.Add("player.material", materialHandle);

const std::string serialized = manifest.Serialize();

Pyramid::ResourceManifest restored;
std::vector<Pyramid::ResourceManifestDiagnostic> diagnostics;
if (!Pyramid::ResourceManifest::Deserialize(serialized, restored, diagnostics))
{
    // Unsupported versions and malformed entries leave `restored` unchanged.
}

auto restoredMesh = restored.GetMeshHandle("player.mesh");
const auto report = restored.Restore(*resources);
```

Manifest keys are caller-owned stable reference names containing letters, digits, `.`, `_`, `-`, or `/`. The serialized 128-bit asset ID is restored exactly; it is not re-hashed from a path. `Restore()` validates the current registry and reports `MissingAsset` separately from `StaleGeneration`. It never updates a serialized generation automatically.

### Scene

### Entities and components

```cpp
Pyramid::Scene scene("Battlefield");

auto army = scene.CreateEntity("Army");
auto unit = scene.CreateEntity("Infantry");
unit.SetParent(army);
unit.SetLocalPosition({2.0f, 0.0f, 4.0f});

Pyramid::MeshRendererComponent renderer;
renderer.mesh = meshHandle;
renderer.material = materialHandle;
unit.SetMeshRenderer(renderer, &registry);

auto sun = scene.CreateEntity("Sun");
Pyramid::LightComponent light;
light.type = Pyramid::LightType::Directional;
light.intensity = 4.0f;
sun.SetLight(light);
scene.SetPrimaryLight(sun);
```

Every entity has a stable `EntityId`, name, visibility flag, and mandatory `TransformComponent`. Optional `MeshRendererComponent` and `LightComponent` values provide rendering state. `Entity` is a lightweight non-owning facade; it becomes permanently invalid after its entity is destroyed, its scene is cleared, or its scene is destroyed. Reusing an ID does not revive old facades, and a replacement facade compares unequal to the old one. Copies of a facade retain the same lifetime identity. A bare `EntityId` identifies the current entity at lookup time; it does not carry this lifetime check.

```cpp
const Pyramid::EntityId id = unit.GetId();
auto restored = scene.FindEntity(id);
auto children = army.GetChildren();
auto world = unit.GetWorldMatrix();
```

Hierarchy edits reject missing entities, cycles, and self-parenting. Reparenting preserves local TRS. World matrices and inherited visibility are recalculated from the authoritative hierarchy.

`SetParent()` rejects stale or foreign non-empty facade arguments without changing the current parent; pass `Entity{}` or use `ClearParent()` to detach explicitly. `Scene::SetPrimaryLight()` likewise ignores stale or foreign non-empty facades; pass `Entity{}` to clear the primary light. `Scene::DestroyEntity(Entity)` rejects expired facades even when their IDs have been reused.

### Renderer proxies

`Scene::GetRenderObjects()` and `Scene::GetLights()` return generated proxies used by render passes, culling, and the octree. Do not author scene state by mutating those proxies. New code should edit entities/components and request the proxies again. Transitional `AddRenderObject()` and `AddLight()` helpers exist only for low-level compatibility.

### Spatial management

`SceneManager` synchronizes entity-generated render proxies into the octree. Point, sphere, box, ray, nearest, and K-nearest queries operate on complete world AABBs. `QueryResult::entities` identifies the owning scene entities. `GetVisibleObjects()` respects inherited visibility and the supported frustum-culling setting, through the octree or linear path. The unused `SetOcclusionCullingEnabled(bool)` method, private flag and always-false predicate have been removed; calls to the old method no longer compile. There is no replacement occlusion algorithm or setting. Rebuild consumers because the pre-alpha class layout changed. Scene statistics and persistent-format versions are unchanged; the removed flag was never serialized.

## Math

Umbrella header: `Pyramid/Math/Math.hpp`

```cpp
using namespace Pyramid::Math;

Vec3 position(1.0f, 2.0f, 3.0f);
Mat4 model = Mat4::CreateTranslation(position)
           * Mat4::CreateRotationY(Radians(30.0f));
```

Concrete headers define exact conventions and available operations for vectors, matrices, quaternions, geometry, interpolation, and SIMD helpers. These headers and implementations belong to the independently installable `Pyramid::Math` target. `Mat4::Determinant()` and `Mat4::Inverse()` use pivoted elimination and return zero/identity respectively for singular matrices.

## Images

Header: `Pyramid/Util/Image.hpp`

```cpp
auto image = Pyramid::Util::Image::Load("assets/texture.png");
if (image.Data)
{
    // Consume image.Width, image.Height, image.Channels, and image.Data.
    Pyramid::Util::Image::Free(image.Data);
}
```

`Pyramid::Image` is the standalone CMake target that implements these headers; `Pyramid::Engine` links it publicly for compatibility. PNG uses Pyramid's custom non-interlaced decoder. JPEG uses Pyramid's owned 8-bit Huffman decoder for baseline and progressive streams, grayscale/YCbCr RGB normalization, common chroma subsampling, and restart markers. Arithmetic-coded, lossless, hierarchical, 12-bit, and four-component JPEG variants fail explicitly. `ImageData::Data` remains manually owned and must be released with `Image::Free()`.

## Logging

Header: `Pyramid/Util/Log.hpp`

The logger, primitive aliases, colors, and assertions are supplied by the standalone `Pyramid::Foundation` target.

```cpp
Pyramid::Util::LoggerConfig config;
config.consoleLevel = Pyramid::Util::LogLevel::Info;
config.fileLevel = Pyramid::Util::LogLevel::Debug;
config.logFilePath = "pyramid_game.log";
Pyramid::Util::Logger::GetInstance().Configure(config);

PYRAMID_LOG_INFO("Loaded ", objectCount, " objects");
PYRAMID_ASSERT(resource != nullptr, "Resource must exist");
```
