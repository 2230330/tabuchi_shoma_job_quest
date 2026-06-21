#pragma once

//更新システムのインターフェース
class IUpdateSystem
{
public :
    virtual ~IUpdateSystem() = default;
    virtual void Update(float elapsed_time) = 0;
};