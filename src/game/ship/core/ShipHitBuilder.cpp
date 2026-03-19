#include "ShipHitBuilder.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

using namespace game::damage;

namespace game::ship
{

        void ShipHitBuilder::build(
            HitComponent& hitComponent,
            const ShipDescriptor& descriptor)
        {
            hitComponent.volumes.clear();

            // --- structural volumes из descriptor ---
            for (const auto& desc : descriptor.hitVolumes)
            {
                HitVolume v;
                v.zone = desc.zone;
                v.priority = desc.priority;
                v.m_label = desc.label;
                v.center = desc.center;
                v.halfSize = desc.size * 0.5f;
                v.destructible = desc.destructible;
                v.meshChunk = desc.meshChunk;
                hitComponent.volumes.push_back(v);
            }

            // --- генерация радиаторных панелей ---
            buildRadiatorPanels(hitComponent, descriptor);
        }



        void ShipHitBuilder::buildRadiatorPanels(
            HitComponent& hitComponent,
            const ShipDescriptor& descriptor)
        {
            const auto& rad = descriptor.radiator;

            if (!rad.procedural)
                return;

            if (rad.panelCount <= 0)
                return;

            float panelWidth = rad.width / rad.panelCount;
            for (int i = 0; i < rad.panelCount; i++)
            {
                HitVolume panel;
                panel.zone = HitZoneType::Radiator;
                panel.priority = 120;
                panel.panelIndex = i;
                panel.destructible = true;
                panel.m_label = "radiator_" + std::to_string(i);
                panel.halfSize =
                {
                    panelWidth * 0.5f,
                    rad.height * 0.5f,
                    0.1f
                };

                float x = -rad.width * 0.5f + panelWidth * (i + 0.5f);
                panel.center =
                {
                    rad.center.x + x,
                    rad.center.y,
                    rad.center.z
                };
                hitComponent.volumes.push_back(panel);
            }
        }


        void ShipHitBuilder::loadVolumes(
            game::damage::HitComponent& component,
            const json& data
        )
        {
            for (const auto& v : data)
            {
                HitVolume vol;

                vol.center = glm::vec3(
                    v["position"][0].get<float>(),
                    v["position"][1].get<float>(),
                    v["position"][2].get<float>()
                );

                vol.halfSize = glm::vec3(
                    v["size"][0].get<float>() * 0.5f,
                    v["size"][1].get<float>() * 0.5f,
                    v["size"][2].get<float>() * 0.5f
                );

                vol.priority = v.value("priority", 0);
                vol.health = v.value("health", 100.0f);
                vol.m_label = v.value("label", "");
                vol.destructible = true;
                component.volumes.push_back(vol);
            }
        }

}

