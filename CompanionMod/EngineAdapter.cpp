// ============================================================
//  EngineAdapter.cpp — GTA Native Call Wrapper (Implementation)
// ============================================================
//
//  WHAT CHANGED FROM THE FIRST VERSION:
//  The SDK's natives.h organizes functions into namespaces like
//  MISC::, HUD::, PED::, etc. But the exact namespace names
//  vary between SDK versions. Some use GAMEPLAY:: instead of
//  MISC::, UI:: instead of HUD::, and so on.
//
//  To avoid this fragility, we now use nativeCaller.h directly.
//  This gives us the invoke<>() function, which calls ANY native
//  by its hash — a unique 64-bit ID that never changes.
//
//  HOW NATIVE HASHES WORK:
//  Every GTA V native function has a permanent hash identifier.
//  For example:
//      GET_MISSION_FLAG  →  0xA33CDCCDA663159E
//      SET_TEXT_FONT     →  0x66E0276CC5F6B9DA
//
//  You can look these up at: https://nativedb.dotindustries.dev
//
//  invoke<ReturnType>(hash, arg1, arg2, ...)  calls the native.
//  This is what ALL the namespace wrappers do under the hood
//  anyway — we're just being explicit about it.
//
//  WHY THIS IS ACTUALLY BETTER:
//  Using raw hashes makes your code SDK-version-independent.
//  It's also how most experienced ASI modders write their code.
//  The namespace wrappers are convenient but fragile.
//
//  COMPARING TO YOUR C# MOD:
//  In C#, you already used hashes! Look at your old code:
//      Function.Call(Hash.SET_PED_COMBAT_MOVEMENT, _karen, 2);
//
//  Hash.SET_PED_COMBAT_MOVEMENT was a hash enum. Now you're
//  doing the exact same thing, just in C++:
//      invoke<Void>(0x4D9CA1009AFBD057, ped, 2);
//
//  Same concept, different syntax.
// ============================================================

#include "types.h"
#include "nativeCaller.h"
#include "EngineAdapter.h"
#include "Logger.h"
#include <Windows.h>

static Ped g_testPed = 0;

// ============================================================
//  NATIVE HASH REFERENCE
// ============================================================
//  We define these as constants at the top of the file so they
//  are easy to find, update, and cross-reference with nativedb.
//
//  Naming convention: HASH_<CATEGORY>_<FUNCTION_NAME>
//  This makes them self-documenting and searchable.
// ============================================================

// Game state
static const UINT64 HASH_GET_MISSION_FLAG                         = 0xA33CDCCDA663159E;

// Text drawing
static const UINT64 HASH_SET_TEXT_FONT                            = 0x66E0276CC5F6B9DA;
static const UINT64 HASH_SET_TEXT_SCALE                           = 0x07C837F9A01C34C9;
static const UINT64 HASH_SET_TEXT_COLOUR                          = 0xBE6B23FFA53FB442;
static const UINT64 HASH_BEGIN_TEXT_COMMAND_DISPLAY_TEXT          = 0x25FBB336DF1804CB;
static const UINT64 HASH_ADD_TEXT_COMPONENT_SUBSTRING_PLAYER_NAME = 0x6C188BE134E074AA;
static const UINT64 HASH_END_TEXT_COMMAND_DISPLAY_TEXT            = 0xCD015E5BB0D96A57;

// Player / entity
static const UINT64 HASH_PLAYER_PED_ID                            = 0xD80958FC74E988A6;
static const UINT64 HASH_DOES_ENTITY_EXIST                        = 0x7239B21A38F536BA;
static const UINT64 HASH_IS_PED_DEAD_OR_DYING                     = 0x3317DEDB88C95038;
static const UINT64 HASH_IS_PED_IN_ANY_VEHICLE                    = 0x997ABD671D25CA0B;
static const UINT64 HASH_GET_ENTITY_COORDS                        = 0x3FEF770D40960D5A;

static const UINT64 HASH_REQUEST_MODEL                            = 0x963D27A58DF860AC;
static const UINT64 HASH_HAS_MODEL_LOADED                         = 0x98A4EB5D89A0C952;
static const UINT64 HASH_SET_MODEL_AS_NO_LONGER_NEEDED            = 0xE532F5D78798DAAB;

static const UINT64 HASH_CREATE_PED                               = 0xD49F9B0955C367DE;
static const UINT64 HASH_SET_ENTITY_AS_MISSION_ENTITY             = 0xAD738C3085FE7E11;
static const UINT64 HASH_DELETE_ENTITY                            = 0x961AC54BF0613F5D;
static const UINT64 HASH_DELETE_PED                               = 0x9614299DCB53E54B;

static const UINT64 HASH_SET_BLOCKING_OF_NON_TEMPORARY_EVENTS     = 0x9F8AA94D6D97DBF4;
static const UINT64 HASH_SET_PED_FLEE_ATTRIBUTES                  = 0x70A2D1137C8ED7C9;
static const UINT64 HASH_SET_PED_COMBAT_ATTRIBUTES                = 0x9F7794730795E019;
static const UINT64 HASH_TASK_GO_TO_ENTITY                        = 0x6A071245EB0D1882;
static const UINT64 HASH_FREEZE_ENTITY_POSITION                   = 0x428CA6DBD1094446;
static const UINT64 HASH_SET_ENTITY_DYNAMIC                       = 0x1718DE8E3F2823CA;
static const UINT64 HASH_CLEAR_PED_TASKS                          = 0xE1EF3C1216AFF2CD;
static const UINT64 HASH_TASK_FOLLOW_TO_OFFSET_OF_ENTITY          = 0x304AE42E357B8C7E;

static const UINT64 HASH_SET_ENTITY_COORDS_NO_OFFSET              = 0x239A3351AC1DA385;
static const UINT64 HASH_SET_ENTITY_VELOCITY                      = 0x1C99BB7B6E96D16F;
static const UINT64 HASH_CLEAR_PED_TASKS_IMMEDIATELY              = 0xAAA34F8A7CB32098;

// Vehicle
static const UINT64 HASH_GET_VEHICLE_PED_IS_IN                    = 0x9A9112A0FE9A4713;
static const UINT64 HASH_IS_VEHICLE_SEAT_FREE                     = 0x22AC59A870E6A669;
static const UINT64 HASH_SET_PED_INTO_VEHICLE                     = 0xF75B0D629E1C063D;

namespace EngineAdapter
{
    // --------------------------------------------------------
    //  IsMissionActive
    // --------------------------------------------------------
    //  invoke<BOOL>  means "call this native and expect a
    //  BOOL return value." BOOL in GTA's native system is
    //  actually an int (0 or 1), not a C++ bool. That's why
    //  we compare != 0 to convert it properly.
    // --------------------------------------------------------
    bool IsMissionActive()
    {
        return invoke<BOOL>(HASH_GET_MISSION_FLAG) != 0;
    }

    // --------------------------------------------------------
    //  DrawDebugText
    // --------------------------------------------------------
    //  Same 6-step sequence as before, just using invoke<>()
    //  with hashes instead of namespace::function() calls.
    //
    //  invoke<Void> means "call this native, no return value."
    //
    //  Notice how the string "STRING" is passed to
    //  BEGIN_TEXT_COMMAND — this tells GTA's text system
    //  "I'm going to feed you a raw string next." It's a
    //  format specifier, like printf's %s.
    // --------------------------------------------------------
    void DrawDebugText(const char* text, float x, float y)
    {
        invoke<Void>(HASH_SET_TEXT_FONT, 0);
        invoke<Void>(HASH_SET_TEXT_SCALE, 0.0f, 0.35f);
        invoke<Void>(HASH_SET_TEXT_COLOUR, 255, 255, 255, 255);
        invoke<Void>(HASH_BEGIN_TEXT_COMMAND_DISPLAY_TEXT, "STRING");
        invoke<Void>(HASH_ADD_TEXT_COMPONENT_SUBSTRING_PLAYER_NAME, text);
        invoke<Void>(HASH_END_TEXT_COMMAND_DISPLAY_TEXT, x, y, 0);
    }

    bool PlayerExists()
    {
        Ped p = invoke<Ped>(HASH_PLAYER_PED_ID);
        if (p == 0) return false;
        return invoke<BOOL>(HASH_DOES_ENTITY_EXIST, p) != 0;
    }

    bool IsPlayerDead()
    {
        Ped p = invoke<Ped>(HASH_PLAYER_PED_ID);
        if (p == 0) return true;
        return invoke<BOOL>(HASH_IS_PED_DEAD_OR_DYING, p, TRUE) != 0;
    }

    bool IsPlayerInVehicle()
    {
        Ped p = invoke<Ped>(HASH_PLAYER_PED_ID);
        if (p == 0) return false;
        return invoke<BOOL>(HASH_IS_PED_IN_ANY_VEHICLE, p, FALSE) != 0;
    }

    Vec3 GetPlayerPosition()
    {
        Vec3 out{};
        Ped p = invoke<Ped>(HASH_PLAYER_PED_ID);
        if (p == 0) return out;

        // native returns a Vector3 from types.h
        Vector3 v = invoke<Vector3>(HASH_GET_ENTITY_COORDS, p, TRUE);
        out.x = v.x;
        out.y = v.y;
        out.z = v.z;
        return out;
    }

    bool SpawnTestPed()
    {
        // Already spawned?
        if (g_testPed != 0 && invoke<BOOL>(HASH_DOES_ENTITY_EXIST, g_testPed))
            return true;

        // Example model: a common ambient ped
        // "a_m_m_business_01"
        const Hash model = 0x7E6A64B7;

        invoke<void>(HASH_REQUEST_MODEL, model);

        // Wait up to ~2 seconds (120 frames)
        for (int i = 0; i < 120; ++i)
        {
            if (invoke<BOOL>(HASH_HAS_MODEL_LOADED, model))
                break;
            WAIT(0);
        }

        if (!invoke<BOOL>(HASH_HAS_MODEL_LOADED, model))
        {
            invoke<void>(HASH_SET_MODEL_AS_NO_LONGER_NEEDED, model);
            return false;
        }

        Vec3 p = GetPlayerPosition();

        float x = p.x + 1.2f;
        float y = p.y + 0.8f;
        float z = p.z;

        // pedType: 4 = CIVMALE, usually safe for ambient peds
        const int pedType = 4;

        g_testPed = invoke<Ped>(HASH_CREATE_PED, pedType, model, x, y, z, 0.0f, TRUE, TRUE);

        if (g_testPed == 0 || !invoke<BOOL>(HASH_DOES_ENTITY_EXIST, g_testPed))
            return false;

        invoke<void>(HASH_FREEZE_ENTITY_POSITION, g_testPed, FALSE);
        invoke<void>(HASH_SET_ENTITY_DYNAMIC, g_testPed, TRUE);

        // Mark as ours so we can delete cleanly
        invoke<void>(HASH_SET_ENTITY_AS_MISSION_ENTITY, g_testPed, TRUE, TRUE);

        // Make it "dumb" so it doesn't flee / do random ambient behavior
        invoke<void>(HASH_SET_BLOCKING_OF_NON_TEMPORARY_EVENTS, g_testPed, TRUE);
        invoke<void>(HASH_SET_PED_FLEE_ATTRIBUTES, g_testPed, 0, FALSE);
        // Disable a couple combat attributes so it doesn’t pick fights
        invoke<void>(HASH_SET_PED_COMBAT_ATTRIBUTES, g_testPed, 46, FALSE); // BF_CanFightArmedPeds (common)
        invoke<void>(HASH_SET_PED_COMBAT_ATTRIBUTES, g_testPed, 17, FALSE); // BF_AlwaysFight (common)

        invoke<void>(HASH_SET_MODEL_AS_NO_LONGER_NEEDED, model);
        return true;
    }

    void DespawnTestPed()
    {
        if (g_testPed == 0)
            return;

        // Log before
        BOOL existsBefore = invoke<BOOL>(HASH_DOES_ENTITY_EXIST, g_testPed);
        Logger::Log("[Adapter] DespawnTestPed handle=%d existsBefore=%d", (int)g_testPed, (int)existsBefore);

        if (existsBefore)
        {
            // Make sure we "own" it
            invoke<void>(HASH_SET_ENTITY_AS_MISSION_ENTITY, g_testPed, TRUE, TRUE);

            // Delete as PED (more reliable than DELETE_ENTITY)
            Ped p = g_testPed;
            invoke<void>(HASH_DELETE_PED, &p);
        }

        // Log after (note: g_testPed is our stored handle, so this checks whether it’s still alive)
        BOOL existsAfter = invoke<BOOL>(HASH_DOES_ENTITY_EXIST, g_testPed);
        Logger::Log("[Adapter] DespawnTestPed existsAfter=%d", (int)existsAfter);

        g_testPed = 0;
    }

    void TaskFollowPlayer(float followDist, float speed)
    {
        if (g_testPed == 0) return;
        if (!invoke<BOOL>(HASH_DOES_ENTITY_EXIST, g_testPed)) return;

        Ped player = invoke<Ped>(HASH_PLAYER_PED_ID);
        if (player == 0) return;

        // Make sure ped is not stuck/frozen
        invoke<void>(HASH_FREEZE_ENTITY_POSITION, g_testPed, FALSE);

        // Follow slightly behind/right for now
        float offX = 0.5f;
        float offY = -followDist;
        float offZ = 0.0f;

        // speed=2.0, timeout=-1, stoppingRange=followDist, persistFollowing=true
        invoke<void>(
            HASH_TASK_FOLLOW_TO_OFFSET_OF_ENTITY,
            g_testPed,
            player,
            offX, offY, offZ,
            speed,    // speed
            -1,      // timeout
            followDist, // stopping range
            TRUE     // persist
        );
    }

    bool IsKeyJustPressed(int vk)
    {
        static SHORT prev[256] = {};
        SHORT cur = GetAsyncKeyState(vk);

        bool downNow = (cur & 0x8000) != 0;
        bool wasDown = (prev[vk] & 0x8000) != 0;

        prev[vk] = cur;
        return downNow && !wasDown;
    }

    bool DoesTestPedExist()
    {
        return (g_testPed != 0) && invoke<BOOL>(HASH_DOES_ENTITY_EXIST, g_testPed);
    }

    Vec3 GetTestPedPosition()
    {
        Vec3 out{};
        if (!DoesTestPedExist()) return out;

        Vector3 v = invoke<Vector3>(HASH_GET_ENTITY_COORDS, g_testPed, TRUE);
        out.x = v.x;
        out.y = v.y;
        out.z = v.z;
        return out;
    }

    void SetTestPedPosition(const Vec3& pos)
    {
        if (!DoesTestPedExist()) return;

        // Teleport without physics offsets
        invoke<void>(HASH_SET_ENTITY_COORDS_NO_OFFSET, g_testPed, pos.x, pos.y, pos.z, TRUE, TRUE, TRUE);

        // Kill velocity so it doesn’t slide or pop
        invoke<void>(HASH_SET_ENTITY_VELOCITY, g_testPed, 0.0f, 0.0f, 0.0f);
    }

    void TeleportTestPedNearPlayer(float offsetX, float offsetY, float offsetZ)
    {
        if (!DoesTestPedExist()) return;

        Ped player = invoke<Ped>(HASH_PLAYER_PED_ID);
        if (player == 0) return;

        Vec3 p = GetPlayerPosition();

        float x = p.x + offsetX;
        float y = p.y + offsetY;
        float z = p.z + offsetZ;

        // Stop whatever it was doing (prevents weird “rubberband” tasks)
        invoke<void>(HASH_CLEAR_PED_TASKS_IMMEDIATELY, g_testPed);

        // Teleport without physics offsets
        invoke<void>(HASH_SET_ENTITY_COORDS_NO_OFFSET, g_testPed, x, y, z, TRUE, TRUE, TRUE);

        // Kill velocity so it doesn’t slide or pop
        invoke<void>(HASH_SET_ENTITY_VELOCITY, g_testPed, 0.0f, 0.0f, 0.0f);

        // Make sure it’s not frozen
        invoke<void>(HASH_FREEZE_ENTITY_POSITION, g_testPed, FALSE);
    }

    void ClearTestPedTasks()
    {
        if (!DoesTestPedExist()) return;
        invoke<void>(HASH_CLEAR_PED_TASKS_IMMEDIATELY, g_testPed);
    }

    void FreezeTestPed(bool freeze)
    {
        if (!DoesTestPedExist()) return;
        invoke<void>(HASH_FREEZE_ENTITY_POSITION, g_testPed, freeze ? TRUE : FALSE);
    }

    // ============================================================
    // VEHICLE RIDING (V1)
    // ============================================================

    int GetPlayerVehicleHandle()
    {
        Ped player = invoke<Ped>(HASH_PLAYER_PED_ID);
        if (player == 0) return 0;

        // p1=false: current vehicle only (not last vehicle)
        Vehicle v = invoke<Vehicle>(HASH_GET_VEHICLE_PED_IS_IN, player, FALSE);
        return (int)v;
    }

    bool IsVehicleSeatFree(int vehicleHandle, int seatIndex)
    {
        if (vehicleHandle == 0) return false;
        Vehicle v = (Vehicle)vehicleHandle;
        return invoke<BOOL>(HASH_IS_VEHICLE_SEAT_FREE, v, seatIndex) != 0;
    }

    bool PutTestPedIntoVehicle(int vehicleHandle, int seatIndex)
    {
        if (!DoesTestPedExist()) return false;
        if (vehicleHandle == 0) return false;

        Vehicle v = (Vehicle)vehicleHandle;

        // Clear tasks first to avoid the ped fighting the warp.
        invoke<void>(HASH_CLEAR_PED_TASKS_IMMEDIATELY, g_testPed);

        // Ensure not frozen (Stay mode freezes position).
        invoke<void>(HASH_FREEZE_ENTITY_POSITION, g_testPed, FALSE);

        // Warp instantly into the seat.
        invoke<void>(HASH_SET_PED_INTO_VEHICLE, g_testPed, v, seatIndex);

        // Basic sanity: ped should still exist after.
        return invoke<BOOL>(HASH_DOES_ENTITY_EXIST, g_testPed) != 0;
    }

    // ============================================================
    // VEHICLE RIDING (V2)
    // ============================================================
    int GetTestPedVehicleHandle()
    {
        if (!DoesTestPedExist()) return 0;
        Vehicle v = invoke<Vehicle>(HASH_GET_VEHICLE_PED_IS_IN, g_testPed, FALSE);
        return (int)v;
    }
}
