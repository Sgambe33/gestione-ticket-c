#ifndef GUI_H
#define GUI_H
#include <gtk/gtk.h>
#include "models.h"

extern User currentUser;
extern int clientFD;

char *stateToString(enum State state);

char *priorityToString(enum Priority priority);

gboolean onDeleteEvent(GtkWidget *widget, GdkEvent *event, gpointer userData);

gboolean onMainWindowDeleteEvent(GtkWidget *widget, GdkEvent *event, gpointer userData);

void onLoginButtonClicked(GtkButton *button, gpointer userData);

void renderPriority(GtkTreeViewColumn *col, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer userData);

void renderState(GtkTreeViewColumn *col, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer userData);

void renderDate(GtkTreeViewColumn *col, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer userData);

void onDisconnectButtonClicked(GtkWidget *widget, gpointer userData);

void onSaveTicketButtonClicked(GtkWidget *widget, gpointer userData);

void onDiscardAddTicketButtonClicked(GtkWidget *widget, gpointer userData);

void onDiscardSearchButtonClicked(GtkWidget *widget, gpointer userData);

void onAddTicketButtonClicked(GtkWidget *widget, gpointer userData);

void onSearchButtonClicked(GtkWidget *widget, gpointer userData);

void updateTicketFilter(GtkWidget *widget, gpointer userData);

int ticketFilterFunction(GtkTreeModel *model, GtkTreeIter *iter, gpointer userData);

void onAgentComboboxChanged(GtkComboBox *combobox, gpointer userData);

void onTicketRowSelected(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column, gpointer userData);

void onSaveEditedTicketButtonClicked(GtkWidget *widget, gpointer userData);

void refreshTicketList();

void showErrorDialog(GtkWindow *parent, const char *message);

void initMainWindow(GtkWidget *window, gpointer userData);
#endif // GUI_H
