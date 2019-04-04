/*
 * File system size: 1MB
 * Block size: 512B
 *
 * Block 0: directory (only one directory in file system)
 * Block 1 - 8: FAT
 * Block 9 - 2047: for files
 *
 * First directory entry: free space
 *
 * Only one opened file allowed
 * Can only write to the end of file
 *
 * No error handling implemented; just return EFILE_FAILURE in the case of any failure
 */

#include "eFile.h"
#include "eDisk.h"
#include "string.h"
#include "OS.h"
#include "Serial.h"
#include "LED.h"

typedef WORD block_t;
typedef DWORD file_len_t;

#define BLOCK_SIZE 	 		512 	   // in byte
#define FS_SIZE 			2048	   // in number of blocks
#define BLOCK_NUMBER_BYTES	2
#define DIR_SIZE			1		   // in number of blocks
#define DIR_ENTRY_SIZE 		32  	   // in byte
#define FAT_SIZE 			(BLOCK_NUMBER_BYTES * FS_SIZE / BLOCK_SIZE)  // in number of blocks
#define FAT_START_BLOCK		DIR_SIZE
#define FAT_END_BLOCK		FAT_SIZE
#define FIRST_FILE_BLOCK 	(DIR_SIZE + FAT_SIZE)
#define MAX_FILE_NUM   		12

/* The directory entry stored in disk */
typedef struct {
	block_t  firstBlock;
	BYTE  name[15];
	file_len_t fileSize;    // in byte
} DirEntry;

/* The file object created after opened*/
typedef struct {
	DirEntry *entry;
	BYTE data[BLOCK_SIZE];  // one block in memory at most
	block_t curBlock;		// currently opened block, if any
	file_len_t pos;         // current cursor position (the next to read/write)
	int rw;     		    // 0 for read, 1 for write
} File;


struct __directory {
	DirEntry entries[MAX_FILE_NUM];
	block_t freeSpace;  // first block in free space link
	BYTE fileNum;
};

struct __directory directory;   // have to make sure this is under DIR_SIZE * BLOCK_SIZE

BYTE fat[FS_SIZE];  // zero mean no next block

File openedFile;  // only one opened file allowed
int fileOpened = 0;

// return 0 if no free block
static block_t allocateBlock(void) {
	if (directory.freeSpace == 0) return 0;

	block_t temp = directory.freeSpace;
	directory.freeSpace = fat[temp];
	fat[temp] = 0;
	return temp;
}

/* Public Functions */
/**
 * @details This function must be called first, before calling any of the other eFile functions
 * @param  none
 * @return 0 if successful and 1 on failure (already initialized)
 * @brief  Activate the file system, without formating
 */
FRESULT eFile_Init(void) {
	DRESULT res = eDisk_Init(0);
	if (res != RES_OK) return EFILE_FAILURE;

	// read directory and FAT
	BYTE * pt = (BYTE *) &directory;
	for (unsigned int i = 0; i < DIR_SIZE; i++) {
		DRESULT res = eDisk_ReadBlock(pt, i);
		if (res != RES_OK) return EFILE_FAILURE;
		pt += BLOCK_SIZE;
	}
	pt = fat;
	for (unsigned int i = FAT_START_BLOCK; i <= FAT_END_BLOCK; i++) {
		DRESULT res = eDisk_ReadBlock(pt, i);
		if (res != RES_OK) return EFILE_FAILURE;
		pt += BLOCK_SIZE;
	}

	return EFILE_SUCCESS;
}


/**
 * @details Erase all files, create blank directory, initialize free space manager
 * @param  none
 * @return 0 if successful and 1 on failure (e.g., trouble writing to flash)
 * @brief  Format the disk
 */
FRESULT eFile_Format(void) {
	// reset opened file tracking variables
	fileOpened = 0;

	// directory and FAT initialization
	memset(&directory, 0, sizeof (directory));
	memset(fat, 0, sizeof (fat));

	directory.freeSpace = FIRST_FILE_BLOCK;
	for (int i = FIRST_FILE_BLOCK; i < FS_SIZE - 1; i++) {
		fat[i] = i+1;
	}

	// write directory to disk
	BYTE * pt = (BYTE *) &directory;
	for (int i = 0; i < DIR_SIZE; i++) {
		DRESULT res = eDisk_WriteBlock(pt, i);
		if (res != RES_OK) return EFILE_FAILURE;
		pt += BLOCK_SIZE;
	}

	// write FAT to disk
	pt = fat;
	for (int i = FAT_START_BLOCK; i <= FAT_END_BLOCK; i++) {
		DRESULT res = eDisk_WriteBlock(pt, i);
		if (res != RES_OK) return EFILE_FAILURE;
		pt += BLOCK_SIZE;
	}

	// this may not be necessary
	BYTE zeros[BLOCK_SIZE] = {0};
	for (int i = FIRST_FILE_BLOCK; i < FS_SIZE; i++) {
		DRESULT res = eDisk_WriteBlock(zeros, i);
		if (res != RES_OK) return EFILE_FAILURE;
	}

	return EFILE_SUCCESS;
}

/**
 * @details Create a new, empty file with one allocated block
 * @param  name file name is an ASCII string up to seven characters
 * @return 0 if successful and 1 on failure (e.g., already exists)
 * @brief  Create a new file
 */
FRESULT eFile_Create(char name[]) {
	// directory full

	if (directory.fileNum == MAX_FILE_NUM)
		return EFILE_FAILURE;

	// file name already exists
	for (int i = 0; i < MAX_FILE_NUM; i++) {
		if (directory.entries[i].firstBlock != 0 && strcmp(name, directory.entries[i].name) == 0) {
			return EFILE_FAILURE;
		}
	}

	int in = 0;
	while (in++ < MAX_FILE_NUM) {
		if (directory.entries[in].firstBlock == 0) {
			block_t bl = allocateBlock();
			if (bl == 0) return EFILE_FAILURE; // no more free space
			directory.entries[in].firstBlock = bl;
			directory.entries[in].fileSize = 0;
			strcpy(directory.entries[in].name, name);
			directory.fileNum++;
			break;
		}
	}

	return EFILE_SUCCESS;
}


/**
 * @details Delete the file with this name, recover blocks so they can be used by another file
 * @param  name file name is an ASCII string up to seven characters
 * @return 0 if successful and 1 on failure (e.g., file doesn't exist)
 * @brief  delete this file
 */
FRESULT eFile_Delete(char name[]) {
	for (int i = 0; i < MAX_FILE_NUM; i++) {
		if (directory.entries[i].firstBlock != 0 && strcmp(name, directory.entries[i].name) == 0) {
			if (fileOpened && openedFile.entry == &directory.entries[i])
				fileOpened = 0;

			// update directory and FAT
			block_t b;
			b = directory.entries[i].firstBlock;
			directory.entries[i].firstBlock = 0;
			do {
				block_t cb = b;
				b = fat[cb];
				fat[cb] = directory.freeSpace;
				directory.freeSpace = cb;
			} while (b != 0);
			directory.fileNum--;

			// write directory to disk
			BYTE * pt = (BYTE *) &directory;
			for (int i = 0; i < DIR_SIZE; i++) {
				DRESULT res = eDisk_WriteBlock(pt, i);
				if (res != RES_OK) return EFILE_FAILURE;
				pt += BLOCK_SIZE;
			}

			// write FAT to disk
			pt = fat;
			for (int i = FAT_START_BLOCK; i <= FAT_END_BLOCK; i++) {
				DRESULT res = eDisk_WriteBlock(pt, i);
				if (res != RES_OK) return EFILE_FAILURE;
				pt += BLOCK_SIZE;
			}

			return EFILE_SUCCESS;
		}
	}
	return EFILE_FAILURE;
}


/**
 * @details Open the file for reading, read first block into RAM
 * @param  name file name is an ASCII string up to seven characters
 * @return 0 if successful and 1 on failure (e.g., trouble reading from flash)
 * @brief  Open an existing file for reading
 */
FRESULT eFile_ROpen(char name[]) {
	// if a file currently opened, fail
	if (fileOpened) return EFILE_FAILURE;

	for (int i = 0; i < MAX_FILE_NUM; i++) {
		if (directory.entries[i].firstBlock != 0 && strcmp(name, directory.entries[i].name) == 0) {
			openedFile.entry = &directory.entries[i];
			openedFile.pos = 0;
			openedFile.rw = 0;
			openedFile.curBlock = openedFile.entry->firstBlock;
			fileOpened = 1;
			DRESULT res = eDisk_ReadBlock((BYTE *) &openedFile.data, openedFile.curBlock);
			if (res != RES_OK) {
				fileOpened = 0;
				return EFILE_FAILURE;
			}
			break;
		}
	}
	// not existing file matches the name
	if (fileOpened == 0) return EFILE_FAILURE;

	return EFILE_SUCCESS;
}


/**
 * @details Read one byte from disk into RAM
 * @param  pt call by reference pointer to place to save data
 * @return 0 if successful and 1 on failure (e.g., trouble reading from flash)
 * @brief  Retreive data from open file
 */
FRESULT eFile_ReadNext(char *pt) {
	if (!fileOpened || openedFile.rw) return EFILE_FAILURE;

	// must include "equal" here since pos starts with 0
	if (openedFile.pos >= openedFile.entry->fileSize) return EFILE_FAILURE;
	*pt = openedFile.data[openedFile.pos % BLOCK_SIZE];

	// increment pos and switch block
	if ((++openedFile.pos % BLOCK_SIZE) == 0) {
		openedFile.curBlock = fat[openedFile.curBlock];
		if (openedFile.curBlock != 0) {  // 0 means already last block
			DRESULT res = eDisk_ReadBlock((BYTE *) &openedFile.data, openedFile.curBlock);
			if (res != RES_OK) {
				fileOpened = 0;
				return EFILE_FAILURE;
			}
		}
	}

	return EFILE_SUCCESS;
}


/**
 * @details Open the file for writing, read into RAM last block
 * @param  name file name is an ASCII string up to seven characters
 * @return 0 if successful and 1 on failure (e.g., trouble reading from flash)
 * @brief  Open an existing file for writing
 */
FRESULT eFile_WOpen(char name[]) {
	// if a file currently opened, fail
	if (fileOpened) return EFILE_FAILURE;

	for (int i = 0; i < MAX_FILE_NUM; i++) {
		if (directory.entries[i].firstBlock != 0 && strcmp(name, directory.entries[i].name) == 0) {
			openedFile.entry = &directory.entries[i];
			openedFile.pos = openedFile.entry->fileSize;
			openedFile.rw = 1;
			fileOpened = 1;

			block_t endBlock = openedFile.entry->firstBlock;

			int cur = BLOCK_SIZE;
			while (cur < openedFile.pos) {
				endBlock = fat[endBlock];
				cur += BLOCK_SIZE;
			}

			DRESULT res = eDisk_ReadBlock((BYTE *) &openedFile.data, endBlock);
			if (res != RES_OK) {
				fileOpened = 0;
				return EFILE_FAILURE;
			}
			openedFile.curBlock = endBlock;
			break;
		}
	}
	// not existing file matches the name
	if (fileOpened == 0) return EFILE_FAILURE;

	return EFILE_SUCCESS;
}


/**
 * @details Save one byte at end of the open file
 * @param  data byte to be saved on the disk
 * @return 0 if successful and 1 on failure (e.g., trouble writing to flash)
 * @brief  Format the disk
 */
FRESULT eFile_Write(char data) {
	if (!fileOpened || !openedFile.rw) return EFILE_FAILURE;

	openedFile.data[(openedFile.pos % BLOCK_SIZE)] = data;

	if ((++openedFile.pos % BLOCK_SIZE) == 0) {
		DRESULT res = eDisk_WriteBlock((BYTE *) &openedFile.data, openedFile.curBlock);
		if (res != RES_OK) {
			fileOpened = 0;
			return EFILE_FAILURE;
		}
		// allocate new block
		block_t b = allocateBlock();

		if (b == 0) {
			fileOpened = 0;
			return EFILE_FAILURE;
		}
		fat[openedFile.curBlock] = b;

//		memset(&openedFile.data, 0, sizeof (openedFile.data));
		res = eDisk_ReadBlock((BYTE *) &openedFile.data, b);
		if (res != RES_OK) {
			fileOpened = 0;
			return EFILE_FAILURE;
		}
		openedFile.curBlock = b;
	}

	openedFile.entry->fileSize++;

	return EFILE_SUCCESS;
}

/**
 * @details Close the file, leave disk in a state power can be removed.
 * This function will flush all RAM buffers to the disk.
 * @param  none
 * @return 0 if successful and 1 on failure (e.g., trouble writing to flash)
 * @brief  Close the file that was being written
 */
FRESULT eFile_WClose(void) {
	if (!fileOpened || !openedFile.rw) return EFILE_FAILURE;
	fileOpened = 0;
	DRESULT res = eDisk_WriteBlock((BYTE *) &openedFile.data, openedFile.curBlock);
	if (res != RES_OK) {
		return EFILE_FAILURE;
	}

	// write directory to disk
	BYTE * pt = (BYTE *) &directory;
	for (int i = 0; i < DIR_SIZE; i++) {
		DRESULT res = eDisk_WriteBlock(pt, i);
		if (res != RES_OK) return EFILE_FAILURE;
		pt += BLOCK_SIZE;
	}

	// write FAT to disk
	pt = fat;
	for (int i = FAT_START_BLOCK; i <= FAT_END_BLOCK; i++) {
		DRESULT res = eDisk_WriteBlock(pt, i);
		if (res != RES_OK) return EFILE_FAILURE;
		pt += BLOCK_SIZE;
	}

	return EFILE_SUCCESS;
}


/**
 * @details Close the file, leave disk in a state power can be removed.
 * @param  none
 * @return 0 if successful and 1 on failure (e.g., wasn't open)
 * @brief  Close the file that was being read
 */
FRESULT eFile_RClose(void) {
	if (!fileOpened || openedFile.rw) return EFILE_FAILURE;
	fileOpened = 0;
	return EFILE_SUCCESS;
}

/**
 * @details Deactivate the file system. One can reactive the file system with eFile_Init.
 * @param  none
 * @return 0 if successful and 1 on failure (e.g., trouble writing to flash)
 * @brief  Close the disk
 */
FRESULT eFile_Close(void) {
	DRESULT res;

	if (fileOpened && openedFile.rw)
		res = eFile_WClose();
	else if (fileOpened && !openedFile.rw)
		res = eFile_RClose();

	return res;
}


/**
 * @details Display the directory with filenames and sizes
 * @param  printf pointer to a function that outputs ASCII characters to display
 * @return none
 * @brief  Show directory
 */
void eFile_Directory(void printf(const char *, ...)) {
	printf("File Name   File Size\n\r");

	for (int i=0; i<MAX_FILE_NUM; i++) {
		if (directory.entries[i].firstBlock == 0)
			continue;


		printf("%s   %u\n\r", directory.entries[i].name, directory.entries[i].fileSize);
	}
}


/**
 * @details open the file for writing, redirect stream I/O (printf) to this file
 * @note if the file exists it will append to the end<br>
 If the file doesn't exist, it will create a new file with the name
 * @param  name file name is an ASCII string up to seven characters
 * @return 0 if successful and 1 on failure (e.g., can't open)
 * @brief  redirect printf output into this file
 */
FRESULT eFile_RedirectToFile(char *name) {
	OS_bWait(&output_lock);                  // cannot change stream in the middle of printing something

	eFile_Create(name);
//	LED_GREEN_TOGGLE();

	FRESULT res = eFile_WOpen(name);

	if (res != EFILE_SUCCESS) {
		return res;
	}
//	LED_BLUE_TOGGLE();

	outstream = FILE_STREAM;
	OS_bSignal(&output_lock);

	return EFILE_SUCCESS;
}

/**
 * @details close the file for writing, redirect stream I/O (printf) back to the UART
 * @param  none
 * @return 0 if successful and 1 on failure (e.g., trouble writing)
 * @brief  Stop streaming printf to file
 */
FRESULT eFile_EndRedirectToFile(void) {
	OS_bWait(&output_lock);
	FRESULT res = eFile_WClose();
	if (res != EFILE_SUCCESS) return res;
	outstream = UART_STREAM;
	OS_bSignal(&output_lock);
	return EFILE_SUCCESS;
}

