#include "../include/gui.h"
#include <netinet/in.h>
#include "../include/network_utils.h"

User currentUser;
int clientFD;

static gboolean initialized = FALSE;
static GtkListStore *store;
static GtkTreeModelFilter *filterModel;
static GtkTreeView *ticketTreeView;
static GtkWindow *mainWindow = NULL;

enum Columns { COL_ID, COL_TITLE, COL_DESCRIPTION, COL_CREATIONDATE, COL_PRIORITY, COL_STATE, COL_CREATOR, COL_ASSIGNEDAGENT };

char *stateToString(const enum State state) {
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

char *priorityToString(const enum Priority priority) {
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

// CUSTOM RENDERERS

/**
 * @brief Set the renderer's text based on the priority of a row in a model
 * @param col Pointer to the GtkTreeViewColumn associated with the renderer - not used
 * @param renderer Pointer to the GtkCellRenderer used to draw the cell
 * @param model Pointer to the GtkTreeModel containing the data
 * @param iter Pointer to the GtkTreeIter indicating the current row in the model
 * @param userData Pointer to additional user data - not used
 */
void renderPriority(GtkTreeViewColumn *col, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer userData) {
	int priority;
	gtk_tree_model_get(model, iter, COL_PRIORITY, &priority, -1);
	g_object_set(renderer, "text", priorityToString(priority), NULL);
}

/**
 * @brief Set the renderer's text based on the state of a row in a model
 * @param col Pointer to the GtkTreeViewColumn associated with the renderer - not used
 * @param renderer Pointer to the GtkCellRenderer used to draw the cell
 * @param model Pointer to the GtkTreeModel containing the data
 * @param iter Pointer to the GtkTreeIter indicating the current row in the model
 * @param userData Pointer to additional user data - not used
 */
void renderState(GtkTreeViewColumn *col, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer userData) {
	int state;
	gtk_tree_model_get(model, iter, COL_STATE, &state, -1);
	g_object_set(renderer, "text", stateToString(state), NULL);
}

/**
 * @brief Set the renderer's text to a formatted creation date from the model
 * @param col Pointer to the GtkTreeViewColumn associated with the renderer - not used
 * @param renderer Pointer to the GtkCellRenderer used to draw the cell
 * @param model Pointer to the GtkTreeModel containing the data
 * @param iter Pointer to the GtkTreeIter indicating the current row in the model
 * @param userData Pointer to additional user data - not used
 */
void renderDate(GtkTreeViewColumn *col, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer userData) {
	time_t creationDate = 0;
	gtk_tree_model_get(model, iter, COL_CREATIONDATE, &creationDate, -1);

	char dateBuffer[32] = "Invalid date";
	if (creationDate > 0) {
		struct tm tm;
		if (localtime_r(&creationDate, &tm)) {
			strftime(dateBuffer, sizeof dateBuffer, "%H:%M %d-%m-%Y", &tm);
		}
	}

	g_object_set(renderer, "text", dateBuffer, NULL);
}

/**
 * @brief Limit text insertion into a GtkTextBuffer to a maximum character count
 * @param buffer Pointer to the GtkTextBuffer receiving the text
 * @param location Pointer to a GtkTextIter indicating where the insertion occurs
 * @param text Pointer to the UTF_8 text being inserted
 * @param len Number of bytes in @p text (not necessarily characters).
 * @param userData Pointer containing the character limit, passed as a GPOINTER_TO_INT
 */
void onTextBufferInsertText(GtkTextBuffer *buffer, GtkTextIter *location, const gchar *text, const gint len, gpointer userData) {
	const gint limit = GPOINTER_TO_INT(userData);
	const gint currentLength = gtk_text_buffer_get_char_count(buffer);

	if (currentLength >= limit) {
		g_signal_stop_emission_by_name(buffer, "insert-text");
		return;
	}

	if (currentLength + len > limit) {
		const gint allowed = limit - currentLength;
		g_signal_handlers_block_by_func(buffer, G_CALLBACK(onTextBufferInsertText), userData);
		gtk_text_buffer_insert(buffer, location, text, allowed);
		g_signal_handlers_unblock_by_func(buffer, G_CALLBACK(onTextBufferInsertText), userData);
		g_signal_stop_emission_by_name(buffer, "insert-text");
	}
}

// WIDGET DELETION HANDLERS

/**
 * @brief Handle the delete-event signal by hiding instead of destroying it.
 * @param widget Pointer to the GtkWidget that received the delete-event
 * @param event Pointer to the GdkEvent triggering the signal (destroying the window) - not used
 * @param data Pointer to additional user data - not used
 * @returns TRUE to stop further handling of the event and prevent the dafault destruction behavior
 */
gboolean onDeleteEvent(GtkWidget *widget, GdkEvent *event, gpointer data) {
	gtk_widget_hide(widget);
	return TRUE;
}

/**
 * @brief Handle the main window's delete-event by closing the client connection and terminating the GTK main loop
 * @param widget Pointer to the GtkWidget that received the delete-event
 * @param event Pointer to the GdkEvent triggering the signal (destroying the window) - not used
 * @param userData Pointer to additional user data - not used
 * @returns FALSE to propagate the event further and allow the default destruction behavior
 */
gboolean onMainWindowDeleteEvent(GtkWidget *widget, GdkEvent *event, gpointer userData) {
	if (clientFD >= 0) {
		close(clientFD);
		clientFD = -1;
	}

	gtk_main_quit();
	return FALSE;
}

// CALLBACKS

/**
 * @brief Callback for the login button click event; handles user login
 * @param button Pointer to the GtkButton that was clicked (the login button)
 * @param userData Pointer to user data passed during signal connection
 */
void onLoginButtonClicked(GtkButton *button, gpointer userData) {
	GtkBuilder *builder = GTK_BUILDER(userData);
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

	yyjson_mut_doc *req = yyjson_mut_doc_new(NULL);
	yyjson_mut_val *root = yyjson_mut_obj(req);
	yyjson_mut_doc_set_root(req, root);

	yyjson_mut_obj_add_str(req, root, "jsonrpc", "2.0");
	yyjson_mut_obj_add_str(req, root, "method", "login");

	yyjson_mut_val *params = yyjson_mut_obj(req);
	yyjson_mut_obj_add_str(req, params, "email", email);
	yyjson_mut_obj_add_str(req, params, "password", password);
	yyjson_mut_obj_add_val(req, root, "params", params);

	yyjson_mut_obj_add_int(req, root, "id", 1);

	if (sendJsonRpcMessage(clientFD, req) < 0) {
		yyjson_mut_doc_free(req);
		showErrorDialog(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(button))), "Errore di rete durante il login");
		return;
	}
	yyjson_mut_doc_free(req);

	yyjson_doc *resp = NULL;
	if (receiveJsonRpcMessage(clientFD, &resp) < 0) {
		showErrorDialog(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(button))), "Errore di rete durante il login");
		return;
	}

	yyjson_val *responseRoot = yyjson_doc_get_root(resp);
	yyjson_val *errorVal = yyjson_obj_get(responseRoot, "error");
	yyjson_val *resultVal = yyjson_obj_get(responseRoot, "result");

	if (errorVal) {
		yyjson_val *messageVal = yyjson_obj_get(errorVal, "message");
		const char *errorMessage = messageVal ? yyjson_get_str(messageVal) : "Errore sconosciuto dal server";
		gtk_label_set_text(GTK_LABEL(errorLabel), errorMessage);
		gtk_widget_show(errorLabel);
		yyjson_doc_free(resp);
		return;
	}

	if (resultVal) {
		const char *emailStr = yyjson_get_str(yyjson_obj_get(resultVal, "email"));
		const char *nameStr = yyjson_get_str(yyjson_obj_get(resultVal, "name"));
		const char *surnameStr = yyjson_get_str(yyjson_obj_get(resultVal, "surname"));
		strncpy(currentUser.name, nameStr, sizeof(currentUser.name) - 1);
		strncpy(currentUser.surname, surnameStr, sizeof(currentUser.surname) - 1);
		strncpy(currentUser.email, emailStr, sizeof(currentUser.email) - 1);
		currentUser.role = yyjson_get_int(yyjson_obj_get(resultVal, "role"));
		GtkWidget *loginWin = GTK_WIDGET(gtk_builder_get_object(builder, "login_window"));
		GtkWidget *mainWin = GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));
		gtk_widget_hide(loginWin);
		gtk_widget_show_all(mainWin);
	} else {
		gtk_label_set_text(GTK_LABEL(errorLabel), "Risposta server non valida");
		gtk_widget_show(errorLabel);
	}

	yyjson_doc_free(resp);
}


/**
 * @brief Callaback for the disconnect button click event
 * @param widget Pointer to the GtkWidget that was clicked (the disconnect button) - not used
 * @param userData Pointer to user data passed during signal connection
 */
void onDisconnectButtonClicked(GtkWidget *widget, gpointer userData) {
	GtkBuilder *builder = GTK_BUILDER(userData);
	GtkWidget *mainWindow = GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));

	gtk_main_quit();
	gtk_widget_destroy(mainWindow);
}

/**
 * @brief Callback for the save ticket button click event
 * @param widget Pointer to the GtkWidget that was clicked (the save ticket button) - not used
 * @param userData Pointer to user data passed during signal connection
 */
void onSaveTicketButtonClicked(GtkWidget *widget, gpointer userData) {
	GtkBuilder *builder = GTK_BUILDER(userData);
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

	yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
	yyjson_mut_val *root = yyjson_mut_obj(doc);
	yyjson_mut_doc_set_root(doc, root);

	yyjson_mut_obj_add_str(doc, root, "jsonrpc", "2.0");
	yyjson_mut_obj_add_str(doc, root, "method", "createTicket");
	yyjson_mut_obj_add_int(doc, root, "id", 2);

	yyjson_mut_val *params = yyjson_mut_obj(doc);
	yyjson_mut_obj_add_str(doc, params, "title", title);
	yyjson_mut_obj_add_str(doc, params, "description", description);
	yyjson_mut_obj_add_int(doc, params, "priority", LOW);
	yyjson_mut_obj_add_int(doc, params, "state", OPEN);
	yyjson_mut_obj_add_uint(doc, params, "creationDate", (long) time(NULL));
	yyjson_mut_obj_add_str(doc, params, "creator", currentUser.email);
	yyjson_mut_obj_add_str(doc, params, "assignedAgent", "");
	yyjson_mut_obj_add_val(doc, root, "params", params);

	if (sendJsonRpcMessage(clientFD, doc) < 0) {
		showErrorDialog(mainWindow, "Errore di rete durante la creazione del ticket");
		g_free(description);
		return;
	}

	yyjson_doc *resp = NULL;
	if (receiveJsonRpcMessage(clientFD, &resp) < 0) {
		showErrorDialog(mainWindow, "Errore di rete durante la creazione del ticket");
		g_free(description);
		return;
	}

	yyjson_val *responseRoot = yyjson_doc_get_root(resp);
	yyjson_val *errorVal = yyjson_obj_get(responseRoot, "error");

	if (errorVal) {
		gtk_label_set_text(GTK_LABEL(errorLabel), yyjson_get_str(yyjson_obj_get(errorVal, "message")));
		gtk_widget_show(errorLabel);
		g_free(description);
		return;
	}

	g_free(description);
	gtk_entry_set_text(GTK_ENTRY(titleEntry), "");
	GtkTextBuffer *descBuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(descriptionText));
	gtk_text_buffer_set_text(descBuffer, "Descrizione del problema...", -1);
	gtk_widget_hide(errorLabel);

	GtkWidget *editTicketDialog = GTK_WIDGET(gtk_builder_get_object(builder, "edit_ticket_dialog"));
	gtk_widget_hide(editTicketDialog);

	refreshTicketList();
}

/**
 * @brief Callback for the discard search button click event
 * @param widget Pointer to the GtkWidget that was clicked (the discard search button) - not used
 * @param userData Pointer to user data passed during signal connection
 */
void onDiscardSearchButtonClicked(GtkWidget *widget, gpointer userData) {
	GtkBuilder *builder = GTK_BUILDER(userData);
	GtkWidget *titleEntry = GTK_WIDGET(gtk_builder_get_object(builder, "title_filter_entry"));
	gtk_entry_set_text(GTK_ENTRY(titleEntry), "");
	GtkWidget *descriptionEntry = GTK_WIDGET(gtk_builder_get_object(builder, "description_filter_entry"));
	gtk_entry_set_text(GTK_ENTRY(descriptionEntry), "");

	GtkWidget *priorityCombo = GTK_WIDGET(gtk_builder_get_object(builder, "priority_filter_combobox"));
	gtk_combo_box_set_active(GTK_COMBO_BOX(priorityCombo), 0);
	GtkWidget *stateCombo = GTK_WIDGET(gtk_builder_get_object(builder, "state_filter_combobox"));
	gtk_combo_box_set_active(GTK_COMBO_BOX(stateCombo), 0);
	GtkWidget *agentCombo = GTK_WIDGET(gtk_builder_get_object(builder, "agent_filter_combobox"));
	gtk_combo_box_set_active(GTK_COMBO_BOX(agentCombo), 0);

	GtkWidget *calendar = GTK_WIDGET(gtk_builder_get_object(builder, "date_filter_calendar"));
	GDateTime *now = g_date_time_new_now_local();
	gtk_calendar_select_month(GTK_CALENDAR(calendar), g_date_time_get_month(now) - 1, g_date_time_get_year(now));
	gtk_calendar_select_day(GTK_CALENDAR(calendar), g_date_time_get_day_of_month(now));
	g_date_time_unref(now);

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

	GtkWidget *searchTicketDialog = GTK_WIDGET(gtk_builder_get_object(builder, "search_ticket_dialog"));
	gtk_widget_hide(searchTicketDialog);
}

/**
 * @brief Callback for the discard add ticket button click event
 * @param widget Pointer to the GtkWidget that was clicked (the discard add ticket button) - not used
 * @param userData Pointer to user data passed during signal connection
 */
void onDiscardAddTicketButtonClicked(GtkWidget *widget, gpointer userData) {
	GtkBuilder *builder = GTK_BUILDER(userData);

	GtkWidget *titleEntry = GTK_WIDGET(gtk_builder_get_object(builder, "ticket_title_entry"));
	gtk_entry_set_text(GTK_ENTRY(titleEntry), "");
	GtkWidget *descriptionText = GTK_WIDGET(gtk_builder_get_object(builder, "ticket_description_text"));
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(descriptionText));
	gtk_text_buffer_set_text(buffer, "Descrizione del problema...", -1);

	GtkWidget *editTicketDialog = GTK_WIDGET(gtk_builder_get_object(builder, "edit_ticket_dialog"));
	gtk_widget_hide(editTicketDialog);
}

/**
 * @brief Callback for the add ticket button click event
 * @param widget Pointer to the GtkWidget that was clicked (the discard add ticket button) - not used
 * @param userData Pointer to user data passed during signal connection
 */
void onAddTicketButtonClicked(GtkWidget *widget, gpointer userData) {
	GtkBuilder *builder = GTK_BUILDER(userData);
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
	g_signal_connect(gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtk_builder_get_object(builder, "ticket_description_text"))), "insert-text", G_CALLBACK(onTextBufferInsertText), GINT_TO_POINTER(2048));
}

/**
 * @brief Initialize the main application window and its related UI components
 * @param window Pointer to the GtkWidget representing the main application window
 * @param userData Pointer to user data passed during initialization
 */
void initMainWindow(GtkWidget *window, gpointer userData) {
	if (initialized) {
		return;
	}
	mainWindow = GTK_WINDOW(window);
	GtkBuilder *builder = GTK_BUILDER(userData);
	ticketTreeView = GTK_TREE_VIEW(gtk_builder_get_object(builder, "treeview_items"));
	store = GTK_LIST_STORE(gtk_builder_get_object(builder, "liststore_items"));
	GtkWidget *editDialog = GTK_WIDGET(gtk_builder_get_object(builder, "edit_ticket_dialog"));
	GtkWidget *searchDialog = GTK_WIDGET(gtk_builder_get_object(builder, "search_ticket_dialog"));
	GtkLabel *welcomeLabel = GTK_LABEL(gtk_builder_get_object(builder, "welcome_label"));
	char welcomeText[80];

	snprintf(welcomeText, sizeof(welcomeText), "Benvenuto/a %s %s", currentUser.name, currentUser.surname);
	gtk_label_set_text(welcomeLabel, welcomeText);

	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(gtk_builder_get_object(builder, "add_ticket_button"), "clicked", G_CALLBACK(onAddTicketButtonClicked), builder);
	g_signal_connect(gtk_builder_get_object(builder, "disconnect_button"), "clicked", G_CALLBACK(onDisconnectButtonClicked), builder);
	g_signal_connect(gtk_builder_get_object(builder, "search_button"), "clicked", G_CALLBACK(onSearchButtonClicked), builder);
	g_signal_connect(gtk_builder_get_object(builder, "refresh_button"), "clicked", G_CALLBACK(refreshTicketList), NULL);
	g_signal_connect(editDialog, "delete-event", G_CALLBACK(onDeleteEvent), NULL);
	g_signal_connect(searchDialog, "delete-event", G_CALLBACK(onDeleteEvent), NULL);

	if (currentUser.role == SUPER_AGENT || currentUser.role == AGENT) {
		g_signal_connect(ticketTreeView, "row-activated", G_CALLBACK(onTicketRowSelected), builder);
		gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "add_ticket_button")), FALSE);
	} else {
		GtkTreeViewColumn *colCreator = GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "col_creator"));
		gtk_tree_view_column_set_visible(colCreator, FALSE);
	}

	filterModel = GTK_TREE_MODEL_FILTER(gtk_tree_model_filter_new(GTK_TREE_MODEL(store), NULL));
	gtk_tree_model_filter_set_visible_func(filterModel, ticketFilterFunction, builder, NULL);

	GtkTreeModel *sortModel = gtk_tree_model_sort_new_with_model(GTK_TREE_MODEL(filterModel));
	gtk_tree_view_set_model(GTK_TREE_VIEW(ticketTreeView), sortModel);
	g_object_set_data(G_OBJECT(ticketTreeView), "sort_model", sortModel);

	GtkTreeSortable *sortable = GTK_TREE_SORTABLE(sortModel);
	gtk_tree_sortable_set_sort_column_id(sortable, COL_PRIORITY, GTK_SORT_ASCENDING);
	gtk_tree_sortable_set_sort_column_id(sortable, COL_STATE, GTK_SORT_ASCENDING);
	gtk_tree_sortable_set_sort_column_id(sortable, COL_CREATIONDATE, GTK_SORT_ASCENDING);
	gtk_tree_sortable_set_sort_column_id(sortable, COL_ID, GTK_SORT_ASCENDING);

	gtk_tree_view_column_set_cell_data_func(GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "col_priority")), GTK_CELL_RENDERER(gtk_builder_get_object(builder, "renderer_priority")), renderPriority, NULL, NULL);
	gtk_tree_view_column_set_cell_data_func(GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "col_status")), GTK_CELL_RENDERER(gtk_builder_get_object(builder, "renderer_status")), renderState, NULL, NULL);
	gtk_tree_view_column_set_cell_data_func(GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "col_date")), GTK_CELL_RENDERER(gtk_builder_get_object(builder, "renderer_date")), renderDate, NULL, NULL);
	GtkCellRenderer *rendererDescription = GTK_CELL_RENDERER(gtk_builder_get_object(builder, "renderer_desc"));
	g_object_set(rendererDescription, "wrap-mode", PANGO_WRAP_WORD_CHAR, "wrap-width", 300, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	refreshTicketList();

	initialized = TRUE;
}

/**
 * @brief Request and refresh the ticket list from the server
 */
void refreshTicketList() {
	yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
	yyjson_mut_val *root = yyjson_mut_obj(doc);
	yyjson_mut_doc_set_root(doc, root);

	yyjson_mut_obj_add_str(doc, root, "jsonrpc", "2.0");
	yyjson_mut_obj_add_str(doc, root, "method", "getTickets");
	yyjson_mut_obj_add_int(doc, root, "id", 3);

	yyjson_mut_val *params = yyjson_mut_obj(doc);
	yyjson_mut_obj_add_str(doc, params, "email", currentUser.email);
	yyjson_mut_obj_add_val(doc, root, "params", params);


	if (sendJsonRpcMessage(clientFD, doc) < 0) {
		showErrorDialog(mainWindow, "Errore di rete durante la richiesta dei ticket");
		return;
	}

	yyjson_doc *responseDoc;

	if (receiveJsonRpcMessage(clientFD, &responseDoc) < 0) {
		showErrorDialog(mainWindow, "Errore di rete durante il recupero dei ticket");
		return;
	}

	gtk_list_store_clear(store);

	yyjson_val *responseRoot = yyjson_doc_get_root(responseDoc);
	yyjson_val *result = yyjson_obj_get(responseRoot, "result");

	const uint32_t nTickets = yyjson_arr_size(result);
	GtkTreeIter iter;

	for (size_t i = 0; i < nTickets; ++i) {
		yyjson_val *jticket = yyjson_arr_get(result, i);
		if (!jticket || !yyjson_is_obj(jticket)) continue;

		yyjson_val *v = yyjson_obj_get(jticket, "id");
		const gint id = v ? (gint) yyjson_get_int(v) : 0;

		v = yyjson_obj_get(jticket, "title");
		const char *title = v ? yyjson_get_str(v) : "";

		v = yyjson_obj_get(jticket, "description");
		const char *description = v ? yyjson_get_str(v) : "";

		v = yyjson_obj_get(jticket, "creationDate");
		const guint64 creationDate = v ? yyjson_get_uint(v) : 0;

		v = yyjson_obj_get(jticket, "priority");
		const int priority = v ? (int) yyjson_get_int(v) : 0;

		v = yyjson_obj_get(jticket, "state");
		const int state = v ? (int) yyjson_get_int(v) : 0;

		v = yyjson_obj_get(jticket, "creator");
		const char *creator = v ? yyjson_get_str(v) : "";

		v = yyjson_obj_get(jticket, "assignedAgent");
		const char *assignedAgent = v ? yyjson_get_str(v) : "";

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, COL_ID, id, COL_TITLE, title, COL_DESCRIPTION, description, COL_CREATIONDATE, (gint) creationDate, COL_PRIORITY, priority, COL_STATE, state, COL_CREATOR, creator, COL_ASSIGNEDAGENT, assignedAgent, -1);
	}

	yyjson_doc_free(responseDoc);
}

/**
 * @brief Handle the search button click event by displaying the search ticket dialog, setting up related UI elements, and populating the agent list with unique assigned agent emails from the ticket list
 * @param widget Pointer to the GtkWidget that triggered the callback (the search button) - not used
 * @param userData Pointer to user data passed during signal connection
 */
void onSearchButtonClicked(GtkWidget *widget, gpointer userData) {
	GtkBuilder *builder = GTK_BUILDER(userData);
	GtkWidget *searchTicketDialog = GTK_WIDGET(gtk_builder_get_object(builder, "search_ticket_dialog"));
	gtk_widget_show_all(searchTicketDialog);
	GtkWidget *searchTicketButton = GTK_WIDGET(gtk_builder_get_object(builder, "search_ticket_button"));
	GtkWidget *discardTicketButton = GTK_WIDGET(gtk_builder_get_object(builder, "discard_search_button"));
	GtkWidget *noTicketsFoundLabel = GTK_WIDGET(gtk_builder_get_object(builder, "no_tickets_found_label"));
	g_object_set_data(G_OBJECT(ticketTreeView), "no_tickets_found_label", noTicketsFoundLabel);
	gtk_widget_hide(noTicketsFoundLabel);

	g_signal_handlers_disconnect_by_func(searchTicketButton, G_CALLBACK(updateTicketFilter), NULL);
	g_signal_connect(searchTicketButton, "clicked", G_CALLBACK(updateTicketFilter), NULL);
	g_signal_handlers_disconnect_by_func(discardTicketButton, G_CALLBACK(onDiscardSearchButtonClicked), builder);
	g_signal_connect(discardTicketButton, "clicked", G_CALLBACK(onDiscardSearchButtonClicked), builder);

	GtkListStore *listStore = GTK_LIST_STORE(gtk_builder_get_object(builder, "liststore_items"));
	GtkTreeIter iter;

	GtkListStore *agentListStore = GTK_LIST_STORE(gtk_builder_get_object(builder, "agents_liststore"));
	gtk_list_store_clear(agentListStore);

	int valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(listStore), &iter);
	GList *existingAgents = NULL;

	while (valid) {
		char *agentEmail = NULL;
		gtk_tree_model_get(GTK_TREE_MODEL(listStore), &iter, COL_ASSIGNEDAGENT, &agentEmail, -1);

		if (agentEmail && strlen(agentEmail) > 0 && !g_list_find_custom(existingAgents, agentEmail, (GCompareFunc) g_strcmp0)) {
			existingAgents = g_list_append(existingAgents, g_strdup(agentEmail));
			GtkTreeIter agentIter;
			gtk_list_store_append(agentListStore, &agentIter);
			gtk_list_store_set(agentListStore, &agentIter, 0, agentEmail, -1);
		}
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(listStore), &iter);
	}
}

/**
 * @brief Callback for updating the ticket filter and showing/hiding the "no results" label
 * @param widget Pointer to the GtkWidget that triggered the callback (the search ticket button) - not used
 * @param userData Pointer passed during signal connection - not used
 */
void updateTicketFilter(GtkWidget *widget, gpointer userData) {
	GtkWidget *noResultLabel = g_object_get_data(G_OBJECT(ticketTreeView), "no_tickets_found_label");
	gtk_tree_model_filter_refilter(filterModel);
	GtkTreeModel *sortModel = GTK_TREE_MODEL(g_object_get_data(G_OBJECT(ticketTreeView), "sort_model"));
	const gint rowsCount = gtk_tree_model_iter_n_children(sortModel, NULL);

	if (rowsCount == 0) {
		gtk_widget_show(noResultLabel);
	} else {
		gtk_widget_hide(noResultLabel);
	}
}

/**
 * @brief Filter function for the ticket list rows based on active search criteria
 * @param model Pointer to the GtkTreeModel containing the ticket data
 * @param iter Pointer to a GtkTreeIter for the current row being tested
 * @param userData Pointer to user data passed during filter setup
 * @returns 1 if the row matches the filters, 0 otherwise
 */
int ticketFilterFunction(GtkTreeModel *model, GtkTreeIter *iter, gpointer userData) {
	GtkBuilder *builder = GTK_BUILDER(userData);
	GtkWidget *titleEntry = GTK_WIDGET(gtk_builder_get_object(builder, "title_filter_entry"));
	GtkWidget *descriptionEntry = GTK_WIDGET(gtk_builder_get_object(builder, "description_filter_entry"));
	GtkWidget *priorityCombo = GTK_WIDGET(gtk_builder_get_object(builder, "priority_filter_combobox"));
	GtkWidget *stateCombo = GTK_WIDGET(gtk_builder_get_object(builder, "state_filter_combobox"));
	GtkWidget *agentCombo = GTK_WIDGET(gtk_builder_get_object(builder, "agent_filter_combobox"));
	GtkWidget *calendar = GTK_WIDGET(gtk_builder_get_object(builder, "date_filter_calendar"));

	GtkWidget *enableTitle = GTK_WIDGET(gtk_builder_get_object(builder, "enable_title_filter"));
	GtkWidget *enableDescription = GTK_WIDGET(gtk_builder_get_object(builder, "enable_description_filter"));
	GtkWidget *enablePriority = GTK_WIDGET(gtk_builder_get_object(builder, "enable_priority_filter"));
	GtkWidget *enableState = GTK_WIDGET(gtk_builder_get_object(builder, "enable_state_filter"));
	GtkWidget *enableAgent = GTK_WIDGET(gtk_builder_get_object(builder, "enable_agent_filter"));
	GtkWidget *enableDate = GTK_WIDGET(gtk_builder_get_object(builder, "enable_date_filter"));

	const char *titleFilter = gtk_entry_get_text(GTK_ENTRY(titleEntry));
	const char *descriptionFilter = gtk_entry_get_text(GTK_ENTRY(descriptionEntry));
	GtkTreeIter comboIter;
	GtkTreeModel *comboModel = gtk_combo_box_get_model(GTK_COMBO_BOX(agentCombo));
	char *selectedAgent = NULL;

	if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(agentCombo), &comboIter)) {
		gtk_tree_model_get(comboModel, &comboIter, 0, &selectedAgent, -1);
	}

	guint year, month, day;
	gtk_calendar_get_date(GTK_CALENDAR(calendar), &year, &month, &day);
	struct tm date = {0};
	date.tm_year = year - 1900;
	date.tm_mon = month;
	date.tm_mday = day;

	char *title, *desc, *agent;
	int priority, state;
	time_t creationDate;

	gtk_tree_model_get(model, iter, COL_TITLE, &title, COL_DESCRIPTION, &desc, COL_PRIORITY, &priority, COL_STATE, &state, COL_ASSIGNEDAGENT, &agent, COL_CREATIONDATE, &creationDate, -1);

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enableTitle)) && titleFilter[0]) {
		char *titleLower = g_utf8_strdown(title, -1);
		char *filterLower = g_utf8_strdown(titleFilter, -1);

		gboolean match = g_strstr_len(titleLower, -1, filterLower) != NULL;

		free(titleLower);
		free(filterLower);

		if (!match) {
			return 0;
		}
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enableDescription)) && descriptionFilter[0]) {
		char *descLower = g_utf8_strdown(desc, -1);
		char *filterLower = g_utf8_strdown(descriptionFilter, -1);

		const gboolean match = g_strstr_len(descLower, -1, filterLower) != NULL;

		free(descLower);
		free(filterLower);

		if (!match) {
			return 0;
		}
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enablePriority))) {
		if (priority != gtk_combo_box_get_active_id(GTK_COMBO_BOX(priorityCombo))[0] - '0') {
			return 0;
		}
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enableState))) {
		if (state != gtk_combo_box_get_active_id(GTK_COMBO_BOX(stateCombo))[0] - '0') {
			return 0;
		}
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enableAgent)) && selectedAgent != NULL) {
		if (g_strcmp0(selectedAgent, agent) != 0) {
			g_free(selectedAgent);
			return 0;
		}
		g_free(selectedAgent);
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enableDate))) {
		struct tm *ticketDate = localtime(&creationDate);
		if (ticketDate->tm_year != date.tm_year || ticketDate->tm_mon != date.tm_mon || ticketDate->tm_mday != date.tm_mday) {
			return 0;
		}
	}

	return 1;
}

/**
 * @brief Callback for handling changes in the agent combobox
 * @param combobox Pointer to the GtkComboBox that changed
 * @param userData Pointer to user data (GtkBuilder)
 */
void onAgentComboboxChanged(GtkComboBox *combobox, gpointer userData) {
	GtkBuilder *builder = GTK_BUILDER(userData);
	GtkWidget *editTicketDialog = GTK_WIDGET(gtk_builder_get_object(builder, "edit_ticket_dialog"));
	GtkWidget *stateCombobox = GTK_WIDGET(gtk_builder_get_object(builder, "state_combobox"));
	
	gboolean hadAgentAssigned = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(editTicketDialog), "original-had-agent"));
	
	const gchar *selectedAgent = gtk_combo_box_get_active_id(combobox);
	
	if (!hadAgentAssigned && selectedAgent && strlen(selectedAgent) > 0) {
		gchar *assignedStateId = g_strdup_printf("%d", ASSIGNED); 
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(stateCombobox), assignedStateId);
		g_free(assignedStateId);
	}
}

/**
 * @brief Callback for handling selection of a ticket row in the ticket list
 * @param treeview Pointer to the GtkTreeView containing the ticket rows
 * @param path Pointer to the GtkTreePath of the selected row
 * @param column Pointer to the GtkTreeViewColumn that was clicked - not used
 * @param userData Pointer to user data passed during signal connection
 */
void onTicketRowSelected(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column, gpointer userData) {
	GtkBuilder *builder = GTK_BUILDER(userData);
	GtkTreeModel *model = gtk_tree_view_get_model(treeview);
	GtkTreeIter iter;

	if (!gtk_tree_model_get_iter(model, &iter, path)) {
		return;
	}

	gint id, priority, state;
	gchar *title = NULL, *description = NULL, *agent = NULL, *creator = NULL;
	time_t creationDate;

	gtk_tree_model_get(model, &iter, COL_ID, &id, COL_TITLE, &title, COL_DESCRIPTION, &description, COL_PRIORITY, &priority, COL_STATE, &state, COL_CREATIONDATE, &creationDate, COL_CREATOR, &creator, COL_ASSIGNEDAGENT, &agent, -1);

	GtkWidget *editTicketDialog = GTK_WIDGET(gtk_builder_get_object(builder, "edit_ticket_dialog"));
	gtk_window_set_title(GTK_WINDOW(editTicketDialog), "Modifica ticket");

	GtkLabel *ticketEditErrorLabel = GTK_LABEL(gtk_builder_get_object(builder, "ticket_edit_error_label"));
	GtkWidget *priorityComboBox = GTK_WIDGET(gtk_builder_get_object(builder, "priority_combobox"));
	GtkWidget *stateCombobox = GTK_WIDGET(gtk_builder_get_object(builder, "state_combobox"));
	GtkWidget *agentCombobox = GTK_WIDGET(gtk_builder_get_object(builder, "agent_combobox"));
	GtkWidget *titleEntry = GTK_WIDGET(gtk_builder_get_object(builder, "ticket_title_entry"));
	GtkWidget *descriptionTextView = GTK_WIDGET(gtk_builder_get_object(builder, "ticket_description_text"));

	g_object_set_data(G_OBJECT(editTicketDialog), "ticket-id", GINT_TO_POINTER(id));
	g_object_set_data_full(G_OBJECT(editTicketDialog), "ticket-creator", g_strdup(currentUser.email), g_free);

	gboolean hadAgentAssigned = (agent && strlen(agent) > 0);
	g_object_set_data(G_OBJECT(editTicketDialog), "original-had-agent", GINT_TO_POINTER(hadAgentAssigned));

	gtk_entry_set_text(GTK_ENTRY(titleEntry), title);
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(descriptionTextView));
	gtk_text_buffer_set_text(buffer, description, -1);

	GtkListStore *agentListStore = GTK_LIST_STORE(gtk_builder_get_object(builder, "agents_liststore"));
	gtk_list_store_clear(agentListStore);

	if (currentUser.role == SUPER_AGENT) {
		gtk_widget_set_sensitive(agentCombobox, 1);

		yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
		yyjson_mut_val *root = yyjson_mut_obj(doc);
		yyjson_mut_doc_set_root(doc, root);

		yyjson_mut_obj_add_str(doc, root, "jsonrpc", "2.0");
		yyjson_mut_obj_add_str(doc, root, "method", "getUsers");
		yyjson_mut_obj_add_int(doc, root, "id", 4);

		yyjson_mut_val *params = yyjson_mut_obj(doc);
		yyjson_mut_obj_add_int(doc, params, "role", AGENT);
		yyjson_mut_obj_add_val(doc, root, "params", params);

		if (sendJsonRpcMessage(clientFD, doc) < 0) {
			showErrorDialog(mainWindow, "Errore di rete durante il recupero degli agent");
			yyjson_mut_doc_free(doc);
			return;
		}

		yyjson_doc *responseDoc = NULL;
		if (receiveJsonRpcMessage(clientFD, &responseDoc) < 0) {
			showErrorDialog(mainWindow, "Errore di rete durante il recupero degli agent");
			yyjson_mut_doc_free(doc);
			return;
		}

		yyjson_val *responseRoot = yyjson_doc_get_root(responseDoc);
		yyjson_val *result = yyjson_obj_get(responseRoot, "result");

		const uint32_t nAgents = yyjson_arr_size(result);

		for (size_t i = 0; i < nAgents; ++i) {
			yyjson_val *juser = yyjson_arr_get(result, i);
			if (!juser || !yyjson_is_obj(juser)) {
				continue;
			}
			yyjson_val *v = yyjson_obj_get(juser, "email");
			const char *email = v && yyjson_is_str(v) ? yyjson_get_str(v) : "";
			gtk_list_store_append(agentListStore, &iter);
			gtk_list_store_set(agentListStore, &iter, 0, email, -1);
		}

		if (agent && strlen(agent) > 0) {
			gtk_combo_box_set_active_id(GTK_COMBO_BOX(agentCombobox), agent);
		} else {
			gtk_combo_box_set_active(GTK_COMBO_BOX(agentCombobox), -1);
		}

		g_signal_handlers_disconnect_by_func(agentCombobox, G_CALLBACK(onAgentComboboxChanged), builder);
		g_signal_connect(agentCombobox, "changed", G_CALLBACK(onAgentComboboxChanged), builder);

	} else {
		gtk_widget_set_sensitive(agentCombobox, 0);

		GtkTreeIter agentIter;
		gtk_list_store_append(agentListStore, &agentIter);
		gtk_list_store_set(agentListStore, &agentIter, 0, agent, -1);
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(agentCombobox), agent);
	}

	gchar *priorityId = g_strdup_printf("%d", priority);
	gchar *stateId = g_strdup_printf("%d", state);

	gtk_combo_box_set_active_id(GTK_COMBO_BOX(priorityComboBox), priorityId);
	gtk_combo_box_set_active_id(GTK_COMBO_BOX(stateCombobox), stateId);

	g_free(priorityId);
	g_free(stateId);

	gtk_widget_set_sensitive(titleEntry, 0);
	gtk_widget_set_sensitive(descriptionTextView, 0);
	gtk_widget_set_sensitive(priorityComboBox, 1);
	gtk_widget_set_sensitive(stateCombobox, 1);

	gtk_widget_show_all(editTicketDialog);
	gtk_widget_hide(GTK_WIDGET(ticketEditErrorLabel));

	GtkWidget *saveButton = GTK_WIDGET(gtk_builder_get_object(builder, "save_ticket_button"));
	g_signal_handlers_disconnect_by_func(saveButton, G_CALLBACK(onSaveEditedTicketButtonClicked), builder);
	g_signal_connect(saveButton, "clicked", G_CALLBACK(onSaveEditedTicketButtonClicked), builder);

	GtkWidget *discardButton = GTK_WIDGET(gtk_builder_get_object(builder, "discard_ticket_button"));
	g_signal_handlers_disconnect_by_func(discardButton, G_CALLBACK(onDiscardAddTicketButtonClicked), builder);
	g_signal_connect(discardButton, "clicked", G_CALLBACK(onDiscardAddTicketButtonClicked), builder);

	g_free(title);
	g_free(description);
	g_free(agent);
}

/**
 * @brief Callback for the save button in the edit ticket dialog; send updated ticket data to the server
 * @param widget Pointer to the GtkWidget that was clicked (the save button) - not used
 * @param userData Pointer to user data passed during signal connection
 */
void onSaveEditedTicketButtonClicked(GtkWidget *widget, gpointer userData) {

	GtkBuilder *builder = GTK_BUILDER(userData);

	GtkWidget *dialog = GTK_WIDGET(gtk_builder_get_object(builder, "edit_ticket_dialog"));
	GtkWidget *priorityCombo = GTK_WIDGET(gtk_builder_get_object(builder, "priority_combobox"));
	GtkWidget *stateCombo = GTK_WIDGET(gtk_builder_get_object(builder, "state_combobox"));
	GtkWidget *agentCombo = GTK_WIDGET(gtk_builder_get_object(builder, "agent_combobox"));
	GtkLabel *errorLabel = GTK_LABEL(gtk_builder_get_object(builder, "ticket_edit_error_label"));

	const gint ticketId = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(dialog), "ticket-id"));
	const char *priorityId = gtk_combo_box_get_active_id(GTK_COMBO_BOX(priorityCombo));
	const char *stateId = gtk_combo_box_get_active_id(GTK_COMBO_BOX(stateCombo));
	const char *agentEmail = gtk_combo_box_get_active_id(GTK_COMBO_BOX(agentCombo));

	if (!agentEmail) {
		agentEmail = "";
	}

	yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
	yyjson_mut_val *root = yyjson_mut_obj(doc);
	yyjson_mut_doc_set_root(doc, root);

	yyjson_mut_obj_add_str(doc, root, "jsonrpc", "2.0");
	yyjson_mut_obj_add_str(doc, root, "method", "updateTicket");
	yyjson_mut_obj_add_int(doc, root, "id", 5);

	yyjson_mut_val *params = yyjson_mut_obj(doc);
	yyjson_mut_obj_add_int(doc, params, "id", ticketId);
	yyjson_mut_obj_add_int(doc, params, "priority", atoi(priorityId));
	yyjson_mut_obj_add_int(doc, params, "state", atoi(stateId));
	yyjson_mut_obj_add_str(doc, params, "assignedAgent", agentEmail);
	yyjson_mut_obj_add_val(doc, root, "params", params);

	if (sendJsonRpcMessage(clientFD, doc) < 0) {
		showErrorDialog(mainWindow, "Errore di rete durante l'aggiornamento del ticket");
		return;
	}

	yyjson_doc *responseDoc = NULL;
	if (receiveJsonRpcMessage(clientFD, &responseDoc) < 0) {
		showErrorDialog(mainWindow, "Errore di rete durante il recupero degli agent");
		yyjson_mut_doc_free(doc);
		return;
	}

	yyjson_val *responseRoot = yyjson_doc_get_root(responseDoc);
	yyjson_val *error = yyjson_obj_get(responseRoot, "error");

	if (error) {
		gtk_label_set_text(errorLabel, yyjson_get_str(yyjson_obj_get(error, "message")));
		yyjson_mut_doc_free(doc);
		gtk_widget_show(GTK_WIDGET(errorLabel));
		return;
	}

	gtk_widget_hide(dialog);
	refreshTicketList();
}

/**
 * @brief Display an error dialog with a custom message
 * @param parent Pointer to the parent GtkWindow for the dialog
 * @param message Error message text to display in the dialog
 */
void showErrorDialog(GtkWindow *parent, const char *message) {
	GtkWidget *dialog = gtk_message_dialog_new(parent, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", message);
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "Impossibile comunicare con il server: disconnettiti e riprova.");
	gtk_window_set_title(GTK_WINDOW(dialog), "Errore");
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}
