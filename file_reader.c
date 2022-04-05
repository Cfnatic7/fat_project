//
// Created by micha on 11.11.2021.
//

#include "file_reader.h"
struct clusters_chain_t *get_chain_fat16(const void * const buffer, size_t size, uint16_t first_cluster, file_t *stream) {
    if (!buffer || !size) return NULL;
    if (size & 1) return NULL;
    const uint16_t * ptr = buffer;
    clusters_chain_t *cluster_chain = malloc(sizeof(clusters_chain_t));
    if (!cluster_chain) return NULL;
    cluster_chain->size = get_chain_size_fat16(ptr, first_cluster, stream);
    if (!cluster_chain->size) {
        free(cluster_chain);
        return NULL;
    }
    cluster_chain->clusters = malloc(cluster_chain->size * sizeof(uint16_t));
    if (!cluster_chain->clusters) {
        free(cluster_chain);
        return NULL;
    }
    fill_clusters_data_in_single_chain_fat16(cluster_chain->clusters, ptr, first_cluster, stream);
    return cluster_chain;
}

size_t get_chain_size_fat16(const uint16_t *buffer, int first_clusters_index, file_t *stream) {
    size_t length = 0;
    int index = first_clusters_index;
    while(1) {
        if (index >= 0xFFF8) return length;
        index = buffer[index];
        length++;
    }
    return 0;
}
void fill_clusters_data_in_single_chain_fat16(uint16_t *clusters, const uint16_t *buffer, int first_clusters_index, file_t *stream) {
    int index = first_clusters_index;
    int i = 0;
    while(1) {
        if (index >= 0xFFF8) return;
        clusters[i] = index + stream->first_data_cluster - 2;
        index = buffer[index];
        i++;
    }
}
struct clusters_chain_t *get_chain_fat12(const void * const buffer, size_t size, uint16_t first_cluster) {
    if (!buffer || !size) return NULL;
    const char *ptr = buffer;
    int index = TRANSLATE_FAT12_INDEX_TO_CHAR_INDEX(first_cluster);
    clusters_chain_t *cluster_chain = malloc(sizeof(clusters_chain_t));
    if (!cluster_chain) return NULL;
    cluster_chain->size = get_chain_size_fat12(ptr, index);
    if (!cluster_chain->size) {
        free(cluster_chain);
        return NULL;
    }
    cluster_chain->clusters = malloc(cluster_chain->size * sizeof(uint16_t));
    if (!cluster_chain->clusters) {
        free(cluster_chain);
        return NULL;
    }
    fill_clusters_data_in_single_chain_fat12(cluster_chain->clusters, ptr, first_cluster);
    return cluster_chain;
}
uint16_t get_fat_entry_from_even_index_position_in_buffer(uint16_t value) {
    unsigned char *ptr = (unsigned char *) &value;
    uint16_t first_part = ptr[1] & 0x0F;
    first_part <<= 8;
    uint16_t second_part = ptr[0];
    second_part |= first_part;
    return second_part;
}
uint16_t get_fat_entry_from_odd_index_position_in_buffer(uint16_t value) {
    char *ptr = (char *) &value;
    uint16_t second_part = ptr[0] & 0xF0;
    second_part >>= 4;
    uint16_t first_part = ptr[1];
    first_part <<= 4;
    second_part |= first_part;
    return second_part;
}
size_t get_chain_size_fat12(const char *buffer, int first_clusters_index) {
    size_t length = 0;
    int index = first_clusters_index;
    uint16_t *ptr;
    while(1) {
        if (index >= 0x0FF8) return length;
        ptr = (uint16_t*) ((char *) buffer + index);
        if (!(index % 3)) {
            index = get_fat_entry_from_even_index_position_in_buffer(*ptr);
        }
        else {
            index = get_fat_entry_from_odd_index_position_in_buffer(*ptr);
        }
        index = TRANSLATE_FAT12_INDEX_TO_CHAR_INDEX(index);
        length++;
    }
    return 0;
}
void fill_clusters_data_in_single_chain_fat12(uint16_t *clusters, const char *buffer, int first_clusters_index) {
    int index = first_clusters_index;
    uint16_t *ptr;
    int i = 0;
    while(1) {
        clusters[i] = index;
        i++;
        index = TRANSLATE_FAT12_INDEX_TO_CHAR_INDEX(index);
        ptr = (uint16_t*) ((char *) buffer + index);
        if (!(index % 3)) {
            index = get_fat_entry_from_even_index_position_in_buffer(*ptr);
        }
        else {
            index = get_fat_entry_from_odd_index_position_in_buffer(*ptr);
        }
        if (index >= 0x0FF8) return;
    }
}
struct dir_entry_t *read_directory_entry(const char *filename) {
    static FILE *file = NULL;
    if (!filename && !file) return NULL;
    struct dir_entry_t *dir = malloc(sizeof(dir_entry_t));
    unsigned int attribute;
    unsigned short time;
    unsigned short date;
    char rozszerzenie[5];
    char control;
    rozszerzenie[4] = '\0';
    rozszerzenie[0] = '.';
    if (!dir) return NULL;
    memset((void *)dir, 0, sizeof(dir_entry_t));
    if (filename) {
        if (file != NULL) {
            fclose(file);
        }
        file = fopen(filename, "rb");
        if (!file) {
            free(dir);
            return NULL;
        }
        fread((void *)dir->name, sizeof(char), 8, file);
        while((unsigned char)dir->name[0] == 0xe5) {
            fseek(file, 24, SEEK_CUR);
            fread((void *)dir->name, sizeof(char), 8, file);
        }
        if (!dir->name[0]) {
            fclose(file);
            file = NULL;
            free(dir);
            return NULL;
        }
        dir->name[8] = '\0';
        fread( rozszerzenie + 1, sizeof(char), 3, file);
        format_directory_entry_name(dir->name);
        // 11 bajtów przeczytane
        fread((void *) &attribute, sizeof(char), 1, file);
        // 12 bajtów przeczytane
        if (attribute & 0x01) {
            dir->is_readonly = 1;
        }
        if (attribute & 0x02) {
            dir->is_hidden = 1;
        }
        if (attribute & 0x04) {
            dir->is_system = 1;
        }
        if (attribute & 0x10) {
            dir->is_directory = 1;
        }
        else {
            strcat(dir->name, rozszerzenie);
        }
        if (attribute & 0x20) {
            dir->is_archived = 1;
        }
        fseek(file, 2, SEEK_CUR);
        // 14 bajtów przeczytane
        fread((void *) &time, sizeof(unsigned short), 1, file);
        // 16 bajtów przeczytane
        dir->creation_time.second = (time & 0x001F) << 1;
        dir->creation_time.minute = (time & 0x07E0) >> 5;
        dir->creation_time.hour = (time & 0xF800) >> 11;
        fread((void *) &date, sizeof(unsigned short), 1, file);
        // 18 bajtów przeczytane
        dir->creation_date.day = date & 0x001F;
        dir->creation_date.month = (date & 0x01E0) >> 5;
        dir->creation_date.year = ((date & 0xFE00) >> 9) + 1980;
        fseek(file, 10, SEEK_CUR);
        fread((void *) &dir->size, 1, 4, file);
        return dir;
    }
    else {
        fread((void *)dir->name, sizeof(char), 8, file);
        while((unsigned char)dir->name[0] == 0xe5) {
            fseek(file, 24, SEEK_CUR);
            fread((void *)dir->name, sizeof(char), 8, file);
        }
        if (!dir->name[0]) {
            fclose(file);
            file = NULL;
            free(dir);
            return NULL;
        }
        dir->name[8] = '\0';
        fread(rozszerzenie + 1, sizeof(char), 3, file);
        format_directory_entry_name(dir->name);
        // 11 bajtów przeczytane
        fread((void *) &attribute, sizeof(char), 1, file);
        // 12 bajtów przeczytane
        if (attribute & 0x01) {
            dir->is_readonly = 1;
        }
        if (attribute & 0x02) {
            dir->is_hidden = 1;
        }
        if (attribute & 0x04) {
            dir->is_system = 1;
        }
        if (attribute & 0x10) {
            dir->is_directory = 1;
        }
        else {
            strcat(dir->name, rozszerzenie);
        }
        if (attribute & 0x20) {
            dir->is_archived = 1;
        }
        fseek(file, 2, SEEK_CUR);
        // 14 bajtów przeczytane
        fread((void *) &time, sizeof(unsigned short), 1, file);
        // 16 bajtów przeczytane
        dir->creation_time.second = (time & 0x001F) << 1;
        dir->creation_time.minute = (time & 0x07E0) >> 5;
        dir->creation_time.hour = (time & 0xF800) >> 11;
        fread((void *) &date, sizeof(unsigned short), 1, file);
        // 18 bajtów przeczytane
        dir->creation_date.day = date & 0x001F;
        dir->creation_date.month = (date & 0x01E0) >> 5;
        dir->creation_date.year = ((date & 0xFE00) >> 9) + 1980;
        fseek(file, 10, SEEK_CUR);
        fread((void *) &dir->size, 1, 4, file);
        fread(&control, 1, 1, file);
        if (control == EOF) {
            fclose(file);
            file = NULL;
        }
        else fseek(file, -1, SEEK_CUR);
        return dir;
    }
}
void print_directory_entry(dir_entry_t *dir) {
    if (!dir) return;
    if (dir->is_directory) {
        printf("%02d/%02d/%02d %02d:%02d   *DIRECTORY*  %s\n", dir->creation_date.day, dir->creation_date.month, dir->creation_date.year, dir->creation_time.hour, dir->creation_time.minute, dir->name);
        fflush(stdout);
    }
    else {
        printf("%02d/%02d/%02d %02d:%02d   %11zu  %s\n", dir->creation_date.day, dir->creation_date.month, dir->creation_date.year, dir->creation_time.hour, dir->creation_time.minute, dir->size, dir->name);
        fflush(stdout);
    }

}
void format_directory_entry_name(char *name) {
    if (!name) return;
    int i = 0;
    for (; isalpha(name[i]); i++);
    name[i] = '\0';
}


//project section
struct disk_t* disk_open_from_file(const char* volume_file_name) {
    if (!volume_file_name) {
        errno = EFAULT;
        return NULL;
    }
    FILE *fp = fopen(volume_file_name, "rb");
    if (!fp) {
        errno = ENOENT;
        return NULL;
    }
    disk_t *disk = malloc(sizeof(disk_t));
    if (!disk) {
        fclose(fp);
        errno = ENOMEM;
        return NULL;
    }
    disk->global_file = fp;
    return disk;
}
int disk_read(struct disk_t* pdisk, int32_t first_sector, void* buffer, int32_t sectors_to_read) {
    if (!pdisk || !buffer) {
        errno = EFAULT;
        return -1;
    }
    char *ptr = (char *) buffer;
    fseek(pdisk->global_file, first_sector * SECTOR_SIZE, SEEK_SET);
    int check = 0, num_of_read_sectors = 0;
    for (int i = 0; i < (int)sectors_to_read; i++) {
        check = fread(ptr, SECTOR_SIZE, 1, pdisk->global_file);
        if (errno == ERANGE) {
            return -1;
        }
        if (check) num_of_read_sectors++;
        ptr = ptr + SECTOR_SIZE;
    }
    if (num_of_read_sectors != sectors_to_read) {
        errno = ERANGE;
        return -1;
    } // idk if this if statement is necessary
    fseek(pdisk->global_file, 0, SEEK_SET);
    return sectors_to_read;
}
int disk_close(struct disk_t* pdisk) {
    if (!pdisk) {
        errno = EFAULT;
        return -1;
    }
    fclose(pdisk->global_file);
    free(pdisk);
    return 0;
}
struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector) {
    if (!pdisk) {
        errno = EFAULT;
        return NULL;
    }
//    if (pdisk->super.magic != 0x55AA) {
//        errno = EINVAL;
//        return NULL;
//    }
    int check;
    volume_t *volume = malloc(sizeof(volume_t));
    if (!volume) {
        errno = ENOMEM;
        return NULL;
    }
    check = disk_read(pdisk, first_sector, &volume->super, 1);
    if (!check) {
        free(volume);
        return NULL;
    }
    if (!volume->super.bytes_per_sector || !volume->super.sectors_per_fat || !volume->super.reserved_sectors || !volume->super.fat_count || !volume->super.root_dir_capacity || (!volume->super.logical_sectors16 && !volume->super.logical_sectors32) || !volume->super.sectors_per_fat || volume->super.magic != 43605) {
        free(volume);
        errno = EINVAL;
        return NULL;
    }
    volume->total_sectors_in_volume = (volume->super.logical_sectors16 == 0)? volume->super.logical_sectors32 : volume->super.logical_sectors16;
    volume->root_dir_sectors = ((volume->super.root_dir_capacity * 32) + (volume->super.bytes_per_sector - 1)) / volume->super.bytes_per_sector;
    volume->first_data_sector = volume->super.reserved_sectors + (volume->super.fat_count * volume->super.sectors_per_fat) + volume->root_dir_sectors;
    volume->first_fat_sector = volume->super.reserved_sectors;
    volume->number_of_data_sectors = volume->total_sectors_in_volume - (volume->super.reserved_sectors + (volume->super.fat_count * volume->super.sectors_per_fat) + volume->root_dir_sectors);
    volume->first_root_directory_sector = volume->super.reserved_sectors + (volume->super.fat_count * volume->super.sectors_per_fat);
    volume->fat_table_length_in_bytes = volume->super.sectors_per_fat * volume->super.bytes_per_sector;
    volume->bytes_per_cluster = volume->super.bytes_per_sector * volume->super.sectors_per_cluster;
    volume->disk = pdisk;
    //allocating memory for all the fats
    uint32_t **table_of_fats = malloc(sizeof(void *) * volume->super.fat_count);
    if (!table_of_fats) {
        errno = ENOMEM;
        return NULL;
    }
    for (int i = 0; i < volume->super.fat_count; i++) {
        table_of_fats[i] = malloc(volume->super.sectors_per_fat * volume->super.bytes_per_sector);
        if (!table_of_fats[i]) {
            for (int j = 0; j < i; j++) {
                free(table_of_fats[j]);
            }
            errno = ENOMEM;
            free(table_of_fats);
            free(volume);
            return NULL;
        }
    }
    //checking fats
    for (int i = 0; i < volume->super.fat_count; i++) {
        disk_read(pdisk, volume->first_fat_sector + (volume->super.sectors_per_fat * i), table_of_fats[i], volume->super.sectors_per_fat);
//        for (int j = 0; j < ((volume->super.sectors_per_fat * volume->super.bytes_per_sector) >> 1); j++) {
//            if (/*((uint16_t *) table_of_fats[i])[j] >= 0xFFF8 || */((uint16_t *) table_of_fats[i])[j] == 0) continue;
//            ((uint16_t *) table_of_fats[i])[j] += (volume->first_data_sector / volume->super.sectors_per_cluster) + 2;
//        }
//        print_fat_table((uint16_t *)table_of_fats[i], (volume->super.sectors_per_fat * volume->super.bytes_per_sector) >> 1);
    }
    for (int i = 0; i < volume->super.fat_count; i++) {
        for (int j = 0; j < volume->super.fat_count; j++) {
            if (i == j) continue;
            check = strncmp((char *) table_of_fats[i], (char *) table_of_fats[j], volume->super.sectors_per_fat * volume->super.bytes_per_sector);
            if (check) {
                for (int k = 0; k < volume->super.fat_count; k++) {
                    free(table_of_fats[k]);
                }
                free(table_of_fats);
                free(volume);
                errno = EINVAL;
                return NULL;
            }
        }
    }
    for (int i = 1; i < volume->super.fat_count; i++) {
        free(table_of_fats[i]);
    }
    volume->fat_table = table_of_fats[0];
    free(table_of_fats);
    volume->main_directory = malloc(volume->root_dir_sectors * volume->super.bytes_per_sector);
    if (!volume->main_directory) {
        free(volume->fat_table);
        free(volume);
        errno = ENOMEM;
        return NULL;
    }
    disk_read(pdisk, volume->first_root_directory_sector, volume->main_directory, volume->root_dir_sectors);
    return volume;
}
int fat_close(struct volume_t* pvolume) {
    if (!pvolume) {
        errno = EFAULT;
        return -1;
    }
    free(pvolume->fat_table);
    free(pvolume->main_directory);
    free(pvolume);
    return 0;
}
struct dir_entry_t *find_directory_entry(const void* main_directory, size_t size_of_main_directory_buffer, const char *filename, volume_t *volume) {
    struct dir_entry_t *dir_entry = malloc(sizeof(dir_entry_t));
    if (!dir_entry) {
        errno = ENOMEM;
        return NULL;
    }
    const char* main_directory_ptr = main_directory;
    uint8_t attribute;
    uint8_t is_directory;
    const uint16_t *probe1;
    const uint32_t *probe2;
    size_t used_size = 0;
    char file_name_from_entry[13] = {0};
    char extension[5] = {0};
    extension[0] = '.';
    while(1) {
        if (used_size >= size_of_main_directory_buffer) {
            errno = ENOENT;
            free(dir_entry);
            return NULL;
        }
        attribute = main_directory_ptr[0x0B];
        is_directory = attribute & 0x10;
        for (int i = 0; i < 8; i++) {
            if (!isalpha(main_directory_ptr[i])) break;
            file_name_from_entry[i] = main_directory_ptr[i];
        }
        if (!is_directory) {
            for (int i = 0; i < 3; i++) {
                if (!isalpha(main_directory_ptr[i + 8])) break;
                extension[i + 1] = main_directory_ptr[i + 8];
            }
            if (extension[1] != ' ' && extension[2] != 0) {
                strcat(file_name_from_entry, extension);
            }
        }
        if (!strcmp(file_name_from_entry, filename)) {
            if (is_directory) {
                dir_entry->is_directory = 1;
            }
            break;
        }
        main_directory_ptr += 32;
        used_size += 32;
        memset(file_name_from_entry, 0, 13);
        memset(extension + 1, 0, 4);
    }
    strcpy(dir_entry->name, filename);
    attribute = main_directory_ptr[11];
    // 12 bajtów przeczytane
    if (attribute & 0x01) {
        dir_entry->is_readonly = 1;
    }
    else {
        dir_entry->is_readonly = 0;
    }
    if (attribute & 0x02) {
        dir_entry->is_hidden = 1;
    }
    else {
        dir_entry->is_hidden = 0;
    }
    if (attribute & 0x04) {
        dir_entry->is_system = 1;
    }
    else {
        dir_entry->is_system = 0;
    }
    if (attribute & 0x10) {
        dir_entry->is_directory = 1;
    }
    else {
        dir_entry->is_directory = 0;
    }
    if (attribute & 0x20) {
        dir_entry->is_archived = 1;
    }
    else {
        dir_entry->is_archived = 0;
    }
    probe1 = (uint16_t *)((char *)main_directory_ptr + 0x0E);
    dir_entry->creation_time.second = (*probe1 & 0x001F) << 1;
    dir_entry->creation_time.minute = (*probe1 & 0x07E0) >> 5;
    dir_entry->creation_time.hour = (*probe1 & 0xF800) >> 11;
    probe1 = (uint16_t *)((char *)main_directory_ptr + 0x10);
    dir_entry->creation_date.day = *probe1 & 0x001F;
    dir_entry->creation_date.month = (*probe1 & 0x01E0) >> 5;
    dir_entry->creation_date.year = ((*probe1 & 0xFE00) >> 9) + 1980;
    probe1 = (uint16_t *)((char *)main_directory_ptr + 0x1A);
    dir_entry->file_starting_cluster_fat16 = *probe1;
    probe2 = (uint32_t *)((char *)main_directory_ptr + 0x1C);
    dir_entry->size = *probe2;
    return dir_entry;
}
struct file_t* file_open(struct volume_t* pvolume, const char* file_name) {
    if (!pvolume || !file_name) {
        errno = EFAULT;
        return NULL;
    }
    file_t *file = malloc(sizeof(file_t));
    if (!file) {
        errno = ENOMEM;
        return NULL;
    }
    file->position_in_file = 0;
    file->current_cluster_index = 0;
    file->bytes_per_cluster = pvolume->bytes_per_cluster;
    file->disk = pvolume->disk;
    file->sections_per_cluster = pvolume->super.sectors_per_cluster;
    file->position_in_cluster = 0;
    file->bytes_per_sector = pvolume->super.bytes_per_sector;
    file->first_fat_sector = pvolume->first_fat_sector;
    file->first_data_cluster = pvolume->first_data_sector / pvolume->super.sectors_per_cluster;
    file->dir_entry = find_directory_entry(pvolume->main_directory, pvolume->root_dir_sectors * pvolume->super.bytes_per_sector, file_name, pvolume);
    if (!file->dir_entry) {
        free(file);
        return NULL;
    }
    if (file->dir_entry->is_directory) {
        free(file->dir_entry);
        free(file);
        return NULL;
    }
    file->fat_cluster_chain = get_chain_fat16(pvolume->fat_table, pvolume->fat_table_length_in_bytes, file->dir_entry->file_starting_cluster_fat16, file);
    if (!file->fat_cluster_chain) {
        errno = ENOMEM;
        free(file->dir_entry);
        free(file);
        return NULL;
    }
    file->loaded_cluster = malloc(pvolume->bytes_per_cluster);
    if (!file->loaded_cluster) {
        errno = ENOMEM;
        free(file->fat_cluster_chain);
        free(file->dir_entry);
        free(file);
        return NULL;
    }
    disk_read(pvolume->disk, (int32_t)file->fat_cluster_chain->clusters[0] * pvolume->super.sectors_per_cluster, file->loaded_cluster, pvolume->super.sectors_per_cluster);
    return file;
}
int file_close(struct file_t* stream) {
    if (!stream) {
        errno = EFAULT;
        return -1;
    }
    free(stream->fat_cluster_chain->clusters);
    free(stream->fat_cluster_chain);
    free(stream->loaded_cluster);
    free(stream->dir_entry);
    free(stream);
    return 0;
}
int32_t file_seek(struct file_t* stream, int32_t offset, int whence) {
    if (!stream) {
        errno = EFAULT;
        return -1;
    }
    if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) {
        errno = EINVAL;
        return -1;
    }
    uint32_t new_cluster_index;
    if (whence == SEEK_SET) {
        if (offset >= (int32_t)stream->dir_entry->size || offset < 0) {
            errno = ENXIO;
            return -1;
        }
        new_cluster_index = offset / stream->bytes_per_cluster;
        if (new_cluster_index != stream->current_cluster_index) {
            stream->current_cluster_index = new_cluster_index;
            disk_read(stream->disk, (stream->fat_cluster_chain->clusters[stream->current_cluster_index]) * stream->sections_per_cluster, stream->loaded_cluster, stream->sections_per_cluster);
        }
        stream->position_in_file = offset;
    }
    else if (whence == SEEK_CUR) {
        if (offset + stream->position_in_file >= stream->dir_entry->size || (int32_t)((int32_t)offset + (int32_t)stream->position_in_file) < 0) {
            errno = ENXIO;
            return -1;
        }
        new_cluster_index = (offset + stream->position_in_file) / stream->bytes_per_cluster;
        if (new_cluster_index != stream->current_cluster_index) {
            stream->current_cluster_index = new_cluster_index;
            disk_read(stream->disk, stream->fat_cluster_chain->clusters[stream->current_cluster_index] * stream->sections_per_cluster, stream->loaded_cluster, stream->sections_per_cluster);
        }
        stream->position_in_file += offset;
    }
    else {
        int32_t new_position = (int32_t)stream->dir_entry->size;
        new_position += offset;
        if (new_position >= (int32_t)stream->dir_entry->size || new_position < 0) {
            errno = ENXIO;
            return -1;
        }
        new_cluster_index = new_position / stream->bytes_per_cluster;
        if (new_cluster_index != stream->current_cluster_index) {
            stream->current_cluster_index = new_cluster_index;
            disk_read(stream->disk, stream->fat_cluster_chain->clusters[stream->current_cluster_index] * stream->sections_per_cluster, stream->loaded_cluster, stream->sections_per_cluster);
        }
        stream->position_in_file = new_position;
    }
    stream->position_in_cluster = stream->position_in_file % stream->bytes_per_cluster;
    return stream->position_in_file;
}
size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream) {
    if (!ptr || !stream) {
        errno = EFAULT;
        return -1;
    }
    char *buffer = (char *) ptr;
    uint32_t i = 0;
    for (;stream->position_in_file < stream->dir_entry->size && i < size * nmemb; stream->position_in_file++, i++) {
        if (!(stream->position_in_cluster % stream->bytes_per_cluster) && stream->position_in_file < stream->dir_entry->size && stream->position_in_cluster) {
            stream->current_cluster_index++;
            stream->position_in_cluster = 0;
            disk_read(stream->disk, (stream->fat_cluster_chain->clusters[stream->current_cluster_index]) * stream->sections_per_cluster, stream->loaded_cluster, stream->sections_per_cluster);
            if (errno == ERANGE) {
                return -1;
            }
        }
        if (stream->position_in_file == stream->dir_entry->size) {
            return i / size;
        }
        buffer[i] = ( (char *)stream->loaded_cluster)[stream->position_in_cluster++];
    }
    return i / size;
}
struct dir_t* dir_open(struct volume_t* pvolume, const char* dir_path) {
    if (!pvolume) {
        errno = EFAULT;
        return NULL;
    }
    if (!dir_path) {
        errno = ENOENT;
        return NULL;
    }
    int dir_path_length = strlen(dir_path);
    dir_t *dir = malloc(sizeof(dir_t));
    if (!dir) {
        errno = ENOMEM;
        return NULL;
    }
    if (dir_path_length == 1 && dir_path[0] == '\\') {
        dir->disk = pvolume->disk;
        dir->number_of_dir_entries = pvolume->super.root_dir_capacity;
        dir->position_in_file = 0;
        dir->position_in_cluster = 0;
        dir->current_cluster_index = 0;
        dir->bytes_per_cluster = pvolume->bytes_per_cluster;
        dir->sections_per_cluster = pvolume->super.sectors_per_cluster;
        dir->main_directory = pvolume->main_directory;
        dir->read_index = 0;
        return dir;
    }
    free(dir);
    return NULL;
}
int dir_close(struct dir_t* pdir) {
    if (!pdir) {
        errno = EFAULT;
        return -1;
    }
    free(pdir);
    return 0;
}
int dir_read(struct dir_t* pdir, struct dir_entry_t* pentry) {
    if (!pdir || !pentry) {
        errno = EFAULT;
        return -1;
    }
//    if (pdir->read_index == pdir->number_of_dir_entries) {
//        pdir->read_index = 0;
//        return 1;
//    }
    if (pdir->read_index > pdir->number_of_dir_entries) {
        errno = EIO;
        return -1;
    }
    const char* main_directory_ptr = (char *)pdir->main_directory + 32 * pdir->read_index++;
    if (!main_directory_ptr[0]) {
        pdir->read_index = 0;
        return 1;
    }
    while (!isalpha(main_directory_ptr[0])) {
        main_directory_ptr += 32;
        pdir->read_index++;
    }
    uint8_t attribute;
    uint8_t is_directory;
    const uint16_t *probe1;
    const uint32_t *probe2;
    char file_name_from_entry[13] = {0};
    char extension[5] = {0};
    extension[0] = '.';
    attribute = main_directory_ptr[0x0B];
    is_directory = attribute & 0x10;
    for (int i = 0; i < 8; i++) {
        if (!isalpha(main_directory_ptr[i])) break;
        file_name_from_entry[i] = main_directory_ptr[i];
    }
    if (!is_directory) {
        for (int i = 0; i < 3; i++) {
            if (!isalpha(main_directory_ptr[i + 8])) break;
            extension[i + 1] = main_directory_ptr[i + 8];
        }
        if (extension[1] != ' ' && extension[2] != 0) {
            strcat(file_name_from_entry, extension);
        }
    }
    if (is_directory) {
        pentry->is_directory = 1;
    }
    strcpy(pentry->name, file_name_from_entry);
    attribute = main_directory_ptr[11];
    // 12 bajtów przeczytane
    if (attribute & 0x01) {
        pentry->is_readonly = 1;
    }
    else {
        pentry->is_readonly = 0;
    }
    if (attribute & 0x02) {
        pentry->is_hidden = 1;
    }
    else {
        pentry->is_hidden = 0;
    }
    if (attribute & 0x04) {
        pentry->is_system = 1;
    }
    else {
        pentry->is_system = 0;
    }
    if (attribute & 0x10) {
        pentry->is_directory = 1;
    }
    else {
        pentry->is_directory = 0;
    }
    if (attribute & 0x20) {
        pentry->is_archived = 1;
    }
    else {
        pentry->is_archived = 0;
    }
    probe1 = (uint16_t *)((char *)main_directory_ptr + 0x0E);
    pentry->creation_time.second = (*probe1 & 0x001F) << 1;
    pentry->creation_time.minute = (*probe1 & 0x07E0) >> 5;
    pentry->creation_time.hour = (*probe1 & 0xF800) >> 11;
    probe1 = (uint16_t *)((char *)main_directory_ptr + 0x10);
    pentry->creation_date.day = *probe1 & 0x001F;
    pentry->creation_date.month = (*probe1 & 0x01E0) >> 5;
    pentry->creation_date.year = ((*probe1 & 0xFE00) >> 9) + 1980;
    probe1 = (uint16_t *)((char *)main_directory_ptr + 0x1A);
    pentry->file_starting_cluster_fat16 = *probe1;
    probe2 = (uint32_t *)((char *)main_directory_ptr + 0x1C);
    pentry->size = *probe2;
    return 0;
}
void print_fat_table(uint16_t*table, int size) {
    for (int i = 0 ; i < size; i++) {
        if (!(i % 16)) {
            printf("\n");
            fflush(stdout);
        }
        printf("%7x ", table[i]);
        fflush(stdout);
    }
    printf("\n\n\n");
    fflush(stdout);
}

