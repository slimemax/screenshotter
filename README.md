# Screenshot Daemon

A lightweight X11 screenshot daemon for Linux Mint (or any X11 desktop).  
It captures the entire screen every *N* milliseconds and saves PNGs in a
`~/Screenshots/YYYY/MM/DD/HH/` hierarchy with random filenames.

## Build

```bash
sudo apt install build-essential libx11-dev libpng-dev
gcc screenshot_daemon.c -o screenshot_daemon -lX11 -lpng

Usage

./screenshot_daemon          # default 1000 ms interval
./screenshot_daemon 250      # capture every 250 ms

