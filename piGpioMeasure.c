#include <stdio.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <time.h>
#include <sqlite3.h>

// Configuration
// -------------
// Define photo transistor input if not already defined
#ifndef CONF_PT_PIN
#define CONF_PT_PIN 0
#endif
// Delay after high signal to filter out flutter
#define CONF_FILTER_DELAY_MS 50
// Delay between polling, lower number increases accuracy and CPU usage
#define CONF_POLL_DELAY_MS 2
// Time between saving data (in minutes)
#define CONF_SAVE_DATA_TIME 5
// Database name
#define CONF_DB_NAME "power.db"

// Constants
// ---------
#define INVALID_TIME (-1)
#define MUTEX_LOCK 0

// Global variables
// ----------------
long long g_timeOfPrevSignalMs = INVALID_TIME;
int g_blinks = 0; // Multi thread access, must be accessed while holding lock

static void onSignal() {
    piLock(MUTEX_LOCK);
    g_blinks++;
    piUnlock(MUTEX_LOCK);

    // Get timestamp
    struct timeval tv;
    gettimeofday(&tv, NULL);
    const long long timeOfSignalMs = (tv.tv_sec * 1000LL) + (tv.tv_usec / 1000LL);

    if (INVALID_TIME != g_timeOfPrevSignalMs) {
        long long dt = timeOfSignalMs - g_timeOfPrevSignalMs;
        double f_h = 3600000.0 / dt;
        printf("Time between signals: %lldms, Signals per hour: %.0f (%.2f kW)\n", dt, f_h, f_h / 1000.0);
    }
    g_timeOfPrevSignalMs = timeOfSignalMs;
}

PI_THREAD (pollingThread) {
    int prevSig = -1;
    while (1) {
        int sig = digitalRead(CONF_PT_PIN);
        if (sig != prevSig) {
            if (sig == 1) {
                onSignal();
#ifdef DBG_D_PIN
                digitalWrite(DBG_D_PIN, 1);
#endif
            } else if (sig == 0) {
#ifdef DBG_D_PIN
                digitalWrite(DBG_D_PIN, 0);
#endif
                delay(CONF_FILTER_DELAY_MS);
            }

            prevSig = sig;
        }

        // Delay in order to reduce CPU usage
        delay(CONF_POLL_DELAY_MS);
    }
}

// Callback for DB
static int dbCallback(void *NotUsed, int argc, char **argv, char **azColName) {
int i;
    for(i=0; i<argc; i++) {
        printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }
    printf("\n");
    return 0;
}

int main() {
    int i;
    sqlite3* pDb;
    char* pDbErrMsg = 0;
    int res;

    // Open (create if it doesn't exist) database
    res = sqlite3_open(CONF_DB_NAME, &pDb);
    if (res) {
        fprintf(stderr, "Can't open db: %s\n", sqlite3_errmsg(pDb));
        sqlite3_close(pDb);
        return res;
    }

    // Create table if it does not exist
    // TODO Do we need to free this query?
    char* query = sqlite3_mprintf(
            "CREATE TABLE IF NOT EXISTS power_table (id INTEGER PRIMARY KEY ASC, year INTEGER, "
            "month INTEGER, day INTEGER, hour INTEGER, minute INTEGER, blinks INTEGER);");
    res = sqlite3_exec(pDb, query, dbCallback, 0, &pDbErrMsg);
    if (res) {
        fprintf(stderr, "SQL error: %s\n", pDbErrMsg);
        sqlite3_free(pDbErrMsg);
        sqlite3_close(pDb);
        return res;
    }
    sqlite3_close(pDb);

    // Setup wiring pi
    wiringPiSetup();
    pinMode(CONF_PT_PIN, INPUT);
#ifdef DBG_D_PIN
    pinMode(DBG_D_PIN, OUTPUT);
#endif

    // Create and start thread that monitors power usage
    res = piThreadCreate(pollingThread);
    if (res) {
        fprintf(stderr, "Error creating polling thread");
        return res;
    }

    printf("Started\n");

    while (1) {
        // Get current time and calculate delay needed until next save point
        time_t tim = time(NULL);
        struct tm *now=localtime(&tim);
        int year = now->tm_year + 1900;
        int month = now->tm_mon + 1;
        int day = now->tm_mday;
        int hour = now->tm_hour;
        int minute = now->tm_min;
        int second = now->tm_sec;
        int delay_s = (CONF_SAVE_DATA_TIME - (minute % CONF_SAVE_DATA_TIME)) * 60 - second;
        printf("--- Time now:%d:%d = Delay:%d to next %d min\n",
                minute, second, delay_s, CONF_SAVE_DATA_TIME);
        delay(delay_s * 1000);

        // Capture and reset blink counter
        int intervalBlinks;
        piLock(MUTEX_LOCK);
        intervalBlinks = g_blinks;
        g_blinks = 0;
        piUnlock(MUTEX_LOCK);

        // Only store data if we've monitored a whole interval
        if (minute % CONF_SAVE_DATA_TIME == 0) {
            // Open DB and insert new values
            res = sqlite3_open(CONF_DB_NAME, &pDb);
            if (res) {
               fprintf(stderr, "Can't open db: %s\n", sqlite3_errmsg(pDb));
               sqlite3_close(pDb);
            } else {
                char* query = sqlite3_mprintf(
                        "INSERT INTO power_table (year,month,day,hour,minute,blinks) VALUES "
                        "(%d,%d,%d,%d,%d,%d)",
                        year, month, day, hour, minute, intervalBlinks);
                res = sqlite3_exec(pDb, query, dbCallback, 0, &pDbErrMsg);
                if (res) {
                    fprintf(stderr, "SQL error: %s\n", pDbErrMsg);
                    sqlite3_free(pDbErrMsg);
                }
                sqlite3_close(pDb);
            }
        }
    }
}
