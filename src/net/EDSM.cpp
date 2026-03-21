//
// Created by mkizub on 21.03.2026.
//

#include "../pch.h"

#include "EDSM.h"
#include "HttpInterceptor.h"
#include "../Galaxy.h"

#include <curl/curl.h>
#include <cpr/cpr.h>

const std::string API = "https://eddn.edcd.io:4430/upload/";

std::shared_ptr<EDSM> EDSM::getInstance() {
    static std::shared_ptr<EDSM> instance = std::shared_ptr<EDSM>(new EDSM);
    return instance;
}


EDSM::EDSM() {
}

EDSM::~EDSM() {
}

js::value EDSM::loadStarSystem(const std::string& name) {

    std::string system_url = "https://www.edsm.net/api-v1/system?showId=1&showCoordinates=1&systemName=";
    system_url += curl_escape(name.c_str(), name.length());
    auto cr = cpr::Get(cpr::Url{system_url});
    auto jsystem = getJS(cr);
    if (name != jsystem["name"].as_string_or())
        return {};

    std::string bodies_url = "https://www.edsm.net/api-system-v1/bodies?systemName=";
    bodies_url += curl_escape(name.c_str(), name.length());
    cr = cpr::Get(cpr::Url{bodies_url});
    auto jbodies = getJS(cr);
    if (name == jbodies["name"].as_string_or())
        jsystem["bodies"] = jbodies["bodies"].deref();

    std::string stations_url = "https://www.edsm.net/api-system-v1/stations?systemName=";
    stations_url += curl_escape(name.c_str(), name.length());
    cr = cpr::Get(cpr::Url{bodies_url});
    auto jstations = getJS(cr);
    if (name == jstations["name"].as_string_or())
        jsystem["stations"] = jstations["stations"].deref();

    return jsystem;
}
