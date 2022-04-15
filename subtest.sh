#!/bin/sh
mosquitto_sub --verbose \
  -h 'ec36fe04c68947d399f3cbbc782e89ff.s2.eu.hivemq.cloud' \
  -p '8883' \
  -u "$(head -1 passwd.txt)" \
  -P "$(tail -1 passwd.txt)" \
  -t 'mqttify/#' 
