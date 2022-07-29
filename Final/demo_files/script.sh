#!/bin/bash

PORT=34000

./server -p $PORT -t 5 &
sleep 1
./servant -d ../dataset -c 1-9 -r 127.0.0.1 -p $PORT &
./servant -d ../dataset -c 10-18 -r 127.0.0.1 -p $PORT &
./servant -d ../dataset -c 19-27 -r 127.0.0.1 -p $PORT &
./servant -d ../dataset -c 28-36 -r 127.0.0.1 -p $PORT &
./servant -d ../dataset -c 37-45 -r 127.0.0.1 -p $PORT &
./servant -d ../dataset -c 46-54 -r 127.0.0.1 -p $PORT &
./servant -d ../dataset -c 55-63 -r 127.0.0.1 -p $PORT &
./servant -d ../dataset -c 64-72 -r 127.0.0.1 -p $PORT &
./servant -d ../dataset -c 73-81 -r 127.0.0.1 -p $PORT &
sleep 3
./client -r ../requestFile2 -q $PORT -s 127.0.0.1


