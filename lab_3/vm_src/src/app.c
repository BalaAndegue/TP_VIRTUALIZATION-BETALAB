#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

/* TODO */
void SAVE()
{
    //on utilise un port specifique pour signaler la migration au VM 
    // Le port 0x80 est souvent utilise pour des codes de diagnostic/debug
    asm volatile("outb %0, $0x80" : : "a"(0));
}

char *buff_1 = "Hello World !!!\n";
char *buff_2 = "Bye Bye !!!\n";

#define FILENAME "./ay_caramba.txt"

int main(void)
{
    int fd = open(FILENAME, O_RDWR | O_CREAT, 0777);
    write(fd, buff_1, strlen(buff_1));
    SAVE();
    write(fd, buff_2, strlen(buff_2));
    close(fd);
    return 0;
}
