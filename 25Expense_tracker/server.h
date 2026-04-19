// server.h — Minimal Windows HTTP Server (Winsock, single-threaded, no external deps)
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include <string>
#include <map>
#include <vector>
#include <functional>
#include <sstream>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cctype>

// ─── URL Decode ──────────────────────────────────────────────────────────────
static std::string urlDecode(const std::string& s) {
    std::string result;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            int val = 0;
            sscanf(s.substr(i+1, 2).c_str(), "%02x", &val);
            result += (char)val;
            i += 2;
        } else if (s[i] == '+') {
            result += ' ';
        } else {
            result += s[i];
        }
    }
    return result;
}

// ─── HTTP Request ────────────────────────────────────────────────────────────
struct HttpRequest {
    std::string method;
    std::string path;
    std::string query;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> params;   // query params
    std::string body;

    std::string getParam(const std::string& key) const {
        auto it = params.find(key);
        return it != params.end() ? urlDecode(it->second) : "";
    }
    std::string getHeader(const std::string& key) const {
        // headers are stored lowercase
        std::string k = key;
        std::transform(k.begin(), k.end(), k.begin(), ::tolower);
        auto it = headers.find(k);
        return it != headers.end() ? it->second : "";
    }
};

// ─── HTTP Response ───────────────────────────────────────────────────────────
struct HttpResponse {
    int status = 200;
    std::string contentType = "text/plain; charset=utf-8";
    std::map<std::string, std::string> extraHeaders;
    std::string body;

    void setJson(const std::string& json) {
        contentType = "application/json; charset=utf-8";
        body = json;
    }
    void setHtml(const std::string& html) {
        contentType = "text/html; charset=utf-8";
        body = html;
    }
    void setFile(const std::string& content, const std::string& mime) {
        contentType = mime;
        body = content;
    }

    std::string buildRaw() const {
        static const std::map<int,const char*> reasons = {
            {200,"OK"},{201,"Created"},{204,"No Content"},
            {400,"Bad Request"},{401,"Unauthorized"},{403,"Forbidden"},
            {404,"Not Found"},{500,"Internal Server Error"}
        };
        auto it = reasons.find(status);
        const char* reason = (it != reasons.end()) ? it->second : "OK";

        std::ostringstream oss;
        oss << "HTTP/1.1 " << status << " " << reason << "\r\n";
        oss << "Content-Type: " << contentType << "\r\n";
        oss << "Content-Length: " << body.size() << "\r\n";
        oss << "Access-Control-Allow-Origin: *\r\n";
        oss << "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n";
        oss << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
        oss << "Connection: close\r\n";
        for (auto& h : extraHeaders) {
            oss << h.first << ": " << h.second << "\r\n";
        }
        oss << "\r\n";
        oss << body;
        return oss.str();
    }
};

using RouteHandler = std::function<void(const HttpRequest&, HttpResponse&)>;

// ─── Route Entry ─────────────────────────────────────────────────────────────
struct Route {
    std::string method;   // "GET","POST","PUT","DELETE","OPTIONS","*"
    std::string path;
    bool prefix;          // if true: path is a prefix, not exact
    RouteHandler handler;
};

// ─── MIME Types ──────────────────────────────────────────────────────────────
static std::string getMimeType(const std::string& path) {
    auto ext_pos = path.rfind('.');
    if (ext_pos == std::string::npos) return "application/octet-stream";
    std::string ext = path.substr(ext_pos);
    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".css")  return "text/css; charset=utf-8";
    if (ext == ".js")   return "application/javascript; charset=utf-8";
    if (ext == ".json") return "application/json";
    if (ext == ".png")  return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".webp") return "image/webp";
    if (ext == ".svg")  return "image/svg+xml";
    if (ext == ".ico")  return "image/x-icon";
    if (ext == ".gif")  return "image/gif";
    if (ext == ".woff2") return "font/woff2";
    return "application/octet-stream";
}

// ─── HTTP Server ─────────────────────────────────────────────────────────────
class HttpServer {
    std::vector<Route> routes_;
    bool running_ = false;
    SOCKET serverSock_ = INVALID_SOCKET;

    static void parseQueryParams(const std::string& query,
                                  std::map<std::string,std::string>& params) {
        std::istringstream ss(query);
        std::string token;
        while (std::getline(ss, token, '&')) {
            auto eq = token.find('=');
            if (eq != std::string::npos) {
                params[token.substr(0, eq)] = token.substr(eq + 1);
            } else if (!token.empty()) {
                params[token] = "";
            }
        }
    }

    HttpRequest parseRequest(const std::string& raw) {
        HttpRequest req;
        std::istringstream ss(raw);
        std::string line;

        // First line
        if (std::getline(ss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            std::istringstream fl(line);
            fl >> req.method >> req.path;
            auto q = req.path.find('?');
            if (q != std::string::npos) {
                req.query = req.path.substr(q + 1);
                req.path  = req.path.substr(0, q);
                parseQueryParams(req.query, req.params);
            }
        }

        // Headers
        int contentLength = 0;
        while (std::getline(ss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) break;
            auto col = line.find(':');
            if (col != std::string::npos) {
                std::string key = line.substr(0, col);
                std::string val = line.substr(col + 1);
                // Trim
                while (!val.empty() && (val.front()==' '||val.front()=='\t')) val.erase(val.begin());
                std::transform(key.begin(), key.end(), key.begin(), ::tolower);
                req.headers[key] = val;
                if (key == "content-length") {
                    try { contentLength = std::stoi(val); } catch(...) {}
                }
            }
        }

        // Body — read remaining bytes (after \r\n\r\n) up to content-length
        size_t headerEnd = raw.find("\r\n\r\n");
        if (headerEnd != std::string::npos && contentLength > 0) {
            size_t bodyStart = headerEnd + 4;
            if (bodyStart < raw.size()) {
                req.body = raw.substr(bodyStart, contentLength);
            }
        }
        return req;
    }

    void readFullRequest(SOCKET sock, std::string& rawData) {
        char buf[8192];
        int n;
        // Set a small receive timeout
        DWORD timeout = 3000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

        while ((n = recv(sock, buf, sizeof(buf), 0)) > 0) {
            rawData.append(buf, n);

            size_t headerEnd = rawData.find("\r\n\r\n");
            if (headerEnd == std::string::npos) continue;

            // Check for content-length
            std::string lowerRaw = rawData.substr(0, headerEnd);
            std::transform(lowerRaw.begin(), lowerRaw.end(), lowerRaw.begin(), ::tolower);
            auto clPos = lowerRaw.find("content-length:");
            if (clPos == std::string::npos) break; // No body

            auto clEnd = lowerRaw.find("\r\n", clPos);
            std::string clStr = lowerRaw.substr(clPos + 15, clEnd - (clPos + 15));
            while (!clStr.empty() && clStr.front() == ' ') clStr.erase(clStr.begin());
            int expectedBody = 0;
            try { expectedBody = std::stoi(clStr); } catch(...) { break; }

            int bodyReceived = (int)rawData.size() - (int)(headerEnd + 4);
            if (bodyReceived >= expectedBody) break;
        }
    }

    bool routeMatches(const Route& r, const std::string& method, const std::string& path) {
        if (r.method != "*" && r.method != method) return false;
        if (r.prefix) {
            return path.rfind(r.path, 0) == 0;
        }
        return path == r.path;
    }

    void handleConnection(SOCKET clientSock) {
        std::string rawData;
        readFullRequest(clientSock, rawData);

        if (rawData.empty()) {
            closesocket(clientSock);
            return;
        }

        HttpRequest req = parseRequest(rawData);
        HttpResponse res;

        // CORS preflight
        if (req.method == "OPTIONS") {
            res.status = 204;
            res.body = "";
            std::string r = res.buildRaw();
            send(clientSock, r.c_str(), (int)r.size(), 0);
            closesocket(clientSock);
            return;
        }

        bool handled = false;
        for (auto& route : routes_) {
            if (routeMatches(route, req.method, req.path)) {
                route.handler(req, res);
                handled = true;
                break;
            }
        }

        if (!handled) {
            res.status = 404;
            res.body = "404 Not Found";
            res.contentType = "text/plain";
        }

        std::string response = res.buildRaw();
        // Send in chunks if large (e.g. images)
        size_t total = 0;
        while (total < response.size()) {
            int sent = send(clientSock, response.c_str() + total,
                            (int)(response.size() - total), 0);
            if (sent <= 0) break;
            total += sent;
        }
        closesocket(clientSock);
    }

public:
    void Get(const std::string& path, RouteHandler h, bool prefix = false) {
        routes_.push_back({"GET", path, prefix, h});
    }
    void Post(const std::string& path, RouteHandler h) {
        routes_.push_back({"POST", path, false, h});
    }
    void Put(const std::string& path, RouteHandler h, bool prefix = false) {
        routes_.push_back({"PUT", path, prefix, h});
    }
    void Delete(const std::string& path, RouteHandler h, bool prefix = false) {
        routes_.push_back({"DELETE", path, prefix, h});
    }

    bool listen(int port) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
            std::cerr << "WSAStartup failed\n";
            return false;
        }
        serverSock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (serverSock_ == INVALID_SOCKET) {
            std::cerr << "Socket creation failed: " << WSAGetLastError() << "\n";
            WSACleanup();
            return false;
        }
        int yes = 1;
        setsockopt(serverSock_, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes));

        sockaddr_in addr = {};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port        = htons((u_short)port);

        if (bind(serverSock_, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            std::cerr << "Bind failed: " << WSAGetLastError() << "\n";
            closesocket(serverSock_);
            WSACleanup();
            return false;
        }
        if (::listen(serverSock_, SOMAXCONN) == SOCKET_ERROR) {
            std::cerr << "Listen failed\n";
            closesocket(serverSock_);
            WSACleanup();
            return false;
        }

        running_ = true;
        while (running_) {
            SOCKET client = accept(serverSock_, NULL, NULL);
            if (client != INVALID_SOCKET) {
                handleConnection(client);
            }
        }
        closesocket(serverSock_);
        WSACleanup();
        return true;
    }
};
