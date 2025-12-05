#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

// Boot sector / BPB structure
typedef struct __attribute__((packed)) {
    uint8_t  BS_jmpBoot[3];
    uint8_t  BS_OEMName[8];
    uint16_t BPB_BytsPerSec;
    uint8_t  BPB_SecPerClus;
    uint16_t BPB_RsvdSecCnt;
    uint8_t  BPB_NumFATs;
    uint16_t BPB_RootEntCnt;     // 0 for FAT32
    uint16_t BPB_TotSec16;
    uint8_t  BPB_Media;
    uint16_t BPB_FATSz16;        // 0 for FAT32
    uint16_t BPB_SecPerTrk;
    uint16_t BPB_NumHeads;
    uint32_t BPB_HiddSec;
    uint32_t BPB_TotSec32;
    // FAT32 specific
    uint32_t BPB_FATSz32;
    uint16_t BPB_ExtFlags;
    uint16_t BPB_FSVer;
    uint32_t BPB_RootClus;
    uint16_t BPB_FSInfo;
    uint16_t BPB_BkBootSec;
    uint8_t  BPB_Reserved[12];
    uint8_t  BS_DrvNum;
    uint8_t  BS_Reserved1;
    uint8_t  BS_BootSig;
    uint32_t BS_VolID;
    uint8_t  BS_VolLab[11];
    uint8_t  BS_FilSysType[8];
} BootSector;

// Directory entry (32 bytes)
typedef struct __attribute__((packed)) {
    uint8_t  DIR_Name[11];       // 8.3 name
    uint8_t  DIR_Attr;
    uint8_t  DIR_NTRes;
    uint8_t  DIR_CrtTimeTenth;
    uint16_t DIR_CrtTime;
    uint16_t DIR_CrtDate;
    uint16_t DIR_LstAccDate;
    uint16_t DIR_FstClusHI;
    uint16_t DIR_WrtTime;
    uint16_t DIR_WrtDate;
    uint16_t DIR_FstClusLO;
    uint32_t DIR_FileSize;
} DirEntry;

// attributes
#define ATTR_READ_ONLY  0x01
#define ATTR_HIDDEN     0x02
#define ATTR_SYSTEM     0x04
#define ATTR_VOLUME_ID  0x08
#define ATTR_DIRECTORY  0x10
#define ATTR_ARCHIVE    0x20
#define ATTR_LONG_NAME  (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)

// FAT special values
#define FAT_EOC         0x0FFFFFF8
#define FAT_FREE        0x00000000
#define FAT_BAD         0x0FFFFFF7
#define FAT_MASK        0x0FFFFFFF

#define MAX_OPEN_FILES  10

#define MODE_READ       0x01
#define MODE_WRITE      0x02
#define MODE_RW         (MODE_READ | MODE_WRITE)

#define MAX_PATH        256

typedef struct {
    char name[12];
    char path[MAX_PATH];
    uint32_t first_cluster;
    uint32_t size;
    uint32_t offset;
    uint8_t mode;
    bool in_use;
    uint32_t dir_cluster;
    uint32_t dir_entry_offset;
} OpenFile;

typedef struct {
    FILE *fp;
    BootSector bs;
    uint32_t fat_start;
    uint32_t data_start;
    uint32_t total_clusters;
    uint32_t current_dir;
    char current_path[MAX_PATH];
    char image_name[MAX_PATH];
    OpenFile open_files[MAX_OPEN_FILES];
} FAT32;

extern FAT32 fs;

// mount/unmount
int fat32_mount(const char *image_path);
void fat32_unmount(void);

// FAT operations
uint32_t fat32_get_fat_entry(uint32_t cluster);
int fat32_set_fat_entry(uint32_t cluster, uint32_t value);
uint32_t fat32_find_free_cluster(void);
uint32_t fat32_allocate_cluster(uint32_t prev_cluster);

// cluster ops
uint32_t fat32_cluster_to_offset(uint32_t cluster);
int fat32_read_cluster(uint32_t cluster, void *buffer);
int fat32_write_cluster(uint32_t cluster, const void *buffer);

// directory stuff
int fat32_read_dir_entry(uint32_t cluster, uint32_t offset, DirEntry *entry);
int fat32_write_dir_entry(uint32_t cluster, uint32_t offset, DirEntry *entry);
int fat32_find_entry(uint32_t dir_cluster, const char *name, DirEntry *entry,
                     uint32_t *entry_cluster, uint32_t *entry_offset);
int fat32_add_dir_entry(uint32_t dir_cluster, DirEntry *entry);
int fat32_remove_dir_entry(uint32_t dir_cluster, uint32_t offset);
bool fat32_is_dir_empty(uint32_t cluster);

// name conversion
void fat32_name_to_83(const char *name, char *name83);
void fat32_83_to_name(const uint8_t *name83, char *name);

// misc
uint32_t fat32_get_cluster(DirEntry *entry);
void fat32_set_cluster(DirEntry *entry, uint32_t cluster);
uint32_t fat32_get_cluster_size(void);
uint32_t fat32_get_image_size(void);

#endif
