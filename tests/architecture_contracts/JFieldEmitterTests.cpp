// Author 2026-11-10 (ACCEPTED): the jump-field emitter damage rule (JDrive.h jFieldFactor).
#include "src/game/navigation/rules/JDrive.h"

#include <cstdlib>
#include <iostream>

namespace jd = game::navigation::jdrive;

int main()
{
    int fails = 0;
    auto check = [&](bool ok, const char* what) { if (!ok) { ++fails; std::cerr << "FAIL: " << what << '\n'; } };
    check(jd::jFieldFactor({2, 2, true}) == 1.0 && jd::jumpFieldComplete({2, 2, true}), "both emitters intact: full field, jump possible");
    check(jd::jFieldFactor({2, 1, true}) == 0.5 && !jd::jumpFieldComplete({2, 1, true}), "one emitter lost: J halved, no jump");
    check(jd::jFieldFactor({4, 3, true}) == 0.5, "one of four lost: J halved");
    check(jd::jFieldFactor({2, 0, true}) == 0.0, "every emitter lost: no field");
    check(jd::jFieldFactor({2, 2, false}) == 0.0, "core lost: no field");
    check(jd::jFieldFactor({0, 0, true}) == 1.0, "hull without declared emitters keeps the full field");
    std::cout << (fails ? "J FIELD EMITTERS: FAIL" : "J FIELD EMITTERS: PASS (full / half + no jump / none)") << '\n';
    return fails ? EXIT_FAILURE : EXIT_SUCCESS;
}
