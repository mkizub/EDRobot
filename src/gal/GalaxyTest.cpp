//
// Created by mkizub on 25.09.2026.
//

#ifndef PCH_H
#include "../pch.h"
#endif

#include "Galaxy.h"
#include "../DB/DB.h"

#include <fcntl.h>
#include <io.h>
#include <zlib.h>

namespace gal {

namespace fs = std::filesystem;

struct TestBackdor {
    static fs::path initCfgForTests(std::string_view dirPath);
    static spGameEvent parseEvent(Timestamp ts_now, std::string& line);
};

fs::path TestBackdor::initCfgForTests(std::string_view dirPath) {
    Cfg.load();
    Cfg.loadCommodityDatabase();
    Cfg.mNetworkDisabled = true;

    fs::path dir;
    if (!dirPath.empty()) {
        dir = dirPath;
        Cfg.mEDLogsPath = dir.native();
    } else {
        dir = Cfg.mEDLogsPath;
    }

    if (!db::init()) {
        LOG_ERROR("Cannot init database");
        return "";
    }

    return dir;
}

spGameEvent TestBackdor::parseEvent(Timestamp ts_now, std::string& line) {
    return Cfg.parseEvent(ts_now, line);
}


bool verifyStarSystem(db::StarSystemJS& ss) {

    std::string carrier_code;
    std::string carrier_name;
    TypeNav tp = NavType::checkCarrierName(ss.name, &carrier_code, &carrier_name);
    if (tp != TypeNav::Other)
        LOG_ERROR("Star system '{}' looks like carrier name '{}' / '{}'", ss.name, carrier_code, carrier_name);

    std::map<int,std::string> used;
    for (auto& body : ss.bodies) {
        TypeNav tn = toTypeNav(body.type);
        if (tn == TypeNav::Star && !body.name.starts_with(ss.name))
            LOG_ERROR(R"({{ star:"{}", id:{}, system:"{}" }}, // unique star name)", body.name, body.bodyId.val, ss.name);
        if (tn == TypeNav::Planet) {
            if (body.name == ss.name)
                LOG_ERROR(R"({{ planet:"{}", id:{}, system:"{}" }}, // same as system)", body.name, body.bodyId.val, ss.name);
            else if (body.name.size() < 2+ss.name.size() || !body.name.starts_with(ss.name) || body.name[ss.name.size()] != ' ')
                LOG_ERROR(R"({{ planet:"{}", id:{}, system:"{}" }}, // unique planet name)", body.name, body.bodyId.val, ss.name);
            for (auto& star : ss.bodies) {
                if (toTypeNav(star.type) == TypeNav::Star && body.name == star.name) {
                    LOG_ERROR(R"({{ star:"{}", id:{}, system:"{}" }}, // same as planet id:{})",
                              star.name, star.bodyId.val, ss.name, body.bodyId.val);
                    LOG_ERROR(R"({{ planet:"{}", id:{}, system:"{}" }}, // same as star id:{})",
                              body.name, body.bodyId.val, ss.name, star.bodyId.val);
                }
            }
        }
        if (tn == TypeNav::Barycenter || tn == TypeNav::Ring)
            body.name.clear();
        if (!body.bodyId.has_value()) {
            LOG_ERROR("Serializing body without bodyId");
            return false;
        }
        if (used.contains(body.bodyId.val)) {
            LOG_ERROR("Duplicated bodyId[{}]: {} and body {}", body.bodyId.val, used[body.bodyId.val], body.name);
            return false;
        }
        used[body.bodyId.val] = body.name;

        tp = NavType::checkCarrierName(body.name, &carrier_code, &carrier_name);
        if (tp != TypeNav::Other)
            LOG_ERROR("Body name '{}' looks like carrier name '{}' / '{}'", body.name, carrier_code, carrier_name);

        for (auto& st : body.stations) {
            tp = toTypeNav(st.type);
            if (tp != TypeNav::FleetCarrier && tp != TypeNav::SquadronCarrier) {
                tp = NavType::checkCarrierName(st.name, &carrier_code, &carrier_name);
                if (tp != TypeNav::Other)
                    LOG_ERROR("Station name '{}' looks like carrier name '{}' / '{}'", st.name, carrier_code, carrier_name);
            }
        }
    }

    for (auto& st : ss.stations) {
        if (st.bodyId.has_value() && used.contains(st.bodyId.val)) {
            LOG_ERROR("Duplicated bodyId[{}]: {} and station {}", st.bodyId.val, used[st.bodyId.val], st.name);
            return false;
        }
        if (st.parentId.has_value()) {
            if (!used.contains(st.parentId.val)) {
                LOG_ERROR("Unknown parent body id[{}] for station {}", st.parentId.val, st.name);
            }
        }

        tp = toTypeNav(st.type);
        if (tp != TypeNav::FleetCarrier && tp != TypeNav::SquadronCarrier) {
            tp = NavType::checkCarrierName(st.name, &carrier_code, &carrier_name);
            if (tp != TypeNav::Other)
                LOG_ERROR("Station name '{}' looks like carrier name '{}' / '{}'", st.name, carrier_code, carrier_name);
        }
    }
    return true;
}

void test_json_system(std::string& strbuf, bool saveDbBlob) {
    db::StarSystemJS db_ss {};
    if (db::parseSpanshSystemEntry(db_ss, strbuf)) {
        verifyStarSystem(db_ss);
        // clear markets
        for (auto& st : db_ss.stations)
            st.market.reset();
        for (auto& body : db_ss.bodies) {
            for (auto& st : body.stations)
                st.market.reset();
        }
        auto ss = gal::makeStarSystem(db_ss.name, db_ss.address, db_ss.coords, true, false);
        if (ss) {
            gal::update_star_system(ss.get(), db_ss);
            if (!saveDbBlob) {
                ss->needBlobSave = false;
            }
            ss->save();
        }
    }
    strbuf.clear();
}

struct Reader {
    std::string path;
    std::uintmax_t fizeSize;
    int io_file {-1};
    gzFile gz_file {};
    int64_t totalBytesRead {};

    Reader(std::string_view filePath) : path(filePath) {
        fizeSize = std::filesystem::file_size(filePath);
        //if (path.ends_with(".gz"))
        //    fizeSize *= 11;
    }
    ~Reader() {
        close();
    }

    bool open() {
        if (path.ends_with(".gz")) {
            gz_file = gzopen(path.c_str(), "rb");
            if (!gz_file) {
                LOG_ERROR("Failed to open gzip file: {}", path);
                return false;
            }
        } else {
            io_file = _open(path.c_str(), _O_BINARY|_O_RDONLY, _S_IREAD);
            if (io_file < 0) {
                LOG_ERROR("Failed to open text file: {}", path);
                return false;
            }
        }
        return true;
    }

    void close() {
        if (io_file >= 0) {
            _close(io_file);
            io_file = -1;
        }
        if (gz_file) {
            gzclose(gz_file);
            gz_file = nullptr;
        }
    }

    int read(char* buffer, int size) {
        int cnt = -1;
        if (io_file >= 0)
            cnt = _read(io_file, buffer, size);
        else if (gz_file)
            cnt = gzread(gz_file, buffer, size);
        if (cnt > 0)
            totalBytesRead += cnt;
        return cnt;
    }

    const char* error() {
        if (io_file >= 0)
            return strerror(errno);
        if (gz_file) {
            int errnum;
            return gzerror(gz_file, &errnum);
        }
        return "file is not open";
    }
};

bool spansh_systems_dump_test(std::string_view filePath, bool saveDbBlob) {
    std::string strbuf;

    Reader rd(filePath);
    if (!rd.open())
        return false;

    auto dir = TestBackdor::initCfgForTests("");
    if (dir.empty())
        return false;

    // Buffer to hold chunks of uncompressed data
    char buffer[4096];
    int bytesRead = 0;
    int systemsRead = 0;
    int reported_pc = 0;
    int reported_ss = 0;

    // Read chunk by chunk until EOF
    bool at_file_start = true;
    bool at_system_start = true;
    while ((bytesRead = rd.read(buffer, sizeof(buffer))) > 0) {
        for (int i=0; i < bytesRead; i++) {
            if (at_file_start) {
                assert(buffer[i] == '[');
                at_file_start = false;
                continue;
            }
            if (buffer[i] == '\n') {
                if (strbuf.empty() || strbuf == "[") {
                    strbuf.clear();
                    continue;
                }
                test_json_system(strbuf, saveDbBlob);
                systemsRead += 1;
                strbuf.clear();
                at_system_start = true;
                continue;
            }
            if (at_system_start) {
                if (buffer[i] == ' ' || buffer[i] == '\t' || buffer[i] == '\n')
                    continue;
                at_system_start = false;
                assert(buffer[i] == ',' || buffer[i] == '{' || buffer[i] == ']');
                if (buffer[i] == ',')
                    continue;
                if (buffer[i] == ']') {
                    LOG_ERROR("Parsed galaxy file completely");
                    test_json_system(strbuf, saveDbBlob);
                    systemsRead += 1;
                    strbuf.clear();
                    break;
                }
            }
            strbuf.push_back(buffer[i]);
        }
        if (systemsRead - reported_ss > 1000) {
            reported_ss = systemsRead;
            gal::cleanupStarSystemCache({});
            //db::checkpoint(db::CheckPoint::PASSIVE);
        }
        int pc = rd.totalBytesRead*1000 / rd.fizeSize;
        if (pc != reported_pc) {
            reported_pc = pc;
            reported_ss = systemsRead;
            LOG_INFO(">>>>>>> Parsed galaxy file {}.{}%, {} systems", pc/10, pc%10, systemsRead);
            spdlog::default_logger_raw()->flush();
            gal::cleanupStarSystemCache({});
            //db::checkpoint(db::CheckPoint::PASSIVE);
        }
    }
    strbuf.clear();

    // Check for errors before closing
    if (bytesRead < 0) {
        LOG_ERROR("Error reading file: {}", rd.error());
    }

    return true;
}

std::vector<std::pair<fs::file_time_type,std::string>> get_log_files(fs::path dir) {
    std::vector<std::pair<fs::file_time_type,std::string>> files;
    for (const auto &entry: std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file())
            continue;
        auto &ep = entry.path();
        auto es = ep.filename().string();
        // "Journal.190107234301.01.log" "Journal.2026-07-01T235123.01.log"
        if (es.starts_with("Journal.") && ep.extension() == ".log") {
            auto tp = fs::last_write_time(ep);
            files.emplace_back(tp,es);
        }
    }
    std::sort(files.begin(), files.end(), [](auto& f1, auto& f2)->bool {
        return f1.first < f2.first;
    });
    return files;
}

bool game_logs_test(std::string_view dirPath, std::string_view firstFile) {
    fs::path dir = TestBackdor::initCfgForTests(dirPath);
    if (dir.empty())
        return false;

    auto log_files = get_log_files(dirPath);

    int counter = 0;
    Timestamp ts_now {};
    for (auto& [ts,nm] : log_files) {
        counter += 1;
        if (!firstFile.empty()) {
            if (nm != firstFile)
                continue;
            firstFile = {};
        }
        LOG_INFO(">>>>>>>>>>>> Processing: {} ({} of {})", nm, counter, log_files.size());
        fs::path fname = dir / nm;
        std::ifstream inp(fname);
        std::string line;
        while (std::getline(inp, line)) {
            auto ge = TestBackdor::parseEvent(ts_now, line);
            if (!ge || ge->event == "Shutdown") {
                inp.close();
                break;
            }
        }
        gal::cleanupStarSystemCache({});
        db::checkpoint(db::CheckPoint::PASSIVE);
    }

    return true;
}

} // namespace gal