// main.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "fat32.h"
#include "commands.h"

int main(int argc, char *argv[])
{
    if(argc != 2) {
        fprintf(stderr, "Usage: %s [FAT32 ISO]\n", argv[0]);
        return 1;
    }
    if(fat32_mount(argv[1]) != 0){
        fprintf(stderr, "Error: Could not mount %s\n", argv[1]);
        return 1;
    }

    while(1)
    {
        printf("%s%s> ", fs.image_name, fs.current_path);
        fflush(stdout);

        char* input = get_input();
        
        if(input == NULL || strlen(input) == 0) {
            free(input);
            continue;
        }

        tokenlist *tokens = get_tokens(input);
        if(tokens->size > 0) {
            int result = dispatch_command(tokens);
            if(result == -1) {
                free(input);
                free_tokens(tokens);
                break;
            }
        }

        free(input);
        free_tokens(tokens);
    }

    fat32_unmount();

    return 0;
}

int dispatch_command(tokenlist *tokens)
{
    if(tokens->size == 0)
        return 0;

    const char* cmd = tokens->items[0];

    if (strcmp(cmd, "info") == 0) {
        cmd_info(tokens);
    }
    else if(strcmp(cmd, "exit") == 0) {
        cmd_exit(tokens);
        return -1;
    }
    else if (strcmp(cmd, "cd") == 0)
        cmd_cd(tokens);
    else if(strcmp(cmd, "ls") == 0)
        cmd_ls(tokens);
    else if (strcmp(cmd, "mkdir") == 0)
        cmd_mkdir(tokens);
    else if(strcmp(cmd, "creat") == 0)
        cmd_creat(tokens);
    else if(strcmp(cmd, "open") == 0)
        cmd_open(tokens);
    else if (strcmp(cmd, "close") == 0)
        cmd_close(tokens);
    else if(strcmp(cmd, "lsof") == 0)
        cmd_lsof(tokens);
    else if (strcmp(cmd, "lseek") == 0)
        cmd_lseek(tokens);
    else if(strcmp(cmd, "read") == 0)
        cmd_read(tokens);
    else if(strcmp(cmd, "write") == 0)
        cmd_write(tokens);
    else if (strcmp(cmd, "mv") == 0)
        cmd_mv(tokens);
    else if(strcmp(cmd, "rm") == 0)
        cmd_rm(tokens);
    else if (strcmp(cmd, "rmdir") == 0)
        cmd_rmdir(tokens);
    else {
        printf("Error: Unknown command '%s'\n", cmd);
        return 1;
    }

    return 0;
}
