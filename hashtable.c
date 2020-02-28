#include <stdlib.h>
#include <stdio.h>

#include "hashtable.h"

void hashtable_init()
{
    inode_table = NULL;
    attributes = NULL;
}

void hashtable_add_entry(char *link, int inode_value)
{
    hashable *new_item = (hashable *)malloc(sizeof(hashable));

    strcpy(new_item->key, link);
    new_item->inode_value = inode_value;

    HASH_ADD_STR(inode_table, key, new_item);
}

int hashable_get_entry(char *link)
{
    hashable *h = (hashable *)malloc(sizeof(hashable));
    HASH_FIND_STR(inode_table, link, h);

    int value = -1;

    if (h)
    {
        value = h->inode_value;
    }

    return value;
}

int hashtable_remove_entry(char *link)
{
    hashable *h = (hashable *)malloc(sizeof(hashable));

    HASH_FIND_STR(inode_table, link, h);
    int value = h->inode_value;

    HASH_DEL(inode_table, h);
    free(h);

    return value;
}

void hashtable_string_to_table(char *links) // link\nvalue\nlink\nvalue\n\0
{
    char cur = '\r';
    int index = 0;

    char cur_word[1024];
    int cur_word_size = 0;
    int is_link = 1;

    char *link = NULL;
    while (cur != '\0')
    {
        cur = links[index];

        if (cur == '\n')
        {
            if (is_link)
            {
                link = (char *)malloc(cur_word_size + 1);
                memcpy(link, cur_word, cur_word_size);
                link[cur_word_size] = '\0';

                memset(cur_word, 0, cur_word_size + 1);
                cur_word_size = 0;

                is_link = 0;
            }
            else
            {
                cur_word[cur_word_size] = '\0';
                int value = atoi(cur_word);

                memset(cur_word, 0, cur_word_size + 1);
                cur_word_size = 0;
                is_link = 1;

                hashtable_add_entry(link, value);
                free(link);
            }
        }
        else
        {
            cur_word[cur_word_size] = cur;
            cur_word_size += 1;
        }

        index += 1;
    }
}

unsigned long hashtable_get_attribute(char *attr_name)
{
    hashable_attr *h = (hashable_attr *)malloc(sizeof(hashable_attr));
    HASH_FIND_STR(attributes, attr_name, h);

    unsigned long value = 0;

    if (h)
    {
        value = h->value;
    }

    return value;
}

void hashtable_construct_attributes(char *attribute_data) // assumes data : a\n0\nb\n1\n
{
    char *token = strtok(attribute_data, "\n");
    char *name = token;

    int is_token_key = 0;

    while (token != NULL)
    {
        token = strtok(NULL, "\n");
        if (is_token_key)
        {
            name = token;

            is_token_key = 0;
        }
        else
        {
            char *value = token;

            hashable_attr *new_item = (hashable_attr *)malloc(sizeof(hashable_attr));
            char *key = strdup(name);
            strcpy(new_item->key, key);

            unsigned long value_long = strtoul(value, NULL, 10);
            new_item->value = value_long;

            HASH_ADD_STR(attributes, key, new_item);

            is_token_key = 1;
        }
    }
}

void hashtable_free_attributes()
{
    attributes = NULL;
}

int hashtable_count()
{
    return HASH_COUNT(inode_table);
}

void hashtable_free()
{
    struct hashable *current, *tmp;

    HASH_ITER(hh, inode_table, current, tmp)
    {
        HASH_DEL(inode_table, current);
        free(current);
    }
}