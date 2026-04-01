#pragma once


enum RenderPass {
    RenderPass_Background = 0,
    RenderPass_Object = 1,
    RenderPass_UI = 2,
    RenderPass_Lighting=3,
    RenderPass_None ,
};

class IRenderSystem
{
public:
    explicit IRenderSystem(RenderPass pass)
        : pass_(pass)
    {
    }

    virtual ~IRenderSystem() = default;
    RenderPass GetPass() const { return pass_; }
    virtual void Render() = 0;

private:
    RenderPass pass_;
};