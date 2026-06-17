#include "database.hpp"
#include "repository.hpp"
#include "routes.hpp"
#include "crypto.hpp"
#include "auth.hpp"
#include <httplib.h>
#include <iostream>

int main() {
    try {
        std::cout << "Starting Secure Messenger C++ Server..." << std::endl;

        // Initialize cryptography keys (AES and JWT)
        crypto::init_key();
        auth::init_jwt();

        // Initialize SQLite connection and Repository
        // Keep DB file name consistent with python (messenger.db), allow test override
        const char* db_path_env = std::getenv("MESSENGER_DB_PATH");
        std::string db_file = db_path_env ? db_path_env : "messenger.db";
        repository::SqliteDb db(db_file);
        repository::Repository repo(db);
        repo.create_tables();
        std::cout << "Database tables validated successfully." << std::endl;

        // Initialize httplib server
        httplib::Server svr;

        // Mount REST and SSE routes
        routes::setup_routes(svr, repo);

        std::cout << "Server listening on http://0.0.0.0:8000" << std::endl;
        if (!svr.listen("0.0.0.0", 8000)) {
            std::cerr << "Error: Failed to bind or listen on port 8000" << std::endl;
            return 1;
        }

    } catch (const std::exception& e) {
        std::cerr << "Server crashed with critical exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
