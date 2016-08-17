#!/bin/bash

if [ "$1"x == ""x ]; then
  echo "$0: missing argument"
  exit 0
fi

DEVICE_PATH="$1"
MODULE_NAME="nooice"
CLIENT_NAME="nooice$(echo ${DEVICE_PATH} | tr -d a-z/)"

exec /usr/bin/jack_load -a -w -i ${DEVICE_PATH} ${CLIENT_NAME} ${MODULE_NAME}
