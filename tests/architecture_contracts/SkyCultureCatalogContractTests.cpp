#include <iostream>
#include <string>

#include "src/render/starfield/SkyCultureCatalog.h"

namespace
{
int failures = 0;

void expect(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "[FAIL] " << message << '\n';
        ++failures;
    }
}
}

int main()
{
    SkyCultureCatalog catalog;
    expect(
        catalog.loadManifest("src/assets/data/galaxy/sky_cultures/manifest.json"),
        "sky-culture manifest must load from the source tree"
    );

    const auto& cultures = catalog.cultures();
    expect(cultures.size() == 3, "approved catalog must expose three sky cultures");
    expect(catalog.defaultCultureIndex() == 0, "western culture must remain default");

    if (cultures.size() == 3)
    {
        expect(cultures[0].id == "iau-western", "western culture order changed");
        expect(cultures[1].id == "chinese-28-mansions", "Chinese culture order changed");
        expect(cultures[2].id == "hawaiian-starlines", "Hawaiian culture order changed");

        expect(cultures[0].constellations.size() == 88, "western set changed");
        expect(cultures[1].constellations.size() == 42, "curated Chinese set changed");
        expect(cultures[2].constellations.size() == 13, "Hawaiian set changed");

        expect(
            cultures[0].constellations.front().displayName("ru") == "Андромеда",
            "global Russian UI locale must select Russian western label"
        );
        expect(
            cultures[1].constellations.front().displayName("zh-Hans") == "毕宿",
            "global Chinese UI locale must select native Chinese label"
        );
        expect(
            cultures[1].constellations.front().displayName("ru") == "Net",
            "missing sky-culture translation must fall back to English"
        );
        expect(
            cultures[2].constellations.front().displayName("zh-Hans") ==
                "玛卡利伊的舀水器",
            "Hawaiian starlines must carry Chinese display translations"
        );
        expect(
            cultures[2].constellations.front().displayName("ja") ==
                "マカリイのカヌーのあか汲み",
            "Hawaiian starlines must carry Japanese display translations"
        );
        expect(
            cultures[1].constellations.front().displayName("ja") == "毕宿",
            "Japanese UI must not fall back to English for Chinese mansions"
        );
        expect(
            cultures[0].displayName("zh-Hans") == "西方 / IAU",
            "Chinese UI must localize the western culture indicator"
        );
        expect(
            cultures[1].displayName("ja") == "中国伝統星官",
            "Japanese UI must localize the Chinese culture indicator"
        );
        expect(
            cultures[2].displayName("zh-Hans") == "夏威夷星线",
            "Chinese UI must localize the Hawaiian culture indicator"
        );
    }

    if (failures != 0)
        return 1;

    std::cout << "[PASS] sky culture loading + global-locale/English fallback\n";
    return 0;
}
