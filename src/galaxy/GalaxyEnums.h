#pragma once
#include <string>

enum class ActorTier
{
    Superpower,
    Regional,
    Minor,
    Periphery,
    Anomaly
};

enum class ActorKind
{
    State,
    Corporation,
    Cult,
    PirateUnion,
    Zone,
    Unknown
};

enum class NodeType
{
    Planet,
    Station,
    AsteroidBase,
    Habitat,
    Zone
};

enum class NodeRole
{
    None = 0,

    // Центры власти
    Capital,            // Столица государства / анклава

    // Колониальные формы
    Colony,             // Обычная колония
    Agricultural,       // Аграрная колония
    Industrial,         // Индустриальная / инженерная
    Extraction,         // Ресурсная / карьер
    DemographicFront,  // Демографический фронт

    // Специальные режимы
    Research,           // Научный полигон / аванпост
    Prison,             // Пенитенциарная колония
    Military,           // Военная база / крепость

    // Экономика и трафик
    TradeHub,           // Торговый хаб / порт
    Hub,                // Универсальный узел / центр

    // Политические особенности
    Enclave,            // Анклав
    Condominium,        // Совместное государство

    // Миссии и религия
    Religious,          // Религиозный центр
    Mission,            // Миссионерский аванпост

    // Аномалии и запреты
    QuarantineZone,     // Карантинная зона
    Anomaly             // Аномалия / чуждая зона
};

enum class StarSystemKind
{
    Capital,
    Colony,
    Hub,
    Route,
    Frontier,
    Target
};

enum class PresenceType
{
    Human,
    Alien,
    Synthetic,
    Unknown
};

enum class GenerationShipStatus
{
    Unknown = 0,

    InTransit,      // всё ещё летит
    Arrived,        // прибыл и стал основой колонии
    Lost,           // потерян / пропал
    Anomaly,        // искажён, изменён
    Museum,         // музей / реликт
    Abandoned,      // заброшен
    Derelict        // дрейфующий остов
};
