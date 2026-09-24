#include "src/core/JsonNumericLocale.h"

#include <clocale>
#include <cstring>
#include <iostream>
#include <nlohmann/json.hpp>

int main()
{
    // Use a comma-decimal locale when installed, as on the affected Windows
    // machine. CTest runners with only the C locale still verify JSON parsing.
    for (const char* locale : {
             "Ukrainian_Ukraine.1251", "German_Germany.1252",
             "French_France.1252", "de_DE.UTF-8", "fr_FR.UTF-8"})
    {
        if (std::setlocale(LC_NUMERIC, locale) &&
            std::strcmp(std::localeconv()->decimal_point, ",") == 0)
            break;
    }

    if (!core::useJsonNumericLocale() ||
        std::strcmp(std::localeconv()->decimal_point, ".") != 0)
    {
        std::cerr << "[FAIL] JSON numeric locale is not decimal-dot\n";
        return 1;
    }

    const auto value = nlohmann::json::parse(R"({"fraction":1.25,"exponent":1.2e-2})");
    if (value.at("fraction").get<double>() != 1.25 ||
        value.at("exponent").get<double>() != 0.012)
    {
        std::cerr << "[FAIL] JSON fraction changed by numeric locale\n";
        return 1;
    }

    std::cout << "[PASS] locale-independent JSON fractional numbers\n";
    return 0;
}
