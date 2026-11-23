#include "Online.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cstring>

namespace mm
{
OnlineClient::OnlineClient (juce::String baseUrl, juce::String apiKey) : base (std::move (baseUrl)), key (std::move (apiKey))
{
    while (base.endsWithChar ('/')) base = base.dropLastCharacters (1);
}

HttpRequest OnlineClient::request (const juce::String& method, const juce::String& path) const
{
    HttpRequest r;
    r.method = method;
    r.url = base + path;
    r.connectTimeoutMs = 8000;
    if (key.isNotEmpty()) r.headers.add ("Authorization: Bearer " + key);
    r.headers.add ("User-Agent: MUMusicMaker/1.0");
    return r;
}

static juce::String failureText (const HttpResponse& res)
{
    if (res.status == 0)   return "The online service could not be reached. Check the internet connection.";
    if (res.status == 401 || res.status == 403) return "The online service did not accept that key. Check it in Settings.";
    if (res.status == 429) return "The online service is busy, or this key has made too many songs for now. Try again in a minute.";
    if (res.status == 413) return "The audio to start from is too big to send. Use a shorter piece.";
    if (res.status >= 500 && res.json()["detail"].toString().isEmpty() && res.json()["error"]["message"].toString().isEmpty())
        return "The online service is having trouble (error " + juce::String (res.status) + "). Try again in a minute.";
    return res.errorText();
}

EngineStatus OnlineClient::check (juce::Array<EngineModel>& models) const
{
    EngineStatus s;
    s.online = true;
    if (key.isEmpty())
    {
        s.needsKey = true;
        s.error = "Add your free key in Settings";
        return s;
    }

    auto r = request ("GET", "/v1/models");
    r.idleTimeoutMs = 15000;
    const auto res = httpSend (r);
    if (! res.ok())
    {
        s.needsKey = res.status == 401 || res.status == 403;
        s.error = failureText (res);
        return s;
    }

    const auto answer = res.json();
    if (auto* list = answer["data"].getArray())
        for (auto& m : *list)
        {
            EngineModel e;
            e.name = m["id"].toString();
            e.isDefault = models.isEmpty();
            e.isLoaded = true;
            if (e.name.isNotEmpty()) models.add (e);
        }
    if (! answer.isObject()) { s.error = "Something else is answering at that address, not the online service."; return s; }

    s.reachable = true;
    s.modelsReady = true;
    s.languageModelReady = true;
    s.model = models.isEmpty() ? juce::String() : models.getReference (0).name;
    return s;
}

juce::var OnlineClient::buildPayload (const SongRequest& s, const juce::String& model, const juce::String& audioFormat,
                                      const juce::String& audioBase64, const juce::String& audioFileFormat)
{
    const bool simple = s.mode == SongRequest::Mode::simple;
    juce::String words;
    if (simple)
    {
        words = s.description.trim();
        if (s.instrumental && ! words.containsIgnoreCase ("instrumental")) words << ", instrumental";
    }
    else
    {
        words << "<prompt>" << s.style.trim() << "</prompt>";
        if (! s.instrumental && s.lyrics.trim().isNotEmpty()) words << "<lyrics>" << s.lyrics.trim() << "</lyrics>";
    }

    auto* message = new juce::DynamicObject();
    message->setProperty ("role", "user");
    if (audioBase64.isNotEmpty())
    {
        // words first, then the audio to start from
        auto* text = new juce::DynamicObject();
        text->setProperty ("type", "text");
        text->setProperty ("text", words);
        auto* audioData = new juce::DynamicObject();
        audioData->setProperty ("data", audioBase64);
        audioData->setProperty ("format", audioFileFormat.isEmpty() ? juce::String ("flac") : audioFileFormat);
        auto* audio = new juce::DynamicObject();
        audio->setProperty ("type", "input_audio");
        audio->setProperty ("input_audio", juce::var (audioData));
        juce::Array<juce::var> parts;
        parts.add (juce::var (text));
        parts.add (juce::var (audio));
        message->setProperty ("content", parts);
    }
    else
    {
        message->setProperty ("content", words);
    }
    juce::Array<juce::var> messages;
    messages.add (juce::var (message));

    auto* config = new juce::DynamicObject();
    config->setProperty ("format", audioFormat);
    config->setProperty ("vocal_language", s.language.isEmpty() ? juce::String ("en") : s.language);
    if (s.seconds > 0.0)                    config->setProperty ("duration", juce::jlimit (10.0, 600.0, s.seconds));
    if (s.bpm > 0)                          config->setProperty ("bpm", juce::jlimit (30, 300, s.bpm));
    if (s.key.trim().isNotEmpty())          config->setProperty ("key_scale", s.key.trim());
    if (s.timeSignature.trim().isNotEmpty())
    {
        const auto t = s.timeSignature.trim();
        config->setProperty ("time_signature", t.containsChar ('/') ? t : (t == "6" ? juce::String ("6/8") : t + "/4"));
    }
    if (s.instrumental)                     config->setProperty ("instrumental", true);

    auto* o = new juce::DynamicObject();
    o->setProperty ("model", model);
    o->setProperty ("messages", messages);
    o->setProperty ("stream", true);
    o->setProperty ("sample_mode", simple && s.source == SongRequest::Source::none);
    o->setProperty ("thinking", s.thinking && s.source != SongRequest::Source::cover && s.source != SongRequest::Source::repaint);
    o->setProperty ("use_format", ! simple && s.enhance);
    o->setProperty ("batch_size", juce::jlimit (1, 4, s.variations));
    if (! s.randomSeed) o->setProperty ("seed", juce::String (s.seed));
    o->setProperty ("audio_config", juce::var (config));

    switch (s.source)
    {
        case SongRequest::Source::none:
        case SongRequest::Source::reference:
            o->setProperty ("task_type", "text2music");
            break;
        case SongRequest::Source::cover:
            o->setProperty ("task_type", "cover");
            o->setProperty ("audio_cover_strength", juce::jlimit (0.0, 1.0, s.strength));
            break;
        case SongRequest::Source::repaint:
            o->setProperty ("task_type", "repaint");
            o->setProperty ("repainting_start", juce::jmax (0.0, s.repaintStart));
            if (s.repaintEnd >= 0.0) o->setProperty ("repainting_end", s.repaintEnd);
            break;
    }
    return juce::var (o);
}

juce::MemoryBlock OnlineClient::packForSending (const juce::File& file, juce::String& format)
{
    juce::MemoryBlock out;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    if (auto reader = std::unique_ptr<juce::AudioFormatReader> (formats.createReaderFor (file)))
    {
        const int channels = (int) juce::jlimit ((unsigned int) 1, (unsigned int) 2, reader->numChannels);
        juce::FlacAudioFormat flac;
        auto* stream = new juce::MemoryOutputStream (out, false);
        if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (flac.createWriterFor (stream, reader->sampleRate, (unsigned int) channels, 16, {}, 5)))
        {
            const bool ok = writer->writeFromAudioReader (*reader, 0, reader->lengthInSamples);
            writer.reset();         // finishes the file and lets go of the stream
            if (ok && out.getSize() > 0) { format = "flac"; return out; }
        }
        else
        {
            delete stream;
        }
        out.reset();
    }

    // a file this build cannot read is sent as it is, and the service works it out
    file.loadFileAsData (out);
    format = file.getFileExtension().trimCharactersAtStart (".").toLowerCase();
    if (format.isEmpty()) format = "mp3";
    return out;
}

juce::File OnlineClient::unpackToWav (const juce::File& flacFile)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    const auto wavFile = flacFile.withFileExtension (".wav");
    {
        auto reader = std::unique_ptr<juce::AudioFormatReader> (formats.createReaderFor (flacFile));
        if (reader == nullptr) return flacFile;
        wavFile.deleteFile();
        auto stream = wavFile.createOutputStream();
        if (stream == nullptr) return flacFile;
        juce::WavAudioFormat wav;
        const int bits = reader->bitsPerSample >= 24 ? 24 : 16;
        auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.get(), reader->sampleRate, reader->numChannels, bits, {}, 0));
        if (writer == nullptr) return flacFile;
        stream.release();
        if (! writer->writeFromAudioReader (*reader, 0, reader->lengthInSamples)) { writer.reset(); wavFile.deleteFile(); return flacFile; }
    }
    flacFile.deleteFile();
    return wavFile;
}

juce::String OnlineClient::extensionFor (const juce::MemoryBlock& first, const juce::String& mimeType)
{
    const auto* b = static_cast<const unsigned char*> (first.getData());
    const size_t n = first.getSize();
    if (n >= 4 && std::memcmp (b, "RIFF", 4) == 0) return ".wav";
    if (n >= 4 && std::memcmp (b, "fLaC", 4) == 0) return ".flac";
    if (n >= 4 && std::memcmp (b, "OggS", 4) == 0) return ".ogg";
    if (n >= 3 && std::memcmp (b, "ID3", 3) == 0)  return ".mp3";
    if (n >= 2 && b[0] == 0xff && (b[1] & 0xe0) == 0xe0) return ".mp3";

    const auto mime = mimeType.toLowerCase();
    if (mime.contains ("wav"))  return ".wav";
    if (mime.contains ("flac")) return ".flac";
    if (mime.contains ("ogg") || mime.contains ("opus")) return ".ogg";
    if (mime.contains ("aac") || mime.contains ("mp4")) return ".m4a";
    return ".mp3";
}

void OnlineClient::parseDetails (const juce::String& text, OnlineResult& out)
{
    // The service describes what it made in a few labelled lines, then the lyrics under their own heading:
    //   **Caption:** ...   **BPM:** 120   **Duration:** 30s   **Key:** C major   ## Lyrics
    const auto lines = juce::StringArray::fromLines (text);
    bool inLyrics = false;
    juce::StringArray lyricLines;
    for (auto& raw : lines)
    {
        const auto line = raw.trim();
        if (line.startsWith ("##"))
        {
            inLyrics = line.containsIgnoreCase ("lyrics");
            continue;
        }
        if (inLyrics) { lyricLines.add (raw.trimEnd()); continue; }
        if (! line.startsWith ("**")) continue;

        const auto label = line.fromFirstOccurrenceOf ("**", false, false).upToFirstOccurrenceOf ("**", false, false).trimCharactersAtEnd (": ").toLowerCase();
        const auto value = line.fromFirstOccurrenceOf ("**", false, false).fromFirstOccurrenceOf ("**", false, false).trimCharactersAtStart (": ").trim();
        if (value.isEmpty() || value == "N/A") continue;
        if (label == "caption")                         out.caption = value;
        else if (label == "bpm")                        out.bpm = value.getDoubleValue();
        else if (label == "duration")                   out.seconds = value.getDoubleValue();
        else if (label == "key" || label == "key scale") out.key = value;
        else if (label.startsWith ("time"))             out.timeSignature = value;
    }
    out.lyrics = lyricLines.joinIntoString ("\n").trim();
}

bool OnlineClient::parseAnswer (const juce::String& body, const juce::File& folder, const juce::String& filePrefix, OnlineResult& out, juce::String& error)
{
    // Either one JSON answer, or a stream of "data: {json}" lines. The audio sits inside as base64, which is
    // lifted straight out into files so several songs' worth of text never has to be parsed as JSON.
    juce::StringArray chunks;
    if (body.trimStart().startsWithChar ('{'))
    {
        chunks.add (body);
    }
    else
    {
        for (auto& line : juce::StringArray::fromLines (body))
            if (line.startsWith ("data:"))
            {
                const auto payload = line.substring (5).trim();
                if (payload.isNotEmpty() && payload != "[DONE]") chunks.add (payload);
            }
    }

    juce::String words;
    bool failed = false;
    for (auto chunk : chunks)
    {
        for (;;)
        {
            const int at = chunk.indexOf ("data:audio/");
            if (at < 0) break;
            const int end = chunk.indexOfChar (at, '"');
            if (end < 0) break;
            const auto url = chunk.substring (at, end);
            chunk = chunk.substring (0, at) + chunk.substring (end);

            const auto mime = url.substring (5).upToFirstOccurrenceOf (";", false, false).upToFirstOccurrenceOf (",", false, false);
            const auto encoded = url.fromFirstOccurrenceOf (",", false, false).removeCharacters ("\\ \r\n");
            juce::MemoryOutputStream decoded;
            if (encoded.isEmpty() || ! juce::Base64::convertFromBase64 (decoded, encoded) || decoded.getDataSize() < 64) continue;

            juce::MemoryBlock first (decoded.getData(), juce::jmin ((size_t) 16, decoded.getDataSize()));
            const auto file = folder.getChildFile (filePrefix + "-" + juce::String (out.files.size() + 1) + extensionFor (first, mime));
            file.deleteFile();
            if (file.replaceWithData (decoded.getData(), decoded.getDataSize())) out.files.add (file);
        }

        const auto v = juce::JSON::parse (chunk);
        if (! v.isObject()) continue;
        if (v["error"].isObject() || v["error"].isString() || v["detail"].isString())
        {
            error = v["detail"].toString();
            if (error.isEmpty()) error = v["error"].isString() ? v["error"].toString() : v["error"]["message"].toString();
            failed = true;
            continue;
        }
        if (auto* choices = v["choices"].getArray())
            for (auto& choice : *choices)
            {
                for (auto* holder : { "delta", "message" })
                    if (auto* audio = choice[holder]["audio"].getArray())
                        for (auto& item : *audio)
                        {
                            const auto link = item["audio_url"]["url"].toString();
                            if (link.startsWithIgnoreCase ("http")) out.links.addIfNotAlreadyThere (link);
                        }

                auto piece = choice["delta"]["content"].toString();
                if (piece.isEmpty()) piece = choice["message"]["content"].toString();
                if (piece != ".") words << piece;                 // a lone dot is the service saying it is still working
                if (choice["finish_reason"].toString() == "error") failed = true;
            }
    }

    if (failed)
    {
        if (error.isEmpty()) error = words.trim().isNotEmpty() ? words.trim() : juce::String ("The online service could not make this one.");
        for (auto& f : out.files) f.deleteFile();
        out.files.clear();
        return false;
    }
    if (out.files.isEmpty() && out.links.isEmpty())
    {
        error = "The online service answered but sent no audio. Try again.";
        return false;
    }
    parseDetails (words, out);
    return true;
}

bool OnlineClient::generate (const SongRequest& s, const juce::String& audioFormat, const juce::File& folder, const juce::String& filePrefix,
                             OnlineResult& out, juce::String& error, std::function<bool()> shouldCancel) const
{
    if (key.isEmpty()) { error = "Add your free key in Settings first."; return false; }

    // which model: the one asked for if it is one of theirs, otherwise the first the service offers
    juce::String model = s.model.startsWith ("acemusic/") ? s.model : juce::String();
    if (model.isEmpty())
    {
        juce::Array<EngineModel> models;
        const auto status = check (models);
        if (! status.reachable) { error = status.error; return false; }
        model = models.isEmpty() ? juce::String ("acemusic/acestep-v15-turbo") : models.getReference (0).name;
    }

    juce::String audioBase64, audioFileFormat;
    if (s.source != SongRequest::Source::none)
    {
        const juce::File audio (s.audioPath);
        const auto packed = packForSending (audio, audioFileFormat);
        if (packed.getSize() == 0) { error = "Could not read " + audio.getFileName(); return false; }
        if (packed.getSize() > (size_t) 60 * 1024 * 1024) { error = "The audio to start from is too big to send. Use a shorter piece."; return false; }
        audioBase64 = juce::Base64::toBase64 (packed.getData(), packed.getSize());
    }

    auto r = request ("POST", "/v1/chat/completions");
    r.headers.add ("Content-Type: application/json; charset=utf-8");
    r.headers.add ("Accept: text/event-stream");
    // A WAV is asked for as FLAC, which is the same sound in half the download, and turned back into a WAV here.
    const bool wantsWav = audioFormat == "wav";
    const auto text = juce::JSON::toString (buildPayload (s, model, wantsWav ? juce::String ("flac") : audioFormat, audioBase64, audioFileFormat), true);
    r.body.append (text.toRawUTF8(), text.getNumBytesAsUTF8());
    r.idleTimeoutMs = 300000;           // the service can think for a while before the first word comes back
    r.shouldCancel = std::move (shouldCancel);

    const auto res = httpSend (r);
    if (res.error == "Cancelled") { error = "Cancelled"; return false; }
    if (! res.ok()) { error = failureText (res); return false; }
    if (! parseAnswer (res.text(), folder, filePrefix, out, error)) return false;

    // audio handed over as a link is fetched now
    for (auto& link : out.links)
    {
        const auto target = folder.getChildFile (filePrefix + "-" + juce::String (out.files.size() + 1) + ".download");
        HttpRequest get;
        get.url = link;
        get.saveTo = target;
        get.idleTimeoutMs = 60000;
        get.headers.add ("User-Agent: MUMusicMaker/1.0");
        if (! httpSend (get).ok() || target.getSize() < 64) { target.deleteFile(); continue; }

        juce::MemoryBlock first;
        {
            juce::FileInputStream in (target);
            if (in.openedOk()) in.readIntoMemoryBlock (first, 16);
        }
        const auto named = target.withFileExtension (extensionFor (first, link.upToFirstOccurrenceOf ("?", false, false).fromLastOccurrenceOf (".", false, false)));
        named.deleteFile();
        if (target.moveFileTo (named)) out.files.add (named);
    }
    if (out.files.isEmpty()) { error = "The online service made the song but it could not be fetched. Try again."; return false; }

    if (wantsWav)
        for (auto& f : out.files)
            if (f.hasFileExtension ("flac")) f = unpackToWav (f);
    return true;
}
}
