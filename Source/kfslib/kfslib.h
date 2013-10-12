//
//  kfslib.h
//  KFS
//
//  Copyright (c) 2012, FadingRed LLC
//  All rights reserved.
//  
//  Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
//  following conditions are met:
//  
//    - Redistributions of source code must retain the above copyright notice, this list of conditions and the
//      following disclaimer.
//    - Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
//      following disclaimer in the documentation and/or other materials provided with the distribution.
//    - Neither the name of the FadingRed LLC nor the names of its contributors may be used to endorse or promote
//      products derived from this software without specific prior written permission.
//  
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
//  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
//  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
//  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#ifndef _KFS_H_
#define _KFS_H_

#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef int64_t kfsid_t;
typedef struct kfsfilesystem kfsfilesystem_t;
typedef struct kfsstat kfsstat_t;
typedef struct kfsstatfs kfsstatfs_t;
typedef struct kfsoptions kfsoptions_t;
typedef struct kfstime kfstime_t;
typedef struct kfscontents kfscontents_t;

typedef enum {
	KFS_REG,
	KFS_DIR,
	KFS_BLK,
	KFS_CHR,
	KFS_LNK,
	KFS_SOCK,
	KFS_FIFO,
} kfstype_t;

typedef enum {
	KFS_IRUSR = 0x400,
	KFS_IWUSR = 0x200,
	KFS_IXUSR = 0x100,
	KFS_IRGRP = 0x040,
	KFS_IWGRP = 0x020,
	KFS_IXGRP = 0x010,
	KFS_IROTH = 0x004,
	KFS_IWOTH = 0x002,
	KFS_IXOTH = 0x001,
} kfsmode_t;

typedef enum {
	KFSERR_PERM = EPERM,
	KFSERR_NOENT = ENOENT,
	KFSERR_IO = EIO,
	KFSERR_NXIO = ENXIO,
	KFSERR_ACCES = EACCES,
	KFSERR_EXIST = EEXIST,
	KFSERR_XDEV = EXDEV,
	KFSERR_NODEV = ENODEV,
	KFSERR_NOTDIR = ENOTDIR,
	KFSERR_ISDIR = EISDIR,
	KFSERR_INVAL = EINVAL,
	KFSERR_FBIG = EFBIG,
	KFSERR_NOSPC = ENOSPC,
	KFSERR_ROFS = EROFS,
	KFSERR_MLINK = EMLINK,
	KFSERR_NAMETOOLONG = ENAMETOOLONG,
	KFSERR_NOTEMPTY = ENOTEMPTY,
	KFSERR_DQUOT = EDQUOT,
} kfserror_t;

/*!
 \name		Main functionality
 \details	The following types and functions are creating and mounting user space filesystems.
 @{
 */// ----------------------------------------------------------------------------------------------------

/*!
 \brief		Stat a filesystem
 \details	Get statistics from the filesystem located at path. Set all attributes you
			support in the stat structure.
 */
typedef bool (*kfsstatfs_f)(const char *path, kfsstatfs_t *stat, int *error, void *context);

/*!
 \brief		Stat a file
 \details	Get statistics from the file located at path. Set all attributes you
			support in the stat structure. This should not follow symbolic links.
 */
typedef bool (*kfsstat_f)(const char *path, kfsstat_t *stat, int *error, void *context);

/*!
 \brief		Read from a file
 \details	Read length bytes from the file at path starting at offset. Read the data into the buffer which
			is guarenteed to be large enough to hold length bytes. Return -1 on error.
 */
typedef ssize_t (*kfsread_f)(const char *path, char *buf, size_t offset, size_t length, int *error, void *context);

/*!
 \brief		Write to a file
 \details	Write length bytes to the file at path starting at offset. Write the data from the buffer which
			is guarenteed to be large enough to hold length bytes. Return -1 on error.
 */
typedef ssize_t (*kfswrite_f)(const char *path, const char *buf, size_t offset, size_t length, int *error, void *context);

/*!
 \brief		Create a symbolic link
 \details	Create a link at path with the given value.
 */
typedef bool (*kfssymlink_f)(const char *path, const char *value, int *error, void *context);

/*!
 \brief		Read the contents of a link
 \details	Get the contents of the link at path, and return by reference a newly created
			string in value (you should allocate memeory for this). The caller will free
			the memory you allocate. This is not your responsibility.
 */
typedef bool (*kfsreadlink_f)(const char *path, char **value, int *error, void *context);

/*!
 \brief		Create a file
 \details	Create a file at the given path.
 */
typedef bool (*kfscreate_f)(const char *path, int *error, void *context);

/*!
 \brief		Remove a file
 \details	Remove a file at the given path.
 */
typedef bool (*kfsremove_f)(const char *path, int *error, void *context);

/*!
 \brief		Move a file
 \details	Move a file at the given path to the new path.
 */
typedef bool (*kfsrename_f)(const char *path, const char *new_path, int *error, void *context);

/*!
 \brief		Resize a file
 \details	Resize the file to the given size.
 */
typedef bool (*kfstruncate_f)(const char *path, uint64_t size, int *error, void *context);

/*!
 \brief		Change mode for a file
 \details	Change to the specified mode.
 */
typedef bool (*kfschmod_f)(const char *path, kfsmode_t mode, int *error, void *context);

/*!
 \brief		Change times for a file
 \details	Change the access and modification times of a file. If a time should be set,
			it will be non-null.
 */
typedef bool (*kfsutimes_f)(const char *path, const kfstime_t *atime, const kfstime_t *mtime, int *error, void *context);

/*!
 \brief		Create a directory
 \details	Create a directory at the given path.
 */
typedef bool (*kfsmkdir_f)(const char *path, int *error, void *context);

/*!
 \brief		Remove a directory
 \details	Remove a directory at the given path.
 */
typedef bool (*kfsrmdir_f)(const char *path, int *error, void *context);

/*!
 \brief		Get a directory's contents
 \details	Get the contents of the directory at path. Add file entries to the contents by calling
			kfscontents_append. Values are copied so you do not need to keep them in memory.
 */
typedef bool (*kfsreaddir_f)(const char *path, kfscontents_t *contents, int *error, void *context);

/*!
 \brief		
 \details	
 */
struct kfsoptions {
	const char *mountpoint;
};

/*!
 \brief		
 \details	
			Currently unsupported filesystem features:
				- No support for users/groups on files
				- No support for creating special file types
				- No support for hard links
 */
struct kfsfilesystem {
	kfsstatfs_f statfs;
	kfsstat_f stat;
	kfsread_f read;
	kfswrite_f write;
	kfssymlink_f symlink;
	kfsreadlink_f readlink;
	kfscreate_f create;
	kfsremove_f remove;
	kfsrename_f rename;
	kfstruncate_f truncate;
	kfschmod_f chmod;
	kfsutimes_f utimes;
	kfsmkdir_f mkdir;
	kfsrmdir_f rmdir;
	kfsreaddir_f readdir;
	kfsoptions_t options;
	void *context;
};

/*!
 \brief		Mout a filesystem
 \details	Mounts a filesystem and returns an identifier that you will need in order to unmount it.
			On error, this will return -1. The kfs library will not be able to tell when a filesystem
			is unmounted. You should make your best effort to call kfs_unmount explicitly (even if
			the system has already unmounted the filesystem) to reclaim identifiers and free memory
			used by the kfs library. This mount command will create the directory at the mountpoint
			if needed (but will not create intermediate directories).
 */
kfsid_t kfs_mount(const kfsfilesystem_t *filesystem);

/*!
 \brief		Unmount a filesystem
 \details	Give the identifier you received when mounting the filesystem.
 */
void kfs_unmount(kfsid_t identifier);

/*!
 \brief		KFS device name prefix
 \details	Name used when mounting devices.
 */
extern const char *kfs_devprefix;

/*!@}*/


/*!
 \name		Supporting functionality
 \details	The following types and functions are for use while implemeting the different callback
			methods required for implementation of a filesystem.
 @{
 */// ----------------------------------------------------------------------------------------------------

struct kfstime {
	uint64_t sec;
	uint64_t nsec;
};

struct kfsstat {
	kfstype_t type;
	kfsmode_t mode;
	uint64_t size;
	uint64_t used;
	kfstime_t atime;
	kfstime_t mtime;
	kfstime_t ctime;
};

struct kfsstatfs {
	uint64_t free;
	uint64_t size;
};

/*!
 \brief		Create a content listing
 \details	You must call destory unless you relinquish ownership at some point.
 */
kfscontents_t *kfscontents_create(void);

/*!
 \brief		Destory a content listing
 \details	Destory a content listing.
 */
void kfscontents_destroy(kfscontents_t *contents);

/*!
 \brief		Append an entry
 \details	Append an entry to this listing of contents. Once you append the entry,
			you relinquish ownership of it, so you do not need to destory it. This
			will be done for you.
 */
void kfscontents_append(kfscontents_t *contents, const char *entry);

/*!
 \brief		Get the count of a content listing
 \details	Get the count of a content listing.
 */
uint64_t kfscontents_count(kfscontents_t *contents);

/*!
 \brief		Get an entry in a content listing
 \details	Get an entry in a content listing. This method returns NULL if the
			index is out of range.
 */
const char *kfscontents_at(kfscontents_t *contents, uint64_t index);

/*!@}*/


/*!
 \name		Threading helper functionality
 \details	The following methods allow you to add callback methods for when the kfs library uses threads.
			You should call these before doing anything else with the kfs library.
 @{
 */// ----------------------------------------------------------------------------------------------------

void kfs_set_thread_begin_callback(void (*)(void));
void kfs_set_thread_end_callback(void (*)(void));

/*!@}*/


/*!
 \name		Errors
 \details	Error numbers set by the kfs library.
 @{
 */// ----------------------------------------------------------------------------------------------------

#define EKFS_INTR	(ELAST+1) /* Internal error */
#define EKFS_EMFS	(ELAST+2) /* Max number of filesystems exceeded */

/*!
 \brief		Write an error message
 \details	Like perror. Uses perror for unknown errors.
 */
void kfs_perror(const char *s);

/*!@}*/

#endif
