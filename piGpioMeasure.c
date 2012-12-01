#include <stdio.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <sys/time.h>

// Configuration
// -------------
// Input (according to wiringPi) to use as input
const int CONF_COUNTER_WIRING_INPUT = 0;
// Delay after high signal to filter out flutter
const int CONF_FILTER_DELAY_MS = 25;
// Delay between polling, lower number increases accuracy and CPU usage
const int CONF_POLL_DELAY_MS = 2;

// Globals
#define G_INVALID_TIME (-1)
long long g_timeOfPrevSignalMs = G_INVALID_TIME;

void onSignal() {
    // Get timestamp
    struct timeval tv;
    gettimeofday(&tv, NULL);
    const long long timeOfSignalMs = (tv.tv_sec * 1000LL) + (tv.tv_usec / 1000LL);

    if (G_INVALID_TIME != g_timeOfPrevSignalMs) {
        long long dt = timeOfSignalMs - g_timeOfPrevSignalMs;
        double f_h = 3600000.0 / dt;
        printf("Time between signals: %lld, Signals per hour: %f\n", dt, f_h);
    }

    g_timeOfPrevSignalMs = timeOfSignalMs;
}

PI_THREAD (pollingThread) {
    int prevSig = -1;
    while (1) {
        int sig = digitalRead(CONF_COUNTER_WIRING_INPUT);
        if (sig != prevSig) {
            // Up event
            if (sig == 1) {
                onSignal();
            }
            // Down event, delay to filter out flutter
            else if (sig == 0) {
                delay(CONF_FILTER_DELAY_MS);
            }

            prevSig = sig;
        }

        // Delay in order to reduce CPU usage
        delay(CONF_POLL_DELAY_MS);
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

    // TODO Figure out what to do in this thread
    while (1);

    /* TODO Remove

    // Initialize array for average calculations
    int arrayPos = 0;
    int counterArray[CONF_AVERAGE_COUNT];
    for (i = 0; i < CONF_AVERAGE_COUNT; ++i) {
        counterArray[i] = 0;
    }

    // Get counter at regular intervals
    while (1) {
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
    */
}
