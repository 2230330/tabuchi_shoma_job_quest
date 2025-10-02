#pragma once

class IRenderSystem
{
public :
    virtual ~IRenderSystem() = default;
    virtual void Render() = 0;
};