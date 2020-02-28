#include <stdio.h>
#include <stdlib.h>

#include "random_access.h"

file_blocks_t *get_file_blocks_info(off_t offset, size_t size, int block_size)
{
    file_blocks_t *block = (file_blocks_t *)malloc(sizeof(file_blocks_t));

    block->start_block = (int)(offset / block_size);

    int bytes_left_in_start_block = block_size - (offset - block->start_block * block_size);

    block->offset_in_start_block = offset % block_size;

    if (size <= bytes_left_in_start_block)
    {
        block->num_blocks = 1;
        block->bytes_in_end_block = size;
    }
    else
    {
        int s = size - bytes_left_in_start_block;

        if (size % block_size == 0)
        {
            block->num_blocks = 1 + (int)(s / block_size);
            block->bytes_in_end_block = block_size;
        }
        else
        {
            block->num_blocks = 2 + (int)(s / block_size);
            block->bytes_in_end_block = (size - bytes_left_in_start_block) % block_size;
        }
    }

    return block;
}
