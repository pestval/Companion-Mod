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

    // Track mission state changes for logging
    bool wasMissionActive = false;

    // Simple frame counter for periodic logging
    int frameCount = 0;

    // --- MAIN LOOP (runs every frame) ---
    while (true)
    {
        // ------------------------------------------------
        // MISSION DETECTION
        // ------------------------------------------------
        // This maps directly to your C# code:
        //     Function.Call<bool>(Hash.GET_MISSION_FLAG)
        //
        // In ASI, we call the native directly through
        // EngineAdapter (which wraps the raw native call).
        // ------------------------------------------------
        bool isMissionActive = EngineAdapter::IsMissionActive();

        if (isMissionActive && !wasMissionActive)
        {
            Logger::Log("Mission STARTED — systems would suspend here.");
        }
        else if (!isMissionActive && wasMissionActive)
        {
            Logger::Log("Mission ENDED — systems would resume here.");
        }
        wasMissionActive = isMissionActive;

        // ------------------------------------------------
        // ON-SCREEN DEBUG TEXT
        // ------------------------------------------------
        // Draw a small status line so you KNOW the mod is
        // running without needing to check the log file.
        // This uses GTA's native text drawing system.
        // ------------------------------------------------
        EngineAdapter::DrawDebugText("CompanionMod v0.1 — Skeleton Active", 0.01f, 0.01f);

        if (isMissionActive)
        {
            EngineAdapter::DrawDebugText("MISSION ACTIVE", 0.01f, 0.04f);
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
