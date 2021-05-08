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
	printf("in get_avail_ino\n");
	// Step 1: Read inode bitmap from disk

	// Step 2: Traverse inode bitmap to find an available slot
	int retVal = -1;

	for(int i = 0; i < MAX_INUM; i++) {
		if(get_bitmap(iMap, i) == 0){
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
	printf("in get_avail_blkno\n");
	// Step 1: Read data block bitmap from disk

	// Step 2: Traverse data block bitmap to find an available slot
	int retVal = -1;

	for(int i = 0; i < MAX_DNUM; i++) {
		if(get_bitmap(dMap, i) == 0){
			// Set next avail data block number
			retVal = i;

			// Update bitmap
			set_bitmap(dMap, i); 
			
			break;
		}
	}

	// Step 3: Update data block bitmap and write to disk 
	bio_write(superblock.i_bitmap_blk, dMap);
	printf("end get_avail_blkno\n");
	return retVal;
}


// *inode points to heap memory - must be malloced by caller
int readi(uint16_t ino, struct inode *inode) {

    int i, j, readRet = 0, inodeSize = sizeof(struct inode);
    struct inode *readBuf = malloc(BLOCK_SIZE);
    struct inode *tempInode = malloc(inodeSize);

    // Step 1: Get the inode's on-disk block number
    // Iterate thru all inode blocks
    // Assumes inode blocks are stored before data blocks
	printf("from %d to %d\n", superblock.i_start_blk, superblock.d_start_blk);
    for(i = superblock.i_start_blk; i < superblock.d_start_blk; i++){
        // Read contents of block i - cast for easier manipulation
        readRet = bio_read(i, readBuf);

        // On successful read, go thru all inodes in single block (max of 16)
        if(readRet > 0){
            // Step 2: Get offset of the inode in the inode on-disk block
            for(j = 0; j < BLOCK_SIZE / inodeSize; j++){
                // Extract jth inode in readBuf
                memcpy(tempInode, readBuf + j, inodeSize);

                // Step 3: Read the block from disk and then copy into inode structure
                // Compare ith inode's number to desired number
                if(tempInode->ino == ino){
                    memcpy(inode, tempInode, inodeSize);
                    free(tempInode);
                    free(readBuf);
                    return 1;
                }
            }
        }
    }
    free(tempInode);
    free(readBuf);
    return 0;
}

int writei(uint16_t ino, struct inode *inode) {
    int i, j, readRet = 0, inodeSize = sizeof(struct inode);
    struct inode *readBuf = malloc(BLOCK_SIZE);
    struct inode *tempInode = malloc(inodeSize);
	int currInode = 0;
      // Step 1: Get the inode's on-disk block number
    // Iterate thru all inode blocks
    // Assumes inode blocks are stored before data blocks
    for(i = superblock.i_start_blk; i < superblock.d_start_blk; i++){
        // Read contents of block i - cast for easier manipulation
        readRet = bio_read(i, readBuf);

        // On successful read, go thru all inodes in single block (max of 16)
        if(readRet >= 0){
            // Step 2: Get offset of the inode in the inode on-disk block
            for(j = 0; j < BLOCK_SIZE / inodeSize; j++){
                // Extract jth inode in readBuf
                memcpy(tempInode, readBuf + j, inodeSize);

                // Step 3: Read the block from disk and then copy into inode structure
                // Compare ith inode's number to desired number
                if(currInode == ino){
                    memcpy(readBuf + j, inode, inodeSize);
					bio_write(i, readBuf);
                    free(tempInode);
                    free(readBuf);
                    return 1;
                }
				currInode++;
            }
        }
    }
    free(tempInode);
    free(readBuf);
    return 0;
}

/* 
 * directory operations
 */
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {
	printf("in dir_find, searching ino %d for %s\n", ino, fname);
  // Step 1: Call readi() to get the inode using ino (inode number of current directory)
	struct inode *inode = malloc(sizeof(struct inode));
	//printf("before readi \n");
	if(readi(ino, inode) == 0) printf("failed to read in given inode\n");
	struct inode temp_node = *inode;
	//printf("inode validity is %d %d %d %d %d\n", 	temp_node.ino,	temp_node.valid,			temp_node.size,		temp_node.type,	temp_node.link	);
	
	//entries of directory minus self and parent
	int child_count = inode->link;
	//printf("child_count is %d\n", child_count);
	//number of valid entries iterated through at present
	int curr_child_count = 0;
	int dirents_per_block = BLOCK_SIZE/sizeof(struct dirent);
	struct dirent *dirents = malloc(BLOCK_SIZE);
	
	// Step 2: Get data block of current directory from inode
	// Step 3: Read directory's data block and check each directory entry.
	//If the name matches, then copy directory entry to dirent structure 
	//for each of directory's direct pointers
	for(int i = 0; (i < 16) && (curr_child_count < child_count); i++) {
		if(inode->direct_ptr[i] != -1) {
			//printf("valid direct pointer found pointing to block %d\n", inode->direct_ptr[i]);
			bio_read(inode->direct_ptr[i], dirents);
			for(int i = 0; (i < dirents_per_block) && (curr_child_count < child_count); i++) {
				//if dirent valid
				if(dirents[i].valid) {
					//printf("valid dir \"%s\" at index %d\n", dirents[i].name, i);
					if(strncmp(fname, dirents[i].name, name_len) == 0) {
						
						memcpy(dirent, dirents+i, sizeof(struct dirent));
						//printf("end find_dir, found\n");
						return 1;
					}
					curr_child_count++;
				}
			}
		}
	} 
	
	int *pointers = malloc(BLOCK_SIZE);
	int pointers_per_block = BLOCK_SIZE/sizeof(int);
	//for each of directory's indirect pointers
	for(int i = 0; (i < 8) && (curr_child_count < child_count); i++) {
		//if indirect pointer in use
		if(inode->indirect_ptr[i] != -1) {
			printf("start indirect, %d\n", (inode->indirect_ptr)[i]);
			bio_read(inode->indirect_ptr[i], pointers);
			printf("end indirect\n");
			//go through direct pointers in block 
			for(int b = 0; (b < pointers_per_block) && (curr_child_count < child_count); b++) {
				if(pointers[b] != -1) {
					bio_read(pointers[b], dirents);
					for(int i = 0; (i < dirents_per_block) && (curr_child_count < child_count); i++) {
						//if dirent valid
						if(dirents[i].valid) {
							if(strncmp(fname, dirents[i].name, name_len) == 0) {
								memcpy(dirent, &dirents[i], sizeof(struct dirent));
								printf("end dir_find, 1\n");
								fflush(stdout);
								return 1;
							}
							curr_child_count++;
						}
					}
				}
			}
		}
	}
	printf("end dir_find, -1\n");
	fflush(stdout);
	return -1;
}

//reason for duplication: if setting a pointer in the data block of an indirect pointer, must write that data block
int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {
	printf("in dir_add\n");
	if(name_len > 208) {
		printf("Name is too long-- name must be less than 208 characters.\n");
		return -1;
	}
	struct dirent dummyDirent;
	//entry with given name doesn't already exist inside directory
	//if(dir_find(dir_inode.ino, fname, name_len, &dummyDirent) == -1) {
		struct dirent *dirents = malloc(BLOCK_SIZE);
		int dirent_index = -1; //index of the dirent in the block of dirents
		int dirent_block_num = -1; //block num of the block of dirents
		int pointer_block_index = -1; //block num of block of pointers
		int pointer_index = -1; //index of pointer in block of pointers
		int found = 0;
		int unused_direct_index = -1;
		//general I/O buffer
		struct dirent *buffer = malloc(BLOCK_SIZE);
		
		//SEARCHING------------------------------------------------------------------------------------------------
		//for each of directory's direct pointers
		for(int i = 0; (!found) && (i < 16); i++) {
			//valid block of dirents found
			if(dir_inode.direct_ptr[i] != -1) {
				bio_read(dir_inode.direct_ptr[i], dirents);
				int dirents_per_block = BLOCK_SIZE/sizeof(struct dirent);
				//for each dirent in block
				for(int b = 0; b < dirents_per_block; b++) {
					//printf("b is %d\n", b);
					//if unused dirent found
					if(!dirents[b].valid) {
						dirent_block_num = dir_inode.direct_ptr[i];
						dirent_index = b;
						found = 1;
						break;
					}
				}
				// if(!found) {
				// 	printf("Not enough space in directory-- failed operation.\n");
				// 	return -1;
				// }
			}
			//store unused direct pointer-- might be needed later
			else {
				unused_direct_index = i;
			}
		} 
		
		int *pointers = (int*)buffer;
		int pointers_per_block = BLOCK_SIZE/sizeof(int);
		//for each of directory's indirect pointers
		for(int i = 0; (!found) && (i < 8); i++) {
			//if indirect pointer in use
			if(dir_inode.indirect_ptr[i] != -1) {
				bio_read(dir_inode.indirect_ptr[i], pointers);
				//go through direct pointers in pointer block 
				for(int j = 0; (!found) && (j < pointers_per_block); j++) {
					//valid block of dirents found
					if(pointers[j] != -1) {
						bio_read(pointers[j], dirents);
						int dirents_per_block = BLOCK_SIZE/sizeof(struct dirent);
						//for each dirent in block
						for(int b = 0; b < dirents_per_block; b++) {
							//if unused dirent found in preexisting dirent block
							if(!dirents[b].valid) {
								dirent_block_num = pointers[j];
								dirent_index = b;
								found = 1;
								break;
							}
						}
					}
					//store unused pointer-- might be needed later
					else {
						pointer_index = j;								//unused pointer to dirent block at this index
						pointer_block_index = dir_inode.indirect_ptr[i]; 	//in this existing block of pointers

					}
				}
			}
		}
		//VALUE SETTING---------------------------------------------------------------------------------------------------
		//free dirent in existing data block not found
		if((!found)) {
			//allocate new block of dirents
			int temp_dirent_block_num = get_avail_blkno();
			if(temp_dirent_block_num == -1) {
				printf("Not enough space in memory-- failed operation.\n");
				return -1;
			}
			dirent_block_num = temp_dirent_block_num; //store block num of block of dirents
			
			//unused direct pointer was found, so will be used
			if(unused_direct_index != -1) {
				dir_inode.direct_ptr[unused_direct_index] = dirent_block_num;
				//block of dirents added to dir_inode
				dir_inode.size += BLOCK_SIZE;
			}
			//neither unused dirent nor unused pointer to block of dirents were found
			//either there exists an unused indirect pointer or there is no free space--add failed
			else if(pointer_block_index == -1) {
				//go through indirect pointers
				for(int i = 0; (!found) && (i < 8); i++) {
					//unused indirect pointer found
					if(dir_inode.indirect_ptr[i] == -1) {
						//map indirect pointer to block of pointers
						int temp = get_avail_blkno();
						if(temp == -1) {
							printf("Not enough space in memory-- failed operation.\n");
							return -1;
						}
						//block added for new indirect pointer
						dir_inode.size += BLOCK_SIZE;
						dir_inode.indirect_ptr[i] = temp;
						//format block of unused int pointers
						memset(buffer, -1, BLOCK_SIZE);
						pointer_block_index = dir_inode.indirect_ptr[i]; //pointer to block of pointers
						pointer_index = 0; //new block, so first free pointer is at index 0
						((int*)(buffer))[pointer_index ] = dirent_block_num; //store pointer to block of dirents as first entry in block of pointers
						bio_write(dir_inode.indirect_ptr[i], buffer); //write block of pointers to memory			
						found = 1;
					}
				}
				if(!found) {
					printf("current directory has reached max capacity. failed to add more files\n");
					return -1;
				}
				//block of dirents added to dir_inode
				dir_inode.size += BLOCK_SIZE;
				

			}
			dirent_index = 0;
			
			//format new block of dirents
			struct dirent invalidDirent;
			invalidDirent.valid = 0;
			int dirents_per_block = BLOCK_SIZE/sizeof(struct dirent);
			//format full data block with invalid dirents
			for(int i = 0; i < dirents_per_block; i++) {
				memcpy(buffer+i, &invalidDirent, sizeof(struct dirent));
			}
		}
		else {
			bio_read(dirent_block_num, buffer);
		}
		//dirent for new entry
		struct dirent dirent;
		dirent.valid = 1;
		dirent.ino = f_ino;
		strncpy(dirent.name, fname, name_len+1);
		dirent.len = name_len;		
		//printf("dir_add, name is %s with length %d\n", dirent.name, dirent.len);
		//printf("passed in: dir_add, fname is %s with name_len %d\n", fname, name_len);
		
		memcpy(buffer+dirent_index, &dirent, sizeof(struct dirent)); //write dirent to block of dirents
		bio_write(dirent_block_num, buffer); //write block of dirents

		dir_inode.link++; //increase parent link count
		writei(dir_inode.ino, &dir_inode); //write parent to disk
		return 1;
	//}
	//else {
		//printf("cannot create %s: file exists\n", fname);
	//}
	//return -1;
}


int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {
	printf("in dir_remove\n");
	struct inode *inode = &dir_inode;
	//entries of directory minus self and parent
	int child_count = inode->link;
	//number of valid entries iterated through at present
	int curr_child_count = 0;
	int dirents_per_block = BLOCK_SIZE/sizeof(struct dirent);
	struct dirent *dirents = malloc(BLOCK_SIZE);
	int member_count = 0;
	int retVal = 0;
	int block_of_member = 0;
	// Step 2: Get data block of current directory from inode

	//for each of directory's direct pointers
	for(int i = 0; (i < 16) && (curr_child_count < child_count); i++) {
		if(inode->direct_ptr[i] != -1) {
			bio_read(inode->direct_ptr[i], dirents);
			for(int j = 0; (j < dirents_per_block) && (curr_child_count < child_count); j++) {
				//if dirent valid
				if(dirents[j].valid) {
					//found entry to remove
					if(strncmp(fname, dirents[j].name, name_len) == 0) {
						// Step 3: If exist, then remove it from dir_inode's data block and write to disk
						dirents[j].valid = 0;
						bio_write(dir_inode.direct_ptr[i], dirents); 
						block_of_member = i;
						retVal = 1;
					}
					member_count++;
					curr_child_count++;
				}
			}
			//member found
			if(retVal) { 
				//member to be removed is only one in data block, can deallocate data block
				if(member_count == 1) {
					//mark data block as free
					set_bitmap(dMap, dir_inode.direct_ptr[block_of_member]);
					bio_write(superblock.d_bitmap_blk, dMap);
					dir_inode.size -= BLOCK_SIZE;
					dir_inode.direct_ptr[block_of_member] = -1;
				}
				//reduce parent link count
				dir_inode.link--;
				//write parent to file
				printf("in dir_remove, 1\n");
				fflush(stdout);
				writei(dir_inode.ino, &dir_inode);
				return 1;
			}
		}
	} 
	retVal = 0;
	block_of_member = 0;
	member_count = 0;
	int *pointers = malloc(BLOCK_SIZE);
	int pointers_per_block = BLOCK_SIZE/sizeof(int);
	int indirect_index = 0;
	//for each of directory's indirect pointers
	for(int i = 0; (i < 8) && (curr_child_count < child_count); i++) {
		//if indirect pointer in use
		if(inode->indirect_ptr[i] != -1) {
			bio_read(inode->indirect_ptr[i], pointers);
			//go through direct pointers in block of pointers
			for(int b = 0; (b < pointers_per_block) && (curr_child_count < child_count); b++) {
				if(pointers[b] != -1) {
					bio_read(pointers[b], dirents);
					for(int k = 0; (k < dirents_per_block) && (curr_child_count < child_count); k++) {
						//if dirent valid
						if(dirents[k].valid) {
							if(strncmp(fname, dirents[k].name, name_len) == 0) {
								dirents[k].valid = 0;
								bio_write(pointers[b], dirents); 
								block_of_member = b;
								indirect_index = i;
								retVal = 1;
							}
							member_count++;
							curr_child_count++;
						}
						
					}
					//member found
					if(retVal) { 
						//member to be removed is only one in data block, can deallocate data block
						if(member_count == 1) {
							//mark data block as free
							set_bitmap(dMap, pointers[block_of_member]);
							bio_write(superblock.d_bitmap_blk, dMap);
							dir_inode.size -= BLOCK_SIZE;
							pointers[block_of_member] = -1;
							//write block of pointers to file
							bio_write(inode->indirect_ptr[indirect_index], pointers);
						}
						//reduce parent link count
						dir_inode.link--;
						//write parent to file
						writei(dir_inode.ino, &dir_inode);
						printf("in dir_remove, 1\n");
						fflush(stdout);
						return 1;
					}
				}
			}
		}
	}
	printf("in dir_remove, 1\n");
	fflush(stdout);
	return 0;
}

int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {
	// Get next name in path
	// Extract <next> from <next>/<nextNext>/<nextNextNext>/...
	printf("in get_node-by_path, will attempt to parse %s\n", path);
	char nextName[208], nextPath[208];
	int i = 0, nameIndex = 0, nextIndex = 0;

	// Copy chars into nextName until next delimiter or end of path
	if(path[0] == '/'){
		i++;
	}
	for(nameIndex = 0; i < 208 && path[i] != '\0' && path[i] != '/'; i++, nameIndex++){
		nextName[nameIndex] = path[i];
	}
	// End string with null terminator
	nextName[nameIndex] = '\0';

	// Extract <nextNext>/<nextNextNext>/... from /<next>/<nextNext>/<nextNextNext>/...
	if(path[i] == '/'){
		i++;
	}
	for(nextIndex = 0; i < 208 && path[i] != '\0'; i++, nextIndex++){
		nextPath[nextIndex] = path[i];
	}
	// End string with null terminator
	nextPath[nextIndex] = '\0';
	
	// Make sure path is not an empty string, should never happen
	if(i == 0){
		//printf("path is empty.\n");
		inode = NULL;
		printf("couldn't find.\n");
		return -1;
	}

	// Case where only "/" is sent in as path - should just return root directory's inode
	if(i == 1){
		// printf("only /\n");
		// return 1;
		printf("root node\n");
		return readi(0, inode);
	}

	// Find currPath's dir inside given ino's dir
	struct dirent *nextDirent = malloc(sizeof(struct dirent));
	int ret = dir_find(ino, nextName, i, nextDirent);

	// Couldn't find subdirectory/subfile name within directory number ino
	if(ret == -1 || nextDirent == NULL || nextDirent ->valid < 0){
		printf("couldn't find.\n");
		printf("ret: %d\n", ret);
		printf("nextDirent valid: %d\n", nextDirent->valid);
		inode = NULL;
		return -1;
	}
	
	// Base case: after extracting currPath's inode, check if (/)path == nextName
	if(path[0] == '/') path = path + 1;
	printf("comparing %s and %s\n", nextName, path);
	if(strcmp(nextName, path) == 0){
		printf("found match: inode %d\n", nextDirent->ino);
		int dirInodeNumber = nextDirent->ino;
		free(nextDirent);
		// readi returns -1 on fail, 1 on success
		int ret_val = readi(dirInodeNumber, inode);
		printf("read in: %d, ret_val is %d\n", inode->ino, ret_val);
		return ret_val;
	}

	// Case where nextPath is empty - reached end of path
	if(nextIndex == 0){
		printf("end of path.\n");
		return -1;
	}

	// Recurse on next segment of the path
	return get_node_by_path(nextPath, nextDirent->ino, inode);
}



/* 
 * Make file system
 */
int tfs_mkfs() {
	printf("in tfs_mkfs\n");
	// Call dev_init() to initialize (Create) Diskfile
	dev_init(diskfile_path);

	// write superblock information
	superblock.magic_num = MAGIC_NUM;	/* magic number */
	superblock.max_inum = MAX_INUM;		/* maximum inode number */
	superblock.max_dnum = MAX_DNUM;		/* maximum data block number */
	superblock.i_bitmap_blk = 1;		/* start block of inode bitmap */
	superblock.d_bitmap_blk = 2;		/* start block of data block bitmap */
	superblock.i_start_blk =  3;		/* start block of inode region */
	
	/* start block of data block region */
	superblock.d_start_blk =  superblock.i_start_blk + (MAX_INUM*sizeof(struct inode))/BLOCK_SIZE;		
	if((MAX_INUM*sizeof(struct inode))%BLOCK_SIZE != 0) superblock.d_start_blk++;
	
	// initialize inode bitmap
	iMap = malloc(BLOCK_SIZE);
	memset(iMap, 0, MAX_INUM/8);
	
	// initialize data block bitmap
	dMap = malloc(BLOCK_SIZE);
	memset(dMap, 0, MAX_DNUM/8);
	
	//intialize non-datablock areas
	for(int i = 0; i < superblock.d_start_blk; i++) {
		set_bitmap(dMap, i);
	}

	// update bitmap information for root directory
	set_bitmap(iMap, 0);

	// update inode for root directory
	struct inode temp_node;
	temp_node.ino = 0;			/* inode number */
	temp_node.valid = 1;			/* validity of the inode */
	temp_node.size = BLOCK_SIZE;			/* size of the file */
	temp_node.type = 1;			/* type of the file */
	temp_node.link = 2;			/* link count */
	temp_node.direct_ptr[0] = get_avail_blkno();
	printf("direct ptr 0 is %d\n", temp_node.direct_ptr[0]);
	struct dirent temp_dirent;
	temp_dirent.valid = 0;
	struct dirent *buffer = malloc(BLOCK_SIZE);
	int dirents_per_block = BLOCK_SIZE/(sizeof(struct dirent));
	for(int i = 2; i < dirents_per_block; i++) {
		memcpy(buffer+i, &temp_dirent, sizeof(struct dirent));
		//buffer[i] = temp_dirent;
	}
	temp_dirent.valid = 1;
	strcpy(temp_dirent.name, ".");
	temp_dirent.len = 1;
	temp_dirent.ino = 0;
	//buffer[0] = temp_dirent;
	memcpy(buffer, &temp_dirent, sizeof(struct dirent));
	
	strcpy(temp_dirent.name, "..");
	temp_dirent.len = 2;
	temp_dirent.ino = 0;
	//buffer[1] = temp_dirent;
	memcpy(buffer+1, &temp_dirent, sizeof(struct dirent));
	
	bio_write(temp_node.direct_ptr[0], buffer);
	
	for(int i = 1; i < 16; i++) { /* direct pointer to data block */
		temp_node.direct_ptr[i] = -1;
	}	
	for(int i = 0; i < 8; i++) { /* indirect pointer to data block */
		temp_node.indirect_ptr[i] = -1;
	}	
	//write superblock
	bio_write(0, &superblock);
	//write inode map
	bio_write(1, iMap);	
	//write dnode map
	bio_write(2, dMap); 
	//write root inode
	bio_write(superblock.i_start_blk, &temp_node);
	printf("end mkfs\n");
	return 0;
}


/* 
 * FUSE file operations
 */
static void *tfs_init(struct fuse_conn_info *conn) {
	printf("in tfs_init\n");
	// Step 1a: If disk file is not found, call mkfs
	if(access(diskfile_path, F_OK) != 0)
		tfs_mkfs();
  // Step 1b: If disk file is found, just initialize in-memory data structures
  // and read superblock from disk
	else {
		//open file
		dev_open(diskfile_path); 
		printf("superblock\n");
		//read in superblock
		char* temp = malloc(BLOCK_SIZE);
		bio_read(0, temp);
		superblock = *(struct superblock*)temp;
		
		printf("imap\n");
		//read in iMap
		temp = malloc(BLOCK_SIZE);
		bio_read(superblock.i_bitmap_blk, temp);
		iMap = (bitmap_t)temp;
		
		printf("dmap\n");
		//read in dMap
		temp = malloc(BLOCK_SIZE);
		bio_read(superblock.d_bitmap_blk, temp);
		dMap = (bitmap_t)temp;
	}
	printf("end tfs_init\n");
	return NULL;
}

static void tfs_destroy(void *userdata) {
	printf("in tfs_destroy\n");
	// Step 1: De-allocate in-memory data structures
	
	// Free the bitmaps
	free(iMap);
	free(dMap);

	
	// Step 2: Close diskfile
	dev_close();
	printf("end tfs_destroy\n");
}

static int tfs_getattr(const char *path, struct stat *stbuf) {
	printf("in tfs_getattr, path is %s\n", path);
	// Step 1: call get_node_by_path() to get inode from path
	struct inode* inode = malloc(sizeof(struct inode));
	int ret = get_node_by_path(path, 0, inode);
	int retVal = 0;
	// Step 2: fill attribute of file into stbuf from inode

//	stbuf->st_mode   = S_IFDIR | 0755;
	//stbuf->st_mode = S_IFREG | 0644;
//	stbuf->st_nlink  = 2;
	//time(&stbuf->st_mtime);

	// must right error code on fail - returns "no such file or directory found"
	if(ret == -1){
		printf("creating new file\n");
		return -ENOENT;
	}
	else {
		printf("not creating new file, ret is %d\n", ret);
		//printf("inode is %d\n", inode->ino);
	}

	// Step 2: fill attribute of file into stbuf from inode
	// st_dev
	if(inode->type == 0) //file
		stbuf->st_mode = S_IFREG | 0644;
	else //dir
		stbuf->st_mode   = S_IFDIR | 0755;

	
	stbuf->st_ino 	 = inode->ino;
	//stbuf->st_mode   = inode->vstat.st_mode;
	stbuf->st_nlink  = inode->link;
	stbuf->st_uid 	 = getuid();
	stbuf->st_gid	 = getgid();
	// st_rdev
	stbuf->st_size	 = inode->size;
	stbuf->st_blksize = BLOCK_SIZE;
	
	time(&stbuf->st_atime);
	time(&stbuf->st_mtime);
	printf("end tfs_getattr\n");
	// stbuf->st_ctim?
	return retVal;
}

static int tfs_opendir(const char *path, struct fuse_file_info *fi) {
	printf("in tfs_opendir\n");
	// Step 1: Call get_node_by_path() to get inode from path
	struct inode *inode = malloc(sizeof(struct inode));
	// Step 2: If not find, return -1
	if(get_node_by_path(path, 0, inode) == 1){
		return 0;
	}
	return -1;
}

static int tfs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
	printf("in tfs_readdir\n");
	// Step 1: Call get_node_by_path() to get inode from path
	struct inode base_node;
	struct inode *inode = &base_node;
    int ret = get_node_by_path(path, 0, inode);
	// Fail if path doesn't exist
	if(ret == -1){
		return -ENOENT;
	}
	// Step 2: Read directory entries from its data blocks, and copy them to filler

	// If inode pointed to by path is a file, break
	if(inode->type == 0){
		printf("Input a file when expected directory.\n");
		return -ENOENT;
	}

	// Continue as normal if given a directory
	int *datablocks = inode->direct_ptr;
	int currBlockNum;
	int i, j;
	struct dirent *readBuf = malloc(BLOCK_SIZE);
	struct dirent *currDirent = malloc(BLOCK_SIZE);
	int direntSize = sizeof(struct dirent);
	// Traverse all datablocks of dirents of given inode
	for(i = 0; i < 16; i++){
		currBlockNum = datablocks[i];
		// Check if inode uses the ith block
		if(currBlockNum != -1) {
			bio_read(currBlockNum, readBuf);
			// Traverse all 16 possible dirent structs in current block
			for(j = 0; j < BLOCK_SIZE / direntSize; j++){
				memcpy(currDirent, readBuf + j, direntSize);
				currDirent = (struct dirent*)currDirent;

				// Check if this section of the data block is in use
				if(currDirent->valid == 0){
					continue;
				}
				filler(buffer, currDirent->name, NULL, 0);
			}
		}
	}
	int *pointers = (int*)buffer;
	int pointers_per_block = BLOCK_SIZE/sizeof(int);
	//for each of directory's indirect pointers
	for(int i = 0; i < 8; i++) {
		//if indirect pointer in use
		if(inode->indirect_ptr[i] != -1) {
			bio_read(inode->indirect_ptr[i], pointers);
			//go through direct pointers in pointer block 
			for(int j = 0; j < pointers_per_block; j++) {
				//valid block of dirents found
				if(pointers[j] != -1) {
					bio_read(pointers[j], readBuf);
					// Traverse all 16 possible dirent structs in current block
					for(j = 0; j < BLOCK_SIZE / direntSize; j++){
						memcpy(currDirent, readBuf + j, direntSize);
						currDirent = (struct dirent*)currDirent;

						// Check if this section of the data block is in use
						if(currDirent->valid == 0){
							continue;
						}
						filler(buffer, currDirent->name, NULL, 0);
					}
				}
			}
		}
	}
	printf("end readdir\n");
	return 0;
}


static int tfs_mkdir(const char *path, mode_t mode) {
	printf("in tfs_mkdir\n");
	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	char *dcopy, *bcopy, *dname, *bname;
	dcopy = strdup(path);
	bcopy = strdup(path);
	dname = dirname(dcopy);
	bname = basename(bcopy);

	// Step 2: Call get_node_by_path() to get inode of parent directory
	struct inode *inode = malloc(sizeof(struct inode));
	int get_node_ret = get_node_by_path(dname, 0, inode);

	// Check if directory inode was returned
	if(get_node_ret < 0 || inode == NULL || inode->type == 0){
		printf("parent dir doesn't exist\n");
		return -1;
	}

	struct dirent dummy_dirent;
	if(dir_find(inode->ino, bname, strlen(bname), &dummy_dirent) == 1) {
		printf("name already exists\n");
		return -1;
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
	int parent_ino = inode->ino;
	inode->ino = get_avail_ret;
	inode->type = 1;
	inode->link = 2; 
	inode->direct_ptr[0] = get_avail_blkno();
	struct dirent temp_dirent;
	temp_dirent.valid = 0;
	struct dirent *buffer = malloc(BLOCK_SIZE);
	//format block with invalid dirents
	int dirents_per_block = BLOCK_SIZE/(sizeof(struct dirent));
	for(int i = 2; i < dirents_per_block; i++) {
		buffer[i] = temp_dirent;
	}
	temp_dirent.valid = 1;
	strcpy(temp_dirent.name, ".");
	temp_dirent.len = 1;
	temp_dirent.ino = inode->ino;
	buffer[0] = temp_dirent;
	
	strcpy(temp_dirent.name, "..");
	temp_dirent.len = 2;
	temp_dirent.ino = parent_ino;
	buffer[1] = temp_dirent;
	
	//write first data block to disk
	bio_write(inode->direct_ptr[0], buffer);

	// Step 6: Call writei() to write inode to disk
	writei(get_avail_ret, inode);

	free(inode);

	return 0;
}
static int tfs_unlink(const char *path) {
	printf("in tfs_unlink\n");
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
	//go through direct pointers
	for(int i = 0; i < 16; i++) { 
		if(inode.direct_ptr[i] != 0)
			unset_bitmap(dMap, inode.direct_ptr[i]);
	}
	int *pointers = malloc(BLOCK_SIZE);
	int pointers_per_block = BLOCK_SIZE/sizeof(int);
	//for each of directory's indirect pointers
	for(int i = 0; (i < 8); i++) {
		if(inode.indirect_ptr[i] != -1) {
			bio_read(inode.indirect_ptr[i], pointers);
			//go through direct pointers in block 
			for(int b = 0; b < pointers_per_block; b++) {
				if(pointers[b] != -1) {
					unset_bitmap(dMap, pointers[b]);
				}
			}
			
		}
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
static int tfs_rmdir(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	// Step 2: Call get_node_by_path() to get inode of target directory
	// Step 3: Clear data block bitmap of target directory
	// Step 4: Clear inode bitmap and its data block
	// Step 5: Call get_node_by_path() to get inode of parent directory
	// Step 6: Call dir_remove() to remove directory entry of target directory in its parent directory
	printf("in tfs_rmdir\n");
	struct inode inode;
	get_node_by_path(path, 0, &inode);

	if(inode.link > 2) { //2 links for self and parent, further signifies items in directory
		printf("%s %s%s\n","rfs_rmdir: failed to remove ", basename((char*)path),": Directory not empty");
		return 1;
	}
	tfs_unlink(path);
	return 0;
}

static int tfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
	printf("in tfs_create\n");
	// Step 1: Use dirname() and basename() to separate parent directory path and target file name
	char *dcopy, *bcopy, *dname, *bname;
	dcopy = strdup(path);
	bcopy = strdup(path);
	dname = dirname(dcopy);
	bname = basename(bcopy);

	// Step 2: Call get_node_by_path() to get inode of parent directory
	struct inode *inode = malloc(sizeof(struct inode));
	int get_node_ret = get_node_by_path(dname, 0, inode);
	//printf("get_node_ret %d\n", get_node_ret);
	//printf("inode->type %d\n", inode->type);
	// Check if directory inode was returned
	if(get_node_ret < 0 || inode == NULL || inode->type == 0){
		printf("parent dir doesn't exist\n");
		return -1;
	}

	struct dirent dummy_dirent;
	if(dir_find(inode->ino, bname, strlen(bname), &dummy_dirent) == 1) {
		printf("name already exists\n");
		return -1;
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
	inode->size = 0;
	for(int i = 0; i < 16; i++) { /* direct pointer to data block */
		inode->direct_ptr[i] = -1;
	}	
	for(int i = 0; i < 8; i++) { /* indirect pointer to data block */
		inode->indirect_ptr[i] = -1;
	}	
	

	// Step 6: Call writei() to write inode to disk
	writei(get_avail_ret, inode);
	printf("tfs_create: created file at inode %d\n", get_avail_ret);
	struct inode *tempnode = malloc(sizeof(struct inode));
	if(readi(get_avail_ret, tempnode) == 0)
		printf("failed to read in\n");
	else 
		printf("tfs_create: in memory, inode num is %d\n", tempnode->ino);
	
	return 0;
}

static int tfs_open(const char *path, struct fuse_file_info *fi) {
	printf("in tfs_open\n");
//int get_node_by_path(const char *path, uint16_t ino, struct inode *inode)
	// Step 1: Call get_node_by_path() to get inode from path
	// Step 2: If not find, return -1
	struct inode inode;
	if(get_node_by_path(path, 0, &inode) != 1) return -1;
	if(inode.valid == 0) return -1;



	return 0;
}

static int tfs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	printf("in tfs_read\n");
	// Step 1: You could call get_node_by_path() to get inode from path
	int block_num = 0;
	if(size < 0) {
		printf("must write in positive number of bytes\n");
		return -1;
	}
	if(tfs_open(path, fi) == 0) {
		struct inode *inode = malloc(sizeof(struct inode));
		get_node_by_path(path, 0, inode);
		int start_block = offset/BLOCK_SIZE;
		int end_block = (offset+size)/BLOCK_SIZE;
		
		int first_block_size = 0;
		char *block_temp = malloc(BLOCK_SIZE);
		for(int i = 0; (i < 16) && (block_num <= end_block); i++) {
			//if within blocks to read from
			if((block_num >= start_block) && (block_num <= end_block)) {
				//if pointer in use
				if(inode->direct_ptr[i] != -1) {
					bio_read(inode->direct_ptr[i], block_temp);
					if(block_num == start_block) {								
						if(size > (BLOCK_SIZE - offset%BLOCK_SIZE)) 
							first_block_size = BLOCK_SIZE - offset%BLOCK_SIZE;
						else 
							first_block_size = size; //less than one block 
						
						memcpy(buffer, block_temp + offset%BLOCK_SIZE, first_block_size);
					}
					else if(block_num == end_block) {
						int end_block_size = (offset+size)%BLOCK_SIZE;
						memcpy(buffer + first_block_size + (end_block - start_block-1)*BLOCK_SIZE, block_temp, end_block_size);				
					}
					//intermediate block
					else {
						memcpy(buffer + first_block_size + (block_num - start_block-1)*BLOCK_SIZE, block_temp, BLOCK_SIZE);
					}
				}
				else {
					printf("read failed-- bytes not in use\n");
				}
			}
			block_num++;
		} 

		int *pointers = malloc(BLOCK_SIZE);
		int pointers_per_block = BLOCK_SIZE/sizeof(int);
		//for each of directory's indirect pointers
		for(int i = 0; (i < 8) && (block_num <= end_block); i++) {
			if((block_num >= start_block) && (block_num <= end_block)) {
				//if indirect pointer not in use, must allocate
				if(inode->indirect_ptr[i] != -1) {
					bio_read(inode->indirect_ptr[i], pointers);
					//go through direct pointers in block 
					for(int b = 0; (b < pointers_per_block) && (block_num <= end_block); b++) {
						//if within blocks to write to
						if((block_num >= start_block) && (block_num <= end_block)) {
							if(pointers[b] != -1) {
								bio_read(pointers[b], block_temp);
								if(block_num == start_block) {								
									if(size > (BLOCK_SIZE - offset%BLOCK_SIZE)) 
										first_block_size = BLOCK_SIZE - offset%BLOCK_SIZE;
									else 
										first_block_size = size; //less than one block 
									
									memcpy(block_temp + offset%BLOCK_SIZE, buffer, first_block_size);
								}
								else if(block_num == end_block) {
									int end_block_size = (offset+size)%BLOCK_SIZE;
									memcpy(block_temp, buffer + first_block_size + (end_block - start_block-1)*BLOCK_SIZE, end_block_size);				
								}
								//intermediate block
								else {
									memcpy(block_temp, buffer + first_block_size + (block_num - start_block-1)*BLOCK_SIZE, BLOCK_SIZE);
								}
							}
						}
						block_num++;
					}
				}
			}
		}

		// Note: this function should return the amount of bytes you write to disk
		return size;
	}
	printf("file not found\n");
	return 0;
}

static int tfs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: Write the correct amount of data from offset to disk

	// Step 4: Update the inode info and write it to disk

	//printf("in tfs_write, attempting to write %u bytes to offset %u path %s\n", size, offset, path);
	int block_num = 0;
	if(size < 0) {
		printf("must write in positive number of bytes\n");
		return -1;
	}
	if(tfs_open(path, fi) == 0) {
		struct inode *inode = malloc(sizeof(struct inode));
		get_node_by_path(path, 0, inode);
		printf("in tfs_write, attempting to write %u bytes to offset %u to inode %d\n", size, offset, inode->ino);
		int start_block = offset/BLOCK_SIZE;
		int end_block = (offset+size)/BLOCK_SIZE;
		//printf("start_block: %d end_block: %d\n", start_block, end_block);
		
		//16 direct, 8 indirect pointers
		if(end_block > (16 + 8*(BLOCK_SIZE/sizeof(int)))) return 0; //requesting too much space or offset is too large
		
		// Step 2: Based on size and offset, read its data blocks from disk
		// Step 3: copy the correct amount of data from offset to buffer
		int first_block_size = 0;
		int end_block_size = (offset+size)%BLOCK_SIZE;
		if(end_block_size == 0) end_block--;
							
		char *block_temp = malloc(BLOCK_SIZE);
		for(int i = 0; (i < 16) && (block_num <= end_block); i++) {
			//printf("block %d\n", i);
			//if within blocks to write to
			if((block_num >= start_block) && (block_num <= end_block)) {
				if(inode->direct_ptr[i] == -1) {
					int temp = get_avail_blkno();
					if(temp > 0) {
						inode->direct_ptr[i] = temp;
						inode->size += BLOCK_SIZE;
						
					}
					else {
						printf("write failed-- not enough space\n");
						free(block_temp);
						return 0;
					}
				}
				else {
					printf("inode->direct_ptr[%d] is %d\n", i, inode->direct_ptr[i]);
				}
				bio_read(inode->direct_ptr[i], block_temp);
				if(block_num == start_block) {								
					if(size > (BLOCK_SIZE - offset%BLOCK_SIZE)) 
						first_block_size = BLOCK_SIZE - offset%BLOCK_SIZE;
					else 
						first_block_size = size; //less than one block 
					//printf("first_block_size is %d\n", first_block_size);
					memcpy(block_temp + offset%BLOCK_SIZE, buffer, first_block_size);
				}
				else if(block_num == end_block) {
					memcpy(block_temp, buffer + first_block_size + (end_block - start_block-1)*BLOCK_SIZE, end_block_size);		
					//printf("end_block_size is %d\n", end_block_size);					
				}
				//intermediate block
				else {
					memcpy(block_temp, buffer + first_block_size + (block_num - start_block-1)*BLOCK_SIZE, BLOCK_SIZE);
				}
				bio_write(inode->direct_ptr[i], block_temp);
			}
			block_num++;
		} 

		int *pointers = malloc(BLOCK_SIZE);
		int pointers_per_block = BLOCK_SIZE/sizeof(int);
		//for each of directory's indirect pointers
		for(int i = 0; (i < 8) && (block_num <= end_block); i++) {
			if((block_num >= start_block) && (block_num <= end_block)) {
				//if indirect pointer not in use, must allocate
				if(inode->indirect_ptr[i] == -1) {
					int temp = get_avail_blkno();
					if(temp > 0) {
						inode->indirect_ptr[i] = temp;
						inode->size += BLOCK_SIZE;
						bio_read(inode->indirect_ptr[i], pointers);
						//set block of pointers to unused pointers
						memset(pointers, -1, BLOCK_SIZE);
						bio_write(inode->indirect_ptr[i], pointers);
					}
					else {
						printf("write failed-- not enough space\n");
						free(block_temp);
						return 0;
					}
				}
				bio_read(inode->indirect_ptr[i], pointers);
				//go through direct pointers in block 
				for(int b = 0; (b < pointers_per_block) && (block_num <= end_block); b++) {
					//if within blocks to write to
					if((block_num >= start_block) && (block_num <= end_block)) {
						if(pointers[b] == -1) {
							int temp = get_avail_blkno();
							if(temp > 0) {
								pointers[b] = temp;
								//write block of pointers
								bio_write(inode->indirect_ptr[i], pointers);
							}
							else {
								printf("write failed-- not enough space\n");
								free(block_temp);
								return 0;
							}
						}
						
						bio_read(pointers[b], block_temp);
						if(block_num == start_block) {								
							if(size > (BLOCK_SIZE - offset%BLOCK_SIZE)) 
								first_block_size = BLOCK_SIZE - offset%BLOCK_SIZE;
							else 
								first_block_size = size; //less than one block 
							
							memcpy(block_temp + offset%BLOCK_SIZE, buffer, first_block_size);
						}
						else if(block_num == end_block) {
							int end_block_size = (offset+size)%BLOCK_SIZE;
							memcpy(block_temp, buffer + first_block_size + (end_block - start_block-1)*BLOCK_SIZE, end_block_size);				
						}
						//intermediate block
						else {
							memcpy(block_temp, buffer + first_block_size + (block_num - start_block-1)*BLOCK_SIZE, BLOCK_SIZE);
						}
						bio_write(pointers[b], block_temp);
					}
					block_num++;
				}
			}
		}

		// Note: this function should return the amount of bytes you write to disk
		writei(inode->ino, inode); 
		printf("\ninode->size is %d\n\n", inode->size);
		return size;
	}
	printf("file not found\n");
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