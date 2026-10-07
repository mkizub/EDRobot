//
// Created by mkizub on 01.09.2026.
//

#ifndef PCH_H
#include "../pch.h"
#endif

#include "DB.h"
#include <sqlite3.h>
#include <SQLiteCpp/SQLiteCpp.h>

namespace db {

const int USER_VERSION = 1;
const char* GalaxyDB = "cache/Galaxy.db";
static std::mutex dbMutex;
static std::unique_ptr<SQLite::Database> DB;
static int db_user_version = 0;

static std::map<std::string, SQLite::Statement> gStatements;

struct StatRef {
    SQLite::Statement *stmt;

    StatRef() : stmt(nullptr) {}
    explicit StatRef(SQLite::Statement *st)
        : stmt(st)
    {
        if (stmt) {
            stmt->tryReset();
            stmt->clearBindings();
        }
    }
    StatRef(const StatRef&) = delete;
    StatRef(StatRef&& other) {
        std::swap(stmt, other.stmt);
    }
    ~StatRef() {
        try {
            if (stmt) {
                stmt->tryReset();
                //stmt->clearBindings();
            }
        } catch (...) {
            LOG_ERROR("SQLite error on statement reset");
        }
    }

    StatRef& operator=(StatRef&& other) {
        std::swap(stmt, other.stmt);
        return *this;
    }

    SQLite::Statement* operator->() {
        return stmt;
    }

    operator bool() {
        return stmt != nullptr;
    }
};

StatRef getStat(const std::string& sql) {
    auto* db = DB.get();
    if (!db)
        return StatRef(nullptr);
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
        return StatRef(stmt);
    } catch (SQLite::Exception& e) {
        LOG_ERROR("DB prepare statement SQL error[{}({})]: {}", e.getErrorCode(), e.getExtendedErrorCode(), e.getErrorStr());
    }
    return StatRef(nullptr);
}

bool create() {
    if (DB)
        return false;
    DB = std::make_unique<SQLite::Database>(GalaxyDB, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);


    DB->exec("PRAGMA auto_vacuum = FULL;");
    DB->exec("PRAGMA busy_timeout = 5000;");
    DB->exec("PRAGMA synchronous = " + Cfg.sqliteSynchronous());
    DB->exec("PRAGMA journal_mode = " + Cfg.sqliteJouralMode());
    DB->exec("PRAGMA journal_size_limit = " + std::to_string(Cfg.sqliteJouralSize()));

    DB->exec(R"SQL(CREATE TABLE MarketBlobs (
 marketId INTEGER PRIMARY KEY,
 systemId INTEGER NOT NULL,
 updated REAL,
 name TEXT,
 type TEXT,
 system TEXT,
 data BLOB,
 FOREIGN KEY(systemId) REFERENCES Systems(id) ON DELETE CASCADE
) WITHOUT ROWID;)SQL");

    DB->exec(R"SQL(CREATE TABLE Systems (
 id INTEGER PRIMARY KEY,
 name TEXT NOT NULL UNIQUE COLLATE NOCASE,
 x REAL NOT NULL,
 y REAL NOT NULL,
 z REAL NOT NULL,
 updated REAL,
 eddn_updated REAL
) WITHOUT ROWID;)SQL");

    DB->exec(R"SQL(CREATE TABLE SystemBlobs (
 systemId INTEGER PRIMARY KEY,
 data BLOB,
 FOREIGN KEY(systemId) REFERENCES Systems(id) ON DELETE CASCADE
) WITHOUT ROWID;)SQL");

    DB->exec(R"SQL(CREATE TABLE Megaships (
 systemId INTEGER NOT NULL,
 marketId INTEGER UNIQUE,
 code TEXT UNIQUE COLLATE NOCASE,
 anon BOOL NOT NULL,
 name TEXT NOT NULL COLLATE NOCASE,
 updated REAL,
 data BLOB
);)SQL");
    DB->exec(R"SQL(CREATE UNIQUE INDEX SystMegaship on Megaships (systemId, name COLLATE NOCASE) WHERE anon;)SQL");
    DB->exec(R"SQL(CREATE UNIQUE INDEX UniqMegaship on Megaships (name COLLATE NOCASE) WHERE NOT anon;)SQL");
    DB->exec(R"SQL(CREATE INDEX NameMegaship on Megaships (name COLLATE NOCASE);)SQL");

    DB->exec("INSERT INTO Systems (id, name, x, y, z) VALUES (10477373803, 'Sol', 0,0,0)");

    DB->exec("PRAGMA user_version = " + std::to_string(USER_VERSION));

    return true;
}

bool check_and_migrate() {
    if (!DB)
        return create();
    int version = db_user_version; //DB->execAndGet("PRAGMA user_version").getInt();
    if (version <= 0) {
        DB.reset();
        //std::filesystem::remove(GalaxyDB);
        //return create();
        return false;
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

    bool quick_check = false;
    {
        auto stmt = getStat("PRAGMA quick_check;");
        if (!stmt)
            return false;
        for (int r=0; stmt->executeStep(); r++) {
            auto err = stmt->getColumn(0).getString();
            LOG_INFO("Database integrity check: {}", err);
            if (r==0 && err=="ok") {
                quick_check = true;
                break;
            }
        }
    }
    if (!quick_check)
        return false;

//    DB->exec("PRAGMA synchronous = " + Cfg.sqliteSynchronous());
//    DB->exec("PRAGMA journal_mode = " + Cfg.sqliteJouralMode());
//    DB->exec("PRAGMA journal_size_limit = 16777216;");

    return true;
}

int pragma_callback(void* x, int y, char** z, char** w) {
    db_user_version = atoi(z[0]);
    return 0;
}

bool init() {
    if (DB)
        return true;

    LOG_INFO("Opening database Galaxy.db");
    try {
        if (std::filesystem::exists(GalaxyDB)) {
            sqlite3* handle;
            int err = sqlite3_open_v2(GalaxyDB, &handle, SQLITE_OPEN_READWRITE, nullptr);
            if (err == SQLITE_OK) {
                auto sql_sync = "PRAGMA synchronous = " + Cfg.sqliteSynchronous();
                auto sql_mode = "PRAGMA journal_mode = " + Cfg.sqliteJouralMode();
                auto sql_size = "PRAGMA journal_size_limit = " + std::to_string(Cfg.sqliteJouralSize());

                err = sqlite3_exec(handle, sql_sync.c_str(), nullptr, nullptr, nullptr);
                if (err != SQLITE_OK)
                    LOG_ERROR("DB error[{}] for: {}", err, sql_sync);
                err = sqlite3_exec(handle, sql_mode.c_str(), nullptr, nullptr, nullptr);
                if (err != SQLITE_OK)
                    LOG_ERROR("DB error[{}] for: {}", err, sql_mode);
                err = sqlite3_exec(handle, sql_size.c_str(), nullptr, nullptr, nullptr);
                if (err != SQLITE_OK)
                    LOG_ERROR("DB error[{}] for: {}", err, sql_size);
                err = sqlite3_exec(handle, "PRAGMA user_version", pragma_callback, nullptr, nullptr);
                if (err != SQLITE_OK)
                    LOG_ERROR("DB error[{}] for: PRAGMA user_version", err);
                else
                    LOG_INFO("DB user_version: {}", db_user_version);
            }
            DB = std::make_unique<SQLite::Database>(GalaxyDB, handle);
            if (!check_and_migrate())
                return false;
        } else {
            if (!create())
                return false;
        }
        checkpoint(CheckPoint::TRUNCATE);
    } catch (SQLite::Exception& e) {
        LOG_ERROR("DB init SQL error[{}({})]: {}", e.getErrorCode(), e.getExtendedErrorCode(), e.getErrorStr());
        return false;
    }
    LOG_INFO("Database Galaxy.db opened and optimized");

    return true;
}

bool shutdown() {
    if (DB) {
        gStatements.clear();
        DB.reset();
    }
    return true;
}

bool checkpoint(CheckPoint mode) {
    if (DB) {
        int eMode = SQLITE_CHECKPOINT_NOOP;
        switch (mode) {
        case CheckPoint::PASSIVE: eMode = SQLITE_CHECKPOINT_PASSIVE; break;
        case CheckPoint::FULL: eMode = SQLITE_CHECKPOINT_FULL; break;
        case CheckPoint::RESTART: eMode = SQLITE_CHECKPOINT_RESTART; break;
        case CheckPoint::TRUNCATE: eMode = SQLITE_CHECKPOINT_TRUNCATE; break;
        case CheckPoint::NOOP: eMode = SQLITE_CHECKPOINT_NOOP; break;
        }
        auto* handle = DB->getHandle();
        int pnLog {}, pnCkpt {};
        int err = sqlite3_wal_checkpoint_v2(handle, nullptr, eMode, &pnLog, &pnCkpt);
        if (err != SQLITE_OK) {
            LOG_ERROR("DB wal_checkpoint({}) SQL error[{}({})]: {}", magic_enum::enum_name(mode),
                      err, sqlite3_extended_errcode(handle), sqlite3_errmsg(handle));
        } else {
            LOG_INFO("DB wal_checkpoint({}): wal frames {}, checkpointed {}", magic_enum::enum_name(mode), pnLog,
                     pnCkpt);
            DB->exec("PRAGMA optimize=0x10002;");
        }
//        std::string res;
//        DB->exec("PRAGMA journal_mode=delete;");
//        res = DB->execAndGet("PRAGMA journal_mode;").getString();
//        DB->exec("PRAGMA journal_mode=wal;");
//        res = DB->execAndGet("PRAGMA journal_mode;").getString();
//        DB->exec("PRAGMA optimize=0x10002;");
    }
    return true;
}


inline Timestamp ts_from_db(SQLite::Column c) {
    if (c.isFloat()) {
        std::chrono::duration<double> dur_real{c.getDouble()};
        Timestamp tp{std::chrono::duration_cast<Timestamp::duration>(dur_real)};
        return tp;
    }
    if (c.isNull())
        return {};
    LOG_ERROR("Bad timestamp column type {}", c.getType());
    return {};
}

inline void bind(StatRef& stmt, int idx, Timestamp ts) {
    if (!ts) {
        stmt->bind(idx);
    } else {
        auto dur = std::chrono::duration_cast<std::chrono::duration<double>>(ts.time_since_epoch());
        stmt->bind(idx, dur.count());
    }
}

StarSystem loadStarSystem(std::string_view name) {
    static std::string sql = "SELECT id,x,y,z,updated,eddn_updated FROM Systems WHERE name = ?;";

    std::unique_lock<std::mutex> lock(dbMutex);
    auto stmt = getStat(sql);
    if (!stmt)
        return {};

    try {
        stmt->bind(1, name.data());

        if (!stmt->executeStep())
            return {};

        return {
                .id = stmt->getColumn(0).getInt64(),
                .name = *name,
                .coords = {
                    .x = stmt->getColumn(1).getDouble(),
                    .y = stmt->getColumn(2).getDouble(),
                    .z = stmt->getColumn(3).getDouble(),
                    },
                .updated = ts_from_db(stmt->getColumn(4)),
                .eddn_updated = ts_from_db(stmt->getColumn(5)),
        };
    } catch (SQLite::Exception& e) {
        LOG_ERROR("DB loadStarSystem(name) SQL error[{}({})]: {}", e.getErrorCode(), e.getExtendedErrorCode(), e.getErrorStr());
        return {};
    }
}

StarSystem loadStarSystem(int64_t address) {
    static std::string sql = "SELECT name,x,y,z,updated,eddn_updated FROM Systems WHERE id = ?;";

    std::unique_lock<std::mutex> lock(dbMutex);
    auto stmt = getStat(sql);
    if (!stmt)
        return {};

    try {
        stmt->bind(1, address);

        if (!stmt->executeStep())
            return {};

        return {
                .id = address,
                .name = stmt->getColumn(0).getString(),
                .coords = {
                        .x = stmt->getColumn(1).getDouble(),
                        .y = stmt->getColumn(2).getDouble(),
                        .z = stmt->getColumn(3).getDouble(),
                        },
                .updated = ts_from_db(stmt->getColumn(4)),
                .eddn_updated = ts_from_db(stmt->getColumn(5)),
        };
    } catch (SQLite::Exception& e) {
        LOG_ERROR("DB loadStarSystem(address) SQL error[{}({})]: {}", e.getErrorCode(), e.getExtendedErrorCode(), e.getErrorStr());
        return {};
    }
}

bool loadStarSystemBlob(StarSystemJS& ss, const std::string& system_name, int64_t address) {
    if (!address || system_name.empty())
        return false;

    static std::string sql = "SELECT data FROM SystemBlobs WHERE systemId = ?;";

    std::unique_lock<std::mutex> lock(dbMutex);
    auto stmt = getStat(sql);
    if (!stmt)
        return false;

    try {
        stmt->bind(1, address);

        if (!stmt->executeStep())
            return false;

        auto column = stmt->getColumn(0);
        if (!column.isBlob())
            return false;
        return decode_system_blob(ss, system_name, address, column.getBlob(), column.getBytes());
    } catch (SQLite::Exception& e) {
        LOG_ERROR("DB loadStarSystemBlob SQL error[{}({})]: {}", e.getErrorCode(), e.getExtendedErrorCode(), e.getErrorStr());
        return false;
    }
}


bool saveStarSystem(const StarSystem& ss) {
    static std::string sql = R"SQL(INSERT INTO Systems(id,name,x,y,z,updated,eddn_updated) VALUES (?,?,?,?,?,?,?)
ON CONFLICT DO UPDATE SET
x=excluded.x, y=excluded.y, z=excluded.z, updated=excluded.updated, eddn_updated=excluded.eddn_updated; )SQL";

    if (!ss.id || ss.name.empty())
        return false;
    if (ss.coords.empty() && ss.name != "Sol")
        return false;
    std::unique_lock<std::mutex> lock(dbMutex);
    auto stmt = getStat(sql);
    if (!stmt)
        return {};

    try {
        stmt->bind(1, ss.id);
        stmt->bind(2, ss.name);
        stmt->bind(3, ss.coords.x);
        stmt->bind(4, ss.coords.y);
        stmt->bind(5, ss.coords.z);
        bind(stmt, 6, ss.updated);
        bind(stmt, 7, ss.eddn_updated);
        stmt->executeStep();
        return true;
    } catch (SQLite::Exception& e) {
        LOG_ERROR("DB saveStarSystem SQL error[{}({})]: {}", e.getErrorCode(), e.getExtendedErrorCode(), e.getErrorStr());
    }
    return false;
}

bool saveStarSystemBlob(StarSystemJS& ss, const std::string& system_name, int64_t address) {
    std::stringstream buffer;
    if (!encode_system_blob(ss, system_name, address, buffer))
        return false;

    static std::string sql = R"SQL(INSERT INTO SystemBlobs(systemId,data) VALUES (?,?)
ON CONFLICT DO UPDATE SET data=excluded.data;)SQL";

    std::unique_lock<std::mutex> lock(dbMutex);
    auto stmt = getStat(sql);
    if (!stmt)
        return {};

    auto sv = buffer.view();
    try {
        stmt->bind(1, address);
        stmt->bind(2, sv.data(), sv.size());
        stmt->executeStep();
        return true;
    } catch (SQLite::Exception& e) {
        LOG_ERROR("DB saveStarSystemBlob SQL error[{}({})]: {}", e.getErrorCode(), e.getExtendedErrorCode(), e.getErrorStr());
    }
    return false;
}

bool loadMegaShips(int64_t address, std::vector<MegashipJS>& megaships) {
    if (!address)
        return false;

    static std::string sql = "SELECT systemId,marketId,code,anon,name,updated,data FROM Megaships WHERE systemId = ?;";

    std::unique_lock<std::mutex> lock(dbMutex);
    try {
        auto stmt = getStat(sql);
        if (!stmt)
            return false;

        stmt->bind(1, address);

        while (stmt->executeStep()) {
            MegashipJS& megaship = megaships.emplace_back();
            megaship.systemId = stmt->getColumn(0).getInt64();
            megaship.marketId = stmt->getColumn(1).getInt64();
            megaship.code = stmt->getColumn(2).getString();
            megaship.anon = stmt->getColumn(3).getInt() != 0;
            megaship.name = stmt->getColumn(4).getString();
            megaship.updated = ts_from_db(stmt->getColumn(5));
            if (auto column = stmt->getColumn(6); column.isBlob())
                decode_megaship_blob(megaship.blob, column.getBlob(), column.getBytes());
        }
        return true;
    } catch (SQLite::Exception& e) {
        LOG_ERROR("DB loadMegaShips SQL error[{}({})]: {}", e.getErrorCode(), e.getExtendedErrorCode(), e.getErrorStr());
        return false;
    }

}

bool loadMegaShip(MegashipJS& megaship, bool silent) {
    if (megaship.code.empty() && megaship.name.empty() && megaship.marketId==0)
        return false;
    assert (megaship.code.empty() || megaship.code.size() == 7 || megaship.code.size() == 4);

    static std::string code_sql = "SELECT systemId,marketId,code,anon,name,updated,data FROM Megaships WHERE code = ?;";
    static std::string mkid_sql = "SELECT systemId,marketId,code,anon,name,updated,data FROM Megaships WHERE marketId = ?;";
    static std::string name_sql = "SELECT systemId,marketId,code,anon,name,updated,data FROM Megaships WHERE name = ? AND NOT anon;";
    static std::string syst_sql = "SELECT systemId,marketId,code,anon,name,updated,data FROM Megaships WHERE name = ? AND systemId = ? AND anon;";

    std::unique_lock<std::mutex> lock(dbMutex);
    try {
        StatRef stmt;

        bool loaded = false;
        if (!megaship.code.empty()) {
            stmt = getStat(code_sql);
            if (stmt) {
                stmt->bind(1, megaship.code);
                loaded = stmt->executeStep();
            }
        }
        if (!loaded && megaship.marketId != 0) {
            stmt = getStat(mkid_sql);
            if (stmt) {
                stmt->bind(1, megaship.marketId);
                loaded = stmt->executeStep();
            }
        }
        if (!loaded && !megaship.name.empty()) {
            stmt = getStat(name_sql);
            if (stmt) {
                stmt->bind(1, megaship.name);
                loaded = stmt->executeStep();
            }
        }
        if (!loaded && !megaship.name.empty()) {
            stmt = getStat(syst_sql);
            if (stmt) {
                stmt->bind(1, megaship.name);
                stmt->bind(2, megaship.systemId);
                loaded = stmt->executeStep();
            }
        }
        if (!loaded) {
            if (!silent) {
                LOG_ERROR("DB loadMegaShip failed for: code='{}', marketId={}, name='{}'",
                          megaship.code, megaship.marketId, megaship.name);
            }
            return false;
        }

        megaship.systemId = stmt->getColumn(0).getInt64();
        megaship.marketId = stmt->getColumn(1).getInt64();
        megaship.code     = stmt->getColumn(2).getString();
        megaship.anon     = stmt->getColumn(3).getInt() != 0;
        megaship.name     = stmt->getColumn(4).getString();
        megaship.updated  = ts_from_db(stmt->getColumn(5));
        assert (megaship.code.empty() || megaship.code.size() == 7 || megaship.code.size() == 4);
        if (auto column = stmt->getColumn(6); column.isBlob())
            decode_megaship_blob(megaship.blob, column.getBlob(), column.getBytes());
        return true;
    } catch (SQLite::Exception& e) {
        LOG_ERROR("DB loadMegaShip({}) SQL error[{}({})]: {}", (!megaship.code.empty() ? "code" : "name"),
                  e.getErrorCode(), e.getExtendedErrorCode(), e.getErrorStr());
        return false;
    }
}

bool saveMegaShip(MegashipJS& megaship) {
    if (!megaship.systemId || megaship.name.empty())
        return false;
    if (megaship.anon && megaship.marketId)
        megaship.marketId = 0;
    std::stringstream buffer;
    if (!encode_megaship_blob(megaship.blob, buffer))
        return false;
//    if (megaship.code == "K5N-3VG" && megaship.marketId == 0)
//        LOG_WARNING("K5N-3VG");

    static std::string syst_sql = "SELECT rowid FROM Megaships WHERE name=? AND systemId = ? AND anon=1";
    static std::string name_sql = "SELECT rowid FROM Megaships WHERE name=? AND anon=0";
    static std::string code_sql = "SELECT rowid FROM Megaships WHERE code=?";
    static std::string mkid_sql = "SELECT rowid FROM Megaships WHERE marketId=?";

    static std::string insert_sql = "INSERT INTO Megaships(systemId,marketId,code,anon,name,updated,data) VALUES (?,?,?,?,?,?,?);";
    static std::string update_sql = "UPDATE Megaships SET systemId=?,marketId=?,code=?,anon=?,name=?,updated=?,data=? WHERE rowid=?;";

    std::unique_lock<std::mutex> lock(dbMutex);

    int64_t rowid = 0;

    bool found = false;
    if (!found && megaship.anon) {
        StatRef stmt = getStat(syst_sql);
        if (stmt) {
            stmt->bind(1, megaship.name);
            stmt->bind(2, megaship.systemId);
            found = stmt->executeStep();
            if (found)
                rowid = stmt->getColumn(0).getInt64();
        }
    }
    if (!found && !megaship.anon) {
        StatRef stmt = getStat(name_sql);
        if (stmt) {
            stmt->bind(1, megaship.name);
            found = stmt->executeStep();
            if (found)
                rowid = stmt->getColumn(0).getInt64();
        }
    }
    if (!found && !megaship.code.empty()) {
        StatRef stmt = getStat(code_sql);
        if (stmt) {
            stmt->bind(1, megaship.code);
            found = stmt->executeStep();
            if (found)
                rowid = stmt->getColumn(0).getInt64();
        }
    }
    if (!found && megaship.marketId && !megaship.anon) {
        StatRef stmt = getStat(mkid_sql);
        if (stmt) {
            stmt->bind(1, megaship.marketId);
            found = stmt->executeStep();
            if (found)
                rowid = stmt->getColumn(0).getInt64();
        }
    }

    auto stmt = getStat(found ? update_sql : insert_sql);
    if (!stmt)
        return {};

    auto sv = buffer.view();
    try {
        stmt->bind(1, megaship.systemId);
        if (!megaship.marketId)
            stmt->bind(2);
        else
            stmt->bind(2, megaship.marketId);
        if (megaship.code.empty())
            stmt->bind(3);
        else
            stmt->bind(3, megaship.code);
        stmt->bind(4, megaship.anon ? 1 : 0);
        stmt->bind(5, megaship.name);
        bind(stmt, 6, megaship.updated);
        stmt->bind(7, sv.data(), sv.size());
        if (found)
            stmt->bind(8, rowid);
        int count = stmt->exec();
        if (!count) {
            if (found)
                LOG_ERROR("DB saveMegaShip failed to update: code='{}', name={}, systemId='{}'",
                          megaship.code, megaship.name, megaship.systemId);
            else
                LOG_ERROR("DB saveMegaShip failed to insert: code='{}', name={}, systemId='{}'",
                          megaship.code, megaship.name, megaship.systemId);
            return false;
        }
        return true;
    } catch (SQLite::Exception& e) {
        LOG_ERROR("DB saveMegaShip SQL error[{}({})]: {}", e.getErrorCode(), e.getExtendedErrorCode(), e.getErrorStr());
    }
    return false;
}

bool removeMegaShip(MegashipJS& megaship) {
    static std::string code_sql = "DELETE FROM Megaships WHERE code = ?";
    static std::string name_sql = "DELETE FROM Megaships WHERE name = ? AND systemId = ?";

//    if (megaship.code == "K5N-3VG" || megaship.name == "909 VSO K5N-3VG")
//        LOG_WARNING("K5N-3VG");

    std::unique_lock<std::mutex> lock(dbMutex);
    try {
        bool removed = false;
        if (!megaship.code.empty()) {
            StatRef stmt = getStat(code_sql);
            if (stmt) {
                stmt->bind(1, megaship.code);
                removed = stmt->executeStep();
            }
        }
        if (!removed && megaship.systemId && !megaship.name.empty()) {
            StatRef stmt = getStat(name_sql);
            if (stmt) {
                stmt->bind(1, megaship.name);
                stmt->bind(2, megaship.systemId);
                removed = stmt->executeStep();
            }
        }
        if (!removed) {
            LOG_ERROR("DB removeMegaShip failed for: code='{}', name={}, systemId='{}'",
                      megaship.code, megaship.name, megaship.systemId);
        }
        return removed;
    } catch (SQLite::Exception& e) {
        LOG_ERROR("DB loadMegaShip({}) SQL error[{}({})]: {}", (!megaship.code.empty() ? "code" : "name"),
                  e.getErrorCode(), e.getExtendedErrorCode(), e.getErrorStr());
        return false;
    }
}

bool loadMarket(MarketJS& market) {
    if (!market.marketId)
        return false;

    static std::string sql = "SELECT marketId,systemId,updated,name,type,system,data FROM MarketBlobs WHERE marketId = ?;";

    std::unique_lock<std::mutex> lock(dbMutex);
    auto stmt = getStat(sql);
    if (!stmt)
        return false;

    try {
        stmt->bind(1, market.marketId);

        if (!stmt->executeStep())
            return false;

        market.marketId =    stmt->getColumn(0).getInt64();
        market.systemId =    stmt->getColumn(1).getInt64();
        market.updated =     ts_from_db(stmt->getColumn(2));
        market.stationName = stmt->getColumn(3).getString();
        market.stationType = stmt->getColumn(4).getString();
        market.starSystem =  stmt->getColumn(5).getString();
        auto blobColumn = stmt->getColumn(6);
        if (blobColumn.isNull()) {
            market.commodities.clear();
            return true;
        }
        if (!blobColumn.isBlob())
            return false;
        return decode_market_blob(market, blobColumn.getBlob(), blobColumn.getBytes());
    } catch (SQLite::Exception& e) {
        LOG_ERROR("DB loadMarket SQL error[{}({})]: {}", e.getErrorCode(), e.getExtendedErrorCode(), e.getErrorStr());
        return false;
    }
}

bool saveMarket(MarketJS& market) {
    if (!market.marketId || !market.systemId)
        return false;

    std::stringstream buffer;
    if (!encode_market_blob(market, buffer))
        return false;

    static std::string sql = R"SQL(INSERT INTO MarketBlobs(marketId,systemId,updated,name,type,system,data) VALUES (?,?,?,?,?,?,?)
ON CONFLICT DO UPDATE SET systemId=excluded.systemId,
updated=excluded.updated,name=excluded.name,type=excluded.type,system=excluded.system,data=excluded.data;)SQL";

    std::unique_lock<std::mutex> lock(dbMutex);
    auto stmt = getStat(sql);
    if (!stmt)
        return {};

    auto sv = buffer.view();
    try {
        stmt->bind(1, market.marketId);
        stmt->bind(2, market.systemId);
        bind(stmt, 3, market.updated);
        stmt->bind(4, market.stationName);
        stmt->bind(5, market.stationType);
        stmt->bind(6, market.starSystem);
        stmt->bind(7, sv.data(), sv.size());
        stmt->executeStep();
        return true;
    } catch (SQLite::Exception& e) {
        LOG_ERROR("DB saveMarket SQL error[{}({})]: {}", e.getErrorCode(), e.getExtendedErrorCode(), e.getErrorStr());
    }
    return false;
}


} // namespace db
