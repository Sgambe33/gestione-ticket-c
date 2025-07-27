#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> /* For AF_INET sockets */
#include <gtk/gtk.h>
#include "../include/models.h"

#define DEFAULT_PROTOCOL 0 // Defaults to TCP

int clientFD;
User currentUser;

void sendRequest(int socket, RequestHeader *req, void *payload)
{
    ssize_t bytesWritten = write(socket, req, sizeof(*req));
    if (bytesWritten < 0)
    {
        perror("write header");
        return;
    }
    if (payload != NULL)
    {
        bytesWritten = write(socket, payload, ntohl(req->payloadSize));
        if (bytesWritten < 0)
        {
            perror("write payload");
            return;
        }
    }
}

void receiveResponse(int socket, ResponseHeader *resp, void *payloadBuffer)
{
    ssize_t bytesRead = read(socket, resp, sizeof(*resp));
    if (bytesRead < 0)
    {
        perror("read header");
        return;
    }
    uint32_t payloadSize = ntohl(resp->payloadSize);
    if (payloadSize > 0 && payloadBuffer != NULL)
    {
        bytesRead = read(socket, payloadBuffer, payloadSize);
        if (bytesRead < 0)
        {
            perror("read payload");
            return;
        }
    }
}

void on_login_button_clicked(GtkButton *button, gpointer user_data)
{
    GtkBuilder *builder = (GtkBuilder *)user_data;
    GtkWidget *emailEntry = GTK_WIDGET(gtk_builder_get_object(builder, "email_entry"));
    GtkWidget *passwordEntry = GTK_WIDGET(gtk_builder_get_object(builder, "password_entry"));
    GtkWidget *errorLabel = GTK_WIDGET(gtk_builder_get_object(builder, "login_error_label"));
    gtk_widget_hide(errorLabel);

    const char *email = gtk_entry_get_text(GTK_ENTRY(emailEntry));
    const char *password = gtk_entry_get_text(GTK_ENTRY(passwordEntry));

    if (strlen(email) == 0 || strlen(password) == 0)
    {
        gtk_label_set_text(GTK_LABEL(errorLabel), "Email and password cannot be empty.");
        gtk_widget_show(errorLabel);
        return;
    }

    RequestHeader req;
    req.commandId = htons(1);
    req.payloadSize = htonl(strlen(email) + strlen(password) + 2);
    char payload[strlen(email) + strlen(password) + 2];
    snprintf(payload, sizeof(payload), "%s\n%s", email, password);
    sendRequest(clientFD, &req, payload);

    ResponseHeader resp;
    receiveResponse(clientFD, &resp, &currentUser);

    if (ntohs(resp.statusCode) != 200)
    {
        gtk_label_set_text(GTK_LABEL(errorLabel), resp.message);
        gtk_widget_show(errorLabel);
        return;
    }
    else
    {
        GtkWidget *window = GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));
        gtk_widget_show_all(window);
        GtkWidget *loginWindow = GTK_WIDGET(gtk_builder_get_object(builder, "login_window"));
        gtk_widget_destroy(loginWindow);
    }
}

int main(int argc, char *argv[])
{

    gtk_init(&argc, &argv);
    GtkBuilder *builder = gtk_builder_new();
    GError *error = NULL;
    if (!gtk_builder_add_from_file(builder, "main.glade", &error))
    {
        fprintf(stderr, "Error loading UI file: %s\n", error->message);
        g_error_free(error);
        return 1;
    }

    // Get the main window from the UI
    GtkWidget *window = GTK_WIDGET(gtk_builder_get_object(builder, "login_window"));
    if (!window)
    {
        fprintf(stderr, "Could not find 'login_window' in UI file.\n");
        return 1;
    }

    GtkWidget *loginButton = GTK_WIDGET(gtk_builder_get_object(builder, "connect_button"));
    g_signal_connect(loginButton, "clicked", G_CALLBACK(on_login_button_clicked), builder);

    gtk_widget_show_all(window);

    int serverLen, result;
    unsigned short int port = 1420;
    struct sockaddr_in serverINETAddress; /*Server address*/
    struct sockaddr *serverSockAddrPtr;   /*Ptr to server address*/

    serverSockAddrPtr = (struct sockaddr *)&serverINETAddress;
    serverLen = sizeof(serverINETAddress);

    serverINETAddress.sin_family = AF_INET;
    serverINETAddress.sin_port = htons(port);
    serverINETAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // INADDR_LOOPBACK == 127.0.0.1
    clientFD = socket(AF_INET, SOCK_STREAM, DEFAULT_PROTOCOL);

    // Connect to the server
    do
    {
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