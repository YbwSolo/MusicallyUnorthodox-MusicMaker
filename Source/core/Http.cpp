#include "Http.h"
#include <cstring>
#include <cmath>

namespace mm
{
juce::String HttpResponse::errorText() const
{
    if (status == 0) return error.isNotEmpty() ? error : juce::String ("No connection");
    const auto v = json();
    auto message = v["detail"].toString();
    if (message.isEmpty() && v["error"].isString()) message = v["error"].toString();
    if (message.isEmpty()) message = v["error"]["message"].toString();
    if (message.isEmpty()) message = "Error " + juce::String (status);
    return message;
}

UrlParts splitUrl (const juce::String& url)
{
    UrlParts p;
    auto rest = url.trim();
    if (rest.startsWithIgnoreCase ("https://"))     { p.https = true; p.port = 443; rest = rest.substring (8); }
    else if (rest.startsWithIgnoreCase ("http://")) { rest = rest.substring (7); }
    else return p;

    const int slash = rest.indexOfChar ('/');
    auto authority = slash >= 0 ? rest.substring (0, slash) : rest;
    p.path = slash >= 0 ? rest.substring (slash) : juce::String ("/");

    if (authority.startsWithChar ('['))                      // [::1]:8001
    {
        const int close = authority.indexOfChar (']');
        if (close < 0) return p;
        p.host = authority.substring (1, close);
        const auto after = authority.substring (close + 1);
        if (after.startsWithChar (':')) p.port = after.substring (1).getIntValue();
    }
    else
    {
        const int colon = authority.lastIndexOfChar (':');
        p.host = colon >= 0 ? authority.substring (0, colon) : authority;
        if (colon >= 0) p.port = authority.substring (colon + 1).getIntValue();
    }

    p.valid = p.host.isNotEmpty() && p.port > 0 && p.port < 65536;
    return p;
}

juce::String mimeTypeFor (const juce::File& f)
{
    const auto ext = f.getFileExtension().toLowerCase();
    if (ext == ".wav")  return "audio/wav";
    if (ext == ".flac") return "audio/flac";
    if (ext == ".mp3")  return "audio/mpeg";
    if (ext == ".ogg")  return "audio/ogg";
    if (ext == ".m4a" || ext == ".aac") return "audio/mp4";
    if (ext == ".aif" || ext == ".aiff") return "audio/aiff";
    return "application/octet-stream";
}

//==============================================================================
MultipartForm::MultipartForm()
{
    boundary = "----MUMusicMaker" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64());
}

void MultipartForm::addField (const juce::String& name, const juce::String& value)
{
    out << "--" << boundary << "\r\n"
        << "Content-Disposition: form-data; name=\"" << name << "\"\r\n\r\n"
        << value << "\r\n";
}

bool MultipartForm::addFile (const juce::String& name, const juce::File& file, const juce::String& mimeType)
{
    juce::MemoryBlock data;
    if (! file.loadFileAsData (data)) return false;
    const auto safeName = file.getFileName().retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._- ");
    out << "--" << boundary << "\r\n"
        << "Content-Disposition: form-data; name=\"" << name << "\"; filename=\"" << (safeName.isEmpty() ? juce::String ("audio") + file.getFileExtension() : safeName) << "\"\r\n"
        << "Content-Type: " << mimeType << "\r\n\r\n";
    out.write (data.getData(), data.getSize());
    out << "\r\n";
    return true;
}

juce::MemoryBlock MultipartForm::finish()
{
    out << "--" << boundary << "--\r\n";
    return out.getMemoryBlock();
}

//==============================================================================
namespace
{
struct Deadline
{
    explicit Deadline (int ms) : limit (ms) { touch(); }
    void touch()            { last = juce::Time::getMillisecondCounter(); }
    bool passed() const     { return (int) (juce::Time::getMillisecondCounter() - last) > limit; }
    int limit;
    juce::uint32 last = 0;
};

bool cancelled (const HttpRequest& r) { return r.shouldCancel != nullptr && r.shouldCancel(); }

bool sendAll (juce::StreamingSocket& socket, const void* data, size_t size, const HttpRequest& r, juce::String& error)
{
    auto* p = static_cast<const char*> (data);
    Deadline idle (r.idleTimeoutMs);
    while (size > 0)
    {
        if (cancelled (r)) { error = "Cancelled"; return false; }
        const int ready = socket.waitUntilReady (false, 250);
        if (ready < 0) { error = "The connection dropped"; return false; }
        if (ready == 0) { if (idle.passed()) { error = "The engine stopped answering"; return false; } continue; }
        const int n = socket.write (p, (int) juce::jmin (size, (size_t) 65536));
        if (n <= 0) { error = "The connection dropped"; return false; }
        p += n;
        size -= (size_t) n;
        idle.touch();
    }
    return true;
}

// returns bytes read, 0 when the other end has finished, -1 on failure
int readSome (juce::StreamingSocket& socket, char* buffer, int size, const HttpRequest& r, Deadline& idle, juce::String& error)
{
    for (;;)
    {
        if (cancelled (r)) { error = "Cancelled"; return -1; }
        const int ready = socket.waitUntilReady (true, 250);
        if (ready < 0) { error = "The connection dropped"; return -1; }
        if (ready == 0) { if (idle.passed()) { error = "The engine stopped answering"; return -1; } continue; }
        const int n = socket.read (buffer, size, false);
        if (n < 0) { error = "The connection dropped"; return -1; }
        idle.touch();
        return n;
    }
}

/** Takes the body as it arrives and puts it in memory or straight into a file. */
struct BodySink
{
    BodySink (HttpResponse& res, const HttpRequest& req) : response (res), request (req)
    {
        if (request.saveTo != juce::File())
        {
            request.saveTo.deleteFile();
            file = std::make_unique<juce::FileOutputStream> (request.saveTo);
            if (! file->openedOk()) { failed = true; file.reset(); }
        }
    }

    bool write (const char* data, size_t size)
    {
        if (failed) return false;
        if (size == 0) return true;
        if (file != nullptr && wantsFile) { if (! file->write (data, size)) { failed = true; return false; } }
        else                              response.body.append (data, size);
        done += (juce::int64) size;
        if (request.onProgress != nullptr) request.onProgress (done, total);
        return true;
    }

    HttpResponse& response;
    const HttpRequest& request;
    std::unique_ptr<juce::FileOutputStream> file;
    bool wantsFile = true, failed = false;
    juce::int64 done = 0, total = -1;
};

HttpResponse sendOverSocket (const HttpRequest& r, const UrlParts& u)
{
    HttpResponse res;
    juce::StreamingSocket socket;
    if (! socket.connect (u.host, u.port, r.connectTimeoutMs))
    {
        res.error = "Nothing is answering at " + u.host + ":" + juce::String (u.port);
        return res;
    }

    juce::String head;
    head << r.method << " " << u.path << " HTTP/1.1\r\n"
         << "Host: " << (u.host.containsChar (':') ? "[" + u.host + "]" : u.host) << ":" << u.port << "\r\n"
         << "Connection: close\r\nAccept: */*\r\nUser-Agent: MUMusicMaker\r\n";
    for (auto& h : r.headers) head << h << "\r\n";
    if (r.body.getSize() > 0 || r.method == "POST") head << "Content-Length: " << (juce::int64) r.body.getSize() << "\r\n";
    head << "\r\n";

    const auto headUtf8 = head.toRawUTF8();
    if (! sendAll (socket, headUtf8, std::strlen (headUtf8), r, res.error)) return res;
    if (r.body.getSize() > 0 && ! sendAll (socket, r.body.getData(), r.body.getSize(), r, res.error)) return res;

    // read up to the blank line that ends the headers
    juce::MemoryBlock raw;
    juce::HeapBlock<char> buffer (65536);
    Deadline idle (r.idleTimeoutMs);
    int headerEnd = -1;
    while (headerEnd < 0)
    {
        const int n = readSome (socket, buffer, 65536, r, idle, res.error);
        if (n < 0) return res;
        if (n == 0) { res.error = "The engine closed the connection early"; return res; }
        raw.append (buffer, (size_t) n);
        const auto* d = static_cast<const char*> (raw.getData());
        for (size_t i = 0; i + 3 < raw.getSize(); ++i)
            if (d[i] == '\r' && d[i + 1] == '\n' && d[i + 2] == '\r' && d[i + 3] == '\n') { headerEnd = (int) i; break; }
        if (headerEnd < 0 && raw.getSize() > 256 * 1024) { res.error = "That is not an engine"; return res; }
    }

    const auto headerText = juce::String::fromUTF8 (static_cast<const char*> (raw.getData()), headerEnd);
    auto lines = juce::StringArray::fromLines (headerText);
    if (lines.isEmpty() || ! lines[0].startsWith ("HTTP/")) { res.error = "That is not an engine"; return res; }
    const int status = lines[0].fromFirstOccurrenceOf (" ", false, false).upToFirstOccurrenceOf (" ", false, false).getIntValue();
    for (int i = 1; i < lines.size(); ++i)
    {
        const int colon = lines[i].indexOfChar (':');
        if (colon > 0) res.headers.set (lines[i].substring (0, colon).trim().toLowerCase(), lines[i].substring (colon + 1).trim());
    }

    BodySink sink (res, r);
    sink.wantsFile = status >= 200 && status < 300;      // an error message is kept in memory, never written over the file
    if (sink.failed) { res.error = "Could not write to " + r.saveTo.getFullPathName(); return res; }

    const auto* rest = static_cast<const char*> (raw.getData()) + headerEnd + 4;
    const size_t restSize = raw.getSize() - (size_t) headerEnd - 4;
    const bool chunked = res.headers["transfer-encoding"].containsIgnoreCase ("chunked");
    const bool hasLength = res.headers.containsKey ("content-length");
    const juce::int64 length = res.headers["content-length"].getLargeIntValue();

    auto fail = [&] (const juce::String& why) { res.error = why; res.status = 0; return res; };

    if (chunked)
    {
        // sizes in hex on their own line, then that many bytes, then a blank line; a size of zero ends it
        juce::MemoryBlock pending (rest, restSize);
        size_t pos = 0;
        bool finished = false;
        while (! finished)
        {
            const auto* d = static_cast<const char*> (pending.getData());
            const size_t have = pending.getSize();
            size_t lineEnd = pos;
            while (lineEnd + 1 < have && ! (d[lineEnd] == '\r' && d[lineEnd + 1] == '\n')) ++lineEnd;
            const bool gotLine = lineEnd + 1 < have;
            if (gotLine)
            {
                const auto sizeText = juce::String::fromUTF8 (d + pos, (int) (lineEnd - pos)).upToFirstOccurrenceOf (";", false, false).trim();
                const auto chunk = (size_t) sizeText.getHexValue64();
                const size_t dataStart = lineEnd + 2;
                if (chunk == 0) { finished = true; break; }
                if (have >= dataStart + chunk + 2)
                {
                    if (! sink.write (d + dataStart, chunk)) return fail ("Could not save the download");
                    pos = dataStart + chunk + 2;
                    if (pos > (1 << 20)) { pending.removeSection (0, pos); pos = 0; }
                    continue;
                }
            }
            const int n = readSome (socket, buffer, 65536, r, idle, res.error);
            if (n < 0) { res.status = 0; return res; }
            if (n == 0) return fail ("The engine closed the connection early");
            pending.append (buffer, (size_t) n);
        }
    }
    else
    {
        sink.total = hasLength ? length : -1;
        if (! sink.write (rest, hasLength ? (size_t) juce::jmin ((juce::int64) restSize, length) : restSize)) return fail ("Could not save the download");
        while (! hasLength || sink.done < length)
        {
            const int n = readSome (socket, buffer, 65536, r, idle, res.error);
            if (n < 0) { res.status = 0; return res; }
            if (n == 0)
            {
                if (hasLength) return fail ("The engine closed the connection early");
                break;
            }
            const size_t take = hasLength ? (size_t) juce::jmin ((juce::int64) n, length - sink.done) : (size_t) n;
            if (! sink.write (buffer, take)) return fail ("Could not save the download");
        }
    }

    if (sink.file != nullptr) { sink.file->flush(); sink.file.reset(); }
    if (! sink.wantsFile && r.saveTo != juce::File()) r.saveTo.deleteFile();
    res.status = status;
    return res;
}

HttpResponse sendThroughSystem (const HttpRequest& r)
{
    HttpResponse res;
    juce::URL target (r.url);
    if (r.body.getSize() > 0) target = target.withPOSTData (r.body);

    juce::WebInputStream stream (target, r.body.getSize() > 0);
    stream.withExtraHeaders (r.headers.joinIntoString ("\r\n"))
          .withCustomRequestCommand (r.method)
          .withConnectionTimeout (r.idleTimeoutMs)
          .withNumRedirectsToFollow (4);
    if (! stream.connect (nullptr)) { res.error = "Nothing is answering at that address"; return res; }

    const auto responseHeaders = stream.getResponseHeaders();
    for (auto& key : responseHeaders.getAllKeys())
        res.headers.set (key.toLowerCase(), responseHeaders[key]);

    const int status = stream.getStatusCode();
    BodySink sink (res, r);
    sink.wantsFile = status >= 200 && status < 300;
    sink.total = stream.getTotalLength();
    juce::HeapBlock<char> buffer (65536);
    while (! stream.isExhausted())
    {
        if (cancelled (r)) { res.error = "Cancelled"; return res; }
        const int n = stream.read (buffer, 65536);
        if (n <= 0) break;
        if (! sink.write (buffer, (size_t) n)) { res.error = "Could not save the download"; return res; }
    }
    if (sink.file != nullptr) { sink.file->flush(); sink.file.reset(); }
    if (! sink.wantsFile && r.saveTo != juce::File()) r.saveTo.deleteFile();
    res.status = status;
    return res;
}
}

HttpResponse httpSend (const HttpRequest& r)
{
    const auto parts = splitUrl (r.url);
    if (! parts.valid)
    {
        HttpResponse res;
        res.error = "That address does not look right";
        return res;
    }
    return parts.https ? sendThroughSystem (r) : sendOverSocket (r, parts);
}
}
