#!/bin/bash

# Set up tray icon
mkfifo /tmp/$$.icon
./mktrayicon /tmp/$$.icon &

# set menu with a separator and a quit option
echo "m terminal,xterm|-----,true|quit,echo 'q' > /tmp/$$.icon" > /tmp/$$.icon
