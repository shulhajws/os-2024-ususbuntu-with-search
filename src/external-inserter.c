#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "header/filesystem/fat32.h"
#include "header/driver/disk.h"
#include "header/stdlib/string.h"

// Global variable
uint8_t* image_storage;
uint8_t* file_buffer;


char* get_filename(char* path) {
    char* filename = strrchr(path, '/');
    if (filename == NULL) {
        return path;
    }
    else {
        return filename + 1;
    }
}

void read_blocks(void* ptr, uint32_t logical_block_address, uint8_t block_count) {
    for (int i = 0; i < block_count; i++) {
        memcpy(
            (uint8_t*)ptr + BLOCK_SIZE * i,
            image_storage + BLOCK_SIZE * (logical_block_address + i),
            BLOCK_SIZE
        );
    }
}

void write_blocks(const void* ptr, uint32_t logical_block_address, uint8_t block_count) {
    for (int i = 0; i < block_count; i++) {
        memcpy(
            image_storage + BLOCK_SIZE * (logical_block_address + i),
            (uint8_t*)ptr + BLOCK_SIZE * i,
            BLOCK_SIZE
        );
    }
}

void split_by_first_inserter(char* pstr, char by, char* result) {
    int i = 0;
    while (pstr[i] != '\0' && pstr[i] != by) {
        result[i] = pstr[i];
        i++;
    }
    result[i] = '\0';

    if (pstr[i] == by) {
        i++;
        int j = 0;
        while (pstr[i] != '\0') {
            pstr[j] = pstr[i];
            i++;
            j++;
        }
        pstr[j] = '\0';
    }
    else {
        pstr[0] = '\0';
    }
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        fprintf(stderr, "inserter: ./inserter <file to insert> <parent cluster index> <storage>\n");
        exit(1);
    }

    // Read storage into memory, requiring 4 MB memory
    image_storage = malloc(4 * 1024 * 1024);
    file_buffer = malloc(4 * 1024 * 1024);
    FILE* fptr = fopen(argv[3], "r");
    fread(image_storage, 4 * 1024 * 1024, 1, fptr);
    fclose(fptr);

    // Read target file, assuming file is less than 4 MiB
    FILE* fptr_target = fopen(argv[1], "r");
    size_t filesize = 0;
    if (fptr_target == NULL)
        filesize = 0;
    else {
        fread(file_buffer, 4 * 1024 * 1024, 1, fptr_target);
        fseek(fptr_target, 0, SEEK_END);
        filesize = ftell(fptr_target);
        fclose(fptr_target);
    }

    printf("Filepath : %s\n", argv[1]);
    printf("Filesize : %ld bytes\n", filesize);

    char* full_filename = get_filename(argv[1]);
    char filename[9] = { 0 };
    char extension[4] = { 0 };

    // Split the filename and extension
    split_by_first_inserter(full_filename, '.', filename);
    split_by_first_inserter(full_filename, '.', extension);

    // Ensure filename is exactly 8 characters long
    uint8_t name_len = strlen(filename);
    while (name_len < 8) {
        filename[name_len] = '\0';
        name_len++;
    }

    // FAT32 operations
    initialize_filesystem_fat32();
    struct FAT32DriverRequest request = {
        .buf = file_buffer,
        .ext = "\0\0\0",
        .buffer_size = filesize,
    };
    sscanf(argv[2], "%u", &request.parent_cluster_number);
    memcpy(request.name, filename, 8);
    memcpy(request.ext, extension, 3);
    int retcode = write(request);
    switch (retcode) {
    case 0:  puts("Write success"); break;
    case 1:  puts("Error: File/folder name already exist"); break;
    case 2:  puts("Error: Invalid parent cluster"); break;
    default: puts("Error: Unknown error");
    }

    // Write image in memory into original, overwrite them
    fptr = fopen(argv[3], "w");
    fwrite(image_storage, 4 * 1024 * 1024, 1, fptr);
    fclose(fptr);

    return 0;
}