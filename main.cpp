#include "pch.h"

#include <stdlib.h>
#include "UI.h"
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
        Configuration* cfg = master.getConfiguration();
        std::string msg1 = std_format(_("Press '{}' key to start selling"), cfg->getShortcutFor(Command::Start));
        std::string msg2 = std_format(_("Press '{}' to stop"), cfg->getShortcutFor(Command::Stop));
        UI::showStartupDialog(msg1, msg2);
        //UI::showToast(gettext(L"EDRobot"), _(L"xxx"));
        Master::getInstance().loop();
    }
    UI::shutdownUI();
    el::Loggers::flushAll();
    return err;
}

