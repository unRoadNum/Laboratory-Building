#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    int fd_src, fd_dst;
    char buf[1024];
    int line;
    if (argc < 3) {
      perror("./mycp src dst\n");
      exit(1);
    }

    fd_src = open(argv[1], O_RDONLY);
    if (fd_src < 0){
        perror("open src file fail!\n");
        exit(1);
    }

    fd_dst = open(argv[2], O_CREAT|O_RDWR, 0777);
    if (fd_dst < 0) {
        perror("open dst file fail!\n");
        exit(1);
    }

   while(line = read(fd_src, buf, sizeof(buf))) {
       write(fd_dst, buf, line);
   } 

   close(fd_src);
   close(fd_dst);
   return 0;
    
}
