#pragma once
#include <juce_core/juce_core.h>

namespace mm
{
struct HttpResponse
{
    int status = 0;                 // 0 means the other end could not be reached
    juce::StringPairArray headers;  // names in lower case
    juce::MemoryBlock body;
    juce::String error;             // why it could not be reached

    bool ok() const                 { return status >= 200 && status < 300; }
    juce::String text() const       { return body.toString(); }
    juce::var json() const          { return juce::JSON::parse (text()); }
    juce::String errorText() const; // the engine's own message when it gave one
};

struct HttpRequest
{
    juce::String method = "GET";
    juce::String url;
    juce::StringArray headers;
    juce::MemoryBlock body;
    int connectTimeoutMs = 2500;
    int idleTimeoutMs = 30000;                                  // gives up after this long with nothing arriving
    std::function<bool()> shouldCancel;
    std::function<void (juce::int64 done, juce::int64 total)> onProgress;
    juce::File saveTo;                                          // a download goes straight to this file
};

/** Plain http goes over a socket of our own, so it behaves the same inside every host on every system
    (a host's own networking rules do not get a say). https goes through the system. */
HttpResponse httpSend (const HttpRequest&);

/** Builds the body of a form that carries files. */
class MultipartForm
{
public:
    MultipartForm();
    void addField (const juce::String& name, const juce::String& value);
    bool addFile (const juce::String& name, const juce::File& file, const juce::String& mimeType);
    juce::MemoryBlock finish();
    juce::String contentType() const    { return "multipart/form-data; boundary=" + boundary; }

private:
    juce::String boundary;
    juce::MemoryOutputStream out;
};

struct UrlParts
{
    bool valid = false, https = false;
    juce::String host, path;        // path includes the query
    int port = 80;
};
UrlParts splitUrl (const juce::String& url);
juce::String mimeTypeFor (const juce::File& audio);
}
