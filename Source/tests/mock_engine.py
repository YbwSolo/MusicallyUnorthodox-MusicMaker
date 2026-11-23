#!/usr/bin/env python3
"""A stand-in for the ACE-Step engine, for the tests. It answers the same addresses in the same shapes
(taken from the engine's own source), and makes a short tone instead of a song.

    python3 mock_engine.py --port 18001

POST /mock/config changes how it behaves: {"writer": false} pretends the language model is not loaded,
{"fail": "message"} makes every song fail, {"delay": 0.4} sets how long a song takes, {"chunked": true}
sends audio in chunks, {"api_key": "k"} demands a key. GET /mock/last returns the last song request it was sent.

Under /online it also stands in for ACE Music's online service (the chat-completions shape from the engine's
own documentation): GET /online/v1/models and POST /online/v1/chat/completions, which want the key "testkey".
"""
import argparse, base64, json, math, os, struct, tempfile, threading, time, urllib.parse, uuid, wave
from email.parser import BytesParser
from email.policy import HTTP
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

CONFIG = {"writer": True, "fail": "", "delay": 0.5, "chunked": False, "api_key": ""}
TASKS = {}
LAST = {}
AUDIO_DIR = tempfile.mkdtemp(prefix="mock-engine-audio-")
LOCK = threading.Lock()


def wrap(data, code=200, error=None):
    return {"data": data, "code": code, "error": error, "timestamp": int(time.time() * 1000), "extra": None}


def make_audio(seconds, fmt, pitch):
    path = os.path.join(AUDIO_DIR, uuid.uuid4().hex + "." + ("wav" if fmt not in ("wav", "flac", "mp3") else "wav"))
    rate = 48000
    with wave.open(path, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(rate)
        frames = bytearray()
        for i in range(int(seconds * rate)):
            v = int(9000 * math.sin(2 * math.pi * pitch * i / rate))
            frames += struct.pack("<hh", v, v)
        w.writeframes(bytes(frames))
    return path


def truthy(v):
    return v is True or str(v).lower() in ("true", "1", "yes")


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):
        pass

    def send_json(self, obj, status=200):
        body = json.dumps(obj).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def send_chunked(self, data, content_type):
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Transfer-Encoding", "chunked")
        self.end_headers()
        for i in range(0, len(data), 7001):
            piece = data[i:i + 7001]
            self.wfile.write(("%x\r\n" % len(piece)).encode() + piece + b"\r\n")
        self.wfile.write(b"0\r\n\r\n")

    def authorised(self):
        key = CONFIG["api_key"]
        if not key or self.headers.get("Authorization", "") == "Bearer " + key:
            return True
        self.send_json({"detail": "Unauthorized"}, 401)
        return False

    def read_body(self):
        length = int(self.headers.get("Content-Length", "0") or 0)
        return self.rfile.read(length) if length > 0 else b""

    def online_authorised(self):
        if self.headers.get("Authorization", "") == "Bearer testkey":
            return True
        self.send_json({"detail": "Invalid API key"}, 401)
        return False

    def send_events(self, events):
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Transfer-Encoding", "chunked")
        self.end_headers()
        for event in events:
            piece = ("data: " + (event if isinstance(event, str) else json.dumps(event)) + "\n\n").encode("utf-8")
            self.wfile.write(("%x\r\n" % len(piece)).encode() + piece + b"\r\n")
            self.wfile.flush()
        self.wfile.write(b"0\r\n\r\n")

    def online_song(self, fields):
        def chunk(delta, finish=None):
            return {"id": "chatcmpl-mock", "object": "chat.completion.chunk", "created": int(time.time()),
                    "model": fields.get("model", ""), "choices": [{"index": 0, "delta": delta, "finish_reason": finish}]}

        content = fields["messages"][0]["content"]
        text, sent_audio = content, None
        if isinstance(content, list):
            text = "".join(part.get("text", "") for part in content if part.get("type") == "text")
            for part in content:
                if part.get("type") == "input_audio":
                    raw = base64.b64decode(part["input_audio"]["data"])
                    sent_audio = {"bytes": len(raw), "format": part["input_audio"]["format"], "magic": raw[:4].decode("latin-1")}
                    part["input_audio"]["data"] = "(%d bytes)" % len(raw)
        with LOCK:
            LAST.clear()
            LAST.update({"fields": fields, "text": text, "audio": sent_audio})

        if CONFIG["fail"]:
            return self.send_events([chunk({"role": "assistant", "content": ""}), chunk({"content": CONFIG["fail"]}, "error"), "[DONE]"])

        config = fields.get("audio_config") or {}
        events = [chunk({"role": "assistant", "content": ""})]
        if fields.get("sample_mode") or fields.get("thinking") or fields.get("use_format"):
            lyrics = "[Instrumental]" if config.get("instrumental") else "[Verse]\nWords the service wrote\n\n[Chorus]\nAnd a hook..."
            events.append(chunk({"content": "\n\n## Metadata\n**Caption:** A song the service wrote from: %s\n**BPM:** %s\n**Duration:** 2s\n"
                                           "**Key:** %s\n**Time Signature:** 4\n\n## Lyrics\n%s" % (text[:60], config.get("bpm") or 96,
                                                                                               config.get("key_scale") or "G minor", lyrics)}))
        events += [chunk({"content": "."}), chunk({"content": "."})]
        time.sleep(CONFIG["delay"])
        songs = []
        for i in range(max(1, min(4, int(fields.get("batch_size") or 1)))):
            path = make_audio(1.5, "wav", 220.0 * (i + 1))
            data = base64.b64encode(open(path, "rb").read()).decode("ascii")
            songs.append({"type": "audio_url", "audio_url": {"url": "data:audio/wav;base64," + data}})
        events += [chunk({"audio": songs}), chunk({}, "stop"), "[DONE]"]
        self.send_events(events)

    def do_GET(self):
        url = urllib.parse.urlparse(self.path)
        if url.path == "/online/v1/models":
            if not self.online_authorised():
                return
            return self.send_json({"data": [{"id": "acemusic/acestep-v15-turbo", "object": "model"},
                                            {"id": "acemusic/acestep-v15-xl-turbo", "object": "model"}]})
        if url.path == "/health":
            return self.send_json(wrap({"status": "ok", "service": "ACE-Step API", "version": "1.0", "models_initialized": True,
                                        "llm_initialized": CONFIG["writer"], "loaded_model": "acestep-v15-turbo",
                                        "loaded_lm_model": "acestep-5Hz-lm-1.7B" if CONFIG["writer"] else None}))
        if url.path == "/mock/last":
            return self.send_json(LAST)
        if url.path == "/mock/chunked":
            return self.send_chunked(b"0123456789" * 5000, "text/plain")
        if not self.authorised():
            return
        if url.path == "/v1/models":
            return self.send_json(wrap({"models": [{"name": "acestep-v15-turbo", "is_default": True, "is_loaded": True},
                                                   {"name": "acestep-v15-base", "is_default": False, "is_loaded": False}],
                                        "default_model": "acestep-v15-turbo"}))
        if url.path == "/v1/audio":
            path = urllib.parse.parse_qs(url.query).get("path", [""])[0]
            if not os.path.abspath(path).startswith(AUDIO_DIR):
                return self.send_json({"detail": "Access denied: path outside allowed directory"}, 403)
            if not os.path.isfile(path):
                return self.send_json({"detail": "Audio file not found"}, 404)
            data = open(path, "rb").read()
            if CONFIG["chunked"]:
                return self.send_chunked(data, "audio/wav")
            self.send_response(200)
            self.send_header("Content-Type", "audio/wav")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)
            return
        self.send_json({"detail": "Not Found"}, 404)

    def do_POST(self):
        url = urllib.parse.urlparse(self.path)
        body = self.read_body()
        ctype = self.headers.get("Content-Type", "")

        if url.path == "/mock/config":
            CONFIG.update(json.loads(body or b"{}"))
            return self.send_json(CONFIG)
        if url.path == "/online/v1/chat/completions":
            if not self.online_authorised():
                return
            return self.online_song(json.loads(body or b"{}"))
        if not self.authorised():
            return

        fields, files = {}, {}
        if ctype.startswith("application/json"):
            try:
                fields = json.loads(body or b"{}")
            except ValueError:
                return self.send_json({"detail": "Malformed JSON payload"}, 400)
        elif ctype.startswith("multipart/form-data"):
            message = BytesParser(policy=HTTP).parsebytes(b"Content-Type: " + ctype.encode() + b"\r\n\r\n" + body)
            for part in message.iter_parts():
                name = part.get_param("name", header="content-disposition")
                if part.get_filename():
                    files[name] = {"filename": part.get_filename(), "bytes": len(part.get_payload(decode=True) or b""),
                                   "type": part.get_content_type()}
                else:
                    fields[name] = (part.get_payload(decode=True) or b"").decode("utf-8")

        if url.path == "/release_task":
            task_id = str(uuid.uuid4())
            with LOCK:
                LAST.clear()
                LAST.update({"fields": fields, "files": files, "content_type": ctype.split(";")[0]})
                TASKS[task_id] = {"created": time.time(), "fields": fields, "files": files}
            return self.send_json(wrap({"task_id": task_id, "status": "queued", "queue_position": 1}))

        if url.path == "/query_result":
            ids = fields.get("task_id_list", [])
            if isinstance(ids, str):
                ids = json.loads(ids)
            return self.send_json(wrap([self.result_for(i) for i in ids]))

        if url.path == "/format_input":
            if not CONFIG["writer"]:
                return self.send_json({"detail": "LLM not initialized. Set ACESTEP_INIT_LLM=true in .env to enable."}, 503)
            return self.send_json(wrap({"caption": "A full, detailed version of: " + fields.get("prompt", ""),
                                        "lyrics": "[Verse]\nWritten out by the engine\n\n[Chorus]\nAnd a hook", "bpm": 142,
                                        "key_scale": "F minor", "time_signature": "4", "duration": 150, "vocal_language": "en"}))
        self.send_json({"detail": "Not Found"}, 404)

    def result_for(self, task_id):
        task = TASKS.get(task_id)
        if task is None:
            return {"task_id": task_id, "result": "[]", "status": 0}
        fields = task["fields"]
        elapsed = time.time() - task["created"]
        created = int(task["created"])
        if elapsed < CONFIG["delay"]:
            item = {"file": "", "wave": "", "status": 0, "create_time": created, "env": "development", "prompt": "", "lyrics": "",
                    "metas": {}, "progress": round(elapsed / CONFIG["delay"], 3), "stage": "running", "error": None}
            return {"task_id": task_id, "result": json.dumps([item]), "status": 0, "progress_text": "Generating..."}

        needs_writer = bool(fields.get("sample_query")) or truthy(fields.get("thinking")) or truthy(fields.get("use_format"))
        error = CONFIG["fail"]
        if not error and needs_writer and not CONFIG["writer"]:
            error = "5Hz LM init failed: LLM not initialized at startup. To enable LLM, set ACESTEP_INIT_LLM=true in .env or environment variables."
        if error:
            item = {"file": "", "wave": "", "status": 2, "create_time": created, "env": "development", "prompt": "", "lyrics": "",
                    "metas": {}, "progress": 0.0, "stage": "failed", "error": error}
            return {"task_id": task_id, "result": json.dumps([item]), "status": 2, "progress_text": error}

        if "audio" not in task:
            count = max(1, min(8, int(fields.get("batch_size") or 2)))
            seconds = 1.5
            fmt = fields.get("audio_format", "mp3")
            task["audio"] = [make_audio(seconds, fmt, 220.0 * (i + 1)) for i in range(count)]
        written = bool(fields.get("sample_query"))
        prompt = "A song the engine wrote from: " + fields["sample_query"] if written else fields.get("prompt", "")
        lyrics = "[Verse]\nWords the engine wrote" if written and "instrumental" not in fields["sample_query"].lower() else fields.get("lyrics", "")
        metas = {"bpm": int(fields.get("bpm") or 128), "duration": float(fields.get("audio_duration") or 1.5), "genres": "",
                 "keyscale": fields.get("key_scale") or "A minor", "timesignature": fields.get("time_signature") or "4",
                 "prompt": prompt, "lyrics": lyrics}
        seeds = ",".join(str(1000 + i) for i in range(len(task["audio"])))
        items = [{"file": "/v1/audio?path=" + urllib.parse.quote(path, safe=""), "wave": "", "status": 1, "create_time": created,
                  "env": "development", "prompt": prompt, "lyrics": lyrics, "metas": metas, "generation_info": "", "seed_value": seeds,
                  "lm_model": "acestep-5Hz-lm-1.7B", "dit_model": "acestep-v15-turbo", "progress": 1.0, "stage": "succeeded"}
                 for path in task["audio"]]
        return {"task_id": task_id, "result": json.dumps(items), "status": 1, "progress_text": "Done"}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=18001)
    args = parser.parse_args()
    server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    print("mock engine on http://127.0.0.1:%d" % args.port, flush=True)
    server.serve_forever()
