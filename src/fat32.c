#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/stdlib/string.h"
#include "header/filesystem/fat32.h"

const uint8_t fs_signature[BLOCK_SIZE] = {
    'C',
    'o',
    'u',
    'r',
    's',
    'e',
    ' ',
    ' ',
    ' ',
    ' ',
    ' ',
    ' ',
    ' ',
    ' ',
    ' ',
    ' ',
    'D',
    'e',
    's',
    'i',
    'g',
    'n',
    'e',
    'd',
    ' ',
    'b',
    'y',
    ' ',
    ' ',
    ' ',
    ' ',
    ' ',
    'L',
    'a',
    'b',
    ' ',
    'S',
    'i',
    's',
    't',
    'e',
    'r',
    ' ',
    'I',
    'T',
    'B',
    ' ',
    ' ',
    'M',
    'a',
    'd',
    'e',
    ' ',
    'w',
    'i',
    't',
    'h',
    ' ',
    '<',
    '3',
    ' ',
    ' ',
    ' ',
    ' ',
    '-',
    '-',
    '-',
    '-',
    '-',
    '-',
    '-',
    '-',
    '-',
    '-',
    '-',
    '2',
    '0',
    '2',
    '4',
    '\n',
    [BLOCK_SIZE - 2] = 'O',
    [BLOCK_SIZE - 1] = 'k',
};

static struct FAT32DriverState driver_state = {0};

/**
 * Convert cluster number to logical block address
 *
 * @param cluster Cluster number to convert
 * @return uint32_t Logical Block Address
 */
uint32_t cluster_to_lba(uint32_t cluster)
{
    return cluster * CLUSTER_BLOCK_COUNT;
}

/**
 * Initialize DirectoryTable value with
 * - Entry-0: DirectoryEntry about itself
 * - Entry-1: Parent DirectoryEntry
 *
 * @param dir_table          Pointer to directory table
 * @param name               8-byte char for directory name
 * @param parent_dir_cluster Parent directory cluster number
 */
void init_directory_table(struct FAT32DirectoryTable *dir_table, char *name, uint32_t parent_dir_cluster)
{
    uint16_t cluster_low = parent_dir_cluster & 0xFFFF;
    uint16_t cluster_high = (parent_dir_cluster >> 16) & 0xFFFF;
    dir_table->table[0].cluster_low = cluster_low;
    dir_table->table[0].cluster_high = cluster_high;
    dir_table->table[0].user_attribute = UATTR_NOT_EMPTY;
    dir_table->table[0].attribute = ATTR_SUBDIRECTORY;
    memcpy(dir_table->table[0].name, name, 8);
}

uint32_t move_to_child_directory(struct FAT32DriverRequest request)
{
    struct FAT32DirectoryTable directory;
    read_clusters(&directory, request.parent_cluster_number, 1);
    int dir_length = sizeof(struct FAT32DirectoryTable) / sizeof(struct FAT32DirectoryEntry);
    for (int i = 1; i < dir_length; i++)
    {
        struct FAT32DirectoryEntry current_child = directory.table[i];
        bool current_entry_name_equal = memcmp(current_child.name, request.name, 8) == 0;
        bool current_entry_ext_equal = memcmp(current_child.ext, "dir", 3) == 0;
        if (current_entry_ext_equal && current_entry_name_equal)
        {
            return current_child.cluster_high << 16 | current_child.cluster_low;
        }
    }
    return 0;
}

uint32_t move_to_parent_directory(struct FAT32DriverRequest request)
{
    struct FAT32DirectoryTable directory;
    read_clusters(&directory, request.parent_cluster_number, 1);
    return directory.table->cluster_high << 16 | directory.table->cluster_low;
    ;
}

// Helper function to check if two directory requests represent the same directory
bool is_same_directory(struct FAT32DriverRequest req1, struct FAT32DriverRequest req2)
{
    return (req1.parent_cluster_number == req2.parent_cluster_number) &&
           (memcmp(req1.name, req2.name, sizeof(req1.name)) == 0) &&
           (memcmp(req1.ext, req2.ext, sizeof(req1.ext)) == 0);
}

// Helper function to find the directory entry index in a directory table
int32_t find_entry_index(struct FAT32DirectoryTable *dir_table, char *name, char *ext) {
    for (int i = 0; i < CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry); i++) {
        if (memcmp(dir_table->table[i].name, name, 8) == 0 &&
            memcmp(dir_table->table[i].ext, ext, 3) == 0 &&
            dir_table->table[i].user_attribute == UATTR_NOT_EMPTY) {
            return i;
        }
    }
    return -1;
}

// Helper function to find an empty slot in the directory table
int32_t find_empty_entry_index(struct FAT32DirectoryTable *dir_table) {
    struct FAT32DirectoryEntry empty_entry;
    memset(&empty_entry, 0, sizeof(struct FAT32DirectoryEntry));
    for (int i = 0; i < CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry); i++) {
        if (memcmp(&dir_table->table[i], &empty_entry, sizeof(struct FAT32DirectoryEntry)) == 0) {
            return i;
        }
    }
    return -1;
}

uint32_t move_dir(struct FAT32DriverRequest src_req, struct FAT32DriverRequest dest_req) {
    struct FAT32DirectoryTable src_dir_table;
    struct FAT32DirectoryTable dest_dir_table;
    int32_t src_entry_index, dest_entry_index;

    // Check if the source and destination directories are the same
    if (is_same_directory(src_req, dest_req)) {
        return 4;  // Source and destination directories are the same
    }

    // Read source directory table
    src_req.buf = &src_dir_table;
    src_req.buffer_size = sizeof(struct FAT32DirectoryTable);


    // Find the entry to be moved in the source directory table
    src_entry_index = find_entry_index(&src_dir_table, src_req.name, src_req.ext);
    if (src_entry_index == -1) {
        return 1;  // Source entry not found
    }

    // Copy the entry to be moved
    struct FAT32DirectoryEntry entry_to_move = src_dir_table.table[src_entry_index];

    // Read destination directory table
    dest_req.buf = &dest_dir_table;
    dest_req.buffer_size = sizeof(struct FAT32DirectoryTable);
    if (read_directory(dest_req) != 0) {
        return 2;  // Failed to read destination directory
    }

    // Find an empty slot in the destination directory table
    dest_entry_index = find_empty_entry_index(&dest_dir_table);
    if (dest_entry_index == -1) {
        return 3;  // Destination directory is full
    }

    // Add the entry to the destination directory table
    dest_dir_table.table[dest_entry_index] = entry_to_move;

    // Clear the entry from the source directory table
    memset(&src_dir_table.table[src_entry_index], 0, sizeof(struct FAT32DirectoryEntry));

    // Write back the updated source and destination directory tables
    write_clusters(&src_dir_table, src_req.parent_cluster_number, 1);
    write_clusters(&dest_dir_table, dest_req.parent_cluster_number, 1);

    return 0;  // Success
}

bool is_empty_storage(void)
{
    struct BlockBuffer boot_sector;
    read_blocks(&boot_sector, BOOT_SECTOR, 1);
    return memcmp(&boot_sector, fs_signature, BLOCK_SIZE);
}

/**
 * Create new FAT32 file system. Will write fs_signature into boot sector and
 * proper FileAllocationTable (contain CLUSTER_0_VALUE, CLUSTER_1_VALUE,
 * and initialized root directory) into cluster number 1
 */
void create_fat32(void)
{
    write_blocks(fs_signature, BOOT_SECTOR, 1);

    driver_state.fat_table.cluster_map[0] = CLUSTER_0_VALUE;
    driver_state.fat_table.cluster_map[1] = CLUSTER_1_VALUE;
    driver_state.fat_table.cluster_map[ROOT_CLUSTER_NUMBER] = FAT32_FAT_END_OF_FILE;

    for (uint16_t i = 3; i < CLUSTER_MAP_SIZE; i++)
    {
        driver_state.fat_table.cluster_map[i] = FAT32_FAT_EMPTY_ENTRY;
    }

    write_clusters(&driver_state.fat_table, FAT_CLUSTER_NUMBER, 1);

    struct FAT32DirectoryTable root_dir_table = {0};
    init_directory_table(&root_dir_table, "root", ROOT_CLUSTER_NUMBER);
    write_clusters(&root_dir_table, ROOT_CLUSTER_NUMBER, 1);
}

/**
 * Initialize file system driver state, if is_empty_storage() then create_fat32()
 * Else, read and cache entire FileAllocationTable (located at cluster number 1) into driver state
 */
void initialize_filesystem_fat32(void)
{
    if (is_empty_storage())
    {
        create_fat32();
    }
    else
    {
        read_clusters(&driver_state.fat_table, FAT_CLUSTER_NUMBER, 1);
    }
}

/**
 * Write cluster operation, wrapper for write_blocks().
 * Recommended to use struct ClusterBuffer
 *
 * @param ptr            Pointer to source data
 * @param cluster_number Cluster number to write
 * @param cluster_count  Cluster count to write, due limitation of write_blocks block_count 255 => max cluster_count = 63
 */
void write_clusters(const void *ptr, uint32_t cluster_number, uint8_t cluster_count)
{
    write_blocks(ptr, cluster_to_lba(cluster_number), cluster_count * CLUSTER_BLOCK_COUNT);
}

/**
 * Read cluster operation, wrapper for read_blocks().
 * Recommended to use struct ClusterBuffer
 *
 * @param ptr            Pointer to buffer for reading
 * @param cluster_number Cluster number to read
 * @param cluster_count  Cluster count to read, due limitation of read_blocks block_count 255 => max cluster_count = 63
 */
void read_clusters(void *ptr, uint32_t cluster_number, uint8_t cluster_count)
{
    read_blocks(ptr, cluster_to_lba(cluster_number), cluster_count * CLUSTER_BLOCK_COUNT);
}

/* -- CRUD Operation -- */

/**
 *  FAT32 Folder / Directory read
 *
 * @param request buf point to struct FAT32DirectoryTable,
 *                name is directory name,
 *                ext is unused,
 *                parent_cluster_number is target directory table to read,
 *                buffer_size must be exactly sizeof(struct FAT32DirectoryTable)
 * @return Error code: 0 success - 1 not a folder - 2 not found - -1 unknown
 */
int8_t read_directory(struct FAT32DriverRequest request)
{
    read_clusters(&driver_state.dir_table_buf, request.parent_cluster_number, 1);

    // Check if parent directory is a folder
    if (driver_state.dir_table_buf.table[0].attribute != ATTR_SUBDIRECTORY)
    {
        return -1;
    }

    // Loop over entries in directory table
    uint32_t directory_size = sizeof(struct FAT32DirectoryTable) / sizeof(struct FAT32DirectoryEntry);
    for (uint32_t i = 0; i < directory_size; i++)
    {
        // Check if name and extension is match
        bool is_name_match = !memcmp(driver_state.dir_table_buf.table[i].name, request.name, 8);
        bool is_ext_match = !memcmp(driver_state.dir_table_buf.table[i].ext, request.ext, 3);

        if (is_name_match && is_ext_match)
        {
            // Check if is a directory
            bool is_directory = driver_state.dir_table_buf.table[i].attribute == ATTR_SUBDIRECTORY;
            if (!is_directory)
            {
                return 1;
            }

            // Read directory table
            uint32_t cluster_number = driver_state.dir_table_buf.table[i].cluster_low | (driver_state.dir_table_buf.table[i].cluster_high << 16);
            read_clusters(&driver_state.dir_table_buf, cluster_number, 1);
            return 0;
        }
    }

    return 2;
}

/**
 * FAT32 read, read a file from file system.
 *
 * @param request All attribute will be used for read, buffer_size will limit reading count
 * @return Error code: 0 success - 1 not a file - 2 not enough buffer - 3 not found - -1 unknown
 */
int8_t read(struct FAT32DriverRequest request)
{
    read_clusters(&driver_state.dir_table_buf, request.parent_cluster_number, 1);

    // Check if parent directory is a folder
    if (driver_state.dir_table_buf.table[0].attribute != ATTR_SUBDIRECTORY)
    {
        return -1;
    }

    // Loop over entries in directory table
    uint32_t directory_size = sizeof(struct FAT32DirectoryTable) / sizeof(struct FAT32DirectoryEntry);
    for (uint32_t i = 0; i < directory_size; i++)
    {
        // Check if name and extension is match
        bool is_name_match = !memcmp(driver_state.dir_table_buf.table[i].name, request.name, 8);
        bool is_ext_match = !memcmp(driver_state.dir_table_buf.table[i].ext, request.ext, 3);

        if (is_name_match && is_ext_match)
        {
            // Check if is a file
            bool is_file = driver_state.dir_table_buf.table[i].attribute != ATTR_SUBDIRECTORY;
            if (!is_file)
            {
                return 1;
            }

            // Check if buffer size is enough
            bool is_buffer_enough = request.buffer_size >= driver_state.dir_table_buf.table[i].filesize;
            if (!is_buffer_enough)
            {
                return 2;
            }

            // Read file content
            uint32_t cluster_number = driver_state.dir_table_buf.table[i].cluster_low | (driver_state.dir_table_buf.table[i].cluster_high << 16);
            uint32_t offset = 0;

            do
            {
                read_clusters(request.buf + offset * CLUSTER_SIZE, cluster_number, 1);
                cluster_number = driver_state.fat_table.cluster_map[cluster_number];
                offset++;
            } while (cluster_number != FAT32_FAT_END_OF_FILE);

            return 0;
        }
    }

    return 3;
}

int32_t ceil_div(int32_t a, int32_t b)
{
    return a / b + (a % b != 0);
}

/**
 * FAT32 write, write a file or folder to file system.
 *
 * @param request All attribute will be used for write, buffer_size == 0 then create a folder / directory
 * @return Error code: 0 success - 1 file/folder already exist - 2 invalid parent cluster - -1 unknown
 */
int8_t write(struct FAT32DriverRequest request)
{
    read_clusters(&driver_state.dir_table_buf, request.parent_cluster_number, 1);

    // Check if parent directory is a folder
    if (driver_state.dir_table_buf.table[0].attribute != ATTR_SUBDIRECTORY)
    {
        return -1;
    }

    // Loop over entries in directory table
    uint32_t directory_size = sizeof(struct FAT32DirectoryTable) / sizeof(struct FAT32DirectoryEntry);
    for (uint32_t i = 0; i < directory_size; i++)
    {
        // Check if name and extension is match
        bool is_name_match = !memcmp(driver_state.dir_table_buf.table[i].name, request.name, 8);
        bool is_ext_match = !memcmp(driver_state.dir_table_buf.table[i].ext, request.ext, 3);

        if (is_name_match && is_ext_match)
        {
            return 1;
        }
    }

    // Check if amount of cluster is enough
    uint32_t cluster_count = ceil_div(request.buffer_size, CLUSTER_SIZE);
    uint32_t cluster_available = 0;
    for (uint32_t i = 2; i < CLUSTER_MAP_SIZE; i++)
    {
        if (driver_state.fat_table.cluster_map[i] == FAT32_FAT_EMPTY_ENTRY)
        {
            cluster_available++;
        }
    }

    if (cluster_available < cluster_count)
    {
        return -1;
    }

    uint32_t empty_cluster = 0;
    for (uint32_t i = 2; i < CLUSTER_MAP_SIZE; i++)
    {
        if (driver_state.fat_table.cluster_map[i] == FAT32_FAT_EMPTY_ENTRY)
        {
            empty_cluster = i;
            break;
        }
    }

    // Write file content
    struct FAT32DirectoryEntry new_entry = {.filesize = request.buffer_size, .user_attribute = UATTR_NOT_EMPTY};
    memcpy(new_entry.name, request.name, 8);
    memcpy(new_entry.ext, request.ext, 3);
    new_entry.cluster_low = empty_cluster & 0xFFFF;
    new_entry.cluster_high = (empty_cluster >> 16) & 0xFFFF;

    if (request.buffer_size == 0)
    {
        new_entry.attribute = ATTR_SUBDIRECTORY;
        struct FAT32DirectoryTable new_dir_table = {0};
        init_directory_table(&new_dir_table, request.name, request.parent_cluster_number);
        driver_state.fat_table.cluster_map[empty_cluster] = FAT32_FAT_END_OF_FILE;
        write_clusters(&new_dir_table, empty_cluster, 1);
    }
    else
    {
        uint32_t empty_clusters[CLUSTER_MAP_SIZE] = {0};
        uint32_t idx = 0;
        for (uint32_t i = 0; i < CLUSTER_MAP_SIZE; i++)
        {
            if (driver_state.fat_table.cluster_map[i] == FAT32_FAT_EMPTY_ENTRY)
            {
                empty_clusters[idx++] = i;
            }
        }

        for (uint32_t i = 0; i < cluster_count; i++)
        {
            uint32_t cluster_number = empty_clusters[i];
            if (i == cluster_count - 1)
            {
                driver_state.fat_table.cluster_map[cluster_number] = FAT32_FAT_END_OF_FILE;
            }
            else
            {
                driver_state.fat_table.cluster_map[cluster_number] = empty_clusters[i + 1];
            }
            write_clusters(request.buf + i * CLUSTER_SIZE, cluster_number, 1);
        }
    }

    uint32_t new_entry_idx = 0;
    for (uint32_t i = 1; i < directory_size; i++)
    {
        if (driver_state.dir_table_buf.table[i].user_attribute != UATTR_NOT_EMPTY)
        {
            new_entry_idx = i;
            break;
        }
    }
    driver_state.dir_table_buf.table[new_entry_idx] = new_entry;
    write_clusters(&driver_state.dir_table_buf, request.parent_cluster_number, 1);
    write_clusters(&driver_state.fat_table, FAT_CLUSTER_NUMBER, 1);

    return 0;
}

/**
 * FAT32 delete, delete a file or empty directory (only 1 DirectoryEntry) in file system.
 *
 * @param request buf and buffer_size is unused
 * @return Error code: 0 success - 1 not found - 2 folder is not empty - -1 unknown
 */
int8_t delete(struct FAT32DriverRequest request)
{
    read_clusters(&driver_state.dir_table_buf, request.parent_cluster_number, 1);

    // Check if parent directory is a folder
    if (driver_state.dir_table_buf.table[0].attribute != ATTR_SUBDIRECTORY)
    {
        return -1;
    }

    // Loop over entries in directory table
    uint32_t directory_size = sizeof(struct FAT32DirectoryTable) / sizeof(struct FAT32DirectoryEntry);
    for (uint32_t i = 0; i < directory_size; i++)
    {
        // Check if name and extension is match
        bool is_name_match = !memcmp(driver_state.dir_table_buf.table[i].name, request.name, 8);
        bool is_ext_match = !memcmp(driver_state.dir_table_buf.table[i].ext, request.ext, 3);

        if (is_name_match && is_ext_match)
        {
            struct FAT32DirectoryEntry entry = driver_state.dir_table_buf.table[i];

            if (entry.attribute == ATTR_SUBDIRECTORY)
            {
                struct FAT32DirectoryTable dir_table;
                uint32_t cluster_number = entry.cluster_low | (entry.cluster_high << 16);
                read_clusters(&dir_table, cluster_number, 1);

                for (uint32_t i = 1; i < directory_size; i++)
                {
                    if (dir_table.table[i].user_attribute == UATTR_NOT_EMPTY)
                    {
                        return 2;
                    }
                }
            }

            // Remove entry
            driver_state.dir_table_buf.table[i].user_attribute = 0;
            memset(driver_state.dir_table_buf.table[i].name, 0, 8);
            memset(driver_state.dir_table_buf.table[i].ext, 0, 3);

            // Remove file content
            uint32_t cluster_number = entry.cluster_low | (entry.cluster_high << 16);
            do
            {
                uint32_t next_cluster = driver_state.fat_table.cluster_map[cluster_number];
                driver_state.fat_table.cluster_map[cluster_number] = FAT32_FAT_EMPTY_ENTRY;
                cluster_number = next_cluster;
            } while (cluster_number != FAT32_FAT_END_OF_FILE);

            write_clusters(&driver_state.dir_table_buf, request.parent_cluster_number, 1);
            write_clusters(&driver_state.fat_table, FAT_CLUSTER_NUMBER, 1);

            return 0;
        }
    }

    return 1;
}

void list_dir_content(char *buffer, uint32_t dir_cluster_number)
{
    struct FAT32DirectoryTable dirtable;
    read_clusters(&dirtable, dir_cluster_number, 1);
    int dir_length = sizeof(struct FAT32DirectoryTable) / sizeof(struct FAT32DirectoryEntry);
    int idx = 0;
    for (int i = 1; i < dir_length; i++)
    {
        struct FAT32DirectoryEntry current_content = dirtable.table[i];
        bool is_current_content_name_na = memcmp(current_content.name, "\0\0\0\0\0\0\0\0", 8) == 0;
        bool is_current_content_ext_na = memcmp(current_content.ext, "\0\0\0", 3) == 0;
        if (is_current_content_name_na && is_current_content_ext_na)
        {
            continue;
        }
        else
        {
            for (int j = 0; j <= 8; j++)
            {
                if (current_content.name[j] == '\0')
                {
                    break;
                }
                buffer[idx] = current_content.name[j];
                idx++;
            }
            if (memcmp(current_content.ext, "dir", 3) == 1)
            { // file
                buffer[idx] = '.';
                idx++;
                for (int j = 0; j <= 3; j++)
                {
                    if (current_content.ext[j] == '\0')
                    {
                        break;
                    }
                    buffer[idx] = current_content.ext[j];
                    idx++;
                }
            }
            else if (memcmp(current_content.ext, "dir", 3) == 0)
            { // folder
                buffer[idx] = '/';
                idx++;
            }
        }
        buffer[idx] = '\n';
        idx++;
    }
}

void print(char *buffer, uint32_t dir_cluster_number)
{
    int dir_idx = 0;
    int level = 0;

    all_list_dir_content(buffer, dir_cluster_number, &dir_idx, &level);
}

void all_list_dir_content(char *buffer, uint32_t dir_cluster_number, int *dir_idx, int *level)
{
    // Read the directory table from the given cluster number
    struct FAT32DirectoryTable dirtable;
    read_clusters(&dirtable, dir_cluster_number, 1);

    // Calculate the number of entries in the directory table
    int dir_length = sizeof(struct FAT32DirectoryTable) / sizeof(struct FAT32DirectoryEntry);

    // Iterate over each entry in the directory table
    for (int i = 1; i < dir_length; i++)
    {
        // Get the current directory entry
        struct FAT32DirectoryEntry current_content = dirtable.table[i];

        // Check if the name and extension are null (empty entry)
        bool is_current_content_name_na = memcmp(current_content.name, "\0\0\0\0\0\0\0\0", 8) == 0;
        bool is_current_content_ext_na = memcmp(current_content.ext, "\0\0\0", 3) == 0;

        // Skip the entry if it's empty
        if (is_current_content_name_na && is_current_content_ext_na)
        {
            continue;
        }
        else
        {
            for (int i = 0; i < (*level); i++)
            {
                buffer[(*dir_idx)] = ' ';
                (*dir_idx)++;

                buffer[(*dir_idx)] = ' ';
                (*dir_idx)++;

                buffer[(*dir_idx)] = ' ';
                (*dir_idx)++;
            }

            // Copy the name of the entry to the buffer
            for (int j = 0; j <= 8; j++)
            {
                if (current_content.name[j] == '\0')
                {
                    break;
                }
                buffer[(*dir_idx)] = current_content.name[j];
                (*dir_idx)++;
            }

            // Check if it's a directory
            if (memcmp(current_content.ext, "dir", 3) == 0)
            {
                // If it's a directory, append '/' to the buffer
                buffer[(*dir_idx)] = '/';
                (*dir_idx)++;

                buffer[(*dir_idx)] = '\n';
                (*dir_idx)++;

                // Recursively print the contents of the subdirectory
                uint32_t sub_dir_cluster_number = current_content.cluster_low | (current_content.cluster_high << 16);
                (*level)++;
                all_list_dir_content(buffer, sub_dir_cluster_number, dir_idx, level);
                (*level)--;
            }
            else if (memcmp(current_content.ext, "dir", 3) == 1)
            {
                buffer[(*dir_idx)] = '.';
                (*dir_idx)++;
                buffer[(*dir_idx)] = current_content.ext[0];
                (*dir_idx)++;
                buffer[(*dir_idx)] = current_content.ext[1];
                (*dir_idx)++;
                buffer[(*dir_idx)] = current_content.ext[2];
                (*dir_idx)++;
                buffer[(*dir_idx)] = '\n';
                (*dir_idx)++;
            }
            else
            {
                buffer[(*dir_idx)] = '\n';
                (*dir_idx)++;
            }
        }
    }
}

// Custom strncpy function
void custom_strncpy(char *dest, const char *src, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        if (i < n)
        {
            dest[i] = src[i];
        }
        if (src[i] == '\0')
        {
            break;
        }
    }
    if (n > 0)
    {
        dest[n - 1] = '\0';
    }
}

// Custom strcmp function
int custom_strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

void clear_buffer(char *buffer, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        buffer[i] = '\0';
    }
}

void print_path_to_dir(char *buffer, uint32_t dir_cluster_number, const char *target_dir_name)
{
    int dir_idx = 0;
    int level = 0;
    bool found = false;

    clear_buffer(buffer, (size_t)255);
    find_and_print_path(buffer, dir_cluster_number, target_dir_name, &dir_idx, &level, &found);
}

void find_and_print_path(char *buffer, uint32_t dir_cluster_number, const char *target_dir_name, int *dir_idx, int *level, bool *found)
{
    // Read the directory table from the given cluster number
    struct FAT32DirectoryTable dirtable;
    read_clusters(&dirtable, dir_cluster_number, 1);

    // Calculate the number of entries in the directory table
    int dir_length = sizeof(dirtable.table) / sizeof(dirtable.table[0]);

    *found = false;
    bool found_lokal = false;
    // Iterate over each entry in the directory table
    for (int i = 0; i < dir_length; i++)
    {
        // if (*found) return; // Stop searching if we've found the directory

        // Get the current directory entry
        struct FAT32DirectoryEntry current_content = dirtable.table[i];

        // Check if the name and extension are null (empty entry)
        bool is_current_content_name_na = memcmp(current_content.name, "\0\0\0\0\0\0\0\0", 8) == 0;
        bool is_current_content_ext_na = memcmp(current_content.ext, "\0\0\0", 3) == 0;

        // Skip the entry if it's empty
        if (is_current_content_name_na && is_current_content_ext_na)
        {
            continue;
        }

        // Prepare the name for comparison
        char name[9];
        custom_strncpy(name, current_content.name, 8);
        name[8] = '\0';

        // Compare with the target directory name
        if (memcmp(current_content.ext, "dir", 3) == 0 && custom_strcmp(name, target_dir_name) == 0)
        {
            // If it's the target directory, print the path and stop
            for (int j = 0; j < *level; j++)
            {
                buffer[(*dir_idx)] = ' ';
                (*dir_idx)++;

                buffer[(*dir_idx)] = ' ';
                (*dir_idx)++;

                buffer[(*dir_idx)] = ' ';
                (*dir_idx)++;
            }
            for (int j = 0; j < 8; j++)
            {
                if (current_content.name[j] == '\0')
                    break;
                buffer[(*dir_idx)] = current_content.name[j];
                (*dir_idx)++;
            }
            buffer[(*dir_idx)] = '/';
            (*dir_idx)++;

            buffer[(*dir_idx)] = '\n';
            (*dir_idx)++;

            *found = true;
        }
        else if (memcmp(current_content.ext, "dir", 3) == 0)
        {
            // If it's a directory, add its name to the path and search recursively
            for (int j = 0; j < *level; j++)
            {
                buffer[(*dir_idx)] = ' ';
                (*dir_idx)++;

                buffer[(*dir_idx)] = ' ';
                (*dir_idx)++;

                buffer[(*dir_idx)] = ' ';
                (*dir_idx)++;
            }
            for (int j = 0; j < 8; j++)
            {
                if (current_content.name[j] == '\0')
                    break;
                buffer[(*dir_idx)] = current_content.name[j];
                (*dir_idx)++;
            }
            buffer[(*dir_idx)] = '/';
            (*dir_idx)++;

            buffer[(*dir_idx)] = '\n';
            (*dir_idx)++;

            // Recursively search the subdirectory
            uint32_t sub_dir_cluster_number = current_content.cluster_low | (current_content.cluster_high << 16);
            (*level)++;

            // handling for case if directories that doesn't contain the searched file/directory is located after the directory where file/directory is found
            // resulting in deletion of path even though the file/directory is found
            if (*found)
            {
                found_lokal = true;
            }
            find_and_print_path(buffer, sub_dir_cluster_number, target_dir_name, dir_idx, level, found);

            (*level)--;

            if (!*found)
            {
                // Remove first \n
                (*dir_idx)--;
                buffer[(*dir_idx)] = '\0';

                // If not found, remove the directory name from the path
                while (*dir_idx > 0 && buffer[*dir_idx - 1] != '\n')
                {
                    (*dir_idx)--;
                    buffer[(*dir_idx)] = '\0';
                }
            }
        }
        else if (memcmp(current_content.ext, "dir", 3) == 1)
        {
            if (custom_strcmp(name, target_dir_name) == 0)
            {
                for (int j = 0; j < *level; j++)
                {
                    buffer[(*dir_idx)] = ' ';
                    (*dir_idx)++;

                    buffer[(*dir_idx)] = ' ';
                    (*dir_idx)++;

                    buffer[(*dir_idx)] = ' ';
                    (*dir_idx)++;
                }
                for (int j = 0; j < 8; j++)
                {
                    if (current_content.name[j] == '\0')
                        break;
                    buffer[(*dir_idx)] = current_content.name[j];
                    (*dir_idx)++;
                }
                buffer[(*dir_idx)] = '.';
                (*dir_idx)++;
                buffer[(*dir_idx)] = current_content.ext[0];
                (*dir_idx)++;
                buffer[(*dir_idx)] = current_content.ext[1];
                (*dir_idx)++;
                buffer[(*dir_idx)] = current_content.ext[2];
                (*dir_idx)++;
                buffer[(*dir_idx)] = '\n';
                (*dir_idx)++;

                *found = true;
            }
        }
    }

    if (found_lokal)
    {
        *found = true;
    }
}

bool knuth_morris_pratt(char *buffer_pattern, char *buffer_text) {
    int m = strlen(buffer_pattern);
    int n = strlen(buffer_text);
    if (m == 0) return false;

    // Bounding Function
    int pi[m];
    pi[0] = 0;
    int k = 0;
    for (int i = 1; i < m; i++) {
        while (k > 0 && buffer_pattern[k] != buffer_pattern[i]) {
            k = pi[k - 1];
        }
        if (buffer_pattern[k] == buffer_pattern[i]) {
            k++;
        }
        pi[i] = k;
    }

    int j = 0;
    for (int i = 0; i < n; i++) {
        while (j > 0 && buffer_pattern[j] != buffer_text[i]) {
            j = pi[j - 1];
        }
        if (buffer_pattern[j] == buffer_text[i]) {
            j++;
        }
        if (j == m) {
            return true;
        }
    }

    return false;
}

bool boyer_moore(char *buffer_pattern, char *buffer_text){
    int m = strlen(buffer_pattern);
    int n = strlen(buffer_text);

    int bad_char[256];

    // Last Occurence Function
    for (int i = 0; i < 256; i++)
    {
        bad_char[i] = -1;
    }
    for (int i = 0; i < m; i++)
    {
        bad_char[(unsigned char)buffer_pattern[i]] = i; 
    }

    int s = 0;
    while (s <= (n - m))
    {
        int j = m - 1;

        while (j >= 0 && buffer_pattern[j] == buffer_text[s + j])
        {
            j--;
        }

        if (j < 0)
        {
            return true;
        }
        else
        {
            s += (j - bad_char[(unsigned char)buffer_text[s + j]] > 1) ? j - bad_char[(unsigned char)buffer_text[s + j]] : 1;
        }
    }

    return false;
}

// ----------------Using Boyer-Moore------------------
void search_dls_bm(char *buffer, uint32_t dir_cluster_number, char *pattern_input) {
    int idx = 0;
    int level = 0;
    int limit = 10;
    bool found = false;

    clear_buffer(buffer, (size_t)1024); 
    depth_limited_search_bm(buffer, dir_cluster_number, pattern_input, &idx, &level, limit, &found);
}

void depth_limited_search_bm(char *buffer, uint32_t dir_cluster_number, char *pattern_input, int *idx, int *level, int limit, bool *found) {
    if (*level > limit) {
        return;
    }

    struct FAT32DirectoryTable dirtable;
    read_clusters(&dirtable, dir_cluster_number, 1);
    int dir_length = sizeof(dirtable.table) / sizeof(dirtable.table[0]);

    *found = false;
    bool local_found = false;
    for (int i = 0; i < dir_length; i++) {
        struct FAT32DirectoryEntry current_content = dirtable.table[i];

        bool is_current_content_name_na = memcmp(current_content.name, "\0\0\0\0\0\0\0\0", 8) == 0;
        bool is_current_content_ext_na = memcmp(current_content.ext, "\0\0\0", 3) == 0;

        if (is_current_content_name_na && is_current_content_ext_na) {
            continue;
        }

        if (memcmp(current_content.ext, "dir", 3) == 0) {
            // If it's a directory, append '/' to the buffer and search recursively
            for (int j = 0; j < *level; j++) {
                buffer[(*idx)] = ' ';
                (*idx)++;
                buffer[(*idx)] = ' ';
                (*idx)++;
                buffer[(*idx)] = ' ';
                (*idx)++;
            }
            for (int j = 0; j < 8; j++) {
                if (current_content.name[j] == '\0') break;
                buffer[(*idx)] = current_content.name[j];
                (*idx)++;
            }
            buffer[(*idx)] = '/';
            (*idx)++;
            buffer[(*idx)] = '\n';
            (*idx)++;

            // Recursively search the subdirectory
            uint32_t sub_dir_cluster_number = current_content.cluster_low | (current_content.cluster_high << 16);
            (*level)++;
            if (*found)
            {
                local_found = true;
            }
            depth_limited_search_bm(buffer, sub_dir_cluster_number, pattern_input, idx, level, limit, found);
            (*level)--;
            if (!*found)
            {
                // Remove first \n
                (*idx)--;
                buffer[(*idx)] = '\0';

                // If not found, remove the directory name from the path
                while (*idx > 0 && buffer[*idx - 1] != '\n')
                {
                    (*idx)--;
                    buffer[(*idx)] = '\0';
                }
            }
        } else if (memcmp(current_content.ext, "txt", 3) == 0) {
            // If it's a text file, read the file content and check for the pattern
            struct FAT32DriverRequest request;

            // custom_strncpy(request.name, current_content.name, 8);
            // custom_strncpy(request.ext, current_content.ext, 4);
            memcpy(request.name, current_content.name, 8);
            memcpy(request.ext, current_content.ext, 4);
            request.buffer_size = current_content.filesize;
            char file_content[current_content.filesize];
            request.buf = file_content;
            request.parent_cluster_number = dir_cluster_number;

            if (read(request) == 0) {
                read_clusters(&dirtable, dir_cluster_number, 1);
                current_content = dirtable.table[i];
                // If the pattern matches, append the file details to the buffer
                if (boyer_moore(pattern_input, file_content)) {
                    for (int j = 0; j < *level; j++) {
                        buffer[(*idx)] = ' ';
                        (*idx)++;
                        buffer[(*idx)] = ' ';
                        (*idx)++;
                        buffer[(*idx)] = ' ';
                        (*idx)++;
                    }
                    for (int j = 0; j < 8; j++) {
                        if (current_content.name[j] == '\0') break;
                        buffer[(*idx)] = current_content.name[j];
                        (*idx)++;
                    }
                    buffer[(*idx)] = '.';
                    (*idx)++;
                    for (int j = 0; j < 3; j++) {
                        if (current_content.ext[j] == '\0') break;
                        buffer[(*idx)] = current_content.ext[j];
                        (*idx)++;
                    }
                    buffer[(*idx)] = ' ';
                    (*idx)++;
                    for (int j = 0; j < strlen(file_content); j++) {
                        buffer[(*idx)] = file_content[j];
                        (*idx)++;
                    }
                    buffer[(*idx)] = '\n';
                    (*idx)++;

                    *found = true;
                }
            }
        }
    }
    if (local_found)
    {
        *found = true;
    }
}

// ----------------Using Knuth-Morris-Pratt------------------
void search_dls_kmp(char *buffer, uint32_t dir_cluster_number, char *pattern_input) {
    int idx = 0;
    int level = 0;
    int limit = 10;
    bool found = false;

    clear_buffer(buffer, (size_t)1024); 
    depth_limited_search_kmp(buffer, dir_cluster_number, pattern_input, &idx, &level, limit, &found);
}

void depth_limited_search_kmp(char *buffer, uint32_t dir_cluster_number, char *pattern_input, int *idx, int *level, int limit, bool *found) {
    if (*level > limit) {
        return;
    }

    struct FAT32DirectoryTable dirtable;
    read_clusters(&dirtable, dir_cluster_number, 1);
    int dir_length = sizeof(dirtable.table) / sizeof(dirtable.table[0]);

    *found = false;
    bool local_found = false;
    for (int i = 0; i < dir_length; i++) {
        struct FAT32DirectoryEntry current_content = dirtable.table[i];

        bool is_current_content_name_na = memcmp(current_content.name, "\0\0\0\0\0\0\0\0", 8) == 0;
        bool is_current_content_ext_na = memcmp(current_content.ext, "\0\0\0", 3) == 0;

        if (is_current_content_name_na && is_current_content_ext_na) {
            continue;
        }

        if (memcmp(current_content.ext, "dir", 3) == 0) {
            // If it's a directory, append '/' to the buffer and search recursively
            for (int j = 0; j < *level; j++) {
                buffer[(*idx)] = ' ';
                (*idx)++;
                buffer[(*idx)] = ' ';
                (*idx)++;
                buffer[(*idx)] = ' ';
                (*idx)++;
            }
            for (int j = 0; j < 8; j++) {
                if (current_content.name[j] == '\0') break;
                buffer[(*idx)] = current_content.name[j];
                (*idx)++;
            }
            buffer[(*idx)] = '/';
            (*idx)++;
            buffer[(*idx)] = '\n';
            (*idx)++;

            // Recursively search the subdirectory
            uint32_t sub_dir_cluster_number = current_content.cluster_low | (current_content.cluster_high << 16);
            (*level)++;
            if (*found)
            {
                local_found = true;
            }
            depth_limited_search_kmp(buffer, sub_dir_cluster_number, pattern_input, idx, level, limit, found);
            (*level)--;
            if (!*found)
            {
                // Remove first \n
                (*idx)--;
                buffer[(*idx)] = '\0';

                // If not found, remove the directory name from the path
                while (*idx > 0 && buffer[*idx - 1] != '\n')
                {
                    (*idx)--;
                    buffer[(*idx)] = '\0';
                }
            }
        } else if (memcmp(current_content.ext, "txt", 3) == 0) {
            // If it's a text file, read the file content and check for the pattern
            struct FAT32DriverRequest request;

            memcpy(request.name, current_content.name, 8);
            memcpy(request.ext, current_content.ext, 4);
            request.buffer_size = current_content.filesize;
            char file_content[current_content.filesize];
            request.buf = file_content;
            request.parent_cluster_number = dir_cluster_number;

            if (read(request) == 0) {
                read_clusters(&dirtable, dir_cluster_number, 1);
                current_content = dirtable.table[i];
                // If the pattern matches, append the file details to the buffer
                if (knuth_morris_pratt(pattern_input, file_content)) {
                    for (int j = 0; j < *level; j++) {
                        buffer[(*idx)] = ' ';
                        (*idx)++;
                        buffer[(*idx)] = ' ';
                        (*idx)++;
                        buffer[(*idx)] = ' ';
                        (*idx)++;
                    }
                    for (int j = 0; j < 8; j++) {
                        if (current_content.name[j] == '\0') break;
                        buffer[(*idx)] = current_content.name[j];
                        (*idx)++;
                    }
                    buffer[(*idx)] = '.';
                    (*idx)++;
                    for (int j = 0; j < 3; j++) {
                        if (current_content.ext[j] == '\0') break;
                        buffer[(*idx)] = current_content.ext[j];
                        (*idx)++;
                    }
                    buffer[(*idx)] = ' ';
                    (*idx)++;
                    for (int j = 0; j < strlen(file_content); j++) {
                        buffer[(*idx)] = file_content[j];
                        (*idx)++;
                    }
                    buffer[(*idx)] = '\n';
                    (*idx)++;

                    *found = true;
                }
            }
        }
    }
    if (local_found)
    {
        *found = true;
    }
}

