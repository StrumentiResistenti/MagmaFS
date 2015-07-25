#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int main()
{
	struct stat s;
	printf("    st_dev: %d\n", sizeof(s.st_dev));
	printf("    st_ino: %d\n", sizeof(s.st_ino));
	printf("   st_mode: %d\n", sizeof(s.st_mode));
	printf("  st_nlink: %d\n", sizeof(s.st_nlink));
	printf("    st_uid: %d\n", sizeof(s.st_uid));
	printf("    st_gid: %d\n", sizeof(s.st_gid));
	printf("   st_rdev: %d\n", sizeof(s.st_rdev));
	printf("   st_size: %d\n", sizeof(s.st_size));
	printf("st_blksize: %d\n", sizeof(s.st_blksize));
	printf(" st_blocks: %d\n", sizeof(s.st_blocks));
	printf("  st_atime: %d\n", sizeof(s.st_atime));
	printf("  st_mtime: %d\n", sizeof(s.st_mtime));
	printf("  st_ctime: %d\n", sizeof(s.st_ctime));
	printf("\n");
	printf("    struct: %d\n", sizeof(s));

	return 0;
}
