#pragma once

class HeavyTask
{
public:
    virtual const char * getName() const = 0;
    virtual void longTask() = 0;
    volatile bool longScheduled = false;
};
