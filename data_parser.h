typedef struct list
{
    char *keys;
    size_t size;
} list;

char *ulong_to_string(unsigned long x);
char *int_to_string(int x);
char *get_attr_pair(char *attr_name, unsigned long value);
char *block_key_to_string(int inode_value, int block_num);

char *add_attr(char *attribute_data, char *attr_name, char *value);
char *modify_attr(char *attribute_data, char *attr_name, unsigned long new_value);
char *modify_attr_str(char *attribute_data, char *attr_name, char *attr_value);
unsigned long get_attr_value(char *attribute_data, char *attr_name);
char *get_attr_value_str(char *attribute_data, char *attr_name);

char *add_inode_to_table(char *inode_table, char *path, int inode_value);
char *remove_inode_from_table(char *inode_table, char *path);

char *get_parent_directory(const char *path);
char *get_name_from_path(const char *path);

struct list *get_extended_attrs_list(char *attribute_data);
char *remove_extended_attr(char *attribute_data, char *attr_name);

char *construct_path(char *parent_dir, char *linkname);