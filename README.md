# mktrayicon

`mktrayicon` is a simple proxy program that lets you create and modify
system tray icons without having to deal with a graphical toolkit like
GTK.

`mktrayicon` can be used two ways: To create an icon that is controlled
by a named pipe or, more simply, to create a non-interactive icon.

If a FIFO is not provided, mktrayicon will run until killed (e.g.,
`pkill -f 'mktrayicon.*<ICON>'`). If you are using a named pipe (FIFO)
to control the icon, *the pipe should already be created before you call
`mktrayicon`*.

Every line written to the pipe should contain a single letter specifying
what operation to perform, optionally followed by a space and a
parameter to the command. Each command should be terminated by a
newline. The following commands are supported:

  - `q`: Terminate `mktrayicon` and remove the tray icon
  - `i <icon>`: Set the graphic to use for the tray icon; it can be a
    stock icon name (see `/usr/share/icons`) or path to a custom icon
  - `t <text>`: Set the text to display in the icon tooltip
  - `t`: Remove the icon tooltip
  - `c <cmnd>`: Set the command to be execute when the user clicks the
    icon (`cmnd` is passed to `/bin/sh -c`)
  - `c`: Remove the click handler
  - `m <label1>,<cmd1>|<label2>,<cmd2>|...`: Set the labels and the
    corresponding commands to be executed when the user opens the icon
    menu (right-click usually) (`cmd#` is passed to `/bin/sh -c`)
  - `m`: Remove the menu handler
  - `h`: Hide the tray icon
  - `s`: Show the tray icon

By default, the `none` tooltip icon is used. To change this, pass `-i
<stock_icon_name>` or `-i <path_to_custom_icon>` when running
`mktrayicon`.

Note that any script communicating with `mktrayicon` via the pipe
**must**, for the time being, send `q` when they are done. Just removing
the FIFO file will **not** cause the tray icon to be removed.

The command argument can be quoted with either `'` or `"` if you wish it
to include newlines. Other string interpolation may be added later.
Quoted strings are terminated by a matching quote at the end of a line
(ignoring whitespace). To escape a quote character at the end of a line
to continue a quoted string, prefix it with a `\`.

The m(enu) command uses `,` as a delimiter between label and command and
`|` as a delimiter between entries (label+command). If you want to use
these two characters in a label or command, you have to escape them with
`\`. You can make a blank label or a label without an action by leaving
out the `label` or `cmd` respectively. Use `-----` or an empty label to
insert a menu separator. For example:

```console
$ echo "m Browser,firefox|Terminal,xterm|Label-only||,chromium" > /tmp/test
$ # (where `mkfifo /tmp/test` has been executed before)
```

Would give you a menu with five entries:

 - "Browser", which launches `firefox` when clicked
 - "Terminal", which launches `xterm` when clicked
 - "Label-only", which does nothing if clicked
 - A separator
 - An unlabeled entry which launches `chromium` when clicked

## Why?

Because I wanted to be able to create tray icons from bash without all
the hassle of interacting with GTK. Now I can create scripts for
measuring stuff and instantly make tray icons out of them (3G signal
strength for example).

## Example run

This example is also in `examples/test.sh` so you can try running it.

```bash
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
  sleep 2
done

# Remove tooltip and click handler
echo "c" > /tmp/$$.icon
echo "t" > /tmp/$$.icon

# Toggle the visibility of the icon for a bit
for i in {1..3}; do
  for j in h s; do
    echo $j > /tmp/$$.icon
    sleep 1
  done
done

# Remove tray icon
echo "q" > /tmp/$$.icon
rm /tmp/$$.icon
```

## Known bugs

This is my first time using the GTK+ C library, and I've got to say it
is less than pleasant to work with. My biggest issue has been trying to
do blocking IO without blocking the GUI thread, as GTK seems to not like
that.

**Patches are very welcome!**
