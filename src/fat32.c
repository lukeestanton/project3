// fat32.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "fat32.h"

FAT32 fs;

int fat32_mount(const char *image_path)
{
    fs.fp = fopen(image_path, "r+b");
    if(fs.fp == NULL) {
        fprintf(stderr, "Error: %s does not exist\n", image_path);
        return -1;
    }

    if(fread(&fs.bs, sizeof(BootSector), 1, fs.fp) != 1){
        fprintf(stderr, "Error: Failed to read boot sector\n");
        fclose(fs.fp);
        return -1;
    }

    fs.fat_start = fs.bs.BPB_RsvdSecCnt * fs.bs.BPB_BytsPerSec;
    
    fs.data_start = fs.fat_start + 
                    (fs.bs.BPB_NumFATs * fs.bs.BPB_FATSz32 * fs.bs.BPB_BytsPerSec);

    uint32_t totSectors = (fs.bs.BPB_TotSec16 != 0) ? 
                           fs.bs.BPB_TotSec16 : fs.bs.BPB_TotSec32;
    uint32_t dataSectors = totSectors - 
                           (fs.bs.BPB_RsvdSecCnt + 
                            fs.bs.BPB_NumFATs * fs.bs.BPB_FATSz32);
    fs.total_clusters = dataSectors / fs.bs.BPB_SecPerClus;

    fs.current_dir = fs.bs.BPB_RootClus;
    strcpy(fs.current_path, "/");

    const char *name = strrchr(image_path, '/');
    if(name != NULL)
        name++;
    else
        name = image_path;
    strncpy(fs.image_name, name, MAX_PATH - 1);
    fs.image_name[MAX_PATH - 1] = '\0';

    char *ext = strstr(fs.image_name, ".img");
    if(ext != NULL)
        *ext = '\0';

    for(int i=0; i<MAX_OPEN_FILES; i++)
        fs.open_files[i].in_use = false;

    return 0;
}

void fat32_unmount(void)
{
    if (fs.fp != NULL) {
        fclose(fs.fp);
        fs.fp = NULL;
    }
}

uint32_t fat32_get_fat_entry(uint32_t cluster)
{
    uint32_t fatOffset = fs.fat_start + (cluster * 4);
    uint32_t entry;

    fseek(fs.fp, fatOffset, SEEK_SET);
    fread(&entry, 4, 1, fs.fp);

    return(entry & FAT_MASK);
}

int fat32_set_fat_entry(uint32_t cluster, uint32_t value)
{
    uint32_t fat_offset = fs.fat_start + (cluster * 4);

    for(int i = 0; i < fs.bs.BPB_NumFATs; i++){
        uint32_t offset = fat_offset + (i * fs.bs.BPB_FATSz32 * fs.bs.BPB_BytsPerSec);
        fseek(fs.fp, offset, SEEK_SET);
        if(fwrite(&value, 4, 1, fs.fp) != 1)
            return -1;
    }

    fflush(fs.fp);
    return 0;
}

uint32_t fat32_find_free_cluster(void)
{
    for(uint32_t i = 2; i < fs.total_clusters + 2; i++)
    {
        if(fat32_get_fat_entry(i) == FAT_FREE)
            return i;
    }
    return 0; // none are free
}

uint32_t fat32_allocate_cluster(uint32_t prev_cluster)
{
    uint32_t newCluster = fat32_find_free_cluster();
    if(newCluster == 0) return 0;

    fat32_set_fat_entry(newCluster, FAT_EOC);

    if(prev_cluster != 0) fat32_set_fat_entry(prev_cluster, newCluster);

    uint32_t clus_size = fat32_get_cluster_size();
    uint8_t *buffer = calloc(1, clus_size);
    if(buffer != NULL) {
        fat32_write_cluster(newCluster, buffer);
        free(buffer);
    }

    return newCluster;
}

uint32_t fat32_cluster_to_offset(uint32_t cluster)
{
    return fs.data_start + ((cluster - 2) * fs.bs.BPB_SecPerClus * fs.bs.BPB_BytsPerSec);
}

uint32_t fat32_get_cluster_size(void)
{
    return fs.bs.BPB_SecPerClus * fs.bs.BPB_BytsPerSec;
}

uint32_t fat32_get_image_size(void)
{
    uint32_t tot_sectors = (fs.bs.BPB_TotSec16 != 0) ? 
                            fs.bs.BPB_TotSec16 : fs.bs.BPB_TotSec32;
    return tot_sectors * fs.bs.BPB_BytsPerSec;
}

int fat32_read_cluster(uint32_t cluster, void *buffer)
{
    uint32_t off = fat32_cluster_to_offset(cluster);
    uint32_t sz = fat32_get_cluster_size();

    fseek(fs.fp, off, SEEK_SET);
    if(fread(buffer, 1, sz, fs.fp) != sz)
        return -1;

    return 0;
}

int fat32_write_cluster(uint32_t cluster, const void *buffer)
{
    uint32_t offset = fat32_cluster_to_offset(cluster);
    uint32_t size = fat32_get_cluster_size();

    fseek(fs.fp, offset, SEEK_SET);
    if (fwrite(buffer, 1, size, fs.fp) != size)
        return -1;

    fflush(fs.fp);
    return 0;
}

int fat32_read_dir_entry(uint32_t cluster, uint32_t offset, DirEntry *entry)
{
    uint32_t byteOffset = fat32_cluster_to_offset(cluster) + offset;

    fseek(fs.fp, byteOffset, SEEK_SET);
    if(fread(entry, sizeof(DirEntry), 1, fs.fp) != 1)
        return -1;

    return 0;
}

int fat32_write_dir_entry(uint32_t cluster, uint32_t offset, DirEntry* entry)
{
    uint32_t byte_offset = fat32_cluster_to_offset(cluster) + offset;

    fseek(fs.fp, byte_offset, SEEK_SET);
    if(fwrite(entry, sizeof(DirEntry), 1, fs.fp) != 1)
        return -1;

    fflush(fs.fp);
    return 0;
}

void fat32_name_to_83(const char *name, char *name83)
{
    memset(name83, ' ', 11);

    if(strcmp(name, ".") == 0){
        name83[0] = '.';
        return;
    }
    if(strcmp(name, "..") == 0) {
        name83[0] = '.';
        name83[1] = '.';
        return;
    }

    int i = 0, j = 0;
    while(name[i] != '\0' && j < 11)
    {
        if(name[i] == '.') {
            j = 8; // to extension
            i++;
            continue;
        }
        name83[j++] = toupper((unsigned char)name[i++]);
    }
}

void fat32_83_to_name(const uint8_t *name83, char *name)
{
    int i, j=0;

    for(i = 0; i < 8 && name83[i] != ' '; i++) name[j++] = name83[i];

    if(name83[8] != ' ')
    {
        name[j++] = '.';
        for(i=8; i<11 && name83[i] != ' '; i++)
            name[j++] = name83[i];
    }
    name[j] = '\0';
}

uint32_t fat32_get_cluster(DirEntry *entry)
{
    return ((uint32_t)entry->DIR_FstClusHI << 16) | entry->DIR_FstClusLO;
}

void fat32_set_cluster(DirEntry *entry, uint32_t cluster)
{
    entry->DIR_FstClusHI = (cluster >> 16) & 0xFFFF;
    entry->DIR_FstClusLO = cluster & 0xFFFF;
}

int fat32_find_entry(uint32_t dir_cluster, const char *name, DirEntry *entry, uint32_t *entry_cluster, uint32_t *entry_offset)
{
    char name83[11];
    fat32_name_to_83(name, name83);

    uint32_t clus = dir_cluster;
    uint32_t clusSize = fat32_get_cluster_size();
    uint32_t entriesPerCluster = clusSize / sizeof(DirEntry);

    while(clus < FAT_EOC && clus != 0)
    {
        for(uint32_t i = 0; i < entriesPerCluster; i++)
        {
            DirEntry tmp;
            uint32_t off = i * sizeof(DirEntry);

            if(fat32_read_dir_entry(clus, off, &tmp) != 0)
                return -1;

            if(tmp.DIR_Name[0] == 0x00)
                return -1;

            if(tmp.DIR_Name[0] == 0xE5 || 
               (tmp.DIR_Attr & ATTR_LONG_NAME) == ATTR_LONG_NAME)
                continue;

            if(memcmp(tmp.DIR_Name, name83, 11) == 0)
            {
                if(entry != NULL) *entry = tmp;
                if(entry_cluster != NULL) *entry_cluster = clus;
                if(entry_offset != NULL) *entry_offset = off;
                return 0;
            }
        }

        clus = fat32_get_fat_entry(clus);
    }

    return -1;
}

int fat32_add_dir_entry(uint32_t dir_cluster, DirEntry *entry)
{
    uint32_t cluster = dir_cluster;
    uint32_t cluster_size = fat32_get_cluster_size();
    uint32_t entries_per_cluster = cluster_size / sizeof(DirEntry);
    uint32_t prevCluster = 0;

    while(cluster < FAT_EOC && cluster != 0)
    {
        for(uint32_t i = 0; i < entries_per_cluster; ++i)
        {
            DirEntry temp;
            uint32_t offset = i * sizeof(DirEntry);

            if(fat32_read_dir_entry(cluster, offset, &temp) != 0) return -1;
            if(temp.DIR_Name[0] == 0x00 || temp.DIR_Name[0] == 0xE5)
                return fat32_write_dir_entry(cluster, offset, entry);
        }

        prevCluster = cluster;
        cluster = fat32_get_fat_entry(cluster);
    }

    uint32_t newClus = fat32_allocate_cluster(prevCluster);
    if(newClus == 0) return -1;
    return fat32_write_dir_entry(newClus, 0, entry);
}

int fat32_remove_dir_entry(uint32_t cluster, uint32_t offset)
{
    DirEntry entry;
    
    if (fat32_read_dir_entry(cluster, offset, &entry) != 0)
        return -1;

    entry.DIR_Name[0] = 0xE5; // deleted marker
    
    return fat32_write_dir_entry(cluster, offset, &entry);
}

bool fat32_is_dir_empty(uint32_t cluster)
{
    uint32_t curr = cluster;
    uint32_t clusterSize = fat32_get_cluster_size();
    uint32_t entriesPerClus = clusterSize / sizeof(DirEntry);

    while(curr < FAT_EOC && curr != 0)
    {
        for(uint32_t i = 0; i < entriesPerClus; i++)
        {
            DirEntry ent;
            uint32_t off = i * sizeof(DirEntry);

            if(fat32_read_dir_entry(curr, off, &ent) != 0) return true;
            if(ent.DIR_Name[0] == 0x00) return true;
            if(ent.DIR_Name[0] == 0xE5) continue;
            if((ent.DIR_Attr & ATTR_LONG_NAME) == ATTR_LONG_NAME) continue;

            char name[13];
            fat32_83_to_name(ent.DIR_Name, name);
            
            if(strcmp(name, ".") != 0 && strcmp(name, "..") != 0)
                return false;
        }
        curr = fat32_get_fat_entry(curr);
    }

    return true;
}
