// ============================================================
//  Logger.h — File Logging System (Interface)
// ============================================================
//
//  PURPOSE:
//  When your mod is running inside GTA V, you can't use
//  Console.WriteLine or set breakpoints easily. Your primary
//  debugging tool is a log file.
//
//  This logger writes timestamped messages to a .log file
//  in your GTA V directory. Every important event gets logged:
//  startup, shutdown, mission changes, errors, state transitions.
//
//  WHY NOT JUST USE printf?
//  GTA V is a graphical application — there's no console window.
//  printf output goes nowhere. We need to write to a file.
//
//  WHY A DEDICATED LOGGER (instead of raw file writes)?
//  1. Timestamps on every line (you'll thank yourself later)
//  2. Automatic flush (so logs survive crashes)
//  3. One Init/Shutdown — no file handle management in game code
//  4. Format string support (like printf: "frame %d", count)
//
//  USAGE:
//      Logger::Init("CompanionMod.log");
//      Logger::Log("Mod loaded successfully");
//      Logger::Log("Karen spawned at position %f, %f, %f", x, y, z);
//      Logger::Shutdown();
//
//  The log file will appear next to your .asi in the GTA V folder.
// ============================================================

#pragma once

namespace Logger
{
    // Opens the log file for writing. Call once at startup.
    // Filename is relative to the GTA V directory.
    void Init(const char* filename);

    // Writes a timestamped line to the log file.
    // Supports printf-style formatting:
    //   Logger::Log("Value is %d", 42);
    //   Logger::Log("Position: %f, %f", x, y);
    void Log(const char* format, ...);

    // Closes the log file cleanly. Call on shutdown.
    void Shutdown();
}
