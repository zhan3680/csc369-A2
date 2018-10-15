#include <stdio.h>
#include <stdlib.h>



int main(int argc,char **argv) {
    if(argc!=2){
        printf("invalid!\n");
        return 1;
    }

    int times = strtol(argv[1],NULL,10);
    FILE *fp=fopen("moreSchedule.txt","w+");
    if(fp == NULL){
       perror("open:");
       return 2;
    }
    int id,in_dir,out_dir;
    for(id=1;id<=times;id++){
        in_dir=rand()%4;
        out_dir=rand()%4;
        while(out_dir==in_dir){
            out_dir=rand()%4;
        }
        fprintf(fp,"%d %d %d\n",id,in_dir,out_dir);
    }
}



