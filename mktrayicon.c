/*
 * @author Jon Gjengset <jon@tsp.io>
 * @see http://blog.sacaluta.com/2007/08/gtk-system-tray-icon-example.html
 */
#include <gtk/gtk.h>
#include <glib.h>
#include <X11/Xlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

#define DEBUG 1

GtkStatusIcon *icon;
char *onclick = NULL;

void tray_icon_on_click(GtkStatusIcon *status_icon, 
			gpointer user_data)
{
	if (onclick != NULL && fork() == 0) {
		execl("/bin/sh", "sh", "-c", onclick, (char *) NULL);
	}
}

void tray_icon_on_menu(GtkStatusIcon *status_icon, guint button, 
		       guint activate_time, gpointer user_data)
{
#ifdef DEBUG
	printf("Popup menu\n");
#endif
}

gboolean set_tooltip(gpointer data)
{
	char *p = (char*)data;
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

gboolean set_icon(gpointer data)
{
	char *p = (char*)data;
#ifdef DEBUG
	printf("Setting icon to '%s'\n", p);
#endif
	if (strchr(p, '/')) {
		gtk_status_icon_set_from_file(icon, p);
	}
	else {
		gtk_status_icon_set_from_icon_name(icon, p);
	}
	free(data);
	return FALSE;
}

gboolean set_visible(gpointer data)
{
	gtk_status_icon_set_visible(icon, data == 0 ? FALSE : TRUE);
	return FALSE;
}

gboolean do_quit(gpointer data)
{
	gtk_main_quit();
	return FALSE;
}

gpointer watch_fifo(gpointer argv)
{
	char *buf = malloc(1024*sizeof(char));
	char cmd;
	char *param;
	char *tmp = malloc(1024*sizeof(char));
	char *read;
	size_t len, i;
	char *fifo_path = (char*)argv;
	FILE *fifo;
	struct stat fifo_st;   

	/* outer is for open */
	outer: while (1) {
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
		while ((*read == '\n' || *read == ' ' || *read == '\t') && *read != '\0') {
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
		} else if (*(read + 2) != '\'') {
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
			param = malloc((len+1)*sizeof(char));
			strncpy(param, read, len+1);
		} else {
			// quoted string
			read += 3;
			len -= 3;
			*tmp = '\0';
			*(tmp+1024-1) = '\0';
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
					if (tmp[i] == '\'') {
						// maybe the end!
						// let's make sure it isn't escaped
						if (i >= 2 && tmp[i-2] == '\\') {
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
			param = malloc((len+1)*sizeof(char));
			strncpy(param, tmp, len+1);
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
				break;
			}

			onclick = param;
#ifdef DEBUG
			printf("Setting onclick handler to '%s'\n", onclick);
#endif
			break;
		case 'm': /* menu */
			fprintf(stderr, "Menu command not yet implemented\n");
			if (param != NULL) {
				free(param);
			}
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

static GtkStatusIcon *create_tray_icon(char *start_icon)
{
	GtkStatusIcon *tray_icon;

	if (strchr(start_icon, '/')) {
		tray_icon = gtk_status_icon_new_from_file(start_icon);
	}
	else {
		tray_icon = gtk_status_icon_new_from_icon_name(start_icon);
	}
	g_signal_connect(G_OBJECT(tray_icon), "activate", G_CALLBACK(tray_icon_on_click), NULL);
	g_signal_connect(G_OBJECT(tray_icon), "popup-menu", G_CALLBACK(tray_icon_on_menu), NULL);
	gtk_status_icon_set_visible(tray_icon, TRUE);

	return tray_icon;
}

int main(int argc, char **argv)
{
	char *start_icon = "none";
	char *tooltip = NULL;
	char *pipe = NULL;
	FILE *fifo;
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
	while ((c = getopt (argc, argv, "i:t:")) != -1)
 		switch (c)
		{
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
	/* getopt moved positional arguments (if there were any) to the end of the argv array, without parsing them */
	/* so if there were only non-positional arguments, all arguments have been parsed and optind will be equal to argc */
	if (optind < argc) {
		pipe = argv[optind];
		reader = g_thread_new("watch_fifo", watch_fifo, pipe);
	}

	gtk_main();
	return 0;
}
