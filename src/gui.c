#include <netinet/in.h>
#include "../include/gui.h"
#include "../include/network_utils.h"

User currentUser;
int clientFD;

//TODO: Add a new renderer (like stateRendered) for CreationDate: keep epoch in table but display formatted

enum Columns {
    COL_ID,
    COL_TITLE,
    COL_DESCRIPTION,
    COL_CREATIONDATE,
    COL_PRIORITY,
    COL_STATE,
    COL_ASSIGNEDAGENT
};

char *stateToString(enum State state) {
    switch (state) {
        case CLOSED:
            return "Chiuso";
        case ASSIGNED:
            return "Assegnato";
        case OPEN:
            return "Aperto";
        default:
            return "Sconosciuto";
    }
}

char *priorityToString(enum Priority priority) {
    switch (priority) {
        case LOW:
            return "Bassa";
        case MEDIUM:
            return "Media";
        case HIGH:
            return "Alta";
        default:
            return "Sconosciuta";
    }
}

void onLoginButtonClicked(GtkButton *button, gpointer user_data) {
    GtkBuilder *builder = (GtkBuilder *) user_data;
    GtkWidget *emailEntry = GTK_WIDGET(gtk_builder_get_object(builder, "email_entry"));
    GtkWidget *passwordEntry = GTK_WIDGET(gtk_builder_get_object(builder, "password_entry"));
    GtkWidget *errorLabel = GTK_WIDGET(gtk_builder_get_object(builder, "login_error_label"));
    gtk_widget_hide(errorLabel);

    const char *email = gtk_entry_get_text(GTK_ENTRY(emailEntry));
    const char *password = gtk_entry_get_text(GTK_ENTRY(passwordEntry));

    if (strlen(email) == 0 || strlen(password) == 0) {
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

    if (ntohs(resp.statusCode) != 200) {
        gtk_label_set_text(GTK_LABEL(errorLabel), resp.message);
        gtk_widget_show(errorLabel);
    } else {
        GtkWidget *window = GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));
        gtk_widget_show_all(window);
        GtkWidget *loginWindow = GTK_WIDGET(gtk_builder_get_object(builder, "login_window"));
        gtk_widget_destroy(loginWindow);
    }
}

void renderPriority(GtkTreeViewColumn *col, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer data) {
    int priority;
    gtk_tree_model_get(model, iter, COL_PRIORITY, &priority, -1);
    g_object_set(renderer, "text", priorityToString(priority), NULL);
}

void renderState(GtkTreeViewColumn *col, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer data) {
    int state;
    gtk_tree_model_get(model, iter, COL_STATE, &state, -1);
    g_object_set(renderer, "text", stateToString(state), NULL);
}

void renderDate(GtkTreeViewColumn *col, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer data) {
    time_t creationDate = 0;
    gtk_tree_model_get(model, iter, COL_CREATIONDATE, &creationDate, -1);

    char date_buf[32] = "Invalid date";
    if (creationDate > 0) {
        struct tm tm;
        if (localtime_r(&creationDate, &tm)) {
            strftime(date_buf, sizeof date_buf, "%H:%M %d-%m-%Y", &tm);
        }
    }

    g_object_set(renderer, "text", date_buf, NULL);
}

void onDisconnectButtonClicked(GtkWidget *widget, gpointer user_data) {
    GtkBuilder *builder = GTK_BUILDER(user_data);
    GtkWidget *mainWindow = GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));

    gtk_main_quit();
    gtk_widget_destroy(mainWindow);
}

void onSaveTicketButtonClicked(GtkWidget *widget, gpointer user_data) {
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

    if (strlen(title) == 0) {
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
        .payloadSize = htonl(sizeof(newTicket))
    };

    sendRequest(clientFD, &req, &newTicket);

    ResponseHeader resp;
    receiveResponse(clientFD, &resp, NULL);

    if (ntohs(resp.statusCode) != 201) {
        gtk_label_set_text(GTK_LABEL(errorLabel), resp.message);
        gtk_widget_show(errorLabel);
        return;
    }

    gtk_entry_set_text(GTK_ENTRY(titleEntry), "");
    GtkTextBuffer *descBuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(descriptionText));
    gtk_text_buffer_set_text(descBuffer, "Descrizione del problema...", -1);
    gtk_widget_hide(errorLabel);

    GtkWidget *editTicketDialog = GTK_WIDGET(gtk_builder_get_object(builder, "edit_ticket_dialog"));
    gtk_widget_hide(editTicketDialog);

    onMainWindowShow(NULL, builder);
}

void onDiscardButtonClicked(GtkWidget *widget, gpointer user_data) {
    GtkBuilder *builder = GTK_BUILDER(user_data);

    GtkWidget *titleEntry = GTK_WIDGET(gtk_builder_get_object(builder, "ticket_title_entry"));
    gtk_entry_set_text(GTK_ENTRY(titleEntry), "");
    GtkWidget *descriptionText = GTK_WIDGET(gtk_builder_get_object(builder, "ticket_description_text"));
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(descriptionText));
    gtk_text_buffer_set_text(buffer, "Descrizione del problema...", -1);

    GtkWidget *editTicketDialog = GTK_WIDGET(gtk_builder_get_object(builder, "edit_ticket_dialog"));
    gtk_widget_hide(editTicketDialog);
}

void onAddTicketButtonClicked(GtkWidget *widget, gpointer user_data) {
    GtkBuilder *builder = GTK_BUILDER(user_data);
    GtkWidget *editTicketDialog = GTK_WIDGET(gtk_builder_get_object(builder, "edit_ticket_dialog"));
    gtk_window_set_title(GTK_WINDOW(editTicketDialog), "Aggiungi ticket");
    gtk_widget_show_all(editTicketDialog);
    GtkLabel *ticketEditErrorLabel = GTK_LABEL(gtk_builder_get_object(builder, "ticket_edit_error_label"));
    gtk_widget_hide(GTK_WIDGET(ticketEditErrorLabel));

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

void onMainWindowShow(GtkWidget *widget, gpointer user_data) {
    GtkBuilder *builder = GTK_BUILDER(user_data);

    GtkLabel *welcomeLabel = GTK_LABEL(gtk_builder_get_object(builder, "welcome_label"));
    char welcomeText[128];
    snprintf(welcomeText, sizeof(welcomeText), "Benvenuto/a, %s %s", currentUser.name, currentUser.surname);
    gtk_label_set_text(welcomeLabel, welcomeText);

    GtkWidget *addTicketButton = GTK_WIDGET(gtk_builder_get_object(builder, "add_ticket_button"));
    GtkWidget *disconnectButton = GTK_WIDGET(gtk_builder_get_object(builder, "disconnect_button"));
    GtkWidget *searchButton =  GTK_WIDGET(gtk_builder_get_object(builder, "search_button"));

    g_signal_connect(addTicketButton, "clicked", G_CALLBACK(onAddTicketButtonClicked), builder);
    g_signal_connect(disconnectButton, "clicked", G_CALLBACK(onDisconnectButtonClicked), builder);
    g_signal_connect(searchButton, "clicked", G_CALLBACK(onSearchButtonClicked), builder);

    GtkTreeView *ticketTreeView = GTK_TREE_VIEW(gtk_builder_get_object(builder, "treeview_items"));
    GtkListStore *store = GTK_LIST_STORE(gtk_builder_get_object(builder, "liststore_items"));
    gtk_list_store_clear(store);

    GtkTreeModel *filter_model = gtk_tree_model_filter_new(GTK_TREE_MODEL(store), NULL);
    gtk_tree_view_set_model(GTK_TREE_VIEW(ticketTreeView), filter_model);
    gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(filter_model), ticketFilterFunction, builder, NULL);

    GtkTreeIter iter;

    GtkTreeViewColumn *priority_col = GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "col_priority"));
    GtkCellRenderer *priority_renderer = GTK_CELL_RENDERER(gtk_builder_get_object(builder, "renderer_priority"));
    gtk_tree_view_column_set_cell_data_func(priority_col, priority_renderer, renderPriority, NULL, NULL);

    GtkTreeViewColumn *state_col = GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "col_status"));
    GtkCellRenderer *state_renderer = GTK_CELL_RENDERER(gtk_builder_get_object(builder, "renderer_status"));
    gtk_tree_view_column_set_cell_data_func(state_col, state_renderer, renderState, NULL, NULL);

    GtkTreeViewColumn *col_date = GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "col_date"));
    GtkCellRenderer *date_renderer = GTK_CELL_RENDERER(gtk_builder_get_object(builder, "renderer_date"));
    gtk_tree_view_column_set_cell_data_func(col_date, date_renderer, renderDate, NULL, NULL);

    RequestHeader req = {
        .commandId = htons(3),
        .payloadSize = htonl(strlen(currentUser.email) + 1)
    };
    sendRequest(clientFD, &req, currentUser.email);

    ResponseHeader resp;
    Ticket *tickets = malloc(100 * sizeof *tickets);
    receiveResponse(clientFD, &resp, tickets);

    uint32_t payloadBytes = ntohl(resp.payloadSize);
    int nTickets = payloadBytes / sizeof *tickets;

    for (int i = 0; i < nTickets; i++) {
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           COL_ID, tickets[i].id,
                           COL_TITLE, tickets[i].title,
                           COL_DESCRIPTION, tickets[i].description,
                           COL_CREATIONDATE, tickets[i].creationDate,
                           COL_PRIORITY, tickets[i].priority,
                           COL_STATE, tickets[i].state,
                           COL_ASSIGNEDAGENT, tickets[i].assignedAgent.email,
                           -1);
    }
    free(tickets);
}

void onSearchButtonClicked(GtkWidget *widget, gpointer user_data) {
    GtkBuilder *builder = GTK_BUILDER(user_data);
    GtkWidget *searchTicketDialog = GTK_WIDGET(gtk_builder_get_object(builder, "search_ticket_dialog"));
    gtk_widget_show_all(searchTicketDialog);
    GtkWidget *searchTicketButton = GTK_WIDGET(gtk_builder_get_object(builder, "search_ticket_button"));

    g_signal_handlers_disconnect_by_func(searchTicketButton, G_CALLBACK(updateTicketFilter), builder);
    g_signal_connect(searchTicketButton, "clicked", G_CALLBACK(updateTicketFilter), builder);
}

void updateTicketFilter(GtkWidget *widget, gpointer user_data) {
    GtkBuilder *builder = GTK_BUILDER(user_data);
    GtkTreeView *treeview = GTK_TREE_VIEW(gtk_builder_get_object(builder, "treeview_items"));
    GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER(gtk_tree_view_get_model(treeview));
    gtk_tree_model_filter_refilter(filter);
}


gboolean ticketFilterFunction(GtkTreeModel *model, GtkTreeIter *iter, gpointer data) {
    GtkBuilder *builder = GTK_BUILDER(data);
    GtkWidget *title_entry = GTK_WIDGET(gtk_builder_get_object(builder, "title_filter_entry"));
    GtkWidget *description_entry = GTK_WIDGET(gtk_builder_get_object(builder, "description_filter_entry"));
    GtkWidget *priority_combo = GTK_WIDGET(gtk_builder_get_object(builder, "priority_filter_combobox"));
    GtkWidget *state_combo = GTK_WIDGET(gtk_builder_get_object(builder, "state_filter_combobox"));
    GtkWidget *agent_combo = GTK_WIDGET(gtk_builder_get_object(builder, "agent_filter_combobox"));
    GtkWidget *calendar = GTK_WIDGET(gtk_builder_get_object(builder, "date_filter_calendar"));

    GtkWidget *enable_title = GTK_WIDGET(gtk_builder_get_object(builder, "enable_title_filter"));
    GtkWidget *enable_description = GTK_WIDGET(gtk_builder_get_object(builder, "enable_description_filter"));
    GtkWidget *enable_priority = GTK_WIDGET(gtk_builder_get_object(builder, "enable_priority_filter"));
    GtkWidget *enable_state = GTK_WIDGET(gtk_builder_get_object(builder, "enable_state_filter"));
    GtkWidget *enable_agent = GTK_WIDGET(gtk_builder_get_object(builder, "enable_agent_filter"));
    GtkWidget *enable_date = GTK_WIDGET(gtk_builder_get_object(builder, "enable_date_filter"));

    const char *title_filter = gtk_entry_get_text(GTK_ENTRY(title_entry));
    const char *description_filter = gtk_entry_get_text(GTK_ENTRY(description_entry));
    const int priority_id = gtk_combo_box_get_active(GTK_COMBO_BOX(priority_combo));
    const int state_id = gtk_combo_box_get_active(GTK_COMBO_BOX(state_combo));
    const char *agent_email = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(agent_combo));

    guint year, month, day;
    gtk_calendar_get_date(GTK_CALENDAR(calendar), &year, &month, &day);
    struct tm date = {0};
    date.tm_year = year - 1900;
    date.tm_mon = month;
    date.tm_mday = day;

    char *title, *desc, *agent;
    int priority, state;
    time_t creation_date;

    gtk_tree_model_get(model, iter,
        COL_TITLE, &title,
        COL_DESCRIPTION, &desc,
        COL_PRIORITY, &priority,
        COL_STATE, &state,
        COL_ASSIGNEDAGENT, &agent,
        COL_CREATIONDATE, &creation_date,
        -1);

    //OK
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enable_title)) && title_filter[0]) {
        char *title_lower = g_utf8_strdown(title, -1);
        char *filter_lower = g_utf8_strdown(title_filter, -1);

        gboolean match = g_strstr_len(title_lower, -1, filter_lower) != NULL;

        free(title_lower);
        free(filter_lower);

        if (!match) return FALSE;
    }

    //OK
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enable_description)) && description_filter[0]) {
        char *desc_lower = g_utf8_strdown(desc, -1);
        char *filter_lower = g_utf8_strdown(description_filter, -1);

        gboolean match = g_strstr_len(desc_lower, -1, filter_lower) != NULL;

        free(desc_lower);
        free(filter_lower);

        if (!match) return FALSE;
    }

    //OK
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enable_priority))) {
        if (priority != gtk_combo_box_get_active_id(GTK_COMBO_BOX(priority_combo))[0] - '0') return FALSE;
    }

    //OK
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enable_state))) {
        if (state != gtk_combo_box_get_active_id(GTK_COMBO_BOX(state_combo))[0] - '0') return FALSE;
    }

    //TODO: Populate list with agent's email
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enable_agent)) && agent_email && agent_email[0]) {
        if (g_strcmp0(agent_email, agent) != 0) return FALSE;
    }

    //OK
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enable_date))) {
        struct tm *ticket_date = localtime(&creation_date);
        if (ticket_date->tm_year != date.tm_year || ticket_date->tm_mon != date.tm_mon || ticket_date->tm_mday != date.tm_mday)
            return FALSE;
    }

    return TRUE;
}
