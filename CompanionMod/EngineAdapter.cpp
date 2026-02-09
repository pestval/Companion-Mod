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
static const UINT64 HASH_BEGIN_TEXT_COMMAND_DISPLAY_TEXT           = 0x25FBB336DF1804CB;
static const UINT64 HASH_ADD_TEXT_COMPONENT_SUBSTRING_PLAYER_NAME = 0x6C188BE134E074AA;
static const UINT64 HASH_END_TEXT_COMMAND_DISPLAY_TEXT             = 0xCD015E5BB0D96A57;

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
}
