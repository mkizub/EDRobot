//
// Created by mkizub on 01.09.2026.
//

#include "../pch.h"

#include "DB.h"
#include <sqlite3.h>
#include <SQLiteCpp/SQLiteCpp.h>

namespace db {

const int USER_VERSION = 1;
const char* GalaxyDB = "cache/Galaxy.db";
const char* MarketsDB = "cache/Markets.db";
static std::mutex dbMutex;
static std::unique_ptr<SQLite::Database> DB;

static std::map<std::string, SQLite::Statement> gStatements;

SQLite::Statement* getStat(const std::string& sql) {
    auto* db = DB.get();
    if (!db)
        return nullptr;
    try {
        SQLite::Statement *stmt = nullptr;
        auto it = gStatements.find(sql);
        if (it != gStatements.end()) {
            stmt = &it->second;
            stmt->reset();
            stmt->clearBindings();
        } else {
            auto p = gStatements.emplace(sql, SQLite::Statement(*db, sql));
            stmt = &p.first->second;
        }
        return stmt;
    } catch (SQLite::Exception& e) {
        LOG_ERROR("DB prepare statement SQL error[{}({})]: {}", e.getErrorCode(), e.getExtendedErrorCode(), e.getErrorStr());
    }
    return nullptr;
}

bool create() {
    if (DB)
        return false;
    DB = std::make_unique<SQLite::Database>(GalaxyDB, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);

    DB->exec(R"SQL(CREATE TABLE Allegiance (id INTEGER PRIMARY KEY, name TEXT NOT NULL) )SQL");
    DB->exec(R"SQL(INSERT INTO Allegiance (name) VALUES
('Alliance'),('Empire'),('Federation'),('Frontline Solutions'),('Guardian'),('Independent'),('Pilots Federation'),('Thargoid')
)SQL");

    DB->exec(R"SQL(CREATE TABLE Government (id INTEGER PRIMARY KEY, name TEXT NOT NULL) )SQL");
    DB->exec(R"SQL(INSERT INTO Government (name) VALUES
("None"),("Anarchy"),("Communism"),("Confederacy"),("Cooperative"),("Corporate"),("Democracy"),("Dictatorship"),
("Engineer"),("Feudal"),("Megaconstruction"),("Patronage"),("Prison"),("Prison Colony"),("Private Ownership"),("Theocracy")
)SQL");

    DB->exec(R"SQL(CREATE TABLE Factions (
id INTEGER PRIMARY KEY,
name TEXT NOT NULL,
allegiance INTEGER,
government INTEGER,
FOREIGN KEY(allegiance) REFERENCES Allegiance(id),
FOREIGN KEY(government) REFERENCES Government(id)
) )SQL");

    // 'table' is 1:Systems, 2:Bodies, 3:Stations
    DB->exec(R"SQL(CREATE TABLE JsBlobs (
id INTEGER PRIMARY KEY,
tbl INTEGER,
data BLOB
) )SQL");


    // 'id' is a system address, population and (controlling)faction for SQL requests
    DB->exec(R"SQL(CREATE TABLE Systems (
id INTEGER PRIMARY KEY,
name TEXT NOT NULL UNIQUE COLLATE NOCASE,
x REAL NOT NULL,
y REAL NOT NULL,
z REAL NOT NULL,
population INTEGER,
faction INTEGER,
js INTEGER,
FOREIGN KEY(faction) REFERENCES Factions(id),
FOREIGN KEY(js) REFERENCES JsBlobs(id)
)  WITHOUT ROWID
)SQL");

    // 'id' = (bodyId << 55) | (systemId)
    // types: S-star, P-planet, R-ring, B-Barycenter, A-AsteroidCluster, NULL for unknown (TypeNav:Body)
    DB->exec(R"SQL(CREATE TABLE Bodies (
id INTEGER PRIMARY KEY,
systemId INTEGER NOT NULL,
bodyId INTEGER NOT NULL,
type TEXT,
name TEXT NOT NULL,
js INTEGER,
FOREIGN KEY(js) REFERENCES JsBlobs(id)
)  WITHOUT ROWID
)SQL");

    // 'bodyId' - station bodyId, NULL for installations
    // 'parentId' - planet (star, barycenter, etc) bodyId
    // stations include megaships and carriers, they can be moved between systems
    DB->exec(R"SQL(CREATE TABLE Stations (
systemId INTEGER NOT NULL,
marketId INTEGER UNIQUE,
type TEXT,
name TEXT NOT NULL,
bodyId INTEGER,
parentId INTEGER,
updated REAL,
js INTEGER,
FOREIGN KEY(js) REFERENCES JsBlobs(id)
)
)SQL");

    // usually signals are created temporary, but in cased we'll want to save them add this table
    DB->exec(R"SQL(CREATE TABLE Signals (
systemId INTEGER NOT NULL,
type TEXT,
name TEXT,
eddn BLOB
)
)SQL");

    DB->exec("INSERT INTO Systems (id, name, x, y, z) VALUES (10477373803, 'Sol', 0,0,0)");

    DB->exec(std::format("PRAGMA user_version = {};", USER_VERSION));

    return true;
}

bool check_and_migrate() {
    if (!DB)
        return create();
    int version = DB->execAndGet("PRAGMA user_version;").getInt();
    if (version <= 0) {
        DB.reset();
        std::filesystem::remove(GalaxyDB);
        return create();
    }
    if (version > USER_VERSION) {
        LOG_ERROR("Database {} has version {}, but only version {} is known to EDRobot {}", GalaxyDB, version, USER_VERSION, EDROBOT_VERSION);
        LOG_ERROR("Remove {} to re-create DB or run newer version of EDRobot", GalaxyDB);
        return false;
    }
    if (version < USER_VERSION) {
        LOG_ERROR("Database {} has version {}, migration to version {} is not implemented", GalaxyDB, version, USER_VERSION);
        return false;
    }
    return true;
}

bool init() {
    if (DB)
        return true;

    LOG_INFO("Opening database Galaxy.db");
    try {
        if (std::filesystem::exists(GalaxyDB)) {
            DB = std::make_unique<SQLite::Database>(GalaxyDB, SQLITE_OPEN_READWRITE);
            if (!check_and_migrate())
                return false;
        } else {
            if (!create())
                return false;
        }
    } catch (SQLite::Exception& e) {
        LOG_ERROR("DB init SQL error[{}({})]: {}", e.getErrorCode(), e.getExtendedErrorCode(), e.getErrorStr());
        return false;
    }

    return true;
}

bool shutdown() {
    if (DB) {
        gStatements.clear();
        DB.reset();
    }
    return true;
}

StarSystem loadStarSystem(std::string_view name) {
    static std::string sql = "SELECT id,x,y,z,js FROM Systems WHERE name = ?;";

    std::unique_lock<std::mutex> lock(dbMutex);
    auto* stmt = getStat(sql);
    if (!stmt)
        return {};

    try {
        stmt->bind(1, name.data(), (int) name.length());

        if (!stmt->executeStep())
            return {};

        return {
                .id = stmt->getColumn(0).getInt64(),
                .name = *name,
                .x = stmt->getColumn(1).getDouble(),
                .y = stmt->getColumn(2).getDouble(),
                .z = stmt->getColumn(3).getDouble(),
                .blobId = stmt->getColumn(4).getInt64(),
        };
    } catch (SQLite::Exception& e) {
        LOG_ERROR("DB loadStarSystem SQL error[{}({})]: {}", e.getErrorCode(), e.getExtendedErrorCode(), e.getErrorStr());
        return {};
    }
}

StarSystem loadStarSystem(int64_t address) {
    static std::string sql = "SELECT name,x,y,z,js FROM Systems WHERE id = ?;";

    std::unique_lock<std::mutex> lock(dbMutex);
    auto* stmt = getStat(sql);
    if (!stmt)
        return {};

    try {
        stmt->bind(1, address);

        if (!stmt->executeStep())
            return {};

        return {
                .id = address,
                .name = stmt->getColumn(0).getString(),
                .x = stmt->getColumn(1).getDouble(),
                .y = stmt->getColumn(2).getDouble(),
                .z = stmt->getColumn(3).getDouble(),
                .blobId = stmt->getColumn(4).getInt64(),
        };
    } catch (SQLite::Exception& e) {
        LOG_ERROR("DB loadStarSystem SQL error[{}({})]: {}", e.getErrorCode(), e.getExtendedErrorCode(), e.getErrorStr());
        return {};
    }
}

bool saveStarSystem(const StarSystem& ss) {
    static std::string sql = R"SQL(INSERT INTO Systems(id,name,x,y,z) VALUES (?,?,?,?,?)
                                     ON CONFLICT DO UPDATE SET x=excluded.x, y=excluded.y, z=excluded.z; )SQL";

    if (!ss.id || ss.name.empty())
        return false;
    if (ss.x*ss.x + ss.y*ss.y + ss.z*ss.z < 1.e-3) {
        if (ss.name != "Sol")
            return false;
    }
    std::unique_lock<std::mutex> lock(dbMutex);
    auto* stmt = getStat(sql);
    if (!stmt)
        return {};

    try {
        stmt->bind(1, ss.id);
        stmt->bind(2, ss.name.data(), (int)ss.name.length());
        stmt->bind(3, ss.x);
        stmt->bind(4, ss.y);
        stmt->bind(5, ss.z);
        return stmt->executeStep();
    } catch (SQLite::Exception& e) {
        LOG_ERROR("DB loadStarSystem SQL error[{}({})]: {}", e.getErrorCode(), e.getExtendedErrorCode(), e.getErrorStr());
    }
    return false;
}


//void loadJsBlobs(int64_t blobId) {
//    std::unique_lock<std::mutex> lock(dbMutex);
//    sqlite3_stmt* stmt = stmtSelectJsBlob;
//    if (!DB || !stmt)
//        return {};
//
//    int err = sqlite3_bind_int64(stmt, 1, blobId);
//    if (err != SQLITE_OK) {
//        LOG_ERROR("loadJsBlobs SQL error[{}] with SQL statement prepare", err);
//        return {};
//    }
//
//    err = sqlite3_step(stmt);
//    if (err != SQLITE_ROW) {
//        LOG_ERROR("loadJsBlobs SQL error[{}] with SQL statement step", err);
//        return {};
//    }
//
//    std::string name = (const char*)sqlite3_column_text(stmt, 0);
//    double x = sqlite3_column_int64(stmt, 1);
//    double y = sqlite3_column_int64(stmt, 2);
//    double z = sqlite3_column_int64(stmt, 3);
//    int64_t blob = sqlite3_column_int64(stmt, 4);
//
//    return std::make_shared<gal::StarSystem>(address, name, x,y,z, blob);
//}


} // namespace db
