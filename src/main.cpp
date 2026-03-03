#include "pch.h"

#include <cstdlib>
#include "ui/UIManager.h"
#include "Configuration.h"
#include "Galaxy.h"

INITIALIZE_EASYLOGGINGPP

std::thread::id main_thread_id;

int main(int argc, char *argv[]) {
    main_thread_id = std::this_thread::get_id();

//    js::value obj;
//    std::vector<js::value> arr;
//    obj["a"] = 1;
//    obj["d"] = 4;
//    obj["c"] = 3;
//    obj["b"] = 2;
//    obj["o"] = obj;
//    obj["a"] = js::array({1, 2, nullptr, 3});
//    LOG(INFO) << "obj: " << obj.stringify5(js::rule::space_indent<4>());
//    obj["o"].deref().add_flags(js::force::no_indent|js::force::no_object_nulls);
//    obj["a"].deref().add_flags(js::force::no_indent);
//    LOG(INFO) << "obj: " << obj.stringify5(js::rule::space_indent<4>());
//    obj["a"].deref().add_flags(js::force::no_array_nulls);
//    LOG(INFO) << "obj: " << obj.stringify5(js::rule::space_indent<4>());

    START_EASYLOGGINGPP(argc, argv);
    el::Loggers::getLogger("OpenCV");
    //el::Configurations conf("logging.conf");
    //el::Loggers::reconfigureAllLoggers(conf);
    el::Loggers::configureFromGlobal("logging.conf");

    Master& master = Master::getInstance();
    if (master.initialize(argc, argv))
        master.loop();
    master.shutdown();
    LOG(INFO) << "Shutdown";
    el::Loggers::flushAll();
    return 0;
}

