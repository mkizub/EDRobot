//
// Created by mkizub on 09.02.2026.
//

#pragma once

#ifndef EDROBOT_NETUTILS_H
#define EDROBOT_NETUTILS_H

std::string curlRequestGithubLatest();
json5pp::value curlRequestEDSM(std::string url, std::string systemName);
json5pp::value curlRequestRavenFC(int64_t marketId);


#endif //EDROBOT_NETUTILS_H
