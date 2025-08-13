#include <gtk/gtk.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "../include/gui.h"
#include "../include/models.h"
#include "../include/network_utils.h"
#include "../include/yyjson.h"

#define DEFAULT_PROTOCOL 0

int clientFD;

/**
 * @brief Callback for the login window delete event; close the client socket
 * and terminate the GTK main loop
 * @param widget Pointer to the GtkWidget receiving the the event (the login
 * window) - not used
 * @param event Pointer to the GdkEvent triggering the delete event - not used
 * @param user_data Pointer to user data passed during signal connection - not
 * used
 * @returns FALSE
 */
gboolean onLoginWindowDeleteEvent(GtkWidget *widget, GdkEvent *event,
                                  gpointer user_data) {
	if (clientFD >= 0) {
		close(clientFD);
		clientFD = -1;
	}

	gtk_main_quit();
	return FALSE;
}

int main(int argc, char *argv[]) {
	int result;
	const unsigned short int port = 1420;
	struct sockaddr_in serverINETAddress;
	const struct sockaddr *serverSockAddrPtr =
			(struct sockaddr *) &serverINETAddress;
	const int serverLen = sizeof(serverINETAddress);

	serverINETAddress.sin_family = AF_INET;
	serverINETAddress.sin_port = htons(port);
	serverINETAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	clientFD = socket(AF_INET, SOCK_STREAM, DEFAULT_PROTOCOL);

	do {
		result = connect(clientFD, serverSockAddrPtr, serverLen);
		printf("Non connesso...\n");
		if (result == -1) sleep(1);
	} while (result == -1);
	printf("Connesso\n");

	yyjson_mut_doc *req = yyjson_mut_doc_new(NULL);
	yyjson_mut_val *root = yyjson_mut_obj(req);
	yyjson_mut_doc_set_root(req, root); // <-- important

	yyjson_mut_obj_add_str(req, root, "jsonrpc", "2.0");
	yyjson_mut_obj_add_str(req, root, "method", "sum");

	yyjson_mut_val *params = yyjson_mut_arr(req);
	yyjson_mut_arr_add_int(req, params, 42);
	yyjson_mut_arr_add_int(req, params, 23);
	yyjson_mut_obj_add_val(req, root, "params", params);

	yyjson_mut_obj_add_int(req, root, "id", 1);

	sendJsonRpcMessage(clientFD, req);
	yyjson_mut_doc_free(req);

	yyjson_doc *resp;
	if (receiveJsonRpcMessage(clientFD, &resp) == 0) {
		yyjson_val *rroot = yyjson_doc_get_root(resp);
		yyjson_val *result_val = yyjson_obj_get(rroot, "result");
		if (result_val) {
			printf("Result: %lld\n", yyjson_get_sint(result_val));
		} else {
			// It means there has been an error
			yyjson_val *error_obj = yyjson_obj_get(rroot, "error");
			yyjson_val *code_val = yyjson_obj_get(error_obj, "code");
			yyjson_val *message_val = yyjson_obj_get(error_obj, "message");
			printf("Error code: %ld \n", yyjson_get_sint(code_val));
			printf("Error message: %s \n", yyjson_get_str(message_val));
		}
		yyjson_doc_free(resp);
	} else {
		fprintf(stderr, "Failed to receive JSON-RPC response\n");
	}

	// gtk_main();
	close(clientFD);

	exit(0);
}
