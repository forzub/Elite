
#pragma once
#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "render/types/Viewport.h"



enum class UIAnchor
{
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    Center
};


class UIComponent
{
public:
    virtual ~UIComponent() = default;

    std::string         id;
    bool                visible = true;
    glm::vec2           position {0.0f, 0.0f};
    glm::vec2           size     {1.0f, 1.0f};
    float               cornerRadius = 0.0f;
    UIAnchor            anchor = UIAnchor::TopLeft;


    // ---- children ----
    void addChild(std::unique_ptr<UIComponent> child)
    {
        children.push_back(std::move(child));
    }




    // ---- entry point ----
    virtual void render(const Viewport& vp)
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


    virtual void update(float dt)
    {
        for (auto& child : children)
            child->update(dt);
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

    protected:

    void computeLayout(
        float parentX,
        float parentY,
        float parentW,
        float parentH,
        float& outX,
        float& outY,
        float& outW,
        float& outH
    ) const;

private:
    std::vector<std::unique_ptr<UIComponent>>   children;
    
};
