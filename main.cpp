#include "Master.h"
#include "easylogging++.h"

INITIALIZE_EASYLOGGINGPP

int main(int argc, char *argv[]) {
    el::Configurations conf("logging.conf");
    el::Loggers::reconfigureAllLoggers(conf);
    Master master;
    master.showStartDialog(argc, argv);
    master.loop();
    return 0;
}

