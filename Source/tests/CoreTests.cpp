// Checks the parts that need no DAW and no real engine. Run with HOME pointing at an empty folder.
// The network checks talk to Source/tests/mock_engine.py, which this starts by itself when Python is there.
#include "core/Services.h"
#include "core/Online.h"
#include "core/Player.h"
#include "core/Recorder.h"
#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>

using namespace mm;

#ifndef MUM_TESTS_DIR
 #define MUM_TESTS_DIR ""
#endif

static int failures = 0;
static void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  ok    " : "  FAIL  ") << what << std::endl;
    if (! ok) ++failures;
}

static void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

static juce::File makeTone (const juce::File& file, double seconds, double rate = 44100.0)
{
    file.deleteFile();
    juce::WavAudioFormat wav;
    if (auto* stream = file.createOutputStream().release())
        if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream, rate, 2, 16, {}, 0)))
        {
            const int total = (int) (seconds * rate);
            juce::AudioBuffer<float> buffer (2, total);
            for (int i = 0; i < total; ++i)
            {
                const float v = 0.4f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 330.0 * (double) i / rate);
                buffer.setSample (0, i, v);
                buffer.setSample (1, i, v);
            }
            writer->writeFromAudioSampleBuffer (buffer, 0, total);
        }
    return file;
}

static juce::var mockConfig (const juce::String& base, const juce::String& json)
{
    HttpRequest r;
    r.method = "POST";
    r.url = base + "/mock/config";
    r.headers.add ("Content-Type: application/json");
    r.body.append (json.toRawUTF8(), json.getNumBytesAsUTF8());
    return httpSend (r).json();
}

static juce::var mockLast (const juce::String& base)
{
    HttpRequest r;
    r.url = base + "/mock/last";
    return httpSend (r).json();
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto scratch = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("mum-tests-" + juce::String (juce::Random::getSystemRandom().nextInt (100000)));
    scratch.createDirectory();

    std::cout << "Addresses" << std::endl;
    {
        auto u = splitUrl ("http://127.0.0.1:8001/v1/audio?path=%2Ftmp%2Fa.wav");
        check (u.valid && ! u.https && u.host == "127.0.0.1" && u.port == 8001 && u.path == "/v1/audio?path=%2Ftmp%2Fa.wav", "host, port and path are split apart");
        u = splitUrl ("https://engine.example.com");
        check (u.valid && u.https && u.port == 443 && u.path == "/", "https with nothing after the name");
        u = splitUrl ("http://[::1]:9000/health");
        check (u.valid && u.host == "::1" && u.port == 9000, "an IPv6 address in brackets");
        check (! splitUrl ("127.0.0.1:8001").valid && ! splitUrl ("ftp://x").valid, "anything that is not http is refused");

        Settings s;
        s.set ("engineUrl", "192.168.1.20:8001/");
        check (s.engineUrl() == "http://192.168.1.20:8001", "an address typed without http:// or with a stray slash is tidied");
        s.set ("engineUrl", "");
        check (s.engineUrl() == "http://127.0.0.1:8001", "an empty address means this computer");
        s.remove ("engineUrl");
    }

    std::cout << "What is sent to the engine" << std::endl;
    {
        SongRequest r;
        r.description = "A dark drill track about coming up";
        auto p = EngineClient::buildPayload (r, "wav", true);
        check (p["sample_query"].toString() == r.description && ! p.hasProperty ("prompt"), "a described song goes to the engine's writer");
        check (! p.hasProperty ("bpm") && ! p.hasProperty ("key_scale") && ! p.hasProperty ("audio_duration"), "tempo, key and length are left out when they are on Auto");
        check ((int) p["batch_size"] == 2 && p["audio_format"].toString() == "wav" && (bool) p["use_random_seed"] && ! p.hasProperty ("seed"), "versions, file type and a random seed");

        r.instrumental = true;
        p = EngineClient::buildPayload (r, "wav", true);
        check (p["sample_query"].toString().endsWith (", instrumental"), "an instrumental says so in the description");
        p = EngineClient::buildPayload (r, "wav", false);
        check (p["prompt"].toString() == r.description && p["lyrics"].toString() == "[Instrumental]" && ! p.hasProperty ("sample_query"),
               "without the writer the description is used as the style");

        SongRequest c;
        c.mode = SongRequest::Mode::custom;
        c.style = "dark trap, 808s";
        c.lyrics = "[Verse]\nHello\n";
        c.bpm = 140; c.key = "C minor"; c.timeSignature = "4"; c.seconds = 120; c.variations = 3; c.randomSeed = false; c.seed = 4242; c.thinking = true; c.enhance = true;
        p = EngineClient::buildPayload (c, "flac", true, "mlx");
        check (p["prompt"].toString() == "dark trap, 808s" && p["lyrics"].toString() == "[Verse]\nHello", "style and lyrics go in as written");
        check ((int) p["bpm"] == 140 && p["key_scale"].toString() == "C minor" && p["time_signature"].toString() == "4" && (double) p["audio_duration"] == 120.0, "tempo, key, time and length");
        check ((int) p["batch_size"] == 3 && ! (bool) p["use_random_seed"] && p["seed"].toString() == "4242", "a fixed seed is sent");
        check ((bool) p["thinking"] && (bool) p["use_format"] && p["lm_backend"].toString() == "mlx", "planning and writing-out are asked for when the writer is used");
        p = EngineClient::buildPayload (c, "flac", false, "mlx");
        check (! (bool) p["thinking"] && ! p.hasProperty ("use_format") && ! p.hasProperty ("lm_backend"), "and left out when it is not");

        c.source = SongRequest::Source::cover; c.strength = 0.6;
        p = EngineClient::buildPayload (c, "wav", true);
        check (p["task_type"].toString() == "cover" && std::abs ((double) p["audio_cover_strength"] - 0.6) < 1.0e-6 && ! (bool) p["thinking"], "a remake");
        c.source = SongRequest::Source::repaint; c.repaintStart = 10; c.repaintEnd = 25;
        p = EngineClient::buildPayload (c, "wav", true);
        check (p["task_type"].toString() == "repaint" && (double) p["repainting_start"] == 10.0 && (double) p["repainting_end"] == 25.0 && p["chunk_mask_mode"].toString() == "explicit", "redoing a part");
        c.source = SongRequest::Source::reference;
        check (EngineClient::buildPayload (c, "wav", true)["task_type"].toString() == "text2music", "borrowing a sound is still a new song");

        check (EngineClient::needsWriter (r) && ! EngineClient::needsWriter (SongRequest::fromVar (juce::JSON::parse ("{\"mode\":\"custom\"}"))), "knows which songs lean on the writer");
        check (EngineClient::isWriterMissing ("5Hz LM init failed: LLM not initialized at startup.") && ! EngineClient::isWriterMissing ("CUDA out of memory"), "tells a missing writer from any other failure");
    }

    std::cout << "What is sent to the online service" << std::endl;
    {
        SongRequest r;
        r.description = "A dark drill track about coming up";
        r.variations = 2;
        auto p = OnlineClient::buildPayload (r, "acemusic/acestep-v15-turbo", "wav");
        check (p["messages"][0]["content"].toString() == r.description && (bool) p["sample_mode"] && (bool) p["stream"] && (int) p["batch_size"] == 2
               && p["model"].toString() == "acemusic/acestep-v15-turbo" && p["audio_config"]["format"].toString() == "wav", "a described song asks the service to write it");
        check (! p["audio_config"].hasProperty ("bpm") && ! p["audio_config"].hasProperty ("duration") && ! p.hasProperty ("seed"), "tempo, length and seed are left to the service when they are on Auto");

        SongRequest c;
        c.mode = SongRequest::Mode::custom;
        c.style = "dark trap, 808s";
        c.lyrics = "[Verse]\nHello";
        c.bpm = 140; c.key = "C minor"; c.timeSignature = "4"; c.seconds = 120; c.randomSeed = false; c.seed = 77; c.language = "es";
        p = OnlineClient::buildPayload (c, "m", "flac");
        check (p["messages"][0]["content"].toString() == "<prompt>dark trap, 808s</prompt><lyrics>[Verse]\nHello</lyrics>" && ! (bool) p["sample_mode"], "style and lyrics go in their own tags");
        check ((int) p["audio_config"]["bpm"] == 140 && p["audio_config"]["key_scale"].toString() == "C minor" && p["audio_config"]["time_signature"].toString() == "4/4"
               && (double) p["audio_config"]["duration"] == 120.0 && p["audio_config"]["vocal_language"].toString() == "es" && p["seed"].toString() == "77", "tempo, key, time, length, language and seed");
        c.instrumental = true;
        p = OnlineClient::buildPayload (c, "m", "flac");
        check (p["messages"][0]["content"].toString() == "<prompt>dark trap, 808s</prompt>" && (bool) p["audio_config"]["instrumental"], "an instrumental sends no lyrics and says so");

        c.source = SongRequest::Source::cover; c.strength = 0.6;
        p = OnlineClient::buildPayload (c, "m", "flac", "QUJD", "flac");
        check (p["task_type"].toString() == "cover" && p["messages"][0]["content"][1]["type"].toString() == "input_audio"
               && p["messages"][0]["content"][1]["input_audio"]["data"].toString() == "QUJD" && p["messages"][0]["content"][1]["input_audio"]["format"].toString() == "flac"
               && p["messages"][0]["content"][0]["text"].toString().startsWith ("<prompt>"), "a remake carries the audio with the words");
        c.source = SongRequest::Source::repaint; c.repaintStart = 5; c.repaintEnd = -1;
        p = OnlineClient::buildPayload (c, "m", "flac", "QUJD", "flac");
        check (p["task_type"].toString() == "repaint" && (double) p["repainting_start"] == 5.0 && ! p.hasProperty ("repainting_end"), "redoing a part up to the end");

        OnlineResult details;
        OnlineClient::parseDetails ("\n\n## Metadata\n**Caption:** Upbeat pop song\n**BPM:** 120\n**Duration:** 30s\n**Key:** C major\n**Time Signature:** 4\n\n## Lyrics\n[Verse 1]\nHello world\n\n[Chorus]\nLa la", details);
        check (details.caption == "Upbeat pop song" && details.bpm == 120.0 && details.seconds == 30.0 && details.key == "C major" && details.timeSignature == "4"
               && details.lyrics == "[Verse 1]\nHello world\n\n[Chorus]\nLa la", "reads what the service says it made");

        const auto folder = scratch.getChildFile ("online-parse");
        folder.createDirectory();
        juce::MemoryBlock fake;
        fake.append ("RIFF", 4);
        for (int i = 0; i < 40; ++i) fake.append ("wave", 4);
        const auto encoded = juce::Base64::toBase64 (fake.getData(), fake.getSize());
        OnlineResult result;
        juce::String error;
        const juce::String whole = "{\"id\":\"x\",\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":\"## Metadata\\n**BPM:** 90\",\"audio\":[{\"type\":\"audio_url\",\"audio_url\":{\"url\":\"data:audio/mpeg;base64,"
                                   + encoded + "\"}}]},\"finish_reason\":\"stop\"}]}";
        check (OnlineClient::parseAnswer (whole, folder, "one", result, error) && result.files.size() == 1 && result.files[0].getFileName() == "one-1.wav"
               && result.files[0].getSize() == (juce::int64) fake.getSize() && result.bpm == 90.0, "an answer in one piece: the audio is saved, named for what it really is");
        OnlineResult streamed;
        const juce::String stream = "data: {\"choices\":[{\"index\":0,\"delta\":{\"role\":\"assistant\",\"content\":\"\"},\"finish_reason\":null}]}\n\n"
                                    "data: {\"choices\":[{\"index\":0,\"delta\":{\"content\":\"## Lyrics\\nLine one\"},\"finish_reason\":null}]}\n\n"
                                    "data: {\"choices\":[{\"index\":0,\"delta\":{\"content\":\".\"},\"finish_reason\":null}]}\n\n"
                                    "data: {\"choices\":[{\"index\":0,\"delta\":{\"audio\":[{\"type\":\"audio_url\",\"audio_url\":{\"url\":\"data:audio/wav;base64," + encoded
                                    + "\"}},{\"type\":\"audio_url\",\"audio_url\":{\"url\":\"data:audio/wav;base64," + encoded + "\"}}]},\"finish_reason\":null}]}\n\n"
                                    "data: {\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"stop\"}]}\n\ndata: [DONE]\n\n";
        check (OnlineClient::parseAnswer (stream, folder, "two", streamed, error) && streamed.files.size() == 2 && streamed.lyrics == "Line one", "an answer that streams in: both versions saved, and the waiting dots are not mistaken for lyrics");
        OnlineResult bad;
        check (! OnlineClient::parseAnswer ("data: {\"choices\":[{\"index\":0,\"delta\":{\"content\":\"Out of capacity\"},\"finish_reason\":\"error\"}]}\n\n", folder, "three", bad, error)
               && error == "Out of capacity" && bad.files.isEmpty(), "a failure carries the service's own reason");
        check (! OnlineClient::parseAnswer ("{\"detail\":\"Invalid API key\"}", folder, "four", bad, error) && error == "Invalid API key", "so does a refusal");
        OnlineResult linked;
        check (OnlineClient::parseAnswer ("{\"choices\":[{\"index\":0,\"message\":{\"content\":\"ok\",\"audio\":[{\"type\":\"audio_url\",\"audio_url\":{\"url\":\"https://example.com/a.mp3\"}}]},\"finish_reason\":\"stop\"}]}", folder, "six", linked, error)
               && linked.files.isEmpty() && linked.links.size() == 1 && linked.links[0] == "https://example.com/a.mp3", "audio given as a link instead is noted, to be fetched");
        check (! OnlineClient::parseAnswer ("data: {\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"stop\"}]}\n\n", folder, "five", bad, error) && error.isNotEmpty(), "an answer with no audio in it counts as a failure");

        juce::MemoryBlock mp3 ("ID3\x04", 4), flac ("fLaC", 4), unknown ("\x01\x02\x03\x04", 4);
        check (OnlineClient::extensionFor (mp3, "audio/wav") == ".mp3" && OnlineClient::extensionFor (flac, "") == ".flac"
               && OnlineClient::extensionFor (unknown, "audio/flac") == ".flac" && OnlineClient::extensionFor (unknown, "") == ".mp3", "the file type comes from the audio itself, then from what the service called it");

        const auto take = makeTone (scratch.getChildFile ("to-send.wav"), 3.0);
        juce::String format;
        const auto packed = OnlineClient::packForSending (take, format);
        const auto packedFile = scratch.getChildFile ("packed.flac");
        packedFile.replaceWithData (packed.getData(), packed.getSize());
        check (format == "flac" && packed.getSize() > 1000 && (juce::int64) packed.getSize() < take.getSize() / 2 && std::abs (audioLengthSeconds (packedFile) - 3.0) < 0.01,
               "audio to start from is packed small before it is sent, with nothing lost (" + juce::String (take.getSize() / 1024) + " KB to " + juce::String ((int) packed.getSize() / 1024) + " KB)");
        const auto back = OnlineClient::unpackToWav (packedFile);
        check (back.getFileName() == "packed.wav" && ! packedFile.existsAsFile() && std::abs (audioLengthSeconds (back) - 3.0) < 0.01 && back.getSize() > take.getSize() - 4096,
               "and a FLAC that comes back is turned into a WAV of the same length");
    }

    std::cout << "What comes back from the engine" << std::endl;
    {
        TaskUpdate u;
        const juce::String running = R"({"data":[{"task_id":"abc","result":"[{\"file\": \"\", \"wave\": \"\", \"status\": 0, \"create_time\": 1700000000, \"env\": \"development\", \"prompt\": \"\", \"lyrics\": \"\", \"metas\": {}, \"progress\": 0.42, \"stage\": \"running\", \"error\": null}]","status":0,"progress_text":"Generating..."}],"code":200,"error":null,"timestamp":1,"extra":null})";
        check (EngineClient::parseTaskUpdate (running, "abc", u) && u.status == 0 && std::abs (u.progress - 0.42) < 1.0e-6 && u.stage == "running" && u.items.isEmpty(), "a song that is still being made");

        const juce::String done = R"({"data":[{"task_id":"abc","result":"[{\"file\": \"/v1/audio?path=%2Ftmp%2Fa.wav\", \"status\": 1, \"prompt\": \"dark trap\", \"lyrics\": \"[Verse]\\nHi\", \"metas\": {\"bpm\": 140, \"duration\": 121.5, \"genres\": \"\", \"keyscale\": \"C minor\", \"timesignature\": \"4\"}, \"seed_value\": \"11,22\", \"dit_model\": \"acestep-v15-turbo\", \"progress\": 1.0, \"stage\": \"succeeded\"}, {\"file\": \"/v1/audio?path=%2Ftmp%2Fb.wav\", \"status\": 1, \"prompt\": \"dark trap\", \"lyrics\": \"\", \"metas\": {\"bpm\": \"N/A\", \"duration\": \"N/A\", \"keyscale\": \"N/A\", \"timesignature\": \"N/A\"}, \"seed_value\": \"11,22\"}]","status":1}],"code":200})";
        check (EngineClient::parseTaskUpdate (done, "abc", u) && u.status == 1 && u.items.size() == 2, "a finished song with two versions");
        check (u.items[0].fileUrl == "/v1/audio?path=%2Ftmp%2Fa.wav" && u.items[0].bpm == 140.0 && u.items[0].seconds == 121.5 && u.items[0].key == "C minor"
               && u.items[0].seed == "11" && u.items[1].seed == "22" && u.items[0].lyrics == "[Verse]\nHi", "its details and each version's own seed");
        check (u.items[1].bpm == 0.0 && u.items[1].key.isEmpty(), "\"N/A\" from the engine is treated as not known");

        const juce::String failed = R"({"data":[{"task_id":"abc","result":"[{\"file\": \"\", \"status\": 2, \"progress\": 0.0, \"stage\": \"failed\", \"error\": \"CUDA out of memory\"}]","status":2,"progress_text":"x"}]})";
        check (EngineClient::parseTaskUpdate (failed, "abc", u) && u.status == 2 && u.error == "CUDA out of memory", "a failed song carries the engine's reason");
        check (! EngineClient::parseTaskUpdate (done, "other", u) && ! EngineClient::parseTaskUpdate ("not json", "abc", u), "an answer about some other song, or nonsense, is not accepted");
        const juce::String empty = R"({"data":[{"task_id":"abc","result":"[]","status":1}]})";
        check (EngineClient::parseTaskUpdate (empty, "abc", u) && u.status == 2, "\"finished\" with no audio counts as a failure");

        auto h = EngineClient::parseHealth (R"({"data":{"status":"ok","service":"ACE-Step API","models_initialized":false,"llm_initialized":true,"loaded_model":"acestep-v15-turbo"},"code":200})");
        check (h.reachable && ! h.modelsReady && h.languageModelReady && h.model == "acestep-v15-turbo", "reads the engine's health");
        check (! EngineClient::parseHealth ("<html>router login</html>").reachable, "something else answering at that address is not taken for the engine");
    }

    std::cout << "The song form" << std::endl;
    {
        SongRequest r;
        check (r.problem().isNotEmpty(), "an empty form cannot be sent");
        r.description = "smooth late night r&b, about missing someone";
        check (r.problem().isEmpty() && r.suggestedTitle() == "Smooth late night r&b", "a description is enough, and gives the song a name");
        {
            SongRequest t;
            t.description = "A dark UK drill track with sliding 808s and a haunting piano, about coming up from nothing";
            check (t.suggestedTitle() == "Dark UK drill track", "the name is the opening of the description, not a sentence cut off half way (" + t.suggestedTitle() + ")");
            t.description = "lo-fi";
            check (t.suggestedTitle() == "Lo-fi", "a one word description still names the song");
        }
        r.mode = SongRequest::Mode::custom;
        check (r.problem().isNotEmpty(), "writing it yourself needs a style");
        r.style = "r&b";
        check (r.problem().isNotEmpty(), "and lyrics, unless it is an instrumental");
        r.lyrics = "[Verse]\nTwo in the morning and I am thinking of you\nMore";
        check (r.problem().isEmpty() && r.suggestedTitle() == "Two in the morning and", "named after the first line that is sung");
        r.title = "  Late Night  ";
        check (r.suggestedTitle() == "Late Night", "a title that was typed wins");
        r.source = SongRequest::Source::cover;
        check (r.problem().isNotEmpty(), "starting from audio needs the audio");
        r.audioPath = makeTone (scratch.getChildFile ("source.wav"), 1.0).getFullPathName();
        check (r.problem().isEmpty(), "and is fine once it has it");
        r.source = SongRequest::Source::repaint; r.repaintStart = 20; r.repaintEnd = 10;
        check (r.problem().isNotEmpty(), "a part to redo cannot end before it starts");

        r.repaintEnd = 30; r.bpm = 97; r.key = "Eb major"; r.seed = 9007199254740993LL; r.randomSeed = false; r.variations = 4; r.thinking = true;
        const auto back = SongRequest::fromVar (juce::JSON::parse (juce::JSON::toString (r.toVar())));
        check (back.mode == r.mode && back.title == r.title && back.style == r.style && back.lyrics == r.lyrics && back.bpm == 97 && back.key == "Eb major"
               && back.seed == r.seed && ! back.randomSeed && back.variations == 4 && back.thinking && back.source == r.source
               && back.audioPath == r.audioPath && back.repaintStart == 20.0 && back.repaintEnd == 30.0, "the form survives being saved with the project and loaded again");
    }

    std::cout << "The library" << std::endl;
    {
        const auto folder = scratch.getChildFile ("lib");
        juce::String firstId;
        {
            Library lib (folder);
            Song a;
            a.title = "Cold Outside"; a.style = "dark uk drill"; a.lyrics = "block stay warm"; a.bpm = 142; a.key = "F minor"; a.seconds = 164;
            const auto savedA = lib.add (a, makeTone (scratch.getChildFile ("in1.wav"), 0.5));
            Song b;
            b.title = "Cold Outside"; b.style = "smooth r&b";
            const auto savedB = lib.add (b, makeTone (scratch.getChildFile ("in2.wav"), 0.5));
            Song c;
            c.title = "What/If: \"Maybe\"?";
            const auto savedC = lib.add (c, makeTone (scratch.getChildFile ("in3.wav"), 0.5));
            firstId = savedA.id;

            check (lib.count() == 3 && lib.songs()[0].id == savedC.id, "songs are kept, newest first");
            check (savedA.fileName == "Cold Outside.wav" && savedB.fileName == "Cold Outside 2.wav" && lib.audioOf (savedA).existsAsFile() && lib.audioOf (savedB).existsAsFile(),
                   "each file is named after its song, and two with the same name do not overwrite each other");
            check (savedC.fileName == "WhatIf Maybe.wav" && ! scratch.getChildFile ("in3.wav").existsAsFile(), "characters a file name cannot have are dropped, and the audio is moved in");
            check (lib.search ("drill", false).size() == 1 && lib.search ("COLD", false).size() == 2 && lib.search ("warm block", false).size() == 1 && lib.search ("jazz", false).isEmpty(),
                   "search looks through names, styles and lyrics");
            lib.setLiked (savedB.id, true);
            check (lib.search ({}, true).size() == 1 && lib.search ({}, true)[0].id == savedB.id, "liked songs can be listed on their own");
            lib.rename (savedA.id, "Colder");
            Song found;
            check (lib.find (savedA.id, found) && found.title == "Colder" && found.fileName == "Cold Outside.wav", "renaming changes the name but leaves the file where projects expect it");
            lib.remove (savedC.id);
            check (lib.count() == 2 && ! folder.getChildFile ("Songs/WhatIf Maybe.wav").existsAsFile(), "deleting removes the song and its file");
            check (savedA.detailText().contains ("142 BPM") && savedA.detailText().contains ("F minor") && savedA.lengthText() == "2:44", "tempo, key and length read properly");
        }
        Library again (folder);
        Song found;
        check (again.count() == 2 && again.find (firstId, found) && found.title == "Colder" && found.bpm == 142.0, "the library is still there after closing and reopening");
        again.audioOf (found).deleteFile();
        Library third (folder);
        check (third.count() == 1, "a song whose file was deleted by hand is dropped from the list");
    }

    std::cout << "Catching audio from the track" << std::endl;
    {
        juce::File caught;
        double caughtSeconds = 0.0;
        Recorder recorder (scratch.getChildFile ("takes"));
        recorder.onFinished = [&] (juce::File f, double s) { caught = f; caughtSeconds = s; };
        recorder.prepare (44100.0, 2);
        juce::AudioBuffer<float> buffer (2, 512);
        for (int i = 0; i < 512; ++i) { buffer.setSample (0, i, 0.3f * std::sin ((float) i * 0.1f)); buffer.setSample (1, i, 0.3f * std::sin ((float) i * 0.1f)); }

        recorder.arm();
        for (int b = 0; b < 50; ++b) recorder.process (buffer, false, true);
        check (recorder.state() == Recorder::State::armed && recorder.seconds() == 0.0, "armed waits for the project to play");
        const int blocks = 431;         // about five seconds
        for (int b = 0; b < blocks; ++b) recorder.process (buffer, true, true, true);
        check (recorder.state() == Recorder::State::recording, "it starts with the project's play button");
        recorder.process (buffer, false, true);
        pump (400);
        check (recorder.state() == Recorder::State::idle && caught.existsAsFile(), "it stops by itself when the project stops, and hands over a file");
        check (std::abs (caughtSeconds - (double) (blocks * 512) / 44100.0) < 0.02, "the take is as long as what was played (" + juce::String (caughtSeconds, 2) + " s)");
        check (std::abs (audioLengthSeconds (caught) - caughtSeconds) < 0.02, "and so is the file");

        caught = juce::File();
        recorder.arm();
        recorder.stop();
        pump (200);
        check (recorder.state() == Recorder::State::idle && ! caught.existsAsFile() && scratch.getChildFile ("takes").getNumberOfChildFiles (juce::File::findFiles) == 1,
               "stopping before anything played leaves nothing behind");

        recorder.arm();
        for (int b = 0; b < 20; ++b) recorder.process (buffer, true, true, true);
        recorder.process (buffer, false, true);
        pump (400);
        check (! caught.existsAsFile(), "a take of a fraction of a second is thrown away");

        recorder.arm();
        for (int b = 0; b < 200; ++b) recorder.process (buffer, false, false, true);
        check (recorder.state() == Recorder::State::recording, "in a host with no play button it starts straight away");
        recorder.stop();
        pump (200);
        check (caught.existsAsFile(), "and Stop keeps the take");

        caught = juce::File();
        recorder.arm();
        recorder.startNow();
        for (int b = 0; b < 200; ++b) recorder.process (buffer, false, true, true);
        check (recorder.state() == Recorder::State::recording, "Start now does not wait for play");
        recorder.stop();
        pump (200);
        check (caught.existsAsFile(), "and that take is kept too");
    }

    std::cout << "Playing a song through the plugin" << std::endl;
    {
        Player player;
        const auto tone = makeTone (scratch.getChildFile ("play.wav"), 2.0, 48000.0);
        player.prepare (44100.0, 512);
        check (player.load (tone, "song1") && std::abs (player.length() - 2.0) < 0.01 && player.songId() == "song1", "loads a song and knows how long it is");
        juce::AudioBuffer<float> out (2, 512);
        out.clear();
        player.process (out);
        check (out.getMagnitude (0, 512) == 0.0f, "silent until play is pressed");
        player.play();
        float loudest = 0.0f;
        for (int b = 0; b < 60; ++b)
        {
            out.clear();
            juce::Thread::sleep (2);       // the reading happens on a background thread
            player.process (out);
            loudest = juce::jmax (loudest, out.getMagnitude (0, 512));
        }
        check (loudest > 0.1f && loudest < 0.6f, "the song comes out of the plugin (" + juce::String (loudest, 2) + ")");
        check (player.position() > 0.3 && player.position() < 1.0, "the position moves with it, at the project's sample rate (" + juce::String (player.position(), 2) + " s)");

        for (int i = 0; i < 512; ++i) { out.setSample (0, i, 0.25f); out.setSample (1, i, 0.25f); }
        player.pause();
        player.process (out);
        check (out.getSample (0, 100) == 0.25f, "the track's own sound passes through untouched");
        player.setPosition (1.5);
        check (std::abs (player.position() - 1.5) < 0.05, "it can be moved to any point");
        check (! player.load (scratch.getChildFile ("missing.wav"), "x") && player.songId() == "song1", "a missing file is refused and the current song stays");

        MakerProcessor processor;
        processor.prepareToPlay (48000.0, 256);
        juce::AudioBuffer<float> block (2, 256);
        juce::MidiBuffer midi;
        for (int i = 0; i < 256; ++i) { block.setSample (0, i, 0.5f); block.setSample (1, i, -0.5f); }
        processor.processBlock (block, midi);
        check (block.getSample (0, 10) == 0.5f && block.getSample (1, 10) == -0.5f && processor.getTailLengthSeconds() == 0.0, "the plugin itself changes nothing when no song is playing");

        processor.draft.description = "kept with the project";
        processor.draft.variations = 3;
        processor.followProjectTempo = false;
        juce::MemoryBlock state;
        processor.getStateInformation (state);
        MakerProcessor other;
        other.setStateInformation (state.getData(), (int) state.getSize());
        check (other.draft.description == "kept with the project" && other.draft.variations == 3 && ! other.followProjectTempo, "what was on the Create page comes back with the project");
    }

    std::cout << "The engine's log" << std::endl;
    {
        check (EngineRunner::tidyLogLine ("\x1b[32mINFO\x1b[0m:     Uvicorn running on http://127.0.0.1:8001  ") == "INFO:     Uvicorn running on http://127.0.0.1:8001", "colour codes are stripped");
        check (EngineRunner::tidyLogLine ("Downloading:  10%\rDownloading:  55%") == "Downloading:  55%", "a progress bar shows only where it has got to");
        Settings s;
        EngineRunner runner (s);
        check (runner.usesThisComputer() && runner.port() == 8001 && ! runner.installed(), "by default the engine is expected on this computer, and is not set up yet");
        juce::String error;
        check (! runner.start (error) && error.isNotEmpty(), "starting an engine that is not set up says so instead of doing nothing");
        s.set ("engineUrl", "http://192.168.1.20:9000");
        check (! runner.usesThisComputer() && runner.port() == 9000, "an address on the network is recognised as another computer");
        s.remove ("engineUrl");
    }

    std::cout << "Talking to the engine" << std::endl;
    {
        const int port = 21000 + juce::Random::getSystemRandom().nextInt (20000);
        const juce::String base = "http://127.0.0.1:" + juce::String (port);
        const auto script = juce::File (MUM_TESTS_DIR).getChildFile ("mock_engine.py");
        juce::ChildProcess mock;
        bool up = false;
        if (script.existsAsFile())
            for (auto* python : { "python3", "python" })
            {
                if (! mock.start (juce::StringArray { python, script.getFullPathName(), "--port", juce::String (port) }, 0)) continue;
                for (int i = 0; i < 60 && ! up; ++i)
                {
                    juce::Thread::sleep (100);
                    up = EngineClient (base, {}).health (500).reachable;
                    if (! mock.isRunning()) break;
                }
                if (up) break;
                mock.kill();
            }

        {
            const EngineClient nobody ("http://127.0.0.1:" + juce::String (port + 1), {});
            const auto h = nobody.health (800);
            check (! h.reachable && h.error.isNotEmpty(), "an address with nothing behind it is reported, not waited on forever");
        }

        if (! up)
        {
            std::cout << "  skipped: Python is not on this machine, so the stand-in engine could not be started" << std::endl;
        }
        else
        {
            const EngineClient client (base, {});
            const auto h = client.health();
            check (h.reachable && h.modelsReady && h.languageModelReady && h.model == "acestep-v15-turbo", "finds the engine and reads its state");
            juce::String error;
            const auto models = client.models (error);
            check (models.size() == 2 && models[0].name == "acestep-v15-turbo" && models[0].isDefault && ! models[1].isLoaded, "lists the engine's models");

            HttpRequest chunked;
            chunked.url = base + "/mock/chunked";
            const auto big = httpSend (chunked);
            check (big.ok() && big.body.getSize() == 50000 && big.text().startsWith ("0123456789") && big.text().endsWith ("0123456789"), "an answer sent in chunks is put back together");

            // one song all the way through: send, wait, bring the audio in
            SongRequest request;
            request.mode = SongRequest::Mode::custom;
            request.style = "dark trap, 808s";
            request.lyrics = "[Verse]\nHello";
            request.bpm = 140;
            request.variations = 2;
            juce::String taskId;
            check (client.submit (request, "wav", true, taskId, error) && taskId.isNotEmpty(), "the engine accepts a song");
            auto last = mockLast (base);
            check (last["content_type"].toString() == "application/json" && last["fields"]["prompt"].toString() == "dark trap, 808s" && (int) last["fields"]["bpm"] == 140, "it arrived as it was sent");

            TaskUpdate update;
            check (client.query (taskId, update, error) && update.status == 0, "it is still being made a moment later");
            for (int i = 0; i < 50 && update.status == 0; ++i) { juce::Thread::sleep (100); client.query (taskId, update, error); }
            check (update.status == 1 && update.items.size() == 2 && update.items[0].bpm == 140.0 && update.items[1].seed == "1001", "then it finishes with both versions");
            const auto target = scratch.getChildFile ("downloaded.wav");
            double furthest = 0.0;
            check (client.download (update.items[0].fileUrl, target, error, {}, [&] (double p) { furthest = juce::jmax (furthest, p); }) && std::abs (audioLengthSeconds (target) - 1.5) < 0.01,
                   "the audio downloads whole and plays");
            check (furthest > 0.99, "and reports how far it has got on the way");

            mockConfig (base, "{\"chunked\": true}");
            const auto target2 = scratch.getChildFile ("downloaded2.wav");
            check (client.download (update.items[1].fileUrl, target2, error) && target2.getSize() == target.getSize(), "the same when the engine sends it in chunks");
            mockConfig (base, "{\"chunked\": false}");
            const auto missing = scratch.getChildFile ("missing-download.wav");
            check (! client.download ("/v1/audio?path=%2Fetc%2Fpasswd", missing, error) && ! missing.exists() && error.containsIgnoreCase ("denied"), "a refused download leaves no file and gives the engine's reason");

            // audio travels with the request, so the engine can be on another computer
            request.source = SongRequest::Source::cover;
            request.audioPath = makeTone (scratch.getChildFile ("My Loop.wav"), 1.0).getFullPathName();
            const auto sourceSize = juce::File (request.audioPath).getSize();
            check (client.submit (request, "wav", true, taskId, error), "a song that starts from audio is accepted");
            last = mockLast (base);
            check (last["content_type"].toString() == "multipart/form-data" && (juce::int64) last["files"]["src_audio"]["bytes"] == sourceSize
                   && last["files"]["src_audio"]["filename"].toString() == "My Loop.wav" && last["fields"]["task_type"].toString() == "cover"
                   && last["fields"]["lyrics"].toString() == "[Verse]\nHello" && last["fields"]["use_random_seed"].toString() == "true", "the audio and every setting arrive with it");
            request.source = SongRequest::Source::reference;
            client.submit (request, "wav", true, taskId, error);
            last = mockLast (base);
            check ((juce::int64) last["files"]["reference_audio"]["bytes"] == sourceSize && last["fields"]["task_type"].toString() == "text2music", "audio to borrow a sound from goes in its own slot");

            SongRequest rough;
            rough.mode = SongRequest::Mode::custom;
            rough.style = "sad piano";
            check (client.enhance (rough, error) && rough.style.contains ("sad piano") && rough.lyrics.contains ("[Chorus]") && rough.bpm == 142 && rough.key == "F minor", "the engine can write a rough idea out in full");

            mockConfig (base, "{\"api_key\": \"secret\"}");
            check (! client.models (error).size() && EngineClient (base, "secret").models (error).size() == 2, "an engine with a key lets in only those who have it");
            mockConfig (base, "{\"api_key\": \"\"}");

            // the queue, exactly as the plugin uses it
            Settings settings;
            settings.set ("engineMode", "local");
            settings.set ("engineUrl", base);
            {
                Library library (scratch.getChildFile ("queue-lib"));
                EngineWatch watch (settings);
                Generator generator (settings, library, watch);
                generator.pollMs = 100;
                auto waitFor = [&] (const juce::String& id)
                {
                    for (int i = 0; i < 400; ++i)
                    {
                        pump (50);
                        for (auto& j : generator.jobs()) if (j.id == id && ! j.active()) return j;
                    }
                    return Job();
                };

                for (int i = 0; i < 40 && ! watch.everChecked(); ++i) pump (50);
                check (watch.status().reachable && watch.models().size() == 2, "the plugin notices the engine by itself");

                SongRequest song;
                song.description = "A dark drill track about coming up from nothing";
                song.variations = 2;
                auto job = waitFor (generator.add (song));
                check (job.state == Job::State::done && job.songIds.size() == 2, "a described song is made and both versions land in the library  " + job.message);
                auto made = library.songs();
                check (made.size() == 2 && made[0].title == "Dark drill track 2" && made[1].title == "Dark drill track 1", "they are named after the description and numbered");
                check (made.size() == 2 && library.audioOf (made[0]).existsAsFile() && std::abs (made[0].seconds - 1.5) < 0.01 && made[0].lyrics.contains ("engine wrote")
                       && made[0].bpm == 128.0 && made[0].seed == "1001" && made[0].request.isObject(), "each has its audio, its words, its tempo, its seed and how it was made");
                check (library.folder().getNumberOfChildFiles (juce::File::findFiles) == 2, "nothing half-downloaded is left lying about");

                mockConfig (base, "{\"writer\": false}");
                job = waitFor (generator.add (song));
                check (job.state == Job::State::done && job.message.contains ("without lyrics") && library.count() == 4, "when the engine's writer is off the song is still made, and the plugin says what is missing");
                last = mockLast (base);
                check (last["fields"]["prompt"].toString() == song.description && ! last["fields"].hasProperty ("sample_query"), "by sending it again without asking for the writer");
                mockConfig (base, "{\"writer\": true}");

                mockConfig (base, "{\"fail\": \"CUDA out of memory\"}");
                job = waitFor (generator.add (song));
                check (job.state == Job::State::failed && job.message == "CUDA out of memory" && library.count() == 4, "a song the engine cannot make fails with the engine's own reason");
                mockConfig (base, "{\"fail\": \"\", \"delay\": 3.0}");

                const auto slow = generator.add (song);
                const auto waiting = generator.add (song);
                pump (600);
                generator.cancel (waiting);
                generator.cancel (slow);
                job = waitFor (slow);
                check (job.state == Job::State::cancelled && library.count() == 4, "a song can be cancelled while it is being made");
                bool waitingCancelled = false;
                for (auto& j : generator.jobs()) if (j.id == waiting) waitingCancelled = j.state == Job::State::cancelled;
                check (waitingCancelled, "and one that was still waiting its turn never starts");
                generator.dismiss (slow);
                generator.dismiss (waiting);
                bool gone = true;
                for (auto& j : generator.jobs()) if (j.id == slow || j.id == waiting) gone = false;
                check (gone, "finished ones can be cleared off the list");
                mockConfig (base, "{\"delay\": 0.5}");

                settings.set ("engineUrl", "http://127.0.0.1:" + juce::String (port + 1));
                job = waitFor (generator.add (song));
                check (job.state == Job::State::failed && job.message.contains ("not running"), "with no engine the song fails at once and says to start it");
            }
            settings.remove ("engineUrl");

            // the same queue, making songs with the online service instead
            mockConfig (base, "{\"delay\": 0.3, \"fail\": \"\"}");
            settings.set ("engineMode", "online");
            settings.set ("onlineUrl", base + "/online");
            settings.set ("onlineKey", "testkey");
            {
                Library library (scratch.getChildFile ("online-lib"));
                EngineWatch watch (settings);
                Generator generator (settings, library, watch);
                auto waitFor = [&] (const juce::String& id)
                {
                    for (int i = 0; i < 400; ++i)
                    {
                        pump (50);
                        for (auto& j : generator.jobs()) if (j.id == id && ! j.active()) return j;
                    }
                    return Job();
                };
                for (int i = 0; i < 60 && ! watch.everChecked(); ++i) pump (50);
                check (watch.status().online && watch.status().reachable && watch.models().size() == 2 && watch.models()[0].name == "acemusic/acestep-v15-turbo",
                       "with a key, the plugin finds the online service and its models");

                SongRequest song;
                song.description = "A dark drill track about coming up from nothing";
                song.variations = 2;
                auto job = waitFor (generator.add (song));
                check (job.state == Job::State::done && job.songIds.size() == 2, "a described song is made online and both versions land in the library  " + job.message);
                auto made = library.songs();
                check (made.size() == 2 && made[0].title == "Dark drill track 2" && library.audioOf (made[0]).existsAsFile() && made[0].fileName.endsWith (".wav")
                       && std::abs (made[0].seconds - 1.5) < 0.01 && made[0].lyrics.contains ("Words the service wrote") && made[0].lyrics.endsWith ("hook...")
                       && made[0].bpm == 96.0 && made[0].key == "G minor" && made[0].style.startsWith ("A song the service wrote"),
                       "each has its audio, its real length, and the words, tempo and key the service chose");
                check (library.folder().getNumberOfChildFiles (juce::File::findFiles) == 2, "nothing half-finished is left lying about");
                auto sent = mockLast (base);
                check (sent["text"].toString() == song.description && (bool) sent["fields"]["sample_mode"] && (bool) sent["fields"]["stream"]
                       && sent["fields"]["model"].toString() == "acemusic/acestep-v15-turbo", "it was sent the way the service expects");

                SongRequest remake;
                remake.mode = SongRequest::Mode::custom;
                remake.style = "soulful r&b";
                remake.instrumental = true;
                remake.variations = 1;
                remake.source = SongRequest::Source::cover;
                remake.audioPath = makeTone (scratch.getChildFile ("My Loop.wav"), 2.0).getFullPathName();
                job = waitFor (generator.add (remake));
                sent = mockLast (base);
                check (job.state == Job::State::done && library.count() == 3 && sent["audio"]["format"].toString() == "flac" && sent["audio"]["magic"].toString() == "fLaC"
                       && (int) sent["audio"]["bytes"] > 1000 && sent["fields"]["task_type"].toString() == "cover", "a remake sends its audio along, packed as FLAC  " + job.message);
                check (library.songs()[0].lyrics.isEmpty() && library.songs()[0].title == "Soulful r&b", "an instrumental has no lyrics kept against it");

                mockConfig (base, "{\"fail\": \"The service is out of capacity\"}");
                job = waitFor (generator.add (song));
                check (job.state == Job::State::failed && job.message == "The service is out of capacity" && library.count() == 3, "when the service cannot make a song it says why, and nothing is added");
                mockConfig (base, "{\"fail\": \"\"}");

                settings.set ("onlineKey", "wrong");
                job = waitFor (generator.add (song));
                check (job.state == Job::State::failed && job.message.contains ("key"), "a key the service does not accept is reported as exactly that");
                juce::Array<EngineModel> none;
                const auto refused = OnlineClient (base + "/online", "wrong").check (none);
                check (! refused.reachable && refused.needsKey && OnlineClient (base + "/online", {}).check (none).needsKey, "and so is a missing one, without bothering the service");

                settings.set ("onlineKey", "testkey");
                mockConfig (base, "{\"delay\": 3.0}");
                const auto slow = generator.add (song);
                pump (800);
                generator.cancel (slow);
                job = waitFor (slow);
                check (job.state == Job::State::cancelled && library.count() == 3, "a song being made online can be cancelled");
                mockConfig (base, "{\"delay\": 0.5}");
            }
            settings.remove ("engineMode");
            settings.remove ("onlineUrl");
            settings.remove ("onlineKey");
            mock.kill();
        }
    }

    std::cout << "Updating from inside the plugin" << std::endl;
    {
        UpdateInfo info;
        const juce::String sha = juce::String::repeatedString ("ab", 32);
        check (Updater::parseFeed ("{\"build\":7,\"version\":\"0.1.7\",\"file\":\"MUMusicMaker-bundles.zip\",\"sha256\":\"" + sha + "\"}", info)
               && info.build == 7 && info.version == "0.1.7", "reads the update feed");
        check (! Updater::parseFeed ("{\"build\":7,\"file\":\"../evil.zip\",\"sha256\":\"" + sha + "\"}", info)
               && ! Updater::parseFeed ("{\"build\":7,\"file\":\"a.zip\",\"sha256\":\"short\"}", info)
               && ! Updater::parseFeed ("not json", info), "refuses a feed that points somewhere odd or has no proper checksum");

       #if ! JUCE_WINDOWS
        // a pretend release on disk: new plugin bundles, zipped, with its feed
        const auto feed = scratch.getChildFile ("feed"), make = scratch.getChildFile ("make");
        const auto oldAU = scratch.getChildFile ("installed/MU Music Maker.component"), oldVST = scratch.getChildFile ("installed/MU Music Maker.vst3");
        feed.createDirectory();
        for (auto* kind : { "Components/MU Music Maker.component", "VST3/MU Music Maker.vst3" })
        {
            make.getChildFile (kind).getChildFile ("Contents/MacOS").createDirectory();
            make.getChildFile (kind).getChildFile ("Contents/MacOS/MU Music Maker").replaceWithText ("new plugin");
            make.getChildFile (kind).getChildFile ("Contents/Info.plist").replaceWithText ("new info");
        }
        oldAU.getChildFile ("Contents/MacOS").createDirectory();
        oldAU.getChildFile ("Contents/MacOS/MU Music Maker").replaceWithText ("old plugin");
        oldAU.getChildFile ("Contents/Resources/gone.txt").create();
        oldVST.getChildFile ("Contents/MacOS").createDirectory();

        juce::ChildProcess zipper;
        zipper.start (juce::StringArray { "sh", "-c", "cd '" + make.getFullPathName() + "' && zip -qry '"
                                                        + feed.getChildFile ("MUMusicMaker-bundles.zip").getFullPathName() + "' Components VST3" });
        zipper.waitForProcessToFinish (20000);
        const auto zipSha = Updater::sha256Of (feed.getChildFile ("MUMusicMaker-bundles.zip"));
        feed.getChildFile ("latest.json").replaceWithText ("{\"build\":9,\"version\":\"0.1.9\",\"file\":\"MUMusicMaker-bundles.zip\",\"sha256\":\"" + zipSha + "\"}");

        Settings s3;
        Updater updater (s3);
        auto waitFor = [&] { for (int i = 0; i < 300; ++i) { pump (50); const auto p = updater.getState().phase;
                                 if (p != UpdateState::Phase::checking && p != UpdateState::Phase::downloading && p != UpdateState::Phase::installing) break; } };
        const auto feedUrl = "file://" + feed.getFullPathName();
        updater.configureForTesting (feedUrl, oldAU, oldVST, 9);
        updater.checkNow(); waitFor();
        check (updater.getState().phase == UpdateState::Phase::upToDate, "says it is up to date when it is");

        updater.configureForTesting (feedUrl, oldAU, oldVST, 5);
        updater.checkNow(); waitFor();
        check (updater.getState().phase == UpdateState::Phase::available && updater.getState().latest.version == "0.1.9", "finds a newer build");
        updater.installNow(); waitFor();
        check (updater.getState().phase == UpdateState::Phase::installed
               && oldAU.getChildFile ("Contents/MacOS/MU Music Maker").loadFileAsString() == "new plugin"
               && oldVST.getChildFile ("Contents/Info.plist").loadFileAsString() == "new info"
               && ! oldAU.getChildFile ("Contents/Resources/gone.txt").exists(), "installs it over the old plugin and clears out what the new one dropped  " + updater.getState().message);

        feed.getChildFile ("latest.json").replaceWithText ("{\"build\":10,\"version\":\"0.1.10\",\"file\":\"MUMusicMaker-bundles.zip\",\"sha256\":\"" + sha + "\"}");
        oldAU.getChildFile ("Contents/MacOS/MU Music Maker").replaceWithText ("keep me");
        Updater second (s3);
        auto waitSecond = [&] { for (int i = 0; i < 300; ++i) { pump (50); const auto p = second.getState().phase;
                                    if (p != UpdateState::Phase::checking && p != UpdateState::Phase::downloading && p != UpdateState::Phase::installing) break; } };
        second.configureForTesting (feedUrl, oldAU, oldVST, 5);
        second.checkNow(); waitSecond();
        second.installNow(); waitSecond();
        check (second.getState().phase == UpdateState::Phase::failed && oldAU.getChildFile ("Contents/MacOS/MU Music Maker").loadFileAsString() == "keep me",
               "a download that does not match its checksum is thrown away and nothing is touched");
       #endif
    }

    scratch.deleteRecursively();
    std::cout << (failures == 0 ? "ALL PASSED" : juce::String (failures) + " FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
