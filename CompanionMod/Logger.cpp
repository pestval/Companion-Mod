// ============================================================
//  Logger.cpp — File Logging System (Implementation)
// ============================================================
//
//  TECHNICAL NOTES:
//
//  FILE* is C's way of representing an open file. It's the
//  low-level equivalent of C#'s StreamWriter. We use C-style
//  file I/O (fopen, fprintf, fclose) because:
//  1. It's simpler than C++ streams for this use case
//  2. It's what most GTA V modders use
//  3. It's guaranteed to work without C++ runtime dependencies
//
//  va_list / va_start / va_end:
//  These are how C handles "..." (variadic) arguments.
//  When you call Logger::Log("frame %d", 42), the "..."
//  captures the 42. We then pass it to vfprintf which knows
//  how to format it just like printf would.
//
//  fflush():
//  Forces the OS to write buffered data to disk immediately.
//  Without this, if GTA crashes, your last few log lines
//  might be lost (stuck in a memory buffer). With fflush,
//  every line is guaranteed to be on disk.
// ============================================================

#include "Logger.h"
#include <cstdio>     // FILE, fopen, fprintf, fclose, fflush
#include <cstdarg>    // va_list, va_start, va_end
#include <ctime>      // time, localtime, strftime

// --------------------------------------------------------
//  Module-level state
// --------------------------------------------------------
//  'static' here means "only visible within this .cpp file."
//  No other file can access g_LogFile directly — they MUST
//  go through Logger::Init/Log/Shutdown.
//
//  This is encapsulation at the file level. In your C# mod,
//  you used 'private' on class members. In C++, 'static' at
//  file scope achieves the same thing.
// --------------------------------------------------------
static FILE* g_LogFile = nullptr;

namespace Logger
{
    // --------------------------------------------------------
    //  Init — Open the log file
    // --------------------------------------------------------
    //  "w" mode means "write" — it creates the file if it
    //  doesn't exist, or clears it if it does. This means
    //  each game session starts with a fresh log.
    //
    //  If you wanted to APPEND to existing logs instead,
    //  you'd use "a" mode. We use "w" for now because
    //  during development, fresh logs are easier to read.
    // --------------------------------------------------------
    void Init(const char* filename)
    {
        if (g_LogFile != nullptr)
            return;  // Already initialized, don't double-open

        g_LogFile = fopen(filename, "w");

        if (g_LogFile != nullptr)
        {
            Log("Logger initialized");
        }
    }

    // --------------------------------------------------------
    //  Log — Write a timestamped, formatted line
    // --------------------------------------------------------
    //  Output format: [HH:MM:SS] Your message here
    //
    //  Example output:
    //      [14:32:07] === CompanionMod ASI Loaded ===
    //      [14:32:07] Phase 1 — Skeleton active.
    //      [14:32:17] Heartbeat — frame 600
    //      [14:33:41] Mission STARTED
    //
    //  The timestamp uses localtime() which gives you the
    //  system clock time. This is helpful for correlating
    //  log events with what you saw on screen.
    // --------------------------------------------------------
    void Log(const char* format, ...)
    {
        if (g_LogFile == nullptr)
            return;  // Logger not initialized yet

        // --- Write timestamp ---
        time_t now = time(nullptr);
        struct tm* timeInfo = localtime(&now);
        char timeBuffer[16];
        strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", timeInfo);
        fprintf(g_LogFile, "[%s] ", timeBuffer);

        // --- Write the formatted message ---
        // va_list is how C processes "..." arguments.
        // This is the mechanism behind printf-style formatting.
        va_list args;
        va_start(args, format);       // Initialize args from the "..." 
        vfprintf(g_LogFile, format, args);  // Write formatted text
        va_end(args);                 // Clean up

        // --- End the line and flush ---
        fprintf(g_LogFile, "\n");
        fflush(g_LogFile);  // Force write to disk immediately
    }

    // --------------------------------------------------------
    //  Shutdown — Close the log file
    // --------------------------------------------------------
    //  Called from DllMain when the game is closing.
    //  Always close files you open — it's good practice
    //  and prevents data loss.
    // --------------------------------------------------------
    void Shutdown()
    {
        if (g_LogFile != nullptr)
        {
            Log("Logger shutting down");
            fclose(g_LogFile);
            g_LogFile = nullptr;
        }
    }
}
