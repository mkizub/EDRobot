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
    bool perform_post(const js::value& j);
    bool perform_patch(const js::value& j);
    js::value parse_json();


    CURL* curl;
    std::string url;
    std::string readBuffer;
    char errbuf[CURL_ERROR_SIZE];
    struct curl_slist* headers;
    long http_code {};
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
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        LOG(ERROR) << "Curl GET error: " << errbuf;
        return false;
    }
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    LOG(INFO) << "Curl GET Response Code: " << http_code;
    bool ok = http_code >= 200 && http_code <= 250;
    LOG_IF(!ok,WARNING) << "Curl GET Error Message: " << readBuffer;
    return ok;
}

bool CurlWrapper::perform_put(const std::string& data) {
    if (!curl)
        return false;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)data.size());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        LOG(ERROR) << "Curl PUT error: " << errbuf;
        return false;
    }
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    LOG(INFO) << "Curl PUT Response Code: " << http_code;
    bool ok = http_code >= 200 && http_code <= 250;
    LOG_IF(!ok,WARNING) << "Curl PUT Error Message: " << readBuffer;
    return ok;
}

bool CurlWrapper::perform_post(const js::value& j) {
    if (!curl)
        return false;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    std::string json_data = js::stringify(j);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)json_data.size());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        LOG(ERROR) << "Curl POST error: " << errbuf;
        return false;
    }
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    LOG(INFO) << "Curl POST Response Code: " << http_code;
    bool ok = http_code >= 200 && http_code <= 250;
    LOG_IF(!ok,WARNING) << "Curl POST Error Message: " << readBuffer;
    return ok;
}

bool CurlWrapper::perform_patch(const js::value& j) {
    if (!curl)
        return false;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    std::string json_data = js::stringify(j);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)json_data.size());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        LOG(ERROR) << "Curl PATCH error: " << errbuf;
        return false;
    }
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    LOG(INFO) << "Curl PATCH Response Code: " << http_code;
    bool ok = http_code >= 200 && http_code <= 250;
    LOG_IF(!ok,WARNING) << "Curl PATCH Error Message: " << readBuffer;
    return ok;
}

js::value CurlWrapper::parse_json() {
    if (!curl || readBuffer.empty())
        return {};
    js::value result;
    try {
        result = js::parse5(readBuffer);
    } catch (const js::syntax_error& ex) {
        LOG(ERROR) << ex.what();
    }
    return result;
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

CurlResp curlSimpleGet(std::string url) {
    CurlWrapper cw(url.c_str());
    if (!cw.curl)
        return {};
    LOG(INFO) << "curl GET req: " << cw.url;
    if (!cw.perform_get())
        return {false, cw.http_code};
    auto resp = cw.parse_json();
    LOG(INFO) << "curl GET resp: " << resp;
    return {true, cw.http_code, resp};
}

CurlResp curlSimpleGetWithHeaders(std::string url, std::vector<std::string> headers) {
    CurlWrapper cw(url.c_str());
    if (!cw.curl)
        return {};
    LOG(INFO) << "curl GET req: " << cw.url;
    for (auto& hdr : headers) {
        cw.headers = curl_slist_append(cw.headers, hdr.c_str());
        LOG(INFO) << "curl GET hdr: " << hdr;
    }
    if (!cw.perform_get())
        return {false, cw.http_code};
    auto resp = cw.parse_json();
    LOG(INFO) << "curl GET resp: " << resp;
    return {true, cw.http_code, resp};
}


CurlResp curlSimplePut(std::string url, std::string data) {
    CurlWrapper cw(url.c_str());
    if (!cw.curl)
        return {};
    LOG(INFO) << "curl PUT req: " << cw.url;
    LOG(INFO) << "curl PUT data: " << data;
    if (!cw.perform_put(data))
        return {false, cw.http_code};
    auto resp = cw.parse_json();
    LOG(INFO) << "curl PUT resp: " << resp;
    return {true, cw.http_code, resp};
}

CurlResp curlSimplePost(std::string url, const js::value& j) {
    CurlWrapper cw(url.c_str());
    if (!cw.curl)
        return {};
    LOG(INFO) << "curl POST req: " << cw.url;
    LOG(INFO) << "curl POST body: " << j;
    if (!cw.perform_post(j))
        return {false, cw.http_code};
    auto resp = cw.parse_json();
    LOG(INFO) << "curl POST resp: " << resp;
    return {true, cw.http_code, resp};
}

CurlResp curlSimplePostWithHeaders(std::string url, const js::value& j, std::vector<std::string> headers) {
    CurlWrapper cw(url.c_str());
    if (!cw.curl)
        return {};
    LOG(INFO) << "curl POST req: " << cw.url;
    for (auto& hdr : headers) {
        cw.headers = curl_slist_append(cw.headers, hdr.c_str());
        LOG(INFO) << "curl POST hdr: " << hdr;
    }
    LOG(INFO) << "curl POST body: " << j;
    if (!cw.perform_post(j))
        return {false, cw.http_code};
    auto resp = cw.parse_json();
    LOG(INFO) << "curl POST resp: " << resp;
    return {true, cw.http_code, resp};
}

CurlResp curlSimplePatch(std::string url, const js::value& j) {
    CurlWrapper cw(url.c_str());
    if (!cw.curl)
        return {};
    LOG(INFO) << "curl PATCH req: " << cw.url;
    LOG(INFO) << "curl PATCH body: " << j;
    if (!cw.perform_patch(j))
        return {false, cw.http_code};
    auto resp = cw.parse_json();
    LOG(INFO) << "curl PATCH resp: " << resp;
    return {true, cw.http_code, resp};
}

