#!/bin/bash

set -e

if [ -z "$DEVNAME" ]; then
  exit 0
fi

/usr/bin/jack_wait -w
/usr/bin/jack_load -w noice -i "$DEVNAME"
