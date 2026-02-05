#pragma once

#include <vector>
#include "src/game/equipment/EquipmentModule.h"
#include "DecryptorDesc.h"
#include "src/galaxy/Actors.h"
#include "src/game/items/cryptocard/CryptoCard.h"


struct CipherCard
{
    std::vector<ActorCode> acceptedCodes;
};


struct DecryptorModule : public EquipmentModule
{
    int slotCount;

    void init(const DecryptorDesc& desc)
    {
        
        this->desc = desc.base;
        slotCount = desc.slotCount;
        
        enabled = true;
        health = 1.0f;
        
        printf("[DEBUG DecryptorModule] %s: name=%d, enabled=%d \n",desc.base.id, enabled);
    }

    bool install(CryptoCard* card)
    {
        if (!enabled || health <= 0.0f) return false;
        if (!card) return false;

        if (m_cards.size() >= static_cast<size_t>(slotCount))
            return false;

        m_cards.push_back(card);
        card->installed = true;
        return true;
    };


    bool remove(CryptoCard* card)
    {
        auto it = std::find(m_cards.begin(), m_cards.end(), card);
        if (it == m_cards.end())
            return false;

        m_cards.erase(it);
        card->installed = false;
        return true;
    };


    bool canDecode(ActorCode code) const
    {
        if (code == 0) return false;

        // встроенное правило ALL
        if (code == static_cast<ActorCode>(0xFFFFFFFF))
            return true;

        for (const CryptoCard* card : m_cards)
            if (card->code == code)
                return true;

        return false;
    };

    std::vector<CryptoCard*> m_cards;
    
};



