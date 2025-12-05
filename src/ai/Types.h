//
// Created by mkizub on 21.06.2025.
//

#pragma once

#ifndef EDROBOT_AI_TYPES_H
#define EDROBOT_AI_TYPES_H

namespace ai {

class Step;
class Task;
typedef std::shared_ptr<Step> spStep;
typedef std::shared_ptr<Task> spTask;

class nonlocal_return : public std::exception {
public:
    explicit nonlocal_return(bool failed)
        : failed(failed)
        , std::exception()
    {}
    explicit nonlocal_return(bool failed, const std::string_view message)
        : failed(failed)
        , std::exception(message.data())
    {}

    const bool failed;
};

class interrupted_error : public std::exception {
public:
    explicit interrupted_error() = default;
};

enum MessageSeverity { MSG_INFO, MSG_WARN, MSG_ERROR, MSG_FATAL };

void check_interrupted();
void sleep(int milliseconds, bool precise=false);

} // namespace ai



#endif //EDROBOT_AI_TYPES_H
