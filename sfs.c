/*
  Simple File System

  This code is derived from function prototypes found /usr/include/fuse/fuse.h
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  His code is licensed under the LGPLv2.

*/

#include "params.h"
#include "block.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

#include "log.h"


///////////////////////////////////////////////////////////
//
// Prototypes for all these functions, and the C-style comments,
// come indirectly from /usr/include/fuse.h
//

/**
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *
 * Introduced in version 2.3
 * Changed in version 2.6
 */

//Assuming 100 mib file and 512 byte discs
static int numBlocks = 204800;

static char freeArray[204800];

static int blockSize=516;

static int fdCount=0;

static int maxFileSizeBlocks=16612;

static int numInodes=1000;



typedef struct _pnode {
	int ptrs[128];
} pnode;

typedef struct _inode {
	//0 means directory 1 means file 2 means not in use
	char mode;

	//size in bytes
	int size;

	//block number on the disk, also the inode number
	int blockNumber;

	uid_t userId;
	gid_t groupId;

	//file name
	char path[64];

	//last modified timestamp
	time_t timeStamp;

	int permissions;

	int directMappedPtrs[100];

	int singleIndirectionPtrs[1];

	int doubleIndirectionPtrs[1];

} inode;


//root inode of the file system
static inode* root;

void *sfs_init(struct fuse_conn_info *conn)
{
	struct sfs_state* state = SFS_DATA;

	char* disk=state->diskfile;

	//set up the root inode

	root=malloc(sizeof(inode));

	root->mode=0;
	root->blockNumber=0;
	root->size=512;
	root->userId=getuid();
	root->groupId=getegid();
	root->permissions=S_IFDIR | S_IRWXU;
	root->timeStamp=time(NULL);
	root->singleIndirectionPtrs[0]=-1;
	root->doubleIndirectionPtrs[0]=-1;
	memcpy(root->path,"/",1);

	root->directMappedPtrs[0]=0;

	int i=1;
	for(i;i<100;i++) {
		root->directMappedPtrs[i]=-1;
	}


	disk_open(disk);

	//write the root inode to disk

	block_write(0,(void*)root);

	//set up all inodes and write them to the disk
	i=1;
	for(i;i<1000;i++) {
		inode newInode;
		newInode.mode=2;
		newInode.blockNumber=i;
		newInode.size=-1;
		newInode.userId=getuid();
		newInode.groupId=-getegid();
		newInode.permissions=-1;
		newInode.timeStamp=time(NULL);
		newInode.singleIndirectionPtrs[0]=-1;
		newInode.doubleIndirectionPtrs[0]=-1;
		int j=0;
		for(j;j<100;j++) {
			newInode.directMappedPtrs[j]=-1;
		}

		block_write(i,&newInode);

	}

	//Initialize the free array

	freeArray[0]=1;
	i=1;
	for(i;i<numBlocks;i++) {
		freeArray[i]=0;
	}

    fprintf(stderr, "in bb-init\n");
    log_msg("\nsfs_init()\n");

    log_conn(conn);
    log_fuse_context(fuse_get_context());

    disk_close();

    return state;
}

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void sfs_destroy(void *userdata)
{
    log_msg("\nsfs_destroy(userdata=0x%08x)\n", userdata);
}

/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
int sfs_getattr(const char *path, struct stat *statbuf)
{
    int retstat = 0;
    //char fpath[PATH_MAX];

    log_msg("\nsfs_getattr(path=\"%s\", statbuf=0x%08x)\n",
	  path, statbuf);

    //If the inode is the root directory
    if ((strlen(path)==1)&&path[0]=='/') {
    	statbuf->st_uid = root->userId;
    	statbuf->st_gid = root->groupId;
    	statbuf->st_nlink = 1;
    	statbuf->st_mode=root->permissions;
    	statbuf->st_size=root->size;
    	statbuf->st_mtime=root->timeStamp;

    	return retstat;
    }

    else {
    	//Get the disk path
    	char* disk=SFS_DATA->diskfile;

    	//open the disk
    	disk_open(disk);

    	//buffer to read into
    	char buffer[512];

    	//Read in the root node
    	block_read(0,buffer);
    	inode* rootDir=(inode*)buffer;

    	//Search through all the direct map ptrs
    	int i=1;
    	for(i;i<100;i++) {

    		//block num referenced by ptr
    		int blocknum=rootDir->directMappedPtrs[i];

    		//if valid ptr
    		if(blocknum>0) {

    			//read in inode
    			char buffer2[512];
    			block_read(blocknum,buffer2);
    			inode* tempNode=(inode*)buffer2;

    			//Compares paths for match
    			if(strcmp(tempNode->path,path+1)==0) {
    				statbuf->st_uid = tempNode->userId;
			    	statbuf->st_gid = tempNode->groupId;
			    	statbuf->st_nlink = 1;
			    	statbuf->st_mode=tempNode->permissions;
			    	statbuf->st_size=tempNode->size;
			    	statbuf->st_mtime=tempNode->timeStamp;
			    	disk_close();
			    	return retstat;
    			}
    		}
    	}

    	//Get block referred to by single indirection ptrs
    	int pNodeBlock=rootDir->singleIndirectionPtrs[0];

    	//if not in ue return
    	if(pNodeBlock<=0) {
    		disk_close();
    		return 1;
    	}

    	//read in pnode
    	block_read(pNodeBlock,buffer);
    	pnode* pNode=(pnode*)buffer;

    	//For each inode referenced by pnode
    	i=0;
    	for(i;i<128;i++) {

    		//get the block of the inode
    		int iNodeBlock=pNode->ptrs[i];

    		//if valid ptr
    		if(iNodeBlock>0) {

    			//read in the inode
    			char buffer2[512];
    			block_read(iNodeBlock,buffer2);
    			inode* tempNode=(inode*)buffer2;

    			//Check for path match
    			if(strcmp(tempNode->path,path+1)==0) {
    				statbuf->st_uid = tempNode->userId;
			    	statbuf->st_gid = tempNode->groupId;
			    	statbuf->st_nlink = 1;
			    	statbuf->st_mode=tempNode->permissions;
			    	statbuf->st_size=tempNode->size;
			    	statbuf->st_mtime=tempNode->timeStamp;
			    	disk_close();
			    	return retstat;
    			}
    		}
    	}
    }

    disk_close();
    return 1;
}

/**
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 *
 * Introduced in version 2.5
 */
int sfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_create(path=\"%s\", mode=0%03o, fi=0x%08x)\n",
	    path, mode, fi);


    return retstat;
}

/** Remove a file */
int sfs_unlink(const char *path)
{
		inode* myRoot=NULL;
		inode* cursor=NULL;
    int retstat = 0;
    log_msg("sfs_unlink(path=\"%s\")\n", path);

		//find root i-node
		struct sfs_state* state = SFS_DATA;
		char* file=state->diskfile;

		disk_open(file);
		block_read(0, myRoot);
		//find the i-node associated with that filehandle
		int i=1;
		int j=0;
		for(i=1; i < 1000; i++){
			//get i-node from memory
			block_read(i, cursor);
			//if the paths match the i-node was found
			//reset the i-node then write back to file
			if(strcmp(cursor->path,path) == 0){
				cursor->mode=2;
				cursor->size=-1;
				cursor->userId=getuid();
				cursor->groupId=-getegid();
				//cursor->path=NULL;
				cursor->timeStamp=time(NULL);
				cursor->permissions=-1;
				//TODO:update free array-->single and double indirection
				cursor->singleIndirectionPtrs[0]=-1;
				cursor->doubleIndirectionPtrs[0]=-1;
				for(j;j<100;j++) {
					freeArray[cursor->directMappedPtrs[j]]=0;
					cursor->directMappedPtrs[j]=-1;
				}
				//write the updated cursor back to disk
				block_write(i, cursor);
			}
		}
		disk_close();
    return retstat;
}

/** File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * return an arbitrary filehandle in the fuse_file_info structure,
 * which will be passed to all file operations.
 *
 * Changed in version 2.2
 */
int sfs_open(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_open(path\"%s\", fi=0x%08x)\n",
	    path, fi);


    return retstat;
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int sfs_release(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_release(path=\"%s\", fi=0x%08x)\n",
	  path, fi);


    return retstat;
}

/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.  An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 */
int sfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	    path, buf, size, offset, fi);

    return retstat;
}

/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 * Changed in version 2.2
 */
int sfs_write(const char *path, const char *buf, size_t size, off_t offset,
	     struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	    path, buf, size, offset, fi);


    return retstat;
}


/** Create a directory */
int sfs_mkdir(const char *path, mode_t mode)
{
    int retstat = 0;
    log_msg("\nsfs_mkdir(path=\"%s\", mode=0%3o)\n",
	    path, mode);


    return retstat;
}


/** Remove a directory */
int sfs_rmdir(const char *path)
{
    int retstat = 0;
    log_msg("sfs_rmdir(path=\"%s\")\n",
	    path);


    return retstat;
}


/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this  directory
 *
 * Introduced in version 2.3
 */
int sfs_opendir(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_opendir(path=\"%s\", fi=0x%08x)\n",
	  path, fi);


    return retstat;
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */
int sfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	       struct fuse_file_info *fi)
{
	 log_msg("\nreaddir\n");
    int retstat = 0;


    return retstat;
}

/** Release directory
 *
 * Introduced in version 2.3
 */
int sfs_releasedir(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;


    return retstat;
}

struct fuse_operations sfs_oper = {
  .init = sfs_init,
  .destroy = sfs_destroy,

  .getattr = sfs_getattr,
  .create = sfs_create,
  .unlink = sfs_unlink,
  .open = sfs_open,
  .release = sfs_release,
  .read = sfs_read,
  .write = sfs_write,

  .rmdir = sfs_rmdir,
  .mkdir = sfs_mkdir,

  .opendir = sfs_opendir,
  .readdir = sfs_readdir,
  .releasedir = sfs_releasedir
};

void sfs_usage()
{
    fprintf(stderr, "usage:  sfs [FUSE and mount options] diskFile mountPoint\n");
    abort();
}

int main(int argc, char *argv[])
{
	printf("i node size : %d\n", sizeof(inode) );
	printf("p node size : %d\n", sizeof(pnode) );
    int fuse_stat;
    struct sfs_state *sfs_data;

    // sanity checking on the command line
    if ((argc < 3) || (argv[argc-2][0] == '-') || (argv[argc-1][0] == '-'))
	sfs_usage();

    sfs_data = malloc(sizeof(struct sfs_state));
    if (sfs_data == NULL) {
	perror("main calloc");
	abort();
    }

    // Pull the diskfile and save it in internal data
    sfs_data->diskfile = argv[argc-2];
    argv[argc-2] = argv[argc-1];
    argv[argc-1] = NULL;
    argc--;

    sfs_data->logfile = log_open();

    // turn over control to fuse
    fprintf(stderr, "about to call fuse_main, %s \n", sfs_data->diskfile);
    fuse_stat = fuse_main(argc, argv, &sfs_oper, sfs_data);
    fprintf(stderr, "fuse_main returned %d\n", fuse_stat);

    return fuse_stat;
}
