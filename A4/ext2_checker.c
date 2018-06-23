#include "util.h"
#include "util.c"

void check_free_counts(int* fix_count){
	struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + (2 * EXT2_BLOCK_SIZE));

    unsigned int free_inodes_count = ind_bmap.num_bits_avail;
    unsigned int free_blocks_count = blk_bmap.num_bits_avail;


    if (gd->bg_free_blocks_count != free_blocks_count){
    	int diff = abs((int)gd->bg_free_blocks_count - (int)free_blocks_count);
   		printf("Fixed: block group's free blocks counter was off by %d compared to the bitmap\n", diff);
    	gd->bg_free_blocks_count = free_blocks_count;
    	*fix_count+=diff;

    }
    if (gd->bg_free_inodes_count != free_inodes_count){
    	int diff = abs((int)gd->bg_free_inodes_count - (int)free_inodes_count);
   		printf("Fixed: block group's free inodes counter was off by %d compared to the bitmap\n", diff);
    	gd->bg_free_inodes_count = free_inodes_count;
    	*fix_count+=diff;

    }
    if (sb->s_free_blocks_count != free_blocks_count){
    	int diff = abs((int)sb->s_free_blocks_count - (int)free_blocks_count);
   		printf("Fixed: block group's free blocks counter was off by %d compared to the bitmap\n", diff);
    	sb->s_free_blocks_count = free_blocks_count;
    	*fix_count+=diff;

    }
    if (sb->s_free_inodes_count != free_inodes_count){
    	int diff = abs((int)sb->s_free_inodes_count - (int)free_inodes_count);
   		printf("Fixed: block group's free inodes counter was off by %d compared to the bitmap\n", diff);
    	sb->s_free_inodes_count = free_inodes_count;
    	*fix_count+=diff;

    }

}

void check_matching_types(struct ext2_dir_entry * dir_ent, int* fix_count){

	struct ext2_inode current_inode = IND_TABLE[dir_ent->inode];
	char dir_ent_type = get_directory_entry_type(dir_ent->file_type);
	char inode_type = get_inode_type(current_inode.i_mode);
	if(dir_ent_type!=inode_type){
		switch(inode_type){
		case 'l':
	    	dir_ent->file_type = EXT2_FT_SYMLINK;
		case 'f':
	    	dir_ent->file_type = EXT2_FT_REG_FILE;
		case 'd':
	    	dir_ent->file_type = EXT2_FT_DIR;
		default:
	    	dir_ent->file_type = EXT2_FT_UNKNOWN;
		}
		printf("Fixed: Entry type vs inode mismatch: inode [%d]\n", dir_ent->inode);
		*fix_count++;

	}

}

void check_inode_bitmap(struct ext2_dir_entry * dir_ent, int* fix_count){
	struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + (2 * EXT2_BLOCK_SIZE));

	if(!check_bit(&ind_bmap,dir_ent->inode)){
		set_n_toggle_bit(&ind_bmap, &dir_ent->inode, 1, 1);
		gd->bg_free_inodes_count--;
		sb->s_free_inodes_count--;
		printf("Fixed: inode [%d] not marked as in-use\n", dir_ent->inode);
		*fix_count++;
	}
}

void check_inode_dtime(struct ext2_dir_entry * dir_ent, int* fix_count){
	struct ext2_inode current_inode = IND_TABLE[dir_ent->inode];
	if(!current_inode.i_dtime){
		current_inode.i_dtime = 0;
		printf("Fixed: valid inode marked for deletion: [%d]\n", dir_ent->inode);
		*fix_count++;
	}
}

void check_data_block_bitmap(struct ext2_dir_entry * dir_ent, int* fix_count){
	struct ext2_inode current_inode = IND_TABLE[dir_ent->inode];
    unsigned int*indirect_blks;
    unsigned int indirect_block_index = 0, i_block_index = 0, block_count = 0;
    unsigned int block_num = current_inode.i_block[i_block_index];

    if (current_inode.i_block[INDIRECT_BLK_INDEX]){
        indirect_blks = (unsigned int*)(disk + current_inode.i_block[INDIRECT_BLK_INDEX]*EXT2_BLOCK_SIZE);
    }
     
    while(block_num){
        if(i_block_index == INDIRECT_BLK_INDEX){
            block_num = indirect_blks[indirect_block_index];
            indirect_block_index++;
        }
        else{
            block_num = current_inode.i_block[i_block_index];
            i_block_index++;
        }
        if(!check_bit(&blk_bmap,block_num)){
        	set_n_toggle_bit(&blk_bmap, &block_num, 1, 1);
        	block_count ++;
        }

    }
    if(block_count){
    	printf("Fixed: %d in-use data blocks not marked in data bitmap for inode: [%d]", block_count, dir_ent->inode);
   		*fix_count+=block_count;
    }
    

}



void check_block(unsigned int block_num, int* fix_count){
	int mem_to_check = EXT2_BLOCK_SIZE; //remaining memory in block in check
	struct ext2_dir_entry *curr_dir_ent = (struct ext2_dir_entry *)(disk + block_num * EXT2_BLOCK_SIZE);
	
	while(mem_to_check > 0){
		check_matching_types(curr_dir_ent, fix_count);
		check_inode_bitmap(curr_dir_ent, fix_count);
		check_inode_dtime(curr_dir_ent, fix_count);
		check_data_block_bitmap(curr_dir_ent, fix_count);

		mem_to_check -= curr_dir_ent->rec_len;
		curr_dir_ent = (struct ext2_dir_entry *)(((unsigned char *)curr_dir_ent) + curr_dir_ent->rec_len);
		
	}
}

void check_dir_entries(int* fix_count){
	struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    unsigned int total = sb->s_inodes_count - EXT2_GOOD_OLD_FIRST_INO;
    unsigned int*indirect_blks;
	unsigned int indirect_block_index = 0, i_block_index = 0;
	unsigned int block_num;

    for (unsigned int index = EXT2_GOOD_OLD_FIRST_INO; index < total; index++){
    	struct ext2_inode current_inode = IND_TABLE[index];
    	if (current_inode.i_ctime && IS_DIR(current_inode.i_mode)){
		    if (current_inode.i_block[INDIRECT_BLK_INDEX]){
		        indirect_blks = (unsigned int*)(disk + current_inode.i_block[INDIRECT_BLK_INDEX]*EXT2_BLOCK_SIZE);
		    }
		    block_num = current_inode.i_block[i_block_index];
 
		    while(block_num){
		    	check_block(block_num, fix_count);
		        if(i_block_index == INDIRECT_BLK_INDEX){
		            block_num = indirect_blks[indirect_block_index];
		            indirect_block_index++;
		        }
		        else{
		        	i_block_index++;
		            block_num = current_inode.i_block[i_block_index];
		        }
		    }
    	}
    }
}
    

void ext2_checker(){
	int* fix_count = (int*)malloc(sizeof(int));
	check_free_counts(fix_count);
	check_dir_entries(fix_count);

	if (*fix_count){
		printf("%d file system inconsistencies repaired!\n", *fix_count);
	}
	else{
		printf("No file system inconsistencies detected!");
	}

}

int main(int argc, char **argv) {
	
	if(argc != 2) {
        fprintf(stderr, "Usage: %s <command img filepath>\n", argv[0]);
        exit(1);
    }
    
    int fd = open(argv[1], O_RDWR);
    

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
        
    
	ext2_checker();


	return 0;
}

