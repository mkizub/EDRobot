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

    DB->exec(R"SQL(CREATE TABLE MarketBlobs (
marketId INTEGER PRIMARY KEY,
updated REAL,
name TEXT,
type TEXT,
system TEXT,
data BLOB
)  WITHOUT ROWID )SQL");

    // 'id' is a system address, population and (controlling)faction for SQL requests
    DB->exec(R"SQL(CREATE TABLE Systems (
id INTEGER PRIMARY KEY,
name TEXT NOT NULL UNIQUE COLLATE NOCASE,
x REAL NOT NULL,
y REAL NOT NULL,
z REAL NOT NULL,
updated REAL,
eddn_updated REAL
)  WITHOUT ROWID
)SQL");

    DB->exec(R"SQL(CREATE TABLE SystemBlobs (
systemId INTEGER PRIMARY KEY,
data BLOB,
FOREIGN KEY(systemId) REFERENCES Systems(id)
)  WITHOUT ROWID )SQL");


    // megaships and carriers can be moved between systems
    // fleet and squadron carriers have code for fast lookup
    DB->exec(R"SQL(CREATE TABLE Megaships (
systemId INTEGER NOT NULL,
marketId INTEGER UNIQUE,
code TEXT UNIQUE COLLATE NOCASE,
name TEXT NOT NULL UNIQUE COLLATE NOCASE,
updated REAL,
data BLOB
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

inline void bind(SQLite::Statement* stmt, int idx, Timestamp ts) {
    if (!ts.time_since_epoch().count()) {
        stmt->bind(idx);
    } else {
        auto dur = std::chrono::duration_cast<std::chrono::duration<double>>(ts.time_since_epoch());
        stmt->bind(idx, dur.count());
    }
}

StarSystem loadStarSystem(std::string_view name) {
    static std::string sql = "SELECT id,x,y,z,updated,eddn_updated FROM Systems WHERE name = ?;";

    std::unique_lock<std::mutex> lock(dbMutex);
    auto* stmt = getStat(sql);
    if (!stmt)
        return {};

    try {
        stmt->bind(1, name.data());

        if (!stmt->executeStep())
            return {};

        return {
                .id = stmt->getColumn(0).getInt64(),
                .name = *name,
                .x = stmt->getColumn(1).getDouble(),
                .y = stmt->getColumn(2).getDouble(),
                .z = stmt->getColumn(3).getDouble(),
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
    auto* stmt = getStat(sql);
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
        stmt->bind(2, ss.name);
        stmt->bind(3, ss.x);
        stmt->bind(4, ss.y);
        stmt->bind(5, ss.z);
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
    auto* stmt = getStat(sql);
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

bool loadMegaShips(int64_t address, std::vector<MegashipJS> megaships) {
    if (!address)
        return false;

    static std::string sql = "SELECT systemId,marketId,code,name,updated,data FROM Megaships WHERE systemId = ?;";

    std::unique_lock<std::mutex> lock(dbMutex);
    try {
        SQLite::Statement* stmt = getStat(sql);
        if (!stmt)
            return false;

        stmt->bind(1, address);

        while (stmt->executeStep()) {
            MegashipJS& megaship = megaships.emplace_back();
            megaship.systemId = stmt->getColumn(0).getInt64();
            megaship.marketId = stmt->getColumn(1).getInt64();
            megaship.code = stmt->getColumn(2).getString();
            megaship.name = stmt->getColumn(3).getString();
            megaship.updated = ts_from_db(stmt->getColumn(4));
            if (auto column = stmt->getColumn(5); column.isBlob())
                decode_megaship_blob(megaship.blob, column.getBlob(), column.getBytes());
        }
        return true;
    } catch (SQLite::Exception& e) {
        LOG_ERROR("DB loadMegaShips SQL error[{}({})]: {}", e.getErrorCode(), e.getExtendedErrorCode(), e.getErrorStr());
        return false;
    }

}

bool loadMegaShip(MegashipJS& megaship) {
    if (!megaship.systemId || (megaship.code.empty() && megaship.name.empty()))
        return false;

    static std::string code_sql = "SELECT systemId,marketId,code,name,updated,data FROM Megaships WHERE code = ?;";
    static std::string name_sql = "SELECT systemId,marketId,code,name,updated,data FROM Megaships WHERE name = ?;";

    std::unique_lock<std::mutex> lock(dbMutex);
    try {
        SQLite::Statement* stmt {};
        if (!megaship.code.empty()) {
            stmt = getStat(code_sql);
            if (!stmt)
                return false;
            stmt->bind(1, megaship.code);
        } else {
            stmt = getStat(name_sql);
            if (!stmt)
                return false;
            stmt->bind(1, megaship.name);
        }

        if (!stmt->executeStep())
            return false;

        megaship.systemId = stmt->getColumn(0).getInt64();
        megaship.marketId = stmt->getColumn(1).getInt64();
        megaship.code     = stmt->getColumn(2).getString();
        megaship.name     = stmt->getColumn(3).getString();
        megaship.updated  = ts_from_db(stmt->getColumn(4));
        if (auto column = stmt->getColumn(5); column.isBlob())
            decode_megaship_blob(megaship.blob, column.getBlob(), column.getBytes());
        return true;
    } catch (SQLite::Exception& e) {
        LOG_ERROR("DB loadMegaShip({}) SQL error[{}({})]: {}", (!megaship.code.empty() ? "code" : "name"),
                  e.getErrorCode(), e.getExtendedErrorCode(), e.getErrorStr());
        return false;
    }
}

bool saveMegaShip(MegashipJS& megaship) {
    std::stringstream buffer;
    if (!encode_megaship_blob(megaship.blob, buffer))
        return false;

    static std::string sql = R"SQL(INSERT INTO Megaships(systemId,marketId,code,name,updated,data) VALUES (?,?,?,?,?,?)
ON CONFLICT DO UPDATE SET systemId=excluded.systemId,marketId=excluded.marketId,
code=excluded.code,name=excluded.name,updated=excluded.updated,data=excluded.data;)SQL";

    std::unique_lock<std::mutex> lock(dbMutex);
    auto* stmt = getStat(sql);
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
        stmt->bind(4, megaship.name);
        bind(stmt, 5, megaship.updated);
        stmt->bind(6, sv.data(), sv.size());
        stmt->executeStep();
        return true;
    } catch (SQLite::Exception& e) {
        LOG_ERROR("DB saveMegaShip SQL error[{}({})]: {}", e.getErrorCode(), e.getExtendedErrorCode(), e.getErrorStr());
    }
    return false;
}

bool loadMarket(MarketJS& market) {
    if (!market.marketId)
        return false;

    static std::string sql = "SELECT marketId,updated,name,type,system,data FROM MarketBlobs WHERE marketId = ?;";

    std::unique_lock<std::mutex> lock(dbMutex);
    auto* stmt = getStat(sql);
    if (!stmt)
        return false;

    try {
        stmt->bind(1, market.marketId);

        if (!stmt->executeStep())
            return false;

        market.marketId =    stmt->getColumn(0).getInt64();
        market.updated =     ts_from_db(stmt->getColumn(1));
        market.stationName = stmt->getColumn(2).getString();
        market.stationType = stmt->getColumn(3).getString();
        market.starSystem =  stmt->getColumn(4).getString();
        auto blobColumn = stmt->getColumn(5);
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
    if (!market.marketId)
        return false;

    std::stringstream buffer;
    if (!encode_market_blob(market, buffer))
        return false;

    static std::string sql = R"SQL(INSERT INTO MarketBlobs(marketId,updated,name,type,system,data) VALUES (?,?,?,?,?,?)
ON CONFLICT DO UPDATE SET
updated=excluded.updated,name=excluded.name,type=excluded.type,system=excluded.system,data=excluded.data;)SQL";

    std::unique_lock<std::mutex> lock(dbMutex);
    auto* stmt = getStat(sql);
    if (!stmt)
        return {};

    auto sv = buffer.view();
    try {
        stmt->bind(1, market.marketId);
        bind(stmt, 2, market.updated);
        stmt->bind(3, market.stationName);
        stmt->bind(4, market.stationType);
        stmt->bind(5, market.starSystem);
        stmt->bind(6, sv.data(), sv.size());
        stmt->executeStep();
        return true;
    } catch (SQLite::Exception& e) {
        LOG_ERROR("DB saveMarket SQL error[{}({})]: {}", e.getErrorCode(), e.getExtendedErrorCode(), e.getErrorStr());
    }
    return false;
}


} // namespace db
