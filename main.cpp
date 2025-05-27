#include "Master.h"
#include "UI.h"

/////// For mingw compiler
#define ELPP_STL_LOGGING
// #define ELPP_FEATURE_CRASH_LOG -- Stack trace not available for MinGW GCC
#define ELPP_PERFORMANCE_MICROSECONDS
#define ELPP_LOG_STD_ARRAY
#define ELPP_LOG_UNORDERED_MAP
#define ELPP_UNORDERED_SET
/////// end of mingw definitions
#include "easylogging++.h"
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

