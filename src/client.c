#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <gtk/gtk.h>
#include "../include/models.h"
#include "../include/gui.h"

#define DEFAULT_PROTOCOL 0 // Defaults to TCP

/**
 * @brief Callback for the login window delete event; close the client socket and terminate the GTK main loop
 * @param widget Pointer to the GtkWidget receiving the the event (the login window) - not used
 * @param event Pointer to the GdkEvent triggering the delete event - not used
 * @param user_data Pointer to user data passed during signal connection - not used
 * @returns FALSE
 */
gboolean onLoginWindowDeleteEvent(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
	if (clientFD >= 0) {
		close(clientFD);
		clientFD = -1;
	}

	gtk_main_quit();
	return FALSE;
}

int main(int argc, char *argv[]) {
	gtk_init(&argc, &argv);
	GtkBuilder *builder = gtk_builder_new();
	GError *error = NULL;
	if (!gtk_builder_add_from_file(builder, "main.glade", &error)) {
		fprintf(stderr, "Error loading UI file: %s\n", error->message);
		g_error_free(error);
		return 1;
	}

	// Get the login and main window from the UI
	GtkWidget *loginWindow = GTK_WIDGET(gtk_builder_get_object(builder, "login_window"));
	g_signal_connect(loginWindow, "delete-event", G_CALLBACK(onLoginWindowDeleteEvent), NULL);

	GtkWindow *mainWin = GTK_WINDOW(gtk_builder_get_object(builder, "main_window"));
	g_signal_connect(mainWin, "show", G_CALLBACK(initMainWindow), builder);

	GtkWidget *loginButton = GTK_WIDGET(gtk_builder_get_object(builder, "connect_button"));
	g_signal_connect(loginButton, "clicked", G_CALLBACK(onLoginButtonClicked), builder);

	GtkWidget *loginErrorLabel = GTK_WIDGET(gtk_builder_get_object(builder, "login_error_label"));

	gtk_widget_show_all(loginWindow);
	gtk_widget_hide(loginErrorLabel);

	int result;
	const unsigned short int port = 1420;
	struct sockaddr_in serverINETAddress; /*Server address*/
	const struct sockaddr *serverSockAddrPtr = (struct sockaddr *) &serverINETAddress;
	const int serverLen = sizeof(serverINETAddress);

	serverINETAddress.sin_family = AF_INET;
	serverINETAddress.sin_port = htons(port);
	serverINETAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // INADDR_LOOPBACK == 127.0.0.1
	clientFD = socket(AF_INET, SOCK_STREAM, DEFAULT_PROTOCOL);

	// Connect to the server
	do {
		result = connect(clientFD, serverSockAddrPtr, serverLen);
		printf("Non connesso...\n");
		if (result == -1)
			sleep(1);
	} while (result == -1);
	printf("Connesso\n");

	gtk_main();
	close(clientFD);

	exit(0);
}
