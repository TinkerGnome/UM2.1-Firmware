#ifndef COMMANDBUFFER_H
#define COMMANDBUFFER_H

#include "Marlin.h"

class CommandBuffer
{
  public:

    static void homeHead();
    static void homeBed();
    static void homeAll();
    static void move2dock(bool bRetract);
    static void move2heatup();
    static void move2front();
    static void moveHead(float x, float y, int feedrate);
    static void move2SafeYPos();

#if EXTRUDERS > 1
    void processT0(bool bRetract, bool bWipe);
    void processT1(bool bRetract, bool bWipe);
    void processWipe();
#endif // EXTRUDERS

    // constructor
    CommandBuffer() {}
    // destructor
    ~CommandBuffer() {}
};

extern CommandBuffer cmdBuffer;

#endif //COMMANDBUFFER_H
