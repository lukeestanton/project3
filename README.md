# FAT32 File System Utility

A user-space shell-like utility capable of interpreting FAT32 file system images. This utility allows users to navigate, read, write, and manipulate files within a FAT32 file system image.

## Group Members
- **Luke Stanton**: les22

## Division of Labor

### Part 1: Mounting the Image
- **Responsibilities**: Mount FAT32 image, parse boot sector, implement `info` and `exit` commands
- **Assigned to**: Luke Stanton

### Part 2: Navigation
- **Responsibilities**: Implement `cd` and `ls` commands
- **Assigned to**: Luke Stanton

### Part 3: Create
- **Responsibilities**: Implement `mkdir` and `creat` commands
- **Assigned to**: Luke Stanton

### Part 4: Read
- **Responsibilities**: Implement `open`, `close`, `lsof`, `lseek`, and `read` commands
- **Assigned to**: Luke Stanton

### Part 5: Update
- **Responsibilities**: Implement `write` and `mv` commands
- **Assigned to**: Luke Stanton

### Part 6: Delete
- **Responsibilities**: Implement `rm` and `rmdir` commands
- **Assigned to**: Luke Stanton

## File Listing
```
project3/
├── src/
│   ├── main.c        # Main entry point and command dispatcher
│   ├── fat32.c       # FAT32 core operations
│   ├── commands.c    # Command implementations
│   └── lexer.c       # Input tokenization
│
├── include/
│   ├── fat32.h       # FAT32 structures and function declarations
│   ├── commands.h    # Command function declarations
│   └── lexer.h       # Lexer function declarations
│
├── Makefile
└── README.md
```

`bin/` and `obj/` directories are created during compilation

## How to Compile & Execute

### Requirements
- **Compiler**: GCC with C99 support
- **OS**: Linux (tested on linprog)

### Compilation
```bash
make
```
This builds the executable and place it in `bin/filesys`.

To clean build artifacts:
```bash
make clean
```

### Execution
```bash
./bin/filesys [FAT32_IMAGE]
```

Example:
```bash
./bin/filesys fat32.img
```

### Available Commands

| Command | Description |
|---------|-------------|
| `info` | Display file system information |
| `exit` | Exit program |
| `cd DIRNAME` | Change current directory to DIRNAME |
| `ls` | List directory contents |
| `mkdir DIRNAME` | Create new directory |
| `creat FILENAME` | Create new empty file |
| `open FILENAME FLAGS` | Open file (-r, -w, -rw, -wr) |
| `close FILENAME` | Close a open file |
| `lsof` | List all open files |
| `lseek FILENAME OFFSET` | Set file offset for reading/writing |
| `read FILENAME SIZE` | Read SIZE bytes from file |
| `write FILENAME "STRING"` | Write STRING to file |
| `mv SOURCE DEST` | Move/rename a file or directory |
| `rm FILENAME` | Delete a file |
| `rmdir DIRNAME` | Remove an empty directory |

## Implementation Details

### FAT32 Structure
The utility reads and interprets the following FAT32 structures:
- **Boot Sector (BPB)**: Contains file system metadata
- **File Allocation Table (FAT)**: Maps cluster chains
- **Directory Entries**: 32-byte entries describing files and directories

### Key Features
- Maintains current working directory state
- Supports up to 10 simultaneously open files
- Cluster chain traversal for large files/directories
- Properly updates FAT entries when creating/deleting files
- Creates . and .. entries for new directories

### Error Handling
- Non-existent files/directories
- Invalid command arguments
- File already open/not open
- Offset out of bounds
- Non-empty directory removal
- Name conflicts during creation

## Known Bugs
- None currently known

## Considerations
- Long file name (LFN) entries are skipped (only 8.3 names supported)
- File/directory names are converted to uppercase internally
- The program requires read-write access to the image file

