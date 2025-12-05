#ifndef COMMANDS_H
#define COMMANDS_H

#include "lexer.h"

// command handlers
void cmd_info(tokenlist *tokens);
void cmd_exit(tokenlist *tokens);
void cmd_cd(tokenlist *tokens);
void cmd_ls(tokenlist *tokens);
void cmd_mkdir(tokenlist *tokens);
void cmd_creat(tokenlist *tokens);
void cmd_open(tokenlist *tokens);
void cmd_close(tokenlist *tokens);
void cmd_lsof(tokenlist *tokens);
void cmd_lseek(tokenlist *tokens);
void cmd_read(tokenlist *tokens);
void cmd_write(tokenlist *tokens);
void cmd_mv(tokenlist *tokens);
void cmd_rm(tokenlist *tokens);
void cmd_rmdir(tokenlist *tokens);

int dispatch_command(tokenlist *tokens);

#endif
