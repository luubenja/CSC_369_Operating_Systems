#include "util.h"
#include "time.h"

/*creates new inode with i_mode type symlink, stores path in the i_blocks
*/
void mk_new_symlink_inode(unsigned int src_inode_num, unsigned int link_inode_num, char* link_path){
    unsigned int new_block_num, path_len;

    path_len = sizeof(*link_path);
    new_block_num = find_free(&blk_bmap);

    char* path = (char*)(disk + new_block_num * (EXT2_BLOCK_SIZE));
    strncpy(path, link_path, path_len);

    struct ext2_inode new_inode = IND_TABLE[link_inode_num - 1];
    struct ext2_inode src_inode = IND_TABLE[src_inode_num - 1];

    new_inode.i_block[0] = new_block_num;

    new_inode.i_ctime = (unsigned int)time(NULL);
    new_inode.i_dtime = 0;
    new_inode.i_links_count = 0;
    
    src_inode.i_links_count += 1;

    set_used_bit(&ind_bmap);
    set_used_bit(&blk_bmap);

}
	
void ext2_ln(char **src, char** link_parent, char** lpath, int is_softlink, char* abs_path){


    unsigned int src_inode_num, link_inode_num, link_type, new_inode_num;
    unsigned short new_link_size;

    src_inode_num = dir_entry_exists(src, 0);
    link_inode_num = dir_entry_exists(link_parent, 0);


    if(!(src_inode_num)){
        fprintf(stderr, "Err: invalid inode number\n");
        exit(ENOENT);
    }

    if(!(link_inode_num)){
        fprintf(stderr, "Err: invalid inode number\n");
        exit(EINVAL);
    }
    if(dir_entry_exists(lpath,link_inode_num)){
        fprintf(stderr, "Err: entry already exists at that location\n");
        exit(EEXIST);
    }

    if(IS_DIR(IND_TABLE[src_inode_num].i_mode)){
        fprintf(stderr, "Err: cannot create link to directory\n");
        exit(EISDIR); 
    }

    if(is_softlink){
        link_type = EXT2_FT_SYMLINK;
        new_inode_num = find_free(&ind_bmap);
        mk_new_symlink_inode(src_inode_num, link_inode_num, abs_path);
    }
    else{
        link_type = EXT2_FT_REG_FILE;
        new_inode_num = src_inode_num;

    }

    char* link_file_name = lpath[0];//***
    int link_file_name_len = strlen(lpath[0]);//***
    
    //everything here is the same as mkdir
    new_link_size = TRUE_FILE_SIZE(link_file_name_len);//***
    mk_dir_entry(link_inode_num, new_inode_num, new_link_size,link_file_name_len, link_type,link_file_name, link_file_name_len);
    

    
}
	

int main(int argc, char **argv) {
	
	if(argc != 3 || argc != 4) {
        fprintf(stderr, "Usage: %s <command img filepath>\n", argv[0]);
        exit(1);
    }
    
    int syml = 0;
    if(argc == 4){
    	syml = 1; 	
    }
    
    int p_len = strlen(argv[2]);
    int s_len = strlen(argv[3])+2;
    
    int fd = open(argv[1], O_RDWR);
    char fp[p_len + 1]; 
    char sp[s_len+1];
    
	strcpy(&fp[0], argv[2]);
    strcpy(&sp[0], argv[3]);
	
	normalize_path(&fp[0]);
    normalize_path(&sp[0]);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
        
    init_globals();
    
    char*** srcpaths = create_path(&sp[0]);
    char*** paths = create_path(&fp[0]);


    char** parent_path = paths[0];
    char** child_path = paths[1];
    
    ext2_ln(srcpaths[0], parent_path, child_path, syml, fp);
    
	return 0;
}