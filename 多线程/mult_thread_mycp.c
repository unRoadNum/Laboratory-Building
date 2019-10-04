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
	printf("[%s:%d] index=%d, record=%p\n",
		__FILE__, __LINE__, pInfo->index, pInfo->record);
	*(pInfo->record) = 1;
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
	stSubThreadPara  *pstSubThreadPara;
	int i, size, last_len, tmp;
	int max_num;
	int *pRecord; 
	int *pTmp;
	
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
	p_src = mmap(NULL, len_src, PROT_WRITE | PROT_READ, MAP_SHARED, fd_src, 0);
	close(fd_src);

	// 暂时不考虑超过3个G的文件copy

	// 写入目标文件一个字节，拓展目标文件
	len_dst = lseek(fd_dst, len_src, SEEK_SET);
	write(fd_dst, "\0", 1);

	// 映射目标文件
	p_dst = mmap(NULL, len_dst, PROT_WRITE | PROT_READ, MAP_SHARED, fd_dst, 0);
	close(fd_dst);

	max_num = atoi(argv[3]);
	size = len_src / max_num;
	last_len = len_src % max_num;
	printf("[%s:%d] len_src=%d, max_num=%d, size=%d, last_len=%d\n",
		__FILE__, __LINE__, len_src, max_num, size, last_len);

	pstSubThreadPara = (stSubThreadPara*)malloc(max_num *sizeof(stSubThreadPara));
	if (pstSubThreadPara == NULL) {
		printf("[%s:%d] Get thread para space fail.\n", __FILE__, __LINE__);
		return -1;
	}
	(void)memset(pstSubThreadPara, 0, max_num * sizeof(stSubThreadPara));

	pRecord = (int*)malloc(max_num*sizeof(int));
	if (pRecord == NULL) {
		printf("Get record space fail.\n");
		return -1;
	}
	memset(pRecord, 0, max_num*sizeof(int));

	for (i = 0; i < max_num-1; i++) {
		pstSubThreadPara->size = size;
		pstSubThreadPara->dstAddr = p_dst;
		pstSubThreadPara->srcAddr = p_src;
		pstSubThreadPara->index = i;
		pstSubThreadPara->len = size;
		pstSubThreadPara->record = pRecord + i * sizeof(int);
		printf("[%s:%d] index=%d, record=%p\n", 
			__FILE__, __LINE__, i, pstSubThreadPara->record);
		pthread_create(&tid, NULL, th_copy, (void*)pstSubThreadPara);
		pthread_detach(tid);
		pstSubThreadPara++;
	}

	if (last_len != 0) {
		pstSubThreadPara->size = size;
		pstSubThreadPara->dstAddr = p_dst;
		pstSubThreadPara->srcAddr = p_src;
		pstSubThreadPara->index = max_num-1;
		pstSubThreadPara->len = last_len;
		pstSubThreadPara->record = pRecord + (max_num-1) * sizeof(int);
		printf("[%s:%d] index=%d, record=%p\n",
			__FILE__, __LINE__, pstSubThreadPara->index, pstSubThreadPara->record);
		pthread_create(&tid, NULL, th_copy, (void*)pstSubThreadPara);
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
