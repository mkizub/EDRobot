//
// Created by mkizub on 21.03.2026.
//

#include "../pch.h"

#include "HttpInterceptor.h"

static std::atomic<int> reqCounter;
cpr::ConnectionPool ravenPool;
cpr::ConnectionPool eddnPool;
cpr::ConnectionPool edsmPool;

js::value getJS(cpr::Response& cr) {
    if (!isOK(cr))
        return nullptr;
    js::value result;
    try {
        result = js::parse5(cr.text);
    } catch (const js::syntax_error& ex) {
        LOG(ERROR) << ex.what();
    }
    return result;
}

HttpInterceptor::HttpInterceptor(cpr::Session& session)
    : reqId(++reqCounter) , pool(nullptr), mode("HTTP")
{
    std::string url = session.GetFullRequestUrl();
    if (url.starts_with("https://raven")) {
        pool = &ravenPool;
        mode = "Raven";
    }
    else if (url.contains("eddn.")) {
        pool = &eddnPool;
        mode = "EDDN";
    }
    else if (url.contains("edsm.")) {
        pool = &edsmPool;
        mode = "EDSM";
    }
}

cpr::Response HttpInterceptor::intercept(cpr::Session &session) {
    auto logger = spdlog::get("http");
    // Log the request URL
    logger->info("{}[{}] request   url: {}", mode, reqId, session.GetFullRequestUrl());
    auto& content = session.GetContent();
    if (std::holds_alternative<cpr::Body>(content))
        logger->debug("{}[{}] request  body: {}", mode, reqId, std::get<cpr::Body>(content).str());
    else if (std::holds_alternative<cpr::Body>(content))
        logger->debug("{}[{}] request  body: {}", mode, reqId, std::get<cpr::BodyView>(content).str());

    static std::string ua;
    if (ua.empty())
        ua = std::format("EDRobot {} {}", EDROBOT_VERSION, curl_version());
    session.SetUserAgent(cpr::UserAgent(ua));

    session.UpdateHeader({{"Content-Type", "application/json; charset: utf-8"}}); // "Accept: application/json" ?
    if (mode == "Raven" && !st::cmdr.ravenKey.empty())
        session.UpdateHeader({{"rcc-key", st::cmdr.ravenKey}});

    session.SetTimeout(10s);
    if (pool)
        session.SetConnectionPool(*pool);
    if (Cfg.getCurlInsecure()) {
        auto curl = session.GetCurlHolder()->handle;
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYSTATUS, 0L);
        curl_easy_setopt(curl, CURLOPT_DOH_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_DOH_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_DOH_SSL_VERIFYSTATUS, 0L);
        curl_easy_setopt(curl, CURLOPT_PROXY_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_PROXY_SSL_VERIFYHOST, 0L);
    }
    if (auto& proxy = Cfg.getCurlProxyURL(); !proxy.empty()) {
        auto curl = session.GetCurlHolder()->handle;
        curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());
    }

    // Proceed the request and save the response
    cpr::Response response = proceed(session);

    if (response.status_code == 0) {
        logger->error("{}[{}] request error: {}", mode, reqId, response.error.message);
    } else if (response.status_code >= 400) {
        logger->error("{}[{}] error    code: {}, took {}", mode, reqId, response.status_code, response.elapsed);
    } else {
        logger->info("{}[{}] response code: {}, took {}", mode, reqId, response.status_code, response.elapsed);
        logger->debug("{}[{}] response body: {}", mode, reqId, response.text);
    }

    // Return the stored response
    return response;
}
