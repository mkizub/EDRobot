//
// Created by mkizub on 08.09.2026.
//

#include "../pch.h"

#include "DB.h"
#include <glaze/beve.hpp>
#include <glaze/json.hpp>

namespace db {


extern void test_json(StarSystemJS& ss_js);
void test() {
    StarSystemJS ss_js;
    test_json(ss_js);
}


} // namespace db