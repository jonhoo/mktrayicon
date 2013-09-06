#!/bin/bash

# Set up tray icon
mkfifo /tmp/$$.icon
./mktrayicon /tmp/$$.icon &

# Manipulate tray icon

# Click handling
echo "c xterm -e /bin/sh -c 'iwconfig; read'" > /tmp/$$.icon

# Change the icon and tooltip
for i in none weak ok good excellent; do
  echo "i network-wireless-signal-$i-symbolic" > /tmp/$$.icon
  echo "t Signal strength: $i" > /tmp/$$.icon
  sleep 1
done

# Remove tooltip and click handler
echo "c" > /tmp/$$.icon
echo "t" > /tmp/$$.icon

# Toggle the visibility of the icon for a bit
for i in {1..3}; do
  for j in h s; do
    echo $j > /tmp/$$.icon
    sleep .5s
  done
done

# Remove tray icon
echo "q" > /tmp/$$.icon
rm /tmp/$$.icon
