#pragma once
#include "Settings.h"
#include <juce_events/juce_events.h>

namespace mm
{
/** Sets up, starts and stops the engine on this computer, so nobody has to open a terminal.
    The scripts that do the work travel inside the plugin and are written out when they are needed. */
class EngineRunner
{
public:
    explicit EngineRunner (Settings&);
    ~EngineRunner();

    bool usesThisComputer() const;          // the address in Settings points at this machine
    bool installed() const;                 // the engine's files are in the engine folder
    int port() const;

    bool openInstaller (juce::String& error);        // opens the setup in its own window, where its progress shows
    bool start (juce::String& error);
    void stop();
    bool startedHere() const                { return started; }
    juce::int64 startedAtMs() const         { return startedMs; }
    bool triedAutoStart = false;            // starting by itself happens once, so pressing Stop sticks

    juce::File logFile() const;
    juce::String lastLogLine() const;       // what the engine said last, for showing while it starts
    static juce::String tidyLogLine (const juce::String& raw);

private:
    juce::File scriptsDir() const;
    juce::File writeScript (const juce::String& name) const;

    Settings& settings;
    juce::ChildProcess process;
    bool started = false;
    juce::int64 startedMs = 0;
};
}
