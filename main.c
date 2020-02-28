#define FUSE_USE_VERSION 31
#define FILE_BLOCK_SIZE 1024

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#include "memcached_client.h"
#include "hashtable.h"
#include "data_parser.h"
#include "random_access.h"

static void *memcached_init(struct fuse_conn_info *conn, struct fuse_config *cfg);
static int memcached_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi);

static int memcached_mkdir(const char *path, mode_t mode);
static int memcached_rmdir(const char *path);
static int memcached_opendir(const char *path, struct fuse_file_info *fi);
static int memcached_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t off, struct fuse_file_info *fi, enum fuse_readdir_flags);
static int memcached_releasedir(const char *path, struct fuse_file_info *fi);
static int memcached_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi);

static int memcached_unlink(const char *path);
static int memcached_create(const char *path, mode_t mode, struct fuse_file_info *fi);
static int memcached_open(const char *path, struct fuse_file_info *fi);
static int memcached_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int memcached_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int memcached_release(const char *path, struct fuse_file_info *fi);
static int memcached_flush(const char *path, struct fuse_file_info *fi);
static int memcached_fsync(const char *path, int datasync, struct fuse_file_info *fi);

static int memcached_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi);
static int memcached_statfs(const char *path, struct statvfs *statv);
static void memcached_destroy(void *private_data);

static int memcached_setxattr(const char *path, const char *name, const char *value, size_t size, int flags);
static int memcached_getxattr(const char *path, const char *name, char *value, size_t size);
static int memcached_listxattr(const char *path, char *list, size_t size);
static int memcached_removexattr(const char *path, const char *name);

static int memcached_chmod(const char *path, mode_t mode, struct fuse_file_info *fi);
static int memcached_chown(const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi);

static int memcached_link(const char *oldpath, const char *newpath);
static int memcached_symlink(const char *linkname, const char *path);
static int memcached_readlink(const char *path, char *buf, size_t len);

static struct fuse_operations memcached_oper =
    {
        .init = memcached_init,
        .getattr = memcached_getattr,
        .mkdir = memcached_mkdir,
        .rmdir = memcached_rmdir,
        .opendir = memcached_opendir,
        .readdir = memcached_readdir,
        .releasedir = memcached_releasedir,
        .fsyncdir = memcached_fsyncdir,
        .unlink = memcached_unlink,
        .create = memcached_create,
        .open = memcached_open,
        .read = memcached_read,
        .write = memcached_write,
        .release = memcached_release,
        .flush = memcached_flush,
        .fsync = memcached_fsync,
        .utimens = memcached_utimens,
        .statfs = memcached_statfs,
        .destroy = memcached_destroy,
        .setxattr = memcached_setxattr,
        .getxattr = memcached_getxattr,
        .listxattr = memcached_listxattr,
        .removexattr = memcached_removexattr,
        .chmod = memcached_chmod,
        .chown = memcached_chown,
        .link = memcached_link,
        .symlink = memcached_symlink,
        .readlink = memcached_readlink};

static void add_link_to_parent_dir(char *path)
{
    char *parent_dir = get_parent_directory(path);

    int inode_value = hashable_get_entry(parent_dir);

    if (inode_value == -1)
    {
        printf("could not find path: %s\n", path);
    }

    char *inode_key = int_to_string(inode_value);

    char *block_key = block_key_to_string(inode_value, 0);

    char *links = memcached_get(block_key);

    char *link_name = get_name_from_path(path);
    size_t link_size = strlen(link_name);

    if (links == NULL) // first link
    {
        char link[link_size + 2];
        memcpy(link, link_name, link_size);
        link[link_size] = '\n';
        link[link_size + 1] = '\0';

        memcached_set(block_key, link, strlen(link));
    }
    else
    {
        size_t links_size = strlen(links);

        char updated_links[links_size + link_size + 2];

        memcpy(updated_links, links, links_size);
        memcpy(updated_links + links_size, link_name, link_size);
        updated_links[links_size + link_size] = '\n';
        updated_links[links_size + link_size + 1] = '\0';

        memcached_set(block_key, updated_links, strlen(updated_links));

        free(links);
    }

    free(link_name);

    char *attribute_data = memcached_get(inode_key);
    unsigned long st_blocks = get_attr_value(attribute_data, "st_blocks");
    char *data_with_blocks = modify_attr(attribute_data, "st_blocks", st_blocks + 1);
    memcached_set(inode_key, data_with_blocks, strlen(data_with_blocks));

    free(attribute_data);
    free(data_with_blocks);

    free(parent_dir);
    free(inode_key);
    free(block_key);
}

static void remove_link_from_parent_dir(char *path)
{
    char *parent_dir = get_parent_directory(path);

    int inode_value = hashable_get_entry(parent_dir);

    if (inode_value == -1)
    {
        printf("could not find path: %s\n", path);
    }

    char *inode_key = int_to_string(inode_value);

    char *block_key = block_key_to_string(inode_value, 0);

    char *links = memcached_get(block_key);

    char *link_name = get_name_from_path(path);
    size_t link_size = strlen(link_name);

    if (links != NULL) // should always be true
    {
        size_t links_size = strlen(links);
        char *link = strstr(links, link_name);

        int index_of_link = (link - links) / sizeof(char);

        char updated_links[links_size - link_size + 2];

        memcpy(updated_links, links, index_of_link);
        memcpy(updated_links + index_of_link, links + index_of_link + link_size + 1, links_size - index_of_link - link_size + 1);
        updated_links[links_size - link_size + 1] = '\0';

        memcached_set(block_key, updated_links, strlen(updated_links));

        free(links);
    }
}

// content is null if not symlink - otherwise  path to original file
static int create_inode(char *path, mode_t mode, nlink_t nlink, uid_t uid, gid_t gid, off_t size, char *content)
{
    char *ino_value = memcached_get("inode_value");
    int ino = atoi(ino_value);
    free(ino_value);

    char *st_ino = get_attr_pair("st_ino", ino);
    char *st_mode = get_attr_pair("st_mode", mode);
    char *st_uid = get_attr_pair("st_uid", uid);
    char *st_gid = get_attr_pair("st_gid", gid);
    char *st_nlink = get_attr_pair("st_nlink", nlink);
    char *st_size = get_attr_pair("st_size", size);
    char *st_blocks = get_attr_pair("st_blocks", 0);

    size_t inode_length = strlen(st_ino) + strlen(st_mode) + strlen(st_uid) + strlen(st_gid) + strlen(st_nlink) + strlen(st_size) + strlen(st_blocks);
    char *inode = (char *)malloc(inode_length + 1);
    inode[0] = '\0';

    strcat(inode, st_ino);
    strcat(inode, st_mode);
    strcat(inode, st_uid);
    strcat(inode, st_gid);
    strcat(inode, st_nlink);
    strcat(inode, st_size);
    strcat(inode, st_blocks);

    free(st_ino);
    free(st_mode);
    free(st_uid);
    free(st_gid);
    free(st_nlink);
    free(st_size);
    free(st_blocks);

    if (content != NULL) // symlink
    {
        char *content_key = "st_content\n";
        inode = (char *)realloc(inode, inode_length + 1 + strlen(content) + strlen(content_key) + 1);
        strcat(inode, content_key);
        strcat(inode, content);
        strcat(inode, "\n");
    }

    char *key = int_to_string(ino);
    int set = memcached_set(key, inode, strlen(inode));

    char *new_inode_value = int_to_string(ino + 1);
    memcached_set("inode_value", new_inode_value, strlen(new_inode_value));
    free(new_inode_value);

    free(key);
    free(inode);

    if (set == 1)
    {
        hashtable_add_entry((char *)path, ino);

        char *inode_table = memcached_get("inode_table");
        char *new_inode_table = add_inode_to_table(inode_table, (char *)path, ino);
        memcached_set("inode_table", new_inode_table, strlen(new_inode_table)); // what if > 1024

        // ADDING NEW LINK TO DIRECTORY BLOCK
        if (path != "/")
        {
            add_link_to_parent_dir(path);
        }

        free(inode_table);
        free(new_inode_table);

        return 0;
    }

    return -1;
}

static int delete_inode(char *path)
{
    char *inode_table = memcached_get("inode_table");
    char *new_inode_table = remove_inode_from_table(inode_table, path);
    memcached_set("inode_table", new_inode_table, strlen(inode_table));

    free(inode_table);
    free(new_inode_table);

    int inode_value = hashtable_remove_entry(path);

    // remove from memcached server this inode with its blocks
    char *inode_key = int_to_string(inode_value);
    char *attribute_data = memcached_get(inode_key);

    unsigned long st_nlink = get_attr_value(attribute_data, "st_nlink");

    unsigned long st_mode = get_attr_value(attribute_data, "st_mode");

    if (S_ISREG(st_mode) && st_nlink > 1) // hard link
    {
        // do not delete blocks
        char *modified_data = modify_attr(attribute_data, "st_nlink", st_nlink - 1);
        memcached_set(inode_key, modified_data, strlen(modified_data));

        free(modified_data);

        remove_link_from_parent_dir(path);
    }
    else
    {
        unsigned long st_blocks = get_attr_value(attribute_data, "st_blocks");

        for (int i = 0; i < st_blocks; i++)
        {
            char *block_key = block_key_to_string(inode_value, i);
            memcached_delete(block_key);
            free(block_key);
        }

        memcached_delete(inode_key);
        remove_link_from_parent_dir(path);
    }

    free(inode_key);
    free(attribute_data);

    return 0;
}

static void *memcached_init(struct fuse_conn_info *conn,
                            struct fuse_config *cfg)
{
    printf("init \n");

    memcached_connect();

    char *inode_table = memcached_get("inode_table");

    if (inode_table == NULL) // no filesystem stored in memcached
    {
        memcached_flush_all();

        hashtable_init();
        memcached_set("inode_table", "", 0);
        memcached_set("inode_value", "0", 1);

        int set = create_inode("/", S_IFDIR | 0755, 2, getuid(), getgid(), 0, NULL);
    }
    else
    {

        hashtable_string_to_table(inode_table);
    }

    return NULL;
}

static int memcached_getattr(const char *path, struct stat *stbuf,
                             struct fuse_file_info *fi)
{
    printf("get attr %s\n", path);

    int inode_value = hashable_get_entry((char *)path);

    if (inode_value == -1)
    {
        // this path does not exist
        return -ENOENT;
    }

    char *inode_key = int_to_string(inode_value);
    char *attribute_data = memcached_get(inode_key);

    if (attribute_data != NULL)
    {
        stbuf->st_ino = get_attr_value(attribute_data, "st_ino");
        stbuf->st_mode = get_attr_value(attribute_data, "st_mode");
        stbuf->st_uid = get_attr_value(attribute_data, "st_uid");
        stbuf->st_gid = get_attr_value(attribute_data, "st_gid");
        stbuf->st_nlink = get_attr_value(attribute_data, "st_nlink");
        stbuf->st_size = get_attr_value(attribute_data, "st_size");
        stbuf->st_blocks = get_attr_value(attribute_data, "st_blocks");

        free(attribute_data);
        free(inode_key);
    }

    return 0;
}

static int memcached_mkdir(const char *path, mode_t mode)
{
    printf("mkdir: %s\n", path);

    int set = create_inode((char *)path, S_IFDIR | mode, 2, getuid(), getgid(), 0, NULL);

    return 0;
}

static int memcached_rmdir(const char *path)
{
    int inode_value = hashable_get_entry((char *)path);
    char *inode_key = int_to_string(inode_value);

    char *block_key = block_key_to_string(inode_value, 0);
    char *links = memcached_get(block_key);

    if (links != NULL && strlen(links) > 0)
    {
        free(links);
        free(block_key);
        free(inode_key);

        return -ENOTEMPTY;
    }

    delete_inode((char *)path);

    free(links);
    free(block_key);
    free(inode_key);

    return 0;
}

static int memcached_opendir(const char *path, struct fuse_file_info *fi)
{
    int inode_value = hashable_get_entry((char *)path);

    if (inode_value == -1)
    {
        // this path does not exist
        return -ENOENT;
    }

    return 0;
}

static int memcached_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t off, struct fuse_file_info *fi, enum fuse_readdir_flags flags)
{
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    // get links and parse
    int inode_value = hashable_get_entry((char *)path);
    char *inode_key = int_to_string(inode_value);

    char *block_key = block_key_to_string(inode_value, 0);

    char *links = memcached_get(block_key);

    if (links != NULL)
    {
        char c = 'c';
        int index = 0;
        int size = 0;

        while (c != '\0')
        {
            c = links[index];
            if (c == '\n')
            {
                char link[size + 1];
                link[size] = '\0';
                memcpy(link, links + index - size, size);
                filler(buf, link, NULL, 0, 0);
                size = 0;
            }
            else
            {
                size += 1;
            }

            index += 1;
        }
    }

    return 0;
}

static int memcached_releasedir(const char *path, struct fuse_file_info *fi)
{
    return 0;
}

static int memcached_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi)
{
    return 0;
}

static int memcached_unlink(const char *path)
{
    delete_inode((char *)path);

    return 0;
}

static int memcached_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    char *parent_dir = get_parent_directory(path);
    free(parent_dir);

    int set = create_inode((char *)path, mode, 1, getuid(), getgid(), 0, NULL);

    return 0;
}

static int memcached_open(const char *path, struct fuse_file_info *fi) // ???
{
    int inode_value = hashable_get_entry((char *)path);

    if (inode_value == -1)
    {
        // this path does not exist
        return -ENOENT;
    }

    return 0;
}

static int memcached_read(const char *path, char *buf, size_t size, off_t offset,
                          struct fuse_file_info *fi)
{
    int inode_value = hashable_get_entry((char *)path);

    char *inode_key = int_to_string(inode_value);
    char *attribute_data = memcached_get(inode_key);

    unsigned long st_size = get_attr_value(attribute_data, "st_size");

    if (st_size <= offset) // offset at or beyond end of file
    {
        return 0;
    }

    if (offset + size > st_size)
    {
        size = st_size - offset;
    }

    file_blocks_t *block_info = get_file_blocks_info(offset, size, FILE_BLOCK_SIZE);

    size_t already_read_bytes = 0;

    for (int i = 0; i < block_info->num_blocks; i++)
    {
        int current_block_num = block_info->start_block + i;
        char *block_key = block_key_to_string(inode_value, current_block_num);

        char *data = memcached_get(block_key);

        if (data == NULL)
        {
            data = (char *)malloc(FILE_BLOCK_SIZE + 1);
            memset(data, 0, FILE_BLOCK_SIZE);
            data[FILE_BLOCK_SIZE] = '\0';
        }

        size_t read_offset = (i == 0) ? block_info->offset_in_start_block : 0;

        if (i == 0 && block_info->num_blocks == 1)
        {
            memcpy(buf, data + read_offset, size);
        }
        else if (i == 0 && block_info->num_blocks > 1)
        {
            size_t read_size = FILE_BLOCK_SIZE - read_offset;
            memcpy(buf, data + read_offset, read_size);
            already_read_bytes += read_size;
        }
        else // i != 0
        {
            size_t read_size = (i == block_info->num_blocks - 1) ? block_info->bytes_in_end_block : FILE_BLOCK_SIZE;
            memcpy(buf + already_read_bytes, data + read_offset, read_size);
            already_read_bytes += read_size;
        }

        free(block_key);
        free(data);
    }

    free(block_info);

    return size;
}

static int memcached_write(const char *path, const char *buf, size_t size, off_t offset,
                           struct fuse_file_info *fi)
{
    int inode_value = hashable_get_entry((char *)path);

    file_blocks_t *block_info = get_file_blocks_info(offset, size, FILE_BLOCK_SIZE);

    size_t written_bytes = 0;

    for (int i = 0; i < block_info->num_blocks; i++)
    {
        int current_block_num = block_info->start_block + i;
        char *block_key = block_key_to_string(inode_value, current_block_num);

        char *data = memcached_get(block_key);

        if (data == NULL) // this block does not exist
        {
            data = (char *)malloc(FILE_BLOCK_SIZE + 1);
            memset(data, 0, FILE_BLOCK_SIZE);
            data[FILE_BLOCK_SIZE] = '\0';
        }

        size_t write_offset = (i == 0) ? block_info->offset_in_start_block : 0;

        if (i == 0 && block_info->num_blocks == 1)
        {
            memcpy(data + write_offset, buf, size);
        }
        else if (i == 0 && block_info->num_blocks > 1)
        {
            size_t write_size = FILE_BLOCK_SIZE - write_offset;
            memcpy(data + write_offset, buf + written_bytes, write_size);
            written_bytes += write_size;
        }
        else // i != 0
        {
            size_t write_size = (i == block_info->num_blocks - 1) ? block_info->bytes_in_end_block : FILE_BLOCK_SIZE;
            memcpy(data + write_offset, buf + written_bytes, write_size);
            written_bytes += write_size;
        }

        memcached_set(block_key, data, FILE_BLOCK_SIZE);

        free(block_key);
        free(data);
    }
    char *inode_key = int_to_string(inode_value);

    char *attribute_data = memcached_get(inode_key);
    char *data_with_blocks = modify_attr(attribute_data, "st_blocks", block_info->num_blocks);

    unsigned long st_size = get_attr_value(attribute_data, "st_size");
    if (offset + size > st_size)
    {
        char *data_with_size = modify_attr(data_with_blocks, "st_size", offset + size);
        memcached_set(inode_key, data_with_size, strlen(data_with_size));
        free(data_with_size);
    }
    else
    {
        memcached_set(inode_key, data_with_blocks, strlen(data_with_blocks));
    }

    free(attribute_data);
    free(data_with_blocks);

    free(block_info);

    return size;
}

static int memcached_release(const char *path, struct fuse_file_info *fi)
{
    return 0;
}

static int memcached_flush(const char *path, struct fuse_file_info *fi)
{
    return 0;
}

static int memcached_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
    return 0;
}

static int memcached_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi)
{
    return 0;
}

static int memcached_statfs(const char *path, struct statvfs *statv)
{
    return 0;
}

static void memcached_destroy(void *private_data)
{
    hashtable_free();
    exit(0);
}

static int memcached_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
    int inode_value = hashable_get_entry((char *)path);
    char *inode_key = int_to_string(inode_value);

    char *attribute_data = memcached_get(inode_key);

    char *attr_value = get_attr_value_str(attribute_data, (char *)name);

    char value_string[size + 1];
    memcpy(value_string, value, size);
    value_string[size] = '\0';

    if (attr_value == NULL) // key does not exist
    {
        char *new_attribute_data = add_attr(attribute_data, (char *)name, value_string);
        memcached_set(inode_key, new_attribute_data, strlen(new_attribute_data));

        free(new_attribute_data);
    }
    else // replace
    {
        char *new_attribute_data = modify_attr_str(attribute_data, (char *)name, value_string);
        memcached_set(inode_key, new_attribute_data, strlen(new_attribute_data));

        free(new_attribute_data);
    }

    free(inode_key);
    free(attribute_data);

    return 0;
}

static int memcached_getxattr(const char *path, const char *name, char *value, size_t size)
{
    int inode_value = hashable_get_entry((char *)path);
    char *inode_key = int_to_string(inode_value);

    char *attribute_data = memcached_get(inode_key);

    char *attr_value = get_attr_value_str(attribute_data, (char *)name);
    if (attr_value == NULL)
    {
        free(inode_key);
        free(attribute_data);

        return 0;
    }
    else
    {
        size_t attr_value_size = strlen(attr_value);

        if (size == 0)
        {
            size = attr_value_size;
        }
        else
        {
            if (size > attr_value_size)
                size = attr_value_size;
            memcpy(value, attr_value, size);
        }

        free(inode_key);
        free(attr_value);
        free(attribute_data);

        return size;
    }
}

static int memcached_listxattr(const char *path, char *list, size_t size)
{
    int inode_value = hashable_get_entry((char *)path);
    char *inode_key = int_to_string(inode_value);

    char *attribute_data = memcached_get(inode_key);

    struct list *extended_attributes = get_extended_attrs_list(attribute_data);

    if (size == 0)
    {
        size = extended_attributes->size;
    }
    else
    {
        if (size > extended_attributes->size)
            size = extended_attributes->size;
        if (size > 0)
        {
            memcpy(list, extended_attributes->keys, size);
        }
    }

    free(inode_key);
    free(attribute_data);
    free(extended_attributes->keys);
    free(extended_attributes);

    return size;
}

static int memcached_removexattr(const char *path, const char *name)
{
    int inode_value = hashable_get_entry((char *)path);
    char *inode_key = int_to_string(inode_value);

    char *attribute_data = memcached_get(inode_key);

    char *result_data = remove_extended_attr(attribute_data, (char *)name);

    if (result_data != NULL)
    {
        memcached_set(inode_key, result_data, strlen(result_data));
    }

    free(inode_key);
    free(attribute_data);
    free(result_data);

    return 0;
}

static int memcached_chmod(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    int inode_value = hashable_get_entry((char *)path);
    char *inode_key = int_to_string(inode_value);
    char *attribute_data = memcached_get(inode_key);

    unsigned long current_mode = get_attr_value(attribute_data, "st_mode");

    char *new_attribute_data = modify_attr(attribute_data, "st_mode", mode);

    memcached_set(inode_key, new_attribute_data, strlen(new_attribute_data));

    free(attribute_data);
    free(new_attribute_data);
    free(inode_key);

    return 0;
}

static int memcached_chown(const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi)
{
    if (uid == -1 && gid == -1)
        return 0;

    int inode_value = hashable_get_entry((char *)path);
    char *inode_key = int_to_string(inode_value);
    char *attribute_data = memcached_get(inode_key);

    char *new_attribute_data = NULL;

    if (uid != -1)
    {
        new_attribute_data = modify_attr(attribute_data, "st_uid", uid);
    }

    if (gid != -1)
    {
        char *data_to_change = (new_attribute_data == NULL) ? attribute_data : new_attribute_data;
        new_attribute_data = modify_attr(data_to_change, "st_gid", gid);
    }

    memcached_set(inode_key, new_attribute_data, strlen(new_attribute_data));

    free(new_attribute_data);
    free(attribute_data);
    free(inode_key);

    return 0;
}

static int memcached_link(const char *oldpath, const char *newpath)
{
    int inode_value = hashable_get_entry((char *)oldpath);

    char *inode_key = int_to_string(inode_value);
    char *attribute_data = memcached_get(inode_key);
    unsigned long st_nlink = get_attr_value(attribute_data, "st_nlink");
    char *modified_data = modify_attr(attribute_data, "st_nlink", st_nlink + 1);
    memcached_set(inode_key, modified_data, strlen(modified_data));

    hashtable_add_entry((char *)newpath, inode_value);

    char *inode_table = memcached_get("inode_table");
    char *new_inode_table = add_inode_to_table(inode_table, (char *)newpath, inode_value);
    memcached_set("inode_table", new_inode_table, strlen(new_inode_table));

    add_link_to_parent_dir((char *)newpath);

    return 0;
}

static int memcached_symlink(const char *linkname, const char *path)
{
    char *parent_dir = get_parent_directory(path);

    create_inode((char *)path, S_IFLNK | 0777, 1, getuid(), getgid(), strlen(linkname), (char *)linkname);

    return 0;
}

static int memcached_readlink(const char *path, char *buf, size_t size)
{
    int inode_value = hashable_get_entry((char *)path);

    char *inode_key = int_to_string(inode_value);
    char *attribute_data = memcached_get(inode_key);

    char *content = get_attr_value_str(attribute_data, "st_content");

    size_t content_size = strlen(content);

    if (size != content_size)
    {
        size = content_size;
    }

    memcpy(buf, content, content_size);

    return 0;
}

int main(int argc, char *argv[])
{
    int ret;

    ret = fuse_main(argc, argv, &memcached_oper, NULL);

    return ret;
}