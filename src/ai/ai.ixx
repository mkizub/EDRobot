//
// Created by mkizub on 21.06.2025.
//

export module EDRobotAI;

export namespace ai {

class AIManager {
public:
    AIManager();
    ~AIManager();

    void interrupt();
    void resume();
    void new_task();
};

}

