#include "pch.h"

#include "UI.h"

INITIALIZE_EASYLOGGINGPP

int main(int argc, char *argv[]) {
    START_EASYLOGGINGPP(argc, argv);
    el::Loggers::getLogger("OpenCV");
    //el::Configurations conf("logging.conf");
    //el::Loggers::reconfigureAllLoggers(conf);
    el::Loggers::configureFromGlobal("logging.conf");

    UI::showStartupDialog();
    Master::getInstance().initialize(argc, argv);
    Master::getInstance().loop();
    el::Loggers::flushAll();
    return 0;
}

