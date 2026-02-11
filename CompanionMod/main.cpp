// ============================================================
//  main.cpp — ASI Entry Point
// ============================================================
//
//  PURPOSE:
//  This is the equivalent of your old C# constructor + OnTick.
//  In SHVDN (.NET), the engine called YOUR script via events.
//  In ASI (native C++), YOU own the loop. GTA becomes a service
//  you call into — not the other way around.
//
//  KEY CONCEPTS:
//  - DllMain()       : Windows calls this when your .asi is loaded/unloaded.
//                      We do almost nothing here — just store our module handle.
//
//  - ScriptMain()    : ScriptHookV calls this ONCE after the game is ready.
//                      We run our own infinite loop inside it.
//                      WAIT(0) yields back to the game each frame.
//
//  - The while(true) loop IS your OnTick. Every iteration = one game frame.
//
//  DIFFERENCE FROM YOUR C# MOD:
//  In TheKarenMod.cs, you had:
//      Tick += OnTick;
//  And the engine called OnTick for you.
//
//  Here, YOU call Update() yourself inside the loop.
//  This gives you full control over execution order.
// ============================================================

#include <windows.h>
#include "main.h"

#include "Logger.h"
#include "EngineAdapter.h"

#include "CompanionCore.h"

static float DistSq(const Vec3& a, const Vec3& b)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

static CompanionCore g_core;
static CompanionState g_state;
static uint32_t g_tickCount = 0;

// Follow / Teleport scheduling (shared across tick blocks)
static uint32_t g_lastFollowTick = 0;
static uint32_t g_lastTeleportTick = 0;

// Stay
static bool g_stayToggle = false;     // local input state
static bool g_isStayingActive = false; // tracks whether we already applied stay actions
static uint32_t g_lastStaySnapTick = 0;

// Tuning constants
static constexpr uint32_t FOLLOW_REFRESH_TICKS = 60;   // ~1s @60fps
static constexpr float    TELEPORT_DIST_METERS = 50.0f;
static constexpr float    TELEPORT_DIST_SQ = TELEPORT_DIST_METERS * TELEPORT_DIST_METERS;
static constexpr uint32_t TELEPORT_COOLDOWN_TICKS = 300;  // ~5s @60fps
static constexpr uint32_t STAY_SNAP_TICKS = 60; // ~1s @60fps; re-snap to anchor while staying

// Mission gate state
static bool g_wasMissionActive = false;
static bool g_companionWasSpawnedBeforeMission = false;
static bool g_stayToggleBeforeMission = false;

// Vehicle Riding V1
static bool g_isRiding = false;
static int  g_ridingVehicleHandle = 0;
static bool g_wasPlayerInVehicle = false;

// Store our DLL module handle (needed later for file paths, etc.)
HMODULE g_ModuleHandle = NULL;

// ============================================================
//  ScriptMain — Called once by ScriptHookV when game is ready
// ============================================================
//
//  This is where your mod lives. The while(true) loop below
//  runs once per game frame. WAIT(0) is critical — it yields
//  control back to GTA so the game can render, process input,
//  run physics, etc. Without it, the game would freeze.
//
void ScriptMain()
{
    // --- INITIALIZATION (runs once) ---
    Logger::Init("CompanionMod.log");
    Logger::Log("=== CompanionMod ASI Loaded ===");
    Logger::Log("Phase 1 — Skeleton active. No gameplay systems yet.");

    // Simple frame counter for periodic logging
    int frameCount = 0;

    // --- MAIN LOOP (runs every frame) ---
    while (true)
    {
        g_tickCount++;

        // ------------------------------------------------
        // MISSION GATE (V1)
        // ------------------------------------------------
        bool isMissionActive = EngineAdapter::IsMissionActive();

        // Mission started edge
        if (isMissionActive && !g_wasMissionActive)
        {
            g_companionWasSpawnedBeforeMission = g_state.spawned;
            g_stayToggleBeforeMission = g_stayToggle;

            Logger::Log("[MissionGate] Mission START — suspending companion. spawnedBefore=%d stayBefore=%d",
                (int)g_companionWasSpawnedBeforeMission,
                (int)g_stayToggleBeforeMission);

            // Despawn for maximum stability (recommended)
            if (g_state.spawned)
            {
                EngineAdapter::DespawnTestPed();
                g_state.spawned = false;
            }

            // Clear vehicle state
            g_isRiding = false;
            g_ridingVehicleHandle = 0;
            g_wasPlayerInVehicle = false;

            // Reset timers so we resume cleanly
            g_lastFollowTick = 0;
            g_lastTeleportTick = 0;

            // Clear local stay runtime flags
            g_isStayingActive = false;
        }

        // Mission ended edge
        if (!isMissionActive && g_wasMissionActive)
        {
            Logger::Log("[MissionGate] Mission END — resuming companion. respawn=%d stayRestore=%d",
                (int)g_companionWasSpawnedBeforeMission,
                (int)g_stayToggleBeforeMission);

            // Restore stay toggle (whether you want it ON or OFF after missions)
            g_stayToggle = g_stayToggleBeforeMission;

            // Respawn only if it existed before mission
            if (g_companionWasSpawnedBeforeMission)
            {
                if (EngineAdapter::SpawnTestPed())
                {
                    g_state.spawned = true;
                    Logger::Log("[MissionGate] Respawn OK");
                }
                else
                {
                    Logger::Log("[MissionGate] Respawn FAILED");
                }
            }

            // Clear vehicle state
            g_isRiding = false;
            g_ridingVehicleHandle = 0;
            g_wasPlayerInVehicle = false;

            // Reset timers so follow re-issues immediately
            g_lastFollowTick = 0;
            g_lastTeleportTick = 0;

            // Clear mission-memory
            g_companionWasSpawnedBeforeMission = false;
        }

        g_wasMissionActive = isMissionActive;

        // F6 toggles Stay on/off
        if (EngineAdapter::IsKeyJustPressed(VK_F6))
        {
            g_stayToggle = !g_stayToggle;
            Logger::Log("[Main] Stay toggled: %s", g_stayToggle ? "ON" : "OFF");

            // If turning stay OFF, we want follow to re-issue immediately
            if (!g_stayToggle)
                g_lastFollowTick = 0;
        }

        CompanionContext ctx{};
        ctx.tickCount = g_tickCount;
        ctx.deltaSeconds = 1.0f / 60.0f; // ok for now

        ctx.playerExists = EngineAdapter::PlayerExists();
        ctx.playerDead = EngineAdapter::IsPlayerDead();
        ctx.playerInVehicle = EngineAdapter::IsPlayerInVehicle();
        ctx.playerPos = EngineAdapter::GetPlayerPosition();

        // Keep runtime state honest (prevents desync if ped disappears)
        g_state.spawned = EngineAdapter::DoesTestPedExist();

        // Feed input state into the Core-owned state
        g_state.stayEnabled = g_stayToggle;

        CompanionCommands cmd{};
        g_core.Tick(ctx, g_state, cmd);

        // ------------------------------------------------
// VEHICLE RIDING V1 (simple + stable)
// ------------------------------------------------
        {
            bool playerInVehicle = ctx.playerInVehicle;

            // Safety: if Stay was requested while riding, release first.
            if (cmd.requestStay && g_isRiding && g_state.spawned)
            {
                EngineAdapter::TeleportTestPedNearPlayer(1.2f, 0.8f, 0.0f);
                g_isRiding = false;
                g_ridingVehicleHandle = 0;
                g_lastFollowTick = 0;
                Logger::Log("[VehicleRide] Stay requested while riding -> released companion before Stay");
            }

            // Detect the edge: player just exited their vehicle
            if (g_wasPlayerInVehicle && !playerInVehicle)
            {
                if (g_isRiding && g_state.spawned)
                {
                    EngineAdapter::TeleportTestPedNearPlayer(1.2f, 0.8f, 0.0f);
                    g_lastFollowTick = 0;
                    Logger::Log("[VehicleRide] Player EXIT vehicle -> teleport companion + resume follow");
                }

                g_isRiding = false;
                g_ridingVehicleHandle = 0;
            }

            // While player is in vehicle, try to ride (unless Stay)
            if (playerInVehicle && g_state.spawned && !cmd.requestStay)
            {
                int veh = EngineAdapter::GetPlayerVehicleHandle();

                // Vehicle changed while riding (or we never latched one)
                if (veh != 0 && (!g_isRiding || veh != g_ridingVehicleHandle))
                {
                    int chosenSeat = -999;

                    if (EngineAdapter::IsVehicleSeatFree(veh, 0))
                        chosenSeat = 0; // front passenger
                    else if (EngineAdapter::IsVehicleSeatFree(veh, 1))
                        chosenSeat = 1; // rear left
                    else if (EngineAdapter::IsVehicleSeatFree(veh, 2))
                        chosenSeat = 2; // rear right

                    if (chosenSeat != -999)
                    {
                        if (EngineAdapter::PutTestPedIntoVehicle(veh, chosenSeat))
                        {
                            g_isRiding = true;
                            g_ridingVehicleHandle = veh;

                            // While riding, don't spam follow tasks.
                            g_lastFollowTick = g_tickCount;

                            Logger::Log("[VehicleRide] Warped companion into vehicle=%d seat=%d", veh, chosenSeat);
                        }
                    }
                    else
                    {
                        // No seat free; let follow logic handle on-foot behavior.
                        g_isRiding = false;
                        g_ridingVehicleHandle = 0;
                    }
                }
            }

            g_wasPlayerInVehicle = playerInVehicle;
        }

        // Keep state mirror up to date (useful for future Core logic)
        g_state.ridingVehicle = g_isRiding;

        // ---------------------------
        // STAY EXECUTION (V2 - Anchor)
        // ---------------------------
        if (cmd.requestStay && g_state.spawned)
        {
            // Enter stay once
            if (!g_isStayingActive)
            {
                // Capture anchor at the moment Stay begins
                g_state.stayAnchor = EngineAdapter::GetTestPedPosition();
                g_state.hasStayAnchor = true;
                g_lastStaySnapTick = g_tickCount;

                EngineAdapter::ClearTestPedTasks();
                EngineAdapter::FreezeTestPed(true);

                g_isStayingActive = true;

                // Ensure follow restarts cleanly when we exit stay
                g_lastFollowTick = 0;

                Logger::Log("[Main] Stay ACTIVE anchor=(%.2f,%.2f,%.2f)",
                    g_state.stayAnchor.x, g_state.stayAnchor.y, g_state.stayAnchor.z);
            }

            // Optional: re-snap to anchor occasionally to counter tiny nudges/physics drift
            if (g_state.hasStayAnchor && (g_tickCount - g_lastStaySnapTick) >= STAY_SNAP_TICKS)
            {
                Vec3 cur = EngineAdapter::GetTestPedPosition();

                // only correct if drift is noticeable (10cm)
                const float DRIFT_SQ = 0.10f * 0.10f;

                if (DistSq(cur, g_state.stayAnchor) > DRIFT_SQ)
                {
                    EngineAdapter::SetTestPedPosition(g_state.stayAnchor);
                }

                g_lastStaySnapTick = g_tickCount;
            }
        }
        else
        {
            // Exit stay once
            if (g_isStayingActive)
            {
                EngineAdapter::FreezeTestPed(false);

                g_isStayingActive = false;
                g_state.hasStayAnchor = false;
                g_lastStaySnapTick = 0;

                // Force follow to re-issue immediately after leaving stay
                g_lastFollowTick = 0;

                Logger::Log("[Main] Stay OFF");
            }
        }

        // ---------------------------
        // FOLLOW EXECUTION (command-driven)
        // ---------------------------
        if (!cmd.requestStay && cmd.requestFollow && g_state.spawned && !g_isRiding && !ctx.playerInVehicle)
        {
            uint32_t refresh = (cmd.followRefreshTicks > 0) ? cmd.followRefreshTicks : FOLLOW_REFRESH_TICKS;
            bool timeRefresh = (g_tickCount - g_lastFollowTick) > refresh;

            if (timeRefresh)
            {
                EngineAdapter::TaskFollowPlayer(cmd.followDistance, cmd.followSpeed);
                g_lastFollowTick = g_tickCount;
            }
        }
        else
        {
            g_lastFollowTick = 0;
        }

        if (cmd.requestLog)
        {
            Logger::Log("[Core] tick=%u exists=%d dead=%d inVeh=%d pos=(%.2f,%.2f,%.2f)",
                ctx.tickCount,
                (int)ctx.playerExists,
                (int)ctx.playerDead,
                (int)ctx.playerInVehicle,
                ctx.playerPos.x, ctx.playerPos.y, ctx.playerPos.z
            );
        }

        // ---------------------------
        // AUTO-TELEPORT IF TOO FAR
        // ---------------------------
        if (!cmd.requestStay && g_state.spawned && !ctx.playerInVehicle)
        {
            Vec3 playerPos = EngineAdapter::GetPlayerPosition();
            Vec3 pedPos = EngineAdapter::GetTestPedPosition();

            bool tooFar = (DistSq(playerPos, pedPos) > TELEPORT_DIST_SQ);
            bool canTeleport = (g_tickCount - g_lastTeleportTick) >= TELEPORT_COOLDOWN_TICKS;

            if (tooFar && canTeleport)
            {
                EngineAdapter::TeleportTestPedNearPlayer(1.2f, 0.8f, 0.0f);
                g_lastTeleportTick = g_tickCount;

                // This is the whole point of "Option 2":
                // force follow to re-issue immediately next tick.
                g_lastFollowTick = 0;

                Logger::Log("[Main] Auto-teleport: too far (>%.1fm).", TELEPORT_DIST_METERS);
            }
        }
        else
        {
            g_lastTeleportTick = 0;
        }

        // F7 toggles spawn/despawn
        if (EngineAdapter::IsKeyJustPressed(VK_F7))
        {
            if (!g_state.spawned)
            {
                if (EngineAdapter::SpawnTestPed())
                {
                    Logger::Log("[Main] F7 spawn OK");
                    g_state.spawned = true;
                }
                else
                {
                    Logger::Log("[Main] F7 spawn FAILED");
                }
            }
            else
            {
                EngineAdapter::DespawnTestPed();
                Logger::Log("[Main] F7 despawn OK");
                g_state.spawned = false;
            }
        }

        if (cmd.requestSpawn && !g_state.spawned)
        {
            if (EngineAdapter::SpawnTestPed())
            {
                Logger::Log("[Core] SpawnTestPed OK");
                g_state.spawned = true;
            }
            else
            {
                Logger::Log("[Core] SpawnTestPed FAILED");
            }
        }

        if (cmd.requestDespawn && g_state.spawned)
        {
            EngineAdapter::DespawnTestPed();
            Logger::Log("[Core] DespawnTestPed OK");
            g_state.spawned = false;
        }

        // ------------------------------------------------
        // ON-SCREEN DEBUG TEXT
        // ------------------------------------------------
        // Draw a small status line so you KNOW the mod is
        // running without needing to check the log file.
        // This uses GTA's native text drawing system.
        // ------------------------------------------------
        EngineAdapter::DrawDebugText("CompanionMod v0.1 — Skeleton Active", 0.01f, 0.01f);

        // ------------------------------------------------
        // MANUAL RECALL / TELEPORT (F5)
        // - If in Stay: switch to Follow automatically
        // ------------------------------------------------
        if (!isMissionActive && EngineAdapter::IsKeyJustPressed(VK_F5))
        {
            if (!g_state.spawned)
            {
                Logger::Log("[Recall] Ignored: companion not spawned.");
            }
            else
            {
                // If staying, force exit Stay -> Follow
                if (g_stayToggle || g_isStayingActive)
                {
                    g_stayToggle = false;          // input toggle off (Core will emit follow)
                    g_state.stayEnabled = false;   // safety: ensure Core sees it immediately this tick

                    if (g_isStayingActive)
                    {
                        EngineAdapter::FreezeTestPed(false);
                        g_isStayingActive = false;
                    }

                    // Optional: clear anchor since we're leaving stay
                    g_state.hasStayAnchor = false;

                    Logger::Log("[Recall] Exiting Stay -> Follow");
                }

                // Teleport near player
                EngineAdapter::TeleportTestPedNearPlayer(1.2f, 0.8f, 0.0f);

                // Force follow to re-issue immediately
                g_lastFollowTick = 0;

                // Prevent auto-teleport from immediately re-triggering cooldown logic
                g_lastTeleportTick = g_tickCount;

                Logger::Log("[Recall] Teleported companion to player.");
            }
        }

        // ------------------------------------------------
        // PERIODIC HEARTBEAT LOG
        // ------------------------------------------------
        // Log every ~600 frames (~10 seconds at 60fps)
        // so you can confirm the tick loop is stable
        // without flooding the log file.
        // ------------------------------------------------
        frameCount++;
        if (frameCount % 600 == 0)
        {
            Logger::Log("Heartbeat — frame %d", frameCount);
        }

        // ------------------------------------------------
        // YIELD TO GAME ENGINE
        // ------------------------------------------------
        // WAIT(0) is the most important line in any ASI mod.
        // It pauses YOUR code and lets GTA run for one frame.
        // When GTA is done with that frame, it comes back here.
        //
        // Think of it like this:
        //   Your loop runs → WAIT(0) → GTA renders a frame →
        //   Your loop runs → WAIT(0) → GTA renders a frame → ...
        //
        // If you forget WAIT(0), the game freezes forever.
        // ------------------------------------------------
        WAIT(0);
    }
}

// ============================================================
//  DllMain — Windows entry point (called on load/unload)
// ============================================================
//
//  This is NOT where your mod logic goes. This is a Windows
//  requirement for any DLL. We use it for two things:
//
//  1. DLL_PROCESS_ATTACH: Our DLL was just loaded into GTA's
//     process. We store our module handle and tell ScriptHookV
//     to call ScriptMain() when the game is ready.
//
//  2. DLL_PROCESS_DETACH: Our DLL is being unloaded (game
//     closing). We clean up our logger.
//
//  scriptRegister() is a ScriptHookV function that says:
//  "Hey SHV, here's my script entry point. Call it when ready."
//
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        g_ModuleHandle = hModule;
        scriptRegister(hModule, ScriptMain);
        break;

    case DLL_PROCESS_DETACH:
        Logger::Shutdown();
        scriptUnregister(hModule);
        break;
    }
    return TRUE;
}
