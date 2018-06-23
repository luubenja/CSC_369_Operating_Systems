#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"

//unsigned char *IND_BMAP_ARR, *BLK_BMAP_ARR;

// Entry type, either a Inode or Dir Entry
char *IND_ENTRY = "ind_entry";
char *DIR_ENTRY = "dir_entry";

char get_ent_type(unsigned short i_mode, char *mode_type){

	if (mode_type == IND_ENTRY) {

   		switch(i_mode >> 12){
        	case EXT2_S_IFLNK >> 12:
            	return 'l';
        	case EXT2_S_IFREG >> 12:
            	return 'f';
        	case EXT2_S_IFDIR >> 12:
            	return 'd';
        	default:
            	perror("Invalid inode type\n");
            	exit(1);
    	}
    	
    } else {
    
    	switch(i_mode){
        	case EXT2_FT_UNKNOWN:
            	return 'u';
        	case EXT2_FT_REG_FILE:
            	return 'f';
        	case EXT2_FT_DIR:
            	return 'd';
            case EXT2_FT_SYMLINK:
            	return 's';
        	default:
            	perror("Invalid directory type\n");
            	exit(1);
    	}
    
    }
}

void read_bitmap(int num_blocks, unsigned char *bitmap, char bit_arr[]){
	
	int byte, bits;
	int index = 0;
	
	for(byte = 0; byte < num_blocks/8; byte++){
	
		for(bits = 0; bits < 8; bits++){
			if(bitmap[byte] & (1 << bits)){
				bit_arr[index] = '1';
			} else {
				bit_arr[index] = '0';
			}
			index++;
		}
		
		bit_arr[index] = ' ';
		index++;
	}
	
	bit_arr[index] = '\n';
	bit_arr[index + 1] = '\0';

}

void print_inodes(void){
    int byte, bits, index;
    struct ext2_inode currenti;

    index = 0;
    for(byte = 0; byte < INDS_COUNT/8; byte++){
        for(bits = 0; bits < 8; bits++){
            
            if (index  + 1 == EXT2_ROOT_INO || (index + 1 > EXT2_GOOD_OLD_FIRST_INO &&  IND_BMAP[byte] & (1 << bits))){
                currenti = IND_TABLE[index];
                

                printf("[%d] type: %c size: %d links: %d blocks: %d\n", \
                		index+1, \
                		get_ent_type(currenti.i_mode, IND_ENTRY), \
                		currenti.i_size, \
                		currenti.i_links_count, currenti.i_blocks);
                		
                printf("[%d] Blocks: ", index+1);
                for (int i = 0; i < sizeof(currenti.i_block)/sizeof(unsigned int); i++){
                    if (currenti.i_block[i]!= 0){

                    printf(" %d", currenti.i_block[i]);
                };
                }
                printf("\n");
            }
            index++;
        }
    }
}

void print_dirs(void){

	int byte, bits;
	int index = 0;
	
    struct ext2_inode currenti;
    
    for (byte = 0; byte < INDS_COUNT/8; byte++){
    	for (bits = 0; bits < 8; bits++){

    		if (index + 1 == EXT2_ROOT_INO || (index + 1 > EXT2_GOOD_OLD_FIRST_INO && IND_BMAP[index] & (1 << bits) && \
    			get_ent_type(IND_TABLE[index].i_mode, IND_ENTRY) == 'd') ){
    			
    			currenti = IND_TABLE[index];
    			int block_capacity = EXT2_BLOCK_SIZE;
    			
    			printf("    DIR BLOCK NUM: ");
    			for (int i = 0; i < sizeof(currenti.i_block)/sizeof(unsigned int); i++){
                    if (currenti.i_block[i] != 0){
                    	printf("%d (for inode %d) \n", currenti.i_block[i], index + 1);
                    	
                    	struct ext2_dir_entry *dir_ent = (struct ext2_dir_entry *)(disk + currenti.i_block[i] * EXT2_BLOCK_SIZE);

                    	while (block_capacity > 0){
                    	
                    		block_capacity -= dir_ent->rec_len;
                    		dir_ent->name[dir_ent->name_len] = '\0';
                    	
                    		printf("Inode: %d rec_len: %d name_len: %d type= %c name=%s \n", \
                    				dir_ent->inode, dir_ent->rec_len, \
                    				dir_ent->name_len, \
                    				get_ent_type(dir_ent->file_type, DIR_ENTRY), \
                    				dir_ent->name);
                    	
                    		// If block_capacity is 0, we've printed the last dir_ent, so break loop.
                    		if (block_capacity <= 0)
                    			break;
                    		
                    		/* Pointer to the next directory entry.
                    		   First need to cast dir_ent to a byte pointer (aka unsigned char *)
                    		   before incrementing it by the current dir rec_len offset to the
                    		   next dir entry.
                    		*/ 
                    		dir_ent = (struct ext2_dir_entry *)(((unsigned char *)dir_ent) + dir_ent->rec_len);
                    	}
                	}
    			}
    		}
    		
    		index++;
    		
    	}
    }
    
}


int main(int argc, char **argv) {

    if(argc != 2) {
        fprintf(stderr, "Usage: %s <image file name>\n", argv[0]);
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    
    
    // Should we bother making sb & gd constants (move vars to global space)? Possibly?
    
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + (2 * 1024));
    
    // Assign constants values
    
    BLK_BMAP = (unsigned char *)(disk + 1024 * gd->bg_block_bitmap);
    IND_BMAP = (unsigned char *)(disk + 1024 * gd->bg_inode_bitmap);
    IND_TABLE = (struct ext2_inode*) (disk + gd->bg_inode_table*1024);
    
    INDS_COUNT = sb->s_inodes_count;
    BLKS_COUNT = sb->s_blocks_count;

    
    printf("Inodes: %d\n", INDS_COUNT);
    printf("Blocks: %d\n", BLKS_COUNT);
        
    printf("Block group:\n");
    printf("    block bitmap: %d\n", gd->bg_block_bitmap);
    printf("    inode bitmap: %d\n", gd->bg_inode_bitmap);
    printf("    inode table: %d\n", gd->bg_inode_table);
    printf("    free blocks: %d\n", gd->bg_free_blocks_count);
    printf("    free inodes: %d\n", gd->bg_free_inodes_count);
    printf("    used_dirs: %d\n", gd->bg_used_dirs_count);
    
    // Temp buffers used to store bit strings of respective bitmaps
    char blk_bitmap_arr[BLKS_COUNT + BLKS_COUNT/8 + 2];
    char ind_bitmap_arr[INDS_COUNT + BLKS_COUNT/8 + 2];
    
    read_bitmap(BLKS_COUNT, BLK_BMAP, blk_bitmap_arr);
    read_bitmap(INDS_COUNT, IND_BMAP, ind_bitmap_arr);
    
    printf("Block bitmap: %s", blk_bitmap_arr);
    printf("Inode bitmap: %s", ind_bitmap_arr);

	printf("\n");
	printf("Inodes:\n");
    print_inodes();
    
    printf("\n");
    printf("Directory Blocks:\n");
    print_dirs();
    
    return 0;
}
