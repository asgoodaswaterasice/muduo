#!/bin/bash

server=${1}

ps -ef | grep $server | egrep -v 'grep|vi|bash' | awk '{print $2}' | xargs kill
