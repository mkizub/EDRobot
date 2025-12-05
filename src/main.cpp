#include "pch.h"

#include <cstdlib>
#include "ui/UIManager.h"
#include "Configuration.h"
#include "Galaxy.h"

INITIALIZE_EASYLOGGINGPP

int main(int argc, char *argv[]) {
    START_EASYLOGGINGPP(argc, argv);
    el::Loggers::getLogger("OpenCV");
    //el::Configurations conf("logging.conf");
    //el::Loggers::reconfigureAllLoggers(conf);
    el::Loggers::configureFromGlobal("logging.conf");

    Master& master = Master::getInstance();
    int err = master.initialize(argc, argv);
    if (!err) {
        LOG(INFO) << "Initializing UI";
        UIManager::initialize();
        std::string msg1 = lc_format("Press '{0}' to popup EDRobot", Cfg.getShortcutFor(Command::Start));
        std::string msg2 = lc_format("Press '{0}' to pause/stop", Cfg.getShortcutFor(Command::Stop));
        std::string msg = msg1 + "\n\n" + msg2;
        UIManager::showStartupDialog(msg);
        Master::getInstance().loop();
        LOG(INFO) << "Shutdown UI";
        UIManager::shutdown();
    }
    master.shutdown();
    LOG(INFO) << "Shutdown";
    el::Loggers::flushAll();
    return err;
}

