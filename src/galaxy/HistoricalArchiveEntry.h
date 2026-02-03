#pragma once

struct HistoricalArchiveEntry
{
    ArchiveEntryId id;

    std::string title;        // "Программа GENESIS"
    int yearFrom;
    int yearTo;

    std::string category;     // economy / demography / program / doctrine
    std::string text;         // ПОЛНЫЙ текст, без нарезки

    std::vector<std::string> tags;
};
