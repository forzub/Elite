#pragma once


enum class SignalSemanticState
{
    None,       // сигнала нет
    Noise,      // есть, но не декодирован
    Decoded     // стабильно декодирован
};