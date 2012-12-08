gcc piGpioMeasure.c wiringPi.o piThread.o -o piGpioMeasure -pthread -lsqlite3 -DDBG_D_PIN=2
