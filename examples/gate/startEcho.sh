#!/bin/bash

port=55400

while((port < 55410))
do
    ./echo  $port &
    ((port++))
done
