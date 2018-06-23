#ifndef __UTIL_H__
#define __UTIL_H__

#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <time.h>
#include "ext2.h"

#define INDS_PER_GROUP		32
#define INDIRECT_BLK_INDEX  11
#define	BLKS_PER_GROUP		128
#define IND_ENTRY			"ind_entry"
#define DIR_ENTRY			"dir_entry"
#define IS_DIR(mode)		(mode == EXT2_FT_DIR || ((mode >> 12) == (EXT2_S_IFDIR >> 12)))
#define IS_FILE(mode)		(mode == EXT2_FT_REG_FILE || ((mode >> 12) == (EXT2_S_IFREG >> 12)))
#define IND_IS_TYPE(ind_mode, desired_mode)	(((ind_mode >> 12) == (desired_mode >> 12)))
#define IND_INDEX(ind_num)	((ind_num - 1) % INDS_PER_GROUP)
#define BYTE_INDEX(num)		((num - 1) / 8)		
#define BIT_INDEX(num)		((num - 1) % 8)
#define INDTBL_INDEX(num)	(num - 1)

// Base size of dir_entry = 9 bytes
// len of dir name + 9 gives actual/true size of directory entry
//#define TRUE_FILE_SIZE(len)  (len + 9) 

// Calculates the actual number of bytes needed, aligned to 4 byte boundaries
// The minimum size of a directory entry as a factor of 4 is 12 bytes
#define TRUE_FILE_SIZE(len)	(len <= 12 ? 12 : len % 4 ? len + (4 - len % 4) : len)

unsigned char *disk;						// Pointer to file system disk
char *blk_bitmap_arr, *ind_bitmap_arr;		// Pointers to char arrays representing bitmaps

//fn File system basic counts 
unsigned int BLKS_COUNT, INDS_COUNT;		// The number of blocks and inodes respectively

// Pointers to bitmaps & inode table
//unsigned char *IND_BMAP, *BLK_BMAP;		// Pointers to system inode/block bitmaps
struct ext2_inode *IND_TABLE;				// Pointer to system inode table


//Path preceding_tokens;
//Path last_token; 
//Path *path_composite[2];					// Contains 

// Parent directory of newly added directory
typedef struct {
	unsigned int	ind_num;				// Inode number of parent directory
	int				avail_blk_index;		// Parent dir's free block
} Parent_dir;

// Bitmap metadata - stores relevant info about the state of file system
typedef struct {
	unsigned char 	*sys_bm;
	unsigned int 	num_bits_avail;
	unsigned int	avail_byte_index;
	unsigned int 	avail_bit_index;
	unsigned int	curr_avail_num;
} Bitmap;

Bitmap ind_bmap, blk_bmap;

/* Initialize all global variables */
void init_globals(void);

/* Gets file type of inode/dir entries */
char get_ent_type(unsigned short i_mode, char *mode_type);

/* Writes string array representing inode/block bitmaps */
void read_bitmaps();

/* Normalize (aka remove repeated '/' and the like) the path */
void normalize_path(char *path);

/* Count the number of substrings in the absolute path */
int get_path_substr_count(char *path);

/* Get the entry type of an inode or dir entry */
char file_is_type(unsigned short i_mode, char *mode_type);

/* Traverse user supplied path, ensure it exists */
unsigned int dir_entry_exists(char **path, unsigned int starting_inode);

/* Return free inode number that corresponds to 1st free bit if it exists. 
   A quick/shortcut function to use instead of find_free(ind_bm)
*/
unsigned int find_free_inode(void);

/* Return free inode number that corresponds to 1st free bit if it exists. 
   A quick/shortcut function to use instead of find_free(blk_bm)
*/unsigned int find_free_block(void);

/* Return free block or inode number that corresponds to 1st free bit if it exists. */
unsigned int find_free(Bitmap *bm);

/* Find req_n_bits free inodes or blocks and fill num_arr with corresponding numbers */
void find_n_free(Bitmap *bm, unsigned int *num_arr, unsigned int req_n_bits);

/* Set respective bitmap's bit to 0 (used) */
void set_used_bit(Bitmap *bm);

/* Set respective bitmap's bit(s) to 0 */
void set_n_toggle_bit(Bitmap *bm, unsigned int *num_arr, unsigned int set_flag, unsigned int bits_to_set);

/* Check whether bit at bit_index is a 1 or 0 */
unsigned int check_bit(Bitmap *bm, unsigned int bit_index);

/* Create a new directory entry */
void mk_dir_entry(int dir_inode, unsigned int inode, unsigned short rec_len, unsigned char name_len, unsigned char file_type, char* name, int namelen);

/* Check that the new directory exists */
unsigned int dir_entry_exists(char **path, unsigned int starting_inode);

char*** create_path(char* path);

#endif