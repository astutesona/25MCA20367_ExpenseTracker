// database.h — SQLite wrapper for Smart Expense Tracker
#pragma once

#include "sqlite3.h"
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <cstdio>
#include <iostream>

// ─── Password Hash (FNV-1a 64-bit) ───────────────────────────────────────────
static std::string hashPassword(const std::string& password) {
    uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : password) {
        hash ^= (uint64_t)c;
        hash *= 1099511628211ULL;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%016llx", (unsigned long long)hash);
    return std::string("fnv1a:") + buf;
}

// ─── Data Models ─────────────────────────────────────────────────────────────
struct Transaction {
    int         id          = 0;
    int         user_id     = 0;
    std::string type;        // "income" | "expense"
    double      amount      = 0.0;
    std::string category;
    std::string description;
    std::string date;        // "YYYY-MM-DD"
};

struct CategoryStat {
    std::string category;
    double      amount = 0.0;
};

struct MonthStat {
    std::string month;    // "YYYY-MM"
    double      income  = 0.0;
    double      expense = 0.0;
};

struct DashboardStats {
    double totalIncome  = 0.0;
    double totalExpense = 0.0;
    double balance      = 0.0;
    double savings      = 0.0;
    std::vector<CategoryStat> categoryBreakdown;
    std::vector<MonthStat>    monthlyData;
};

// ─── Database Manager ─────────────────────────────────────────────────────────
class DatabaseManager {
    sqlite3* db_ = nullptr;

    bool exec(const std::string& sql) {
        char* err = nullptr;
        int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err);
        if (rc != SQLITE_OK) {
            std::cerr << "SQL error: " << (err ? err : "unknown") << "\n";
            sqlite3_free(err);
            return false;
        }
        return true;
    }

public:
    explicit DatabaseManager(const std::string& path = "expenses.db") {
        if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
            std::cerr << "Cannot open database: " << sqlite3_errmsg(db_) << "\n";
        }
    }
    ~DatabaseManager() {
        if (db_) sqlite3_close(db_);
    }

    // ── Schema init ───────────────────────────────────────────────────────────
    void initialize() {
        exec("PRAGMA journal_mode=WAL;");
        exec("PRAGMA foreign_keys=ON;");

        exec(R"(
            CREATE TABLE IF NOT EXISTS users (
                id           INTEGER PRIMARY KEY AUTOINCREMENT,
                username     TEXT    UNIQUE NOT NULL,
                password_hash TEXT   NOT NULL,
                created_at   TEXT    DEFAULT (datetime('now'))
            );
        )");

        exec(R"(
            CREATE TABLE IF NOT EXISTS transactions (
                id          INTEGER PRIMARY KEY AUTOINCREMENT,
                user_id     INTEGER NOT NULL,
                type        TEXT    NOT NULL,
                amount      REAL    NOT NULL,
                category    TEXT    NOT NULL DEFAULT 'Other',
                description TEXT    NOT NULL DEFAULT '',
                date        TEXT    NOT NULL,
                created_at  TEXT    DEFAULT (datetime('now')),
                FOREIGN KEY (user_id) REFERENCES users(id)
            );
        )");

        exec("CREATE INDEX IF NOT EXISTS idx_tx_user ON transactions(user_id);");
        exec("CREATE INDEX IF NOT EXISTS idx_tx_date ON transactions(date);");
    }

    // ── Auth ──────────────────────────────────────────────────────────────────
    bool authenticate(const std::string& username, const std::string& password, int& userId) {
        std::string hash = hashPassword(password);
        const char* sql = "SELECT id FROM users WHERE username=? AND password_hash=? LIMIT 1;";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, hash.c_str(),     -1, SQLITE_STATIC);
        bool ok = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            userId = sqlite3_column_int(stmt, 0);
            ok = true;
        }
        sqlite3_finalize(stmt);
        return ok;
    }

    bool registerUser(const std::string& username, const std::string& password, int& userId) {
        std::string hash = hashPassword(password);
        const char* sql = "INSERT INTO users (username, password_hash) VALUES (?,?);";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, hash.c_str(),     -1, SQLITE_STATIC);
        bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        if (ok) {
            userId = (int)sqlite3_last_insert_rowid(db_);
        }
        return ok;
    }

    // ── CRUD Transactions ─────────────────────────────────────────────────────
    int addTransaction(const Transaction& t) {
        const char* sql = R"(
            INSERT INTO transactions (user_id, type, amount, category, description, date)
            VALUES (?,?,?,?,?,?);
        )";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return -1;
        sqlite3_bind_int   (stmt, 1, t.user_id);
        sqlite3_bind_text  (stmt, 2, t.type.c_str(),        -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 3, t.amount);
        sqlite3_bind_text  (stmt, 4, t.category.c_str(),    -1, SQLITE_STATIC);
        sqlite3_bind_text  (stmt, 5, t.description.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text  (stmt, 6, t.date.c_str(),        -1, SQLITE_STATIC);
        int id = -1;
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            id = (int)sqlite3_last_insert_rowid(db_);
        }
        sqlite3_finalize(stmt);
        return id;
    }

    bool updateTransaction(const Transaction& t) {
        const char* sql = R"(
            UPDATE transactions
            SET type=?, amount=?, category=?, description=?, date=?
            WHERE id=? AND user_id=?;
        )";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text  (stmt, 1, t.type.c_str(),        -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 2, t.amount);
        sqlite3_bind_text  (stmt, 3, t.category.c_str(),    -1, SQLITE_STATIC);
        sqlite3_bind_text  (stmt, 4, t.description.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text  (stmt, 5, t.date.c_str(),        -1, SQLITE_STATIC);
        sqlite3_bind_int   (stmt, 6, t.id);
        sqlite3_bind_int   (stmt, 7, t.user_id);
        bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return ok;
    }

    bool deleteTransaction(int id, int userId) {
        const char* sql = "DELETE FROM transactions WHERE id=? AND user_id=?;";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_int(stmt, 1, id);
        sqlite3_bind_int(stmt, 2, userId);
        bool ok = (sqlite3_step(stmt) == SQLITE_DONE) && (sqlite3_changes(db_) > 0);
        sqlite3_finalize(stmt);
        return ok;
    }

    // ── Query Transactions ────────────────────────────────────────────────────
    std::vector<Transaction> getTransactions(int userId,
                                              const std::string& month    = "",
                                              const std::string& category = "",
                                              const std::string& search   = "") {
        std::string sql =
            "SELECT id, type, amount, category, description, date "
            "FROM transactions WHERE user_id=?";
        if (!month.empty())    sql += " AND strftime('%Y-%m', date)=?";
        if (!category.empty()) sql += " AND category=?";
        if (!search.empty())   sql += " AND (LOWER(description) LIKE ? OR LOWER(category) LIKE ?)";
        sql += " ORDER BY date DESC, id DESC;";

        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
            return {};

        int idx = 1;
        sqlite3_bind_int(stmt, idx++, userId);
        if (!month.empty())    sqlite3_bind_text(stmt, idx++, month.c_str(),    -1, SQLITE_STATIC);
        if (!category.empty()) sqlite3_bind_text(stmt, idx++, category.c_str(), -1, SQLITE_STATIC);
        if (!search.empty()) {
            std::string like = "%" + search + "%";
            sqlite3_bind_text(stmt, idx++, like.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, idx++, like.c_str(), -1, SQLITE_TRANSIENT);
        }

        std::vector<Transaction> results;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Transaction t;
            t.user_id     = userId;
            t.id          = sqlite3_column_int   (stmt, 0);
            t.type        = (const char*)sqlite3_column_text(stmt, 1);
            t.amount      = sqlite3_column_double (stmt, 2);
            t.category    = (const char*)sqlite3_column_text(stmt, 3);
            t.description = sqlite3_column_text(stmt, 4) ?
                            (const char*)sqlite3_column_text(stmt, 4) : "";
            t.date        = (const char*)sqlite3_column_text(stmt, 5);
            results.push_back(t);
        }
        sqlite3_finalize(stmt);
        return results;
    }

    // ── Dashboard Stats ───────────────────────────────────────────────────────
    DashboardStats getStats(int userId) {
        DashboardStats s;

        // Total income
        {
            const char* sql = "SELECT COALESCE(SUM(amount),0) FROM transactions WHERE user_id=? AND type='income';";
            sqlite3_stmt* stmt;
            sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
            sqlite3_bind_int(stmt, 1, userId);
            if (sqlite3_step(stmt) == SQLITE_ROW) s.totalIncome = sqlite3_column_double(stmt, 0);
            sqlite3_finalize(stmt);
        }
        // Total expense
        {
            const char* sql = "SELECT COALESCE(SUM(amount),0) FROM transactions WHERE user_id=? AND type='expense';";
            sqlite3_stmt* stmt;
            sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
            sqlite3_bind_int(stmt, 1, userId);
            if (sqlite3_step(stmt) == SQLITE_ROW) s.totalExpense = sqlite3_column_double(stmt, 0);
            sqlite3_finalize(stmt);
        }
        s.balance = s.totalIncome - s.totalExpense;
        s.savings = s.balance;

        // Category breakdown (expenses only)
        {
            const char* sql = R"(
                SELECT category, SUM(amount) as total
                FROM transactions WHERE user_id=? AND type='expense'
                GROUP BY category ORDER BY total DESC;
            )";
            sqlite3_stmt* stmt;
            sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
            sqlite3_bind_int(stmt, 1, userId);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                CategoryStat cs;
                cs.category = (const char*)sqlite3_column_text(stmt, 0);
                cs.amount   = sqlite3_column_double(stmt, 1);
                s.categoryBreakdown.push_back(cs);
            }
            sqlite3_finalize(stmt);
        }

        // Monthly data (last 12 months)
        {
            const char* sql = R"(
                SELECT strftime('%Y-%m', date) as mo,
                       SUM(CASE WHEN type='income'  THEN amount ELSE 0 END) as inc,
                       SUM(CASE WHEN type='expense' THEN amount ELSE 0 END) as exp
                FROM transactions WHERE user_id=?
                GROUP BY mo ORDER BY mo DESC LIMIT 12;
            )";
            sqlite3_stmt* stmt;
            sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
            sqlite3_bind_int(stmt, 1, userId);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                MonthStat ms;
                ms.month   = (const char*)sqlite3_column_text(stmt, 0);
                ms.income  = sqlite3_column_double(stmt, 1);
                ms.expense = sqlite3_column_double(stmt, 2);
                s.monthlyData.push_back(ms);
            }
            sqlite3_finalize(stmt);
            // Reverse so oldest month is first (for chart)
            std::reverse(s.monthlyData.begin(), s.monthlyData.end());
        }
        return s;
    }
};
