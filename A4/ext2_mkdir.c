#include "util.h"

void mk_new_dir_inode(int inode_num, int parent_inode_num){
    int new_block_num = find_free_block();

    struct ext2_dir_entry *parent_dir_ent, *new_dir_ent;
    parent_dir_ent = (struct ext2_dir_entry *)(disk + new_block_num * EXT2_BLOCK_SIZE);
    parent_dir_ent->inode = parent_inode_num;
    parent_dir_ent->name_len = 2;
    parent_dir_ent->rec_len = TRUE_FILE_SIZE(2);
    parent_dir_ent->file_type = EXT2_FT_REG_FILE;
    strncpy(parent_dir_ent->name, "..", 2);

    new_dir_ent = (struct ext2_dir_entry *)(disk + new_block_num  * EXT2_BLOCK_SIZE + parent_dir_ent->rec_len);
    new_dir_ent->inode = inode_num;
    new_dir_ent->name_len = 1;
    new_dir_ent->rec_len = TRUE_FILE_SIZE(1);
    new_dir_ent->file_type = EXT2_FT_REG_FILE;
    strncpy(new_dir_ent->name, ".", 1);

    struct ext2_inode new_inode = IND_TABLE[INDTBL_INDEX(inode_num)];
    struct ext2_inode parent_inode = IND_TABLE[INDTBL_INDEX(parent_inode_num)];
    new_inode.i_block[0] = new_block_num;
    new_inode.i_mode = EXT2_S_IFDIR;

    new_inode.i_ctime = (unsigned int)time(NULL);
    new_inode.i_dtime = 0;
    new_inode.i_links_count = 0;

    new_inode.i_links_count += 1;
    parent_inode.i_links_count += 1;

    set_used_bit(&ind_bmap);
    set_used_bit(&blk_bmap);

}
	

void ext2_mkdir(char **p, char **dpath){

    int parent_inode_num,new_inode_num, new_dir_size;
    
    parent_inode_num = dir_entry_exists(p, EXT2_ROOT_INO);

    if(!(parent_inode_num)){
    	fprintf(stderr, "Err: invalid inode number\n");
    	exit(EINVAL);
	}

	// If the path doesn't consist of a single directory/node, check to make sure
	// that it doesn't exist already. 
    if(!dir_entry_exists(dpath, parent_inode_num)){
        fprintf(stderr, "Err: entry already exists\n");
        exit(EEXIST);
    }
	
	new_inode_num = find_free_inode();

    
    char *new_dir_name = dpath[0];
    int new_dir_name_len = strlen(dpath[0]);


    new_dir_size = TRUE_FILE_SIZE(new_dir_name_len);
    mk_new_dir_inode(new_inode_num, parent_inode_num);
    mk_dir_entry(parent_inode_num, new_inode_num, new_dir_size, new_dir_name_len, EXT2_FT_DIR, new_dir_name, new_dir_name_len);   
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


	ext2_mkdir(parent_path, child_path);


	return 0;
}