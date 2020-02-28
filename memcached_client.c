#define PORT 11211
#define MAX_COMMAND_SIZE 2000

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "memcached_client.h"
#include "data_parser.h"

int sfd;

static char *get_storage_command(char *command_name, char *key, char *value, size_t *count)
{
    size_t key_size = strlen(key);
    size_t command_size = strlen(command_name);

    char *bytes = int_to_string(*(int *)count);

    size_t full_command_size = command_size + 1 + key_size + 5 + strlen(bytes) + 1 + *count + 2;
    char *command = (char *)malloc(full_command_size + 1);
    int index = 0;

    memcpy(command + index, command_name, command_size);
    index += command_size;
    memcpy(command + index, " ", 1);
    index += 1;
    memcpy(command + index, key, key_size);
    index += key_size;
    memcpy(command + index, " 0 0 ", 5);
    index += 5;
    memcpy(command + index, bytes, strlen(bytes));
    index += strlen(bytes);
    memcpy(command + index, "\n", 1);
    index += 1;
    memcpy(command + index, value, *count);
    index += *count;
    memcpy(command + index, "\r\n", 2);
    index += 2;

    command[index] = '\0';

    *count = index;

    free(bytes);
    return command;
}

static char *get_retrieve_command(char *command_name, char *key, size_t *count)
{
    size_t command_size = strlen(command_name);

    size_t key_size = strlen(key);

    char *tail = "\r\n";
    size_t tail_size = strlen(tail);

    char *command = (char *)malloc(command_size + 1 + key_size + tail_size + 1);
    int index = 0;
    memcpy(command + index, command_name, command_size);
    index += command_size;
    memcpy(command + index, " ", 1);
    index += 1;
    memcpy(command + index, key, key_size);
    index += key_size;
    memcpy(command + index, tail, tail_size);
    index += tail_size;
    command[index] = '\0';

    *count = index;

    return command;
}

static char *send_to_server(char *command, int write_count)
{
    size_t bytes_writen = write(sfd, command, write_count);

    if (bytes_writen == -1)
    {
        perror(NULL);
    }

    char *response = (char *)malloc(MAX_COMMAND_SIZE);
    size_t response_size = read(sfd, response, MAX_COMMAND_SIZE);
    response[response_size] = '\0';

    return response;
}

static char *send_command(char *command_name, char *key, char *value, size_t count)
{
    char *command = NULL;
    if (strcmp(value, "NOVALUE") == 0)
    {
        command = get_retrieve_command(command_name, key, &count);
    }
    else
    {
        command = get_storage_command(command_name, key, value, &count);
    }

    char *response = send_to_server(command, count);
    free(command);

    return response;
}

void memcached_connect()
{
    struct sockaddr_in addr;
    sfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sfd == -1)
    {
        perror(NULL);
        exit(errno);
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);

    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    int connection_status = connect(sfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));

    if (connection_status == -1)
    {
        perror(NULL);
        exit(errno);
    }

    if (connection_status == 0)
    {
        printf("connection established\n");
    }
}

/* Set new value to new or existing key. */

int memcached_set(char *key, char *value, size_t count)
{
    char *response = send_command("set", key, value, count);

    if (strcmp(response, "STORED\r\n") == 0)
    {
        printf("Set: stored\n");
        free(response);
        return 1;
    }

    printf("Set: error\n");
    free(response);

    return 0;
}

/* Set a value to a new key. If the key already exists, then it gives the output NOT_STORED. */

int memcached_add(char *key, char *value, size_t count)
{
    char *response = send_command("add", key, value, count);

    if (strcmp(response, "STORED\r\n") == 0)
    {
        printf("Add: stored\n");
        free(response);

        return 1;
    }

    printf("Add: not stored\n");
    free(response);

    return 0;
}

/* returened data needs to be freed when not needed anymore */

char *memcached_get(char *key)
{
    char *response = send_command("get", key, "NOVALUE", 0);

    if (strcmp(response, "END\r\n") == 0)
    {
        printf("Get: end\n");
        free(response);
        return NULL;
    }

    size_t offset = strlen("VALUE") + strlen(key) + 1 + 3; // 3 space

    char data_size[MAX_COMMAND_SIZE];
    int index = 0;

    char cur = response[offset];
    while (cur != '\n')
    {
        data_size[index] = cur;
        index += 1;
        cur = response[offset + index];
    }

    data_size[index] = '\0';
    char *response_data = response + offset + index + 1; // \n

    char *data = (char *)malloc(atoi(data_size) + 1);
    memcpy(data, response_data, atoi(data_size));
    data[atoi(data_size)] = '\0';

    free(response);

    return data;
}

int memcached_delete(char *key)
{
    char *response = send_command("delete", key, "NOVALUE", 0);

    if (strcmp(response, "DELETED\r\n") == 0)
    {
        printf("Delete: deleted\n");
        free(response);
        return 0;
    }

    if (strcmp(response, "ERROR\r\n") == 0)
    {
        printf("Delete: error\n");
        free(response);
        return -1;
    }

    printf("Delete: not found\n");
    free(response);
    return 0;
}

int memcached_flush_all()
{
    char *command = "flush_all\r\n";

    char *response = send_to_server(command, strlen(command));

    if (strcmp(response, "OK\r\n") == 0)
    {
        printf("Flush all: ok\n");
        free(response);
        return 0;
    }

    printf("Flush all: failed\n");
    free(response);

    return -1;
}