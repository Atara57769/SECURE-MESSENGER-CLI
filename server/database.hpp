#pragma once

#include <sqlite3.h>
#include <string>
#include <vector>
#include <stdexcept>
#include <memory>

namespace repository {

class SqliteDb {
private:
    sqlite3* db_ = nullptr;

public:
    SqliteDb(const std::string& path) {
        if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
            std::string err = db_ ? sqlite3_errmsg(db_) : "allocation error";
            throw std::runtime_error("Failed to open database: " + err);
        }
        execute("PRAGMA foreign_keys = ON;");
    }

    ~SqliteDb() {
        if (db_) {
            sqlite3_close(db_);
        }
    }

    // Disable copy semantic to ensure single ownership
    SqliteDb(const SqliteDb&) = delete;
    SqliteDb& operator=(const SqliteDb&) = delete;

    void execute(const std::string& sql) {
        char* err_msg = nullptr;
        if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg) != SQLITE_OK) {
            std::string err = err_msg ? err_msg : "unknown error";
            if (err_msg) sqlite3_free(err_msg);
            throw std::runtime_error("SQL error: " + err + " (Query: " + sql + ")");
        }
    }

    struct Statement {
        sqlite3_stmt* stmt = nullptr;
        sqlite3* db = nullptr;

        Statement(sqlite3* db_ptr, const std::string& sql) : db(db_ptr) {
            if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
                throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db)));
            }
        }

        ~Statement() {
            if (stmt) {
                sqlite3_finalize(stmt);
            }
        }

        // Disable copy
        Statement(const Statement&) = delete;
        Statement& operator=(const Statement&) = delete;

        // Allow move
        Statement(Statement&& other) noexcept : stmt(other.stmt), db(other.db) {
            other.stmt = nullptr;
        }

        Statement& operator=(Statement&& other) noexcept {
            if (this != &other) {
                if (stmt) sqlite3_finalize(stmt);
                stmt = other.stmt;
                db = other.db;
                other.stmt = nullptr;
            }
            return *this;
        }

        void bind(int idx, int val) {
            if (sqlite3_bind_int(stmt, idx, val) != SQLITE_OK) {
                throw std::runtime_error("Failed to bind int: " + std::string(sqlite3_errmsg(db)));
            }
        }

        void bind(int idx, int64_t val) {
            if (sqlite3_bind_int64(stmt, idx, val) != SQLITE_OK) {
                throw std::runtime_error("Failed to bind int64: " + std::string(sqlite3_errmsg(db)));
            }
        }

        void bind(int idx, const std::string& val) {
            if (sqlite3_bind_text(stmt, idx, val.data(), val.size(), SQLITE_TRANSIENT) != SQLITE_OK) {
                throw std::runtime_error("Failed to bind text: " + std::string(sqlite3_errmsg(db)));
            }
        }

        void bind_null(int idx) {
            if (sqlite3_bind_null(stmt, idx) != SQLITE_OK) {
                throw std::runtime_error("Failed to bind null: " + std::string(sqlite3_errmsg(db)));
            }
        }

        bool step() {
            int rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW) return true;
            if (rc == SQLITE_DONE) return false;
            throw std::runtime_error("SQL step error: " + std::string(sqlite3_errmsg(db)));
        }

        int get_int(int col) { return sqlite3_column_int(stmt, col); }
        int64_t get_int64(int col) { return sqlite3_column_int64(stmt, col); }
        std::string get_string(int col) {
            const unsigned char* text = sqlite3_column_text(stmt, col);
            int bytes = sqlite3_column_bytes(stmt, col);
            return text ? std::string(reinterpret_cast<const char*>(text), bytes) : "";
        }
        bool is_null(int col) { return sqlite3_column_type(stmt, col) == SQLITE_NULL; }
        
        void reset() {
            sqlite3_reset(stmt);
            sqlite3_clear_bindings(stmt);
        }
    };

    Statement prepare(const std::string& sql) {
        return Statement(db_, sql);
    }

    sqlite3* get_raw_db() { return db_; }
};

} // namespace repository
