# MU Music Maker

A plugin from Musically Unorthodox that makes whole songs inside your DAW: beats, instruments and vocals, from a
description or from your own lyrics. Think Suno, but it lives on a track in your project and runs on your own computer.

Type what you want, press **Make the song**, and a couple of versions land in the list. Play them through the plugin,
then drag the one you like straight onto a track.

Works in Logic, FL Studio, Ableton and anything else that loads AU or VST3 plugins, on Mac and Windows. There is a
standalone app too, for when you just want to make something without opening a project.

## Install

Go to **Releases** on the right of this page and download the newest build.

- **Mac:** `MUMusicMaker-Mac.pkg`. Double-click it and click through. The first time, macOS says it could not check
  the file: press Done, open System Settings > Privacy & Security, scroll to the bottom and press Open Anyway.
- **Windows:** `MUMusicMaker-Windows-Setup.exe`. Close your DAW first. If Windows says it does not recognise the
  publisher, choose More info > Run anyway.

Then open your DAW, rescan plugins if it does not show up, and put **MU Music Maker** on any audio track
(in Logic it is under Audio FX > Musically Unorthodox).

## Set up the engine (once)

The music is made by an open source model called ACE-Step 1.5. It runs on your computer, so there is no account,
no credits and nothing sent anywhere. It is a big download and has to be set up once:

1. Open the plugin, go to **Settings** and press **Set up the engine**. A window opens and does everything by itself.
   It downloads about 10 GB, so leave it open until it says All done.
2. Back in Settings, press **Start the engine**. It takes a minute or so to come up.
3. When the dot at the bottom left goes green, you are ready. After that the plugin starts the engine by itself.
   The first song after each start is slower, because that is when the engine loads its model.

What it needs:

- **Mac:** Apple Silicon (M1 or newer). 16 GB of memory is comfortable.
- **Windows:** an NVIDIA graphics card with 6 GB or more.
- About 20 GB of free disk space.

If the engine is already on your computer (for example in `C:\ACE-Step-1.5`), press **Change** next to the engine
folder in Settings and point it there instead of setting it up again.

## Using it

- **Describe it:** say what you want in a sentence or two. The engine writes the lyrics and the music.
- **Write it yourself:** set the style with the tags, write your own lyrics with `[Verse]` and `[Chorus]` on their own
  lines, and pick the tempo, key and length. **Use the project's tempo** makes the song at your project's speed.
- **Start from audio:** borrow the sound of a piece of audio, remake it in a new style, or redo one part of it.
  Press **Catch from the track** and press play to record what the track is playing, choose a file, or drop one on
  the plugin.
- **Versions:** how many takes you get back. No two come out the same.
- **Library:** everything you have made, with search and likes. The dots on a song let you make more like it,
  start a new song from its audio, see the lyrics, rename it or delete it.
- **Into your project:** drag a song out of the list, or drag the **Drag to project** handle at the bottom,
  and drop it on a track.

The Help page inside the plugin goes through all of this.

### Two computers

The engine can run on one computer while you work on another, for example a PC with a strong graphics card making the
songs for a laptop. On the engine's computer, tick **Let other computers on my network use this engine** in Settings and
start the engine again. On the other one, put the first computer's address in **Where the engine is**,
like `http://192.168.1.20:8001`.

## Updates

The plugin checks for a new version by itself. When there is one, Settings shows **Install the update**: press it,
then quit and reopen your DAW. On Windows the button downloads the new installer for you to run.

## Where things are kept

- Your songs: `Music/MU Music Maker/Songs`
- Audio caught from a track: `Music/MU Music Maker/Takes`
- Settings and the engine: the app data folder for your user (`MU Music Maker`)

The plugin never changes the sound of the track it is on. Audio passes straight through.

## Building

Every push to `main` builds the Mac and Windows installers on GitHub Actions and publishes them as a release.
The tests run against a small stand-in for the engine (`Source/tests/mock_engine.py`), so they need no graphics card.

To build it yourself: `cmake -B build && cmake --build build --config Release`.

## Credits

Music generation by [ACE-Step 1.5](https://github.com/ace-step/ACE-Step-1.5) (MIT licence).
This plugin started from the idea of [ace-step-ui](https://github.com/fspecii/ace-step-ui) (MIT licence), rebuilt as a
plugin; its list of styles is used here. Built with [JUCE](https://juce.com).
