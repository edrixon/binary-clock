
#include "config.h"
#include "types.h"
#include "globals.h"
#include "morse.h"

// Morse characters 0-9
// To send, clock out 5 bits, LSB first - if LSB is '1', send a dot, otherwise, send a dash
int morse[10] = { 0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x1e, 0x1c, 0x18, 0x10 };

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
        digitalWrite(chimePin, HIGH);
        if((morseChar & 0x01) == 0x01)
        {
            delay(MORSE_DELAY);
        }
        else
        {
            delay(dashDelay);
        }
        digitalWrite(chimePin, LOW);

        delay(MORSE_DELAY);  // inter-symbol delay

        morseChar = morseChar >> 1;
    }
    
    delay(2 * MORSE_DELAY);
}

void chimeMorse()
{
    sendMorseChar(ledDisplay.data[0] & 0x03);
    sendMorseChar(ledDisplay.data[1]);
}

void timeInMorse()
{
    int c;

    Serial.print("Morse time");
    for(c = 0; c < 4; c++)
    {
        // Top bit of hours might be set to show PM
        if(c == 0)
        {
            sendMorseChar(ledDisplay.data[c] & 0x03);
        }
        else
        {
            sendMorseChar(ledDisplay.data[c]);
        }

        // Wait "word space" between hours and minutes
        if(c == 1)
        {
            delay(3 * MORSE_DELAY);
        }
    }
    Serial.println(" - done");
}
