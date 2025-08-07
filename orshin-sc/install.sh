#!/usr/bin/bash

ROOT=$(pwd)
DESTINATION=/opt/orshin-rpi-loader
SERVICES_PATH=/etc/systemd/system/

echo $SERVICE_NAME
echo $SERVICES_PATH

if [ "$EUID" -eq 0 ]; then
    printf "[INF] Running as root\n"
else
    printf "[ERR] Script needs to have root permissions\n"
    exit
fi

printf "[INF] searching for the $DESTINATION\n"
if [ ! -d $DESTINATION ]; then
    mkdir -p $DESTINATION
    printf "[INF] created $DESTINATION\n"
else
    printf "[INF] FOUND\n"
fi

printf "[INF] installing ORSHIN RPI loader\n"
cp $ROOT/orshin-rpi-loader/* $DESTINATION
chmod +x $ROOT/orshin-rpi-loader/*.sh

SERVICE_NAME=$(basename $(echo $DESTINATION/*.service))
printf "[INF] installing $SERVICE_NAME in $SERVICES_PATH\n"
ln -vnsf $DESTINATION/$SERVICE_NAME $SERVICES_PATH

printf "[INF] reload systemd daemons\n"
systemctl daemon-reload

sleep 1

printf "[INF] start $SERVICE_NAME\n"
systemctl start $SERVICE_NAME
