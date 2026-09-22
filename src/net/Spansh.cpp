//
// Created by mkizub on 10.07.2026.
//
#include "../pch.h"

#include "Spansh.h"
#include "HttpInterceptor.h"
#include "../gal/Galaxy.h"
#include "../db/DB.h"

#include <curl/curl.h>
#include <cpr/cpr.h>

const std::string API = "https://spansh.co.uk/api/";

#ifdef CPPTRACE_TRY
# define TRY CPPTRACE_TRY
# define CATCH(param) CPPTRACE_CATCH(param)
# define GET_EXCEPTION_STACK_TRACE cpptrace::from_current_exception().to_string()
#else
# define TRY try
# define CATCH(param) catch(param)
# include <stacktrace>
# define GET_EXCEPTION_STACK_TRACE std::stacktrace::current()
#endif

namespace Spansh {

//static bool loadMarket(int64_t marketId, std::string stationName, std::string stationType, std::string starSystem,
//                       const js::value &jm) {
//    if (!marketId)
//        return false;
//
//    Timestamp timestamp;
//    if (!parseTimestamp(jm["updateTime"], timestamp))
//        return false;
//    spMarket old_market = gal::getMarket(marketId);
//    if (old_market && old_market->timestamp >= timestamp)
//        return false;
//    if (old_market && !old_market->stationType.empty())
//        stationType = old_market->stationType;
//
//    spMarket market = std::make_shared<Market>(Market{
//            .timestamp = timestamp,
//            .marketId = marketId,
//            .stationName = stationName,
//            .stationType = stationType,
//            .starSystem = starSystem,
//    });
//    if (old_market)
//        market->raven = old_market->raven;
//
//    auto &items = jm["commodities"].as_array_or();
//    for (auto it: items) {
//        Commodity *commodity = Cfg.getCommodityById(toLower(it["symbol"].as_string()));
//        if (!commodity)
//            continue;
//        MarketLine ml{};
//        ml.buyPrice = it["buyPrice"].as_int_or();
//        ml.sellPrice = it["sellPrice"].as_int_or();
//        ml.stock = it["supply"].as_int_or();
//        ml.demand = it["demand"].as_int_or();
//        if (old_market && old_market->items.contains(commodity)) {
//            ml.isConsumer = market->items[commodity].isConsumer;
//            ml.isProducer = market->items[commodity].isProducer;
//        }
//        market->items.emplace(commodity, ml);
//    }
//    gal::setMarketData(market);
//
//    return true;
//}

gal::spStarSystem loadStarSystem(gal::spStarSystem& ss) {
    LOG(INFO) << "Spansh query system by id: " << ss->systemAddress;
    TRY {
        auto cr = cpr::Get(cpr::Url{API + "dump/" + std::to_string((uint64_t) ss->systemAddress)});
        if (!isOK(cr))
            return ss;
        db::StarSystemJS db_ss {};
        if (!db::parseSpanshSystemDump(db_ss, cr.text))
            return ss;
        if (db_ss.address != ss->systemAddress || db_ss.name != ss->systemName) {
            LOG_ERROR("Spansh dump address/name missmatch: {}:'{}' != {}:'{}'",
                      db_ss.address, db_ss.name, ss->systemAddress, ss->systemName);
            return ss;
        }

        if (!ss->isBlobLoaded)
            ss->load();

        gal::update_star_system(ss.get(), db_ss);

        if (!ss->isBlobLoaded)
            ss->needBlobSave = true;
        if (ss->needCoreSave || ss->needBlobSave)
            ss->save();
        return ss;
    } CATCH(const std::exception &e) {
        LOG(ERROR) << "Exception in Spansh::loadStarSystem(int): " << e.what() << "\n" << GET_EXCEPTION_STACK_TRACE;
    }
    return {};
}

gal::spStarSystem loadStarSystem(std::string_view name) {
    auto db_ss = db::loadStarSystem(name);
    if (db_ss.id) {
        gal::spStarSystem ss = gal::makeStarSystem(db_ss.name, db_ss.id, nullptr, true, false);
        return loadStarSystem(ss);
    }

    LOG(INFO) << "Spansh query system id by name: " << name;
    TRY {
        // https://spansh.co.uk/api/systems/field_values/system_names?q={systenName}
        std::string url = API + "systems/field_values/system_names?q=";
        char *name_esc = curl_escape(name.data(), (int) name.length());
        std::string name_plus = name_esc;
        curl_free(name_esc);
        size_t pos = 0;
        while ((pos = name_plus.find("%20", pos)) != std::string::npos) {
            name_plus.replace(pos, 3, "+");
            pos += 1; // Move past the newly inserted '+'
        }
        url += name_plus;
        auto cr = cpr::Get(cpr::Url{url});
        auto cr_body = getJS(cr);

        gal::spStarSystem found;
        for (auto &ss: cr_body["min_max"].as_array_or()) {
            int64_t it_address = ss["id64"].as_int();
            const auto it_name = ss["name"].as_string();
            cv::Point3d it_pos {ss["x"].as_real(), ss["y"].as_real(), ss["z"].as_real()};
            auto it_ss = gal::makeStarSystem(it_name, it_address, &it_pos, true, false);
            if (it_name == name)
                found = loadStarSystem(it_ss);
        }
        return found;
    } CATCH(const std::exception &e) {
        LOG(ERROR) << "Exception in Spansh::loadStarSystem(string): " << e.what() << "\n" << GET_EXCEPTION_STACK_TRACE;
    }

    return {};
}

std::vector<gal::spStarSystem> listSystemsUsingRequest(js::value j_request, int max_pages, listCallback systemCallback) {
    std::vector<gal::spStarSystem> result;
    for (int page = 0; page < max_pages; page++) {
        j_request["page"] = page;

        std::ostringstream os;
        os << std::fixed << std::setprecision(5) << js::rule::ecma404() << js::rule::no_object_nulls() << j_request;
        auto payload = os.str();

        auto cr = cpr::Post(cpr::Url{API + "systems/search"}, cpr::Body{payload});
        auto cr_body = getJS(cr);
        if (cr_body.empty())
            break;

        int position = cr_body["from"].as_int_or();
        int count = cr_body["count"].as_int_or();
        int size = cr_body["size"].as_int_or();

        auto jresult = cr_body["results"].as_array_or();
        for (auto jr: jresult) {
            const auto name = jr["name"].as_string();
            int64_t address = jr["id64"].as_int();
            double x = jr["x"].as_real_or();
            double y = jr["y"].as_real_or();
            double z = jr["z"].as_real_or();
            auto total_bodies = jr["body_count"].as_int_or();
            Timestamp updated_at;
            if (jr["updated_at"].is_string())
                parseTimestampString(jr["updated_at"].as_string(), updated_at);
            cv::Point3d pos = {x, y, z};
            gal::spStarSystem ss = gal::makeStarSystem(name, address, &pos, false, false);
            if (systemCallback(ss, jr))
                result.push_back(ss);
        }

        if (position + size >= count)
            break;
    }

    return result;
}

std::vector<gal::spStarSystem> listNearestSystems(const std::string &systemBegin, const std::string &systemEnd, double distance) {
    if (systemBegin.empty())
        return {};
    js::value j_request = js::object({
        {"filters", js::object({{"distance", js::object({{"min", 0},{"max", distance}})}})},
        {"size",    100},
        {"page",    0},
        {"sort",    js::object({{"distance", js::object({{"direction", "asc"}})}})},});
    if (systemEnd.empty() || systemBegin == systemEnd)
        j_request["reference_system"] = systemBegin;
    else
        j_request["reference_route"] = js::object({{"source",      systemBegin},
                                                   {"destination", systemEnd}});

    std::vector<gal::spStarSystem> result = listSystemsUsingRequest(j_request, 10, [](gal::spStarSystem ss, js::value& jr)->bool {
        Timestamp updated_at;
        if (jr["updated_at"].is_string())
            parseTimestampString(jr["updated_at"].as_string(), updated_at);
        if (updated_at < ss->eddn_updated_at || !ss->isBlobLoaded)
            loadStarSystem(ss);
        if (!ss)
            return false;
        auto total_bodies = jr["body_count"].as_int_or();
        LOG_INFO("Star system: {} / {} (at x={:.5f} y={:.5f} z={:.5f}) has {} bodies, updated at {}",
                 ss->systemName, ss->systemAddress, ss->starPos.x, ss->starPos.y, ss->starPos.z, total_bodies, updated_at);
        return true;
    });

    return result;
}

bool parseSpanshTime(const js::value& j, Timestamp& tm) {
    if (!j.is_string() || j.empty())
        return false;
    auto str = j.as_string();
    if (!str.starts_with("now"))
        return parseTimestampString(str, tm);
    auto tp = std::chrono::utc_clock::now();
    if (str.size() == 3) {
        tm = tp;
        return true;
    }
    if (str[3] == '-' || str[3] == '+') {
        char *endptr {};
        int n = std::strtol(str.data()+3, &endptr, 10);
        if (endptr == str.data())
            return false;
        if (*endptr == 'y')
            tm = tp + std::chrono::years(n);
        else if (*endptr == 'm')
            tm = tp + std::chrono::months(n);
        else if (*endptr == 'd')
            tm = tp + std::chrono::days(n);
        else if (*endptr == 'h')
            tm = tp + std::chrono::hours(n);
        else
            return false;
        return true;
    }
    return false;
}

std::vector<gal::spStarSystem> listSystemsUsingRecall(const std::string uuid, listCallback systemCallback) {
    std::vector<gal::spStarSystem> result;
    for (int page = 0;; page++) {
        LOG_INFO("Spansh recall: {} page {}", uuid, page);
        std::string url = std::format("{}systems/search/recall/{}/{}", API, uuid, page);

        auto cr = cpr::Get(cpr::Url{url});
        auto cr_body = getJS(cr);
        if (cr_body.empty())
            break;

        int position = cr_body["from"].as_int_or();
        int count = cr_body["count"].as_int_or();
        int size = cr_body["size"].as_int_or();
        LOG_INFO("Spansh recall: {} page {}; responce: position {}, count {}, size {}",
                 uuid, page, position, count, size);

        bool check_time_filter = false;
        Timestamp tm_min, tm_max;
        const js::value& time_filter = cr_body["search"]["filters"]["updated_at"].deref();
        if (time_filter["comparison"].as_string_or() == "<=>") {
            if (auto& arr = time_filter["value"].as_array_or(); arr.size() == 2) {
                check_time_filter = parseSpanshTime(arr[0], tm_min) && parseSpanshTime(arr[1], tm_max);
            }
        }

        auto jresult = cr_body["results"].as_array_or();
        for (auto jr: jresult) {
            const auto name = jr["name"].as_string();
            int64_t address = jr["id64"].as_int();
            Timestamp updated_at;
            if (check_time_filter && parseTimestamp(jr["updated_at"], updated_at)) {
                if (updated_at < tm_min || updated_at > tm_max)
                    continue;
                auto delta = Timestamp::clock::now() - updated_at;
                auto delta_days = std::chrono::duration_cast<std::chrono::days>(delta);
                LOG_INFO("System '{}' was updated {} ago ({} max {})", name, delta_days, updated_at, tm_max);
            }
            double x = jr["x"].as_real_or();
            double y = jr["y"].as_real_or();
            double z = jr["z"].as_real_or();
            cv::Point3d pos = {x, y, z};

            gal::spStarSystem ss = gal::makeStarSystem(name, address, &pos, false, false);
            if (systemCallback(ss, jr))
                result.push_back(ss);
        }

        if (position + size >= count)
            break;
    }
    LOG_INFO("Spansh recall: {}; got {} systems", uuid, result.size());

    return result;
}

std::vector<gal::spStarSystem> listSpanshSearch(const std::string& uuid, listCallback systemCallback) {
    LOG_INFO("Spansh recall: {}", uuid);
    TRY {
        std::vector<gal::spStarSystem> result = listSystemsUsingRecall(uuid, [](gal::spStarSystem ss, js::value& jr)->bool {
            Timestamp updated_at;
            if (jr["updated_at"].is_string())
                parseTimestampString(jr["updated_at"].as_string(), updated_at);
            if (!ss)
                return false;
            LOG_INFO("Star system: {} / {} (at x={:.5f} y={:.5f} z={:.5f}) updated at {}",
                     ss->systemName, ss->systemAddress, ss->starPos.x, ss->starPos.y, ss->starPos.z, updated_at);
            return true;
        });
        return result;
    } CATCH(const std::exception &e) {
        LOG(ERROR) << "Exception in Spansh::listSpanshSearch(uuid): " << e.what() << "\n" << GET_EXCEPTION_STACK_TRACE;
    }

    return {};
}

} // namespace Spansh