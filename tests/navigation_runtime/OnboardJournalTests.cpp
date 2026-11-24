// The onboard computer's journal (src/game/navigation/rules/OnboardJournal.h): store / merge, radiation, the wire text.
#include <cstdio>
#include <string>

#include "src/game/navigation/rules/OnboardJournal.h"

namespace j = game::navigation::journal;

static int failures = 0, checks = 0;
static void check(bool ok, const char* what)
{
    ++checks;
    if (!ok)
    {
        ++failures;
        std::printf("FAIL: %s\n", what);
    }
}

int main()
{
    std::vector<j::Entry> journal;
    j::Entry hub;
    hub.kind = j::Kind::Hub;
    hub.key = "earth_orbital_hub";
    hub.systemId = 0;
    hub.anchorBodyId = "earth";
    hub.positionM = glm::dvec3(6798000.0, 0.0, 0.0);
    hub.label = "Высокий";
    hub.detail = "доки: МЧС, ремонт";
    hub.recordedAtS = 100.0;
    hub.hasRescueDock = true;
    check(j::store(journal, hub), "a new record changes the journal");
    check(!j::store(journal, hub), "the same record again does not");
    j::Entry older = hub;
    older.recordedAtS = 50.0;
    older.label = "old";
    check(!j::store(journal, older) && journal[0].label == "Высокий", "an older record does not replace a newer one");
    j::Entry moved = hub;
    moved.recordedAtS = 200.0;
    moved.detail = "доки: МЧС";
    check(j::store(journal, moved) && journal.size() == 1 && journal[0].detail == "доки: МЧС", "a newer one replaces it");

    j::Entry danger;
    danger.kind = j::Kind::Danger;
    danger.key = "0:swarm-17";
    danger.systemId = 0;
    danger.positionM = glm::dvec3(1.0e9, 2.0e8, -3.0e7);
    danger.radiusM = 50000.0;
    danger.label = "рой | плотный";   // a '|' in text must not break the line
    danger.recordedAtS = 300.0;
    j::store(journal, danger);

    // the wire text round trip
    const auto back = j::parse(j::format(journal));
    check(back.size() == 2, "round trip keeps both records");
    check(back.size() == 2 && back[0].label == "Высокий" && back[0].hasRescueDock && back[0].anchorBodyId == "earth",
          "round trip keeps the hub's fields");
    check(back.size() == 2 && back[1].kind == j::Kind::Danger && back[1].label == "рой   плотный" &&
              std::abs(back[1].positionM.x - 1.0e9) < 1.0e-3 && back[1].radiusM == 50000.0,
          "round trip keeps the danger's fields ('|' cleaned)");
    check(j::parse("garbage\n1|x\n").empty(), "a broken line is skipped");

    // radiation: the hardening takes a weak dose rate; a strong dose degrades, then erases
    check(j::damagingDoseGy(0.5, 3600.0) == 0.0, "0.5 Gy in an hour does nothing");
    check(std::abs(j::damagingDoseGy(3.0, 3600.0) - 2.0) < 1e-12, "3 Gy in an hour: 2 Gy count");
    auto rad = journal;
    check(j::degrade(rad, 5.0) == 0 && rad[0].reliability01 < 1.0f && rad[0].reliability01 > 0.8f, "5 Gy blurs, keeps");
    check(rad[0].errorM > 0.0, "a degraded record's position blurs");
    j::degrade(rad, 35.0);
    check(rad.empty() || rad[0].detail.empty() || rad[0].reliability01 >= 0.25f, "under 0.25 the description is lost");
    const int lost = j::degrade(rad, 100.0);
    check(rad.empty() && lost >= 0, "a heavy dose erases the records");

    std::printf("onboard_journal: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
