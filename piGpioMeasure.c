#include <stdio.h>
#include <stdlib.h>
#include <wiringPi.h>

// Configuration
const int CONF_MEASUREMENT_TIME = 10000;
const int CONF_AVERAGE_COUNT = 6;
const int CONF_COUNTER_WIRING_INPUT = 0;

// Globals
const int G_COUNTER_LOCK = 0;
int gCounter = 0;

PI_THREAD (pollingThread) {
    int prevSig = -1;
    while (1) {
        int sig = digitalRead(CONF_COUNTER_WIRING_INPUT);
        if (sig != prevSig) {
            // Up event, increment counter
            if (sig == 1) {
                piLock(G_COUNTER_LOCK);
                gCounter++;
                piUnlock(G_COUNTER_LOCK);
            }
            // Down event, delay to filter out flutter
            else if (sig == 0) {
                delay(50);
            }

            prevSig = sig;
        }

        // Delay in order to reduce CPU usage
        delay(2);
    }
}

int main() {
    // Used in for-loops
    int i;

    wiringPiSetup();

    pinMode(CONF_COUNTER_WIRING_INPUT, INPUT);

    int res = piThreadCreate(pollingThread);
    if (res != 0) {
        printf("Error creating polling thread");
    }

    printf("Started\n");

    int arrayPos = 0;
    int counterArray[CONF_AVERAGE_COUNT];

    for (i = 0; i < CONF_AVERAGE_COUNT; ++i) {
        counterArray[i] = 0;
    }

    // Get counter at regular intervals
    while (1) {
        // TODO
        delay(CONF_MEASUREMENT_TIME);

        // Get and reset counter
        int counter;
        piLock(G_COUNTER_LOCK);
        counter = gCounter;
        gCounter = 0;
        piUnlock(G_COUNTER_LOCK);

        counterArray[arrayPos] = counter;

        // Calculate array average
        int arraySum = 0;
        for (i = 0; i < CONF_AVERAGE_COUNT; ++i) {
            arraySum += counterArray[i];
        }
        float arrayAvg = (1.0f * arraySum) / CONF_AVERAGE_COUNT;

        float currentPerHour = arrayAvg * 3600000.f / CONF_MEASUREMENT_TIME;

        printf("Count: %d. currentPerHour: %f\n", counter, currentPerHour);

        // Increament and wrap around array position
        arrayPos++;
        if (arrayPos == CONF_AVERAGE_COUNT) {
            arrayPos = 0;
        }
    }
}
