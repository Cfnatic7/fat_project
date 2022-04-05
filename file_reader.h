//
// Created by micha on 11.11.2021.
//

#ifndef PROJECT_FAT12_16_FILE_READER_H
#define PROJECT_FAT12_16_FILE_READER_H
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
extern int errno;
#define TRANSLATE_FAT12_INDEX_TO_CHAR_INDEX(fat12_index) (int)((fat12_index) * 1.5);
#define SECTOR_SIZE 512
typedef struct clusters_chain_t {
    uint16_t *clusters;
    size_t size;
} clusters_chain_t;
typedef struct date_t {
    int day;
    int month;
    int year;
} date_t;
typedef struct time_t {
    int second;
    int minute;
    int hour;
} time;
typedef struct dir_entry_t {
    char name[13];
    int file_starting_cluster_fat16;
    size_t size;
    int is_archived;
    int is_readonly;
    int is_system;
    int is_hidden;
    int is_directory;
    date_t creation_date;
    time creation_time;
} dir_entry_t;
typedef struct fat_super_t {
    uint8_t  jump_code[3];
    char     oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;
    uint16_t root_dir_capacity;
    uint16_t logical_sectors16;//sectors in volume
    uint8_t  media_type;
    uint16_t sectors_per_fat;
    uint16_t chs_sectors_per_track;
    uint16_t chs_tracks_per_cylinder;
    uint32_t hidden_sectors;
    uint32_t logical_sectors32;//sectors in volume
    uint8_t  media_id;
    uint8_t  chs_head;
    uint8_t  ext_bpb_signature;
    uint32_t serial_number;
    char     volume_label[11];
    char     fsid[8];
    uint8_t  boot_code[448];
    uint16_t magic;
}__attribute__((packed)) fat_super_t;


typedef struct disk_t {
    FILE *global_file;
} disk_t;

typedef struct volume_t {
    disk_t *disk;
    void *fat_table;
    uint32_t fat_table_length_in_bytes;
    uint32_t bytes_per_cluster;
    void *main_directory;
    fat_super_t super;
    unsigned int total_sectors_in_volume;
    unsigned int root_dir_sectors;
    unsigned int first_data_sector;
    unsigned int first_fat_sector;
    unsigned int number_of_data_sectors;
    unsigned int total_number_of_clusters;
    unsigned int first_root_directory_sector;
} volume_t;

typedef struct file_t {
    clusters_chain_t *fat_cluster_chain;
    dir_entry_t *dir_entry;
    void *loaded_cluster;
    uint32_t position_in_file;
    uint32_t position_in_cluster;
    uint32_t current_cluster_index;
    uint32_t bytes_per_cluster;
    disk_t *disk;
    uint32_t sections_per_cluster;
    uint32_t first_data_cluster;
    uint32_t bytes_per_sector;
    uint32_t first_fat_sector;
} file_t;
typedef struct dir_t {
    disk_t *disk;
    void *main_directory;
    uint32_t number_of_dir_entries;
    int32_t position_in_file;
    int32_t position_in_cluster;
    uint32_t current_cluster_index;
    uint32_t bytes_per_cluster;
    uint32_t sections_per_cluster;
    uint32_t read_index;
} dir_t;
struct clusters_chain_t *get_chain_fat16(const void * const buffer, size_t size, uint16_t first_cluster, file_t *stream);
void fill_clusters_data_in_single_chain_fat16(uint16_t *clusters, const uint16_t *buffer, int first_clusters_index, file_t *stream);
size_t get_chain_size_fat16(const uint16_t *buffer, int first_clusters_index, file_t *stream);
struct clusters_chain_t *get_chain_fat12(const void * const buffer, size_t size, uint16_t first_cluster);
uint16_t get_fat_entry_from_even_index_position_in_buffer(uint16_t value);
uint16_t get_fat_entry_from_odd_index_position_in_buffer(uint16_t value);
void fill_clusters_data_in_single_chain_fat12(uint16_t *clusters, const char *buffer, int first_clusters_index);
size_t get_chain_size_fat12(const char *buffer, int first_clusters_index);
struct dir_entry_t *read_directory_entry(const char *filename);
void print_directory_entry(dir_entry_t *dir);
void format_directory_entry_name(char *name);
struct disk_t* disk_open_from_file(const char* volume_file_name);
int disk_read(struct disk_t* pdisk, int32_t first_sector, void* buffer, int32_t sectors_to_read);
int disk_close(struct disk_t* pdisk);
struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector);
int fat_close(struct volume_t* pvolume);
struct file_t* file_open(struct volume_t* pvolume, const char* file_name);
int file_close(struct file_t* stream);
size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream);
int32_t file_seek(struct file_t* stream, int32_t offset, int whence);
struct dir_t* dir_open(struct volume_t* pvolume, const char* dir_path);
int dir_read(struct dir_t* pdir, struct dir_entry_t* pentry);
int dir_close(struct dir_t* pdir);
struct dir_entry_t *find_directory_entry(const void* main_directory, size_t size_of_main_directory_buffer, const char *filename, volume_t *volume);
void print_fat_table(uint16_t*table, int size);
#endif //PROJECT_FAT12_16_FILE_READER_H
