#include <openssl/md5.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

char * hashConvert(char* filepath){
    int n;
    MD5_CTX c;
    char buf[512];
    ssize_t bytes;
    unsigned char out[MD5_DIGEST_LENGTH];

    MD5_Init(&c);
    int fd = open(filepath,O_RDONLY);
    bytes=read(fd, buf, 512);
    while(bytes > 0)
    {
        MD5_Update(&c, buf, bytes);
        bytes=read(fd, buf, 512);
    }

    MD5_Final(out, &c);//int n =0;
    /*for(n=0;n<MD5_DIGEST_LENGTH;n++)
       printf("%02x",out[n]);
    printf("\n");*/
char *result = (char*)malloc(MD5_DIGEST_LENGTH * sizeof(char));
    memset(result,'\0',strlen(result));
    char s[32];
    int i;
    for(i=0; i<16;++i){
     sprintf(s,"%02x",out[i]);
     strcat(result,s);
     }
    //printf("%s\n",result); 
       // printf("%02x", out[n]);
    //printf("\n");*/
   return result;           
}
