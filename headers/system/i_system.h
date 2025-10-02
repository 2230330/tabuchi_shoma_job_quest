#pragma once

class ISystem
{
public :
    virtual ~ISystem() = default;
    virtual void Update(float elapsed_time) = 0;
};