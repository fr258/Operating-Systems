/*
 *  Copyright (C) 2021 CS416 Rutgers CS
 *	Tiny File System
 *	File:	tfs.c
 *
 */

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <libgen.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "block.h"
#include "tfs.h"


bitmap_t iMap;
bitmap_t dMap;

char diskfile_path[PATH_MAX];

// Declare your in-memory data structures here
struct superblock superblock;


/* 
 * Get available inode number from bitmap
 */
int get_avail_ino() {

	// Step 1: Read inode bitmap from disk
	// char inodeBitmap[];
	// bio_read(superblock.i_bitmap_blk, inodeBitmap);

	// Step 2: Traverse inode bitmap to find an available slot
	int retVal = -1;

	for(int i = 0; i < MAX_INUM; i++) {
		if(get_bitmap(iMap, i) & 1){
			// Set next avail inode number
			retVal = i;

			// Update bitmap
			set_bitmap(iMap, i);
			
			break;
		}
	}

	// Step 3: Update inode bitmap and write to disk 
	bio_write(superblock.i_bitmap_blk, iMap);

	return retVal;
}

/* 
 * Get available data block number from bitmap
 */
int get_avail_blkno() {

	// Step 1: Read data block bitmap from disk
	// char blockBitmap[];
	// bio_read(superblock.d_bitmap_blk, blockBitmap);

	// Step 2: Traverse data block bitmap to find an available slot
	int retVal = -1;

	for(int i = 0; i < MAX_DNUM; i++) {
		if(get_bitmap(dMap, i) & 1){
			// Set next avail data block number
			retVal = i;

			// Update bitmap
			set_bitmap(dMap, i); 
			
			break;
		}
	}

	// Step 3: Update data block bitmap and write to disk 
	bio_write(superblock.d_bitmap_blk, dMap);

	return retVal;
}

/* 
 * inode operations
 */
// Returns -1 upon not finding the ino number
// Returns 1 upon finding the corresponding inode and sets the inode pointer
// *inode points to heap memory - must be freed by caller
int readi(uint16_t ino, struct inode *inode) {

	int i, j, readRet = 0, inodeSize = sizeof(struct inode);
	void *readBuf = malloc(BLOCK_SIZE);
	struct inode *tempInode;

  	// Step 1: Get the inode's on-disk block number
	// Iterate thru all inode blocks
	// Assumes inode blocks are stored before data blocks
	for(i = superblock.i_start_blk; i < superblock.d_start_blk; i++){
		// Read contents of block i - cast for easier manipulation
		readRet = bio_read(i, readBuf);
		readBuf = (char*)readBuf;

		// On successful read, go thru all inodes in single block (max of 16)
		if(readRet > 0){
			// Step 2: Get offset of the inode in the inode on-disk block
			for(j = 0; j < BLOCK_SIZE / inodeSize; j++){
				// Extract jth inode in readBuf and cast to struct inode
				memcpy(tempInode, readBuf + (j * inodeSize), inodeSize);
				tempInode = (struct inode*)tempInode;

				// Step 3: Read the block from disk and then copy into inode structure
				// Compare ith inode's number to desired number
				if(tempInode->ino == ino){
					inode = tempInode;
					free(readBuf);
					return 1;
				}
			}
		}
	}
	free(readBuf);
	return -1;
}

// Returns -1 upon not finding the ino number
// Returns 1 upon finding the corresponding inode and sets the inode pointer
// *inode points to heap memory - must be freed by caller
int writei(uint16_t ino, struct inode *inode) {

	int i, j, readRet = 0, inodeSize = sizeof(struct inode);
	void *readBuf = malloc(BLOCK_SIZE);
	struct inode *tempInode;

  	// Step 1: Get the inode's on-disk block number
	// Iterate thru all inode blocks
	// Assumes inode blocks are stored before data blocks
	for(i = superblock.i_start_blk; i < superblock.d_start_blk; i++){
		// Read contents of block i - cast for easier manipulation
		readRet = bio_read(i, readBuf);
		readBuf = (char*)readBuf;

		// On successful read, go thru all inodes in single block (max of 16)
		if(readRet > 0){
			// Step 2: Get offset of the inode in the inode on-disk block
			for(j = 0; j < BLOCK_SIZE / inodeSize; j++){
				// Extract jth inode in readBuf and cast to struct inode
				memcpy(tempInode, readBuf + (j * inodeSize), inodeSize);
				tempInode = (struct inode*)tempInode;

				// Step 3: Read the block from disk and then copy into inode structure
				// Compare ith inode's number to desired number
				if(tempInode->ino == ino){
					// Overwrite read inode with given inode in buffer
					// Write entire buffer to disk
					memcpy(readBuf + (j * inodeSize), inode, inodeSize);
					bio_write(i, readBuf);
					free(readBuf);
					return 1;
				}
			}
		}
	}
	free(readBuf);
	return -1;
}




/* 
 * directory operations
 */
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {

  // Step 1: Call readi() to get the inode using ino (inode number of current directory)
	struct inode *inode = malloc(sizeof(inode));
	readi(ino, inode);

  // Step 2: Get data block of current directory from inode
  // Step 3: Read directory's data block and check each directory entry.
  //If the name matches, then copy directory entry to dirent structure 
	int dirents_per_block = BLOCK_SIZE/sizeof(struct dirent);
  	struct dirent *block = malloc(BLOCK_SIZE);
	for(int i = 0; i < inode->size; i++) {//for each data block of directory
		bio_read(inode->direct_ptr[i], block);
		for(int b = 0; b < dirents_per_block; b++) {
			if(block[b].valid) {
				if(strcmp(fname, block[b].name) == 0) {
					memcpy(dirent, &block[b], sizeof(struct dirent));
					free(block);
					free(inode);
					return 1;
				}
			}
		}
	} 
	free(block);
	free(inode);
	return 0;
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode
	
	// Step 2: Check if fname (directory name) is already used in other entries

	// Step 3: Add directory entry in dir_inode's data block and write to disk

	// Allocate a new data block for this directory if it does not exist

	// Update directory inode

	// Write directory entry
	
	struct dirent dummyDirent;
	if(dir_find(dir_inode.ino, fname, name_len, &dummyDirent) == 0) { //entry with fname not found
		int found = 0;
		int dirents_per_block = BLOCK_SIZE/sizeof(struct dirent);
		struct dirent *block = malloc(BLOCK_SIZE);
		int current_block = 0;
		int curr_dirent_in_block = 0;
		int i;
		for(i = 0; !found && i < 16; i++) {//for each data block of directory
			if(dir_inode.direct_ptr[i] != 0) { //data block is valid
				bio_read(dir_inode.direct_ptr[i], block);
				for(int b = 0;(!found) && (b < dirents_per_block); b++) {
					if(!block[b].valid) { //empty dirent found
						found = 1;
						current_block = i;
						break;
					}
				}
			}
		} 
		if(!found) { //not enough space in current data blocks
			if(dir_inode.size >= 16) { //cannot expand further
				return 0;
			}
			//allocate new block
			else {
				int empty_ptr_index = -1;
				for(i = 0; i < 16; i++) {
					if(dir_inode.direct_ptr[i] == 0)
						empty_ptr_index = i;
				}
				//not enough free pointers
				if(empty_ptr_index == -1) 
					return 1;
				else {
					dir_inode.direct_ptr[empty_ptr_index] =  get_avail_blkno();
					struct dirent *temp_block = (struct dirent*)block; //make new block array of dirents
					bio_read(empty_ptr_index, temp_block); //read newly allocated block in
					struct dirent temp_dirent;
					temp_dirent.valid = 0;
					//format new data block
					int dirents_per_block = BLOCK_SIZE/sizeof(struct dirent);
					for(int i = 0; i < dirents_per_block; i++) {
						memcpy(&temp_block[i], &temp_dirent, sizeof(struct dirent)); 
					}
				}
			}
		}
		struct dirent dirent;
		dirent.ino = f_ino;
		dirent.valid = 1;
		strncpy(dirent.name, fname, name_len);
		dirent.len = name_len;
		
		memcpy(block+curr_dirent_in_block, &dirent, sizeof(dirent)); //write block in memory
		bio_write(dir_inode.direct_ptr[i], block); //write block to disk
		set_bitmap(iMap, f_ino); //set iMap in memory
		for(int d = superblock.i_bitmap_blk, j = 0; d < superblock.d_bitmap_blk; d++, j++) //write iMap to disk
			bio_write(d, iMap + j*BLOCK_SIZE);
		
		struct inode tempNode;
		tempNode.ino = 0;			/* inode number */
		tempNode.valid = 1;			/* validity of the inode */
		tempNode.size = 1;			/* size of the file */
		tempNode.type = 1;			/* type of the file */
		tempNode.link = 2;			/* link count */
		for(int c = 0; c < 16; c++) { /* direct pointer to data block */
			tempNode.direct_ptr[c] = 0;
		}
		
		writei(f_ino, &tempNode); //write new inode to disk	
		writei(dir_inode.ino, &dir_inode); //write directory inode to disk
	}

	return 0;
}

int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and checks each directory entry of dir_inode
	int dirents_per_block = BLOCK_SIZE/sizeof(struct dirent);
  	struct dirent *block = malloc(BLOCK_SIZE);
	
	for(int i = 0; i < dir_inode.size; i++) {//for each data block of directory,
		bio_read(dir_inode.direct_ptr[i], block); //read into memory
		for(int b = 0; b < dirents_per_block; b++) {
			if(block[b].valid) {
				if(strncmp(fname, block[b].name, name_len) == 0) { 	// Step 2: Check if fname exist
					struct dirent invalid_dirent;
					invalid_dirent.valid = 0;
					// Step 3: If exist, then remove it from dir_inode's data block and write to disk
					memcpy(&block[b], &invalid_dirent, sizeof(struct dirent)); 
					bio_write(dir_inode.direct_ptr[i], block); 
					
					free(block);
					return 1;
				}
			}
		}
	} 
	free(block);
	return 0;
}

/* 
 * namei operation
 */
// Given path of <dir1>/<dir2>/<dir3>/<file>, returns the inode of <file>
// Upon verifying <dir1> exists in current root directory, 
	// passes <dir2>/<dir3>/<file> into next iteration
// Assumes it is always given the total root directory's inode number, 0
// Sets *inode to point to <file>'s inode, which is stored in heap memory
	// and must be freed by the caller
// Returns -1 upon early termination, unable to find nextName in current directory
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {
	// Get next name in path
	// Extract <next> from <next>/<nextNext>/<nextNextNext>/...
	char nextName[208], nextPath[208];
	int i, j;
	// Copy chars into nextName until next delimiter or end of path
	for(i = 0; i < 208 && path[i] != '\0' && path[i] != '/'; i++){
		nextName[i] = path[i];
	}
	// End string with null terminator
	nextName[i] = '\0';

	// Extract <nextNext>/<nextNextNext>/... from <next>/<nextNext>/<nextNextNext>/...
	for(j = i; j < 208 && path[j] != '\0'; j++){
		nextPath[j-i] = path[j];
	}
	

	// Make sure path is not an empty string
	if(i == 0){
		inode = NULL;
		return -1;
	}

	// Find currPath's dir inside given ino's dir
	struct dirent *nextDirent = malloc(sizeof(struct dirent));
	int ret = dir_find(ino, nextName, i, nextDirent);

	// Couldn't find subdirectory/subfile name within directory number ino
	if(ret == 0 || nextDirent == NULL || nextDirent ->valid < 0){
		inode = NULL;
		return -1;
	}
	
	// Base case: after extracting currPath's inode, check if basename(path) == nextName
	// Not the best since a path of "test/good/bad/test/good" would return (directory) good's inode
	// Later: maybe it wouldn't since the path still has "/bad/test/good" appended to good
	if(strcmp(basename((char*)path), nextName) == 0){
		int dirInodeNumber = nextDirent->ino;
		free(nextDirent);
		// readi returns -1 on fail, 1 on success
		return readi(dirInodeNumber, inode);
	}
	// Recurse on next segment of the path
	return get_node_by_path(nextPath, nextDirent->ino, inode);
}




/* 
 * Make file system
 */
int tfs_mkfs() {

	//strncpy(diskfile_path, "./disk", PATH_MAX); //<-- this would appear to be taken care of in main
	
	// Call dev_init() to initialize (Create) Diskfile
	dev_init(diskfile_path);

	// write superblock information
	superblock.magic_num = MAGIC_NUM;													/* magic number */
	superblock.max_inum = MAX_INUM;														/* maximum inode number */
	superblock.max_dnum = MAX_DNUM;														/* maximum data block number */
	superblock.i_bitmap_blk = 1;														/* start block of inode bitmap */
	
	superblock.d_bitmap_blk = 1 + (MAX_INUM/8)/BLOCK_SIZE;								/* start block of data block bitmap */
	if((MAX_INUM/8)%BLOCK_SIZE != 0) superblock.d_bitmap_blk++;
	
	superblock.i_start_blk =  superblock.d_bitmap_blk + (MAX_DNUM/8)/BLOCK_SIZE;			/* start block of inode region */
	if((MAX_DNUM/8)%BLOCK_SIZE != 0) superblock.i_start_blk++;
	
	superblock.d_start_blk =  superblock.i_start_blk + (MAX_INUM*sizeof(struct inode))/BLOCK_SIZE;		/* start block of data block region */
	if((MAX_INUM*sizeof(struct inode))%BLOCK_SIZE != 0) superblock.d_start_blk++;
	
	// initialize inode bitmap
	iMap = malloc(MAX_INUM/8);
	memset(iMap, 0, MAX_INUM/8);
	
	// initialize data block bitmap
	dMap = malloc(MAX_DNUM/8);
	memset(dMap, 0, MAX_DNUM/8);

	// update bitmap information for root directory
	set_bitmap(iMap, 0);

	// update inode for root directory
	struct inode tempNode;
	tempNode.ino = 0;			/* inode number */
	tempNode.valid = 1;			/* validity of the inode */
	tempNode.size = 1;				/* size of the file */
	tempNode.type = 1;				/* type of the file */
	tempNode.link = 2;				/* link count */
	for(int i = 1; i < 16; i++) { /* direct pointer to data block */
		tempNode.direct_ptr[i] = 0;
	}	
//	tempNode.indirect_ptr[8];	/* indirect pointer to data block */
//	tempNode.vstat				/* inode stat */

	//write superblock
	char* temp = malloc(BLOCK_SIZE);
	memcpy(temp, &superblock, sizeof(superblock));
	bio_write(0, temp);
	//write inode map
	for(int i = superblock.i_bitmap_blk, j = 0; i < superblock.d_bitmap_blk; i++, j++)
		bio_write(i, iMap + j*BLOCK_SIZE);
	//write dnode map
	for(int i = superblock.d_bitmap_blk, j = 0; i < superblock.i_start_blk; i++, j++)
		bio_write(i, dMap + j*BLOCK_SIZE); 
	//write root inode
	bio_write(superblock.i_start_blk, &tempNode);

	return 0;
}


/* 
 * FUSE file operations
 */
static void *tfs_init(struct fuse_conn_info *conn) {

	// Step 1a: If disk file is not found, call mkfs
	if(access(diskfile_path, F_OK) != 0)
		tfs_mkfs();
  // Step 1b: If disk file is found, just initialize in-memory data structures
  // and read superblock from disk
	else {
		char* temp = malloc(BLOCK_SIZE);
		bio_read(0, temp);
		superblock = *(struct superblock*)temp;
		free(temp); //?
		
		//read in iMap
		temp = malloc(BLOCK_SIZE);
		bio_read(superblock.i_bitmap_blk, temp);
		iMap = (bitmap_t)temp;
		
		//read in dMap
		temp = malloc(BLOCK_SIZE);
		bio_read(superblock.d_bitmap_blk, temp);
		dMap = (bitmap_t)temp;
	}

	return NULL;
}

static void tfs_destroy(void *userdata) {

	// Step 1: De-allocate in-memory data structures

	// Free the bitmaps
	free(iMap);
	free(dMap);
	
	// Step 2: Close diskfile
	dev_close();
}

// Assumes a malloc'd stat struct has been passed in and must be freed by caller
// Returns 0 on success, ENOENT on failure to find file
static int tfs_getattr(const char *path, struct stat *stbuf) {

	// Step 1: call get_node_by_path() to get inode from path
	// must right error code on fail - returns "no such file or directory found"
	struct inode* inode = NULL;
	if(get_node_by_path(path, 0, inode) == -1){
		printf("File at path DNE\n");
		free(inode);
		return ENOENT;
	}

	// Step 2: fill attribute of file into stbuf from inode
	// st_dev - ID of device containing file
	stbuf->st_ino 	 = inode->ino;
	stbuf->st_mode   = inode->vstat.st_mode;
	stbuf->st_nlink  = inode->vstat.st_nlink;
	stbuf->st_uid 	 = getuid();
	stbuf->st_gid	 = getgid();
	// st_rdev - device ID (if special file)
	stbuf->st_size	 = inode->size;
	// stbuf->st_blksize = 4096
	// stbuf->st_blocks - number of 512B blocks allocated
	
	time(&stbuf->st_atime);
	time(&stbuf->st_mtime);
	// stbuf->st_ctim?

	free(inode);
	return 0;
}

static int tfs_opendir(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path
	struct inode *inode = NULL;
	// Sets to 1 on success, -1 on failure
	int ret = get_node_by_path(path, 0, inode);

	// Step 2: If not find, return -1
	free(inode);
    return ret;
}

static int tfs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path
	struct inode *inode = NULL;
    int ret = get_node_by_path(path, 0, inode);
	// Fail if path doesn't exist
	if(ret == -1){
		free(inode);
		return ENOENT;
	}

	// Step 2: Read directory entries from its data blocks, and copy them to filler

	// If inode pointed to by path is a file, it should fill buffer with file's name then return
	// but idk how to get the file name from an inode
	if(inode->type == 0){
		printf("Inputted a file when expected directory.\n");
		return ENOENT;
	}

	// Continue as normal if given a directory
	int i, j;
	int direntSize = sizeof(struct dirent);
	char readBuffer[BLOCK_SIZE];
	struct dirent *dtemp = malloc(direntSize);

	// Traverse all datablocks, which contain dirents, of the returned inode
	// i -> [0,16) since inodes have 16 direct ptrs
	for(i = 0; i < 16; i++){
		// Skip unused ptrs
		if(inode->direct_ptr[i] < 0){
			continue;
		}
		// Read in data block - check if read correctly
		int readRet = bio_read(inode->direct_ptr[i], readBuffer);
		if(readRet < 0){
			printf("bad block read\n");
			continue;
		}

		// Traverse dirent entries in each positive data block
		for(j = 0; j < BLOCK_SIZE / direntSize; j++){
			memcpy(dtemp, readBuffer + (j * direntSize), direntSize);
			
			// Check if this section of the data block is in use
			if(dtemp->valid == 1){
				filler(buffer, dtemp->name, NULL, 0);
			}
		}
	}

	free(inode);
	free(dtemp);
	return 0;
}


static int tfs_mkdir(const char *path, mode_t mode) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	char *dcopy, *bcopy, *dname, *bname;
	dcopy = strdup(path);
	bcopy = strdup(path);
	dname = dirname(dcopy);
	bname = basename(bcopy);

	// Step 2: Call get_node_by_path() to get inode of parent directory
	struct inode *inode = NULL;
	int get_node_ret = get_node_by_path(dname, 0, inode);

	// Check if directory inode was returned or if it is a file
	if(get_node_ret < 0 || inode == NULL || inode->type == 0){
		printf("parent dir doesn't exist\n");
		free(inode);
		return -1;
	}

	int i, j;
	int direntSize = sizeof(struct dirent);
	char readBuffer[BLOCK_SIZE];
	struct dirent *dtemp = malloc(direntSize);

	// Traverse all datablocks, which contain dirents, of the returned inode
	// i -> [0,16) since inodes have 16 direct ptrs
	for(i = 0; i < 16; i++){
		// Skip unused ptrs
		if(inode->direct_ptr[i] < 0){
			continue;
		}
		// Read in data block - check if read correctly
		int readRet = bio_read(inode->direct_ptr[i], readBuffer);
		if(readRet < 0){
			printf("bad block read\n");
			continue;
		}

		// Traverse dirent entries in each positive data block
		for(j = 0; j < BLOCK_SIZE / direntSize; j++){
			memcpy(dtemp, readBuffer + (j * direntSize), direntSize);
			
			// Check if bname already exists in dname's directory
			if(strcmp(dtemp->name, bname) == 0){
				printf("name already exists\n");
				free(inode);
				free(dtemp);
				return -1;
			}
		}
	}

	// Step 3: Call get_avail_ino() to get an available inode number
	int get_avail_ret = get_avail_ino();

	// Step 4: Call dir_add() to add directory entry of target directory to parent directory
	int dir_add_ret = dir_add(*inode, get_avail_ret, bname, strlen(bname));
	if(dir_add_ret < 0){
		printf("couldn't add dir\n");
		return -1;
	}

	// Step 5: Update inode for target directory
	inode->ino = get_avail_ret;
	inode->type = 1;
	inode->vstat.st_mode = mode;

	// Step 6: Call writei() to write inode to disk
	writei(get_avail_ret, inode);
	free(inode);
	return 0;
}

static int tfs_rmdir(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	// Step 2: Call get_node_by_path() to get inode of target directory
	// Step 3: Clear data block bitmap of target directory
	// Step 4: Clear inode bitmap and its data block
	// Step 5: Call get_node_by_path() to get inode of parent directory
	// Step 6: Call dir_remove() to remove directory entry of target directory in its parent directory
	//int is_empty = 1;
	struct inode inode;
	get_node_by_path(path, 0, &inode);
	for(int i = 0; i < 16; i++) {
		if(inode.direct_ptr[i] != 0) {
			printf("%s %s%s\n","rm: cannot remove ", basename((char*)path),": Is a directory");
			return 1;
		}
	}
	unlink(path);

	return 0;
}

static int tfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name
	char *dcopy, *bcopy, *dname, *bname;
	dcopy = strdup(path);
	bcopy = strdup(path);
	dname = dirname(dcopy);
	bname = basename(bcopy);

	// Step 2: Call get_node_by_path() to get inode of parent directory
	struct inode *inode = NULL;
	int get_node_ret = get_node_by_path(dname, 0, inode);

	// Check if directory inode was returned
	if(get_node_ret < 0 || inode == NULL || inode->type == 0){
		printf("parent dir doesn't exist\n");
		return -1;
	}

	// Check if bname is already a file/dir in parent directory
	int i,j;
	int direntSize = sizeof(struct dirent);
	char *readBuffer[BLOCK_SIZE];
	struct dirent *dtemp = malloc(direntSize);
	for(i = 0; i < 16; i++){
		// Skip unused ptrs
		if(inode->direct_ptr[i] < 0){
			continue;
		}
		// Read in data block - check if read correctly
		int readRet = bio_read(inode->direct_ptr[i], readBuffer);
		if(readRet < 0){
			printf("bad block read\n");
			continue;
		}
		// Traverse dirent entries in each positive data block
		for(j = 0; j < BLOCK_SIZE / direntSize; j++){
			memcpy(dtemp, readBuffer + (j * direntSize), direntSize);
			
			// Check if bname already exists in dname's directory
			if(strcmp(dtemp->name, path) == 0){
				printf("name already exists\n");
				return -1;
			}
		}
	}

	// Step 3: Call get_avail_ino() to get an available inode number
	int get_avail_ret = get_avail_ino();

	// Step 4: Call dir_add() to add directory entry of target file to parent directory
	int dir_add_ret = dir_add(*inode, get_avail_ret, bname, strlen(bname));
	if(dir_add_ret < 0){
		printf("couldn't add dir\n");
		return -1;
	}
	// Step 5: Update inode for target file
	inode->ino = get_avail_ret;
	inode->type = 0;

	// Step 6: Call writei() to write inode to disk
	writei(get_avail_ret, inode);

	return 0;
}

static int tfs_open(const char *path, struct fuse_file_info *fi) {
//int get_node_by_path(const char *path, uint16_t ino, struct inode *inode)
	// Step 1: Call get_node_by_path() to get inode from path
	// Step 2: If not find, return -1
	struct inode inode;
	if(get_node_by_path(path, 0, &inode) != 0) return -1;
	if(inode.valid == 0) return -1;

	return 0;
}

static int tfs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

	// Step 1: You could call get_node_by_path() to get inode from path
	if(tfs_open(path, fi) == 0) {
		struct inode inode;
		get_node_by_path(path, 0, &inode);
		int start_block = offset/BLOCK_SIZE;
		int end_block = (offset+size)/BLOCK_SIZE;
	
		// Step 2: Based on size and offset, read its data blocks from disk
		// Step 3: copy the correct amount of data from offset to buffer
		if(size > 0) {
			 //first data block
			char *block_temp = malloc(BLOCK_SIZE);
			bio_read(inode.direct_ptr[0], block_temp);
			int copy_size, first_block_size;
			if(size > (BLOCK_SIZE - offset)) first_block_size = BLOCK_SIZE - offset;
			else first_block_size = size; //less than one block 
			memcpy(buffer, block_temp + offset, first_block_size);
			
			//middle data blocks
			for(int i = start_block + 1, j = 0; i < end_block; i++, j++) {
				bio_read(inode.direct_ptr[i], block_temp);
				memcpy(buffer + first_block_size + j*BLOCK_SIZE, block_temp, BLOCK_SIZE);
			}	

			if(start_block != end_block) {
				//final data block
				bio_read(inode.direct_ptr[0], block_temp);
				copy_size = (size - (BLOCK_SIZE - offset%BLOCK_SIZE))%BLOCK_SIZE;
				memcpy(buffer + first_block_size + (end_block-start_block)*BLOCK_SIZE, block_temp, copy_size);
			}
			return size;
		}
	}


	// Note: this function should return the amount of bytes you copied to buffer
	return 0;
}

static int tfs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: Write the correct amount of data from offset to disk

	// Step 4: Update the inode info and write it to disk

	// Step 1: You could call get_node_by_path() to get inode from path
	if(tfs_open(path, fi) == 0) {
		struct inode inode;
		get_node_by_path(path, 0, &inode);
		int start_block = offset/BLOCK_SIZE;
		int end_block = (offset+size)/BLOCK_SIZE;
		
		if(end_block > 16) return 0; //requesting too much space
		
		int to_allocate = start_block;
		for(int i = start_block; i <= end_block; i++, to_allocate++) {
			if(inode.direct_ptr[i] == 0) 
				break;
		}
		
		//must allocate more blocks for file
		while(to_allocate <= end_block) {
			inode.direct_ptr[to_allocate] = get_avail_blkno();
			to_allocate++;
		}	
		
		// Step 2: Based on size and offset, read its data blocks from disk
		// Step 3: copy the correct amount of data from offset to buffer
		if(size > 0) {
			 //first data block
			char *block_temp = malloc(BLOCK_SIZE);
			bio_read(inode.direct_ptr[0], block_temp);
			int copy_size, first_block_size;
			if(size > (BLOCK_SIZE - offset)) first_block_size = BLOCK_SIZE - offset;
			else first_block_size = size; //less than one block 
			memcpy(block_temp + offset, buffer, first_block_size);
			bio_write(start_block, block_temp);
			
			//middle data blocks
			for(int i = start_block + 1, j = 0; i < end_block; i++, j++) {
				bio_read(inode.direct_ptr[i], block_temp);
				memcpy(block_temp, buffer + first_block_size + j*BLOCK_SIZE, BLOCK_SIZE);
				bio_write(i, block_temp);
			}	

			if(start_block != end_block) {
				//final data block
				bio_read(inode.direct_ptr[0], block_temp);
				copy_size = (size - (BLOCK_SIZE - offset%BLOCK_SIZE))%BLOCK_SIZE;
				memcpy(block_temp, buffer + first_block_size + (end_block-start_block-1)*BLOCK_SIZE, copy_size);
				bio_write(end_block, block_temp);
			}
		}
	}

	// Note: this function should return the amount of bytes you write to disk
	return size;
}

static int tfs_unlink(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name
	char *dcopy, *bcopy, *dir_name, *base_name;
	dcopy = strdup(path);
	bcopy = strdup(path);
	dir_name = dirname(dcopy);
	base_name = basename(bcopy);

	// Step 2: Call get_node_by_path() to get inode of target file
	struct inode inode;
	get_node_by_path(path, 0, &inode);
	// Step 3: Clear data block bitmap of target file
	for(int i = 0; i < 16; i++) { //go through direct pointers
		if(inode.direct_ptr[i] != 0)
			unset_bitmap(dMap, inode.direct_ptr[i]);
	}
	// Step 4: Clear inode bitmap and its data block
	unset_bitmap(iMap, inode.ino); 
	struct inode inval_inode;
	inval_inode.valid = 0;
	writei(inode.ino, &inval_inode);

	// Step 5: Call get_node_by_path() to get inode of parent directory
	get_node_by_path(dir_name, 0, &inode);

	// Step 6: Call dir_remove() to remove directory entry of target file in its parent directory
	dir_remove(inode, base_name, strlen(base_name));

	return 0;
}

// For this project, you don't need to fill these functions
// But DO NOT DELETE THEM!
static int tfs_releasedir(const char *path, struct fuse_file_info *fi) {return 0;}
static int tfs_truncate(const char *path, off_t size) {return 0;}
static int tfs_release(const char *path, struct fuse_file_info *fi) {return 0;}
static int tfs_flush(const char * path, struct fuse_file_info * fi) {return 0;}
static int tfs_utimens(const char *path, const struct timespec tv[2]) {return 0;}

static struct fuse_operations tfs_ope = {
	.init		= tfs_init,
	.destroy	= tfs_destroy,

	.getattr	= tfs_getattr,
	.readdir	= tfs_readdir,
	.opendir	= tfs_opendir,
	.releasedir	= tfs_releasedir,
	.mkdir		= tfs_mkdir,
	.rmdir		= tfs_rmdir,

	.create		= tfs_create,
	.open		= tfs_open,
	.read 		= tfs_read,
	.write		= tfs_write,
	.unlink		= tfs_unlink,

	.truncate   = tfs_truncate,
	.flush      = tfs_flush,
	.utimens    = tfs_utimens,
	.release	= tfs_release
};


int main(int argc, char *argv[]) {
	int fuse_stat;

	getcwd(diskfile_path, PATH_MAX);
	strcat(diskfile_path, "/DISKFILE");

	fuse_stat = fuse_main(argc, argv, &tfs_ope, NULL);

	return fuse_stat;
}