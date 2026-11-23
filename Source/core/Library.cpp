#include "Library.h"

namespace mm
{
juce::var Song::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("id", id);
    o->setProperty ("title", title);
    o->setProperty ("style", style);
    o->setProperty ("lyrics", lyrics);
    o->setProperty ("key", key);
    o->setProperty ("timeSignature", timeSignature);
    o->setProperty ("model", model);
    o->setProperty ("seed", seed);
    o->setProperty ("fileName", fileName);
    o->setProperty ("jobId", jobId);
    o->setProperty ("bpm", bpm);
    o->setProperty ("seconds", seconds);
    o->setProperty ("liked", liked);
    o->setProperty ("createdMs", juce::String (createdMs));
    o->setProperty ("variation", variation);
    o->setProperty ("variationsTotal", variationsTotal);
    o->setProperty ("request", request);
    return juce::var (o);
}

Song Song::fromVar (const juce::var& v)
{
    Song s;
    s.id = v["id"].toString();
    s.title = v["title"].toString();
    s.style = v["style"].toString();
    s.lyrics = v["lyrics"].toString();
    s.key = v["key"].toString();
    s.timeSignature = v["timeSignature"].toString();
    s.model = v["model"].toString();
    s.seed = v["seed"].toString();
    s.fileName = v["fileName"].toString();
    s.jobId = v["jobId"].toString();
    s.bpm = (double) v["bpm"];
    s.seconds = (double) v["seconds"];
    s.liked = (bool) v["liked"];
    s.createdMs = v["createdMs"].toString().getLargeIntValue();
    s.variation = juce::jmax (1, (int) v["variation"]);
    s.variationsTotal = juce::jmax (1, (int) v["variationsTotal"]);
    s.request = v["request"];
    return s;
}

juce::String Library::formatLength (double seconds)
{
    const int total = juce::jmax (0, juce::roundToInt (seconds));
    return juce::String (total / 60) + ":" + juce::String (total % 60).paddedLeft ('0', 2);
}

juce::String Song::lengthText() const { return Library::formatLength (seconds); }

juce::String Song::detailText() const
{
    juce::StringArray parts;
    if (bpm > 0.0) parts.add (juce::String (juce::roundToInt (bpm)) + " BPM");
    if (key.isNotEmpty()) parts.add (key);
    return parts.joinIntoString (juce::String::fromUTF8 ("  \xc2\xb7  "));
}

//==============================================================================
Library::Library() : Library (Settings::libraryDir()) {}

Library::Library (const juce::File& folder)
{
    songsFolder = folder.getChildFile ("Songs");
    songsFolder.createDirectory();
    indexFile = folder.getChildFile ("library.json");
    load();
}

void Library::load()
{
    const juce::ScopedLock sl (lock);
    all.clear();
    const auto v = juce::JSON::parse (indexFile);
    if (auto* list = v["songs"].getArray())
        for (auto& item : *list)
        {
            auto s = Song::fromVar (item);
            if (s.id.isNotEmpty() && songsFolder.getChildFile (s.fileName).existsAsFile()) all.add (s);
        }
}

void Library::save()
{
    juce::Array<juce::var> list;
    {
        const juce::ScopedLock sl (lock);
        for (auto& s : all) list.add (s.toVar());
    }
    auto* o = new juce::DynamicObject();
    o->setProperty ("version", 1);
    o->setProperty ("songs", list);
    // written beside the old file and swapped in, so a crash half way never leaves an empty library
    const auto temp = indexFile.getSiblingFile (indexFile.getFileName() + ".new");
    if (temp.replaceWithText (juce::JSON::toString (juce::var (o))))
        temp.moveFileTo (indexFile);
}

juce::Array<Song> Library::songs() const
{
    const juce::ScopedLock sl (lock);
    return all;
}

int Library::count() const
{
    const juce::ScopedLock sl (lock);
    return all.size();
}

juce::Array<Song> Library::search (const juce::String& text, bool likedOnly) const
{
    const auto words = juce::StringArray::fromTokens (text.toLowerCase().trim(), " ", {});
    juce::Array<Song> out;
    const juce::ScopedLock sl (lock);
    for (auto& s : all)
    {
        if (likedOnly && ! s.liked) continue;
        const auto haystack = (s.title + " " + s.style + " " + s.key + " " + s.lyrics).toLowerCase();
        bool match = true;
        for (auto& w : words)
            if (w.isNotEmpty() && ! haystack.contains (w)) { match = false; break; }
        if (match) out.add (s);
    }
    return out;
}

bool Library::find (const juce::String& id, Song& out) const
{
    const juce::ScopedLock sl (lock);
    for (auto& s : all)
        if (s.id == id) { out = s; return true; }
    return false;
}

juce::String Library::safeFileName (const juce::String& name)
{
    auto cleaned = name.trim().removeCharacters ("\\/:*?\"<>|\r\n\t");
    while (cleaned.contains ("  ")) cleaned = cleaned.replace ("  ", " ");
    cleaned = cleaned.trimCharactersAtEnd (". ").substring (0, 60).trim();
    return cleaned.isEmpty() ? juce::String ("Song") : cleaned;
}

juce::File Library::audioOf (const Song& s) const { return songsFolder.getChildFile (s.fileName); }

Song Library::add (Song s, const juce::File& audio)
{
    if (s.id.isEmpty()) s.id = juce::Uuid().toString().substring (0, 12);
    if (s.createdMs == 0) s.createdMs = juce::Time::currentTimeMillis();
    if (s.title.trim().isEmpty()) s.title = "Untitled";

    // the file gets the song's name, so it reads properly once it is dragged into a project
    const auto base = safeFileName (s.title);
    const auto extension = audio.getFileExtension().isNotEmpty() ? audio.getFileExtension() : juce::String (".wav");
    auto target = songsFolder.getChildFile (base + extension);
    for (int n = 2; target.existsAsFile(); ++n)
        target = songsFolder.getChildFile (base + " " + juce::String (n) + extension);

    if (audio != target && ! audio.moveFileTo (target))
        if (! audio.copyFileTo (target)) return {};
    s.fileName = target.getFileName();

    {
        const juce::ScopedLock sl (lock);
        all.insert (0, s);
    }
    save();
    sendChangeMessage();
    return s;
}

void Library::remove (const juce::String& id)
{
    {
        const juce::ScopedLock sl (lock);
        for (int i = all.size(); --i >= 0;)
            if (all.getReference (i).id == id)
            {
                songsFolder.getChildFile (all.getReference (i).fileName).deleteFile();
                all.remove (i);
            }
    }
    save();
    sendChangeMessage();
}

void Library::setLiked (const juce::String& id, bool liked)
{
    {
        const juce::ScopedLock sl (lock);
        for (auto& s : all)
            if (s.id == id) s.liked = liked;
    }
    save();
    sendChangeMessage();
}

void Library::rename (const juce::String& id, const juce::String& title)
{
    if (title.trim().isEmpty()) return;
    {
        const juce::ScopedLock sl (lock);
        for (auto& s : all)
            if (s.id == id) s.title = title.trim();       // the file keeps its name, so projects that use it still find it
    }
    save();
    sendChangeMessage();
}
}
