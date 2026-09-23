#pragma once

#include <string>
#include <map>
#include <cstdint>

// Simple HTTP 1.0 request parser.
// Only supports GET method.
class HttpRequest {
public:
    enum Method { kInvalid, kGet };

    HttpRequest() : method_(kInvalid), majorVersion_(1), minorVersion_(0) {}

    // Parse the raw HTTP request from the buffer data.
    // Returns true if a complete request was parsed.
    bool parseRequest(const char* begin, const char* end);

    Method method() const { return method_; }
    const std::string& path() const { return path_; }
    const std::map<std::string, std::string>& headers() const { return headers_; }
    const std::string& query() const { return query_; }

    // Get a specific header value. Returns empty string if not found.
    std::string getHeader(const std::string& field) const;

private:
    bool parseRequestLine(const char* begin, const char* end);
    bool parseHeaders(const char* begin, const char* end);
    std::string urlDecode(const std::string& input) const;

    Method method_;
    std::string path_;   // URL-decoded
    std::string query_;
    std::map<std::string, std::string> headers_;
    int majorVersion_;
    int minorVersion_;
};
