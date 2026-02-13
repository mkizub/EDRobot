//
// Created by mkizub on 09.02.2026.
//

#include "../pch.h"

#include "NetUtils.h"
#include <curl/curl.h>

struct CurlWrapper {
    CurlWrapper(const char* url);
    ~CurlWrapper();

    static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp);

    void url_append(const std::string_view str);
    void url_append_esc(const std::string& param);
    bool perform_get();
    bool perform_put(const std::string& data);
    bool perform_post(json5pp::value& j);
    bool perform_patch(json5pp::value& j);
    json5pp::value parse_json();


    CURL* curl;
    std::string url;
    std::string readBuffer;
    char errbuf[CURL_ERROR_SIZE];
    struct curl_slist* headers;
};

size_t CurlWrapper::WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    CurlWrapper* cw = (CurlWrapper*)userp;
    cw->readBuffer.append((char*)contents, size * nmemb);
    return size * nmemb;
}


CurlWrapper::CurlWrapper(const char* url)
    : curl {}
    , url(url)
    , errbuf {}
    , headers {}
{
    curl = curl_easy_init();
    if (!curl)
        return;

    // init request defaults
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);

    if (Cfg.getCurlInsecure()) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYSTATUS, 0L);
        curl_easy_setopt(curl, CURLOPT_DOH_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_DOH_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_DOH_SSL_VERIFYSTATUS, 0L);
        curl_easy_setopt(curl, CURLOPT_PROXY_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_PROXY_SSL_VERIFYHOST, 0L);
    }
    if (auto& proxy = Cfg.getCurlProxyURL(); !proxy.empty())
        curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());

    headers = curl_slist_append(headers, "Content-Type: application/json; charset: utf-8");
    headers = curl_slist_append(headers, "Accept: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    std::string ua = std::format("EDRobot {} {}", EDROBOT_VERSION, curl_version());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, ua.c_str());

    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
}

CurlWrapper::~CurlWrapper() {
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
}

void CurlWrapper::url_append(const std::string_view str) {
    if (curl)
        url += str;
}

void CurlWrapper::url_append_esc(const std::string& param) {
    if (curl)
        url += curl_easy_escape(curl, param.c_str(), param.length());
}

bool CurlWrapper::perform_get() {
    if (!curl)
        return false;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        LOG(ERROR) << "Curl GET error: " << errbuf;
        return false;
    }
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    LOG(INFO) << "Curl GET Response Code: " << http_code;
    return true;
}

bool CurlWrapper::perform_put(const std::string& data) {
    if (!curl)
        return false;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)data.size());
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        LOG(ERROR) << "Curl PUT error: " << errbuf;
        return false;
    }
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    LOG(INFO) << "Curl PUT Response Code: " << http_code;
    return true;
}

bool CurlWrapper::perform_post(json5pp::value& j) {
    if (!curl)
        return false;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    std::string json_data = json5pp::stringify(j);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)json_data.size());
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        LOG(ERROR) << "Curl POST error: " << errbuf;
        return false;
    }
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    LOG(INFO) << "Curl POST Response Code: " << http_code;
    return true;
}

bool CurlWrapper::perform_patch(json5pp::value& j) {
    if (!curl)
        return false;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    std::string json_data = json5pp::stringify(j);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)json_data.size());
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        LOG(ERROR) << "Curl PATCH error: " << errbuf;
        return false;
    }
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    LOG(INFO) << "Curl PATCH Response Code: " << http_code;
    return true;
}

json5pp::value CurlWrapper::parse_json() {
    if (!curl || readBuffer.empty())
        return {};
    json5pp::value result;
    try {
        result = json5pp::parse5(readBuffer);
    } catch (const json5pp::syntax_error& ex) {
        LOG(ERROR) << ex.what();
    }
    return result;
}


json5pp::value curlRequestEDSM(std::string url, std::string systemName) {
    CurlWrapper cw(url.c_str());
    cw.url_append_esc(systemName);
    if (!cw.curl)
        return {};
    if (!cw.perform_get())
        return {};
    return cw.parse_json();
}

std::string curlRequestGithubLatest() {
    CurlWrapper cw("https://api.github.com/repos/mkizub/EDRobot/releases/latest");
    //headers = curl_slist_append(headers, "Accept: application/vnd.github+json");
    if (!cw.curl)
        return {};
    if (!cw.perform_get())
        return {};
    return cw.readBuffer;
}

json5pp::value curlSimpleGet(std::string url) {
    CurlWrapper cw(url.c_str());
    if (!cw.curl)
        return {};
    if (!cw.perform_get())
        return {};
    auto resp = cw.parse_json();
    LOG(INFO) << "curlSimpleGet resp: " << resp;
    return resp;
}

json5pp::value curlSimplePut(std::string url, std::string data) {
    CurlWrapper cw(url.c_str());
    if (!cw.curl)
        return {};
    if (!cw.perform_put(data))
        return {};
    auto resp = cw.parse_json();
    LOG(INFO) << "curlSimplePut resp: " << resp;
    return resp;
}

json5pp::value curlSimplePost(std::string url, json5pp::value& j) {
    CurlWrapper cw(url.c_str());
    if (!cw.curl)
        return {};
    if (!cw.perform_post(j))
        return {};
    auto resp = cw.parse_json();
    LOG(INFO) << "curlSimplePost resp: " << resp;
    return resp;
}

json5pp::value curlSimplePatch(std::string url, json5pp::value& j) {
    CurlWrapper cw(url.c_str());
    if (!cw.curl)
        return {};
    if (!cw.perform_patch(j))
        return {};
    auto resp = cw.parse_json();
    LOG(INFO) << "curlSimplePatch resp: " << resp;
    return resp;
}

// https://ravencolonial100-awcbdvabgze4c5cq.canadacentral-01.azurewebsites.net/api/fc/{marketId}
// {"marketId":3708647424,"name":"VFT-85B","displayName":"Daimonio tou Sokrati","owner":"mkzu","cargo":{"agronomictreatment":32,"bertrandite":234,"cobalt":403,"drones":11,"titanium":587,"tritium":1337}}
json5pp::value curlRequestRavenFC(int64_t marketId) {
    CurlWrapper cw("https://ravencolonial100-awcbdvabgze4c5cq.canadacentral-01.azurewebsites.net/api/fc/");
    cw.url_append(std::to_string(marketId));
    if (!cw.curl)
        return {};
    if (!cw.perform_get())
        return {};
    auto resp = cw.parse_json();
    LOG(INFO) << "RavenColonial FC get resp: " << resp;
    return resp;
}
json5pp::value curlRequestRavenFCPostCargo(int64_t marketId, json5pp::value& j) {
    LOG(INFO) << "RavenColonial FC cargo post: " << j;
    CurlWrapper cw("https://ravencolonial100-awcbdvabgze4c5cq.canadacentral-01.azurewebsites.net/api/fc/");
    cw.url_append(std::to_string(marketId)+"/cargo");
    if (!cw.curl)
        return {};
    if (!cw.perform_post(j))
        return {};
    auto resp = cw.parse_json();
    LOG(INFO) << "RavenColonial FC cargo post resp: " << resp;
    return resp;
}
json5pp::value curlRequestRavenFCPatchCargo(int64_t marketId, json5pp::value& j) {
    LOG(INFO) << "RavenColonial FC cargo patch: " << j;
    CurlWrapper cw("https://ravencolonial100-awcbdvabgze4c5cq.canadacentral-01.azurewebsites.net/api/fc/");
    cw.url_append(std::to_string(marketId)+"/cargo");
    if (!cw.curl)
        return {};
    if (!cw.perform_patch(j))
        return {};
    auto resp = cw.parse_json();
    LOG(INFO) << "RavenColonial FC cargo patch resp: " << resp;
    return resp;
}
