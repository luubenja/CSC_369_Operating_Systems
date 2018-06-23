#include "util.h"
#define INDIRECT_BLK_INDEX 11


/* 
	Initialize the main global variables for this program.
*/

void init_globals(void){

	unsigned int i;

	struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + (2 * EXT2_BLOCK_SIZE));
    
     // Set bg fields to 0
    gd->bg_pad = 0;
    for (i = 0; i < 3; i++){
    	gd->bg_reserved[i] = 0;
    }
    
    INDS_COUNT = sb->s_inodes_count;
    BLKS_COUNT = sb->s_blocks_count;
    IND_TABLE = (struct ext2_inode*)(disk + EXT2_BLOCK_SIZE * gd->bg_inode_table);

    ind_bmap.sys_bm = (unsigned char *)(disk + (gd->bg_inode_bitmap * EXT2_BLOCK_SIZE));
    blk_bmap.sys_bm = (unsigned char *)(disk + (gd->bg_block_bitmap * EXT2_BLOCK_SIZE));
    
    //ind_bmap.num_bytes_alloc = sb->s_inodes_count / 8;
    ind_bmap.num_bits_avail = gd->bg_free_inodes_count;
    
    //blk_bmap.num_bytes_alloc = sb->s_blocks_count / 8;
    blk_bmap.num_bits_avail = gd->bg_free_blocks_count;
    
    read_bitmaps();

}

/*
	Read inode/block img bitmap and insert char representations of bits into bit_arr ptr.
*/

void read_bitmaps(void){
	
	int byte_i, bit, curr_bm, found_bits = -1;
	unsigned int index;
	
	int inds_blks_count[] = {INDS_COUNT, BLKS_COUNT};
	unsigned char *sys_bmaps[] = {ind_bmap.sys_bm, blk_bmap.sys_bm};
	Bitmap *bitmap_arrs[] = {&ind_bmap, &blk_bmap};
	
	for(curr_bm = 0; curr_bm < 2; curr_bm++){
		
		index = 1;
		
		for(byte_i = 0; (byte_i < inds_blks_count[curr_bm]/8) && (found_bits < curr_bm); byte_i++){
			for(bit = 0; (bit < 8) && (found_bits < curr_bm); bit++){
				if(!(sys_bmaps[curr_bm][byte_i] & (1 << bit)) && (index >= EXT2_GOOD_OLD_FIRST_INO)){
					// Store byte index in respective bm struct
					bitmap_arrs[curr_bm]->avail_byte_index = byte_i;
					bitmap_arrs[curr_bm]->avail_bit_index = bit;
					bitmap_arrs[curr_bm]->curr_avail_num = index;
					found_bits++;
				} 
				index++;
			}
		}
	}
	
	// Expecting to have found starting bit for both inode/block bitmaps
	if(!found_bits){
		fprintf(stderr, "Err: No free blocks/inodes in File system \n");
    	exit(ENOSPC); 
	}
}

/* 
	Read Bitmap struct and confirm that a single bit is available, if so return
	the inode number that corresponds to current available bit.
*/

unsigned int find_free_inode(void){

	if (ind_bmap.num_bits_avail < 1){
		fprintf(stderr, "Err: Requires %d free bit but %d remain\n", 1, ind_bmap.num_bits_avail);
    	exit(ENOSPC);
	}
	
	return ind_bmap.curr_avail_num;
}

/* 
	Read Bitmap struct and confirm that a single bit is available, if so return
	the block number that corresponds to current available bit.
*/

unsigned int find_free_block(void){
	
	if (blk_bmap.num_bits_avail < 1){
		fprintf(stderr, "Err: Requires %d free bits but %d remain\n", 1, blk_bmap.num_bits_avail);
    	exit(ENOSPC);
	}
	
	return blk_bmap.curr_avail_num;
}

/* 
	Read inode/block bitmap struct and confirm that req_n_bits is available, if so
	fill num_arr with available numbers.
*/

void find_n_free(Bitmap *bm, unsigned int *num_arr, unsigned int req_n_bits){
	// Precondition: req_n_bits should be > 0
	
	if (req_n_bits > bm->num_bits_avail){
		fprintf(stderr, "Err: Requires %d free bits but %d remain\n", req_n_bits, bm->num_bits_avail);
    	exit(ENOSPC);
	}
	
	unsigned char *sys_bm = bm->sys_bm;
	
	/*unsigned int byte_index = bm->avail_byte_index;
	unsigned int bit_index = bm->avail_bit_index;*/
	
	unsigned int n_bits_to_find = req_n_bits;
	unsigned int current_num = EXT2_GOOD_OLD_FIRST_INO; //Starting point for search
	unsigned int byte_index = BYTE_INDEX(current_num);
	unsigned int bit_index = BIT_INDEX(current_num);
	
	unsigned int num_arr_i = 0;
	unsigned int curr_inode_num = bm->curr_avail_num;
	
	while (n_bits_to_find){
		if(!(sys_bm[byte_index] & (1 << bit_index))){
			num_arr[num_arr_i] = curr_inode_num;
			n_bits_to_find--;
			num_arr_i++;
		}
		curr_inode_num++;
		bit_index = (bit_index + 1) % 8;
		byte_index = !bit_index ? byte_index + 1 : byte_index;
	}
	
}

/* Find the first occurrence of a free node (either block or inode)*/

unsigned int find_free(Bitmap *bm){

	if (bm->num_bits_avail < 1){
		fprintf(stderr, "Err: Requires %d free bit but %d remain\n", 1, bm->num_bits_avail);
    	exit(ENOSPC);
	}
	
	int n_bits_to_find = 1;
	
	unsigned char *sys_bm = bm->sys_bm;
	unsigned int current_num = EXT2_GOOD_OLD_FIRST_INO; //bm->curr_avail_num;
	unsigned int byte_index = BYTE_INDEX(current_num);
	unsigned int bit_index = BIT_INDEX(current_num);
	
	while (n_bits_to_find){
		if(!(sys_bm[byte_index] & (1 << bit_index))){
			return current_num;
		}
		bit_index = (bit_index + 1) % 8;
		byte_index = !bit_index ? byte_index + 1 : byte_index;
		current_num++;
	}
	
	fprintf(stderr, "Err: finding free node\n");
	exit(ENOSPC);
}

/* 
	Set current available bit to used/1 for respective bitmap.
*/

void set_used_bit(Bitmap *bm){
	
	unsigned int byte_index = bm->avail_byte_index;
	unsigned int bit_index = bm->avail_bit_index;
	
	if (!(bm->sys_bm[byte_index] & (1 << bit_index))){
		bm->sys_bm[byte_index] |= (1 << bit_index);
	} else {
		fprintf(stderr, "Err: Bit %d at byte %d is already in use\n", bit_index, byte_index);
		exit(ENOENT);
	}
	
	bm->num_bits_avail -= 1;
	
	// Set bitmap fields according to number of remaining bits
	if (bm->num_bits_avail > 0){
		bm->curr_avail_num = find_free(bm); //find next free num for Bitmap 
		bm->avail_byte_index = BYTE_INDEX(bm->curr_avail_num);
		bm->avail_bit_index = BIT_INDEX(bm->curr_avail_num);
	} 
	// Invalidate bitmap fields since no more space is available
	else {
		bm->avail_bit_index = -1;
		bm->avail_byte_index = -1;
		bm->curr_avail_num = -1;
	}
	
}

/* 
	Set n number of bits to used/1 for respective bitmap.
*/

void set_n_toggle_bit(Bitmap *bm, unsigned int *num_arr, unsigned int set_flag, unsigned int bits_to_set){
	// Precondition: bits_to_set should be > 0
	
	assert(bits_to_set > 0);
	
	unsigned int num_arr_i = 0;
	unsigned int set_n_bits = bits_to_set;
	unsigned int byte_index, bit_index;

	while (set_n_bits){
		
		byte_index = BYTE_INDEX(num_arr[num_arr_i]);
		bit_index = BIT_INDEX(num_arr[num_arr_i]);
		
		if (!((bm->sys_bm[byte_index] | (1 << bit_index)) != set_flag))
			fprintf(stderr, "Warning: toggling num %d (byte %d, bit index %d) had no effect\n", num_arr[num_arr_i], byte_index, bit_index);

		bm->sys_bm[byte_index] ^= (1 << bit_index);
		//if(!(((bm->sys_bm[byte_index] | (1 << bit_index)) ^ set_flag)))
		//	fprintf(stderr, "Warning: toggling num %d byte %d at bit index %d had no effect\n", num_arr[num_arr_i], byte_index, bit_index);
			
		num_arr_i++;
		set_n_bits--;
	}
	
	assert(byte_index >= 1 && byte_index <= 15);
	assert(bit_index >= 0 && bit_index <= 7);
	
	// Check what the flag indicator is. If toggling to 1, then decrease available bits.
	if (set_flag == 1){
		bm->num_bits_avail -= bits_to_set;
	} else {
		bm->num_bits_avail += bits_to_set;
	}
	
	// Set bitmap fields according to number of remaining bits
	if (bm->num_bits_avail > 0){
		bm->curr_avail_num = find_free(bm); //find next free num for Bitmap 
		bm->avail_byte_index = BYTE_INDEX(bm->curr_avail_num);
		bm->avail_bit_index = BIT_INDEX(bm->curr_avail_num);
	} 
	// Invalidate bitmap fields since no more space is available
	else {
		bm->avail_bit_index = -1;
		bm->avail_byte_index = -1;
		bm->curr_avail_num = -1;
	}
	
}

/* Check whether bit at bit_index is a 1 or 0 */
unsigned int check_bit(Bitmap *bm, unsigned int bit_index){

	unsigned int byte_i = bit_index / 8;
	unsigned int bit_i = bit_index % 8;

	return (bm->sys_bm[byte_i] & (1 << bit_i));
}

/* 
	"Normalize" the user supplied file path by trimming repeating slashes. 
*/

void normalize_path(char *path){
	
	
	/* Simply return 0 if beginning of path is invalid.
	   2 such cases: path has a single char, or missing '/' at the beginning of path */
	if (strlen(path) < 2 || strncmp(&path[0], "/", 1)){
		fprintf(stderr, "Err: Invalid path given\n");
		exit(ENOENT);
	}
	
	char *prev_slash = NULL;
	char *curr_slash = strstr(path, "/");
	
	// Just trim last slash if character len is 3
	if (strlen(path) == 3){
	
		path[2] = '\0';
		
	} else {
	
	while (curr_slash){
		
		prev_slash = curr_slash;
		
		// Find next adjacent '/' if any
		while (!(strncmp(curr_slash + 1, "/", 1)))
			curr_slash += 1;
		
		// True if slash has at least 1 adjacent slash
		if (prev_slash < curr_slash)
			memcpy(prev_slash + 1, curr_slash + 1, &path[strlen(path)] - curr_slash);
		
		curr_slash = strstr(prev_slash + 1, "/");
		
		}
	}

}

/* 
	Count the number of substrings (strs between slashes) in the file path.
	These correspond to the number of "levels" we need to access in the file
	system.
*/

int get_path_substr_count(char *path){
	
	int path_len = strlen(path);
	
	// If path doesn't have multi levels, return 1 (just a single level) 
	if (!strstr(path, "/"))
		return 1;
		
	int substr_count = 0;
	int index;
	
	for (index = 0; index < path_len + 1; index++){
		
		if(!strncmp(&path[index], "/", 1) && index > 0){
			substr_count += 1;
		} else if (!strncmp(&path[index], "\0", 1) && strncmp(&path[index-1], "/", 1)){
			substr_count += 1;
		}

	}
	
	return substr_count;
	
}

/*Seperates path into two arrays, one containing each section of the Path upto the second last section
and the other containing soley the last section (a section being any part seperated by a /)*/

char*** create_path(char*path){

	char*** paths;
	char** parent_path;
	char** child_path;

	int path_len = get_path_substr_count(path);
	int substr_len;
	int path_char_len = strlen(path);
	char *prev_char = !strncmp(&path[0], "/", 1) ? &path[1] : path;
	char *curr_char = strstr(&path[1], "/");
	char *p_null_term = &path[path_char_len];

	parent_path = malloc((path_len-1)*sizeof(char*));
	child_path =  malloc(sizeof(char*));

	for (int i = 0; i < path_len-1; i++){
		
		if(!curr_char)
			curr_char = p_null_term;
		
		substr_len = curr_char - prev_char;
		parent_path[i] = malloc((substr_len + 1)*sizeof(char));
		strncpy(parent_path[i],prev_char,substr_len);
		parent_path[i][substr_len] = '\0';
		
		prev_char = curr_char + 1;
		curr_char = strstr(curr_char + 1, "/");
	}

	curr_char = p_null_term;
	substr_len = curr_char - prev_char;
	child_path[0] = malloc((substr_len + 1)*(sizeof(char)));
	strncpy(child_path[0], prev_char, substr_len);
	child_path[0][substr_len] = '\0';

	paths = malloc(2*sizeof(char**));
	paths[0] = parent_path;
	paths[1] = child_path;

	return paths;
}

/* Returns inode the type based on i_mode
*/
char get_inode_type(unsigned short i_mode){
	switch(i_mode >> 12){
	case EXT2_S_IFLNK >> 12:
    	return 'l';
	case EXT2_S_IFREG >> 12:
    	return 'f';
	case EXT2_S_IFDIR >> 12:
    	return 'd';
	default:
    	perror("Invalid inode type\n");
    	exit(EINVAL);
	}
}

/* Returns directory entry type based on file_type
*/
char get_directory_entry_type(unsigned char file_type){
	switch(file_type){
	case EXT2_FT_UNKNOWN:
		return 'u';
	case EXT2_FT_REG_FILE:
    	return 'f';
	case EXT2_FT_DIR:
    	return 'd';
	case EXT2_FT_SYMLINK:
    	return 'l';
	default:
    	perror("Invalid directory entry type\n");
    	exit(EINVAL);
	}

}


/* 
	Check whether the passed in user path does in fact exist. If so,
	return the Parent_dir of the final node in the path.
*/
int check_block_for_dir(int block_num, char* dir_name){
	int mem_to_check = EXT2_BLOCK_SIZE; //remaining memory in block in check
	struct ext2_dir_entry *curr_dir_ent = (struct ext2_dir_entry *)(disk + block_num * EXT2_BLOCK_SIZE);

	while(mem_to_check > 0){
		if(!strncmp(dir_name, curr_dir_ent->name, curr_dir_ent->name_len)){
			return curr_dir_ent -> inode;
		}
		mem_to_check -= curr_dir_ent->rec_len;
		curr_dir_ent = (struct ext2_dir_entry *)(((unsigned char *)curr_dir_ent) + curr_dir_ent->rec_len);		
	}
	return 0;

}

/*Checks whether a directory entry exists along a path 
*/
unsigned int dir_entry_exists(char **path, unsigned int starting_inode){
	int path_len;
	if(*path){
		path_len = sizeof(path)/sizeof(char*);
	}
	else{
		return EXT2_ROOT_INO;
	}
	
	int path_index = 0, i_block_len = 0, i_block_index = 0, i_block_value = 0, match = 0, current_inode_num = 0;
	char *path_section;
	
	
	if(starting_inode){
		current_inode_num = starting_inode;
	}
	else{
		current_inode_num = EXT2_ROOT_INO;
	}
	
	//all paths must start at the root
	struct ext2_inode current_inode;


	for (path_index = 0; path_index < path_len && !match; path_index++){

		current_inode = IND_TABLE[INDTBL_INDEX(current_inode_num)]; // index should be current_inode_num - 1 instead of current_inode_num ? 
		path_section = path[path_index];
		match = 0;

		i_block_len = INDIRECT_BLK_INDEX + 1;
		i_block_index = 0;
		
		while(i_block_index < i_block_len && !match){
			i_block_value = current_inode.i_block[i_block_index];
			if (i_block_value){
				if(i_block_index == INDIRECT_BLK_INDEX){
					unsigned int* indirect_block_table = (unsigned int*)(disk + i_block_value * EXT2_BLOCK_SIZE);
					while(*indirect_block_table || !match){
						match = check_block_for_dir(*indirect_block_table, path_section);
						// If not a match, the same indirect_block_table value will be passed into check_block_for_dir
						indirect_block_table++;
					}
				}
				else{
					match = check_block_for_dir(i_block_value, path_section);
				}
			}
			else{
				break;
			}
			i_block_index+= 1;
		}
		// Check if there wasn't a match and that we're looking beyond the root directory.
		// If examining only the root directory then 
		if(!match){
			return 0;
		} 
		else{
			current_inode_num = match;
		}
		
	}

	return current_inode_num;

}

void mk_dir_entry(int dir_inode, unsigned int inode, unsigned short rec_len, unsigned char name_len, unsigned char file_type, char* name, int namelen){
    int i_block_len, i_block_value, i_block_index, mem_blk = 0, ind_blk_index = 0;
    unsigned int *indirect_blks;
    struct ext2_inode parent_inode = IND_TABLE[dir_inode - 1];
    i_block_len = INDIRECT_BLK_INDEX + 1; 
    for (i_block_index = i_block_len; i_block_index >= 0 ; i_block_index--){
        i_block_value = parent_inode.i_block[i_block_index];
        if (i_block_value){
        	if (i_block_index == INDIRECT_BLK_INDEX){
        		 indirect_blks = (unsigned int*)(disk + i_block_value * EXT2_BLOCK_SIZE);
        		
        		// Increment indirect_blks pointer up until the last block num entry
        		while (indirect_blks){
        			ind_blk_index+=1;
        			indirect_blks++;
        		}
        		
        		indirect_blks--;
        		ind_blk_index--;
        		mem_blk = *indirect_blks;

        	}
        	else{
        		mem_blk = i_block_value;

        	}
        	break;
        }
    }
    //search through the block for space
    int rem_block_space = EXT2_BLOCK_SIZE;
    struct ext2_dir_entry *dir_ent = (struct ext2_dir_entry *)(disk + mem_blk * EXT2_BLOCK_SIZE);
    rem_block_space -= dir_ent->rec_len;

    while (rem_block_space > 0){
        dir_ent = (struct ext2_dir_entry *)(((unsigned char *)dir_ent) + dir_ent->rec_len);
        rem_block_space -= dir_ent->rec_len; 
    }

    int last_dir_ent_sz = TRUE_FILE_SIZE(dir_ent->name_len);
    int new_dir_ent_sz = TRUE_FILE_SIZE(name_len);
    int padding = dir_ent->rec_len - last_dir_ent_sz;

    //if padding is too small to allocate new directory entry move to next block
    if(padding < new_dir_ent_sz){
    	if (i_block_index == INDIRECT_BLK_INDEX){
    		if (ind_blk_index >= EXT2_BLOCK_SIZE/sizeof(unsigned int)){
    			exit(ENOMEM);
    		}
    		indirect_blks++;
    		*indirect_blks = find_free_block();
    		mem_blk = *indirect_blks;
    	}
    	else{
    		i_block_index++;
    		if (i_block_index == INDIRECT_BLK_INDEX){
    			unsigned int ind_blks[2];
    			find_n_free(&blk_bmap, ind_blks, 2);

    			parent_inode.i_block[i_block_index] = ind_blks[0];
    			unsigned int *indirect_blks = (unsigned int*)(disk + ind_blks[0] * EXT2_BLOCK_SIZE);
    			*indirect_blks = ind_blks[1];
    			mem_blk = ind_blks[1];

    		}
    		else{
    			parent_inode.i_block[i_block_index] = find_free_block();
    			mem_blk = parent_inode.i_block[i_block_index];
    		}
    	}
    	dir_ent = (struct ext2_dir_entry *)(disk + mem_blk * EXT2_BLOCK_SIZE);
    	rec_len = EXT2_BLOCK_SIZE;
    }
    else{
    	dir_ent->rec_len = last_dir_ent_sz;
    	dir_ent = (struct ext2_dir_entry *)(((unsigned char *)dir_ent) + dir_ent->rec_len);
    	rec_len = padding;
    }

    dir_ent->rec_len = rec_len;
    dir_ent->inode = inode;
    dir_ent->name_len = name_len;
    dir_ent->file_type = file_type;
    memcpy(dir_ent->name, name, namelen);
}



