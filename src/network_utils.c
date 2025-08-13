#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include "../include/yyjson.h"

/**
 * Send a JSON-RPC message (from a YYJSON mutable doc) over TCP
 */
int sendJsonRpcMessage(int sock, yyjson_mut_doc *doc) {
    /* Serialize JSON */
    size_t json_len;
    char *json_str = yyjson_mut_write(doc, 0, &json_len);
    if (!json_str) {
        fprintf(stderr, "YYJSON serialization failed\n");
        return -1;
    }

    /* Send length prefix */
    uint32_t net_len = htonl((uint32_t)json_len);
    size_t to_write = sizeof(net_len);
    const uint8_t *len_ptr = (const uint8_t *)&net_len;
    while (to_write > 0) {
        ssize_t w = write(sock, len_ptr, to_write);
        if (w > 0) {
            len_ptr += w;
            to_write -= w;
        } else if (w < 0 && errno == EINTR) {
            continue;
        } else {
            perror("write length");
            free(json_str);
            return -1;
        }
    }

    /* Send JSON payload */
    to_write = json_len;
    const uint8_t *data_ptr = (const uint8_t *)json_str;
    while (to_write > 0) {
        ssize_t w = write(sock, data_ptr, to_write);
        if (w > 0) {
            data_ptr += w;
            to_write -= w;
        } else if (w < 0 && errno == EINTR) {
            continue;
        } else {
            perror("write JSON");
            free(json_str);
            return -1;
        }
    }

    free(json_str);
    return 0;
}

/**
 * Receive a JSON-RPC message and parse it into a YYJSON doc
 * Caller must free doc with yyjson_doc_free()
 */
int receiveJsonRpcMessage(int sock, yyjson_doc **out_doc) {
    uint32_t net_len;
    size_t to_read = sizeof(net_len);
    uint8_t *len_ptr = (uint8_t *)&net_len;

    /* Read length prefix */
    while (to_read > 0) {
        ssize_t r = read(sock, len_ptr, to_read);
        if (r > 0) {
            len_ptr += r;
            to_read -= r;
        } else if (r < 0 && errno == EINTR) {
            continue;
        } else {
            perror("read length");
            return -1;
        }
    }

    uint32_t json_len = ntohl(net_len);
    char *json_buf = malloc(json_len + 1);
    if (!json_buf) {
        fprintf(stderr, "malloc failed\n");
        return -1;
    }

    /* Read JSON payload */
    char *data_ptr = json_buf;
    to_read = json_len;
    while (to_read > 0) {
        ssize_t r = read(sock, data_ptr, to_read);
        if (r > 0) {
            data_ptr += r;
            to_read -= r;
        } else if (r < 0 && errno == EINTR) {
            continue;
        } else {
            perror("read JSON");
            free(json_buf);
            return -1;
        }
    }
    json_buf[json_len] = '\0';

    /* Parse JSON into YYJSON doc */
    yyjson_doc *doc = yyjson_read(json_buf, json_len, 0);
    free(json_buf);

    if (!doc) {
        fprintf(stderr, "YYJSON parse error\n");
        return -1;
    }

    *out_doc = doc;
    return 0;
}
