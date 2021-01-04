#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/types.h>
#include <limits.h>
#include "hash.h"
typedef struct Node {
    char *tag;
    char *version;
    char *pathName;
    char *hashcode;
    struct Node* next;
}node;

node *serverHead = NULL;
node *clientHead = NULL;
node *updateHead = NULL;
int getFileStartIdx(char *path){
  int i = strlen(path)-1;
  while(path[i]!='/'){
    i--;
  }
  return i;
}
// 0 makes the dir
// 1 already exists
void  OpenDir(char *s){
  DIR *directory;
  directory = opendir(s);
  if(directory==NULL){
    mkdir(s,0777);
  }
}
void createDir(char *path){
int FileStartIndex = getFileStartIdx(path);
 char tmpPath[strlen(path)+2];
 memset(tmpPath,'\0',sizeof(tmpPath));
 int i =0; int k =0;
 for(i=0;i<FileStartIndex;i++){
   while(path[i]!='/'){
    tmpPath[k]=path[i];
    k++;i++;
   }
   OpenDir(tmpPath);
   if(path[i]=='/'){
    tmpPath[k]='/';
     k++;
   }
 }
}
// reads the message from the socket based on the protocol
//reads till ":" whatever is read till then say 56 then 56 bytes
//are read and returned.
char * getUpdatedManifest(int *socketfd){
  int s = 40000;
  char *clientMessage = (char*)malloc(s*sizeof(char));
  bzero(clientMessage,strlen(clientMessage));
  int readIn = 0; int n = 0;
    do{
     n = read(*socketfd,clientMessage+readIn,1);
     if(n<0){
      perror("read failure");exit(1);
     }
     readIn+=n;

    }while(strstr(clientMessage,":")==NULL);
    //printf("Client message: %s\n", clientMessage);
    readIn=0;n=0;
    char *e;
    int delimIdx;
    e = strchr(clientMessage,':');
    delimIdx = (int)(e-clientMessage);//printf("%d\n",delimIdx);
    char *bytesToRead = (char*)malloc((delimIdx +1)*sizeof(char));
    memset(bytesToRead,'\0',strlen(bytesToRead));
    memcpy(bytesToRead,clientMessage,(delimIdx));
    int nextRead = atoi(bytesToRead);
    //printf("next Read: %d\n", nextRead);
    bzero(clientMessage,strlen(clientMessage));
    do{
     n = read(*socketfd,clientMessage+readIn,nextRead);
     if(n<0){
       perror("Read from socket failed");
     }
     readIn+=n;
     if(readIn>=s){
       s = (nextRead+20);
       char *tmp = (char*)malloc(strlen(clientMessage)*sizeof(char));
       bzero(tmp,strlen(tmp));
       strcpy(tmp,clientMessage);
       clientMessage = (char*)realloc(tmp,s*(sizeof(char)));
     }
    }while(n>0 && readIn<nextRead);
    //printf("Client Message: %s\n",clientMessage);
    return clientMessage;
}
void getDirectories(char *startDir,int *socketfd){
  node *tmp = serverHead;
  if(tmp==NULL){
    createDir(startDir);
    return;
  }
  while(tmp!=NULL){
    createDir(tmp->pathName);
    int length = strlen(tmp->pathName);
    if((tmp->next)==NULL){
      length+=strlen("^");
    }
    char strPathLen[20];
    memset(strPathLen,'\0',sizeof(length));
    sprintf(strPathLen,"%d",length);
    char protocol[strlen(tmp->pathName)+strlen(":")+strlen(strPathLen)+strlen("^")];
    memset(protocol,'\0',sizeof(protocol));
    strcat(protocol,strPathLen);
    strcat(protocol,":");
    strcat(protocol,tmp->pathName);
    if(tmp->next==NULL){
      strcat(protocol,"^");
    }
    int w = write(*socketfd,protocol,strlen(protocol));
    int fd = open(tmp->pathName,O_CREAT|O_RDWR,00600);
    char *contentToWrite = getUpdatedManifest(socketfd);
     w = write(fd,contentToWrite,strlen(contentToWrite));
    tmp=tmp->next;
  }

}

void traverseUpdate(int *socketfd,char *dir){
   node *tmp = updateHead;
   int isAddDone=0;
   if(tmp==NULL){
	 char makeUpdatePath[strlen(dir)+strlen("/.Update")];
     memset(makeUpdatePath,'\0',sizeof(makeUpdatePath));
     strcat(makeUpdatePath,dir);
     strcat(makeUpdatePath,"/.Update");
     remove(makeUpdatePath);
     printf("Server is upto date\n");
     return;
   }
   while(tmp!=NULL){
    if(strcmp(tmp->tag,"D")==0){
      remove(tmp->pathName);
    }else if((strcmp(tmp->tag,"A")==0)||strcmp(tmp->tag,"M")==0){
      int len = 0;
      len = strlen(tmp->pathName);
      if(tmp->next==NULL){
        len += strlen("^");
      }
       char length[20];
       memset(length,'\0',sizeof(length));
       sprintf(length,"%d",len);
     char protocol[strlen("upg")+strlen(tmp->pathName)+strlen(":")+strlen(length)+strlen("^")];
     memset(protocol,'\0',sizeof(protocol));
      if(!isAddDone){
       strcat(protocol,"upg");
      }
       strcat(protocol,length);
       strcat(protocol,":");
       strcat(protocol,tmp->pathName);
       //printf("%s ",protocol);
       //int wr = write(*socketfd,protocol,strlen(protocol));
       isAddDone=1;
       if(tmp->next==NULL){
        strcat(protocol,"^");
       }
      // printf("%s ",protocol);
       int wr = write(*socketfd,protocol,strlen(protocol));
       isAddDone =1;
       if(strcmp(tmp->tag,"A")==0){
          createDir(tmp->pathName);
       }
       else{
       remove(tmp->pathName);
       }
       char *contentsToWrite = getUpdatedManifest(socketfd);
       int sfd = open(tmp->pathName,O_CREAT|O_RDWR,00600);
       wr = write(sfd,contentsToWrite,strlen(contentsToWrite));
       if(wr<0){
         perror("cannot open\n");exit(1);
       }
    }
    tmp=tmp->next;

   }
   int pathLength = strlen(dir)+strlen("/.Manifest");
   char strLen[20];
   memset(strLen,'\0',sizeof(strLen));
   sprintf(strLen,"%d",pathLength);
   char pathManifest[strlen(dir)+strlen("/.Manifest")+strlen(strLen)+strlen(":")+strlen("^")];
   memset(pathManifest,'\0',sizeof(pathManifest));
   strcat(pathManifest,strLen);
   strcat(pathManifest,":");
   strcat(pathManifest,dir);
   strcat(pathManifest,"/.Manifest");
   //strcat(pathManifest,"^");
   int wr = write(*socketfd,pathManifest,strlen(pathManifest));
   char *manifestC = getUpdatedManifest(socketfd);
   memset(pathManifest,'\0',sizeof(pathManifest));
   strcat(pathManifest,dir);
   strcat(pathManifest,"/.Manifest");
   remove(pathManifest);
   int fd = open(pathManifest,O_CREAT|O_RDWR,00600);
   wr = write(fd,manifestC,strlen(manifestC));
   char makeUpdatePath[strlen(dir)+strlen("/.Update")];
   memset(makeUpdatePath,'\0',sizeof(makeUpdatePath));
   strcat(makeUpdatePath,dir);
   strcat(makeUpdatePath,"/.Update");
   remove(makeUpdatePath);
}
void addToUpdateLL(char *t,char*p,char*h){
  if(updateHead==NULL){
    updateHead = (node*)malloc(sizeof(node));
    updateHead->tag = (char*)malloc((strlen(t)+1)*sizeof(char));
    bzero(updateHead->tag,strlen(updateHead->tag));
    memcpy((updateHead->tag),t,strlen(t));

    updateHead->pathName = (char*)malloc((strlen(p)+1)*sizeof(char));
    bzero(updateHead->pathName,strlen(updateHead->pathName));
    memcpy((updateHead->pathName),p,strlen(p));

    updateHead->hashcode = (char*)malloc((strlen(h)+1)*sizeof(char));
    bzero(updateHead->hashcode,strlen(updateHead->hashcode));
    memcpy((updateHead->hashcode),h,strlen(h));
    updateHead->next=NULL;
    return;
  }
  node *current = updateHead;
  node *prev = NULL;
  while(current!=NULL){
    prev = current;
    current=current->next;
  }
  node *newEntry = (node*)malloc(sizeof(node));
  newEntry->tag = (char*)malloc((strlen(t)+1)*sizeof(char));
    bzero(newEntry->tag,strlen(newEntry->tag));
    memcpy((newEntry->tag),t,strlen(t));

    newEntry->pathName = (char*)malloc((strlen(p)+1)*sizeof(char));
    bzero(newEntry->pathName,strlen(newEntry->pathName));
    memcpy((newEntry->pathName),p,strlen(p));

    newEntry->hashcode = (char*)malloc((strlen(h)+1)*sizeof(char));
    bzero(newEntry->hashcode,strlen(newEntry->hashcode));
    memcpy((newEntry->hashcode),h,strlen(h));
    newEntry->next=current;
    prev->next=newEntry;
}
void tokenizeUpdate(char *update){
 int TabCounter=0;int i=0;int k =0;int l=0;int TabCounterLim=2;
 char *tag;char *path; char *hashcode;
 char tmpStr[strlen(update)];
 memset(tmpStr,'\0',sizeof(tmpStr));
 for(i=l;i<strlen(update);i++){
   while(update[i]!='\t'){
    tmpStr[k]= update[i];
    k++;i++;
   }
   if((update[i]=='\t') && (TabCounter<TabCounterLim)){
     TabCounter++;
     if(TabCounter<TabCounterLim){
       tag = (char*)malloc((k+3)*sizeof(char));
       bzero(tag,strlen(tag));
       memcpy(tag,tmpStr,strlen(tmpStr));
       memset(tmpStr,'\0',sizeof(tmpStr));
       k=0;
     }
   }
   if(TabCounter==TabCounterLim){
      path = (char*)malloc((k+3)*sizeof(char));
      bzero(path,strlen(path));
      memcpy(path,tmpStr,strlen(tmpStr));
      memset(tmpStr,'\0',sizeof(tmpStr));
      k=0;
     if(update[i]=='\t'){
       i++;
     }
     while(update[i]!='\n'){
      tmpStr[k]=update[i];k++;i++;
     }
     hashcode= (char*)malloc((k+3)*sizeof(char));
     bzero(hashcode,strlen(hashcode));
     memcpy(hashcode,tmpStr,strlen(tmpStr));
     memset(tmpStr,'\0',sizeof(tmpStr));
     addToUpdateLL(tag,path,hashcode);
     k =0;TabCounter=0;
   }
 }

}
//given a manifest data, it reads till \n
//and finds the current version converts it to char
//and then returns it in int form.
int getVersion(char * manifestData){
  char *e;
    int delimIdx;
    e = strchr(manifestData,'\n');
    delimIdx = (int)(e-manifestData);//printf("%d\n",delimIdx);
    char *version = (char*)malloc((delimIdx +1)*sizeof(char));
    memset(version,'\0',strlen(version));
    memcpy(version,manifestData,(delimIdx));
    int currVersion = atoi(version);
    return currVersion;
}
int doActualUpdate(int ufd, int cfd){
  node *tmpServer = serverHead;
  node *tmpClient = clientHead;
  int isFileThere =0;
  int tagA = 0; int tagR =0;
  int conflict=0;
  if((tmpServer==NULL)&&(tmpClient==NULL)){
    return;
  }
  while(tmpClient!=NULL){
    isFileThere =0;
    if(tmpClient->tag!=NULL){
      if(strcmp(tmpClient->tag,"A")==0){
        tagA=1;tagR=0;
      }else if(strcmp(tmpClient->tag,"R")==0){
        tagR=1;tagA=0;
      }
    }
     while(tmpServer!=NULL){
      if(strcmp(tmpClient->pathName,tmpServer->pathName)==0){
        isFileThere=1;
        if(tagR){
          if(!conflict){
          char toWrite[strlen("A\t")+strlen(tmpServer->pathName)+strlen("\t")+strlen(tmpServer->hashcode)+strlen("\n")];
          bzero(toWrite,sizeof(toWrite));
          strcat(toWrite,"A\t");
          strcat(toWrite,tmpServer->pathName);
          strcat(toWrite,"\t");
          strcat(toWrite,tmpServer->hashcode);
          strcat(toWrite,"\n");
          int wr = write(ufd,toWrite,strlen(toWrite));
         }
         printf("A\t%s\n",tmpServer->pathName);
        }
       else if(strcmp(tmpClient->version,tmpServer->version)!=0){
          if(strcmp(tmpClient->hashcode,tmpServer->hashcode)!=0){
            char *hash = hashConvert(tmpClient->pathName);
            if(strcmp(hash,tmpClient->hashcode)==0){
              if(!conflict){
                char toWrite[strlen("M\t")+strlen(tmpServer->pathName)+strlen("\t")+strlen(tmpServer->hashcode)+strlen("\n")];
                bzero(toWrite,sizeof(toWrite));
                strcat(toWrite,"M\t");
                strcat(toWrite,tmpServer->pathName);
                strcat(toWrite,"\t");
                strcat(toWrite,tmpServer->hashcode);
                strcat(toWrite,"\n");
                int wr = write(ufd,toWrite,strlen(toWrite));

              }
              printf("M\t%s\n",tmpClient->pathName);
            }else{
              conflict=1;
              char toWrite[strlen("C\t")+strlen(tmpServer->pathName)+strlen("\t")+strlen(hash)+strlen("\n")];
              bzero(toWrite,sizeof(toWrite));
              strcat(toWrite,"C\t");
              strcat(toWrite,tmpServer->pathName);
              strcat(toWrite,"\t");
              strcat(toWrite,hash);
              strcat(toWrite,"\n");
              int wr = write(cfd,toWrite,strlen(toWrite));
              printf("C\t%s\n",tmpClient->pathName);
            }
          }
        }
      }
      tmpServer = tmpServer->next;
     }tmpServer=serverHead;
     if(!isFileThere){
       if(!conflict){
       char toWrite[strlen("D\t")+strlen(tmpClient->pathName)+strlen("\t")+strlen(tmpClient->hashcode)+strlen("\n")];
        bzero(toWrite,sizeof(toWrite));
        strcat(toWrite,"D\t");
        strcat(toWrite,tmpClient->pathName);
        strcat(toWrite,"\t");
        strcat(toWrite,tmpClient->hashcode);
        strcat(toWrite,"\n");
        int wr = write(ufd,toWrite,strlen(toWrite));
       }
        printf("D\t%s\n",tmpClient->pathName);
      }
   tmpClient = tmpClient->next;
   }
   tmpClient=clientHead;
   isFileThere=0;
   while(tmpServer!=NULL){
     isFileThere =0;
    while(tmpClient!=NULL){
      if(strcmp(tmpClient->pathName,tmpServer->pathName)==0){
        isFileThere=1;
      }
      tmpClient = tmpClient->next;
     }tmpClient=clientHead;
     if(isFileThere==0){
       char toWrite[strlen("A\t")+strlen(tmpServer->pathName)+strlen("\t")+strlen(tmpServer->hashcode)+strlen("\n")];
          bzero(toWrite,sizeof(toWrite));
          strcat(toWrite,"A\t");
          strcat(toWrite,tmpServer->pathName);
          strcat(toWrite,"\t");
          strcat(toWrite,tmpServer->hashcode);
          strcat(toWrite,"\n");
          int wr = write(ufd,toWrite,strlen(toWrite));
          printf("A\t%s\n",tmpServer->pathName);
         }
        tmpServer = tmpServer->next;
      }
   return conflict;
 }
void addToManifestLL(char *t,char *v, char *p, char*h,int isTagPresent){
  if(clientHead==NULL){
    clientHead = (node*)malloc(sizeof(node));
    if(isTagPresent){
     clientHead->tag = (char*)malloc((strlen(t)+1)*sizeof(char));
     bzero(clientHead->tag,strlen(clientHead->tag));
     memcpy((clientHead->tag),t,strlen(t));
    }
    clientHead->version = (char*)malloc((strlen(v)+1)*sizeof(char));
    bzero(clientHead->version,strlen(clientHead->version));
    memcpy((clientHead->version),v,strlen(v));

    clientHead->pathName = (char*)malloc((strlen(p)+1)*sizeof(char));
    bzero(clientHead->pathName,strlen(clientHead->pathName));
    memcpy((clientHead->pathName),p,strlen(p));

    clientHead->hashcode = (char*)malloc((strlen(h)+1)*sizeof(char));
    bzero(clientHead->hashcode,strlen(clientHead->hashcode));
    memcpy((clientHead->hashcode),h,strlen(h));
    clientHead->next=NULL;
    return;
  }
  node *current = clientHead;
  node *prev = NULL;
  while(current!=NULL){
    prev = current;
    current=current->next;
  }
  node *newEntry = (node*)malloc(sizeof(node));
  if(isTagPresent){
     newEntry->tag = (char*)malloc((strlen(t)+1)*sizeof(char));
     bzero(newEntry->tag,strlen(newEntry->tag));
     memcpy((newEntry->tag),t,strlen(t));
    }
    newEntry->version = (char*)malloc((strlen(v)+1)*sizeof(char));
    bzero(newEntry->version,strlen(newEntry->version));
    memcpy((newEntry->version),v,strlen(v));

    newEntry->pathName = (char*)malloc((strlen(p)+1)*sizeof(char));
    bzero(newEntry->pathName,strlen(newEntry->pathName));
    memcpy((newEntry->pathName),p,strlen(p));

    newEntry->hashcode = (char*)malloc((strlen(h)+1)*sizeof(char));
    bzero(newEntry->hashcode,strlen(newEntry->hashcode));
    memcpy((newEntry->hashcode),h,strlen(h));
    newEntry->next=current;
    prev->next=newEntry;
}
void clientManifestLL(char *manifest){
  int TabCounter=0;int i=0;int k =0;int l=0;int TabCounterLim=2;
 char *version;char *path; char *hashcode;char *tag;
 char tmpStr[strlen(manifest)+10];
 memset(tmpStr,'\0',sizeof(tmpStr));
 while(manifest[l]!='\n'){
  l++;k++;
 }
 l++;k=0;
 for(i=l;i<strlen(manifest);i++){
   while(manifest[i]!='\t'){
    tmpStr[k]= manifest[i];
    k++;i++;
   }
   if((strcmp(tmpStr,"A")==0)||(strcmp(tmpStr,"R")==0)){
      // memset(tmpStr,'\0',strlen(tmpStr));
     tag = (char*)malloc((k+3)*sizeof(char));
     bzero(tag,strlen(version));
     memcpy(tag,tmpStr,strlen(tmpStr));
     memset(tmpStr,'\0',sizeof(tmpStr));
     k=0;
     i++;
     while(manifest[i]!='\t'){
      tmpStr[k]= manifest[i];
      k++;i++;
     }
     version = (char*)malloc((k+3)*sizeof(char));
     bzero(version,strlen(version));
     memcpy(version,tmpStr,strlen(tmpStr));
     memset(tmpStr,'\0',sizeof(tmpStr));
     k=0;
     i++;
       while(manifest[i]!='\t'){
       tmpStr[k]= manifest[i];
       k++;i++;
       }
       path = (char*)malloc((k+3)*sizeof(char));
       bzero(path,strlen(path));
       memcpy(path,tmpStr,strlen(tmpStr));
       memset(tmpStr,'\0',sizeof(tmpStr));
       k=0;
       i++;
       while(manifest[i]!='\n'){
       tmpStr[k]=manifest[i];k++;i++;
       }
       hashcode= (char*)malloc((k+3)*sizeof(char));
       bzero(hashcode,strlen(hashcode));
       memcpy(hashcode,tmpStr,strlen(tmpStr));
       memset(tmpStr,'\0',sizeof(tmpStr));
       addToManifestLL(tag,version,path,hashcode,1);
       k =0;TabCounter=0;
       continue;
      }
     if((manifest[i]=='\t') && (TabCounter<TabCounterLim)){
     TabCounter++;
     if(TabCounter<TabCounterLim){
       version = (char*)malloc((k+3)*sizeof(char));
       bzero(version,strlen(version));
       memcpy(version,tmpStr,strlen(tmpStr));
       memset(tmpStr,'\0',sizeof(tmpStr));
       k=0;
     }
   }
   if(TabCounter==TabCounterLim){
      path = (char*)malloc((k+3)*sizeof(char));
      bzero(path,strlen(path));
      memcpy(path,tmpStr,strlen(tmpStr));
      memset(tmpStr,'\0',sizeof(tmpStr));
      k=0;
     if(manifest[i]=='\t'){
       i++;
     }
     while(manifest[i]!='\n'){
      tmpStr[k]=manifest[i];k++;i++;
     }
     hashcode= (char*)malloc((k+3)*sizeof(char));
     bzero(hashcode,strlen(hashcode));
     memcpy(hashcode,tmpStr,strlen(tmpStr));
     memset(tmpStr,'\0',sizeof(tmpStr));
     addToManifestLL(tag,version,path,hashcode,0);
     k =0;TabCounter=0;
   }
 }
}
// reads the message from the socket based on the protocol
//reads till ":" whatever is read till then say 56 then 56 bytes
//are read and returned.
/*char * getUpdatedManifest(int *socketfd){
  int s = 40000;
  char *clientMessage = (char*)malloc(s*sizeof(char));
  bzero(clientMessage,strlen(clientMessage));
  int readIn = 0; int n = 0;
    do{
     n = read(*socketfd,clientMessage+readIn,1);
     if(n<0){
      perror("read failure");exit(1);
     }
     readIn+=n;

    }while(strstr(clientMessage,":")==NULL);
    printf("Client message: %s\n", clientMessage);
    readIn=0;n=0;
    char *e;
    int delimIdx;
    e = strchr(clientMessage,':');
    delimIdx = (int)(e-clientMessage);//printf("%d\n",delimIdx);
    char *bytesToRead = (char*)malloc((delimIdx +1)*sizeof(char));
    memset(bytesToRead,'\0',strlen(bytesToRead));
    memcpy(bytesToRead,clientMessage,(delimIdx));
    int nextRead = atoi(bytesToRead);
    printf("next Read: %d\n", nextRead);
    bzero(clientMessage,strlen(clientMessage));
    do{
     n = read(*socketfd,clientMessage+readIn,nextRead);
     if(n<0){
       perror("Read from socket failed");
     }
     readIn+=n;
     if(readIn>=s){
       s = (nextRead+20);
       char *tmp = (char*)malloc(strlen(clientMessage)*sizeof(char));
       bzero(tmp,strlen(tmp));
       strcpy(tmp,clientMessage);
       clientMessage = (char*)realloc(tmp,s*(sizeof(char)));
     }
    }while(n>0 && readIn<nextRead);
    printf("Client Message: %s\n",clientMessage);
    return clientMessage;
}*/
//returns the contents of the files based on the file descriptor
//passed in.
char *getContent(int fd){
  int size = 10000;
 char *content = (char*)malloc(size*sizeof(char));
  memset(content,'\0',strlen(content));
  int readIn = 0; int status =0;
  do{
    status = read(fd,content+readIn,size-readIn);
    if(status<0){
     perror("Read failed");exit(1);
    }
    readIn+=status;
    if(readIn>=size){
       size = 2*size;
       char *tmp = (char*)malloc(strlen(content)*sizeof(char));
       bzero(tmp,strlen(tmp));
       strcpy(tmp,content);
       content = (char*)realloc(tmp,size*(sizeof(char)));
     }

   }while(status>0 && readIn<size);
 return content;
}
int numberOfEndLines(char* str){
    int count = 0;int i;
    for(i = 0; i < strlen(str); i++){
        if(str[i] == '\n'){
            count++;
        }
    }
    return count;
}

char** endLineSeparate(char* str){
    char** arrayOfLine =(char**) malloc(sizeof(char*) * numberOfEndLines(str));
    int i;
    for(i = 0; i < numberOfEndLines(str); i++){
        arrayOfLine[i] = malloc(sizeof(char) * strlen(str));
        memset(arrayOfLine[i], '\0', strlen(str));
    }
    char* str1 = malloc(sizeof(char) * strlen(str));
    strcpy(str1, str);
   // printf("%s\n", str1);
    char* str2 = malloc(sizeof(char) * strlen(str));
    memset(str2, '\0', strlen(str)*sizeof(char));
    //str2 = "";
    int count = 0;
    int counter = 0;
    for(i = 0; i < strlen(str); i++){
        if(str[i] == '\n'){
            strcpy(arrayOfLine[count], str2);
            memset(str2, '\0', strlen(str)*sizeof(char));
            count++;
        }else{
            char a = str[i];
            strncat(str2, &a, 1);
            //counter++;
        }
    }
    return arrayOfLine;
}

char* pathSeparate(char* str){
    char* str1 = (char*)malloc(sizeof(char) * strlen(str));
    memset(str1,'\0', strlen(str));
    //str1 = "";
    int count = 0;int i;
    for(i = 0; i < strlen(str); i++){
        if(count == 2){
            while(str[i] != '\t'){
                char a = str[i];
                strncat(str1, &a, 1);
                i++;
            }
            return str1;
        }
        if(str[i] == '\t'){
            count++;
        }
    }
    return str1;
}
// makes Commit file in the project and returns the file descriptor
//proj is where the commit to be made so proj/.Commit
int makeCommit(char *proj){
  char commitProg[strlen(proj)+strlen("/.Commit")];
  bzero(commitProg,strlen(commitProg));
  strcat(commitProg,proj);
  strcat(commitProg,"/.Commit");
   int fd = open(commitProg,O_CREAT|O_RDWR,00600);
   if(fd<0){
     perror("Cannot create or open .Commit");
     return fd;
   }
   return fd;
}
//Adds the content to Commit file in case commit passes
void addToCommit(char mark, char* v, char *path, char *hashcode,char *proj,int *fdp){
 // printf("M: %c  ", mark);
 int version = atoi(v);
 if(mark=='M'){
  version++;
 }
  char intVersionNo[strlen(v)+1];
  memset(intVersionNo,'\0',sizeof(intVersionNo));
  sprintf(intVersionNo,"%d",version);
   char toWrite[sizeof(mark)+strlen(v)+strlen(path)+strlen(hashcode)+(3*strlen("\t"))+strlen("\n")];
   memset(toWrite,'\0',sizeof(toWrite));
   toWrite[0]=mark;
   strcat(toWrite,"\t");
   strcat(toWrite,intVersionNo);
   strcat(toWrite,"\t");
   strcat(toWrite,path);
   strcat(toWrite,"\t");
   strcat(toWrite,hashcode);
   strcat(toWrite,"\n");
   int wr = write(*fdp,toWrite,strlen(toWrite));
   if(wr<0){
     perror("Write failed!");exit(1);
   }

}
//finds the .Commit file in a given project and deletes it.
//Project in which .Commit is to be located and deleted.
int deleteCommit(char *prog){
 char commitProg[strlen(prog)+strlen("/.Commit")];
  bzero(commitProg,strlen(commitProg));
  strcat(commitProg,prog);
  strcat(commitProg,"/.Commit");
  int fd = open(commitProg,O_RDONLY);
  if(fd<0){
   return -1;
  }else{
  return remove(commitProg);

  }
}
void compare(int *fdp,int *socketfd,char *v,char *p, char *h,char* pro,char Mark){
  node *curr = serverHead;
  while(curr!=NULL){
     if(strcmp(curr->pathName,p)==0){
       break;
     }
      curr=curr->next;
  }
  if(curr==NULL){
      if(Mark=='R'){
        return;
      }
      int wr = write(*socketfd,"4:fail",strlen("4:fail"));
      if(wr<0){
        perror("Write to socket failed");exit(1);
      }
     int r = deleteCommit(pro);
      exit(1);
   }
   if(Mark=='R'){
     printf("D %s\n",curr->pathName);
     addToCommit('D',v,curr->pathName,curr->hashcode,pro,fdp);

   }
 else if(strcmp(curr->version,v)<=0){
   if(strcmp(curr->hashcode,h)==0){
    char *liveHash = hashConvert(p);
    if(strcmp(liveHash,h)!=0){
      printf("M %s\n",curr->pathName);
      addToCommit('M',v,curr->pathName,curr->hashcode,pro,fdp);
    }
   }else{
      printf("Commit failed! the client must synch with the repository before committing changes\n");
      int wr = write(*socketfd,"4:fail",strlen("4:fail"));
      if(wr<0){
        perror("Write to socket failed");exit(1);
      }
     int r = deleteCommit(pro);
      exit(1);
   }
  }else {
      printf("Commit failed! the client must synch with the repository before committing changes\n");
      int wr = write(*socketfd,"4:fail",strlen("4:fail"));
      if(wr<0){
        perror("Write to socket failed");exit(1);
      }
      int r = deleteCommit(pro);
      printf("Disconnected from the server\n");
      exit(1);
  }
  /*else if(strcmp(curr->version,v)>0){
     if(strcmp(curr->hashcode,h)!=0){
      printf("Commit failed! the client must synch with the repository before committing changes\n");
      int wr = write(*socketfd,"4:fail",strlen("4:fail"));
      if(wr<0){
        perror("Write to socket failed");exit(1);
      }
     int r = deleteCommit(pro);
      exit(1);
    }if(strcmp(curr->hashcode,h)==0){
       char *liveHash = hashConvert(p);
       if(strcmp(liveHash,h)!=0){
        printf("M %s\n",curr->pathName);
        addToCommit('M',v,curr->pathName,curr->hashcode,pro,fdp);
       }
     }

  }*/
}
void tokenizeClient(int *fdp,int *socketfd,char *manifest,char *project){
 int TabCounter=0;int i=0;int k =0;int l=0;int TabCounterLim=2;
 char *version;char *path; char *hashcode;
 char tmpStr[strlen(manifest)];
 memset(tmpStr,'\0',sizeof(tmpStr));
 while(manifest[l]!='\n'){
  l++;k++;
 }
 l++;k=0;
 for(i=l;i<strlen(manifest);i++){
   while(manifest[i]!='\t'){
     char a = manifest[i];
     strncat(tmpStr, &a, 1);
    k++;i++;
   }
   if((strcmp(tmpStr,"A")==0)||(strcmp(tmpStr,"R")==0)){
      // memset(tmpStr,'\0',strlen(tmpStr));
       int isA=0; int isR=0;
       if(strcmp(tmpStr,"A")==0){
        isA= 1; isR=0;
       }else{
        isA=0; isR=1;
       }
       i++;
       memset(tmpStr,'\0',strlen(tmpStr)); k =0;
       while(manifest[i]!='\t'){
         char a = manifest[i];
         strncat(tmpStr, &a, 1);
        k++;i++;
       }
       version = (char*)malloc((k+3)*sizeof(char));
       bzero(version,strlen(version));
       memcpy(version,tmpStr,strlen(tmpStr));
       memset(tmpStr,'\0',sizeof(tmpStr));
       k=0;
       i++;
       memset(tmpStr,'\0',strlen(tmpStr));
       while(manifest[i]!='\t'){
       //tmpStr[k]= manifest[i];
       char a = manifest[i];
       strncat(tmpStr, &a, 1);
       k++;i++;
       }
       path = (char*)malloc((k+3)*sizeof(char));
       bzero(path,strlen(path));
       memcpy(path,tmpStr,strlen(tmpStr));
       memset(tmpStr,'\0',sizeof(tmpStr));
       k=0;
       i++;
       while(manifest[i]!='\n'){
         char a = manifest[i];
         strncat(tmpStr, &a, 1);
         k++;i++;
       }
       hashcode= (char*)malloc((k+3)*sizeof(char));
       memset(hashcode,'\0',(strlen(hashcode)+1));
       strcat(hashcode,tmpStr);
       memset(tmpStr,'\0',sizeof(tmpStr));
       if(isA){
        printf("A %s\n",path);
        addToCommit('A',version,path,hashcode,project,fdp);
       }else if(isR){

          compare(fdp,socketfd,version,path,hashcode,project,'R');
       }
       k =0;TabCounter=0;
       continue;
      }
   if((manifest[i]=='\t') && (TabCounter<TabCounterLim)){
     TabCounter++;
     if(TabCounter<TabCounterLim){
       version = (char*)malloc((k+3)*sizeof(char));
       bzero(version,strlen(version));
       memcpy(version,tmpStr,strlen(tmpStr));
       memset(tmpStr,'\0',sizeof(tmpStr));
       k=0;
     }
   }
   if(TabCounter==TabCounterLim){
      path = (char*)malloc((k+3)*sizeof(char));
      bzero(path,strlen(path));
      memcpy(path,tmpStr,strlen(tmpStr));
      memset(tmpStr,'\0',sizeof(tmpStr));
      k=0;
     if(manifest[i]=='\t'){
       i++;
     }
     while(manifest[i]!='\n'){
      tmpStr[k]=manifest[i];k++;i++;
     }
     hashcode= (char*)malloc((k+3)*sizeof(char));
     bzero(hashcode,strlen(hashcode));
     memcpy(hashcode,tmpStr,strlen(tmpStr));
     memset(tmpStr,'\0',sizeof(tmpStr));
     compare(fdp,socketfd,version,path,hashcode,project,'M');
     k =0;TabCounter=0;
   }
 }
}
/*void printLL(){
  node *tmp = updateHead;
  while(tmp!=NULL){
    printf("%s\t",tmp->tag);
    printf("%s\t",tmp->pathName);
    printf("%s\t\n",tmp->hashcode);
    tmp= tmp->next;
  }
}
void printCLL(){
  printf("---------------\n");
  node *tmp = clientHead;
  while(tmp!=NULL){
    if(tmp->tag!=NULL)
    printf("%s\t",tmp->tag);
    printf("%s\t",tmp->version);
    printf("%s\t",tmp->pathName);
    printf("%s\t\n",tmp->hashcode);
    tmp= tmp->next;
  }
}*/
//Adds the node containing the data of server's manifest into server's
//Linked List at the end of the list.
void addToList(char *v,char *p, char *h){
  if(serverHead==NULL){
    serverHead = (node*)malloc(sizeof(node));
    serverHead->version = (char*)malloc((strlen(v)+1)*sizeof(char));
    bzero(serverHead->version,strlen(serverHead->version));
    memcpy((serverHead->version),v,strlen(v));

    serverHead->pathName = (char*)malloc((strlen(p)+1)*sizeof(char));
    bzero(serverHead->pathName,strlen(serverHead->pathName));
    memcpy((serverHead->pathName),p,strlen(p));

    serverHead->hashcode = (char*)malloc((strlen(h)+1)*sizeof(char));
    bzero(serverHead->hashcode,strlen(serverHead->hashcode));
    memcpy((serverHead->hashcode),h,strlen(h));
    serverHead->next=NULL;
    return;
  }
  node *current = serverHead;
  node *prev = NULL;
  while(current!=NULL){
    prev = current;
    current=current->next;
  }
  node *newEntry = (node*)malloc(sizeof(node));
  newEntry->version = (char*)malloc((strlen(v)+1)*sizeof(char));
    bzero(newEntry->version,strlen(newEntry->version));
    memcpy((newEntry->version),v,strlen(v));

    newEntry->pathName = (char*)malloc((strlen(p)+1)*sizeof(char));
    bzero(newEntry->pathName,strlen(newEntry->pathName));
    memcpy((newEntry->pathName),p,strlen(p));

    newEntry->hashcode = (char*)malloc((strlen(h)+1)*sizeof(char));
    bzero(newEntry->hashcode,strlen(newEntry->hashcode));
    memcpy((newEntry->hashcode),h,strlen(h));
    newEntry->next=current;
    prev->next=newEntry;
}
//Tokenizes server's manifest to extract file version,path and associated hashcode
void serverManifestLL(char *manifest){
 int TabCounter=0;int i=0;int k =0;int l=0;int TabCounterLim=2;
 char *version;char *path; char *hashcode;
 char tmpStr[strlen(manifest)];
 memset(tmpStr,'\0',sizeof(tmpStr));
 // strcat(toPrint,"0\n");
 while(manifest[l]!='\n'){
  l++;k++;
 }
 l++;k=0;
 if(manifest[strlen(manifest)-1]!='\n'){
   manifest[strlen(manifest)-1]='\n';
 }
 for(i=l;i<strlen(manifest);i++){
   while((manifest[i]!='\t')&&(i<strlen(manifest))){
    tmpStr[k]= manifest[i];
    k++;i++;
   }

   if((manifest[i]=='\t') && (TabCounter<TabCounterLim)){
     TabCounter++;
     if(TabCounter<TabCounterLim){
       version = (char*)malloc((k+1)*sizeof(char));
       bzero(version,strlen(version));
       memcpy(version,tmpStr,strlen(tmpStr));
       memset(tmpStr,'\0',sizeof(tmpStr));
       k=0;
     }
   }
   if(TabCounter==TabCounterLim){
      path = (char*)malloc((k+1)*sizeof(char));
      bzero(path,strlen(path));
      memcpy(path,tmpStr,strlen(tmpStr));
      memset(tmpStr,'\0',sizeof(tmpStr));
      k=0;
     if(manifest[i]=='\t'){
       i++;
     }
     while(manifest[i]!='\n'){
      tmpStr[k]=manifest[i];k++;i++;
     }
     hashcode= (char*)malloc((k+1)*sizeof(char));
     bzero(hashcode,strlen(hashcode));
     strcat(hashcode,tmpStr);
    // memcpy(hashcode,tmpStr,strlen(tmpStr));
     memset(tmpStr,'\0',sizeof(tmpStr));
     addToList(version,path,hashcode);
     k =0;TabCounter=0;
   }
 }
}
//gets the file based on the the path not file descriptor
//it forms the path opens the file loads the content and returns it
//mainly used to get the content of manifest.
char * loadClientManifest(char *dir,char* fileToRead){
  DIR *directory;
  directory = opendir(dir);
  if(directory==NULL){return "failed";}
  char manifestPath[strlen(fileToRead)+strlen(dir)+strlen("/")];
  memset(manifestPath,'\0',sizeof(manifestPath));
  strcat(manifestPath,dir);
  strcat(manifestPath,"/");
  strcat(manifestPath,fileToRead);
  int fd = open(manifestPath,O_RDONLY);
  if(fd<0){
    perror(manifestPath);
    return "failed";
  }
  int length = 5000;
  char *BufferToSend = (char*) malloc(length*sizeof(char));
  int ReadIn = 0; int status = 0;
  do{
    status = read(fd,BufferToSend+ReadIn,length-ReadIn);
    if(status<0){
      perror(manifestPath);exit(1);
    }
    ReadIn+=status;
    if(ReadIn>=length){
      length= length*2;
      char *tmp = (char*)malloc(strlen(BufferToSend)*sizeof(char));
       bzero(tmp,strlen(tmp));
       strcpy(tmp,BufferToSend);
       BufferToSend = (char*)realloc(tmp,length*(sizeof(char)));
    }
  }while(status>0 && ReadIn<length);
 return BufferToSend;
}
//reads from the file if path has already been formed and passed here.
//used while traversing directories and subdirectories where paths are made
//during recursion.
char* readFile(char *filepath){
  //printf("%s\n",filepath);
  int size = 1000;
  char *buffer = (char*)malloc(size *sizeof(char));
  memset(buffer,'\0',strlen(buffer));
  int fd = open(filepath, O_RDONLY);
  if(fd<0){perror("Error opening file");exit(1);}
  int readIn =0;int n =0;
   do{
     n = read(fd,buffer+readIn,size);
     if(n<0){
      perror("read failure");exit(1);
     }
     readIn+=n;
      if(readIn>=size){
      size*=2;
      buffer = (char*) realloc(buffer,size*(sizeof(char*)));
      memset(buffer+readIn,'\0',size);
    }
    }while(n>0 && readIn<size);
  return buffer;
}
//Makes a .Mani
void makeFile(char* fname,char *name){
  int fd = open(fname,O_RDWR | O_CREAT,00600);
  if(fd==0){perror("Cannot create/open file");exit(1);}
  char manifestContent[strlen(fname)+ sizeof("0")+strlen(name)+sizeof('\t')];
  bzero(manifestContent,strlen(manifestContent));
  strcat(manifestContent,"0");
  strcat(manifestContent,"\n");
  //strcat(manifestContent,name);
  write(fd,manifestContent,strlen(manifestContent));

}
//Creates a directory and makes .Manifest in it and initializes the file to
// 0/n (version and nextline)
void crtDir(char *name, char c){
 DIR *dir;
 dir = opendir(name);
 if(dir != NULL){printf("directory exists");exit(1);}
 else{
  int k = mkdir(name,0777);//printf("%d\n",k);
  if(k!=0){perror("make directory error");exit(1);}
  //printf("Here\n");
  char manifestPath [strlen(name)+ sizeof(".Manifest") +sizeof("/")+sizeof("/0")];
  memset(manifestPath,'\0',strlen(manifestPath));
  strcat(manifestPath,name);
  strcat(manifestPath,"/.Manifest");
  //printf("%s",manifestPath);
  makeFile(manifestPath,name);

 }

}
//Adds the content to the .Manifest during add function.
//A is appended in front. So, A <version> <path> <hashcode>
void addManifest(char * progName, char *filePath,char *file,char *hashcode){
char buffer[100000];
memset(buffer,'\0',strlen(buffer));
char  manifestLoc [strlen(".Manifest")+strlen(progName)+sizeof("/")];
  bzero(manifestLoc,strlen(manifestLoc));
  strcat(manifestLoc,progName);
  strcat(manifestLoc,"/");
  strcat(manifestLoc, ".Manifest");
  int fd = open(manifestLoc, O_RDWR);
  if(fd<0){perror("Error opening file");exit(1);}
  int readIn =0;int n =0;
   do{
     n = read(fd,buffer+readIn,100000);
     if(n<0){
      perror("read failure");exit(1);
     }
     readIn+=n;

    }while(n>0 && readIn<100000);
 // printf("%s\n",buffer);
  if(strstr(buffer,filePath)!=NULL){
     char * result = strstr(buffer,filePath);
     int i = result - buffer;
     while(buffer[i]!='\n'){
       if(buffer[i]!='A'||buffer[i]!='R'){
         i--;
       }

       if(buffer[i]=='R'){
         buffer[i]= 'A';
         remove(manifestLoc);
         int mfd = open(manifestLoc,O_CREAT|O_RDWR,00600);
         if(mfd<0){
           perror("Error");exit(1);
         }
         int r = write(mfd,buffer,strlen(buffer));
         printf("Added successfully!!");
         return;
       }else if((buffer[i])=='A'){
         printf("File already exists\n");exit(1);
       }
     }
     printf("File already exists\n");exit(1);
  }
  char toWrite[sizeof("A\t")+sizeof("0")+sizeof("\t")+strlen(filePath)+sizeof("\t")+strlen(hashcode)+ (2*sizeof("\n"))+sizeof('\0')];
  bzero(toWrite,strlen(toWrite));
  strcat(toWrite,"A\t");
  strcat(toWrite,"0");
  strcat(toWrite,"\t");
  strcat(toWrite,filePath);
  strcat(toWrite,"\t");
  strcat(toWrite,hashcode);
  strcat(toWrite,"\n");
  int w = write(fd,toWrite,strlen(toWrite));
  if(w<0){ perror("write to .Manifest failed");exit(1);}
  printf("%s Entry added successfully to the Manifest\n",file);
}
//Locates the file by traversing the directories.If found does it then
//content is added to .Manifest with A appended in front.
int locateTheFile(char *directory, char *file,char *d){
  int doesFileExists = 0;
  DIR *dir;
  struct dirent *sd;
  struct stat buff;
  dir = opendir(directory);
  if(dir == NULL){
    perror(directory);
    exit(1);
  }else{
    while((sd=readdir(dir))!=NULL){
       if(strcmp(sd->d_name,".")==0 || strcmp(sd->d_name,"..")==0)
        continue;

           //int length = sizeof(pathName)+(sizeof(sd->d_name) +sizeof("/")+sizeof("\0"));
           char localname[20000];
           strcpy(localname,directory);
           strcat(localname,"/");
           strcat(localname,sd->d_name);
           if(stat(localname,&buff)==0){
             if(S_ISDIR(buff.st_mode)>0){
              doesFileExists =locateTheFile(localname,file,d);
             }
            if(S_ISREG(buff.st_mode)>0){

                char *fname = strstr(localname,file);
               if(fname!=NULL){
                 if(strcmp(fname,file)==0){
                  int fd = open(localname,O_RDONLY);
                  if(fd<0){
                   if(errno==2){
                    printf("Error: File does not exit\n");exit(1);
                   }
                    perror("Error");exit(1);
                  }
                  close(fd);
                  char *hash = (char*)malloc(40 *sizeof(char));
                  memset(hash,'\0',strlen(hash));
                  hash = hashConvert(localname);
                  //printf("Hash: %s\n",hash);
                  addManifest(d,localname,file,hash);
                  doesFileExists = 1;
                 }
               }

            }
         }

     }

    }
    return doesFileExists;
 //close(dir);

}
//Removes indicated file from the .Manifest
//appends R in front of it. So, R <version> <path> <hashcode>
int removeLine(int fd, char* buffer,char* file){
   int size=1000;
   int isRemoved =0;
   char *toPrint = (char*)malloc(size*sizeof(char));
   memset(toPrint,'\0',size);
   int k=0; int i=0;
  for(i=0;i<strlen(buffer);i++){
   while(buffer[i]!='\n'){
     toPrint[k]=buffer[i];
     k++;i++;
     if(k>=size){
       size = 2*size;
       char *tmp = (char*)malloc(strlen(toPrint)*sizeof(char));
       bzero(tmp,strlen(tmp));
       strcpy(tmp,toPrint);
       toPrint = (char*)realloc(tmp,size*(sizeof(char)));
     }
  }k=0;
   strcat(toPrint,"\n");
   char *s = strstr(toPrint,file);
  if(strstr(toPrint,file)!=NULL){
     char *chopStr = strchr(s,'\t');
     int idx = (int)(chopStr -s);
     char fileN[strlen(s)];
     memset(fileN,'\0',sizeof(fileN));
     memcpy(fileN,&s[0],idx);
     if(strcmp(fileN,file)==0){
       if(toPrint[0]=='R'){
         isRemoved=0;
       int wr = write(fd,toPrint,strlen(toPrint));
       if(wr<0){perror("error writing");exit(1);}
       memset(toPrint,'\0',size);
       }
       else{
         isRemoved=1;
        char appendR[strlen(toPrint)+strlen("R\t")];
        memset(appendR,'\0',strlen(appendR));
        if(toPrint[0]!='A')
        strcat(appendR,"R\t");
        if(toPrint[0]=='A')
        toPrint[0]='R';
        strcat(appendR,toPrint);
       int wr = write(fd,appendR,strlen(appendR));
       if(wr<0){perror("error writing");exit(1);}

       memset(toPrint,'\0',size);
      }
     }
     else{
      isRemoved = 0;
       int wr = write(fd,toPrint,strlen(toPrint));
      if(wr<0){perror("error writing");exit(1);}
       memset(toPrint,'\0',size);
      }
  }
  if(strstr(toPrint,file)==NULL){
    int wr = write(fd,toPrint,strlen(toPrint));
    if(wr<0){perror("error writing");exit(1);}
    memset(toPrint,'\0',size);

  }
 }
 return isRemoved;
}
//opens a given directory and searches for file if found calls
//further functions based on flag.
//Major purpose is to add or remove (that's the flag)
void opnDir(char *progName,char *file,char flag){
  DIR *dir;
  //printf("%s\n", progName);
  dir = opendir(progName);
  if(dir == NULL){
   printf("Error: Project does not exist\n");
   exit(1);
  }
  if(flag=='a'){
    int x = locateTheFile(progName,file,progName);
    if(!x){ printf("File: %s does not exist in the project\n",file);}return;
  }else if(flag=='r'){
   char  manifest [strlen(".Manifest")+strlen(progName)+sizeof("/")];
   bzero(manifest,strlen(manifest));
   strcat(manifest,progName);
   strcat(manifest,"/");
   strcat(manifest, ".Manifest");
   char *buffer = readFile(manifest);
   int k = remove(manifest);
   if(k!=0){perror("error removing");exit(1);}
   int fd = open(manifest,O_RDWR | O_CREAT, 00600);
   if(fd==0){perror("Cannot create/open file");exit(1);}
   if(!removeLine(fd,buffer,file)){
     printf("No such file to be removed\n");exit(1);
   }
    printf("Remove successful\n");
  }
}
int configure(char *host, char *port){
   int fd = open(".configure",O_CREAT|O_RDWR, 0600);
   if(fd<0){
    perror("Error reading file:");
    exit(1);
   }
   char toWriteInConfig[strlen(host)+strlen(port)+sizeof("\t")];
   memset(toWriteInConfig,'\0',strlen(toWriteInConfig));
   strcat(toWriteInConfig,host);
   strcat(toWriteInConfig,"\t");
   strcat(toWriteInConfig,port);
   int n = write(fd,toWriteInConfig,strlen(toWriteInConfig));
   if(n<0){
    perror("Configure writing error: ");exit(1);
   }
  return 0;
}

void setHostAndPort(int * port, char *host){
  memset(host,'\0',strlen(host));
  int fd = open(".configure",O_RDONLY);
  int toRead = 500;
  int n;
  char *tmpBuffer = (char*)malloc(toRead *sizeof(char));
  if(fd<0){
    perror("Cannot get host and port:");
    exit(1);
  }
  int readIn =0;
  do{
    n = read(fd,tmpBuffer+readIn,toRead-readIn);
    //printf("n: %d\n", n);
    if(n<0){
      if(n<0){ perror("Cannot get host and port:"); exit(1);}
    }
    readIn+=n;
    if(n>=toRead){
      toRead*=2;
      tmpBuffer = (char*) realloc(tmpBuffer,toRead*(sizeof(char*)));
      memset(tmpBuffer+readIn,'\0',toRead);
    }
 }while(n >0 && readIn< toRead);

  int i = 0;
  //int tranferIndex =0;
  while(tmpBuffer[i]!='\t'){
    char a = tmpBuffer[i];
    strncat(host, &a, 1);
    //host[i] = tmpBuffer[i] ;
   i++;
 }
  i++;
 char * s = (tmpBuffer + i);
  *port = atoi(s);

}
void print(char *s){
 int TabCounter=0;int i=0;int k =0;int l=0;int TabCounterLim=2;
 char toPrint[strlen(s)];
 memset(toPrint,'\0',sizeof(toPrint));
// strcat(toPrint,"0\n");
 while(s[l]!='\n'){
  toPrint[k]=s[l];
  l++;k++;
 }
 toPrint[k]=s[l];l++;k=0;
 printf("%s",toPrint);
 memset(toPrint,'\0',sizeof(toPrint));
 for(i=l;i<strlen(s);i++){
   while(s[i]!='\t'){
    toPrint[k]= s[i];
    k++;i++;
   }
   if((strcmp(toPrint,"A")==0)|| (strcmp(toPrint,"R")==0))
     TabCounterLim =3;
    else
      TabCounterLim =2;
   if((s[i]=='\t') && (TabCounter<TabCounterLim)){
     TabCounter++;
     if(TabCounter<TabCounterLim)
     toPrint[k]=s[i];k++;
   }
   if(TabCounter==TabCounterLim){
     while(s[i]!='\n'){
      i++;
     }
     toPrint[k]=s[i];
     printf("%s\n",toPrint);
     memset(toPrint,'\0',sizeof(toPrint));
     k =0;TabCounter=0;
   }
 }

}
// 1 for success and 0 for failure
//checks if .Commit and .Update files are there or not.
// If there, then empty or not empty
int isPathValid(char *directory,char *file){
 char updatePath[strlen("/")+strlen(file)+strlen(directory)];
 memset(updatePath,'\0',strlen(updatePath));
 strcat(updatePath,directory);
 strcat(updatePath,"/");
 strcat(updatePath,file);
  struct stat doesFileExists;
     if(stat(updatePath, &doesFileExists)){
       return 1;
     }
     else if(doesFileExists.st_size <= 1){
         if(strcmp(file,".Conflict")==0){
           return 0;
         }else{
           return 1;
         }
     }
     else {
         return 0;
     }

}
int main(int argc, char* argv[]){
 //printf("%d\n",argc);
 int socketfd, portno, n,fd,protocolSize;
 char *hostName = (char*) malloc(300 * sizeof(char));
 memset(hostName,'\0',strlen(hostName));int size = 1024;
 char *result = (char*) malloc(size *sizeof(char));
 char *protocol;
 char *buff = (char*) malloc(size *sizeof(char));
   if(argc == 4){
      if(strcmp(argv[1],"configure")==0){
         int intHost = atoi(argv[3]);
         if(intHost<=0){
           printf("Not a valid port\n");exit(1);
         }
         int r = configure(argv[2],argv[3]);
         exit(0);
      }else if(strcmp(argv[1],"add")==0){
       opnDir(argv[2],argv[3],'a');
       return 0;
      }else if(strcmp(argv[1],"remove")==0){
       opnDir(argv[2],argv[3],'r');
       return 0;
      }

      printf("Not valid arguements!\n");exit(1);

   }
   else if(argc ==3){
     if(strcmp(argv[1],"create")==0){
       DIR *directory;
       directory = opendir(argv[2]);
       if(directory!=NULL){
        printf("Such directory already exists\n");
        protocol = (char*) malloc((strlen("bad")+1) *sizeof(char));
        memset(protocol,'\0',strlen(protocol));
        strcat(protocol,"bad");
       }
       else{
        protocolSize = ((2*strlen(argv[2]))+sizeof("crt")+sizeof(":")+sizeof('\0'));
        protocol = (char*) malloc(protocolSize *sizeof(char));
        bzero(protocol,protocolSize);
        char fileLength[strlen(argv[2])];
        memset(fileLength,'\0',sizeof(fileLength));
        strcat(protocol,"crt");
        sprintf(fileLength,"%d",strlen(argv[2]));
        strcat(protocol,fileLength);
        strcat(protocol,":");
        strcat(protocol,argv[2]);
       }
     }else if(strcmp(argv[1],"currentversion")==0){
       protocolSize = ((2*strlen(argv[2]))+sizeof("crt")+sizeof(":")+sizeof('\0'));
      protocol = (char*) malloc(protocolSize *sizeof(char));
      bzero(protocol,protocolSize);
      char fileLength[strlen(argv[2])];
      memset(fileLength,'\0',sizeof(fileLength));
      strcat(protocol,"crv");
      sprintf(fileLength,"%d",strlen(argv[2]));
      strcat(protocol,fileLength);
      strcat(protocol,":");
      strcat(protocol,argv[2]);
    }else if(strcmp(argv[1],"destroy")==0){
      protocolSize = ((2*strlen(argv[2]))+sizeof("des")+sizeof(":")+sizeof('\0'));
      protocol = (char*) malloc(protocolSize *sizeof(char));
      bzero(protocol,protocolSize);
      char fileLength[strlen(argv[2])];
      memset(fileLength,'\0',sizeof(fileLength));
      strcat(protocol,"des");
      sprintf(fileLength,"%d",strlen(argv[2]));
      strcat(protocol,fileLength);
      strcat(protocol,":");
      strcat(protocol,argv[2]);
    }else if(strcmp(argv[1],"commit")==0){
      int checkUpdateFile = isPathValid(argv[2],".Update");
      int checkConflictFile = isPathValid(argv[2],".Conflict");
     /* printf("UpdateFile: %d\n",checkUpdateFile);
      printf("Conflict: %d\n",checkUpdateFile);*/
      if((checkUpdateFile==0)||(checkConflictFile==0)){
        printf("Non empty Update present or .conflict present\n");
        exit(1);
      }
      protocolSize = ((2*strlen(argv[2]))+sizeof("com")+sizeof(":")+sizeof('\0'));
      protocol = (char*) malloc(protocolSize *sizeof(char));
      bzero(protocol,protocolSize);
      char fileLength[strlen(argv[2])];
      memset(fileLength,'\0',sizeof(fileLength));
      strcat(protocol,"com");
      sprintf(fileLength,"%d",strlen(argv[2]));
      strcat(protocol,fileLength);
      strcat(protocol,":");
      strcat(protocol,argv[2]);
    }else if(strcmp(argv[1],"push")==0){
      protocolSize = ((2*strlen(argv[2]))+sizeof("psh")+sizeof(":")+sizeof('\0'));
      protocol = (char*) malloc(protocolSize *sizeof(char));
      bzero(protocol,protocolSize);
      char fileLength[strlen(argv[2])];
      memset(fileLength,'\0',sizeof(fileLength));
      strcat(protocol,"psh");
      sprintf(fileLength,"%d",strlen(argv[2]));
      strcat(protocol,fileLength);
      strcat(protocol,":");
      strcat(protocol,argv[2]);
      //printf("Protocol: %s\n",protocol);
    }else if(strcmp(argv[1],"update")==0){
      protocolSize = ((2*strlen(argv[2]))+sizeof("upd")+sizeof(":")+sizeof('\0'));
      protocol = (char*) malloc(protocolSize *sizeof(char));
      bzero(protocol,protocolSize);
      char fileLength[strlen(argv[2])];
      memset(fileLength,'\0',sizeof(fileLength));
      strcat(protocol,"upd");
      sprintf(fileLength,"%d",strlen(argv[2]));
      strcat(protocol,fileLength);
      strcat(protocol,":");
      strcat(protocol,argv[2]);
      //printf("%s\n: ",protocol);
    }else if(strcmp(argv[1],"upgrade")==0){
       DIR *dir;
       //printf("%s\n", progName);
      dir = opendir(argv[2]);
      if(dir == NULL){
        printf("Error: Project does not exist\n");
        printf("Disconnected from the server\n");
        exit(1);
      }
       char conflictPath[strlen(argv[2])+strlen("/.Conflict")];
       memset(conflictPath,'\0',sizeof(conflictPath));
       strcat(conflictPath,argv[2]);
       strcat(conflictPath,"/.Conflict");
      int fd = open(conflictPath,O_RDONLY);
      if(fd!=-1){
       printf("Resolve conflicts first!\n");
       printf("Disconnected from the server\n");
       exit(1);
      }
      char updatePath[strlen(argv[2])+strlen("/.Update")];
       memset(updatePath,'\0',sizeof(updatePath));
       strcat(updatePath,argv[2]);
       strcat(updatePath,"/.Update");
       fd= open(updatePath,O_RDONLY);
       if(fd == -1){
         printf("Please update first!");exit(1);
         printf("Disconnected from the server\n");
       }
       char *UpdateContent = readFile(updatePath);
       if((UpdateContent==NULL)||(UpdateContent[0]=='\n')){
         printf("Project is upto date\n");
         printf("Disconnected from the server\n");
         return 0;
       }
       tokenizeUpdate(UpdateContent);
    }else if(strcmp(argv[1],"history")==0){
      protocolSize = ((2*strlen(argv[2]))+sizeof("hst")+sizeof(":")+sizeof('\0'));
      protocol = (char*) malloc(protocolSize *sizeof(char));
      bzero(protocol,protocolSize);
      char fileLength[strlen(argv[2])];
      memset(fileLength,'\0',sizeof(fileLength));
      strcat(protocol,"hst");
      sprintf(fileLength,"%d",strlen(argv[2]));
      strcat(protocol,fileLength);
      strcat(protocol,":");
      strcat(protocol,argv[2]);
    }else if(strcmp(argv[1],"checkout")==0){
      DIR *dir;
      dir = opendir(argv[2]);
      if(dir!=NULL){
        printf("such directory already exists on client\n");
        return -1;
      }
      protocolSize = ((2*strlen(argv[2]))+sizeof("cot")+sizeof(":")+sizeof('\0'));
      protocol = (char*) malloc(protocolSize *sizeof(char));
      bzero(protocol,protocolSize);
      char fileLength[strlen(argv[2])];
      memset(fileLength,'\0',sizeof(fileLength));
      strcat(protocol,"cot");
      sprintf(fileLength,"%d",strlen(argv[2]));
      strcat(protocol,fileLength);
      strcat(protocol,":");
      strcat(protocol,argv[2]);
    }else{
    printf("incorrect arguements\n");return -1;
    }
   }
    setHostAndPort(&portno,hostName);
   //printf("Hostname: %s\n",hostName); printf("Port: %d\n",portno); exit(0);
   struct sockaddr_in server_addr;
   struct hostent *server;
    //portno = atoi(argv[4]);
    socketfd = socket(AF_INET,SOCK_STREAM,0);
    if(socketfd<0){
     perror("Cannot open socket");
     exit(1);
    }
    printf("Socket to server opened!\n");
    //printf("%s\n",hostName);
    server = gethostbyname(hostName);
    if(server == NULL){
     perror("cannot get that host");
     exit(1);
    }
    bzero(&server_addr,sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(portno);
    bcopy((char*)server->h_addr,(char*)&server_addr.sin_addr.s_addr,server->h_length);
    if((connect(socketfd,(struct sockaddr *)&server_addr,sizeof(server_addr))<0)){
     perror("Connection failed");
     exit(1);
   }
  printf("Connection to server successful!\n");
  if(strcmp(argv[1],"history")==0){
    n = write(socketfd,protocol,strlen(protocol));
    if(n<0){perror("write to socket failed");exit(1);}
    bzero(buff,size);
    char *historyFile = getUpdatedManifest(&socketfd);
    if(strcmp(historyFile,"fail")==0){
      printf("project not present on the server\n");
      printf("Disconnected from the server\n");
      return 0;
    }
    printf("%s\n",historyFile);
  }
  if(strcmp(argv[1],"create")==0){
    n = write(socketfd,protocol,strlen(protocol));
    if(n<0){perror("write to socket failed");exit(1);}
    bzero(buff,size);
    int readIn =0;
    do{
     n = read(socketfd,buff+readIn,7);
     if(n<0){
      perror("read failure");exit(1);
     }
     readIn+=n;

    }while(n>0 && readIn<7);
    if(strcmp(buff,"success")==0){
      DIR *dir;
      dir = opendir(argv[2]);
      if(dir==NULL){
       closedir(dir);
       crtDir(argv[2],'c');
       printf("Project created!\n");
       printf("Disconnected from server\n");
      }
    }else if(strcmp(buff,"failure")==0){
      printf("Failed: server had project with same name\n");
      printf("Disconnected from the server\n");
      //return 0;
    }
  }else if(strcmp(argv[1],"currentversion")==0){
    //printf("Protocol: %s\n",protocol);
    n = write(socketfd,protocol,strlen(protocol));
    if(n<0){perror("write to socket failed");exit(1);}
    bzero(buff,size);
     int readIn = 0;
     n = 0;int s =1;
    do{
     n = read(socketfd,buff+readIn,1);
     if(n<0){
      perror("read failure");exit(1);
     }
     readIn+=n;

    }while(strstr(buff,":")==NULL);
    readIn =0;n =0;
    char *e;
    int delimIdx;
    e = strchr(buff,':');
    delimIdx = (int)(e-buff);//printf("%d\n",delimIdx);
    char *bytesToRead = (char*)malloc((delimIdx +1)*sizeof(char));
    memset(bytesToRead,'\0',strlen(bytesToRead));
    memcpy(bytesToRead,buff,(delimIdx));
    int nextRead = atoi(bytesToRead);
    bzero(buff,strlen(buff));
     do{
     n = read(socketfd,buff+readIn,nextRead);
     if(n<0){
      perror("read failure");exit(1);
     }
     readIn+=n;

    }while(n>0 && readIn<nextRead);
   if(strcmp(buff,"failed")==0){
    printf("No such directory exits on server\n");
   }else
   print(buff);

 }else if(strcmp(argv[1],"destroy")==0){
   bzero(buff,strlen(buff));
   n = write(socketfd,protocol,strlen(protocol));
   if(n<0){perror("Write to socket failed");exit(1);}
    int readIn =0;
    do{
     n = read(socketfd,buff+readIn,7);
     if(n<0){
      perror("read failure");exit(1);
     }
     readIn+=n;

    }while(n>0 && readIn<7);
  if(strcmp(buff,"success")==0){
    printf("successfully removed\n");
  } else {

    printf("failed to remove\n");
  }

 }else if(strcmp(argv[1],"commit")==0){
   bzero(buff,strlen(buff));
   n = write(socketfd,protocol,strlen(protocol));
   if(n<0){perror("Write to socket failed");exit(1);}
   int readIn = 0;
     n = 0;int s =1;
    do{
     n = read(socketfd,buff+readIn,1);
     if(n<0){
      perror("read failure");exit(1);
     }
     readIn+=n;

    }while(strstr(buff,":")==NULL);
    readIn =0;n =0;
   // printf("Buff: %s\n",buff);
    char *e;
    int delimIdx;
    e = strchr(buff,':');
    delimIdx = (int)(e-buff);//printf("%d\n",delimIdx);
    char *bytesToRead = (char*)malloc((delimIdx +1)*sizeof(char));
    memset(bytesToRead,'\0',strlen(bytesToRead));
    memcpy(bytesToRead,buff,(delimIdx));
    int nextRead = atoi(bytesToRead);
    bzero(buff,strlen(buff));
    do{
     n = read(socketfd,buff+readIn,nextRead);
     if(n<0){
       perror("Read from socket failed");
     }
     readIn+=n;
     if(readIn>=size){
       size = (nextRead+20);
       char *tmp = (char*)malloc(strlen(buff)*sizeof(char));
       bzero(tmp,strlen(tmp));
       strcpy(tmp,buff);
       buff = (char*)realloc(tmp,size*(sizeof(char)));
     }
    }while(n>0 && readIn<nextRead);
    if(strlen(buff)<size){
      buff[strlen(buff)]='\n';
    }else if(strlen(buff)>=size){
      int k = strlen(buff);
       size = (size+3);
       char *tmp = (char*)malloc(strlen(buff)*sizeof(char));
       bzero(tmp,strlen(tmp));
       strcpy(tmp,buff);
       buff = (char*)realloc(tmp,size*(sizeof(char)));
       buff[k]='\n';
    }
   // printf("Buffer: %s\n",buff);
    if(strcmp(buff,"failure")==0){
      printf("Not a valid directory\n");
    }else{
      char *clientManifest = loadClientManifest(argv[2],".Manifest");
      if(strcmp(clientManifest,"failed")==0){
       int wr =  write(socketfd,"4:fail",strlen("4:fail"));
       if(wr<0){
         perror("Write error");exit(1);
       }
        printf("Failed");exit(1);
      }
      int cVersion = getVersion(clientManifest);
      int sVersion = getVersion(buff);
      if(cVersion!=sVersion){
        printf("Failed: Version were different\n");
       int wr =  write(socketfd,"4:fail",strlen("4:fail"));
       if(wr<0){
         perror("Write error");exit(1);
       }
       exit(1);
      }else{
        if(clientManifest[strlen(clientManifest)-1]!='\n'){
          char *tmp = (char*)malloc((strlen(clientManifest))*sizeof(char));
          bzero(tmp,'\0');
          strcpy(tmp,clientManifest);
          clientManifest = (char*)realloc(tmp,(strlen(tmp)+2)*sizeof(char));
          clientManifest[strlen(tmp)]='\n';
        }
       serverManifestLL(buff);
       int fd = makeCommit(argv[2]);
       if(fd<0){
         perror("Error creating and opening commit\n");
         int wr = write(socketfd,"4:fail",strlen("4:fail"));
         exit(1);
       }
       tokenizeClient(&fd,&socketfd,clientManifest,argv[2]);
       char *commitContent = loadClientManifest(argv[2],".Commit");
       int contentLength = strlen(commitContent);
       char lengthInChar[20];
       bzero(lengthInChar,sizeof(lengthInChar));
       sprintf(lengthInChar,"%d",contentLength);
       char path[strlen(argv[2])+strlen("/.Commit")];
       memset(path,'\0',sizeof(path));
       strcat(path,argv[2]);
       strcat(path,"/.Commit");
       int pathLength = strlen(path);
       char pathLengthStr[20];
       memset(pathLengthStr,'\0',sizeof(pathLengthStr));
       sprintf(pathLengthStr,"%d",pathLength);
       char writeBackProtocol[strlen(commitContent)+strlen(lengthInChar)+strlen(pathLengthStr)+(3*strlen(":"))+strlen(path)];
        memset(writeBackProtocol,'\0',sizeof(writeBackProtocol));
        strcat(writeBackProtocol,pathLengthStr);
        strcat(writeBackProtocol,":");
        strcat(writeBackProtocol,path);
        //strcat(writeBackProtocol,":");
        strcat(writeBackProtocol,lengthInChar);
        strcat(writeBackProtocol,":");
        strcat(writeBackProtocol,commitContent);
        //printf("WriteBack: %s \n",writeBackProtocol);
        int wr = write(socketfd,writeBackProtocol,strlen(writeBackProtocol));
        if(wr<0){perror("Error writing to socket!"); exit(1);}
        printf("Successful Commit!\n");
        printf("Disconnected from the client\n");
      }
    }


 }else if(strcmp(argv[1],"push")==0){
   bzero(buff,strlen(buff));
   char CommitPath[strlen(argv[2])+strlen("/.Commit")];
   memset(CommitPath,'\0',sizeof(CommitPath));
   strcat(CommitPath,argv[2]);
   strcat(CommitPath,"/.Commit");
   int fd = open(CommitPath,O_RDONLY);
   if(fd<0){
    n = write(socketfd,"bad",strlen("bad"));
    if(n<0){perror("Write to socket failed");exit(1);}
    printf("Failed because .Commit cannot be extracted\n");
    exit(1);
   }else{
     //printf("%s \n",protocol);
     char *commitContent = loadClientManifest(argv[2],".Commit");
     char length[20];
     memset(length,'\0',sizeof(length));
     sprintf(length,"%d",strlen(commitContent));
     char modifiedProtocol[strlen(protocol)+strlen(length)+strlen(":")+strlen(commitContent)];
      memset(modifiedProtocol,'\0',sizeof(modifiedProtocol));
     strcat(modifiedProtocol,protocol);
     strcat(modifiedProtocol,length);
     strcat(modifiedProtocol,":");
     strcat(modifiedProtocol,commitContent);
    // printf("Sending: %s\n",modifiedProtocol);
      n = write(socketfd,modifiedProtocol,strlen(modifiedProtocol));
      if(n<0){perror("write to socket failed");exit(1);}
      char goSignal[8];memset(goSignal,'\0',sizeof(goSignal));
      int readIn =0;
      do{
       n = read(socketfd,goSignal+readIn,7);
       if(n<0){
        perror("read failure");exit(1);
       }
       readIn+=n;
      }while(n>0 && readIn<7);
      if(strcmp(goSignal,"failure")==0){
        printf("Push failed .Commit are not same or .Commits are empty\n");
        close(socketfd);
        exit(1);
      }
       char** test = endLineSeparate(commitContent);
        int size = numberOfEndLines(commitContent);
       // printf("%d\n", size);
       int i;int sizeZ=0;
      if(size==0){
        printf("Warning: there is nothing to push!\n");
        remove(CommitPath);
        int wr = write(socketfd,"5:empty",strlen("5:empty"));
        if(wr<0){perror("Failed");exit(1);}
        return 0;
      }
       for(i = 0; i < size; i++){
        char* str1 = pathSeparate(test[i]);
        char pathLength[20];
        memset(pathLength,'\0',sizeof(pathLength));
        sprintf(pathLength,"%d",strlen(str1));
        int fd = open(str1,O_RDONLY);
        if(fd<0){perror("failed");exit(1);}
        char *content = getContent(fd);
        char contentSize[20];
        memset(contentSize,'\0',sizeof(contentSize));
        if((size-1) ==i){
        sprintf(contentSize,"%d",(strlen(content))+strlen("^"));
        }else{
          sprintf(contentSize,"%d",(strlen(content)));
        }
         char protocol[strlen(pathLength)+strlen(":")+strlen(str1)+strlen(":")+strlen(contentSize)+strlen(content)+strlen("^")];
         memset(protocol,'\0',sizeof(protocol));
         strcat(protocol,pathLength);
         strcat(protocol,":");
         strcat(protocol,str1);
         strcat(protocol,contentSize);
         strcat(protocol,":");
         strcat(protocol,content);
         if(i==(size-1))
         strcat(protocol,"^");
         //printf("ToSend: %s\n",protocol);
         int wr = write(socketfd,protocol,strlen(protocol));
         if(wr<0){perror("Failed");exit(1);}

       }
       char *manifestP = getUpdatedManifest(&socketfd);
         remove(manifestP);
         int mfd = open(manifestP,O_CREAT|O_RDWR,00600);
         if(mfd<0){
           perror("failed\n");
           exit(1);
         }
         char *newManifestContent = getUpdatedManifest(&socketfd);
         int wr = write(mfd,newManifestContent,strlen(newManifestContent));
         if(wr<0){
           perror("Write failed\n");exit(1);
         }
         remove(CommitPath);
         printf("successful push!!\n");
         printf("Disconnected from the server\n");
   }

 }else if(strcmp(argv[1],"update")==0){
   int wr = write(socketfd,protocol,strlen(protocol));
   char *manifestReceived = getUpdatedManifest(&socketfd);
   if(strcmp(manifestReceived,"failure")==0){
     printf("project does not exists!\n");
     exit(1);
   }
   char Update [strlen(".Update")+strlen(argv[2])+sizeof("/")];
   bzero(Update,sizeof(Update));
   strcat(Update,argv[2]);
   strcat(Update,"/.Update");
  // printf("%s\n",Update);
   int ufd = open(Update,O_RDWR | O_CREAT,0600);
   if(ufd<0){
     perror("open error\n");
     exit(1);
   }
   char Conflict [strlen(".Conflict")+strlen(argv[2])+sizeof("/")];
   bzero(Conflict,sizeof(Conflict));
   strcat(Conflict,argv[2]);
   strcat(Conflict,"/.Conflict");
   int cfd = open(Conflict,O_RDWR | O_CREAT,00600);
   if(cfd<0){
     perror("open error\n");
     exit(1);
   }
   char manifest [strlen(".Manifest")+strlen(argv[2])+sizeof("/")];
   memset(manifest,'\0',sizeof(manifest));
   strcat(manifest,argv[2]);
   strcat(manifest,"/");
   strcat(manifest, ".Manifest");
   char * getClientManifest = readFile(manifest);
   int ClientManifestVersion =getVersion(getClientManifest);
   int ServerManifestVersion = getVersion(manifestReceived);
    if(ClientManifestVersion==ServerManifestVersion){
      int wr = write(socketfd,"7:success",strlen("7:success"));
      if(wr<0){
       perror("write error");exit(1);
      }
      printf("Upto Date!\n");
      remove(Conflict);
      return 0;
    }
   serverManifestLL(manifestReceived);
   //printLL();
   clientManifestLL(getClientManifest);
   //printCLL();
   int fail = doActualUpdate(ufd,cfd);
   if(fail){
     remove(Update);
   }else{
     remove(Conflict);
   }
 }else if(strcmp(argv[1],"upgrade")==0){
    traverseUpdate(&socketfd,argv[2]);
    printf("Upgrade Successful!\n");
    printf("Disconnected from the server\n");
 }else if(strcmp(argv[1],"checkout")==0){
   int wr = write(socketfd,protocol,strlen(protocol));
   char *manifest =getUpdatedManifest(&socketfd);
    if(strcmp(manifest,"fail")==0){
      printf("no such directory exists on server\n");
      printf("Disconnected from the server\n");
      return -1;
    }
    char path [strlen(".Manifest")+strlen(argv[2])+sizeof("/")];
    memset(path,'\0',sizeof(path));
    strcat(path,argv[2]);
    strcat(path,"/");
    strcat(path, ".Manifest");
    createDir(path);
    int fd = open(path,O_CREAT|O_RDWR,00600);
    int r = write(fd,manifest,strlen(manifest));
   serverManifestLL(manifest);
   getDirectories(argv[2],&socketfd);
   printf("successfully checked out!\n");
    printf("Disconnected from the server\n");
 }

 return 0;
}
