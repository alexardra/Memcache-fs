void memcached_connect();
int memcached_set(char *key, char *value, size_t count);
int memcached_add(char *key, char *value, size_t count);
char *memcached_get(char *key);
int memcached_delete(char *key);

int memcached_flush_all();
