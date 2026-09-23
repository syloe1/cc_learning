#pragma once

#include <string>
#include <map>

class Buffer;

// Builds an HTTP 1.0 response.
class HttpResponse {
public:
    enum HttpStatusCode {
        k200Ok = 200,
        k400BadRequest = 400,
        k404NotFound = 404,
        k500InternalServerError = 500
    };

    HttpResponse(bool close = true)
        : statusCode_(k200Ok), closeConnection_(close) {}

    void setStatusCode(HttpStatusCode code) { statusCode_ = code; }
    void setStatusMessage(const std::string& message) { statusMessage_ = message; }
    void setCloseConnection(bool close) { closeConnection_ = close; }

    void setContentType(const std::string& contentType) {
        addHeader("Content-Type", contentType);
    }

    void addHeader(const std::string& key, const std::string& value) {
        headers_[key] = value;
    }

    void setBody(const std::string& body) { body_ = body; }

    // Serialize the response to a Buffer for sending.
    void appendToBuffer(Buffer* output) const;

private:
    std::string statusMessageForCode(HttpStatusCode code) const;

    HttpStatusCode statusCode_;
    std::string statusMessage_;
    std::map<std::string, std::string> headers_;
    std::string body_;
    bool closeConnection_;
};
