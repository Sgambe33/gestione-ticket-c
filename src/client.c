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
#include "../include/gui_utils.h"

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

void onLoginButtonClicked(GtkButton *button, gpointer user_data)
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
        gtk_label_set_text(GTK_LABEL(errorLabel), "Errore: email e password non possono essere vuoti");
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

void renderPriority(GtkTreeViewColumn *col, GtkCellRenderer *renderer,
                    GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
    int priority;
    gtk_tree_model_get(model, iter, 4, &priority, -1); // column 4 = priority
    g_object_set(renderer, "text", priorityToString(priority), NULL);
}

void renderState(GtkTreeViewColumn *col, GtkCellRenderer *renderer,
                 GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
    int state;
    gtk_tree_model_get(model, iter, 5, &state, -1); // column 5 = state
    g_object_set(renderer, "text", stateToString(state), NULL);
}

void onDisconnectButtonClicked(GtkWidget *widget, gpointer user_data)
{
    GtkBuilder *builder = GTK_BUILDER(user_data);
    GtkWidget *mainWindow = GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));

    gtk_main_quit();
    gtk_widget_destroy(mainWindow);
}

void onSaveTicketButtonClicked(GtkWidget *widget, gpointer user_data)
{
    GtkBuilder *builder = GTK_BUILDER(user_data);
    GtkWidget *titleEntry = GTK_WIDGET(gtk_builder_get_object(builder, "ticket_title_entry"));
    GtkWidget *descriptionText = GTK_WIDGET(gtk_builder_get_object(builder, "ticket_description_text"));
    GtkWidget *errorLabel = GTK_WIDGET(gtk_builder_get_object(builder, "ticket_edit_error_label"));

    const char *title = gtk_entry_get_text(GTK_ENTRY(titleEntry));
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(descriptionText));
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_get_end_iter(buffer, &end);
    char *description = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

    if (strlen(title) == 0)
    {
        gtk_label_set_text(GTK_LABEL(errorLabel), "Errore: il titolo non può essere vuoto");
        gtk_widget_show(errorLabel);
        g_free(description);
        return;
    }

    Ticket newTicket = {0};

    strncpy(newTicket.title, title, sizeof(newTicket.title) - 1);
    strncpy(newTicket.description, description, sizeof(newTicket.description) - 1);
    newTicket.priority = LOW;
    newTicket.state = OPEN;
    newTicket.creationDate = time(NULL);
    newTicket.creator = currentUser;
    User emptyAgent = {0};
    newTicket.assignedAgent = emptyAgent;

    g_free(description);

    RequestHeader req = {
        .commandId = htons(2),
        .payloadSize = htonl(sizeof(newTicket))};

    sendRequest(clientFD, &req, &newTicket);

    ResponseHeader resp;
    receiveResponse(clientFD, &resp, NULL);

    if (ntohs(resp.statusCode) != 201)
    {
        gtk_label_set_text(GTK_LABEL(errorLabel), resp.message);
        gtk_widget_show(errorLabel);
        return;
    }

    // Reset the dialog fields
    gtk_entry_set_text(GTK_ENTRY(titleEntry), "");
    GtkTextBuffer *descBuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(descriptionText));
    gtk_text_buffer_set_text(descBuffer, "Descrizione del problema...", -1);
    gtk_widget_hide(errorLabel);

    GtkWidget *editTicketDialog = GTK_WIDGET(gtk_builder_get_object(builder, "edit_ticket_dialog"));
    gtk_widget_hide(editTicketDialog);

    onMainWindowShow(NULL, builder);
}

void onDiscardButtonClicked(GtkWidget *widget, gpointer user_data)
{
    GtkBuilder *builder = GTK_BUILDER(user_data);
    // Reset the dialog fields
    GtkWidget *titleEntry = GTK_WIDGET(gtk_builder_get_object(builder, "ticket_title_entry"));
    gtk_entry_set_text(GTK_ENTRY(titleEntry), "");
    GtkWidget *descriptionText = GTK_WIDGET(gtk_builder_get_object(builder, "ticket_description_text"));
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(descriptionText));
    gtk_text_buffer_set_text(buffer, "Descrizione del problema...", -1);

    GtkWidget *editTicketDialog = GTK_WIDGET(gtk_builder_get_object(builder, "edit_ticket_dialog"));
    gtk_widget_hide(editTicketDialog);
}

void onAddTicketButtonClicked(GtkWidget *widget, gpointer user_data)
{
    GtkBuilder *builder = GTK_BUILDER(user_data);
    GtkWidget *editTicketDialog = GTK_WIDGET(gtk_builder_get_object(builder, "edit_ticket_dialog"));
    gtk_window_set_title(GTK_WINDOW(editTicketDialog), "Aggiungi ticket");
    gtk_widget_show_all(editTicketDialog);
    GtkLabel *ticketEditErrorLabel = GTK_LABEL(gtk_builder_get_object(builder, "ticket_edit_error_label"));
    gtk_widget_hide(ticketEditErrorLabel);

    GtkWidget *priorityComboBox = GTK_WIDGET(gtk_builder_get_object(builder, "priority_combobox"));
    GtkWidget *stateCombobox = GTK_WIDGET(gtk_builder_get_object(builder, "state_combobox"));
    GtkWidget *agentCombobox = GTK_WIDGET(gtk_builder_get_object(builder, "agent_combobox"));

    gtk_widget_set_sensitive(priorityComboBox, 0);
    gtk_widget_set_sensitive(stateCombobox, 0);
    gtk_widget_set_sensitive(agentCombobox, 0);

    GtkWidget *saveButton = GTK_WIDGET(gtk_builder_get_object(builder, "save_ticket_button"));
    g_signal_handlers_disconnect_by_func(saveButton, G_CALLBACK(onSaveTicketButtonClicked), builder);
    g_signal_connect(saveButton, "clicked", G_CALLBACK(onSaveTicketButtonClicked), builder);
    GtkWidget *discardButton = GTK_WIDGET(gtk_builder_get_object(builder, "discard_ticket_button"));
    g_signal_handlers_disconnect_by_func(discardButton, G_CALLBACK(onDiscardButtonClicked), builder);
    g_signal_connect(discardButton, "clicked", G_CALLBACK(onDiscardButtonClicked), builder);
}

void onMainWindowShow(GtkWidget *widget, gpointer user_data)
{
    GtkBuilder *builder = GTK_BUILDER(user_data);

    GtkLabel *welcomeLabel = GTK_LABEL(gtk_builder_get_object(builder, "welcome_label"));
    char welcomeText[128];
    snprintf(welcomeText, sizeof(welcomeText), "Benvenuto/a, %s %s", currentUser.name, currentUser.surname);
    gtk_label_set_text(welcomeLabel, welcomeText);

    GtkWidget *addTicketButton = GTK_WIDGET(gtk_builder_get_object(builder, "add_ticket_button"));
    GtkWidget *disconnectButton = GTK_WIDGET(gtk_builder_get_object(builder, "disconnect_button"));

    g_signal_connect(addTicketButton, "clicked", G_CALLBACK(onAddTicketButtonClicked), builder);
    g_signal_connect(disconnectButton, "clicked", G_CALLBACK(onDisconnectButtonClicked), builder);

    GtkListStore *store = GTK_LIST_STORE(gtk_builder_get_object(builder, "liststore_items"));
    gtk_list_store_clear(store);

    GtkTreeIter iter;

    GtkTreeViewColumn *priority_col = GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "col_priority"));
    GtkCellRenderer *priority_renderer = GTK_CELL_RENDERER(gtk_builder_get_object(builder, "renderer_priority"));
    gtk_tree_view_column_set_cell_data_func(priority_col, priority_renderer, renderPriority, NULL, NULL);

    GtkTreeViewColumn *state_col = GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "col_status"));
    GtkCellRenderer *state_renderer = GTK_CELL_RENDERER(gtk_builder_get_object(builder, "renderer_status"));
    gtk_tree_view_column_set_cell_data_func(state_col, state_renderer, renderState, NULL, NULL);

    // CLEAR THE STORE
    //  Prepare the store for new data

    RequestHeader req = {
        .commandId = htons(3),
        .payloadSize = htonl(strlen(currentUser.email) + 1)};
    sendRequest(clientFD, &req, currentUser.email);

    ResponseHeader resp;
    Ticket *tickets = malloc(100 * sizeof *tickets);
    receiveResponse(clientFD, &resp, tickets);

    uint32_t payloadBytes = ntohl(resp.payloadSize);
    int nTickets = payloadBytes / sizeof *tickets;

    for (int i = 0; i < nTickets; i++)
    {
        char date_buf[32];
        time_t ts = tickets[i].creationDate;
        struct tm tm;
        localtime_r(&ts, &tm);
        strftime(date_buf, sizeof date_buf, "%H:%M %d-%m-%Y", &tm);

        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           0, tickets[i].id,
                           1, tickets[i].title,
                           2, tickets[i].description,
                           3, date_buf,
                           4, tickets[i].priority,
                           5, tickets[i].state,
                           6, tickets[i].assignedAgent.email,
                           -1);
    }

    free(tickets);
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
    g_signal_connect(loginButton, "clicked", G_CALLBACK(onLoginButtonClicked), builder);

    GtkWidget *mainWindow = GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));
    g_signal_connect(mainWindow, "show", G_CALLBACK(onMainWindowShow), builder);

    GtkWidget *loginErrorLabel = GTK_WIDGET(gtk_builder_get_object(builder, "login_error_label"));

    gtk_widget_show_all(window);
    gtk_widget_hide(loginErrorLabel);

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