#include "pch.h"

#include <cstdlib>
#include "ui/UIManager.h"
#include "Configuration.h"

INITIALIZE_EASYLOGGINGPP

int main(int argc, char *argv[]) {
    START_EASYLOGGINGPP(argc, argv);
    el::Loggers::getLogger("OpenCV");
    //el::Configurations conf("logging.conf");
    //el::Loggers::reconfigureAllLoggers(conf);
    el::Loggers::configureFromGlobal("logging.conf");

    SetConsoleOutputCP(CP_UTF8);
    setlocale(LC_ALL, "");
    auto langId = GetUserDefaultUILanguage();
    if (langId == 0x0419) // ru-RU
    {
        setlocale(LC_MESSAGES, "ru-RU.UTF-8");
        _putenv_s("LC_MESSAGES", "ru");
    }
    bindtextdomain("EDRobot", "locales");
    bind_textdomain_codeset("EDRobot", "UTF-8");
    textdomain("EDRobot");
    //LOG(INFO) << _("Hello world!");

    Master& master = Master::getInstance();
    int err = master.initialize(argc, argv);
    if (!err) {
        LOG(INFO) << "Initializing UI";
        UIManager::initialize();
        std::string msg1 = std_format(_("Press '{}' key to start selling"), Cfg.getShortcutFor(Command::Start));
        std::string msg2 = std_format(_("Press '{}' to stop"), Cfg.getShortcutFor(Command::Stop));
        std::string msg = msg1 + "\n\n" + msg2;
        UIManager::showStartupDialog(msg);
        //UIManager::showToast(gettext("EDRobot"), _("xxx"));
        Master::getInstance().loop();
        LOG(INFO) << "Shutdown UI";
        UIManager::shutdown();
    }
    LOG(INFO) << "Shutdown";
    el::Loggers::flushAll();
    return err;
}

