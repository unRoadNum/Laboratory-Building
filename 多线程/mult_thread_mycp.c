#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

// 每次copy的大小是512M
#define  COPY_BLOCK_SIZE  512*1024*1024

typedef struct subThreadPara{
	int index; // copy地址索引
	char *srcAddr;  // 源起始地址
	char *dstAddr; // 目的起始地址
	int size;  // copy的每个块长度
	int len;  // copy长度
	int *record; // 进度记录地址
}stSubThreadPara;

/*
 * 子线程copy主任务;
 * 参数：
 */
void* th_copy(void *arg)
{
	stSubThreadPara *pInfo = (stSubThreadPara*)arg;
	char *pSrcAddr = pInfo->srcAddr + pInfo->index * pInfo->size;
	char *pDstAddr = pInfo->dstAddr + pInfo->index * pInfo->size;
	memcpy(pDstAddr, pSrcAddr, pInfo->len);
	*(char*)pInfo->record = 1;
	return (void*)0;
}

int main(int argc, char *argv[])
{
	int ret = 0;
	int fd_src, fd_dst;
	int len_src, len_dst;
	char *p_src;
	char *p_dst;
	pthread_t tid;
	stSubThreadPara  stSubThreadPara;
	int i, size, last_len, tmp;
	int max_num;
	int *pRecord, *pTmp;
	
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
	fd_src = open(argv[1], O_RDWR);
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
	close(fd_dst);

	// 暂时不考虑超过3个G的文件copy

	// 写入目标文件一个字节，拓展目标文件
	len_dst = lseek(fd_dst, len_src, SEEK_SET);
	write(fd_dst, "\0", 1);

	// 映射目标文件
	p_src = mmap(NULL, len_dst, PROT_WRITE | PROT_READ, MAP_SHARED, fd_dst, 0);
	close(fd_src);

	max_num = atoi(argv[3]);
	size = len_src / max_num;
	last_len = len_src % max_num;

	(void)memset(&stSubThreadPara, 0, sizeof(stSubThreadPara));
	stSubThreadPara.index = i;
	stSubThreadPara.size = size;
	stSubThreadPara.dstAddr = p_dst;
	stSubThreadPara.srcAddr = p_src;

	pRecord = (int*)malloc(max_num*sizeof(int));
	if (pRecord == NULL) {
		printf("Get record space fail.\n");
		return -1;
	}

	for (i = 0; i < max_num -1; i++) {
		stSubThreadPara.len = size;
		stSubThreadPara.record = pRecord + i * sizeof(int);
		pthread_create(&tid, NULL, th_copy, (void*)&stSubThreadPara);
		pthread_detach(tid);
	}

	if (last_len != 0) {
		stSubThreadPara.len = last_len;
		stSubThreadPara.record = pRecord + (max_num-1) * sizeof(int);
		pthread_create(&tid, NULL, th_copy, (void*)&stSubThreadPara);
		pthread_detach(tid);
	}

	// 计算进度条
	tmp = 0;
	while(1) {
		for (i=0; i<max_num; i++) {
			pTmp = (int*)(pRecord + i*sizeof(int));
			if(*pTmp == 1) {
				printf(" *");
				*pTmp = 0;
				tmp++;
			}
		}
		if (tmp == max_num) {
			printf("\n copy finished.\n");
			break;
		}
	}

	free(pRecord);

	munmap(p_dst, len_dst);
	munmap(p_src, len_src);
	
	return 0;
	
}
