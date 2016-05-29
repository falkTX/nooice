#!/bin/bash

if [ -z "$DEVNAME" ]; then
  exit 0
fi
if [ ! -e "$DEVNAME" ]; then
  exit 0
fi

/usr/bin/systemctl start noice@"$DEVNAME"
