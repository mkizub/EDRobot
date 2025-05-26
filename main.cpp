#include "Master.h"
#include "UI.h"
#include "easylogging++.h"

INITIALIZE_EASYLOGGINGPP

int main(int argc, char *argv[]) {
    el::Configurations conf("logging.conf");
    el::Loggers::reconfigureAllLoggers(conf);
    UI::showStartupDialog();
    Master::getInstance().initialize(argc, argv);
    Master::getInstance().loop();
    return 0;
}

