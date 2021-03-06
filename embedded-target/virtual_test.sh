#!/bin/bash

# define the names of the virtual serial port symlinks
p1="./vpts1"
p2="./vpts2"

# start socat with two virtual serial port links
socat -d -d pty,raw,echo=0,link=$p1 pty,raw,echo=0,link=$p2 &
socat_pid=$!

# start the mqttify program
#./mqttify -f $(realpath $p1) &
./mqttify -f $(realpath $p1) -d
mqttify_pid=$!

#mosquitto -v &
#mosquitto_pid=$!

sleep 5

echo "testing testing 1..2..3..." > $p2

# sleep 3

#read -n1 -s -r -p $'Press space to continue...\n' key

kill -s SIGTERM -p $socat_pid
kill -s SIGTERM -p $mqttify_pid
#kill -s SIGTERM -p $mosquitto_pid

