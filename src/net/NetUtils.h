//
// Created by mkizub on 09.02.2026.
//

#pragma once

#ifndef EDROBOT_NETUTILS_H
#define EDROBOT_NETUTILS_H

struct CurlResp {
    bool ok {};
    int code {};
    js::value body;
};

std::string curlRequestGithubLatest();
CurlResp curlRequestEDSM(std::string url, std::string systemName);
CurlResp curlSimpleGet(std::string url);
CurlResp curlSimpleGetWithHeaders(std::string url, std::vector<std::string> headers);
CurlResp curlSimplePut(std::string url, std::string data);
CurlResp curlSimplePost(std::string url, const js::value& j);
CurlResp curlSimplePostWithHeaders(std::string url, const js::value& j, std::vector<std::string> headers);
CurlResp curlSimplePatch(std::string url, const js::value& j);



#endif //EDROBOT_NETUTILS_H
