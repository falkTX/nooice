#!/bin/bash

if [ -z "$DEVNAME" ]; then
  exit 0
fi
if [ ! -e "$DEVNAME" ]; then
  exit 0
fi

/usr/bin/jack_wait -w
exec /usr/bin/noice "$DEVNAME"

# /usr/bin/jack_load noice -i "$DEVNAME"
# while [ -e "$DEVNAME" ]; do /usr/bin/sleep 1; done
# /usr/bin/jack_unload noice
