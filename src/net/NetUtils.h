//
// Created by mkizub on 09.02.2026.
//

#pragma once

#ifndef EDROBOT_NETUTILS_H
#define EDROBOT_NETUTILS_H

std::string curlRequestGithubLatest();
json5pp::value curlRequestEDSM(std::string url, std::string systemName);
json5pp::value curlSimpleGet(std::string url);
json5pp::value curlSimplePut(std::string url, std::string data);
json5pp::value curlSimplePost(std::string url, json5pp::value& j);
json5pp::value curlSimplePatch(std::string url, json5pp::value& j);



#endif //EDROBOT_NETUTILS_H
