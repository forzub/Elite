#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include "SignalType.h"

struct SignalPayload
{
    static constexpr size_t MaxMessageLength = 256;

    SignalType type = SignalType::None;

    uint32_t channel = 0;      // карта шифрования
    uint32_t actor   = 0;      // кто говорит

    std::array<char, MaxMessageLength> message{};
    uint16_t messageLength = 0;

    void setMessage(const std::string& text)
    {
        messageLength = static_cast<uint16_t>(
            std::min(text.size(), MaxMessageLength - 1)
        );

        std::memcpy(message.data(), text.data(), messageLength);
        message[messageLength] = '\0';
    }

    const char* text() const
    {
        return message.data();
    }

    bool empty() const
    {
        return messageLength == 0 && type == SignalType::None;
    }
};