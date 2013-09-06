/*
 * @author Jon Gjengset <jon@tsp.io>
 * @see http://blog.sacaluta.com/2007/08/gtk-system-tray-icon-example.html
 */
#include <gtk/gtk.h>
#include <glib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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
	printf("Popup menu\n");
}

gboolean set_tooltip(gpointer data)
{
	char *p = (char*)data;
	if (*p == '\0') {
		printf("Removing tooltip\n");
		gtk_status_icon_set_has_tooltip(icon, FALSE);
		return FALSE;
	}

	printf("Setting tooltip to '%s'\n", p);
	gtk_status_icon_set_tooltip_text(icon, p);
	return FALSE;
}

gboolean set_icon(gpointer data)
{
	char *p = (char*)data;
	printf("Setting icon to '%s'\n", p);
	gtk_status_icon_set_from_icon_name(icon, p);
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
	char *param;
	char *read;
	size_t len;
	char *fifo_path = (char*)argv;
	FILE *fifo;
	struct stat fifo_st;   

	/* outer is for open */
	while (1) {
		if (stat(fifo_path, &fifo_st) != 0) {
			perror("FIFO does not exist, exiting\n");
			gdk_threads_add_idle(do_quit, fifo);
			break;
		}

		fifo = fopen(fifo_path, "r");

		if (fifo == NULL) {
			perror("FIFO went away, exiting\n");
			gdk_threads_add_idle(do_quit, fifo);
			break;
		}

	/* inner is for read */
	while (1) {
		read = fgets(buf, 1024 * sizeof(char), fifo);

		if (read == NULL) {
			/* no more data in pipe, reopen and block */
			fclose(fifo);
			break;
		}

		/* trim string */
		while ((*read == '\n' || *read == ' ' || *read == '\t') && *read != '\0') {
			read++;
		}

		if (*read == '\0') {
			/* empty command */
			continue;
		}

		/* get rid of trailing newline */
		len = strlen(buf);
		buf[len-1] = '\0';
		len -= 1;

		/* only read from this if you *know* there are more arguments */
		param = buf + 2;
		if (len < 3) {
			*param = '\0';
		}

		switch (*buf) {
		case 'q':
			gdk_threads_add_idle(do_quit, param);
			break;
		case 't': /* tooltip */
			gdk_threads_add_idle(set_tooltip, param);
			break;
		case 'i': /* icon */
			gdk_threads_add_idle(set_icon, param);
			break;
		case 'h': /* hide */
			gdk_threads_add_idle(set_visible, (void *)0);
			break;
		case 's': /* show */
			gdk_threads_add_idle(set_visible, (void *)1);
			break;
		case 'c': /* click */
			if (onclick != NULL) {
				free(onclick);
				onclick = NULL;
			}

			if (*param == '\0') {
				printf("Removing onclick handler\n");
				break;
			}

			onclick = malloc((len-2+1) * sizeof(char));
			strncpy(onclick, param, len-2+1);
			printf("Setting onclick handler to '%s'\n", onclick);
			break;
		case 'm': /* menu */
			fprintf(stderr, "Menu command not yet implemented\n");
			break;
		default:
			fprintf(stderr, "Unknown command: '%c'\n", *buf);
		}

		gdk_flush();
	}
	}
	return NULL;
}

static GtkStatusIcon *create_tray_icon(char *start_icon)
{
	GtkStatusIcon *tray_icon;

	tray_icon = gtk_status_icon_new_from_icon_name(start_icon);
	g_signal_connect(G_OBJECT(tray_icon), "activate", G_CALLBACK(tray_icon_on_click), NULL);
	g_signal_connect(G_OBJECT(tray_icon), "popup-menu", G_CALLBACK(tray_icon_on_menu), NULL);
	gtk_status_icon_set_visible(tray_icon, TRUE);

	return tray_icon;
}

int main(int argc, char **argv)
{
	char *start_icon = "none";
	FILE *fifo;
	GThread *reader;

	gtk_init(&argc, &argv);

	if (argc == 4 && strcmp(argv[1], "-i") == 0) {
		start_icon = argv[2];
	}

	if (argc == 1) {
		printf("Usage: %s [-i ICON] FIFO\n", *argv);
		printf("Listen to FIFO for system tray icon specifications\n");
		printf("\n");
		printf("  -i ICON\tUse the specified ICON when initializing\n");
		printf("\n");
		printf("Report bugs at https://github.com/jonhoo/mktrayicon\n");
		return 0;
	}

	icon = create_tray_icon(start_icon);
	reader = g_thread_new("watch_fifo", watch_fifo, argv[argc-1]);
	gtk_main();
	return 0;
}
