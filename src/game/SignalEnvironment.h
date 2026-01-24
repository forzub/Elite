#pragma once

struct SignalEnvironment
{
    float interference = 0.0f; // глушилки, активные помехи
    float noise        = 0.0f; // пассивный шум (обломки, бури)
};
