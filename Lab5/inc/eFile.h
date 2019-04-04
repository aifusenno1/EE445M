/**
 * @file      eFile.h
 * @brief     high-level file system
 * @details   This file system sits on top of eDisk.
 * @version   V1.0
 * @author    Valvano
 * @copyright Copyright 2017 by Jonathan W. Valvano, valvano@mail.utexas.edu,
 * @warning   AS-IS
 * @note      For more information see  http://users.ece.utexas.edu/~valvano/
 * @date      March 9, 2017

 ******************************************************************************/

typedef enum {
	EFILE_SUCCESS = 0,
	EFILE_FAILURE = 1
} FRESULT;

/**
 * @details This function must be called first, before calling any of the other eFile functions
 * @param  none
 * @return 0 if successful and 1 on failure (already initialized)
 * @brief  Activate the file system, without formating
 */
FRESULT eFile_Init(void); // initialize file system


/**
 * @details Erase all files, create blank directory, initialize free space manager
 * @param  none
 * @return 0 if successful and 1 on failure (e.g., trouble writing to flash)
 * @brief  Format the disk
 */
FRESULT eFile_Format(void); // erase disk, add format

/**
 * @details Create a new, empty file with one allocated block
 * @param  name file name is an ASCII string up to seven characters
 * @return 0 if successful and 1 on failure (e.g., already exists)
 * @brief  Create a new file
 */
FRESULT eFile_Create(char name[]);  // create new file, make it empty


/**
 * @details Open the file for writing, read into RAM last block
 * @param  name file name is an ASCII string up to seven characters
 * @return 0 if successful and 1 on failure (e.g., trouble reading from flash)
 * @brief  Open an existing file for writing
 */
FRESULT eFile_WOpen(char name[]);      // open a file for writing


/**
 * @details Save one byte at end of the open file
 * @param  data byte to be saved on the disk
 * @return 0 if successful and 1 on failure (e.g., trouble writing to flash)
 * @brief  Format the disk
 */
FRESULT eFile_Write(char data);

/**
 * @details Deactivate the file system. One can reactive the file system with eFile_Init.
 * @param  none
 * @return 0 if successful and 1 on failure (e.g., trouble writing to flash)
 * @brief  Close the disk
 */
FRESULT eFile_Close(void);


/**
 * @details Close the file, leave disk in a state power can be removed.
 * This function will flush all RAM buffers to the disk.
 * @param  none
 * @return 0 if successful and 1 on failure (e.g., trouble writing to flash)
 * @brief  Close the file that was being written
 */
FRESULT eFile_WClose(void); // close the file for writing

/**
 * @details Open the file for reading, read first block into RAM
 * @param  name file name is an ASCII string up to seven characters
 * @return 0 if successful and 1 on failure (e.g., trouble reading from flash)
 * @brief  Open an existing file for reading
 */
FRESULT eFile_ROpen(char name[]);      // open a file for reading


/**
 * @details Read one byte from disk into RAM
 * @param  pt call by reference pointer to place to save data
 * @return 0 if successful and 1 on failure (e.g., trouble reading from flash)
 * @brief  Retreive data from open file
 */
FRESULT eFile_ReadNext(char *pt);       // get next byte

/**
 * @details Close the file, leave disk in a state power can be removed.
 * @param  none
 * @return 0 if successful and 1 on failure (e.g., wasn't open)
 * @brief  Close the file that was being read
 */
FRESULT eFile_RClose(void); // close the file for reading


/**
 * @details Display the directory with filenames and sizes
 * @param  printf pointer to a function that outputs ASCII characters to display
 * @return none
 * @brief  Show directory
 */
void eFile_Directory(void printf(const char *, ...));

/**
 * @details Delete the file with this name, recover blocks so they can be used by another file
 * @param  name file name is an ASCII string up to seven characters
 * @return 0 if successful and 1 on failure (e.g., file doesn't exist)
 * @brief  delete this file
 */
FRESULT eFile_Delete(char name[]);  // remove this file

/**
 * @details open the file for writing, redirect stream I/O (printf) to this file
 * @note if the file exists it will append to the end<br>
 If the file doesn't exist, it will create a new file with the name
 * @param  name file name is an ASCII string up to seven characters
 * @return 0 if successful and 1 on failure (e.g., can't open)
 * @brief  redirect printf output into this file
 */
FRESULT eFile_RedirectToFile(char *name);


/**
 * @details close the file for writing, redirect stream I/O (printf) back to the UART
 * @param  none
 * @return 0 if successful and 1 on failure (e.g., trouble writing)
 * @brief  Stop streaming printf to file
 */
FRESULT eFile_EndRedirectToFile(void);
