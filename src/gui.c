#include <netinet/in.h>
#include "../include/gui.h"
#include "../include/network_utils.h"

User currentUser;
int clientFD;

static gboolean initialized = FALSE;
static GtkListStore *store;
static GtkTreeModelFilter *filterModel;
static GtkTreeView *ticketTreeView;

enum Columns {
    COL_ID,
    COL_TITLE,
    COL_DESCRIPTION,
    COL_CREATIONDATE,
    COL_PRIORITY,
    COL_STATE,
    COL_CREATOR,
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

//CUSTOM RENDERERS
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

//WIDGET DELETION HANDLERS
gboolean onDeleteEvent(GtkWidget *widget, GdkEvent *event, gpointer data) {
    gtk_widget_hide(widget);
    return TRUE;
}

gboolean onMainWindowDeleteEvent(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
    if (clientFD >= 0) {
        close(clientFD);
        clientFD = -1;
    }

    gtk_main_quit();
    return FALSE;
}

//CALLBACKS
void onLoginButtonClicked(GtkButton *button, gpointer user_data) {
    GtkBuilder *builder = GTK_BUILDER(user_data);
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
        GtkWidget *loginWin = GTK_WIDGET(gtk_builder_get_object(builder, "login_window"));
        GtkWidget *mainWin  = GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));

        gtk_widget_hide(loginWin);
        gtk_widget_show_all(mainWin);
    }
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
    strncpy(newTicket.creator, currentUser.email, sizeof(newTicket.creator) - 1);
    strncpy(newTicket.assignedAgent, "", sizeof(newTicket.assignedAgent) - 1);

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

    refreshTicketList();
}

void onDiscardSearchButtonClicked(GtkWidget *widget, gpointer user_data) {
    GtkBuilder *builder = GTK_BUILDER(user_data);

    // Clear text entries
    GtkWidget *titleEntry = GTK_WIDGET(gtk_builder_get_object(builder, "title_filter_entry"));
    gtk_entry_set_text(GTK_ENTRY(titleEntry), "");
    GtkWidget *descriptionEntry = GTK_WIDGET(gtk_builder_get_object(builder, "description_filter_entry"));
    gtk_entry_set_text(GTK_ENTRY(descriptionEntry), "");

    // Reset combo boxes to first item
    GtkWidget *priorityCombo = GTK_WIDGET(gtk_builder_get_object(builder, "priority_filter_combobox"));
    gtk_combo_box_set_active(GTK_COMBO_BOX(priorityCombo), 0);
    GtkWidget *stateCombo = GTK_WIDGET(gtk_builder_get_object(builder, "state_filter_combobox"));
    gtk_combo_box_set_active(GTK_COMBO_BOX(stateCombo), 0);
    GtkWidget *agentCombo = GTK_WIDGET(gtk_builder_get_object(builder, "agent_filter_combobox"));
    gtk_combo_box_set_active(GTK_COMBO_BOX(agentCombo), 0);

    // Reset calendar to today
    GtkWidget *calendar = GTK_WIDGET(gtk_builder_get_object(builder, "date_filter_calendar"));
    GDateTime *now = g_date_time_new_now_local();
    gtk_calendar_select_month(GTK_CALENDAR(calendar), g_date_time_get_month(now) - 1, g_date_time_get_year(now));
    gtk_calendar_select_day(GTK_CALENDAR(calendar), g_date_time_get_day_of_month(now));
    g_date_time_unref(now);

    // Uncheck all filter enable checkboxes
    GtkWidget *enableTitle = GTK_WIDGET(gtk_builder_get_object(builder, "enable_title_filter"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enableTitle), FALSE);
    GtkWidget *enableDescription = GTK_WIDGET(gtk_builder_get_object(builder, "enable_description_filter"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enableDescription), FALSE);
    GtkWidget *enablePriority = GTK_WIDGET(gtk_builder_get_object(builder, "enable_priority_filter"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enablePriority), FALSE);
    GtkWidget *enableState = GTK_WIDGET(gtk_builder_get_object(builder, "enable_state_filter"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enableState), FALSE);
    GtkWidget *enableAgent = GTK_WIDGET(gtk_builder_get_object(builder, "enable_agent_filter"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enableAgent), FALSE);
    GtkWidget *enableDate = GTK_WIDGET(gtk_builder_get_object(builder, "enable_date_filter"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enableDate), FALSE);

    // Close the search dialog
    GtkWidget *searchTicketDialog = GTK_WIDGET(gtk_builder_get_object(builder, "search_ticket_dialog"));
    gtk_widget_hide(searchTicketDialog);
}

void onDiscardAddTicketButtonClicked(GtkWidget *widget, gpointer user_data) {
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
    g_signal_handlers_disconnect_by_func(discardButton, G_CALLBACK(onDiscardAddTicketButtonClicked), builder);
    g_signal_connect(discardButton, "clicked", G_CALLBACK(onDiscardAddTicketButtonClicked), builder);
}

void initMainWindow(GtkWidget *window, gpointer user_data) {
    if (initialized) return;
    GtkBuilder *builder = GTK_BUILDER(user_data);
    ticketTreeView = GTK_TREE_VIEW(gtk_builder_get_object(builder, "treeview_items"));
    store = GTK_LIST_STORE(gtk_builder_get_object(builder, "liststore_items"));
    GtkWidget *editDialog = GTK_WIDGET(gtk_builder_get_object(builder, "edit_ticket_dialog"));
    GtkWidget *searchDialog = GTK_WIDGET(gtk_builder_get_object(builder, "search_ticket_dialog"));

    //Connect signals callbacks
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit),NULL);
    g_signal_connect(gtk_builder_get_object(builder, "add_ticket_button"), "clicked", G_CALLBACK(onAddTicketButtonClicked), builder);
    g_signal_connect(gtk_builder_get_object(builder, "disconnect_button"), "clicked", G_CALLBACK(onDisconnectButtonClicked), builder);
    g_signal_connect(gtk_builder_get_object(builder, "search_button"), "clicked", G_CALLBACK(onSearchButtonClicked), builder);
    g_signal_connect(editDialog, "delete-event", G_CALLBACK(onDeleteEvent), NULL);
    g_signal_connect(searchDialog, "delete-event", G_CALLBACK(onDeleteEvent), NULL);

    //If agent or super agent, handle ticket selection. Otherwise disable creator column
    if (currentUser.role == SUPER_AGENT || currentUser.role == AGENT) {
        g_signal_connect(ticketTreeView, "row-activated", G_CALLBACK(onTicketRowSelected), builder);
        gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "add_ticket_button")), FALSE);
    } else {
        GtkTreeViewColumn *colCreator = GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "col_creator"));
        gtk_tree_view_column_set_visible(colCreator, FALSE);
    }

    //Add filter model to the tree view
    filterModel = GTK_TREE_MODEL_FILTER(gtk_tree_model_filter_new(GTK_TREE_MODEL(store), NULL));
    gtk_tree_model_filter_set_visible_func(filterModel,ticketFilterFunction, builder, NULL);
    gtk_tree_view_set_model(ticketTreeView, GTK_TREE_MODEL(filterModel));

    //Setup custom cell renderers
    gtk_tree_view_column_set_cell_data_func(GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "col_priority")), GTK_CELL_RENDERER(gtk_builder_get_object(builder, "renderer_priority")), renderPriority, NULL, NULL);
    gtk_tree_view_column_set_cell_data_func(GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "col_status")), GTK_CELL_RENDERER(gtk_builder_get_object(builder, "renderer_status")),renderState, NULL, NULL);
    gtk_tree_view_column_set_cell_data_func(GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "col_date")), GTK_CELL_RENDERER(gtk_builder_get_object(builder, "renderer_date")),renderDate, NULL, NULL);

    //Load tickets
    refreshTicketList();

    initialized = TRUE;
}

void refreshTicketList(void) {
    gtk_list_store_clear(store);

    RequestHeader req = {
        .commandId = htons(3),
        .payloadSize = htonl(strlen(currentUser.email) + 1)
    };
    sendRequest(clientFD, &req, currentUser.email);

    ResponseHeader resp;
    Ticket *tickets = malloc(100 * sizeof(Ticket));
    receiveResponse(clientFD, &resp, tickets);

    uint32_t payloadBytes = ntohl(resp.payloadSize);
    int nTickets = payloadBytes / sizeof(Ticket);

    GtkTreeIter iter;
    for (int i = 0; i < nTickets; i++) {
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           COL_ID, tickets[i].id,
                           COL_TITLE, tickets[i].title,
                           COL_DESCRIPTION, tickets[i].description,
                           COL_CREATIONDATE, tickets[i].creationDate,
                           COL_PRIORITY, tickets[i].priority,
                           COL_STATE, tickets[i].state,
                           COL_CREATOR, tickets[i].creator,
                           COL_ASSIGNEDAGENT, tickets[i].assignedAgent,
                           -1);
    }
    free(tickets);
}

void onSearchButtonClicked(GtkWidget *widget, gpointer user_data) {
    GtkBuilder *builder = GTK_BUILDER(user_data);
    GtkWidget *searchTicketDialog = GTK_WIDGET(gtk_builder_get_object(builder, "search_ticket_dialog"));
    gtk_widget_show_all(searchTicketDialog);
    GtkWidget *searchTicketButton = GTK_WIDGET(gtk_builder_get_object(builder, "search_ticket_button"));
    GtkWidget *discardTicketButton = GTK_WIDGET(gtk_builder_get_object(builder, "discard_search_button"));

    g_signal_handlers_disconnect_by_func(searchTicketButton, G_CALLBACK(updateTicketFilter), NULL);
    g_signal_connect(searchTicketButton, "clicked", G_CALLBACK(updateTicketFilter), NULL);
    g_signal_handlers_disconnect_by_func(discardTicketButton, G_CALLBACK(onDiscardSearchButtonClicked), builder);
    g_signal_connect(discardTicketButton, "clicked", G_CALLBACK(onDiscardSearchButtonClicked), builder);

    GtkWidget *agentFilterCombobox = GTK_WIDGET(gtk_builder_get_object(builder, "agent_filter_combobox"));

    GtkListStore *listStore = GTK_LIST_STORE(gtk_builder_get_object(builder, "liststore_items"));
    GtkTreeIter iter;

    GtkListStore *agentListStore = GTK_LIST_STORE(gtk_builder_get_object(builder, "agents_liststore"));
    gtk_list_store_clear(agentListStore);

    int valid = gtk_tree_model_get_iter_first(listStore, &iter);
    GList *existingAgents = NULL;

    while (valid) {
        char *agentEmail = NULL;
        gtk_tree_model_get(listStore, &iter, COL_ASSIGNEDAGENT, &agentEmail, -1);

        if (agentEmail && strlen(agentEmail) > 0 && !g_list_find_custom(existingAgents, agentEmail,
                                                                        (GCompareFunc) g_strcmp0)) {
            existingAgents = g_list_append(existingAgents, g_strdup(agentEmail));
            GtkTreeIter agentIter;
            gtk_list_store_append(agentListStore, &agentIter);
            gtk_list_store_set(agentListStore, &agentIter, 0, agentEmail, -1);
        }
        valid = gtk_tree_model_iter_next(listStore, &iter);
    }
}

void updateTicketFilter(GtkWidget *widget, gpointer user_data) {
    GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER(gtk_tree_view_get_model(ticketTreeView));
    gtk_tree_model_filter_refilter(filter);
}

int ticketFilterFunction(GtkTreeModel *model, GtkTreeIter *iter, gpointer data) {
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
    GtkTreeIter combo_iter;
    GtkTreeModel *combo_model = gtk_combo_box_get_model(GTK_COMBO_BOX(agent_combo));
    char *selected_agent = NULL;

    if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(agent_combo), &combo_iter)) {
        gtk_tree_model_get(combo_model, &combo_iter, 0, &selected_agent, -1);
    }

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

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enable_title)) && title_filter[0]) {
        char *title_lower = g_utf8_strdown(title, -1);
        char *filter_lower = g_utf8_strdown(title_filter, -1);

        gboolean match = g_strstr_len(title_lower, -1, filter_lower) != NULL;

        free(title_lower);
        free(filter_lower);

        if (!match)
            return 0;
    }

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enable_description)) && description_filter[0]) {
        char *desc_lower = g_utf8_strdown(desc, -1);
        char *filter_lower = g_utf8_strdown(description_filter, -1);

        gboolean match = g_strstr_len(desc_lower, -1, filter_lower) != NULL;

        free(desc_lower);
        free(filter_lower);

        if (!match)
            return 0;
    }

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enable_priority))) {
        if (priority != gtk_combo_box_get_active_id(GTK_COMBO_BOX(priority_combo))[0] - '0')
            return 0;
    }

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enable_state))) {
        if (state != gtk_combo_box_get_active_id(GTK_COMBO_BOX(state_combo))[0] - '0')
            return 0;
    }

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enable_agent)) && selected_agent != NULL) {
        if (g_strcmp0(selected_agent, agent) != 0) {
            g_free(selected_agent);
            return 0;
        }
        g_free(selected_agent);
    }

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enable_date))) {
        struct tm *ticket_date = localtime(&creation_date);
        if (ticket_date->tm_year != date.tm_year || ticket_date->tm_mon != date.tm_mon || ticket_date->tm_mday != date.
            tm_mday)
            return 0;
    }

    return 1;
}

void onTicketRowSelected(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data) {
    GtkBuilder *builder = GTK_BUILDER(user_data);
    GtkTreeModel *model = gtk_tree_view_get_model(treeview);
    GtkTreeIter iter;

    if (!gtk_tree_model_get_iter(model, &iter, path))
        return;

    gint id, priority, state;
    gchar *title = NULL, *description = NULL, *agent = NULL, *creator = NULL;
    time_t creationDate;

    gtk_tree_model_get(model, &iter,
                       COL_ID, &id,
                       COL_TITLE, &title,
                       COL_DESCRIPTION, &description,
                       COL_PRIORITY, &priority,
                       COL_STATE, &state,
                       COL_CREATIONDATE, &creationDate,
                       COL_CREATOR, &creator,
                       COL_ASSIGNEDAGENT, &agent,
                       -1);


    GtkWidget *editTicketDialog = GTK_WIDGET(gtk_builder_get_object(builder, "edit_ticket_dialog"));
    gtk_window_set_title(GTK_WINDOW(editTicketDialog), "Modifica ticket");
    gtk_widget_show_all(editTicketDialog);

    GtkLabel *ticketEditErrorLabel = GTK_LABEL(gtk_builder_get_object(builder, "ticket_edit_error_label"));
    gtk_widget_hide(GTK_WIDGET(ticketEditErrorLabel));

    GtkWidget *priorityComboBox = GTK_WIDGET(gtk_builder_get_object(builder, "priority_combobox"));
    GtkWidget *stateCombobox = GTK_WIDGET(gtk_builder_get_object(builder, "state_combobox"));
    GtkWidget *agentCombobox = GTK_WIDGET(gtk_builder_get_object(builder, "agent_combobox"));
    GtkWidget *titleEntry = GTK_WIDGET(gtk_builder_get_object(builder, "ticket_title_entry"));
    GtkWidget *descriptionTextView = GTK_WIDGET(gtk_builder_get_object(builder, "ticket_description_text"));

    //Salviamo i dati del ticket nell'oggetto GObject per poterli recuperare in seguito
    g_object_set_data(G_OBJECT(editTicketDialog), "ticket-id", GINT_TO_POINTER(id));
    g_object_set_data_full(G_OBJECT(editTicketDialog), "ticket-creator", g_strdup(currentUser.email), g_free);

    gtk_entry_set_text(GTK_ENTRY(titleEntry), title);
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(descriptionTextView));
    gtk_text_buffer_set_text(buffer, description, -1);

    GtkListStore *agentListStore = GTK_LIST_STORE(gtk_builder_get_object(builder, "agents_liststore"));
    gtk_list_store_clear(agentListStore);

    if (currentUser.role == SUPER_AGENT) {
        gtk_widget_set_sensitive(agentCombobox, 1);

        GtkTreeIter agent_iter;
        enum Role role = AGENT;

        RequestHeader req = {
            .commandId = htons(5),
            .payloadSize = htonl(sizeof(role))
        };
        sendRequest(clientFD, &req, &role);

        ResponseHeader resp;
        User *agents = calloc(100, sizeof(User));
        receiveResponse(clientFD, &resp, agents);

        uint32_t payloadBytes = ntohl(resp.payloadSize);
        int nAgents = payloadBytes / sizeof(User);

        for (int i = 0; i < nAgents; i++) {
            gtk_list_store_append(agentListStore, &agent_iter);
            gtk_list_store_set(agentListStore, &agent_iter, 0, agents[i].email, -1);
        }

        if (agent && strlen(agent) > 0) {
            gtk_combo_box_set_active_id(GTK_COMBO_BOX(agentCombobox), agent);
        } else {
            gtk_combo_box_set_active(GTK_COMBO_BOX(agentCombobox), -1);
        }
    } else {
        gtk_widget_set_sensitive(agentCombobox, 0);

        GtkTreeIter agent_iter;
        gtk_list_store_append(agentListStore, &agent_iter);
        gtk_list_store_set(agentListStore, &agent_iter, 0, agent, -1);
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(agentCombobox), agent);
    }

    gchar *priority_id = g_strdup_printf("%d", priority);
    gchar *state_id = g_strdup_printf("%d", state);

    gtk_combo_box_set_active_id(GTK_COMBO_BOX(priorityComboBox), priority_id);
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(stateCombobox), state_id);

    g_free(priority_id);
    g_free(state_id);

    gtk_widget_set_sensitive(titleEntry, 0);
    gtk_widget_set_sensitive(descriptionTextView, 0);
    gtk_widget_set_sensitive(priorityComboBox, 1);
    gtk_widget_set_sensitive(stateCombobox, 1);

    // Connect buttons
    GtkWidget *saveButton = GTK_WIDGET(gtk_builder_get_object(builder, "save_ticket_button"));
    g_signal_handlers_disconnect_by_func(saveButton, G_CALLBACK(onSaveEditedTicketButtonClicked), builder);
    g_signal_connect(saveButton, "clicked", G_CALLBACK(onSaveEditedTicketButtonClicked), builder);

    GtkWidget *discardButton = GTK_WIDGET(gtk_builder_get_object(builder, "discard_ticket_button"));
    g_signal_handlers_disconnect_by_func(discardButton, G_CALLBACK(onDiscardAddTicketButtonClicked), builder);
    g_signal_connect(discardButton, "clicked", G_CALLBACK(onDiscardAddTicketButtonClicked), builder);

    // Clean up
    g_free(title);
    g_free(description);
    g_free(agent);
}

void onSaveEditedTicketButtonClicked(GtkWidget *widget, gpointer user_data) {
    GtkBuilder *builder = GTK_BUILDER(user_data);

    // 1) Get all the widgets
    GtkWidget *dialog = GTK_WIDGET(gtk_builder_get_object(builder, "edit_ticket_dialog"));
    GtkWidget *titleEntry = GTK_WIDGET(gtk_builder_get_object(builder, "ticket_title_entry"));
    GtkWidget *descriptionText = GTK_WIDGET(gtk_builder_get_object(builder, "ticket_description_text"));
    GtkWidget *priorityCombo = GTK_WIDGET(gtk_builder_get_object(builder, "priority_combobox"));
    GtkWidget *stateCombo = GTK_WIDGET(gtk_builder_get_object(builder, "state_combobox"));
    GtkWidget *agentCombo = GTK_WIDGET(gtk_builder_get_object(builder, "agent_combobox"));
    GtkLabel *errorLabel = GTK_LABEL(gtk_builder_get_object(builder, "ticket_edit_error_label"));

    // 3) Extract new values
    const char *new_title = gtk_entry_get_text(GTK_ENTRY(titleEntry));

    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(descriptionText));
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(buf, &start);
    gtk_text_buffer_get_end_iter(buf, &end);
    char *new_desc = gtk_text_buffer_get_text(buf, &start, &end, FALSE);

    const char *priority_id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(priorityCombo));
    const char *state_id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(stateCombo));
    const char *agent_email = gtk_combo_box_get_active_id(GTK_COMBO_BOX(agentCombo));

    gint ticket_id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(dialog), "ticket-id"));
    const char *creator = g_object_get_data(G_OBJECT(dialog), "ticket-creator");

    // 5) Build the Ticket struct
    Ticket edited = {0};
    edited.id = ticket_id;
    strncpy(edited.title, new_title, sizeof(edited.title) - 1);
    strncpy(edited.description, new_desc, sizeof(edited.description) - 1);
    edited.priority = atoi(priority_id);
    edited.state = atoi(state_id);
    edited.creationDate = time(NULL);
    strncpy(edited.creator, creator, sizeof(edited.creator) - 1);
    strncpy(edited.assignedAgent, agent_email, sizeof(edited.assignedAgent) - 1);

    g_free(new_desc);

    // 6) Send update request
    RequestHeader req = {
        .commandId = htons(4), // your “update ticket” ID
        .payloadSize = htonl(sizeof(Ticket))
    };
    sendRequest(clientFD, &req, &edited);

    ResponseHeader resp;
    receiveResponse(clientFD, &resp, NULL);
    if (ntohs(resp.statusCode) != 200) {
        gtk_label_set_text(errorLabel, resp.message);
        gtk_widget_show(GTK_WIDGET(errorLabel));
        return;
    }

    // 7) Success! close dialog and reload list
    gtk_widget_hide(dialog);
    refreshTicketList();
}
