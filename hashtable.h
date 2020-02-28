#define MAX_KEY_SIZE 250

#include "uthash.h"

typedef struct hashable
{
    char key[MAX_KEY_SIZE];
    int inode_value;
    UT_hash_handle hh; /* makes this structure hashable */
} hashable;

typedef struct hashable_attr
{
    char key[MAX_KEY_SIZE];
    unsigned long value;
    UT_hash_handle hh;
} hashable_attr;

hashable *inode_table;

hashable_attr *attributes;

void hashtable_init();
void hashtable_add_entry(char *link, int inode_value);
int hashable_get_entry(char *link);
void hashtable_string_to_table(char *links);
int hashtable_count();
int hashtable_remove_entry(char *link);

void hashtable_construct_attributes(char *attribute_data);
unsigned long hashtable_get_attribute(char *attr_name);
void hashtable_free_attributes();

void hashtable_free();