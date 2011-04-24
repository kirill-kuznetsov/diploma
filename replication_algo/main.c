/* 
 * File:   main.c
 * Author: kirill
 *
 * Created on 7 Апрель 2011 г., 22:43
 */

#include <stdio.h>
#include <stdlib.h>
#include <asm/fcntl.h>

#include "cs_crush.h"

/*
 * 
 */

static void map_init(char* map_file){
    int max_map_size = 512;
    char buf[max_map_size];
    int ifd, n;
;
    ifd = open(map_file,O_RDONLY);
    n = read(ifd, buf, max_map_size);
    map = crush_decode((void*)buf, (void*)buf + n);

    close(ifd);
}

int main(int argc, char** argv) {
    printf("before if");
    if(argc < 2)
    {
        printf("zero arg");
        map_init ("./map");
    }
    else
    {

        printf("non zero");
        map_init(argv[1]);
    }
    return (EXIT_SUCCESS);
}


