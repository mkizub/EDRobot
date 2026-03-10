#include "pch.h"

#include <cstdlib>
#include "ui/UIManager.h"
#include "Configuration.h"
#include "Galaxy.h"

INITIALIZE_EASYLOGGINGPP

std::thread::id main_thread_id;

int main(int argc, char *argv[]) {
    main_thread_id = std::this_thread::get_id();

    START_EASYLOGGINGPP(argc, argv);
    el::Loggers::getLogger("OpenCV");
    el::Loggers::configureFromGlobal("logging.conf");

    Master& master = Master::getInstance();
    if (master.initialize(argc, argv))
        master.loop();
    master.shutdown();
    LOG(INFO) << "Shutdown";
    el::Loggers::flushAll();
    return 0;
}

