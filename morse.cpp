
#ifdef __MK1_HW
#include <WiFi101.h>
#else
#include <WiFi.h>
#endif

#include "config.h"
#include "types.h"
#include "globals.h"
#include "morse.h"

// Timing is this:-
//   MORSE_DELAY is one dot period
//   Time between dots/dashes of same character is MORSE_DELAY
//   Time between characters in same number is 5 * MORSE_DELAY
//   Time between numbers is 15 * MORSE_DELAY (a bit longer than it should be...)

// Morse characters 0-9
// To send, clock out 5 bits, LSB first - if LSB is '1', send a dot, otherwise, send a dash
int morse[10] = { 0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x1e, 0x1c, 0x18, 0x10 };

void morseBeep(int beepLength)
{
    digitalWrite(PIN_BEEP, HIGH);
    delay(beepLength);
    digitalWrite(PIN_BEEP, LOW);
}

void sendMorseChar(int ch)
{
    int c;
    int dashDelay;
    int morseChar;

    if(ch > 9)
    {
        Serial.print("Illegal value for sendMorseChar() - ");
        Serial.println(ch);
        return;
    }

    morseChar = morse[ch];
    
    dashDelay = 3 * MORSE_DELAY;

    for(c = 0; c < 5; c++)
    {
        if((morseChar & 0x01) == 0x01)
        {
            morseBeep(MORSE_DELAY);
        }
        else
        {
            morseBeep(dashDelay);
        }

        delay(MORSE_DELAY);  // inter-symbol delay

        morseChar = morseChar >> 1;
    }    
}

void chimeMorse()
{
    sendMorseChar(ledColData[0] & 0x03);
    delay(3 * MORSE_DELAY);
    sendMorseChar(ledColData[1]);
}

void timeInMorse()
{
    int c;

    for(c = 0; c < 4; c++)
    {
        // Top bit of hours might be set to show PM
        if(c == 0)
        {
            sendMorseChar(ledColData[c] & 0x03);
        }
        else
        {
            sendMorseChar(ledColData[c]);
        }
        delay(5 * MORSE_DELAY);

        // Wait "word space" between hours and minutes
        if(c == 1)
        {
            delay(10 * MORSE_DELAY);
        }
    }
}

void numberInMorse(int n)
{
    int divisor;
    int ch;

    divisor = 100;
    while(divisor)
    {
        ch = n / divisor;

        if(divisor == 100)
        {
            if(ch != 0)
            {
                sendMorseChar(ch);
                delay(5* MORSE_DELAY);
            }
        }
        else
        {
            sendMorseChar(ch);
            delay(5 * MORSE_DELAY);
        }
        
        n = n % divisor;
        divisor = divisor / 10;
    }
}

void ipAddressInMorse()
{
    IPAddress ip;
    int c;

    ip = WiFi.localIP();
    for(c = 0; c < 4; c++)
    {
        numberInMorse(ip[c]);
        delay(10 * MORSE_DELAY);
    }
}
