#include <string>

#include "model/CameraState.hpp"
#include "model/SystemStatus.hpp"
#include "tests/TestSupport.hpp"
#include "web/HttpMessage.hpp"

int main()
{
    // --- valid request with query string and mixed-case headers ---
    {
        const std::string raw =
            "GET /api/status?x=1&y=two%20words HTTP/1.1\r\n"
            "Host: localhost:8080\r\n"
            "Content-Length: 0\r\n"
            "\r\n";

        HttpRequest request;
        CHECK(parseRequest(raw, request));
        CHECK_EQ(request.method, std::string("GET"));
        CHECK_EQ(request.path, std::string("/api/status"));
        CHECK_EQ(request.query.count("x") ? request.query.at("x") : std::string("<missing>"), std::string("1"));
        CHECK_EQ(request.query.count("y") ? request.query.at("y") : std::string("<missing>"), std::string("two words"));
        CHECK_EQ(request.header("HOST"), std::string("localhost:8080"));
        CHECK_EQ(request.header("host"), std::string("localhost:8080"));
        CHECK_EQ(request.header("missing"), std::string(""));
    }

    // --- request with a body ---
    {
        const std::string raw =
            "POST /api/config HTTP/1.1\r\n"
            "Content-Type: application/json\r\n"
            "\r\n"
            "{\"width\":1280,\"height\":720}";

        HttpRequest request;
        CHECK(parseRequest(raw, request));
        CHECK_EQ(request.method, std::string("POST"));
        CHECK_EQ(request.path, std::string("/api/config"));
        CHECK_EQ(request.body, std::string("{\"width\":1280,\"height\":720}"));
    }

    // --- malformed requests must be rejected ---
    {
        HttpRequest request;
        CHECK(!parseRequest("", request));
        CHECK(!parseRequest("GARBAGE\r\n\r\n", request));
        CHECK(!parseRequest("GET /x\r\n\r\n", request));                     // missing version
        CHECK(!parseRequest("GET x HTTP/1.1\r\n\r\n", request));             // relative target
        CHECK(!parseRequest("GET /x HTTP/1.1\r\nBadHeader\r\n\r\n", request)); // no colon
    }

    // --- query parsing ---
    {
        const auto query = parseQuery("/api/config?width=1280&height=720");
        CHECK_EQ(query.count("width") ? query.at("width") : std::string("<missing>"), std::string("1280"));
        CHECK_EQ(query.count("height") ? query.at("height") : std::string("<missing>"), std::string("720"));
    }

    // --- response serialization ---
    {
        const std::string wire = serializeResponse(HttpResponse::json("{\"a\":1}", 404));
        CHECK_EQ(wire.rfind("HTTP/1.1 404 Not Found\r\n", 0), std::size_t(0));
        CHECK(wire.find("Content-Type: application/json\r\n") != std::string::npos);
        CHECK(wire.find("Content-Length: 7\r\n") != std::string::npos);
        CHECK(wire.find("Connection: close\r\n") != std::string::npos);
        CHECK(wire.find("\r\n\r\n{\"a\":1}") != std::string::npos);

        const std::string ok = serializeResponse(HttpResponse::text("hi"));
        CHECK_EQ(ok.rfind("HTTP/1.1 200 OK\r\n", 0), std::size_t(0));
        CHECK(ok.find("Content-Type: text/plain; charset=utf-8\r\n") != std::string::npos);
    }

    // --- JSON escaping and model serialization ---
    {
        CHECK_EQ(jsonEscape("a\"b\\c\nd"), std::string("a\\\"b\\\\c\\nd"));
        CHECK_EQ(jsonEscape("\x01"), std::string("\\u0001"));

        const std::string cameraJson = toJson(CameraState{});
        CHECK(cameraJson.find("\"status\":\"idle\"") != std::string::npos);
        CHECK(cameraJson.find("\"sourceName\":\"videotestsrc\"") != std::string::npos);
        CHECK(cameraJson.find("\"width\":640") != std::string::npos);

        const std::string systemJson = toJson(SystemStatus{});
        CHECK(systemJson.find("\"network\":\"unknown\"") != std::string::npos);
        CHECK(systemJson.find("\"cameraConnected\":false") != std::string::npos);

        CHECK_EQ(jsonError("bad \"thing\""), std::string("{\"error\":\"bad \\\"thing\\\"\"}"));
    }

    TEST_RETURN();
}
