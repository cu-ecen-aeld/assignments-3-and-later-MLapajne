#!/bin/sh

#DIR_PATH=$(dirname $(readlink -f "${BASH_SOURCE:-$0}"))

case "$1" in
  start)
        echo "Starting daemon: aesdsocket"
		start-stop-daemon --start --exec /usr/bin/aesdsocket -- -d 
        echo "Started daemon: aesdsocket"
        ;;
  stop)
        echo "Stopping daemon: aesdsocket"
		start-stop-daemon --stop --exec /usr/bin/aesdsocket 
        echo "Stopped daemon: aesdsocket"
        ;;
  *)
        echo "Usage: "$0" {start|stop}"
        exit 1
esac