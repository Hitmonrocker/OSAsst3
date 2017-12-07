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

	//p nodes not used for directories
	int singleIndirectionPtrs[1];

	int doubleIndirectionPtrs[1];

} inode;


//root inode of the file system
static inode* root;


//Struct to hold inodes. Used in hashtable
typedef struct node {
	inode* inode;
	struct node* next;
	int fd;
} Node;



typedef struct hashtable {

	//capacityof hashtable array
	int capacity;

	//Load factor of hashtable
	double loadFactor;

	//Number of elements in hashtable
	int numElements;

	//Array of node*s
	Node** nodeArray;

} Hashtable;

/*Function to generate hashcode. The hash function
is the sum of all the ascii values of thecharacters 
in the word % the capacity of the hashtable;
*/
/*int hashCodePath(Hashtable* table, char* word) {
	char* ptr=word;
	int sum=0;
	while(*ptr!='\0') {
		sum+=(int)*ptr;
		ptr++;
	}
	int code=sum%(table->capacity);
	return code;
}*/

int hashCodeFd(Hashtable* table, int fd) {
	int code=(-1*fd)%(table->capacity);
	return code;
}

Node* searchByFD(Hashtable* table, int fd ) {
	int hashcode=hashCodeFd(table,fd);
	Node** nodeArray=table->nodeArray;
	Node* ptr=nodeArray[hashcode];
	while(ptr!=NULL) {
		if(fd==ptr->fd) {
			return ptr;
		}
		ptr=ptr->next;
	}

	return NULL;
}

void rebalance(Hashtable* table);
/*
	Function to insert a node into the hashtable
*/

void insert(Hashtable* table, Node* node) {

	//Generate a hashcode
	int hashcode=hashCodeFd(table, node->fd);

	node->next=table->nodeArray[hashcode];

	//Insert the new node to the front of the list
	table->nodeArray[hashcode]=node;

	//Update the number of elements in the hashtable
	table->numElements++;

	//If the ratio of elements to capacity of the hashtable is to great, rebalance
	if((double)(table->numElements)/(double)(table->capacity)>table->loadFactor) {
		rebalance(table);
	}
}

/*
	Function to delete a node from the hashtable given a file descriptor
*/

void delete(Hashtable* table, int fd) {

	//Generate hashcode
	int hashcode=hashCodeFd(table,fd);

	//Array of node*s
	Node** nodeArray=table->nodeArray;

	//Linked List of Node*s
	Node* ptr=nodeArray[hashcode];

	//Search throihg list for the fd and remove it from the table and free it
	Node* prev=NULL;
	while(ptr!=NULL) {
		if(fd==ptr->fd) {
			if(prev==NULL) {
				nodeArray[hashcode]=ptr->next;
				table->numElements--;
			} else {
				prev->next=ptr->next;
				table->numElements--;
			}

			free(ptr);
		}
		prev=ptr;
		ptr=ptr->next;
	}

}




/*Function to create a hashtable with a inputted capacity
*/
Hashtable* hashtableCreate(int size) {

	Hashtable* output=(Hashtable*)malloc(sizeof(Hashtable));

	if(output==NULL) {
		fprintf(stderr, "Error allocating memory to hashtable. Exiting...\n");
		exit(-1);
	}

	//Initial capacity is inputted size
	output->capacity=size;

	//Load factor is 1
	output->loadFactor=1.0;

	//Intial number of elements is 0
	output->numElements=0;


	//Allocate memory for an array of node pointers
	Node** nodeArray=(Node**)malloc(sizeof(Node*)*size);

	if (nodeArray==NULL)
	{
		fprintf(stderr, "Error allocating memory to array of Node pointers. Exiting...\n");
		exit(-1);
	}

	//Loop to intialize the hashtable array to empty Nodes
	int i=0;
	for(i;i<size;i++) {
		nodeArray[i]=NULL;
	}

	output->nodeArray=nodeArray;

	return output;

}

/*
	Function to rebalance the hashtable
*/

void rebalance(Hashtable* table) {

	//Pointer to temporary hash table
	Hashtable* newTable=hashtableCreate(table->capacity*2);

	int i=0;

	for(i;i<table->capacity;i++) {
		Node* ptr=table->nodeArray[i];
		while(ptr!=NULL) {
			Node* next=ptr->next;

			int hashcode=hashCodeFd(newTable, ptr->fd);
			ptr->next=newTable->nodeArray[hashcode];
			newTable->nodeArray[hashcode]=ptr;
			ptr=next;
			newTable->numElements++;
		}
	}

	free(table->nodeArray);

	//Set the hashtable pointed to by the inputted pointer to be the new table
	*table=*newTable;


	//Free pointer to temporary table
	free(newTable);
}

//Search for an inode with a path
/*inode* searchPath(Hashtable* table, char* path) {
	int hashcode=hashCodePath(table,path);
	Node** nodeArray=table->nodeArray;
	Node* ptr=nodeArray[hashcode];
	while(ptr!=NULL) {
		if(ptr->inode==NULL) {
			return NULL;
		}
		if(strcmp(path,ptr->inode->path)==0) {
			return ptr->inode;
		}
		ptr=ptr->next;
	}

	return NULL;
}*/


/*Function to insert a inode into the hashtable
*/
/*void insertPathTable(Hashtable* table, inode* node) {

	//Check to see if the inode already exists in the hashtable
	inode* exisitinginode=searchPath(table,node->path);
	if(exisitinginode!=NULL){

		fprintf(stderr, "inode already exists in table\n");
		return;
	}

	//Else generate a hashcode
	int hashcode=hashCodePath(table, node->path);

	//Allocate dynamic memory and initalize a new Node
	Node* newNode=(Node*)malloc(sizeof(Node));
	newNode->inode=node;
	newNode->next=table->nodeArray[hashcode];

	//Insert the new node to the front of the list
	table->nodeArray[hashcode]=newNode;

	//Update the number of elements in the hashtable
	table->numElements++;

	//If the ratio of elements to capacity of the hashtable is to great, rebalance
	if((double)(table->numElements)/(double)(table->capacity)>table->loadFactor) {
		rebalance(table);
	}
}*/








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

	int i=0;
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
    char fpath[PATH_MAX];
    
    log_msg("\nsfs_getattr(path=\"%s\", statbuf=0x%08x)\n",
	  path, statbuf);

    //If the inode is the root directory
    if ((strlen(path)==1)&&path[0]=='/') {
    	statbuf->st_uid = root->userId;
    	statbuf->st_gid = root->groupId;
    	statbuf->st_nlink = 1;
    	statbuf->st_ino = 0;
    	statbuf->st_mode=root->permissions;
    	statbuf->st_size=root->size;
    	statbuf->st_mtime=root->timeStamp;
    }

    //find the inode based off of the path and update statbuf





    
    return retstat;
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
    int retstat = 0;
    log_msg("sfs_unlink(path=\"%s\")\n", path);

    
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
