# CUT - CPU Usage Tracker

Multithreaded program for any Linux distribution that calculates CPU usage from /proc/stat.
The producer-consumer problem between threads is presented.
- Reader thread ( producer ) - is responsible for reading the data from /proc/stat, putting it into the appropriate structure and then sending it for "consumption".
- Analyzer thread ( consumer & producer ) - is responsible for calculating the percentage cpu usage from the data in the structure prepared by the reader and then sending it to the printer.
- Printer thread ( consumer ) - prints the cpu usage for each core in the terminal
- Logger thread - receives messages from threads and writes them to the log_YYYYmmDd_HHmmss.txt file located in the logs folder

**How to compile and run program:**
```sh
cd build
make CUT
./CUT
```
or
```sh
make CUT -C build
./build/CUT
```

**Suppressed warnings:**
- -Wunused-parameter - functions for threads and signal handler are written 
to match the pattern and in this case their parameters are left unused
- -Wdeclaration-after-statement - the program is not written for the c90 standard
