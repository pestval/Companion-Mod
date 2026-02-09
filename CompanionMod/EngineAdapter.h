// ============================================================
//  EngineAdapter.h — GTA Native Call Wrapper (Interface)
// ============================================================
//
//  PURPOSE:
//  This is the single most important architectural decision
//  in your entire project. Every GTA native call goes through
//  here — your gameplay code NEVER calls natives directly.
//
//  WHY THIS MATTERS:
//  In your C# Karen mod, native calls were scattered everywhere:
//      Function.Call(Hash.SET_PED_COMBAT_MOVEMENT, _karen, 2);
//      Function.Call(Hash.CLEAR_PED_TASKS_IMMEDIATELY, _karen);
//      Function.Call(Hash.SET_ENTITY_VELOCITY, ...);
//
//  That worked fine for a single-file mod. But in a multi-system
//  architecture, scattering raw engine calls creates problems:
//
//  1. If a native's behavior changes, you hunt through every file
//  2. You can't add safety checks (null entity? dead ped?) centrally
//  3. You can't log what the engine is doing without modifying
//     every call site
//  4. Testing becomes impossible
//
//  EngineAdapter solves all of these. It's a thin layer, but it
//  gives you ONE place to:
//    - Add null checks and safety
//    - Add logging/debugging
//    - Handle edge cases
//    - Document what each native actually does
//
//  PORTFOLIO NOTE:
//  This pattern is called the "Adapter Pattern" — it's a real
//  software engineering concept used in AAA studios. Wrapping
//  engine calls behind a clean interface shows you understand
//  separation of concerns.
//
//  RIGHT NOW:
//  We only have a few functions (mission check, text drawing).
//  As we migrate systems from Karen, this will grow to include
//  ped spawning, task management, vehicle queries, etc.
// ============================================================

#pragma once  // Ensures this file is only included once per compilation

namespace EngineAdapter
{
    // --- GAME STATE QUERIES ---

    // Checks if a mission is currently active.
    // Wraps: GET_MISSION_FLAG (native 0xA33CDCCDA663159E)
    //
    // In your C# mod this was:
    //     Function.Call<bool>(Hash.GET_MISSION_FLAG)
    //
    // Used in: main tick loop (mission suppression gate)
    bool IsMissionActive();

    // --- DEBUG DRAWING ---

    // Draws text on screen at the given position.
    // x, y are screen-space coordinates (0.0 to 1.0)
    //   0,0 = top-left    1,1 = bottom-right
    //
    // This wraps several natives that must be called in sequence:
    //   SET_TEXT_FONT, SET_TEXT_SCALE, SET_TEXT_COLOUR,
    //   BEGIN_TEXT_COMMAND_DISPLAY_TEXT, ADD_TEXT_COMPONENT_SUBSTRING_PLAYER_NAME,
    //   END_TEXT_COMMAND_DISPLAY_TEXT
    //
    // GTA's text system is verbose — wrapping it here means
    // your gameplay code just calls one function.
    void DrawDebugText(const char* text, float x, float y);
}
