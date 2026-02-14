// #pragma once
// #include <string>
// #include <vector>
// #include <memory>
// #include <glm/glm.hpp>
// #include "core/StateContext.h"

// class UIComponent
// {
// public:
//     virtual ~UIComponent() = default;

//     std::string id;
//     bool visible = true;

//     // относительные координаты
//     glm::vec2 position {0.0f, 0.0f};
//     glm::vec2 size     {1.0f, 1.0f};

//     float cornerRadius = 0.0f;

//     // --- children ---
//     void addChild(std::unique_ptr<UIComponent> child);
    
//     virtual void renderToTexture(const Viewport& vp);
//     virtual void render(
//         const Viewport& vp,
//         float parentX,
//         float parentY,
//         float parentW,
//         float parentH
//     );
//     void render(const Viewport& vp) override
//         {
//             render(vp, 0, 0, vp.width, vp.height);
//         }

// protected:
//     std::vector<std::unique_ptr<UIComponent>> children;

//     void renderChildren(const Viewport& vp);
// };


#pragma once
#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "render/types/Viewport.h"

class UIComponent
{
public:
    virtual ~UIComponent() = default;

    std::string id;
    bool visible = true;

    glm::vec2 position {0.0f, 0.0f};
    glm::vec2 size     {1.0f, 1.0f};

    float cornerRadius = 0.0f;

    // ---- children ----
    void addChild(std::unique_ptr<UIComponent> child)
    {
        children.push_back(std::move(child));
    }

    // ---- entry point ----
    void render(const Viewport& vp)
    {
        render(vp, 0.0f, 0.0f, (float)vp.width, (float)vp.height);
    }

    virtual UIComponent* findById(const std::string& searchId)
    {
        if (id == searchId)
            return this;

        for (auto& child : children)
        {
            if (auto* found = child->findById(searchId))
                return found;
        }

        return nullptr;
    }

protected:

    virtual void render(
        const Viewport& vp,
        float parentX,
        float parentY,
        float parentW,
        float parentH
    );

    void renderChildren(
        const Viewport& vp,
        float x,
        float y,
        float w,
        float h
    );

private:
    std::vector<std::unique_ptr<UIComponent>> children;
};
