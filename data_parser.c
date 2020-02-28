#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "data_parser.h"

/* malloc-ed string with null-terminator */
char *ulong_to_string(unsigned long x)
{
    int n = snprintf(NULL, 0, "%lu", x);

    char *buf = (char *)malloc(n + 1);
    snprintf(buf, n + 1, "%lu", x);

    return buf;
}

char *int_to_string(int x)
{
    int n = snprintf(NULL, 0, "%d", x);

    char *buf = (char *)malloc(n + 1);
    snprintf(buf, n + 1, "%d", x);

    return buf;
}

char *get_attr_pair_str(char *attr_name, char *value_string)
{
    size_t name_size = strlen(attr_name);
    size_t value_size = strlen(value_string);

    char *pair = (char *)malloc(name_size + value_size + 3);

    int index = 0;

    memcpy(pair + index, attr_name, name_size);
    index += name_size;
    pair[index] = '\n';
    index += 1;

    memcpy(pair + index, value_string, value_size);
    index += value_size;
    pair[index] = '\n';

    pair[index + 1] = '\0';

    return pair;
}

/* malloc-ed string. example: st_ino\n3\n\0 */
char *get_attr_pair(char *attr_name, unsigned long value)
{
    char *value_string = ulong_to_string(value);
    char *pair = get_attr_pair_str(attr_name, value_string);
    free(value_string);

    return pair;
}

char *block_key_to_string(int inode_value, int block_num)
{
    char *inode = int_to_string(inode_value);
    size_t inode_size = strlen(inode);

    char *constant = "_b_";
    size_t constant_size = strlen(constant);

    char *block = int_to_string(block_num);
    size_t block_size = strlen(block);

    char *key = malloc(inode_size + constant_size + block_size + 1);

    int index = 0;
    memcpy(key + index, inode, inode_size);
    index += inode_size;

    memcpy(key + index, constant, constant_size);
    index += constant_size;

    memcpy(key + index, block, block_size);
    index += block_size;

    key[index] = '\0';

    free(inode);
    free(block);

    return key;
}

unsigned long get_attr_value(char *attribute_data, char *attr_name)
{
    char *value = get_attr_value_str(attribute_data, attr_name);
    unsigned long value_long = strtoul(value, NULL, 10);
    free(value);

    return value_long;
}

char *get_attr_value_str(char *attribute_data, char *attr_name)
{
    char *attr = strstr(attribute_data, attr_name);

    if (attr == NULL)
    {
        return NULL;
    }

    int index = (attr - attribute_data) / sizeof(char);
    index += (strlen(attr_name) + 1);

    int size = 0;
    char c = '\0';
    while (c != '\n')
    {
        c = attribute_data[index];
        index += 1;
        size += 1;
    }

    char *value = (char *)malloc(size);
    memcpy(value, attribute_data + index - size, size - 1);
    value[size - 1] = '\0';

    return value;
}

char *modify_attr(char *attribute_data, char *attr_name, unsigned long new_value)
{
    char *attr_value = ulong_to_string(new_value);

    char *result_data = modify_attr_str(attribute_data, attr_name, attr_value);

    free(attr_value);

    if (result_data == NULL)
    {
        return NULL;
    }

    return result_data;
}

char *modify_attr_str(char *attribute_data, char *attr_name, char *attr_value)
{
    char *attr = strstr(attribute_data, attr_name);

    if (attr == NULL) // not supposed to
    {
        return NULL;
    }

    int index = (attr - attribute_data) / sizeof(char);

    int rest_index = index + strlen(attr_name) + 1;

    char c = '\0';
    while (c != '\n')
    {
        c = attribute_data[rest_index];
        rest_index += 1;
    }

    char *new_pair = get_attr_pair_str(attr_name, attr_value);

    char *data = (char *)malloc(index + strlen(new_pair) + strlen(attribute_data) - rest_index + 1);

    int data_index = 0;
    memcpy(data, attribute_data, index);
    data_index += index;
    memcpy(data + data_index, new_pair, strlen(new_pair));
    data_index += strlen(new_pair);
    memcpy(data + data_index, attribute_data + rest_index, strlen(attribute_data) - rest_index);
    data_index += (strlen(attribute_data) - rest_index);

    data[data_index] = '\0';

    free(new_pair);

    return data;
}

char *add_attr(char *attribute_data, char *attr_name, char *value)
{
    char *new_pair = get_attr_pair_str(attr_name, value);

    size_t attribute_data_size = strlen(attribute_data);
    size_t new_pair_size = strlen(new_pair);

    char *data = (char *)malloc(attribute_data_size + new_pair_size + 1);

    int index = 0;
    memcpy(data, attribute_data, attribute_data_size);
    index += attribute_data_size;
    memcpy(data + index, new_pair, new_pair_size);
    index += new_pair_size;
    data[index] = '\0';

    free(new_pair);

    return data;
}

char *add_inode_to_table(char *inode_table, char *path, int inode_value)
{
    char *inode = int_to_string(inode_value);

    size_t table_size = strlen(inode_table);
    size_t path_size = strlen(path);
    size_t inode_size = strlen(inode);

    char *new_table = (char *)malloc(table_size + path_size + inode_size + 3);

    int index = 0;
    memcpy(new_table, inode_table, table_size);
    index += table_size;
    memcpy(new_table + index, path, path_size);
    index += path_size;
    memcpy(new_table + index, "\n", 1);
    index += 1;
    memcpy(new_table + index, inode, inode_size);
    index += inode_size;
    memcpy(new_table + index, "\n", 1);
    index += 1;

    new_table[index] = '\0';

    free(inode);

    return new_table;
}

char *remove_inode_from_table(char *inode_table, char *path)
{
    char *inode = strstr(inode_table, path);

    if (inode == NULL)
    {
        printf("inode is null \n");
        return NULL;
    }

    int index = (inode - inode_table) / sizeof(char);

    int rest_index = index + strlen(path) + 1;

    char c;
    while (c != '\n')
    {
        c = inode_table[rest_index];
        rest_index += 1;
    }

    char *data = (char *)malloc(index + strlen(inode_table) - rest_index + 1);

    int data_index = 0;
    memcpy(data, inode_table, index);
    data_index += index;
    memcpy(data + data_index, inode_table + rest_index, strlen(inode_table) - rest_index);
    data_index += (strlen(inode_table) - rest_index);

    data[data_index] = '\0';

    return data;
}

char *get_parent_directory(const char *path)
{
    char *last_slash = strrchr(path, '/');

    if (path == last_slash)
    {
        return strdup("/"); // root is parent
    }

    int size = (last_slash - path) / sizeof(char);
    char *parent = (char *)malloc(size + 1);
    parent[size] = '\0';
    memcpy(parent, path, size);

    return parent;
}

char *get_name_from_path(const char *path)
{
    if (strcmp(path, "/") == 0)
    {
        return strdup("/");
    }

    char *last_slash = strrchr(path, '/');

    size_t name_size = strlen(last_slash);
    char *name = (char *)malloc(name_size);
    name[name_size - 1] = '\0';
    memcpy(name, last_slash + 1, name_size - 1);

    return name;
}

struct list *get_extended_attrs_list(char *attribute_data) // assumes st_blocks is last attribute
{
    char *attr = strstr(attribute_data, "st_blocks") + strlen("st_blocks") + 1;

    char c = '0';
    int index = 0;

    while (c != '\n')
    {
        c = attr[index];
        index += 1;
    }

    attr = attr + index;

    char *token = strtok(attr, "\n");
    int is_key = 1;

    list *l = (list *)malloc(sizeof(list));
    l->size = 0;
    l->keys = NULL;

    while (token != NULL)
    {
        if (is_key)
        {
            size_t token_size = strlen(token);
            size_t size = l->size;
            l->size += (token_size + 1);
            l->keys = realloc(l->keys, l->size);

            memcpy(l->keys + size, token, token_size);
            l->keys[size + token_size] = '\0';
            printf("token %s\n", token);

            is_key = 0;
        }
        else
        {
            is_key = 1;
        }
        token = strtok(NULL, "\n");
    }

    return l;
}

char *remove_extended_attr(char *attribute_data, char *attr_name)
{
    char *attr = strstr(attribute_data, attr_name);

    if (attr == NULL)
    {
        return NULL;
    }
    else
    {
        int index = (attr - attribute_data) / sizeof(char);

        int size = 0;

        char *key_end = strstr(attr, "\n");

        size_t key_size = (key_end - attr) / sizeof(char);

        size += (key_size + 1);

        char *value_end = strstr(key_end + 1, "\n");

        size_t value_size = (value_end - key_end);

        size += value_size;

        char *modified_data = (char *)malloc(strlen(attribute_data) - size + 1);

        memcpy(modified_data, attribute_data, index);
        memcpy(modified_data + index, attribute_data + index + size, strlen(attribute_data) - index - size);

        modified_data[strlen(attribute_data) - size] = '\0';

        return modified_data;
    }
}

char *construct_path(char *parent_dir, char *linkname)
{
    if (strcmp(parent_dir, "/") == 0)
    {
        size_t link_size = strlen(linkname);
        char *link_path = (char *)malloc(link_size + 2);
        link_path[0] = '/';

        memcpy(link_path + 1, linkname, link_size);
        link_path[link_size + 1] = '\0';

        return link_path;
    }

    size_t link_size = strlen(linkname);
    size_t path_size = strlen(parent_dir);
    char *link_path = (char *)malloc(path_size + link_size + 2);

    memcpy(link_path, parent_dir, path_size);
    memcpy(link_path + path_size, "/", 1);
    memcpy(link_path + path_size + 1, linkname, link_size);

    link_path[path_size + link_size + 1] = '\0';

    return link_path;
}