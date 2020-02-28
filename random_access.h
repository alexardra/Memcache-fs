typedef struct
{
    int start_block;
    int offset_in_start_block;
    int num_blocks;
    int bytes_in_end_block;
} file_blocks_t;

file_blocks_t *get_file_blocks_info(off_t offset, size_t size, int block_size);