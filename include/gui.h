#ifndef GUI_H
#define GUI_H
#include <gtk/gtk.h>
#include "models.h"

extern User currentUser;
extern int clientFD;  // <-- only a declaration, not a definition
char *stateToString(enum State state);
char *priorityToString(enum Priority priority);
void onLoginButtonClicked(GtkButton *button, gpointer user_data);
void renderPriority(GtkTreeViewColumn *col, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer data);
void renderState(GtkTreeViewColumn *col, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer data);
void renderDate(GtkTreeViewColumn *col, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer data);
void onDisconnectButtonClicked(GtkWidget *widget, gpointer user_data);
void onSaveTicketButtonClicked(GtkWidget *widget, gpointer user_data);
void onDiscardButtonClicked(GtkWidget *widget, gpointer user_data);
void onAddTicketButtonClicked(GtkWidget *widget, gpointer user_data);
void onMainWindowShow(GtkWidget *widget, gpointer user_data);
void onSearchButtonClicked(GtkWidget *widget, gpointer user_data);
void updateTicketFilter(GtkWidget *widget, gpointer user_data);
gboolean ticketFilterFunction(GtkTreeModel *model, GtkTreeIter *iter, gpointer data);
#endif // GUI_H
