//
// Created by mkizub on 09.02.2026.
//

#pragma once

#ifndef EDROBOT_NETUTILS_H
#define EDROBOT_NETUTILS_H

std::string curlRequestGithubLatest();
js::value curlRequestEDSM(std::string url, std::string systemName);
js::value curlSimpleGet(std::string url);
js::value curlSimplePut(std::string url, std::string data);
js::value curlSimplePost(std::string url, const js::value& j);
js::value curlSimplePatch(std::string url, const js::value& j);



#endif //EDROBOT_NETUTILS_H
