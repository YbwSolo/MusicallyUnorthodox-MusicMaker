#include "Engine.h"
#include <cstring>
#include <cmath>

namespace mm
{
juce::StringArray keyNames()
{
    juce::StringArray out;
    const char* notes[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    for (auto* n : notes) out.add (juce::String (n) + " minor");
    for (auto* n : notes) out.add (juce::String (n) + " major");
    return out;
}

//==============================================================================
juce::var SongRequest::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("mode", mode == Mode::simple ? "simple" : "custom");
    o->setProperty ("description", description);
    o->setProperty ("title", title);
    o->setProperty ("style", style);
    o->setProperty ("lyrics", lyrics);
    o->setProperty ("instrumental", instrumental);
    o->setProperty ("bpm", bpm);
    o->setProperty ("key", key);
    o->setProperty ("timeSignature", timeSignature);
    o->setProperty ("seconds", seconds);
    o->setProperty ("language", language);
    o->setProperty ("variations", variations);
    o->setProperty ("steps", steps);
    o->setProperty ("guidance", guidance);
    o->setProperty ("randomSeed", randomSeed);
    o->setProperty ("seed", juce::String (seed));
    o->setProperty ("thinking", thinking);
    o->setProperty ("enhance", enhance);
    o->setProperty ("model", model);
    const char* sources[] = { "none", "reference", "cover", "repaint" };
    o->setProperty ("source", sources[(int) source]);
    o->setProperty ("audioPath", audioPath);
    o->setProperty ("strength", strength);
    o->setProperty ("repaintStart", repaintStart);
    o->setProperty ("repaintEnd", repaintEnd);
    return juce::var (o);
}

SongRequest SongRequest::fromVar (const juce::var& v)
{
    SongRequest r;
    if (! v.isObject()) return r;
    auto text = [&] (const char* name, const juce::String& fallback = {}) { return v.hasProperty (name) ? v[name].toString() : fallback; };
    auto number = [&] (const char* name, double fallback) { return v.hasProperty (name) ? (double) v[name] : fallback; };
    auto flag = [&] (const char* name, bool fallback) { return v.hasProperty (name) ? (bool) v[name] : fallback; };

    r.mode = text ("mode") == "custom" ? Mode::custom : Mode::simple;
    r.description = text ("description");
    r.title = text ("title");
    r.style = text ("style");
    r.lyrics = text ("lyrics");
    r.instrumental = flag ("instrumental", false);
    r.bpm = juce::jlimit (0, 300, (int) number ("bpm", 0));
    r.key = text ("key");
    r.timeSignature = text ("timeSignature");
    r.seconds = juce::jlimit (0.0, 600.0, number ("seconds", 0.0));
    r.language = text ("language", "en");
    r.variations = juce::jlimit (1, 4, (int) number ("variations", 2));
    r.steps = juce::jlimit (1, 200, (int) number ("steps", 8));
    r.guidance = juce::jlimit (1.0, 15.0, number ("guidance", 7.0));
    r.randomSeed = flag ("randomSeed", true);
    r.seed = text ("seed", "0").getLargeIntValue();
    r.thinking = flag ("thinking", false);
    r.enhance = flag ("enhance", false);
    r.model = text ("model");
    const auto s = text ("source");
    r.source = s == "reference" ? Source::reference : s == "cover" ? Source::cover : s == "repaint" ? Source::repaint : Source::none;
    r.audioPath = text ("audioPath");
    r.strength = juce::jlimit (0.0, 1.0, number ("strength", 1.0));
    r.repaintStart = juce::jmax (0.0, number ("repaintStart", 0.0));
    r.repaintEnd = number ("repaintEnd", -1.0);
    return r;
}

static juce::String firstWords (const juce::String& text, int maxWords, int maxChars)
{
    auto words = juce::StringArray::fromTokens (text.replaceCharacters ("\r\n\t", "   ").trim(), " ", {});
    words.removeEmptyStrings();
    juce::String out;
    for (int i = 0; i < words.size() && i < maxWords; ++i)
    {
        if (out.length() + words[i].length() + 1 > maxChars) break;
        out << (out.isEmpty() ? "" : " ") << words[i];
    }
    out = out.trimCharactersAtEnd (".,;:!?-");
    return out.isEmpty() ? out : out.substring (0, 1).toUpperCase() + out.substring (1);
}

juce::String SongRequest::suggestedTitle() const
{
    if (title.trim().isNotEmpty()) return title.trim();

    if (mode == Mode::custom && ! instrumental)
        for (auto& line : juce::StringArray::fromLines (lyrics))
        {
            const auto t = line.trim();
            if (t.isNotEmpty() && ! t.startsWithChar ('[')) return firstWords (t, 5, 34);      // the first line that is sung
        }

    // otherwise the opening of the description, up to where it starts going into detail
    auto from = (mode == Mode::simple ? description : style).replaceCharacters ("\r\n\t", "   ").trim();
    for (auto* stop : { ",", ".", " with ", " about ", " that ", " for ", " like ", " where ", " - " })
    {
        const int at = from.indexOfIgnoreCase (stop);
        if (at > 3) from = from.substring (0, at);
    }
    for (auto* article : { "a ", "an ", "the " })
        if (from.startsWithIgnoreCase (article)) { from = from.substring ((int) std::strlen (article)); break; }
    const auto guess = firstWords (from, 6, 36);
    return guess.isNotEmpty() ? guess : juce::String ("Untitled");
}

juce::String SongRequest::problem() const
{
    if (mode == Mode::simple && description.trim().isEmpty()) return "Describe the song first.";
    if (mode == Mode::custom && style.trim().isEmpty())        return "Add a style first, like \"dark trap, 808s, piano\".";
    if (mode == Mode::custom && ! instrumental && lyrics.trim().isEmpty() && source != Source::cover && source != Source::repaint)
        return "Write some lyrics, or switch Instrumental on.";
    if (source != Source::none && ! juce::File::isAbsolutePath (audioPath)) return "Choose the audio to start from, or catch it from the track.";
    if (source != Source::none && ! juce::File (audioPath).existsAsFile()) return "The audio to start from has gone missing. Choose it again.";
    if (source == Source::repaint && repaintEnd >= 0.0 && repaintEnd <= repaintStart) return "The part to redo has to end after it starts.";
    return {};
}

//==============================================================================
EngineClient::EngineClient (juce::String baseUrl, juce::String apiKey) : base (std::move (baseUrl)), key (std::move (apiKey))
{
    while (base.endsWithChar ('/')) base = base.dropLastCharacters (1);
}

HttpRequest EngineClient::request (const juce::String& method, const juce::String& path) const
{
    HttpRequest r;
    r.method = method;
    r.url = base + path;
    if (key.isNotEmpty()) r.headers.add ("Authorization: Bearer " + key);
    return r;
}

HttpResponse EngineClient::postJson (const juce::String& path, const juce::var& body, int idleTimeoutMs) const
{
    auto r = request ("POST", path);
    r.headers.add ("Content-Type: application/json");
    const auto text = juce::JSON::toString (body, true);
    r.body.append (text.toRawUTF8(), text.getNumBytesAsUTF8());
    r.idleTimeoutMs = idleTimeoutMs;
    return httpSend (r);
}

juce::String EngineClient::absoluteUrl (const juce::String& fileUrl) const
{
    if (fileUrl.startsWithIgnoreCase ("http://") || fileUrl.startsWithIgnoreCase ("https://")) return fileUrl;
    return base + (fileUrl.startsWithChar ('/') ? "" : "/") + fileUrl;
}

EngineStatus EngineClient::parseHealth (const juce::String& body)
{
    EngineStatus s;
    const auto v = juce::JSON::parse (body);
    const auto data = v["data"];
    if (! data.isObject() || data["status"].toString() != "ok") return s;
    s.reachable = true;
    // older engines do not say whether the models are loaded; they only answer once they are
    s.modelsReady = data.hasProperty ("models_initialized") ? (bool) data["models_initialized"] : true;
    s.languageModelReady = (bool) data["llm_initialized"];
    s.model = data["loaded_model"].toString();
    s.languageModel = data["loaded_lm_model"].toString();
    return s;
}

EngineStatus EngineClient::health (int connectTimeoutMs) const
{
    auto r = request ("GET", "/health");
    r.connectTimeoutMs = connectTimeoutMs;
    r.idleTimeoutMs = 6000;
    const auto res = httpSend (r);
    if (! res.ok())
    {
        EngineStatus s;
        s.error = res.status == 0 ? res.error : (res.status == 401 ? juce::String ("The engine wants a key. Add it in Settings.") : res.errorText());
        return s;
    }
    auto s = parseHealth (res.text());
    if (! s.reachable) s.error = "Something else is answering at that address, not the engine.";
    return s;
}

juce::Array<EngineModel> EngineClient::models (juce::String& error) const
{
    juce::Array<EngineModel> out;
    auto r = request ("GET", "/v1/models");
    r.idleTimeoutMs = 8000;
    const auto res = httpSend (r);
    if (! res.ok()) { error = res.errorText(); return out; }
    const auto answer = res.json();
    if (auto* list = answer["data"]["models"].getArray())
        for (auto& m : *list)
        {
            EngineModel e;
            e.name = m["name"].toString();
            e.isDefault = (bool) m["is_default"];
            e.isLoaded = (bool) m["is_loaded"];
            if (e.name.isNotEmpty()) out.add (e);
        }
    return out;
}

bool EngineClient::needsWriter (const SongRequest& s)
{
    if (s.mode == SongRequest::Mode::simple && s.source == SongRequest::Source::none) return true;
    if (s.mode == SongRequest::Mode::custom && s.enhance) return true;
    return s.thinking && s.source != SongRequest::Source::cover && s.source != SongRequest::Source::repaint;
}

bool EngineClient::isWriterMissing (const juce::String& engineError)
{
    return engineError.containsIgnoreCase ("LM init failed") || engineError.containsIgnoreCase ("LLM not initialized")
        || engineError.containsIgnoreCase ("LLM disabled") || engineError.containsIgnoreCase ("LLM init failed");
}

juce::var EngineClient::buildPayload (const SongRequest& s, const juce::String& audioFormat, bool languageModelReady, const juce::String& backend)
{
    auto* o = new juce::DynamicObject();
    const auto lyricsToSend = s.instrumental ? juce::String ("[Instrumental]") : s.lyrics.trim();

    if (s.mode == SongRequest::Mode::simple)
    {
        // With its language model the engine writes the style, the lyrics and the tempo from the description.
        // Without one, the description goes in as the style and the engine does what it can.
        if (languageModelReady && s.source == SongRequest::Source::none)
        {
            auto query = s.description.trim();
            if (s.instrumental && ! query.containsIgnoreCase ("instrumental")) query << ", instrumental";
            o->setProperty ("sample_query", query);
        }
        else
        {
            o->setProperty ("prompt", s.description.trim());
            o->setProperty ("lyrics", s.instrumental ? juce::String ("[Instrumental]") : juce::String());
        }
    }
    else
    {
        o->setProperty ("prompt", s.style.trim());
        o->setProperty ("lyrics", lyricsToSend);
        if (s.enhance && languageModelReady) o->setProperty ("use_format", true);
    }

    if (s.bpm > 0)                          o->setProperty ("bpm", juce::jlimit (30, 300, s.bpm));
    if (s.key.trim().isNotEmpty())          o->setProperty ("key_scale", s.key.trim());
    if (s.timeSignature.trim().isNotEmpty()) o->setProperty ("time_signature", s.timeSignature.trim());
    if (s.seconds > 0.0)                    o->setProperty ("audio_duration", juce::jlimit (10.0, 600.0, s.seconds));
    o->setProperty ("vocal_language", s.language.isEmpty() ? juce::String ("en") : s.language);
    o->setProperty ("batch_size", juce::jlimit (1, 4, s.variations));
    o->setProperty ("inference_steps", juce::jlimit (1, 200, s.steps));
    o->setProperty ("guidance_scale", s.guidance);
    o->setProperty ("use_random_seed", s.randomSeed);
    if (! s.randomSeed)                     o->setProperty ("seed", juce::String (s.seed));
    o->setProperty ("thinking", s.thinking && languageModelReady && s.source != SongRequest::Source::cover && s.source != SongRequest::Source::repaint);
    if (s.model.trim().isNotEmpty())        o->setProperty ("model", s.model.trim());
    o->setProperty ("audio_format", audioFormat);
    if (backend.isNotEmpty() && languageModelReady) o->setProperty ("lm_backend", backend);

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
            o->setProperty ("repainting_end", s.repaintEnd < 0.0 ? -1.0 : s.repaintEnd);
            o->setProperty ("chunk_mask_mode", "explicit");
            break;
    }
    return juce::var (o);
}

static juce::String fieldText (const juce::var& v)
{
    if (v.isBool()) return (bool) v ? "true" : "false";
    return v.toString();
}

bool EngineClient::submit (const SongRequest& s, const juce::String& audioFormat, bool useWriter, juce::String& taskId,
                           juce::String& error, std::function<bool()> shouldCancel) const
{
    const auto payload = buildPayload (s, audioFormat, useWriter, writerBackend);
    HttpResponse res;

    if (s.source == SongRequest::Source::none)
    {
        res = postJson ("/release_task", payload);
    }
    else
    {
        // the audio travels with the request, so the engine can be on another computer
        const juce::File audio (s.audioPath);
        MultipartForm form;
        if (auto* o = payload.getDynamicObject())
            for (auto& p : o->getProperties())
                form.addField (p.name.toString(), fieldText (p.value));
        const auto field = s.source == SongRequest::Source::reference ? "reference_audio" : "src_audio";
        if (! form.addFile (field, audio, mimeTypeFor (audio))) { error = "Could not read " + audio.getFileName(); return false; }

        auto r = request ("POST", "/release_task");
        r.headers.add ("Content-Type: " + form.contentType());
        r.body = form.finish();
        r.idleTimeoutMs = 60000;
        r.shouldCancel = std::move (shouldCancel);
        res = httpSend (r);
    }

    if (! res.ok())
    {
        error = res.status == 429 ? juce::String ("The engine's queue is full. Try again in a minute.") : res.errorText();
        return false;
    }
    const auto v = res.json();
    taskId = v["data"]["task_id"].toString();
    if (taskId.isEmpty())
    {
        error = v["error"].toString().isNotEmpty() ? v["error"].toString() : juce::String ("The engine did not accept the song.");
        return false;
    }
    return true;
}

static double numberOr (const juce::var& v, double fallback)
{
    if (v.isInt() || v.isInt64() || v.isDouble()) return (double) v;
    if (v.isString() && v.toString().containsOnly ("0123456789.")) return v.toString().getDoubleValue();
    return fallback;
}

static juce::String textUnlessMissing (const juce::var& v)
{
    const auto t = v.toString().trim();
    return (t == "N/A" || v.isVoid()) ? juce::String() : t;
}

bool EngineClient::parseTaskUpdate (const juce::String& body, const juce::String& taskId, TaskUpdate& out)
{
    out = TaskUpdate();
    const auto v = juce::JSON::parse (body);
    auto* list = v["data"].getArray();
    if (list == nullptr) return false;

    for (auto& entry : *list)
    {
        if (entry["task_id"].toString() != taskId) continue;
        out.status = (int) numberOr (entry["status"], 0.0);
        out.progressText = entry["progress_text"].toString().trim();

        // the result is itself JSON, packed inside a string
        auto result = entry["result"];
        if (result.isString()) result = juce::JSON::parse (result.toString());
        const auto seeds = juce::StringArray::fromTokens (result.isArray() && result.size() > 0 ? result[0]["seed_value"].toString() : juce::String(), ",", {});

        if (auto* items = result.getArray())
        {
            int index = 0;
            for (auto& item : *items)
            {
                if (! item.isObject()) continue;
                if (item.hasProperty ("progress")) out.progress = juce::jlimit (0.0, 1.0, numberOr (item["progress"], 0.0));
                if (item["stage"].toString().isNotEmpty()) out.stage = item["stage"].toString();
                if (item["error"].toString().isNotEmpty()) out.error = item["error"].toString();

                ResultItem r;
                r.fileUrl = item["file"].toString();
                r.prompt = item["prompt"].toString();
                r.lyrics = item["lyrics"].toString();
                const auto metas = item["metas"];
                r.bpm = numberOr (metas["bpm"], 0.0);
                r.seconds = numberOr (metas["duration"], 0.0);
                r.key = textUnlessMissing (metas["keyscale"]);
                r.timeSignature = textUnlessMissing (metas["timesignature"]);
                if (r.prompt.isEmpty()) r.prompt = metas["prompt"].toString();
                if (r.lyrics.isEmpty()) r.lyrics = metas["lyrics"].toString();
                r.model = item["dit_model"].toString();
                r.seed = index < seeds.size() ? seeds[index].trim() : juce::String();
                if (r.fileUrl.isNotEmpty()) out.items.add (r);
                ++index;
            }
        }

        if (out.status == 1 && out.items.isEmpty()) { out.status = 2; if (out.error.isEmpty()) out.error = "The engine finished but made no audio."; }
        if (out.status == 2 && out.error.isEmpty())
            out.error = out.progressText.isNotEmpty() ? out.progressText : juce::String ("The engine could not make this one.");
        return true;
    }
    return false;
}

juce::String EngineClient::friendlyStage (const TaskUpdate& u)
{
    const auto stage = u.stage.toLowerCase();
    if (stage == "queued")                                   return "Waiting for the engine";
    if (stage.contains ("lm") || stage.contains ("llm") || stage.contains ("think") || stage.contains ("format")) return "Writing the song";
    if (stage.contains ("decod") || stage.contains ("vae") || stage.contains ("sav")) return "Finishing the audio";
    if (stage.contains ("load") || stage.contains ("init"))  return "Loading the model";
    return "Making the music";
}

bool EngineClient::query (const juce::String& taskId, TaskUpdate& update, juce::String& error) const
{
    auto* o = new juce::DynamicObject();
    juce::Array<juce::var> ids;
    ids.add (taskId);
    o->setProperty ("task_id_list", ids);
    const auto res = postJson ("/query_result", juce::var (o), 15000);
    if (! res.ok()) { error = res.errorText(); return false; }
    if (! parseTaskUpdate (res.text(), taskId, update)) { error = "The engine's answer made no sense."; return false; }
    return true;
}

bool EngineClient::download (const juce::String& fileUrl, const juce::File& target, juce::String& error,
                             std::function<bool()> shouldCancel, std::function<void (double)> progress) const
{
    auto r = request ("GET", "");
    r.url = absoluteUrl (fileUrl);
    r.saveTo = target;
    r.idleTimeoutMs = 60000;
    r.shouldCancel = std::move (shouldCancel);
    if (progress != nullptr)
        r.onProgress = [progress] (juce::int64 done, juce::int64 total) { progress (total > 0 ? (double) done / (double) total : 0.0); };
    const auto res = httpSend (r);
    if (! res.ok() || ! target.existsAsFile() || target.getSize() < 64)
    {
        error = res.ok() ? juce::String ("The song arrived empty.") : res.errorText();
        target.deleteFile();
        return false;
    }
    return true;
}

static void applyFilled (SongRequest& s, const juce::var& data)
{
    const auto caption = data["caption"].toString().trim();
    const auto lyrics = data["lyrics"].toString().trim();
    if (caption.isNotEmpty()) s.style = caption;
    if (lyrics.isNotEmpty() && ! s.instrumental) s.lyrics = lyrics;
    const int bpm = (int) numberOr (data["bpm"], 0.0);
    if (bpm > 0) s.bpm = bpm;
    auto key = textUnlessMissing (data["key_scale"]);
    if (key.isEmpty()) key = textUnlessMissing (data["keyscale"]);
    if (key.isNotEmpty()) s.key = key;
    auto sig = textUnlessMissing (data["time_signature"]);
    if (sig.isEmpty()) sig = textUnlessMissing (data["timesignature"]);
    if (sig.isNotEmpty()) s.timeSignature = sig;
    const double seconds = numberOr (data["duration"], 0.0);
    if (seconds > 0.0) s.seconds = seconds;
    auto language = data["vocal_language"].toString();
    if (language.isEmpty()) language = data["language"].toString();
    if (language.isNotEmpty() && language != "unknown") s.language = language;
}

bool EngineClient::enhance (SongRequest& s, juce::String& error) const
{
    auto* params = new juce::DynamicObject();
    if (s.bpm > 0) params->setProperty ("bpm", s.bpm);
    if (s.seconds > 0.0) params->setProperty ("duration", s.seconds);
    if (s.key.isNotEmpty()) params->setProperty ("key", s.key);
    if (s.timeSignature.isNotEmpty()) params->setProperty ("time_signature", s.timeSignature);
    if (s.language.isNotEmpty()) params->setProperty ("language", s.language);

    auto* o = new juce::DynamicObject();
    o->setProperty ("prompt", s.mode == SongRequest::Mode::simple ? s.description : s.style);
    o->setProperty ("lyrics", s.instrumental ? juce::String ("[Instrumental]") : s.lyrics);
    o->setProperty ("param_obj", juce::var (params));

    const auto res = postJson ("/format_input", juce::var (o), 180000);     // the language model may have to load first
    const auto v = res.json();
    if (! res.ok() || ! v["data"].isObject())
    {
        error = res.status == 503 ? juce::String ("This needs the engine's language model, which is switched off on this computer.")
                                  : (v["error"].toString().isNotEmpty() ? v["error"].toString() : res.errorText());
        return false;
    }
    applyFilled (s, v["data"]);
    return true;
}
}
