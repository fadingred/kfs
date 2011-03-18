//
//  test.c
//  KFS
//
//  Copyright (c) 2011, FadingRed LLC
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <libgen.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <KFS/kfslib.h>

typedef struct {
	const char *backing;
} test_context_t;

const char *test_backingpath(const char *path, test_context_t *context);
const char *test_backingpath(const char *path, test_context_t *context) {
	static char result[PATH_MAX];
	snprintf(result, PATH_MAX, "%s%s", context->backing, path);
	return result;
}

bool test_statfs(const char *path, kfsstatfs_t *result, void *context);
bool test_statfs(const char *path, kfsstatfs_t *result, void *context) {
	bool success = false;
	struct statfs sbuf;
	if (statfs(test_backingpath(path, context), &sbuf) == 0) {
		result->free = sbuf.f_bfree;
		result->size = sbuf.f_bsize;
		success = true;
	}
	return success;
}

bool test_stat(const char *path, kfsstat_t *result, void *context);
bool test_stat(const char *path, kfsstat_t *result, void *context) {
	bool success = false;
	struct stat sbuf;
	if (lstat(test_backingpath(path, context), &sbuf) == 0) {
		result->size = sbuf.st_size;
		result->atime = (kfstime_t){ sbuf.st_atimespec.tv_sec, sbuf.st_atimespec.tv_nsec };
		result->mtime = (kfstime_t){ sbuf.st_mtimespec.tv_sec, sbuf.st_mtimespec.tv_nsec };
		result->ctime = (kfstime_t){ sbuf.st_ctimespec.tv_sec, sbuf.st_ctimespec.tv_nsec };
		result->used = sbuf.st_blocks * sbuf.st_blksize;

		result->mode = 0;
		if (sbuf.st_mode & S_IRUSR) { result->mode |= KFS_IRUSR; }
		if (sbuf.st_mode & S_IWUSR) { result->mode |= KFS_IWUSR; }
		if (sbuf.st_mode & S_IXUSR) { result->mode |= KFS_IXUSR; }
		if (sbuf.st_mode & S_IRGRP) { result->mode |= KFS_IRGRP; }
		if (sbuf.st_mode & S_IWGRP) { result->mode |= KFS_IWGRP; }
		if (sbuf.st_mode & S_IXGRP) { result->mode |= KFS_IXGRP; }
		if (sbuf.st_mode & S_IROTH) { result->mode |= KFS_IROTH; }
		if (sbuf.st_mode & S_IWOTH) { result->mode |= KFS_IWOTH; }
		if (sbuf.st_mode & S_IXOTH) { result->mode |= KFS_IXOTH; }
		
		result->type = KFS_REG;
		if (S_ISREG(sbuf.st_mode)) { result->type = KFS_REG; }
		if (S_ISDIR(sbuf.st_mode)) { result->type = KFS_DIR; }
		if (S_ISBLK(sbuf.st_mode)) { result->type = KFS_BLK; }
		if (S_ISCHR(sbuf.st_mode)) { result->type = KFS_CHR; }
		if (S_ISLNK(sbuf.st_mode)) { result->type = KFS_LNK; }
		if (S_ISSOCK(sbuf.st_mode)) { result->type = KFS_SOCK; }
		if (S_ISFIFO(sbuf.st_mode)) { result->type = KFS_FIFO; }

		success = true;
	}
	return success;
}

ssize_t test_read(const char *path, char *buf, size_t offset, size_t length, void *context);
ssize_t test_read(const char *path, char *buf, size_t offset, size_t length, void *context) {
	ssize_t result = -1;
	int fd = open(test_backingpath(path, context), O_RDONLY);
	if (fd >= 0) {
		lseek(fd, offset, SEEK_SET);
		result = read(fd, buf, length);
		close(fd);
	}
	return result;
}

ssize_t test_write(const char *path, const char *buf, size_t offset, size_t length, void *context);
ssize_t test_write(const char *path, const char *buf, size_t offset, size_t length, void *context) {
	ssize_t result = -1;
	int fd = open(test_backingpath(path, context), O_WRONLY);
	if (fd >= 0) {
		lseek(fd, offset, SEEK_SET);
		result = write(fd, buf, length);
		close(fd);
	}
	return result;
}

bool test_symlink(const char *path, const char *value, void *context);
bool test_symlink(const char *path, const char *value, void *context) {
	return symlink(value, test_backingpath(path, context)) == 0;
}

bool test_readlink(const char *path, char **value, void *context);
bool test_readlink(const char *path, char **value, void *context) {
	bool success = false;
	char *result = malloc(sizeof(char) * PATH_MAX);
	ssize_t length = readlink(test_backingpath(path, context), result, PATH_MAX);
	if (length >= 0 && length < PATH_MAX) {
		result[length] = '\0';
		success = true;
	}
	*value = result;
	return success;
}

bool test_create(const char *path, void *context);
bool test_create(const char *path, void *context) {
	bool success = false;
	int fd = open(test_backingpath(path, context), O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (fd >= 0) {
		close(fd);
		success = true;
	}
	return success;
}

bool test_remove(const char *path, void *context);
bool test_remove(const char *path, void *context) {
	return unlink(test_backingpath(path, context)) == 0;
}

bool test_rename(const char *path, const char *new_path, void *context);
bool test_rename(const char *path, const char *new_path, void *context) {
	// need to dup the to path since test backing stores value in a static buffer
	char *to = strdup(test_backingpath(new_path, context));
	int result = rename(test_backingpath(path, context), to);
	free(to);
	return result == 0;
}

bool test_truncate(const char *path, uint64_t size, void *context);
bool test_truncate(const char *path, uint64_t size, void *context) {
	return truncate(test_backingpath(path, context), size) == 0;
}

bool test_chmod(const char *path, kfsmode_t mode, void *context);
bool test_chmod(const char *path, kfsmode_t mode, void *context) {
	mode_t set = 0;
	if (mode & KFS_IRUSR) { set |= S_IRUSR; }
	if (mode & KFS_IWUSR) { set |= S_IWUSR; }
	if (mode & KFS_IXUSR) { set |= S_IXUSR; }
	if (mode & KFS_IRGRP) { set |= S_IRGRP; }
	if (mode & KFS_IWGRP) { set |= S_IWGRP; }
	if (mode & KFS_IXGRP) { set |= S_IXGRP; }
	if (mode & KFS_IROTH) { set |= S_IROTH; }
	if (mode & KFS_IWOTH) { set |= S_IWOTH; }
	if (mode & KFS_IXOTH) { set |= S_IXOTH; }
	return chmod(test_backingpath(path, context), set) == 0;
}

bool test_utimes(const char *path, const kfstime_t *atime, const kfstime_t *mtime, void *context);
bool test_utimes(const char *path, const kfstime_t *atime, const kfstime_t *mtime, void *context) {
	struct timeval times[2];
	gettimeofday(&times[0], NULL);
	gettimeofday(&times[1], NULL);
	if (atime) {
		times[0].tv_sec = atime->sec;
		times[0].tv_usec = atime->nsec / 1000.0;
	}
	if (mtime) {
		times[1].tv_sec = mtime->sec;
		times[1].tv_usec = mtime->nsec / 1000.0;
	}
	return utimes(test_backingpath(path, context), times) == 0;
}

bool test_mkdir(const char *path, void *context);
bool test_mkdir(const char *path, void *context) {
	return mkdir(test_backingpath(path, context), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == 0;
}

bool test_rmdir(const char *path, void *context);
bool test_rmdir(const char *path, void *context) {
	return rmdir(test_backingpath(path, context)) == 0;
}

bool test_readdir(const char *path, kfscontents_t *contents, void *context);
bool test_readdir(const char *path, kfscontents_t *contents, void *context) {
	bool success = false;
	DIR *dir = opendir(test_backingpath(path, context));
	if (dir) {
		struct dirent *entry = NULL;
		while ((entry = readdir(dir))) {
			kfscontents_append(contents, entry->d_name);
		}
		
		closedir(dir);
		success = true;
	}
	return success;
}


#define cmdassert(command, fail_format, ...) do { \
	if (system(command) != 0) { \
		fprintf(stderr, "test failed (%s:%i): %s\n" fail_format "\n", \
			basename(__FILE__), __LINE__, command, ##__VA_ARGS__); \
		return 1; \
	} \
} while (0)

#define cmdassert_empty(command, fail_format, ...) \
	cmdassert("[[ `" command " 2>&1` == '' ]]", fail_format,  ##__VA_ARGS__)
#define cmdassert_unkwn(command, fail_format, ...) \
	cmdassert("[[ `" command " 2>&1` != '' ]]", fail_format,  ##__VA_ARGS__)
#define cmdassert_match(command, outmatch, fail_format, ...) \
	cmdassert("[[ `" command " 2>&1` =~ '" outmatch "' ]]", fail_format,  ##__VA_ARGS__)



int runtests(void);

int main() {
	test_context_t context = {
		.backing = "/tmp/kfstest/backing",
	};
	
	// set up the backing dir & parent
	mkdir(dirname((char *)context.backing), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
	mkdir(context.backing, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
	
	kfsfilesystem_t filesystem = {
		.statfs = test_statfs,
		.stat = test_stat,
		.read = test_read,
		.write = test_write,
		.symlink = test_symlink,
		.readlink = test_readlink,
		.create = test_create,
		.remove = test_remove,
		.rename = test_rename,
		.truncate = test_truncate,
		.chmod = test_chmod,
		.utimes = test_utimes,
		.mkdir = test_mkdir,
		.rmdir = test_rmdir,
		.readdir = test_readdir,
		.options = {
			.mountpoint = "/tmp/kfstest/mount",
		},
		.context = &context,
	};
	
	kfsid_t fsid = kfs_mount(&filesystem);
	if (fsid < 0) { kfs_perror("mount"); return 1; }

	int result = runtests();

	kfs_unmount(fsid);

	// cleanup
	cmdassert("rm -r /tmp/kfstest", "cleanup");

	return result;
}

int runtests(void) {
	// list empty directory
	cmdassert_unkwn("ls -ld /tmp/kfstest/mount", "ls");
	
	// create a file
	cmdassert_empty("echo tmp > /tmp/kfstest/mount/file", "create file");
	cmdassert_empty("echo hello > /tmp/kfstest/mount/file", "truncate file");
	cmdassert_match("ls   /tmp/kfstest/mount", "file", "directory entry not created");
	cmdassert_unkwn("cat  /tmp/kfstest/mount/file", "read file");
	cmdassert_empty("diff /tmp/kfstest/mount/file /tmp/kfstest/backing/file", "backing file not created properly");
	
	// append to file
	cmdassert_empty("echo world >> /tmp/kfstest/mount/file", "append to file");
	cmdassert_match("cat /tmp/kfstest/mount/file", "hello\nworld", "read file post append");
	
	// chmod the file
	cmdassert_empty("chmod 444 /tmp/kfstest/mount/file", "chmod file");
	cmdassert_match("bash -c 'echo hello >> /tmp/kfstest/mount/file'", "Permission denied", "append readonly file");
	cmdassert_empty("chmod 644 /tmp/kfstest/mount/file", "chmod file");
	
	// move the file
	cmdassert_empty("mv /tmp/kfstest/mount/file /tmp/kfstest/mount/file2", "rename file");
	cmdassert_match("cat /tmp/kfstest/backing/file2", "hello\nworld", "read file post rename");
	cmdassert_empty("mv /tmp/kfstest/mount/file2 /tmp/kfstest/mount/file", "rename file again");
	
	// move files into and out of mount point
	cmdassert_empty("echo move > /tmp/kfstest/mount/moveme", "create file");
	cmdassert_empty("mv /tmp/kfstest/mount/moveme /tmp/kfstest/tmpfile", "rename to outside mount");
	cmdassert_empty("mv /tmp/kfstest/tmpfile /tmp/kfstest/mount/moved", "rename to inside mount");
	cmdassert_empty("mv /tmp/kfstest/mount/file /tmp/kfstest/tmpfile", "rename to outside mount");
	cmdassert_empty("mv /tmp/kfstest/tmpfile /tmp/kfstest/mount/file", "rename to inside mount");

	// create a symlink to the file
	cmdassert_empty("ln -s file /tmp/kfstest/mount/filelink", "create symlink");
	cmdassert_match("ln -s file /tmp/kfstest/mount/filelink", "File exists", "create dup symlink");
	cmdassert_match("ls -l /tmp/kfstest/mount", "filelink -> file", "symlink dir entry not created");
	cmdassert_match("cat   /tmp/kfstest/mount/filelink", "hello\nworld", "read symlink");

	// change modificaiton time of the file
	cmdassert_empty("touch -m -t 201102211100.00 /tmp/kfstest/mount/file", "touch the file");
	cmdassert_match("stat -f '%m' /tmp/kfstest/mount/file", "1298307600", "get modification time");
	cmdassert_empty("touch -m -t 201102211100.01 /tmp/kfstest/mount/file", "touch the file again");
	cmdassert_match("stat -f '%m' /tmp/kfstest/mount/file", "1298307601", "get modification time again");
	
	// remove the file
	cmdassert_empty("rm  /tmp/kfstest/mount/file", "remove file");
	cmdassert_match("rm  /tmp/kfstest/mount/file", "No such file or directory", "remove unknown file (not allowed)");
	cmdassert_match("cat /tmp/kfstest/mount/filelink", "No such file or directory", "read dead symlink (not allowed)");
	
	// create a directory
	cmdassert_empty("mkdir /tmp/kfstest/mount/dir", "create directory");
	cmdassert_match("mkdir /tmp/kfstest/mount/dir", "File exists", "create dup directory");
	cmdassert_match("ls    /tmp/kfstest/mount", "dir", "directory entry not created");
	cmdassert_empty("echo hello world > /tmp/kfstest/mount/dir/file", "create file in dir");
	cmdassert_match("rmdir /tmp/kfstest/mount/dir", "Directory not empty", "remove dir with contents (not allowed)");
	cmdassert_empty("rm -r /tmp/kfstest/mount/dir", "recursive remove dir with contents");
	cmdassert_match("rmdir /tmp/kfstest/mount/dir", "No such file or directory", "remove missing dir");
	
	return 0;
}
