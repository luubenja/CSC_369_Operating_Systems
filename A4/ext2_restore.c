#include "util.h"
#include "time.h"

int restore_inode(unsigned int inode_num){
    struct ext2_inode rest_inode = IND_TABLE[inode_num];
    unsigned int*indirect_blks;
    unsigned int indirect_block_index = 0, i_block_index = 0, block_count = 0;
    unsigned int block_num = rest_inode.i_block[i_block_index];
    if (rest_inode.i_block[INDIRECT_BLK_INDEX]){
        indirect_blks = (unsigned int*)(disk + rest_inode.i_block[INDIRECT_BLK_INDEX]*EXT2_BLOCK_SIZE);
    }
     
    while(block_num){
        if(i_block_index == INDIRECT_BLK_INDEX){
            block_num = indirect_blks[indirect_block_index];
            indirect_block_index++;
        }
        else{
            i_block_index++;
            block_num = rest_inode.i_block[i_block_index];
        }

        if (check_bit(&blk_bmap, block_num)){
            return -1;
        }

        block_count ++;
    }

    unsigned int blocks[block_count];
    block_count = 0;
    i_block_index = 0;
    indirect_block_index = 0;
    block_num = rest_inode.i_block[i_block_index];

    while(block_num){
        if(i_block_index == INDIRECT_BLK_INDEX){
            block_num = indirect_blks[indirect_block_index];
            indirect_block_index++;
        }
        else{
            i_block_index++;
            block_num = rest_inode.i_block[i_block_index];
            
        }
        blocks[block_count] = block_num;
        block_count ++;
    }

    rest_inode.i_dtime = 0;
    set_n_toggle_bit(&blk_bmap, blocks, 1, block_count);
    set_n_toggle_bit(&ind_bmap, &inode_num, 1, 1);
    return 1;
}

unsigned int restore_dir_entry(int parent_inode_num, char* dir_entry_name){
    struct ext2_inode parent_inode = IND_TABLE[parent_inode_num];
    
    int found = 0, i_block_index = 0, dir_ent_sz = 0;
    unsigned int block_num = parent_inode.i_block[i_block_index];
    struct ext2_dir_entry* prev_dir_ent, *curr_dir_ent;

    while(block_num || !found){

        int mem_to_check = EXT2_BLOCK_SIZE; //remaining memory in block in check
        curr_dir_ent = (struct ext2_dir_entry *)(disk + block_num * EXT2_BLOCK_SIZE);

        while(mem_to_check > 0){
            if(!strncmp(dir_entry_name, curr_dir_ent->name, curr_dir_ent->name_len)){
                found = 1;
                break;
            }
            prev_dir_ent = curr_dir_ent;
            dir_ent_sz = TRUE_FILE_SIZE(curr_dir_ent->name_len);

            //if there is a gap
            if (dir_ent_sz < curr_dir_ent->rec_len){
                curr_dir_ent = (struct ext2_dir_entry *)(((unsigned char *)curr_dir_ent) + dir_ent_sz);
                mem_to_check -= dir_ent_sz;

            }
            else{
                curr_dir_ent = (struct ext2_dir_entry *)(((unsigned char *)curr_dir_ent) + curr_dir_ent->rec_len);
                mem_to_check -= curr_dir_ent->rec_len;

            }
              
        }
        i_block_index++;
        block_num = parent_inode.i_block[i_block_index];
    }
    
    if (found){
        return curr_dir_ent->inode;
    }
    else{
        return 0;
    }
    


}



void ext2_restore(char **p, char**fpath){

    //EXAMPLE: /a/b/c/d.txt
    //p = path up to the parent of the copy destination Ex /a/b/c
    //fpath = path including only the copy destination Ex/ d.txt

    int parent_inode_num, file_inode_num;

    parent_inode_num = dir_entry_exists(p, 0);


    if(!(parent_inode_num)){
        fprintf(stderr, "Err: invalid parent path\n");
        exit(EINVAL);
    }

    struct ext2_inode parent_inode = IND_TABLE[parent_inode_num];

    if(!IS_DIR(parent_inode.i_mode)){
        fprintf(stderr, "Err: parent is not a directory\n");
        exit(EISDIR);
    }

    file_inode_num = dir_entry_exists(fpath, parent_inode_num);

    if(!(file_inode_num)){
        file_inode_num = restore_dir_entry(parent_inode_num, fpath[0]);
    }

    if(!(file_inode_num)){
        fprintf(stderr, "Err: cannot find directory entry\n");
        exit(EINVAL);
    }

    if (restore_inode(file_inode_num) != 1){
        fprintf(stderr, "Err: Could not restore\n");
        exit(ENOENT);
    }
    
}
	

int main(int argc, char **argv) {
	
	if(argc != 3) {
        fprintf(stderr, "Usage: %s <command img filepath>\n", argv[0]);
        exit(1);
    }
    
    int p_len = strlen(argv[2]);
    
    int fd = open(argv[1], O_RDWR);
    char fp[p_len + 1]; 
    
    strcpy(&fp[0], argv[2]);
    
    normalize_path(&fp[0]);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
        
    init_globals();
    
    char*** paths = create_path(&fp[0]);

    char** parent_path = paths[0];
    char** child_path = paths[1];


    ext2_restore(parent_path, child_path);
    
	return 0;
}