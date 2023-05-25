# CUT - CPU Usage Tracker

Multithreaded program written in c99 standard for any Linux distribution.
The producer-consumer problem between threads is presented.
- Reader thread ( producer ) - is responsible for reading the data from /proc/stat, putting it into the appropriate structure and then sending it for "consumption".
- Analyzer thread ( consumer & producer ) - is responsible for calculating the percentage cpu usage from the data in the structure prepared by the reader and then sending it to the printer.
- Printer thread ( consumer ) - prints the cpu usage for each core in the terminal

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
