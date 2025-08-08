#ifndef GUI_H
#define GUI_H
#include <gtk/gtk.h>
#include "models.h"

extern User currentUser;
extern int clientFD;
char *stateToString(enum State state);
char *priorityToString(enum Priority priority);
gboolean onDeleteEvent(GtkWidget *widget, GdkEvent *event, gpointer data);
gboolean onMainWindowDeleteEvent(GtkWidget *widget, GdkEvent *event, gpointer user_data);
void onLoginButtonClicked(GtkButton *button, gpointer user_data);
void renderPriority(GtkTreeViewColumn *col, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer data);
void renderState(GtkTreeViewColumn *col, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer data);
void renderDate(GtkTreeViewColumn *col, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer data);
void onDisconnectButtonClicked(GtkWidget *widget, gpointer user_data);
void onSaveTicketButtonClicked(GtkWidget *widget, gpointer user_data);
void onDiscardAddTicketButtonClicked(GtkWidget *widget, gpointer user_data);
void onDiscardSearchButtonClicked(GtkWidget *widget, gpointer user_data);
void onAddTicketButtonClicked(GtkWidget *widget, gpointer user_data);
void onSearchButtonClicked(GtkWidget *widget, gpointer user_data);
void updateTicketFilter(GtkWidget *widget, gpointer user_data);
int ticketFilterFunction(GtkTreeModel *model, GtkTreeIter *iter, gpointer data);
void onTicketRowSelected(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data);
void onSaveEditedTicketButtonClicked(GtkWidget *widget, gpointer user_data);
void refreshTicketList();
void showErrorDialog(GtkWindow *parent, const char *message);
void initMainWindow(GtkWidget *window, gpointer user_data);
#endif // GUI_H
