#include <Arduino.h>

#include "config.h"
#include "util.h"

// true if in 12 hour mode
boolean mode12()
{
    if(digitalRead(PIN_MODESEL) == LOW)
    {
        return true;
    }
    else
    {
        return false;
    }
}

// true if hourly chimes required
boolean chimesEnabled()
{
    if(digitalRead(PIN_CHIME) == LOW)
    {
        return true;
    }
    else
    {
        return false;
    }
}

// Convert a number to binary in ASCII
void binToStr(int bin, char *str)
{
    int c;

    for(c = 0; c < 8; c++)
    {
        if((bin & 0x80) == 0x80)
        {
            *str = '1';
        }
        else
        {
            *str = '0';
        }

        str++;
        bin = bin << 1;
    }

    *str = '\0';
}

void splitDigit(int x, volatile int *digPtr)
{
    *digPtr = (x / 10);
    digPtr++;
    *digPtr = (x % 10);
}
