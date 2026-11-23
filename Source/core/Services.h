#pragma once
#include "Settings.h"
#include "Library.h"
#include "Generator.h"
#include "EngineRunner.h"
#include "Updater.h"

namespace mm
{
/** One of each for the whole host, however many copies of the plugin are open. */
struct Services
{
    Settings settings;
    Library library;
    EngineWatch watch { settings };
    EngineRunner runner { settings };
    Generator generator { settings, library, watch };
    Updater updater { settings };
};
}
