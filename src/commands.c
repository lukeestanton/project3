// commands.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "commands.h"
#include "fat32.h"

static int find_open_file(const char *name);
static int find_free_slot(void);
static void free_cluster_chain(uint32_t cluster);

void cmd_info(tokenlist *tokens)
{
    (void)tokens;

    printf("Position of Root Cluster (in cluster #): %u\n", fs.bs.BPB_RootClus);
    printf("Bytes Per Sector: %u\n", fs.bs.BPB_BytsPerSec);
    printf("Sectors Per Cluster: %u\n", fs.bs.BPB_SecPerClus);
    printf("Total # of Clusters in Data Region: %u\n", fs.total_clusters);
    printf("# of Entries in One FAT: %u\n", fs.bs.BPB_FATSz32 * fs.bs.BPB_BytsPerSec / 4);
    printf("Size of Image (in bytes): %u\n", fat32_get_image_size());
}


// actual exit in main loop now
void cmd_exit(tokenlist *tokens)
{
    (void)tokens;
}

void cmd_cd(tokenlist *tokens)
{
    if(tokens->size != 2){
        printf("Error: cd needs exactly 1 argument\n");
        return;
    }

    const char *dirname = tokens->items[1];

    if (strcmp(dirname, "..") == 0) {
        DirEntry entry;
        if (fat32_find_entry(fs.current_dir, "..", &entry, NULL, NULL) == 0) {
            uint32_t parent_cluster = fat32_get_cluster(&entry);
            
            if(parent_cluster == 0)
                parent_cluster = fs.bs.BPB_RootClus;
            
            fs.current_dir = parent_cluster;
            
            if (strcmp(fs.current_path, "/") != 0) {
                char *last_slash = strrchr(fs.current_path, '/');
                if (last_slash != NULL && last_slash != fs.current_path)
                    *last_slash = '\0';
                else
                    strcpy(fs.current_path, "/");
            }
        }
        return;
    }

    if (strcmp(dirname, ".") == 0) return;

    DirEntry entry;
    if (fat32_find_entry(fs.current_dir, dirname, &entry, NULL, NULL) != 0) {
        printf("Error: %s does not exist\n", dirname);
        return;
    }

    if (!(entry.DIR_Attr & ATTR_DIRECTORY)) {
        printf("Error: %s is not a directory\n", dirname);
        return;
    }

    uint32_t new_cluster = fat32_get_cluster(&entry);
    if (new_cluster == 0)
        new_cluster = fs.bs.BPB_RootClus;
    fs.current_dir = new_cluster;

    if(strcmp(fs.current_path, "/") != 0)
        strcat(fs.current_path, "/");
    
    char name[13];
    fat32_83_to_name(entry.DIR_Name, name);
    strcat(fs.current_path, name);
}

void cmd_ls(tokenlist *tokens)
{
    (void)tokens;
    uint32_t cluster = fs.current_dir;
    uint32_t clusterSize = fat32_get_cluster_size();
    uint32_t entries_per_cluster = clusterSize / sizeof(DirEntry);

    while(cluster < FAT_EOC && cluster != 0)
    {
        for(uint32_t i = 0; i < entries_per_cluster; i++) {
            DirEntry entry;
            uint32_t off = i * sizeof(DirEntry);

            if(fat32_read_dir_entry(cluster, off, &entry) != 0)
                return;

            if (entry.DIR_Name[0] == 0x00) return;
            if (entry.DIR_Name[0] == 0xE5) continue;
            if ((entry.DIR_Attr & ATTR_LONG_NAME) == ATTR_LONG_NAME) continue;

            char name[13];
            fat32_83_to_name(entry.DIR_Name, name);
            printf("%s\n", name);
        }
        cluster = fat32_get_fat_entry(cluster);
    }
}

void cmd_mkdir(tokenlist *tokens)
{
    if (tokens->size != 2) {
        printf("Error: mkdir requires 1 argument\n");
        return;
    }

    const char *dirname = tokens->items[1];

    if(fat32_find_entry(fs.current_dir, dirname, NULL, NULL, NULL) == 0) {
        printf("Error: %s already exists\n", dirname);
        return;
    }

    uint32_t newCluster = fat32_allocate_cluster(0);
    if(newCluster == 0){
        printf("Error: Couldn't allocate cluster for directory\n");
        return;
    }

    DirEntry new_entry;
    memset(&new_entry, 0, sizeof(DirEntry));
    fat32_name_to_83(dirname, (char *)new_entry.DIR_Name);
    new_entry.DIR_Attr = ATTR_DIRECTORY;
    fat32_set_cluster(&new_entry, newCluster);
    new_entry.DIR_FileSize = 0;

    // timestamps
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    uint16_t date = ((tm->tm_year - 80) << 9) | ((tm->tm_mon + 1) << 5) | tm->tm_mday;
    uint16_t timeval = (tm->tm_hour << 11) | (tm->tm_min << 5) | (tm->tm_sec / 2);
    new_entry.DIR_CrtDate = date;
    new_entry.DIR_CrtTime = timeval;
    new_entry.DIR_WrtDate = date;
    new_entry.DIR_WrtTime = timeval;
    new_entry.DIR_LstAccDate = date;

    if(fat32_add_dir_entry(fs.current_dir, &new_entry) != 0) {
        printf("Error: Could not add directory entry\n");
        fat32_set_fat_entry(newCluster, FAT_FREE);
        return;
    }

    // .
    DirEntry dot;
    memset(&dot, 0, sizeof(DirEntry));
    memset(dot.DIR_Name, ' ', 11);
    dot.DIR_Name[0] = '.';
    dot.DIR_Attr = ATTR_DIRECTORY;
    fat32_set_cluster(&dot, newCluster);
    dot.DIR_CrtDate = date;
    dot.DIR_CrtTime = timeval;
    dot.DIR_WrtDate = date;
    dot.DIR_WrtTime = timeval;
    dot.DIR_LstAccDate = date;

    fat32_write_dir_entry(newCluster, 0, &dot);

    // ..
    DirEntry dotdot;
    memset(&dotdot, 0, sizeof(DirEntry));
    memset(dotdot.DIR_Name, ' ', 11);
    dotdot.DIR_Name[0] = '.';
    dotdot.DIR_Name[1] = '.';
    dotdot.DIR_Attr = ATTR_DIRECTORY;
    
    if(fs.current_dir == fs.bs.BPB_RootClus)
        fat32_set_cluster(&dotdot, 0);
    else
        fat32_set_cluster(&dotdot, fs.current_dir);
    dotdot.DIR_CrtDate = date;
    dotdot.DIR_CrtTime = timeval;
    dotdot.DIR_WrtDate = date;
    dotdot.DIR_WrtTime = timeval;
    dotdot.DIR_LstAccDate = date;

    fat32_write_dir_entry(newCluster, sizeof(DirEntry), &dotdot);
}

void cmd_creat(tokenlist *tokens)
{
    if (tokens->size != 2) {
        printf("Error: create requires exactly one argument\n");
        return;
    }
    const char *filename = tokens->items[1];

    if (fat32_find_entry(fs.current_dir, filename, NULL, NULL, NULL) == 0) {
        printf("Error: %s already exists\n", filename);
        return;
    }

    DirEntry newEntry;
    memset(&newEntry, 0, sizeof(DirEntry));
    fat32_name_to_83(filename, (char*)newEntry.DIR_Name);
    newEntry.DIR_Attr = ATTR_ARCHIVE;
    fat32_set_cluster(&newEntry, 0);
    newEntry.DIR_FileSize = 0;

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    uint16_t date = ((tm->tm_year - 80) << 9) | ((tm->tm_mon + 1) << 5) | tm->tm_mday;
    uint16_t timeval = (tm->tm_hour << 11) | (tm->tm_min << 5) | (tm->tm_sec / 2);
    newEntry.DIR_CrtDate = date;
    newEntry.DIR_CrtTime = timeval;
    newEntry.DIR_WrtDate = date;
    newEntry.DIR_WrtTime = timeval;
    newEntry.DIR_LstAccDate = date;

    if (fat32_add_dir_entry(fs.current_dir, &newEntry) != 0) {
        printf("Error: Couldnt add file entry\n");
        return;
    }
}

static int find_open_file(const char *name)
{
    char name83[11];
    fat32_name_to_83(name, name83);

    for(int idx = 0; idx < MAX_OPEN_FILES; idx++)
    {
        if(fs.open_files[idx].in_use)
        {
            char entry_name83[11];
            fat32_name_to_83(fs.open_files[idx].name, entry_name83);
            
            if(memcmp(entry_name83, name83, 11) == 0 &&
               strcmp(fs.open_files[idx].path, fs.current_path) == 0)
            {
                return idx;
            }
        }
    }
    return -1;
}

static int find_free_slot(void)
{
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!fs.open_files[i].in_use)
            return i;
    }
    return -1;
}

void cmd_open(tokenlist *tokens)
{
    if(tokens->size != 3) {
        printf("Error: open requires both the filenames and flags arguments\n");
        return;
    }

    const char* filename = tokens->items[1];
    const char* flags = tokens->items[2];

    uint8_t mode = 0;
    if(strcmp(flags, "-r") == 0) {
        mode = MODE_READ;
    } else if(strcmp(flags, "-w") == 0) {
        mode = MODE_WRITE;
    } else if(strcmp(flags, "-rw") == 0 || strcmp(flags, "-wr") == 0) {
        mode = MODE_RW;
    } else {
        printf("Error: Invalid mode '%s'\n", flags);
        return;
    }

    DirEntry entry;
    uint32_t entry_cluster, entry_offset;
    if(fat32_find_entry(fs.current_dir, filename, &entry, &entry_cluster, &entry_offset) != 0){
        printf("Error: %s does not exist\n", filename);
        return;
    }

    if(entry.DIR_Attr & ATTR_DIRECTORY) {
        printf("Error: %s is a directory\n", filename);
        return;
    }

    if(find_open_file(filename) >= 0){
        printf("Error: %s is already open\n", filename);
        return;
    }

    int slot = find_free_slot();
    if(slot < 0) {
        printf("Error: Max number of files already open\n");
        return;
    }

    OpenFile *of = &fs.open_files[slot];
    fat32_83_to_name(entry.DIR_Name, of->name);
    strncpy(of->path, fs.current_path, MAX_PATH - 1);
    of->path[MAX_PATH - 1] = '\0';
    of->first_cluster = fat32_get_cluster(&entry);
    of->size = entry.DIR_FileSize;
    of->offset = 0;
    of->mode = mode;
    of->in_use = true;
    of->dir_cluster = entry_cluster;
    of->dir_entry_offset = entry_offset;
}

void cmd_close(tokenlist *tokens)
{
    if (tokens->size != 2) {
        printf("Error: close requires FILENAME argument\n");
        return;
    }

    const char *filename = tokens->items[1];

    if(fat32_find_entry(fs.current_dir, filename, NULL, NULL, NULL) != 0)
    {
        printf("Error: %s does not exist\n", filename);
        return;
    }

    int slot = find_open_file(filename);
    if(slot < 0){
        printf("Error: %s is not open\n", filename);
        return;
    }

    fs.open_files[slot].in_use = false;
}

// list open files
void cmd_lsof(tokenlist *tokens)
{
    (void)tokens;

    int cnt = 0;
    printf("Index\tName\t\tMode\tOffset\tPath\n");
    
    for(int i=0; i<MAX_OPEN_FILES; i++)
    {
        if(fs.open_files[i].in_use)
        {
            const char* mode_str;
            switch(fs.open_files[i].mode){
                case MODE_READ: mode_str = "r"; break;
                case MODE_WRITE: mode_str = "w"; break;
                case MODE_RW: mode_str = "rw"; break;
                default: mode_str = "?";  break;
            }
            printf("%d\t%-12s\t%s\t%u\t%s\n",
                   i,
                   fs.open_files[i].name,
                   mode_str,
                   fs.open_files[i].offset,
                   fs.open_files[i].path);
            cnt++;
        }
    }

    if(cnt == 0) printf("No files are currently open\n");
}

// change file offset
void cmd_lseek(tokenlist *tokens)
{
    if (tokens->size != 3) {
        printf("Error: lseek requires FILENAME and OFFSET arguments\n");
        return;
    }

    const char *filename = tokens->items[1];
    uint32_t offset = (uint32_t)atoi(tokens->items[2]);

    DirEntry entry;
    if (fat32_find_entry(fs.current_dir, filename, &entry, NULL, NULL) != 0) {
        printf("Error: %s does not exist\n", filename);
        return;
    }

    int slot = find_open_file(filename);
    if (slot < 0) {
        printf("Error: %s isn't open\n", filename);
        return;
    }

    if(offset > fs.open_files[slot].size){
        printf("Error: Offset %u is larger than file size %u\n", 
               offset, fs.open_files[slot].size);
        return;
    }

    fs.open_files[slot].offset = offset;
}

void cmd_read(tokenlist *tokens)
{
    if(tokens->size != 3){
        printf("Error: read requires FILENAME and SIZE arguments\n");
        return;
    }

    const char *filename = tokens->items[1];
    uint32_t size = (uint32_t)atoi(tokens->items[2]);

    DirEntry entry;
    if(fat32_find_entry(fs.current_dir, filename, &entry, NULL, NULL) != 0) {
        printf("Error: %s does not exist\n", filename);
        return;
    }

    if(entry.DIR_Attr & ATTR_DIRECTORY){
        printf("Error: %s is a directory\n", filename);
        return;
    }

    int slot = find_open_file(filename);
    if (slot < 0) {
        printf("Error: %s is not open\n", filename);
        return;
    }

    if(!(fs.open_files[slot].mode & MODE_READ)) {
        printf("Error: %s is not open for reading\n", filename);
        return;
    }

    OpenFile *of = &fs.open_files[slot];
    
    if(of->offset + size > of->size)
        size = of->size - of->offset;

    if(size == 0) return;

    uint32_t clusterSize = fat32_get_cluster_size();
    uint32_t cluster = of->first_cluster;
    uint32_t bytesRead = 0;
    uint32_t currentPos = 0;

    // skip to cluster with offset
    while(currentPos + clusterSize <= of->offset && cluster < FAT_EOC){
        currentPos += clusterSize;
        cluster = fat32_get_fat_entry(cluster);
    }

    uint8_t *buffer = malloc(clusterSize);
    if(!buffer) {
        printf("Error: Memory allocation failed\n");
        return;
    }

    while(bytesRead < size && cluster < FAT_EOC && cluster != 0)
    {
        if (fat32_read_cluster(cluster, buffer) != 0) {
            free(buffer);
            return;
        }

        uint32_t offsetInCluster = (of->offset + bytesRead) % clusterSize;
        uint32_t toRead = clusterSize - offsetInCluster;
        
        if(toRead > size - bytesRead)
            toRead = size - bytesRead;

        for(uint32_t i = 0; i < toRead; ++i)
            putchar(buffer[offsetInCluster + i]);

        bytesRead += toRead;
        
        if(bytesRead < size)
            cluster = fat32_get_fat_entry(cluster);
    }

    printf("\n");
    of->offset += bytesRead;
    free(buffer);
}

void cmd_write(tokenlist *tokens)
{
    if(tokens->size < 3) {
        printf("Error: write requires both the filename and string arguments\n");
        return;
    }

    const char *filename = tokens->items[1];
    
    char string[1024] = {0};
    int in_quote = 0;
    int str_idx = 0;
    
    for(int i = 2; i < (int)tokens->size; i++)
    {
        const char *tok = tokens->items[i];
        
        for(int j=0; tok[j] != '\0'; j++) {
            if(tok[j] == '"'){
                in_quote = !in_quote;
                continue;
            }
            if(in_quote || (tok[j] != '"'))
                string[str_idx++] = tok[j];
        }
        
        if(i < (int)tokens->size - 1)
            string[str_idx++] = ' ';
    }
    
    while(str_idx > 0 && (string[str_idx - 1] == '"' || string[str_idx - 1] == ' '))
        string[--str_idx] = '\0';

    uint32_t writeSize = strlen(string);

    DirEntry entry;
    uint32_t entryCluster, entryOffset;
    if (fat32_find_entry(fs.current_dir, filename, &entry, &entryCluster, &entryOffset) != 0) {
        printf("Error: %s does not exist\n", filename);
        return;
    }

    if (entry.DIR_Attr & ATTR_DIRECTORY) {
        printf("Error: %s is a directory\n", filename);
        return;
    }

    int slot = find_open_file(filename);
    if(slot < 0) {
        printf("Error: %s is not open\n", filename);
        return;
    }

    if (!(fs.open_files[slot].mode & MODE_WRITE)) {
        printf("Error: %s is not open for writing\n", filename);
        return;
    }

    OpenFile *of = &fs.open_files[slot];
    uint32_t clusterSize = fat32_get_cluster_size();
    uint32_t neededSize = of->offset + writeSize;
    
    // first cluster if empty
    if(of->first_cluster == 0)
    {
        uint32_t newClus = fat32_allocate_cluster(0);
        if(newClus == 0) {
            printf("Error: Couldn't allocate cluster\n");
            return;
        }
        of->first_cluster = newClus;
        entry.DIR_FstClusLO = newClus & 0xFFFF;
        entry.DIR_FstClusHI = (newClus >> 16) & 0xFFFF;
    }
    uint32_t cluster = of->first_cluster;
    uint32_t currentPos = 0;

    while(currentPos + clusterSize <= of->offset)
    {
        uint32_t next = fat32_get_fat_entry(cluster);
        if(next >= FAT_EOC){
            uint32_t newClus = fat32_allocate_cluster(cluster);
            if(newClus == 0){
                printf("Error: Couldn't allocate cluster\n");
                return;
            }
            cluster = newClus;
        } else {
            cluster = next;
        }
        currentPos += clusterSize;
    }

    uint32_t bytesWritten = 0;
    uint8_t* buf = malloc(clusterSize);
    if(buf == NULL){
        printf("Error: Memory allocation failed\n");
        return;
    }

    while(bytesWritten < writeSize)
    {
        fat32_read_cluster(cluster, buf);

        uint32_t offsetInCluster = (of->offset + bytesWritten) % clusterSize;
        uint32_t toWrite = clusterSize - offsetInCluster;
        
        if (toWrite > writeSize - bytesWritten)
            toWrite = writeSize - bytesWritten;

        memcpy(buf + offsetInCluster, string + bytesWritten, toWrite);
        fat32_write_cluster(cluster, buf);
        bytesWritten += toWrite;

        if(bytesWritten < writeSize) {
            uint32_t next = fat32_get_fat_entry(cluster);
            if(next >= FAT_EOC) {
                uint32_t newClus = fat32_allocate_cluster(cluster);
                if(newClus == 0) {
                    printf("Error: Couldn't allocate cluster\n");
                    free(buf);
                    return;
                }
                cluster = newClus;
            } else {
                cluster = next;
            }
        }
    }

    free(buf);

    if(neededSize > of->size) {
        of->size = neededSize;
        entry.DIR_FileSize = neededSize;
    }

    fat32_set_cluster(&entry, of->first_cluster);
    entry.DIR_FileSize = of->size;

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    uint16_t date = ((tm->tm_year - 80) << 9) | ((tm->tm_mon + 1) << 5) | tm->tm_mday;
    uint16_t timeval = (tm->tm_hour << 11) | (tm->tm_min << 5) | (tm->tm_sec / 2);
    entry.DIR_WrtDate = date;
    entry.DIR_WrtTime = timeval;
    
    fat32_write_dir_entry(entryCluster, entryOffset, &entry);
    of->offset += bytesWritten;
}

void cmd_mv(tokenlist *tokens)
{
    if(tokens->size != 3){
        printf("Error: mv needs both source and destination arguments\n");
        return;
    }

    const char* src = tokens->items[1];
    const char* dest = tokens->items[2];

    DirEntry srcEntry;
    uint32_t srcCluster, srcOffset;
    if(fat32_find_entry(fs.current_dir, src, &srcEntry, &srcCluster, &srcOffset) != 0)
    {
        printf("Error: %s doesnt exist\n", src);
        return;
    }

    if (find_open_file(src) >= 0) {
        printf("Error: %s is open\n", src);
        return;
    }

    DirEntry destEntry;
    uint32_t destCluster, destOffset;
    int destExists = fat32_find_entry(fs.current_dir, dest, &destEntry, &destCluster, &destOffset);

    if(destExists == 0)
    {
        if(destEntry.DIR_Attr & ATTR_DIRECTORY)
        {
            uint32_t destDirCluster = fat32_get_cluster(&destEntry);
            
            char srcName[13];
            fat32_83_to_name(srcEntry.DIR_Name, srcName);
            if(fat32_find_entry(destDirCluster, srcName, NULL, NULL, NULL) == 0){
                printf("Error: %s already exists in destination directory\n", srcName);
                return;
            }

            if(fat32_add_dir_entry(destDirCluster, &srcEntry) != 0){
                printf("Error: Could not move entry\n");
                return;
            }

            fat32_remove_dir_entry(srcCluster, srcOffset);

            if(srcEntry.DIR_Attr & ATTR_DIRECTORY)
            {
                uint32_t movedCluster = fat32_get_cluster(&srcEntry);
                DirEntry dotdot;
                fat32_read_dir_entry(movedCluster, sizeof(DirEntry), &dotdot);
                fat32_set_cluster(&dotdot, destDirCluster);
                fat32_write_dir_entry(movedCluster, sizeof(DirEntry), &dotdot);
            }
        }
        else {
            printf("Error: %s is a file\n", dest);
            return;
        }
    }
    else
    {
        fat32_name_to_83(dest, (char *)srcEntry.DIR_Name);
        fat32_write_dir_entry(srcCluster, srcOffset, &srcEntry);
    }
}

static void free_cluster_chain(uint32_t cluster)
{
    while(cluster < FAT_EOC && cluster != 0){
        uint32_t next = fat32_get_fat_entry(cluster);
        fat32_set_fat_entry(cluster, FAT_FREE);
        cluster = next;
    }
}

void cmd_rm(tokenlist *tokens)
{
    if (tokens->size != 2) {
        printf("Error: rm requires FILENAME argument\n");
        return;
    }

    const char *filename = tokens->items[1];

    DirEntry entry;
    uint32_t entCluster, entOffset;
    if (fat32_find_entry(fs.current_dir, filename, &entry, &entCluster, &entOffset) != 0) {
        printf("Error: %s does not exist\n", filename);
        return;
    }

    if(entry.DIR_Attr & ATTR_DIRECTORY) {
        printf("Error: %s is a directory\n", filename);
        return;
    }

    if(find_open_file(filename) >= 0){
        printf("Error: %s is open\n", filename);
        return;
    }

    uint32_t clus = fat32_get_cluster(&entry);
    if(clus != 0)
        free_cluster_chain(clus);

    fat32_remove_dir_entry(entCluster, entOffset);
}

void cmd_rmdir(tokenlist *tokens)
{
    if(tokens->size != 2) {
        printf("Error: rmdir requires DIRNAME argument\n");
        return;
    }

    const char *dirname = tokens->items[1];

    if(strcmp(dirname, ".") == 0 || strcmp(dirname, "..") == 0){
        printf("Error: Cannot remove %s\n", dirname);
        return;
    }
    DirEntry entry;
    uint32_t entryCluster, entryOffset;
    if(fat32_find_entry(fs.current_dir, dirname, &entry, &entryCluster, &entryOffset) != 0)
    {
        printf("Error: %s does not exist\n", dirname);
        return;
    }
    if(!(entry.DIR_Attr & ATTR_DIRECTORY)){
        printf("Error: %s is not a directory\n", dirname);
        return;
    }

    uint32_t dirCluster = fat32_get_cluster(&entry);

    if(!fat32_is_dir_empty(dirCluster)) {
        printf("Error: %s is not empty\n", dirname);
        return;
    }

    for(int i = 0; i < MAX_OPEN_FILES; i++)
    {
        if(fs.open_files[i].in_use) {
            char expectedPath[MAX_PATH];
            if(strcmp(fs.current_path, "/") == 0)
                snprintf(expectedPath, MAX_PATH, "/%s", dirname);
            else
                snprintf(expectedPath, MAX_PATH, "%s/%s", fs.current_path, dirname);
            
            if(strcmp(fs.open_files[i].path, expectedPath) == 0) {
                printf("Error: A file in %s is open\n", dirname);
                return;
            }
        }
    }

    if (dirCluster != 0) free_cluster_chain(dirCluster);

    fat32_remove_dir_entry(entryCluster, entryOffset);
}
