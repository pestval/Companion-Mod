#pragma once
#include <cstdint>

// Engine-agnostic types only (NO GTA natives/types in this file)

enum class CompanionMode : uint8_t
{
    Protection,
    Stay,
    Frenzy
};

struct Vec3
{
    float x{}, y{}, z{};
};

struct CompanionContext
{
    // Time
    uint32_t tickCount = 0;
    float deltaSeconds = 0.0f;

    // Player snapshot
    bool playerExists = false;
    bool playerDead = false;
    bool playerInVehicle = false;
    Vec3 playerPos{};
};

struct CompanionState
{
    CompanionMode mode = CompanionMode::Protection;

    bool spawned = false;
    bool stayEnabled = false;

    // Stay V2 anchor
    bool hasStayAnchor = false;
    Vec3 stayAnchor{};

    bool ridingVehicle = false;
};

struct CompanionCommands
{
    bool requestLog = false;
    bool requestSpawn = false;
    bool requestDespawn = false;

    // Follow intent (Core decides what it wants)
    bool requestFollow = false;
    float followDistance = 2.0f;   // stopping range / follow distance
    float followSpeed = 3.0f;      // movement speed passed to native
    uint32_t followRefreshTicks = 60; // how often to re-issue (60 = ~1s @60fps)

    bool requestStay = false;
};

class CompanionCore
{
public:
    void Tick(const CompanionContext& ctx, CompanionState& state, CompanionCommands& out)
    {
        // Clear commands each tick
        out = {};

        // Heartbeat log every ~2 seconds at 60fps
        if (ctx.tickCount % 120 == 0)
            out.requestLog = true;

        bool requestToggleSpawn = false;

        if (state.spawned && ctx.playerExists && !ctx.playerDead)
        {
            if (state.stayEnabled)
            {
                out.requestStay = true;
                out.requestFollow = false;
            }
            else
            {
                out.requestStay = false;
                out.requestFollow = true;
                out.followDistance = 2.0f;
                out.followSpeed = 3.0f;
                out.followRefreshTicks = 60;
            }
        }

        (void)ctx;
    }
};
