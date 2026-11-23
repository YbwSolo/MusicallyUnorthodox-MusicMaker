#include "EngineRunner.h"
#include "Http.h"
#include "BinaryData.h"

namespace mm
{
EngineRunner::EngineRunner (Settings& s) : settings (s) {}

EngineRunner::~EngineRunner()
{
    // an engine this plugin started goes away with it, unless it was asked to stay
    if (started && ! settings.getBool ("keepEngineRunning", false)) stop();
}

bool EngineRunner::usesThisComputer() const
{
    const auto host = splitUrl (settings.engineUrl()).host.toLowerCase();
    return host == "127.0.0.1" || host == "localhost" || host == "::1" || host == "0.0.0.0";
}

int EngineRunner::port() const
{
    const auto parts = splitUrl (settings.engineUrl());
    return parts.valid ? parts.port : 8001;
}

bool EngineRunner::installed() const
{
    const auto dir = settings.engineFolder();
    return dir.getChildFile ("acestep").isDirectory()
        && (dir.getChildFile (".venv").isDirectory() || dir.getChildFile ("python_embedded").isDirectory() || dir.getChildFile ("python_embeded").isDirectory());
}

juce::File EngineRunner::scriptsDir() const
{
    auto dir = Settings::appDataDir().getChildFile ("scripts");
    dir.createDirectory();
    return dir;
}

juce::File EngineRunner::logFile() const { return Settings::appDataDir().getChildFile ("engine.log"); }

juce::File EngineRunner::writeScript (const juce::String& name) const
{
    int size = 0;
    const auto resource = name.replaceCharacter ('.', '_');
    const char* data = BinaryData::getNamedResource (resource.toRawUTF8(), size);
    if (data == nullptr || size <= 0) return {};

    auto text = juce::String::fromUTF8 (data, size).replace ("\r\n", "\n");
   #if JUCE_WINDOWS
    text = text.replace ("\n", "\r\n");
   #endif
    const auto file = scriptsDir().getChildFile (name);
    if (! file.replaceWithText (text, false, false, nullptr)) return {};
   #if ! JUCE_WINDOWS
    file.setExecutePermission (true);
   #endif
    return file;
}

bool EngineRunner::openInstaller (juce::String& error)
{
    const auto dir = settings.engineFolder();
    if (! dir.createDirectory()) { error = "Could not create " + dir.getFullPathName(); return false; }

   #if JUCE_WINDOWS
    const auto script = writeScript ("install_engine_windows.bat");
    if (script == juce::File()) { error = "The setup could not be written to disk."; return false; }
    // a small launcher with the folder filled in, opened in its own window so the progress is there to see
    const auto launcher = scriptsDir().getChildFile ("Set up the engine.bat");
    launcher.replaceWithText ("@echo off\r\ncall \"" + script.getFullPathName() + "\" \"" + dir.getFullPathName() + "\"\r\n", false, false, nullptr);
    if (! launcher.startAsProcess()) { error = "The setup window could not be opened."; return false; }
   #else
    const auto script = writeScript ("install_engine_mac.sh");
    if (script == juce::File()) { error = "The setup could not be written to disk."; return false; }
    const auto launcher = scriptsDir().getChildFile ("Set up the engine.command");
    launcher.replaceWithText ("#!/bin/bash\nexec /bin/bash \"" + script.getFullPathName() + "\" \"" + dir.getFullPathName() + "\"\n", false, false, nullptr);
    launcher.setExecutePermission (true);
    #if JUCE_MAC
    juce::ChildProcess opener;
    if (! opener.start (juce::StringArray { "/usr/bin/open", "-a", "Terminal", launcher.getFullPathName() })) { error = "Terminal could not be opened."; return false; }
    opener.waitForProcessToFinish (4000);
    #else
    if (! launcher.startAsProcess()) { error = "The setup window could not be opened."; return false; }
    #endif
   #endif
    return true;
}

bool EngineRunner::start (juce::String& error)
{
    if (! usesThisComputer()) { error = "The engine address in Settings points at another computer, so it has to be started there."; return false; }
    if (! installed())        { error = "The engine is not set up on this computer yet. Press Set up the engine first."; return false; }

    const auto dir = settings.engineFolder();
    const auto host = settings.getBool ("shareEngine", false) ? juce::String ("0.0.0.0") : juce::String ("127.0.0.1");
    const auto log = logFile();
    log.deleteFile();

   #if JUCE_WINDOWS
    const auto script = writeScript ("start_engine_windows.bat");
    if (script == juce::File()) { error = "The engine's start-up could not be written to disk."; return false; }
    // cmd strips the outermost pair of quotes with /s, which is what lets every path inside keep its own
    const auto command = "cmd.exe /s /c \"\"" + script.getFullPathName() + "\" \"" + dir.getFullPathName() + "\" " + host + " "
                       + juce::String (port()) + " \"" + log.getFullPathName() + "\"\"";
    const bool ok = process.start (command, 0);
   #else
    const auto script = writeScript ("start_engine_mac.sh");
    if (script == juce::File()) { error = "The engine's start-up could not be written to disk."; return false; }
    const bool ok = process.start (juce::StringArray { "/bin/bash", script.getFullPathName(), dir.getFullPathName(), host,
                                                       juce::String (port()), log.getFullPathName() }, 0);
   #endif

    if (! ok) { error = "The engine could not be started."; return false; }
    started = true;
    startedMs = juce::Time::currentTimeMillis();
    return true;
}

void EngineRunner::stop()
{
   #if JUCE_WINDOWS
    const auto script = writeScript ("stop_engine_windows.bat");
    if (script != juce::File())
    {
        juce::ChildProcess stopper;
        if (stopper.start ("cmd.exe /s /c \"\"" + script.getFullPathName() + "\" " + juce::String (port()) + "\"", 0))
            stopper.waitForProcessToFinish (8000);
    }
   #else
    const auto script = writeScript ("stop_engine_mac.sh");
    if (script != juce::File())
    {
        juce::ChildProcess stopper;
        if (stopper.start (juce::StringArray { "/bin/bash", script.getFullPathName(), juce::String (port()) }, 0))
            stopper.waitForProcessToFinish (8000);
    }
   #endif
    if (process.isRunning()) process.kill();
    started = false;
}

juce::String EngineRunner::tidyLogLine (const juce::String& raw)
{
    // drop colour codes and the timestamps loggers put in front, keep it to one readable line
    juce::String out;
    bool inEscape = false;
    for (auto c : raw)
    {
        if (c == 0x1b) { inEscape = true; continue; }
        if (inEscape) { if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) inEscape = false; continue; }
        if (c == '\r') { out.clear(); continue; }     // a progress bar redrawing itself: keep the latest
        out << juce::String::charToString (c);
    }
    out = out.trim();
    if (out.length() > 160) out = out.substring (0, 157) + "...";
    return out;
}

juce::String EngineRunner::lastLogLine() const
{
    const auto log = logFile();
    if (! log.existsAsFile()) return {};
    juce::FileInputStream in (log);
    if (! in.openedOk()) return {};
    const auto size = in.getTotalLength();
    in.setPosition (juce::jmax ((juce::int64) 0, size - 4096));
    auto lines = juce::StringArray::fromLines (in.readEntireStreamAsString());
    for (int i = lines.size(); --i >= 0;)
    {
        const auto line = tidyLogLine (lines[i]);
        if (line.isNotEmpty()) return line;
    }
    return {};
}
}
