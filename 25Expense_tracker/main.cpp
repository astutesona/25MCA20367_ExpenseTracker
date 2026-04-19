// main.cpp — Smart Expense Tracker — C++ Backend + HTTP Server
// Build: g++ -std=c++14 -O2 -o SmartExpenseTracker.exe main.cpp sqlite3.o -lws2_32 -I.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
// Note: shellapi.h omitted to avoid REFIID issue on old MinGW; using WinExec to open browser

#include "server.h"
#include "database.h"
#include "json.hpp"

#include <string>
#include <map>
#include <sstream>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <random>
#include <cstring>

using json = nlohmann::json;

// ─── Globals ─────────────────────────────────────────────────────────────────
DatabaseManager* gDb = nullptr;

// Session store: token -> user_id, username
std::map<std::string, int>         gSessionUser;
std::map<std::string, std::string> gSessionName;

// ─── Session / Token helpers ──────────────────────────────────────────────────
static std::string newToken() {
    static const char al[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, (int)(sizeof(al) - 2));
    std::string tok;
    tok.reserve(32);
    for (int i = 0; i < 32; ++i) tok += al[dist(gen)];
    return tok;
}

// Returns user_id - bypass authentication for "Remove Login" feature
static int authUser(const HttpRequest& req, std::string& username) {
    username = "Admin";
    return 1; // Use default Admin ID (1)
}

// ─── JSON helpers ─────────────────────────────────────────────────────────────
static void sendJson(HttpResponse& res, const json& j, int status = 200) {
    res.status = status;
    res.setJson(j.dump());
}
static void sendErr(HttpResponse& res, const std::string& msg, int status = 400) {
    sendJson(res, {{"success", false}, {"message", msg}}, status);
}

// ─── Static File Serving ──────────────────────────────────────────────────────
static void serveStatic(const std::string& path, HttpResponse& res) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        res.status = 404;
        res.setHtml("<h1>404 Not Found</h1><p>" + path + "</p>");
        return;
    }
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    res.setFile(content, getMimeType(path));
}

// ─── Route Helpers ────────────────────────────────────────────────────────────
// Extract last path segment as integer ID: /api/transactions/42 -> 42
static int extractId(const std::string& path, const std::string& prefix) {
    if (path.size() <= prefix.size()) return -1;
    std::string idStr = path.substr(prefix.size());
    try { return std::stoi(idStr); } catch (...) { return -1; }
}

// ─── Main ─────────────────────────────────────────────────────────────────────
int main() {
    // Init DB
    gDb = new DatabaseManager("expenses.db");
    gDb->initialize();

    // Ensure default user "Admin" exists (with ID 1 if possible)
    int dummyUid;
    gDb->registerUser("Admin", "admin123", dummyUid);

    HttpServer svr;

    // ── POST /api/login ────────────────────────────────────────────
    svr.Post("/api/login", [](const HttpRequest& req, HttpResponse& res) {
        try {
            json body = json::parse(req.body);
            std::string username = body.value("username", "");
            std::string password = body.value("password", "");
            if (username.empty() || password.empty()) { sendErr(res, "Username and password required"); return; }

            int uid = -1;
            if (gDb->authenticate(username, password, uid)) {
                std::string tok = newToken();
                gSessionUser[tok] = uid;
                gSessionName[tok] = username;
                sendJson(res, {{"success",true},{"token",tok},{"username",username},{"userId",uid}});
            } else {
                sendErr(res, "Invalid username or password", 401);
            }
        } catch (...) { sendErr(res, "Bad request"); }
    });

    // ── POST /api/register ─────────────────────────────────────────
    svr.Post("/api/register", [](const HttpRequest& req, HttpResponse& res) {
        try {
            json body = json::parse(req.body);
            std::string username = body.value("username", "");
            std::string password = body.value("password", "");
            if (username.empty() || password.empty()) { sendErr(res, "Username and password required"); return; }
            if (username.size() < 3) { sendErr(res, "Username must be at least 3 characters"); return; }
            if (password.size() < 4) { sendErr(res, "Password must be at least 4 characters"); return; }

            int uid = -1;
            if (gDb->registerUser(username, password, uid)) {
                std::string tok = newToken();
                gSessionUser[tok] = uid;
                gSessionName[tok] = username;
                sendJson(res, {{"success",true},{"token",tok},{"username",username},{"userId",uid}}, 201);
            } else {
                sendErr(res, "Username already taken. Please choose another.");
            }
        } catch (...) { sendErr(res, "Bad request"); }
    });

    // ── GET /api/stats ─────────────────────────────────────────────
    svr.Get("/api/stats", [](const HttpRequest& req, HttpResponse& res) {
        std::string username;
        int uid = authUser(req, username);
        if (uid < 0) { sendErr(res, "Unauthorized", 401); return; }

        DashboardStats s = gDb->getStats(uid);
        json catArr = json::array();
        for (auto& c : s.categoryBreakdown)
            catArr.push_back({{"category", c.category}, {"amount", c.amount}});
        json monArr = json::array();
        for (auto& m : s.monthlyData)
            monArr.push_back({{"month", m.month}, {"income", m.income}, {"expense", m.expense}});

        sendJson(res, {
            {"success",          true},
            {"username",         username},
            {"totalIncome",      s.totalIncome},
            {"totalExpense",     s.totalExpense},
            {"balance",          s.balance},
            {"savings",          s.savings},
            {"categoryBreakdown",catArr},
            {"monthlyData",      monArr}
        });
    });

    // ── GET /api/transactions ──────────────────────────────────────
    svr.Get("/api/transactions", [](const HttpRequest& req, HttpResponse& res) {
        std::string username;
        int uid = authUser(req, username);
        if (uid < 0) { sendErr(res, "Unauthorized", 401); return; }

        std::string month  = req.getParam("month");
        std::string cat    = req.getParam("category");
        std::string search = req.getParam("search");

        auto txList = gDb->getTransactions(uid, month, cat, search);
        json arr = json::array();
        for (auto& t : txList) {
            arr.push_back({
                {"id",          t.id},
                {"type",        t.type},
                {"amount",      t.amount},
                {"category",    t.category},
                {"description", t.description},
                {"date",        t.date}
            });
        }
        sendJson(res, {{"success", true}, {"transactions", arr}});
    });

    // ── POST /api/transactions ─────────────────────────────────────
    svr.Post("/api/transactions", [](const HttpRequest& req, HttpResponse& res) {
        std::string username;
        int uid = authUser(req, username);
        if (uid < 0) { sendErr(res, "Unauthorized", 401); return; }
        try {
            json body    = json::parse(req.body);
            Transaction t;
            t.user_id     = uid;
            t.type        = body.value("type",        "expense");
            t.amount      = body.value("amount",      0.0);
            t.category    = body.value("category",    "Other");
            t.description = body.value("description", "");
            t.date        = body.value("date",        "");
            if (t.amount <= 0 || t.date.empty()) { sendErr(res, "Amount and date are required"); return; }
            if (t.type != "income" && t.type != "expense") t.type = "expense";

            int id = gDb->addTransaction(t);
            if (id > 0) sendJson(res, {{"success", true}, {"id", id}}, 201);
            else        sendErr(res, "Failed to save transaction", 500);
        } catch (...) { sendErr(res, "Bad request"); }
    });

    // ── PUT /api/transactions/<id> ─────────────────────────────────
    svr.Put("/api/transactions/", [](const HttpRequest& req, HttpResponse& res) {
        std::string username;
        int uid = authUser(req, username);
        if (uid < 0) { sendErr(res, "Unauthorized", 401); return; }

        int id = extractId(req.path, "/api/transactions/");
        if (id < 0) { sendErr(res, "Invalid ID"); return; }

        try {
            json body    = json::parse(req.body);
            Transaction t;
            t.id          = id;
            t.user_id     = uid;
            t.type        = body.value("type",        "expense");
            t.amount      = body.value("amount",      0.0);
            t.category    = body.value("category",    "Other");
            t.description = body.value("description", "");
            t.date        = body.value("date",        "");
            if (t.amount <= 0 || t.date.empty()) { sendErr(res, "Amount and date required"); return; }

            if (gDb->updateTransaction(t)) sendJson(res, {{"success", true}});
            else sendErr(res, "Update failed — record not found", 404);
        } catch (...) { sendErr(res, "Bad request"); }
    }, true); // prefix=true

    // ── DELETE /api/transactions/<id> ──────────────────────────────
    svr.Delete("/api/transactions/", [](const HttpRequest& req, HttpResponse& res) {
        std::string username;
        int uid = authUser(req, username);
        if (uid < 0) { sendErr(res, "Unauthorized", 401); return; }

        int id = extractId(req.path, "/api/transactions/");
        if (id < 0) { sendErr(res, "Invalid ID"); return; }

        if (gDb->deleteTransaction(id, uid)) sendJson(res, {{"success", true}});
        else sendErr(res, "Delete failed — record not found", 404);
    }, true); // prefix=true

    // ── GET /* — Static Files ──────────────────────────────────────
    svr.Get("/", [](const HttpRequest& req, HttpResponse& res) {
        std::string path = req.path;
        if (path == "/" || path.empty()) path = "/index.html";
        serveStatic("frontend" + path, res);
    }, true); // prefix=true (catch-all)

    // ─── Banner ────────────────────────────────────────────────────
    std::cout << "\n";
    std::cout << "  ╔══════════════════════════════════════════╗\n";
    std::cout << "  ║       Smart Expense Tracker v1.0         ║\n";
    std::cout << "  ║  Server → http://localhost:8080           ║\n";
    std::cout << "  ║  Press Ctrl+C to stop                    ║\n";
    std::cout << "  ╚══════════════════════════════════════════╝\n\n";
    std::cout << "  Opening browser...\n\n";

    // Open default browser (using cmd /c start — works on all Windows versions)
    WinExec("cmd /c start http://localhost:8080", SW_HIDE);

    // Start blocking HTTP server
    if (!svr.listen(8080)) {
        std::cerr << "Server failed to start. Is port 8080 in use?\n";
        delete gDb;
        return 1;
    }

    delete gDb;
    return 0;
}
