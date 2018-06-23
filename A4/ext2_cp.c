#include "util.h"
#include "time.h"
#define INDIRECT_BLK_INDEX 11


void mk_new_file_inode(int inode_num, char* path){
    //opens and reads file into buffer
    FILE *fp;
    char* buffer = NULL;

    fp = fopen(path,"r");
    if(fp == NULL){
        perror("No file\n");
    }

    if (fseek(fp, 0L, SEEK_END) != 0){
        perror("Cannot find end of file\n");
    }


    int copied_file_sz = ftell(fp);

    
    if (fseek(fp, 0L, SEEK_SET) != 0 ){
        perror("Cannot find beginning of file");
    }

    fread(buffer, 1, copied_file_sz, fp);

    fclose(fp);



    int new_block_num;
    struct ext2_inode new_inode = IND_TABLE[inode_num];
    int i_block_len = sizeof(new_inode.i_block)/sizeof(unsigned int) - 3;

    int req_mem_blks = (copied_file_sz - 1) / EXT2_BLOCK_SIZE + 1; //ceiling of division
    unsigned int mem_blks[req_mem_blks + 1];
    find_n_free(&blk_bmap, mem_blks,req_mem_blks + 1); // + 1 is for the indirect block pointer

    unsigned int indirect_mem_blks[EXT2_BLOCK_SIZE/sizeof(unsigned int)] = {0};

    int amount_remaining = copied_file_sz;
    int amount_to_copy = 0;
  

    int mem_blk_index = 0;
    int i_block_index = 0;

    //fills memory blocks with file contents
    while (mem_blk_index < req_mem_blks - 1){
        new_block_num = mem_blks[mem_blk_index];

        if (amount_remaining > EXT2_BLOCK_SIZE){
            amount_to_copy = EXT2_BLOCK_SIZE;
        }
        else{
            amount_to_copy = amount_remaining;
        }

        memcpy(disk + new_block_num * EXT2_BLOCK_SIZE, buffer, amount_to_copy);
        buffer+= amount_to_copy;

        if (mem_blk_index < 12){
            if (i_block_index < 12){
                new_inode.i_block[i_block_index] = mem_blks[mem_blk_index];
                i_block_index+=1;
            }
            else{
                indirect_mem_blks[mem_blk_index - (i_block_len - 1)] = mem_blks[mem_blk_index];

            }
        }
        mem_blk_index += 1;


    }
    //sets the 12th i_block to point at start of the indirect_blocks
    if (req_mem_blks > i_block_len - 1){
        new_inode.i_block[INDIRECT_BLK_INDEX] = mem_blks[req_mem_blks]; //location of the last mem_blk
        memcpy(disk + mem_blks[req_mem_blks] * EXT2_BLOCK_SIZE, indirect_mem_blks, EXT2_BLOCK_SIZE);
    }

    //set creation time
    new_inode.i_ctime = (unsigned int)time(NULL);
    new_inode.i_dtime = 0;
    new_inode.i_links_count = 0;

    set_n_toggle_bit(&blk_bmap, mem_blks, 1, req_mem_blks + 1);
    set_used_bit(&ind_bmap);


}
	

void ext2_cp(char **p, char** fpath, char* path){

    //EXAMPLE: /a/b/c/d.txt
    //p = path up to the parent of the copy destination Ex /a/b/c
    //fpath = path including only the copy destination Ex/ d.txt
    //path = "/a/b/c/d.txt"

    unsigned int parent_inode_num, new_inode_num, new_block_num;
    unsigned short new_dir_size;
    struct ext2_inode new_inode, parent_inode;

    //should check local path validity here!!!!!

    parent_inode_num = dir_entry_exists(p, 0);
    
    if(!(parent_inode_num)){
        fprintf(stderr, "Err: invalid inode number\n");
        exit(EINVAL);
    }

    if(dir_entry_exists(fpath,parent_inode_num)){
        fprintf(stderr, "Err: entry already exists\n");
        exit(EEXIST);
    }

    //check is a src is directory 
    new_inode_num = find_free(&ind_bmap);
    new_block_num = find_free(&blk_bmap);

    new_inode = (struct ext2_inode)IND_TABLE[new_inode_num - 1];
    parent_inode = (struct ext2_inode)IND_TABLE[parent_inode_num - 1];

    char* copied_file_name = fpath[0];
    int copied_file_name_len = strlen(fpath[0]);
    

    new_dir_size = TRUE_FILE_SIZE(copied_file_name_len);//***

    mk_new_file_inode(new_inode_num, path);
    mk_dir_entry(parent_inode_num, new_inode_num, new_dir_size, copied_file_name_len, EXT2_FT_REG_FILE, copied_file_name, copied_file_name_len);
   

    
}
	

int main(int argc, char **argv) {
    if(argc != 3) {
        fprintf(stderr, "Usage: %s <command img filepath>\n", argv[0]);
        exit(1);
    }
    
    int p_len = strlen(argv[3]);
    int s_len = strlen(argv[2]);
    
    int fd = open(argv[1], O_RDWR);
    char fp[p_len + 1]; 
    char sp[s_len + 1];  
    
    strcpy(&fp[0], argv[3]);
    strcpy(&sp[0], argv[2])
    
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


    ext2_cp(parent_path, child_path, sp);
	

    
	return 0;
}
