#pragma once
#include<iostream>

#include"i_component.h"

struct ComponentName : public IComponent
{
    std::string value;
};