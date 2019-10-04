#include <pthread.h>

// 每次copy的大小是512M
#define  COPY_BLOCK_SIZE  512*1024*1024

int main(int argc, char *arv[])
{
	int ret = 0;
	int fd_src, fd_dst;
	int len_src, len_dst;
	char *p_src;
	char *p_dst;
	
	if (argc < 4) {
		printf("Usage: ./mycp src dst N");
		return -1;
	}

	// 创建目标文件
	fd_dst = open(argv[2], O_CREAT|O_RDWR, 0777);
	if (fd_dst < 0) {
		perror("Create dst file!\n");
		return -1;
	}

	// 打开源文件
	fd_src = open(argv[2], O_RDWR);
	if (fd_src < 0) {
		perror("Create src file.\n");
		close(fd_dst);
		return -1;
	}

	// 获取源文件长度
	len_src = lseek(fd_src, 0, SEEK_END);
	if (len_src < 0) {
		perror("Get source file length fail.\n");
		close(fd_dst);
		close(fd_src);
		return -1;
	}
	p_dst = mmap(NULL, len_src, PROT_WRITE | PROT_READ, MAP_SHARED, fd_src, 0);
	

	// 暂时不考虑超过3个G的文件copy

	// 写入目标文件一个字节，拓展目标文件
	len_dst = lseek(fd_dst, len_src, SEEK_SET)
	write(fd_dst, "\0", 1);

	// 映射目标文件
	p_src = mmap(NULL, len_dst, PROT_WRITE | PROT_READ, MAP_SHARED, fd_dst, 0)
	
	return 0;
	
}
