#pragma once


#include"render_pass.h"

//描画システムのインターフェース
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