Fuse -- Memcached
Design Document
======================================

Project Architecture:

    1. How data is stored in memcached?

        Data is stored in memcached server with ascii protocol. Connection to server is 
        established via sockets. (connection and read/write api - memcached.h/c)

        Convertion of file system to key/value pairs is following:
        
            Global inode list exists with key - 'inode_table', value - paths with corresponding
            inode ids. This 'inode_table' value is a string and has following structure:
               path1\n1\path2\n2. (path1 - 1, path2 - 2).

            Global 'inode_value' key exists in storage. It's a unique number and corresponds to 
            smallest inode id that is not yet used by file system. Existance of 'inode_value' in
            storage ensures that st_ino variable is unique for every inode.

            File metadata is stored in each inode. Key for inode is inode id and value is metadata 
            stored as a string. File content is stored in blocks. Each block has key of following
            structure: 1_b_3 (1 - inode id, 3 - block number). Blocks are 1024 in size for fitting
            in ip datagrams. (To avoid ip fragmentation).

            Directory metadata is stored same way. Blocks for directories are not provided. Every
            directory has just one block and this block contains all its links as a string. This
            string has following structure: path1\npath2\npath3\n 

            In runtime inode hashset is constructed (uthash) and when certain files are accessed, 
            this hashset is used for looking up inodes corresponding to paths, then corresponding
            inode metadata is pulled from memcached server and then block contents are pulled.
            This hashset is constructed in memcached_init with existing 'inode_table' value. When
            new files, directories or links are created, 'inode_table' is updated and new paths are
            added to the string, also hashset is updated with path-inode id pair.

    2. What are structures of directories, files?

        As described above, files, directories and links are all stored as inodes. 
        In case of softlinks, new blocks are not constructed and source file path is stored in 
        inode metadata, because of the requirement that path can not have size longer than 250, 
        so the path does not make inode metadata too large and although this design is different
        from how file is stored, additional request is not needed for small data. 

    3. How big files are stored?

        All files (does not depend on size) are stored as blocks. Each block is 1024 bytes in size. 
        With packet headers, it's less than 1500 bytes and each block will be sent as just one packet.

    4. How random read/write work for files/directories?

        random_access.h/c - Logic for random read/write.
        With given offset and size blocks of files are calculated. Also calculated is 
        how many bytes should be writen in which block and if first byte should be read from 
        file, there is no need to access all blocks of the file, just the first block. So if 
        some block is not needed for read/write, it will not be pulled from server. 