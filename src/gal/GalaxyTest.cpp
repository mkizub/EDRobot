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

void test_json_system(std::string& strbuf) {
    db::StarSystemJS db_ss;
    if (db::parseSpanshSystemEntry(db_ss, strbuf)) {
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

void spansh_systems_dump_test(std::string_view filePath) {
    std::string strbuf;

    Reader rd(filePath);
    if (!rd.open())
        return;

    Cfg.load();
    Cfg.mNetworkDisabled = true;
    db::init();

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
                test_json_system(strbuf);
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
                    test_json_system(strbuf);
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
        }
        int pc = rd.totalBytesRead*1000 / rd.fizeSize;
        if (pc != reported_pc) {
            reported_pc = pc;
            reported_ss = systemsRead;
            LOG_INFO(">>>>>>> Parsed galaxy file {}.{}%, {} systems", pc/10, pc%10, systemsRead);
            spdlog::default_logger_raw()->flush();
            gal::cleanupStarSystemCache({});
        }
    }
    strbuf.clear();

    // Check for errors before closing
    if (bytesRead < 0) {
        LOG_ERROR("Error reading file: {}", rd.error());
    }
}

namespace fs = std::filesystem;

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

void game_logs_test(std::string_view dirPath) {
    Cfg.load();
    Cfg.loadCommodityDatabase();
    Cfg.mNetworkDisabled = true;

    fs::path dir;
    if (dirPath.empty()) {
        dir = dirPath;
        Cfg.mEDLogsPath = dir.native();
    } else {
        dir = Cfg.mEDLogsPath;
    }

    db::init();

    auto log_files = get_log_files(dirPath);

    Timestamp ts_now {};
    for (auto& [ts,nm] : log_files) {
        LOG_INFO(">>>>>>>>>>>> Processing: {}", nm);
        fs::path fname = dir / nm;
        std::ifstream inp(fname);
        std::string line;
        while (std::getline(inp, line)) {
            auto ge = Cfg.parseEvent(ts_now, line);
            if (!ge || ge->event == "Shutdown") {
                inp.close();
                break;
            }
        }
        gal::cleanupStarSystemCache({});
    }
}

} // namespace gal