/*
 * @author Jon Gjengset <jon@tsp.io>
 * @see http://blog.sacaluta.com/2007/08/gtk-system-tray-icon-example.html
 */
#include <X11/Xlib.h>
#include <glib.h>
#include <gtk/gtk.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/*
 * This function is made because it's needed to escape the '\' character
 * The basic code has been taken from the man pages of the strncpy function
 */
char *strncpy_esc(char *dest, const char *src, size_t n) {
  size_t i = 0;
  size_t index = 0;
  while (i < n && src[i] != '\0') {
    if (src[i] == '\\' && src[i + 1] != '\0') {
      dest[index] = src[i + 1];
      index++;
      i += 2;
    } else {
      dest[index] = src[i];
      index++;
      i++;
    }
  }
  for (; index < n; index++) {
    dest[index] = '\0';
  }
  return dest;
}

char *save_word(char *src, int i_del, int last) {
  char *dest = malloc((i_del - last) * sizeof(char));
  strncpy_esc(dest, src + last + 1, i_del - last - 1);
  dest[i_del - last - 1] = '\0';
  return dest;
}

/*
 * Struct that stores the label names on the menu and
 * their corresponding actions when the user selects them
 */
struct item {
  char *name;
  char *action;
} * onmenu;
int menusize = 0; // number of menu entries
GtkWidget *menu = NULL;

GtkStatusIcon *icon;
char *onclick = NULL;

void tray_icon_on_click(GtkStatusIcon *status_icon, gpointer user_data) {
  if (onclick != NULL && fork() == 0) {
    execl("/bin/sh", "sh", "-c", onclick, (char *)NULL);
  }
}

/*
 * Callback function for when an entry is selected from the menu
 * We loop over all entry names to find what action to execute
 */
void click_menu_item(GtkMenuItem *menuitem, gpointer user_data) {
  const char *label = gtk_menu_item_get_label(menuitem);
  for (int i = 0; i < menusize; i++) {
    if (strcmp(label, onmenu[i].name) == 0 && fork() == 0) {
      execl("/bin/sh", "sh", "-c", onmenu[i].action, (char *)NULL);
    }
  }
}

void tray_icon_on_menu(GtkStatusIcon *status_icon, guint button,
                       guint activate_time, gpointer user_data) {
#ifdef DEBUG
  printf("Popup menu\n");
#endif
  if (menusize) {
    gtk_menu_popup_at_pointer((GtkMenu *)menu, NULL);
  }
}

gboolean set_tooltip(gpointer data) {
  char *p = (char *)data;
  if (*p == '\0') {
#ifdef DEBUG
    printf("Removing tooltip\n");
#endif
    gtk_status_icon_set_has_tooltip(icon, FALSE);
    return FALSE;
  }

#ifdef DEBUG
  printf("Setting tooltip to '%s'\n", p);
#endif
  gtk_status_icon_set_tooltip_text(icon, p);
  free(data);
  return FALSE;
}

gboolean set_icon(gpointer data) {
  char *p = (char *)data;
#ifdef DEBUG
  printf("Setting icon to '%s'\n", p);
#endif
  if (strchr(p, '/')) {
    gtk_status_icon_set_from_file(icon, p);
  } else {
    gtk_status_icon_set_from_icon_name(icon, p);
  }
  free(data);
  return FALSE;
}

gboolean set_visible(gpointer data) {
  gtk_status_icon_set_visible(icon, data == 0 ? FALSE : TRUE);
  return FALSE;
}

gboolean do_quit(gpointer data) {
  gtk_main_quit();
  return FALSE;
}

gpointer watch_fifo(gpointer argv) {
  char *buf = malloc(1024 * sizeof(char));
  char cmd;
  char quote;
  char *param;
  char *tmp = malloc(1024 * sizeof(char));
  char *read;
  size_t len, i;
  char *fifo_path = (char *)argv;
  FILE *fifo;
  struct stat fifo_st;

/* outer is for open */
outer:
  while (1) {
    if (stat(fifo_path, &fifo_st) != 0) {
      perror("FIFO does not exist, exiting\n");
      gdk_threads_add_idle(do_quit, fifo);
      return NULL;
    }

    fifo = fopen(fifo_path, "r");

    if (fifo == NULL) {
      perror("FIFO went away, exiting\n");
      gdk_threads_add_idle(do_quit, fifo);
      return NULL;
    }

    /* inner is for read */
    while (1) {
      read = fgets(buf, 1024 * sizeof(char), fifo);

      if (read == NULL) {
        /* no more data in pipe, reopen and block */
        fclose(fifo);
        goto outer;
      }

      /* trim string */
      while ((*read == '\n' || *read == ' ' || *read == '\t') &&
             *read != '\0') {
        read++;
      }

      if (*read == '\0') {
        /* empty command */
        continue;
      }

      cmd = *read;
      len = strlen(read);
      if (len < 3) {
        param = NULL;
      } else if (*(read + 2) != '\'' && *(read + 2) != '"') {
        // unquoted string
        read += 2;
        len -= 2;
        // trim trailing whitespace
        i = len - 1;
        while (i > 0) {
          if (!isspace(read[i])) {
            len = i + 1;
            read[len] = '\0';
            break;
          }
          i -= 1;
        }
        param = malloc((len + 1) * sizeof(char));
        strncpy(param, read, len + 1);
      } else {
        // quoted string
        quote = *(read + 2);
        read += 3;
        len -= 3;
        *tmp = '\0';
        *(tmp + 1024 - 1) = '\0';
        // keep track of what we have so far
        strncpy(tmp, read, 1023);

        // now keep reading until we have the end quote
        while (1) {
          // check for terminating '
          if (len != 0) {
            // search backwards past whitespace
            i = len - 1;
            while (i > 0) {
              if (!isspace(tmp[i])) {
                break;
              }
              i -= 1;
            }
            if (tmp[i] == quote) {
              // maybe the end!
              // let's make sure it isn't escaped
              if (i >= 2 && tmp[i - 2] == '\\') {
              } else {
                // it's not!
                // we're done.
                // trim off the ' and
                // any whitespace we walked past
                len = i;
                tmp[len] = '\0';
                break;
              }
            }
          }

          if (len == 1023) {
            // we can't read any more
            // but also haven't found the end
            // forcibly terminate the string
            fprintf(stderr, "Quoted string too long (max 1023 chars)\n");
            break;
          }

          // we don't have the end of the string yet
          read = fgets(buf, 1024 * sizeof(char), fifo);
          if (read == NULL) {
            /* no more data in pipe, reopen and block */
            fclose(fifo);
            goto outer;
          }
          // note that we don't trim here, because we're
          // in a quoted string.
          strncpy(tmp + len, read, 1023 - len);
          len += strlen(tmp + len);
        }

        // quoted string is now in param[0:len]
        param = malloc((len + 1) * sizeof(char));
        strncpy(param, tmp, len + 1);
      }

      switch (cmd) {
      case 'q':
        gdk_threads_add_idle(do_quit, param);
        if (param != NULL) {
          free(param);
        }
        break;
      case 't': /* tooltip */
        gdk_threads_add_idle(set_tooltip, param);
        break;
      case 'i': /* icon */
        gdk_threads_add_idle(set_icon, param);
        break;
      case 'h': /* hide */
        gdk_threads_add_idle(set_visible, (void *)0);
        if (param != NULL) {
          free(param);
        }
        break;
      case 's': /* show */
        gdk_threads_add_idle(set_visible, (void *)1);
        if (param != NULL) {
          free(param);
        }
        break;
      case 'c': /* click */
        if (onclick != NULL) {
          free(onclick);
          onclick = NULL;
        }

        if (param != NULL && *param == '\0') {
#ifdef DEBUG
          printf("Removing onclick handler\n");
#endif
          free(param);
          break;
        }

        onclick = param;
#ifdef DEBUG
        printf("Setting onclick handler to '%s'\n", onclick);
#endif
        break;
      case 'm': /* menu */
        if (onmenu != NULL) {
          // destroy the previous menu
          for (int i = 0; i < menusize; i++) {
            free(onmenu[i].name);
            free(onmenu[i].action);
            onmenu[i].name = NULL;
            onmenu[i].action = NULL;
          }
          free(onmenu);
          onmenu = NULL;
          gtk_widget_destroy(menu);
          menu = NULL;
        }

        menusize = 0;

        if (!param) {
          break;
        } else if (*param == '\0') {
#ifdef DEBUG
          printf("Removing onmenu handler\n");
#endif
          free(param);
          break;
        }

        // This block makes sure that the parameter after 'm' is ready to be
        // processed We can't accept 2 straight commas, as it becomes ambiguous
        int straight = 0;
        int bars = 0;
        for (int i = 0; i < len; i++) {
          if (param[i] == ',' && param[i - 1] != '\\') {
            straight++;
            if (straight == 2) {
              break;
            }
          } else if (param[i] == '|' && param[i - 1] != '\\') {
            straight = 0;
            bars++;
          }
        }
        if (straight == 2) {
          printf("Two straight ',' found. Use '\\' to escape\n");
          free(param);
          break;
        }
        // End of block that checks the parameter

        // Create the onmenu array which stores structs with name, action
        // properties
        menusize = bars + 1;
        onmenu = malloc(menusize * sizeof(struct item));
        menu = gtk_menu_new();
        int last = -1;
        int item = 0;
        char lastFound = '|'; // what was the last delimiter processed
        for (int i = 0; i < len; i++) {
          if (param[i] == ',' && param[i - 1] != '\\') {
            onmenu[item].name = save_word(param, i, last);
            last = i;
            lastFound = ',';
          } else if (param[i] == '|' && param[i - 1] != '\\') {
            if (lastFound == ',') { // we have found a ',' so we read an action
              onmenu[item].action = save_word(param, i, last);
            } else { // this is a label-only entry
              onmenu[item].name = save_word(param, i, last);
              onmenu[item].action = malloc(1); // pointer has to be freeable
              *onmenu[item].action = '\0';
            }
            last = i;
            lastFound = '|';
            item++;
          }
        }
        if (item < menusize) { // haven't read all actions because last one
                               // didn't end with a '|'
          if (lastFound == ',') {
            onmenu[item].action = save_word(param, len, last);
          } else {
            onmenu[item].name = save_word(param, len, last);
            onmenu[item].action = malloc(1);
            *onmenu[item].action = '\0';
          }
        }

        // Now create the menu item widgets and attach them on the menu
        for (int i = 0; i < menusize; i++) {
          GtkWidget *w = gtk_menu_item_new_with_label(onmenu[i].name);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu), w);
          g_signal_connect(G_OBJECT(w), "activate", G_CALLBACK(click_menu_item),
                           NULL);
        }
        gtk_widget_show_all(menu);
        free(param);
        break;
      default:
        fprintf(stderr, "Unknown command: '%c'\n", *buf);
        if (param != NULL) {
          free(param);
        }
      }

      gdk_flush();
    }
  }
  return NULL;
}

static GtkStatusIcon *create_tray_icon(char *start_icon) {
  GtkStatusIcon *tray_icon;

  if (strchr(start_icon, '/')) {
    tray_icon = gtk_status_icon_new_from_file(start_icon);
  } else {
    tray_icon = gtk_status_icon_new_from_icon_name(start_icon);
  }
  g_signal_connect(G_OBJECT(tray_icon), "activate",
                   G_CALLBACK(tray_icon_on_click), NULL);
  g_signal_connect(G_OBJECT(tray_icon), "popup-menu",
                   G_CALLBACK(tray_icon_on_menu), NULL);
  gtk_status_icon_set_visible(tray_icon, TRUE);

  return tray_icon;
}

int main(int argc, char **argv) {
  char *start_icon = "none";
  char *tooltip = NULL;
  char *pipe = NULL;
  GThread *reader;

  XInitThreads(); /* see http://stackoverflow.com/a/18690540/472927 */
  gtk_init(&argc, &argv);

  if (argc == 1) {
    printf("Usage: %s [-i ICON] [-t TOOLTIP] [FIFO]\n", *argv);
    printf("Create a system tray icon as specified\n");
    printf("\n");
    printf("  -i ICON\tUse the specified ICON when initializing\n");
    printf("  -t TOOLTIP\tUse the specified TOOLTIP when initializing\n");
    printf("\n");
    printf("If a FIFO is not provided, mktrayicon will run until killed\n");
    printf("Report bugs at https://github.com/jonhoo/mktrayicon\n");
    return 0;
  }

  int c;
  while ((c = getopt(argc, argv, "i:t:")) != -1)
    switch (c) {
    case 'i':
      start_icon = optarg;
      break;
    case 't':
      tooltip = optarg;
      break;
    case '?':
      fprintf(stderr, "Unknown option: %c\n", optopt);
      return 1;
    }

  icon = create_tray_icon(start_icon);

  if (tooltip) {
    gtk_status_icon_set_tooltip_text(icon, tooltip);
  }

  /* optind holds the index of the next argument to be parsed */
  /* getopt moved positional arguments (if there were any) to the end of the
   * argv array, without parsing them */
  /* so if there were only non-positional arguments, all arguments have been
   * parsed and optind will be equal to argc */
  if (optind < argc) {
    pipe = argv[optind];
    reader = g_thread_new("watch_fifo", watch_fifo, pipe);
  }

  gtk_main();
  return 0;
}
