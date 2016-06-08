#!/bin/bash

if [ -z "$DEVNAME" ]; then
  exit 0
fi
if [ ! -e "$DEVNAME" ]; then
  exit 0
fi

/usr/bin/jack_wait -w
exec /usr/bin/nooice "$DEVNAME"
