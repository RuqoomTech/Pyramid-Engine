#include <Pyramid/Examples/RTSReference/RTSMatch.hpp>
#include <Pyramid/Examples/RTSReference/RTSUnitStore.hpp>
#include <Pyramid/Graphics/Scene.hpp>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <thread>

namespace
{
    using namespace Pyramid;
    using namespace Pyramid::Examples::RTSReference;

    void Require(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "RTS match test failed: " << message << '\n';
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
                std::string(context) + " exceeded loopback deadline");
            std::this_thread::yield();
        }
    }

    MeshRendererComponent Renderer()
    {
        MeshRendererComponent renderer;
        renderer.boundsMode = RenderBoundsMode::Manual;
        renderer.localBoundsMin = Math::Vec3(-0.5f);
        renderer.localBoundsMax = Math::Vec3(0.5f);
        return renderer;
    }

    Protocol::ContentId Content()
    {
        Protocol::ContentId result{};
        result[0] = 0x31;
        result[15] = 0xA7;
        return result;
    }

    Entity Owned(Scene& scene, const RTSUnitStore& units, u32 owner)
    {
        for (Entity entity : scene.GetEntities())
        {
            const auto* state = units.FindUnit(entity);
            if (state && state->owner == owner)
            {
                return entity;
            }
        }
        return {};
    }

    void TestPortAndOfflinePolicy()
    {
        std::string error;
        u16 port = 91;
        Require(ParseRTSPort("7777", port, error) && port == 7777 && error.empty() &&
                ParseRTSPort("65535", port, error) && port == 65535 &&
                ParseRTSPort("00001", port, error) && port == 1,
            "valid complete decimal ports failed");
        for (const char* invalid : {"", "0", "65536", "-1", "+1", " 1", "1 ", "1x", "000001", "1\n", u8"١٢"})
        {
            port = 91;
            Require(!ParseRTSPort(invalid, port, error) && port == 91 && !error.empty(),
                "invalid port changed output or escaped validation");
        }
        Scene scene("Offline Match");
        const Entity floor = scene.CreateEntity("Persistent Floor");
        const Entity unrelated = scene.CreateEntity("Other Game State");
        RTSUnitStore units;
        RTSUnitState outside;
        outside.owner = 9;
        Require(units.AddUnit(unrelated, outside), "unrelated state fixture failed");
        RTSSimulation simulation(units);
        RTSMatch match(scene, units, simulation);
        Require(match.SetCatalog(1, Renderer(), error) && match.StartOffline(error),
            "offline start: " + error);
        Entity unit = match.GetPrimaryEntity();
        Require(unit && match.IsReady() && match.GetMode() == RTSMatchMode::Offline &&
                match.GetLocalOwner() == 1 && units.FindUnit(unit)->owner == 1 &&
                scene.GetEntities().size() == 3,
            "offline match did not create exactly its own reference unit");
        Require(match.Update(1.0, false, error) && simulation.GetTickCount() == 0,
            "first active offline frame must discard the resume boundary interval");
        Require(match.QueueCommand({unit.GetId(), Math::Vec3(5.0f, 0.0f, 0.0f), unit}, error) &&
                match.Update(RTSSimulation::FixedStepSeconds, false, error) &&
                std::fabs(unit.GetWorldPosition().x - 0.125f) < 0.0001f,
            "offline command did not execute on fixed simulation ticks");
        const auto tick = simulation.GetTickCount();
        const auto position = unit.GetWorldPosition();
        Require(match.QueueCommand({unit.GetId(), Math::Vec3(99.0f, 0.0f, 0.0f), unit}, error) &&
                match.Update(100.0, true, error) && simulation.GetTickCount() == tick &&
                simulation.GetPendingCommandCount() == 0 && unit.GetWorldPosition().x == position.x,
            "offline pause must discard pending intent/time without moving units");
        Require(match.Update(1.0, false, error) && simulation.GetTickCount() == tick &&
                match.Update(RTSSimulation::FixedStepSeconds, false, error) &&
                units.FindUnit(unit)->movementTarget->x == 5.0f,
            "offline resume must not replay paused commands or catch up paused elapsed");
        const auto unchangedTick = simulation.GetTickCount();
        for (f64 invalid : {-1.0, std::numeric_limits<f64>::infinity(), std::numeric_limits<f64>::quiet_NaN()})
        {
            Require(!match.Update(invalid, false, error) && !error.empty() &&
                    match.GetMode() == RTSMatchMode::Offline && match.GetPrimaryEntity() == unit &&
                    simulation.GetTickCount() == unchangedTick,
                "invalid time must fail without resetting a live match");
        }
        Entity child = scene.CreateEntity("Foreign Child");
        Require(child.SetParent(unit) && !match.Stop(error) && child && unit,
            "match reset must not recursively delete foreign descendants");
        Require(child.ClearParent() && match.Stop(error) && !unit && floor && child && unrelated &&
                units.FindUnit(unrelated)->owner == 9,
            "match stop must preserve scene props and unrelated gameplay state");
        Require(match.StartOffline(error) && match.GetPrimaryEntity() &&
                match.GetPrimaryEntity() != unit && !unit,
            "starting another offline match must create a fresh unit lifetime");
    }

    void TestHostAndPassiveClient()
    {
        Scene hostScene("Authoritative Match");
        Scene clientScene("Client Match");
        const Entity hostProp = hostScene.CreateEntity("Host Terrain Prop");
        const Entity clientProp = clientScene.CreateEntity("Client Terrain Prop");
        RTSUnitStore hostUnits;
        RTSUnitStore clientUnits;
        RTSSimulation hostSimulation(hostUnits);
        RTSSimulation clientSimulation(clientUnits);
        RTSMatch host(hostScene, hostUnits, hostSimulation);
        RTSMatch client(clientScene, clientUnits, clientSimulation);
        std::string error;
        Require(host.SetCatalog(1, Renderer(), error) && client.SetCatalog(1, Renderer(), error) &&
                client.StartOffline(error), "match catalog fixtures failed");
        const Entity oldOffline = client.GetPrimaryEntity();
        Require(host.Host({"127.0.0.1", 0}, 10, Content(), error) && host.GetHostPort() != 0 &&
                !host.IsReady() && host.GetLocalOwner() == 1 &&
                client.Join({"127.0.0.1", host.GetHostPort()}, Content(), error),
            "headless host/join setup: " + error);
        Require(!oldOffline && !client.GetPrimaryEntity(), "join left a local authoritative demo unit behind");
        Until([&]()
        {
            Require(host.Update(0.0, true, error), "host handshake: " + error);
            Require(client.Update(0.0, true, error), "client handshake/application: " + error);
            return host.IsReady() && client.IsReady();
        }, "match handshake and initial replica");
        Entity hostPlayer = host.GetPrimaryEntity();
        Entity hostRemote = Owned(hostScene, hostUnits, 2);
        Entity local = Owned(clientScene, clientUnits, 2);
        Entity enemy = Owned(clientScene, clientUnits, 1);
        Require(hostPlayer && hostRemote && local && enemy && client.GetLocalOwner() == 2 &&
                clientScene.GetEntities().size() == 3 && clientSimulation.GetTickCount() == 0 &&
                std::fabs(local.GetWorldPosition().x - 2.0f) < 0.0001f,
            "client did not receive two authoritative owned units without local simulation");
        Require(!client.QueueCommand({enemy.GetId(), Math::Vec3::Zero, enemy}, error) && !error.empty(),
            "client must reject commands for another player's selection before networking");
        Require(host.QueueCommand({hostPlayer.GetId(), Math::Vec3(-8.0f, 0.0f, 0.0f), hostPlayer}, error) &&
                client.QueueCommand({local.GetId(), Math::Vec3(8.0f, 0.0f, 0.0f), local}, error),
            "both owners' move commands were not queued: " + error);
        Until([&]()
        {
            Require(host.Update(0.0, true, error) && client.Update(0.0, true, error),
                "match command pump: " + error);
            return hostSimulation.GetPendingCommandCount() == 2;
        }, "client selection-to-wire-to-host mapping");
        for (unsigned int tick = 0; tick < 4; ++tick)
        {
            Require(host.Update(RTSSimulation::FixedStepSeconds, true, error),
                "host must tick despite a local pause/menu: " + error);
        }
        Until([&]()
        {
            Require(host.Update(0.0, true, error) && client.Update(0.0, true, error),
                "client scene snapshot pump: " + error);
            return client.GetTick() == host.GetTick();
        }, "authoritative transform projection");
        Require(hostSimulation.GetTickCount() == 4 && clientSimulation.GetTickCount() == 0 &&
                Owned(clientScene, clientUnits, 2) == local &&
                std::fabs(local.GetWorldPosition().x - hostRemote.GetWorldPosition().x) < 0.0001f &&
                local.GetWorldPosition().x > 2.0f && hostPlayer.GetWorldPosition().x < -2.0f &&
                clientUnits.FindUnit(local)->movementTarget,
            "host pause policy or stable client membership/authoritative movement is incorrect");
        const f32 receivedPosition = local.GetWorldPosition().x;
        Require(client.Update(0.25, true, error) && client.Update(0.25, false, error) &&
                clientSimulation.GetTickCount() == 0 && local.GetWorldPosition().x == receivedPosition,
            "client must remain passive with an active target until a new host snapshot arrives");
        Require(client.Stop(error) && !local && !enemy && clientProp &&
                clientScene.GetEntities().size() == 1,
            "client disconnect must clear managed replicas and preserve scene props");
        Until([&]()
        {
            (void)host.Update(0.0, true, error);
            return host.GetMode() == RTSMatchMode::None;
        }, "host disconnect cleanup");
        Require(!hostPlayer && !hostRemote && hostProp && hostScene.GetEntities().size() == 1 &&
                hostSimulation.GetPendingCommandCount() == 0,
            "host disconnect must reset only managed match state");
        Require(!host.Host({"127.0.0.1", 0}, 10, Content(), error) &&
                host.Host({"127.0.0.1", 0}, 11, Content(), error),
            "host retry must require a fresh session epoch");
        Require(client.Join({"127.0.0.1", host.GetHostPort()}, Content(), error),
            "permanent-publication fixture rejoin failed: " + error);
        Until([&]()
        {
            Require(host.Update(0.0, true, error) && client.Update(0.0, true, error),
                "publication-failure handshake: " + error);
            return host.IsReady() && client.IsReady();
        }, "publication-failure ready state");
        Entity malformedHostUnit = host.GetPrimaryEntity();
        Require(malformedHostUnit.SetLocalPosition(Math::Vec3(Protocol::MaximumCoordinate + 1.0f, 0.0f, 0.0f)),
            "out-of-protocol native state fixture failed");
        Require(!host.Update(RTSSimulation::FixedStepSeconds * 4.0, true, error) &&
                !error.empty() && host.GetMode() == RTSMatchMode::None && !malformedHostUnit &&
                hostProp && hostScene.GetEntities().size() == 1,
            "permanent snapshot capture failure must end the match rather than retry as congestion");
        Until([&]()
        {
            (void)client.Update(0.0, true, error);
            return client.GetMode() == RTSMatchMode::None;
        }, "client termination after permanent host publication failure");
        Require(clientProp && clientScene.GetEntities().size() == 1,
            "permanent host failure must clear client replicas without deleting props");
    }
}

int main()
{
    TestPortAndOfflinePolicy();
    TestHostAndPassiveClient();
    std::cout << "RTS match tests passed\n";
    return EXIT_SUCCESS;
}
