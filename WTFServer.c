#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <pthread.h>
typedef struct Node {
    char *tag;
    char *pathName;
    char *version;
    char *hashcode;
    struct Node* next;
}node;
node *commitHead = NULL;
node *serverHead = NULL;
int size;
char * buff;
char *getContent(int fd){
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
char *getPath(char *dir, char *fileName){
  char *s = (char*) malloc(strlen(dir)+strlen(fileName));
   memset(s,'\0',strlen(s));
   strcat(s,dir);
   strcat(s,fileName);
   return s;
}
char* addFromCommit(){
  char *historyData = (char*)malloc(size *sizeof(char));
  memset(historyData,'\0',sizeof(historyData));
  node *tmp = commitHead;
  while (tmp!=NULL) {
    int s = strlen(historyData)+strlen(tmp->tag)+strlen("\t")+strlen(tmp->pathName)+
    strlen("\t")+strlen(tmp->hashcode)+strlen("\n");
   if(s>=size){
     size = 2*s;
     char *tmp = (char*)malloc((strlen(historyData)+2)*sizeof(char));
     memset(tmp,'\0',strlen(tmp));
     strcpy(tmp,historyData);
     historyData = (char*)malloc(size*sizeof(char));
     memset(historyData,'\0',strlen(historyData));
     strcpy(historyData,tmp);
   }
    strcat(historyData,tmp->tag);
    strcat(historyData,"\t");
    strcat(historyData,tmp->pathName);
    strcat(historyData,"\t");
    strcat(historyData,tmp->hashcode);
    strcat(historyData,"\n");
    tmp=tmp->next;
  }
  return historyData;
}
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
void addToHistory(char *repo,char *progVersion){
  char *historyPath = getPath(repo,"/.History");
  int hfd = open(historyPath,O_RDWR);
  if(hfd<0){
    perror("Cannot open history");exit(1);
  }
  char *historyContent = getContent(hfd);
  if(strcmp(progVersion,"1")==0){
    int wr = write(hfd,"1\n",strlen("1\n"));
    if(wr<0){
      perror("error writing");exit(1);
    }
    char *contents = addFromCommit();
    wr = write(hfd,contents,strlen(contents));
    if(wr<0){
      perror("error writing");exit(1);
    }
  }
  else{
    remove(historyPath);
    char *contents = addFromCommit();
    char toPrint[strlen(contents)+strlen(historyContent)+strlen(progVersion)+strlen("\n")];
    memset(toPrint,'\0',sizeof(toPrint));
    strcat(toPrint,historyContent);
    strcat(toPrint,progVersion);
    strcat(toPrint,"\n");
    strcat(toPrint,contents);
    hfd = open(historyPath,O_CREAT|O_RDWR,00600);
    if(hfd<0){perror("Open failed");exit(1);}
    int rs = write(hfd,toPrint,strlen(toPrint));
    if(rs<0){perror("Write failed");exit(1);}
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
//-1 if .Commit does not exists
//0 if .Commit is empty
//1  if .Commit has data
int isPathValid(char *directory,char *file){
 char updatePath[strlen("/")+strlen(file)+strlen(directory)];
 memset(updatePath,'\0',strlen(updatePath));
 strcat(updatePath,directory);
 strcat(updatePath,"/");
 strcat(updatePath,file);
  struct stat doesFileExists;
     if(stat(updatePath, &doesFileExists)){
         return -1;
     }
     else if(doesFileExists.st_size <= 1){
      return 0;
     }
 return 1;
}
char* updateManifest(int fd,int len){
  char *buf = (char*)malloc(len *sizeof(char));
  memset(buf,'\0',strlen(buf));
  node *tmp = serverHead;
  while(tmp!=NULL){
    if((strlen(buf)+strlen(tmp->version))>=strlen(buf)){
        len = ((strlen(buf)+strlen(tmp->version))+10);
       char *temp = (char*)malloc((strlen(buf)+1)*sizeof(char));
       memset(temp,'\0',strlen(temp));
       strcpy(temp,buf);
       buf = (char*)malloc(len*(sizeof(char)));
       memset(buf,'\0',strlen(buf));
       strcpy(buf,temp);
    }
    strcat(buf,tmp->version);
    strcat(buf,"\t");
    if((strlen(buf)+strlen(tmp->pathName))>=strlen(buf)){
        len = ((strlen(buf)+strlen(tmp->pathName))+10);
       char *temp = (char*)malloc((strlen(buf)+1)*sizeof(char));
       memset(temp,'\0',strlen(temp));
       strcpy(temp,buf);
       buf = (char*)malloc(len*(sizeof(char)));
       memset(buf,'\0',strlen(buf));
       strcpy(buf,temp);
    }
    strcat(buf,tmp->pathName);
    strcat(buf,"\t");
     if((strlen(buf)+strlen(tmp->hashcode))>=strlen(buf)){
        len = ((strlen(buf)+strlen(tmp->hashcode))+10);
       char *temp = (char*)malloc((strlen(buf)+1)*sizeof(char));
       memset(temp,'\0',strlen(temp));
       strcpy(temp,buf);
       buf = (char*)malloc(len*(sizeof(char)));
       memset(buf,'\0',strlen(buf));
       strcpy(buf,temp);

    }
    strcat(buf,tmp->hashcode);
    strcat(buf,"\n");
    tmp = tmp->next;
  }
  int wr = write(fd,buf,strlen(buf));
  if(wr<0){
    perror("Write to manifest failed");exit(1);
  }
  return buf;
}
void deleteNode(char *path){
  node *prev = NULL;
  node *curr = serverHead;
  if(strcmp(curr->pathName,path)==0){
        serverHead=curr->next;
        free(serverHead);
        return;
  }
  while((curr != NULL)&&((strcmp(path,curr->pathName)!=0))){
    prev=curr;
    curr=curr->next;
  }
  if(curr!=NULL){
    prev->next=curr->next;
    free(curr);
  }
}
void addNode(char *pathName,char * version,char *hashcode){
    node* newEntry = (node*)malloc(sizeof(node));
    newEntry->version = (char*)malloc((strlen(version)+1)*sizeof(char));
    bzero(newEntry->version,strlen(newEntry->version));
    memcpy((newEntry->version),version,strlen(version));

    newEntry->pathName = (char*)malloc((strlen(pathName)+1)*sizeof(char));
    bzero(newEntry->pathName,strlen(newEntry->pathName));
    memcpy((newEntry->pathName),pathName,strlen(pathName));

  /* char *hash = (char*)malloc(40 *sizeof(char));
   memset(hash,'\0',strlen(hash));
   hash = hashConvert(localname);*/

   newEntry->hashcode = (char*)malloc((50*sizeof(char)));
   memset(newEntry->hashcode,'\0',strlen(newEntry->hashcode));
   strcpy(newEntry->hashcode,hashConvert(newEntry->pathName));

    newEntry->next=serverHead;
    serverHead=newEntry;
}
void findAndUpdate(char* version,char *path){
  node *tmp = serverHead;
  while((tmp!=NULL)&&(strcmp(path,tmp->pathName)!=0)){
   tmp=tmp->next;
  }
  memset(tmp->version,'\0',strlen(tmp->version));
  memcpy(tmp->version,version,sizeof(version));

  tmp->hashcode = (char*)malloc((50*sizeof(char)));
  memset(tmp->hashcode,'\0',strlen(tmp->hashcode));
  strcpy(tmp->hashcode,hashConvert(tmp->pathName));

}
void updateManifestLL(){
  node *curr = commitHead;
  while(curr!=NULL){
   if(strcmp(curr->tag,"M")==0){
     findAndUpdate(curr->version,curr->pathName);
   }else if(strcmp(curr->tag,"D")==0){
      deleteNode(curr->pathName);
   }else{
     addNode(curr->pathName,curr->version,curr->hashcode);
   }
   curr=curr->next;
  }
}
char *getCode(char *path){
  char *tag = (char*)malloc(2*sizeof(char));
  memset(tag,'\0',strlen(tag));
  node *tmp = commitHead;
  while(tmp!=NULL){
    if(strcmp(tmp->pathName,path)==0){
      memcpy(tag,tmp->tag,strlen(tmp->tag));
      break;
    }
   tmp=tmp->next;
 }
 return tag;
}
void addToList(char *t,char *p, char *h,char *v){
  if(commitHead==NULL){
    commitHead = (node*)malloc(sizeof(node));
    commitHead->tag = (char*)malloc((strlen(t)+1)*sizeof(char));
    bzero(commitHead->tag,strlen(commitHead->tag));
    memcpy((commitHead->tag),t,strlen(t));

    commitHead->version = (char*)malloc((strlen(v)+1)*sizeof(char));
    bzero(commitHead->version,strlen(commitHead->version));
    memcpy((commitHead->version),v,strlen(v));

    commitHead->pathName = (char*)malloc((strlen(p)+1)*sizeof(char));
    bzero(commitHead->pathName,strlen(commitHead->pathName));
    memcpy((commitHead->pathName),p,strlen(p));

    commitHead->hashcode = (char*)malloc((strlen(h)+1)*sizeof(char));
    bzero(commitHead->hashcode,strlen(commitHead->hashcode));
    memcpy((commitHead->hashcode),h,strlen(h));
    commitHead->next=NULL;
    return;
  }
  node *current = commitHead;
  node *prev = NULL;
  while(current!=NULL){
    prev = current;
    current=current->next;
  }
  node *newEntry = (node*)malloc(sizeof(node));
  newEntry->tag = (char*)malloc((strlen(t)+1)*sizeof(char));
    bzero(newEntry->tag,strlen(newEntry->tag));
    memcpy((newEntry->tag),t,strlen(t));

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
void addToMList(char *v,char *p, char *h){
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
     memcpy(hashcode,tmpStr,strlen(tmpStr));
     memset(tmpStr,'\0',sizeof(tmpStr));
     addToMList(version,path,hashcode);
     k =0;TabCounter=0;
   }
 }
}
void CommitLL(char *manifest){
 int TabCounter=0;int i=0;int k =0;int l=0;int TabCounterLim=3;
 char *tag;char *path; char *hashcode;char *version;
 char tmpStr[strlen(manifest)];
 memset(tmpStr,'\0',sizeof(tmpStr));
// strcat(toPrint,"0\n");
 for(i=l;i<strlen(manifest);i++){
   while((manifest[i]!='\t')&&(i<strlen(manifest))){
    tmpStr[k]= manifest[i];
    k++;i++;
   }

   if((manifest[i]=='\t') && (TabCounter<TabCounterLim)){
     TabCounter++;
     if(TabCounter<TabCounterLim){
       if(TabCounter==1){
        tag = (char*)malloc((k+5)*sizeof(char));
        bzero(tag,strlen(tag));
        memcpy(tag,tmpStr,strlen(tmpStr));
        memset(tmpStr,'\0',sizeof(tmpStr)); k=0;

       } else if(TabCounter==2){
          version = (char*)malloc((k+5)*sizeof(char));
          bzero(version,strlen(version));
          memcpy(version,tmpStr,strlen(tmpStr));
          memset(tmpStr,'\0',sizeof(tmpStr));
          k=0;
       }
     }
   }
   if(TabCounter==TabCounterLim){
      path = (char*)malloc((k+5)*sizeof(char));
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
     memcpy(hashcode,tmpStr,strlen(tmpStr));
     memset(tmpStr,'\0',sizeof(tmpStr));
     addToList(tag,path,hashcode,version);
     k =0;TabCounter=0;
   }
 }
}

void makeFile(char *fname,char *name){
  int fd = open(fname,O_RDWR | O_CREAT, 00600);
  if(fd==0){perror("Cannot create/open file");exit(1);}
  char manifestContent[strlen(fname)+ sizeof("0")+strlen(name)+sizeof('\t')];
  bzero(manifestContent,strlen(manifestContent));
  strcat(manifestContent,"0");
  strcat(manifestContent,"\n");
  //strcat(manifestContent,name);
  write(fd,manifestContent,strlen(manifestContent));
}
int makeDir(char *name,char flag){
 //printf("%s\n",name);
 DIR *dir;
 dir = opendir(name);
 if(dir != NULL){printf("directory exists");return -1;}
 else{
  int k = mkdir(name,0777);//printf("%d\n",k);
  if(k!=0){perror("make directory error");exit(1);}
 // printf("Here\n");
  if(flag=='c'){
  char manifestPath [strlen(name)+ sizeof(".Manifest") +sizeof("/")+sizeof("/0")];
  memset(manifestPath,'\0',strlen(manifestPath));
  strcat(manifestPath,name);
  strcat(manifestPath,"/.Manifest");
  char HistoryPath [strlen(name)+ sizeof(".History") +sizeof("/")+sizeof("/0")];
  memset(HistoryPath,'\0',strlen(HistoryPath));
  strcat(HistoryPath,name);
  strcat(HistoryPath,"/.History");
  int fd = open(HistoryPath,O_CREAT|O_RDWR,00600);
  if(fd<0){
    perror("Fail to Open");exit(1);
  }
  makeFile(manifestPath,name);
  }
 }
 return 0;
}
int duplicateDir(char *directory,char *verDir){
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
       if(strcmp(sd->d_name,".")==0 || strcmp(sd->d_name,"..")==0 || strcmp(sd->d_name,"History")==0)
        continue;

           //int length = sizeof(pathName)+(sizeof(sd->d_name) +sizeof("/")+sizeof("\0"));
           char localname[20000];
           strcpy(localname,directory);
           strcat(localname,"/");
           strcat(localname,sd->d_name);
           if(stat(localname,&buff)==0){
             if(S_ISDIR(buff.st_mode)>0){
               char *localname2 = (char*)malloc(20000 *sizeof(char));
               memset(localname2,'\0',strlen(localname2));
               strcpy(localname2,verDir);
               strcat(localname2,"/");
               strcat(localname2,sd->d_name);
               int k = makeDir(localname2,'p');
               doesFileExists =duplicateDir(localname,localname2);
             }
            if(S_ISREG(buff.st_mode)>0){
             if((strcmp(sd->d_name,".Commit")!=0)&&(strcmp(sd->d_name,".History")!=0)){
              int origfd = open(localname,O_RDONLY);
              if(origfd<0){perror("open failed");exit(1);}
              char *getFileContent = getContent(origfd);
              char *localname2 = (char*)malloc(20000 *sizeof(char));
              memset(localname2,'\0',strlen(localname2));
              strcpy(localname2,verDir);
              strcat(localname2,"/");
              strcat(localname2,sd->d_name);
              int fd = open(localname2,O_CREAT|O_RDWR,00600);
              if(fd<0){perror("creation failed");exit(1);}
              int wr = write(fd,getFileContent,strlen(getFileContent));
              if(wr<0){perror("write failed error"); exit(1);}
              doesFileExists =1;
             }

            }

            }
         }

     }
      return doesFileExists;
}

char* getProjectVersion(char *path){
  int fd = open(path,O_RDONLY);
  if(fd<0){
    perror("Error opening manifest");exit(1);
  }
  int readIn =0; int status=0;
  int versionLength = 20;
  char *buf = malloc(versionLength *sizeof(char));
  memset(buf,'\0',strlen(buf));
  do{
    status = read(fd,buf+readIn,1);
    if(status<0){
      perror("Cannot open");exit(1);
    }
    readIn+=status;
    if(readIn>=versionLength){
      versionLength = 2*versionLength;
       char *tmp = (char*)malloc(strlen(buf)*sizeof(char));
       bzero(tmp,strlen(tmp));
       strcpy(tmp,buf);
       buf = (char*)realloc(tmp,versionLength*(sizeof(char)));
    }
  }while(strstr(buf,"\n")!=NULL);

    char *bytesToRead = (char*)malloc((strlen(buf) +1)*sizeof(char));
    memset(bytesToRead,'\0',strlen(bytesToRead));
    memcpy(bytesToRead,buf,strlen(buf));
    return bytesToRead;
}
char * readClientMessage(int *socketfd){
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
  //  printf("Client message: %s\n", clientMessage);
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
       clientMessage = (char*)realloc(tmp,size*(sizeof(char)));
     }
    }while(n>0 && readIn<nextRead);
    //printf("Client Message: %s\n",clientMessage);
    return clientMessage;
}
int removeDir(char *directory){
 int removed = -1;
  DIR *dir;
  struct dirent *sd;
  struct stat buff;
  dir = opendir(directory);
  if(dir == NULL){
    return removed;
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
               removed = removeDir(localname);
                continue;
             }
            if(S_ISREG(buff.st_mode)>0){
                if(unlink(localname)==0){
                 printf("Removed:%s\n",localname);
                 removed = 0;
                }else{
                 removed =-1;
                 printf("Cannot remove %s\n",localname);
                }
            }
          }
     }//while loop ends
     if(rmdir(directory)==0){removed = 0; printf("Removed dir\n");}
     else { removed =-1; printf("Cannot remove dir\n");}
   }
   return removed;
}
int sendManifest(int *sockefd, char *dir){
  DIR *directory;
  directory = opendir(dir);
  if(directory==NULL){return -1;}
  char manifestPath[strlen("/.Manifest")+strlen(dir)];
  memset(manifestPath,'\0',sizeof(manifestPath));
  strcat(manifestPath,dir);
  strcat(manifestPath,"/.Manifest");
  int fd = open(manifestPath,O_RDONLY);
  if(fd<0){
    perror(manifestPath);
    return -1;
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
    if(ReadIn>=size){
      length= length*2;
      char *tmp = (char*)malloc(strlen(BufferToSend)*sizeof(char));
       bzero(tmp,strlen(tmp));
       strcpy(tmp,BufferToSend);
       BufferToSend = (char*)realloc(tmp,length*(sizeof(char)));
    }
  }while(status>0 && ReadIn<length);
  //printf("%s\n",BufferToSend);
  int totalBytes = strlen(BufferToSend);
  char bytesToSend[20];
  memset(bytesToSend,'\0',sizeof(bytesToSend));
  sprintf(bytesToSend,"%d",totalBytes);
  char writeBackProtocol[strlen(bytesToSend)+strlen(":")+strlen(BufferToSend)];
  memset(writeBackProtocol,'\0',sizeof(writeBackProtocol));
  strcat(writeBackProtocol,bytesToSend);
  strcat(writeBackProtocol,":");
  strcat(writeBackProtocol,BufferToSend);
  int wr = write(*sockefd,writeBackProtocol,strlen(writeBackProtocol));
  if(wr<0){
    perror("write back failed");return -1;
  }
  return 0;
}
int getBytes(int *start, char delimeter){
  char * bytes = (char*) malloc(size*sizeof(char));
  memset(bytes,'\0',strlen(bytes));
  int i = *start;
  int k =0;
  while(buff[i]!= delimeter){
    bytes[k] = buff[i];
    i++;k++;

  }
  *start = i;
return atoi(bytes);

}
/*void printCLL(){
  node *tmp = commitHead;
  //node *tmp = serverHead;
  while(tmp!=NULL){
    printf("%s\t",tmp->tag);
    printf("%s\t",tmp->version);
    printf("%s\t",tmp->pathName);
    printf("%s\t\n",tmp->hashcode);
    tmp= tmp->next;
  }
  printf("---------------\n");
}
void printLL(){
  //node *tmp = commitHead;
  node *tmp = serverHead;
  while(tmp!=NULL){
    printf("%s\t",tmp->version);
    printf("%s\t",tmp->pathName);
    printf("%s\t\n",tmp->hashcode);
    tmp= tmp->next;
  }
  printf("---------------\n");
}*/
void create(char *fname,int *sockfd){
 int l = makeDir(fname,'c');
 char *writebackprotocol = (char*) malloc(8*sizeof(char));
 if(l == 0){
   writebackprotocol = "success";
 }else{
   writebackprotocol = "failure";

 }

 int n = write(*sockfd,writebackprotocol,sizeof(writebackprotocol));
 if(n<0){perror("Write to client failed");exit(1);}
 //printf("%s\n",writebackprotocol);


}
char* readProtocol(int *sockid){
  int readIn = 0;
  int n = 0;int s =1;
 // printf("Buffer: %s\n", buff);
  do{
    n = read(*sockid,buff+readIn,1);
    if(n<0){
      perror("read failure");exit(1);
    }
    readIn+=n;

 }while(strstr(buff,":")==NULL);
// printf("Buff: %s\n",buff);
 readIn = 0;
 int readBuff = 0;
 int bytesToRead = getBytes(&readBuff,':');
 //printf("FileBytes: %d\n", bytesToRead);
 char *fname = (char*) malloc((bytesToRead+1)*sizeof(char));
 memset(fname,'\0',(bytesToRead+1));
 do{
    n = read(*sockid,fname+readIn,bytesToRead-readIn);
    //printf("bytes read %d\n",n);
    //printf("Fname%s\n",fname);
    if(n<0){
      perror("read failure");exit(1);
    }
    readIn+=n;

 }while(n>0 && readIn<bytesToRead);
 //printf("final Fname%s\n",fname);
 return fname;
 //free(writebackprotocol);exit(0);
}
char *currVersion(char * Dname, int *sockfd){
  DIR *dir;
  dir = opendir(Dname);
  if(dir== NULL){
   if(errno==2){
     return "failed";
   }
    perror("Dname");exit(1);
  }
  int len = 2000;
  char manifestPath[strlen(Dname)+strlen("/.Manifest")+strlen("\0")];
  memset(manifestPath,'\0',strlen(manifestPath));
  strcat(manifestPath,Dname);
  strcat(manifestPath,"/.Manifest");
  int fd = open(manifestPath,O_RDONLY);
  if(fd<0){
   perror("Manifest open error");
   exit(1);
  }
  char * buffer = (char*)malloc(len *sizeof(char));
  memset(buffer,'\0',strlen(buffer));
  int readIn = 0; int status =0;
  do{
    status = read(fd,buffer+readIn,len-readIn);
    if(status<0){
     perror("Read failed");exit(1);
    }
    readIn+=status;
    if(readIn>=len){
       len = 2*len;
       char *tmp = (char*)malloc(strlen(buffer)*sizeof(char));
       bzero(tmp,strlen(tmp));
       strcpy(tmp,buffer);
       buffer = (char*)realloc(tmp,len*(sizeof(char)));
     }

   }while(status>0 && readIn<len);
  //printf("BufferToSend: %s\n", buffer);
  return buffer;
}
void *client_handler(void *arg){
 size = 1024;
  buff = (char*)malloc(size * sizeof(char));
  int *newsockfdAddr = (int*) arg;
  int newsockfd = *newsockfdAddr;
  int readIn =0;
  char * fname = (char*)malloc(size *sizeof(char));
  int n =  read(newsockfd,fname,6);
  if(n < 0) {
    perror("Error receiving\n");exit(1);
  }
 // printf("%s\n",buff);
  int fd = open(fname,O_CREAT|O_RDWR, 00600);
  if(fd <0){

    perror("File open error");exit(1);

  }
  bzero(buff,size);
   readIn = 0;
  do{
    n = read(newsockfd,buff+readIn,size-readIn);
    if(n<0){
      perror("read failure");exit(1);
    }
    readIn+=n;
    if(n>=size){
      size*=2;
      buff = (char*) realloc(buff,size*(sizeof(char*)));
      memset(buff+readIn,'\0',size);
    }

 }while(n >0 && readIn< size);

 n = write(fd,buff,strlen(buff));
 //printf("%s\n",buff);
 if(n<0) {
  printf("%d\n",errno);exit(1);

 }
 size = 0;

 pthread_exit(0);
}
int main(int argc, char *argv[]){

  int sockfd,newsockfd, portno, n;
  size = 1024;
  buff = (char*) malloc(size *sizeof(char));
   //memset(buff,'\0',strlen(buff));
  struct sockaddr_in serv_addr,client_addr;
  socklen_t clientlen;
  sockfd = socket(AF_INET,SOCK_STREAM,0);
  if(sockfd<0){
    perror("Cannot open socket");
    exit(1);
   }
   printf("Socket created successfully\n");
   bzero((char*)&serv_addr,sizeof(serv_addr));
   portno = atoi(argv[1]);

   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = INADDR_ANY;
   serv_addr.sin_port = htons(portno);

   if((bind(sockfd,(struct sockaddr *)&serv_addr, sizeof(serv_addr)))<0){

     perror("Cannot bind");
     exit(1);
    }
   printf("Socket binded successfully\n");
   if(listen(sockfd,15)<0){

      perror("listen");
      exit(1);
   }
   printf("Socket listening...\n");
  clientlen = sizeof(client_addr);
  newsockfd = accept(sockfd,(struct sockaddr *)&client_addr,&clientlen);
   if(newsockfd<0){
     perror("Accept Error");
     exit(1);
   }
   printf("Server accepted a client\n");
    int readIn = 0;
    int readL = 3;
    memset(buff,'\0',strlen(buff));
  do{
    //printf("Size: %d\n",readL);
    n = read(newsockfd,buff+readIn,readL-readIn);
   // printf("%d\n",n);
   //printf("%s\n",buff);
    if(n<0){
      perror("read failure");exit(1);
    }
    readIn+=n;

 }while(n >0 && readIn< readL);
 //printf("%s\n",buff);
 if(strcmp(buff,"bad")==0){
   printf("failed\n");exit(1);
   printf("Server Disconnected\n");
 }
 if(strstr(buff,"crt")!=NULL){
    bzero(buff,size);
   char*fname = readProtocol(&newsockfd);
    create(fname,&newsockfd);
 }else if(strstr(buff,"crv")!=NULL){
    bzero(buff,size);
    char*DirName = readProtocol(&newsockfd);
    char * dataToSend = currVersion(DirName,&newsockfd);
    int bytesToSend = strlen(dataToSend);
    char stringBytesToSend[20];
    sprintf(stringBytesToSend,"%d",bytesToSend);
    char writeBackProtocol [sizeof(":")+sizeof(stringBytesToSend)+strlen(dataToSend)];
    memset(writeBackProtocol,'\0',strlen(writeBackProtocol));
    strcat(writeBackProtocol,stringBytesToSend);
    strcat(writeBackProtocol,":");
    strcat(writeBackProtocol,dataToSend);
    n = write(newsockfd,writeBackProtocol,strlen(writeBackProtocol));
    if(n<0){perror("write to socket failed");exit(1);}
    if(dataToSend[0]=='f'){
     printf("There is no such directory on server\n");
    }else
    printf("Successfully sent information!\n");
 }else if(strstr(buff,"des")!=NULL){
  char*DirName = readProtocol(&newsockfd);
  int failed = removeDir(DirName);
  if(failed == 0){
  printf("directory removed!");
  char name[7]="success";
  name[7]='\0';
   n = write(newsockfd,name,strlen(name));
   if(n<0){
    perror("Write failed");exit(1);
   }
  }else if(failed ==-1){
   printf("Failed to remove!\n");
   char name[7]="failure";
   name[7]='\0';
   n = write(newsockfd,name,strlen(name));
   if(n<0){
    perror("Write failed");exit(1);
   }
  }
 }else if(strstr(buff,"com")!=NULL){
   char*DirName = readProtocol(&newsockfd);
   if(sendManifest(&newsockfd,DirName)==0){
     //printf("Manifest sent to client\n");
     char *clientMessage = readClientMessage(&newsockfd);
     if(strcmp(clientMessage,"fail")==0){
       printf("Failed!\n");
       printf("Server Disconnected\n");return 0;
     }
     int fd = open(clientMessage,O_CREAT|O_RDWR,00600);
     if(fd<0){
       perror("creating/opening error!");exit(1);
     }
     memset(clientMessage,'\0',strlen(clientMessage));
     clientMessage = readClientMessage(&newsockfd);
     int wr = write(fd,clientMessage,strlen(clientMessage));
     if(wr<0){
       perror("write error");exit(1);
     }
     printf("Successfully added .Commit\n");
   }else{
     int wr = write(newsockfd,"7:failure",strlen("7:failure"));
     if(wr<0){
       perror("Write to socket failed!");
       exit(1);
     }
     printf("Failure: %s does not exit\n",DirName);
   }

 }else if(strstr(buff,"psh")!=NULL){
   char *dir = readProtocol(&newsockfd);
   DIR * dirC;
   dirC = opendir(dir);
   if(dirC==NULL){
     printf("No such directory\n");
     int wr = write(newsockfd,"failure",strlen("failure"));
     if(wr<0){perror("write failed"); exit(1);}
     return -1;
   }
   char *CommitPath = getPath(dir,"/.Commit");
   int fd = open(CommitPath,O_RDONLY);
   if(fd<0){
     perror("Cannot open commit");exit(1);
   }
   char *commitContent = getContent(fd);
   char *clientCommit = readClientMessage(&newsockfd);
   int isCommitEmpty = isPathValid(dir,"/.Commit");
   if(!(strcmp(commitContent,clientCommit)==0 && isCommitEmpty!=0)){
     int wr = write(newsockfd,"failure",strlen("failure"));
     if(wr<0){perror("write failed"); exit(1);}
     if(strcmp(commitContent,clientCommit)!=0){
       printf("Failed commits are not same\n");
       printf("Server Disconnected\n");
     } else{
       printf(".Commit are empty\n");
       printf("Server Disconnected\n");
     }
      exit(1);
     // close(newsockfd);
   }
   int wr = write(newsockfd,"success",strlen("success"));
   char *newDir = (char*)malloc(1000*sizeof(char));
   memset(newDir,'\0',strlen(newDir));
   strcpy(newDir,dir);
   strcat(newDir,"/History");
   makeDir(newDir,'p');
   char * project = getPath(dir,"/.Manifest");
   char *projectVersion = getProjectVersion(project);
   int progVersion = atoi(projectVersion);
   progVersion++;
   int pfd = open(project,O_RDONLY);
   char *manifestContent = getContent(pfd);
   int length = strlen(manifestContent);
   if(manifestContent[strlen(manifestContent)-1]!='\n'){
     char *tmp = (char*)malloc((strlen(manifestContent))*sizeof(char));
          bzero(tmp,'\0');
          strcpy(tmp,manifestContent);
          manifestContent = (char*)realloc(tmp,(strlen(tmp)+2)*sizeof(char));
          manifestContent[strlen(tmp)]='\n';
   }
   char *newSubdir = (char*) malloc((strlen(projectVersion)+strlen(newDir)+strlen("/"))*sizeof(char));
   memset(newSubdir,'\0',strlen(newSubdir));
   strcat(newSubdir,newDir);
   strcat(newSubdir,"/");
   strcat(newSubdir,projectVersion);
   makeDir(newSubdir,'p');
   if(duplicateDir(dir,newSubdir)==-1){
     printf("Server Disconnected\n");
   }else{
     serverManifestLL(manifestContent);
     CommitLL(commitContent);
     /*if(strcmp(pathToCheck,"empty")==0){
        printf("Push was successful!\n");
        return 0;
     }*/
     char *pathToCheck;
     int isEnd = 0;
      do{
       pathToCheck = readClientMessage(&newsockfd);
       char * tag = getCode(pathToCheck);
        if(strcmp(tag,"M")==0){
        remove(pathToCheck);
        int fd = open(pathToCheck,O_CREAT|O_RDWR,00600);
        if(fd<0){perror("Error opening");exit(1);}
        char *fileContents = readClientMessage(&newsockfd);
        if(fileContents[strlen(fileContents)-1]=='^'){
          isEnd = 1;
          fileContents[strlen(fileContents)-1]='\0';
        }
        int wr = write(fd,fileContents,strlen(fileContents));
       }else if(strcmp(tag,"A")==0){
         createDir(pathToCheck);
         int fd = open(pathToCheck,O_CREAT|O_RDWR,00600);
        if(fd<0){perror("Error opening");exit(1);}
        char *fileContents = readClientMessage(&newsockfd);
        if(fileContents[strlen(fileContents)-1]=='^'){
          isEnd = 1;
          fileContents[strlen(fileContents)-1]='\0';
        }
        int wr = write(fd,fileContents,strlen(fileContents));
       }else if(strcmp(tag,"D")==0){
         remove(pathToCheck);
         char *fileContents = readClientMessage(&newsockfd);
         if(fileContents[strlen(fileContents)-1]=='^'){
          isEnd = 1;
          fileContents[strlen(fileContents)-1]='\0';
        }
       }
     }  while(!isEnd);
     updateManifestLL();
     char charProgVersion[20];
     memset(charProgVersion,'\0',strlen(charProgVersion));
     sprintf(charProgVersion,"%d",progVersion);
     char *pathManifest = getPath(dir,"/.Manifest");
     int pathLength = strlen(pathManifest);
     char manpathlen[20];
     memset(manpathlen,'\0',sizeof(manpathlen));
     sprintf(manpathlen,"%d",pathLength);
     remove(pathManifest);
     int mfd = open(pathManifest,O_CREAT|O_RDWR,00600);
     if(mfd<0){
      perror("Error creating manifest");exit(1);
     }
       char toWrite[strlen(charProgVersion)+strlen("\n")];
       memset(toWrite,'\0',sizeof(toWrite));
       strcat(toWrite,charProgVersion);
       strcat(toWrite,"\n");
       int wr = write(mfd,toWrite,strlen(toWrite));
       if(wr<0){
         perror("Error in writing");exit(1);
       }
      char *updatedManifest =  updateManifest(mfd,length);
      int ManifestLen = (strlen(updatedManifest)+strlen(charProgVersion)+strlen("\n"));
      char ManifestSize[20];
      memset(ManifestSize,'\0',sizeof(ManifestSize));
      sprintf(ManifestSize,"%d",ManifestLen);
      char writeBackManifest [strlen(manpathlen)+strlen(":")+strlen(pathManifest)+strlen(ManifestSize)+strlen(":")+strlen(updatedManifest)];
      memset(writeBackManifest,'\0',sizeof(writeBackManifest));
      strcat(writeBackManifest,manpathlen);
      strcat(writeBackManifest,":");
      strcat(writeBackManifest,pathManifest);
      strcat(writeBackManifest,ManifestSize);
      strcat(writeBackManifest,":");
      strcat(writeBackManifest,charProgVersion);
      strcat(writeBackManifest,"\n");
      strcat(writeBackManifest,updatedManifest);
      wr = write(newsockfd,writeBackManifest,strlen(writeBackManifest));
      if(wr<0){
        perror("Error writing");
        exit(1);
      }
      addToHistory(dir,charProgVersion);
      printf("Successful Push!\n");
      remove(CommitPath);
    // pathToCheck[strlen(pathToCheck)-1]='\0';
    // printf("Done! %s\n",pathToCheck);
   }
 }else if(strstr(buff,"upd")!=NULL){
  char *dir = readProtocol(&newsockfd);
   DIR * dirC;
   dirC = opendir(dir);
   if(dirC==NULL){
     printf("No such directory\n");
     int wr = write(newsockfd,"7:failure",strlen("7:failure"));
     if(wr<0){perror("write failed"); exit(1);}
     printf("Server Disconnected\n");
     return -1;
   }
   char *path = getPath(dir,"/.Manifest");
   int fd = open(path,O_RDONLY);
   char* fileContent = getContent(fd);
   int length = strlen(fileContent);
   char strLen[20];
   memset(strLen,'\0',sizeof(strLen));
   sprintf(strLen,"%d",length);
   char sendMan[strlen(fileContent)+strlen(":")+strlen(strLen)];
   memset(sendMan,'\0',strlen(sendMan));
   strcat(sendMan,strLen);
   strcat(sendMan,":");
   strcat(sendMan,fileContent);
   int writeCheck = write(newsockfd,sendMan,strlen(sendMan));
   if(writeCheck<0){
     perror("Write Failed");exit(1);
   }
   printf("Successfully Updated\n");
 }else if(strstr(buff,"upg")!=NULL){
   int isEnd=0;
   char *pathToCheck;
   do{
     pathToCheck = readClientMessage(&newsockfd);
    if(pathToCheck[strlen(pathToCheck)-1]=='^'){
          isEnd = 1;
          pathToCheck[strlen(pathToCheck)-1]='\0';
    }
    int fd = open(pathToCheck,O_RDONLY);
    char *fileContent = getContent(fd);
    int len = strlen(fileContent);
    char length[20];
    memset(length,'\0',sizeof(length));
    sprintf(length,"%d",len);
    char writeBack[strlen(":")+strlen(length)+strlen(fileContent)];
    memset(writeBack,'\0',sizeof(writeBack));
    strcat(writeBack,length);
    strcat(writeBack,":");
    strcat(writeBack,fileContent);
    int sr = write(newsockfd,writeBack,strlen(writeBack));
   }while(!isEnd);
   char *manifestP = readClientMessage(&newsockfd);
   int mfd = open(manifestP,O_RDONLY);
    char* contentToWriteBack = getContent(mfd);
    int byteToSend = strlen(contentToWriteBack);
    char bytesInStr[20];
    memset(bytesInStr,'\0',sizeof(bytesInStr));
    sprintf(bytesInStr,"%d",byteToSend);
    char writeBackManifestP[strlen(bytesInStr)+strlen(contentToWriteBack)+strlen(":")];
    memset(writeBackManifestP,'\0',sizeof(writeBackManifestP));
    strcat(writeBackManifestP,bytesInStr);
    strcat(writeBackManifestP,":");
    strcat(writeBackManifestP,contentToWriteBack);
    int rb = write(newsockfd,writeBackManifestP,strlen(writeBackManifestP));
    if(rb<0){
      perror("Cannot write back\n");
    }
    printf("Successfully upgraded\n");
 }else if(strstr(buff,"hst")!=NULL){
   char *dir = readClientMessage(&newsockfd);
   DIR *dirC;
   dirC = opendir(dir);
   if(dirC==NULL){
     printf("no such directory exits\n");
     int rb = write(newsockfd,"4:fail",strlen("4:fail"));
     if(rb<0){perror("write back to socket failed");exit(1);}
     printf("Server Disconnected\n");
     exit(1);
   }
   char *pathToRead = getPath(dir,"/.History");
   int fd = open(pathToRead,O_RDONLY);
   char *content = getContent(fd);
   int length = strlen(content);
   char strLength[20];
   memset(strLength,'\0',sizeof(strLength));
   sprintf(strLength,"%d",length);
   char writeToSocket[strlen(strLength)+strlen(":")+strlen(content)];
   memset(writeToSocket,'\0',sizeof(writeToSocket));
   strcat(writeToSocket,strLength);
   strcat(writeToSocket,":");
   strcat(writeToSocket,content);
  // printf("%s\n",writeToSocket);
   int writeR = write(newsockfd,writeToSocket,strlen(writeToSocket));
   if(writeR<0){perror("write failed");exit(1);}
   printf("History sent back");
 }else if(strstr(buff,"cot")!=NULL){
   char *dir = readClientMessage(&newsockfd);
   DIR *dirC;
   dirC = opendir(dir);
   if(dirC==NULL){
    int rb = write(newsockfd,"4:fail",strlen("4:fail"));
    printf("Server Disconnected\n");
     return -1;
   }else{
     char manifestP[strlen(dir)+strlen("/")+strlen(".Manifest")];
     memset(manifestP,'\0',sizeof(manifestP));
     strcat(manifestP,dir);
     strcat(manifestP,"/");
     strcat(manifestP,".Manifest");
     int fd = open(manifestP,O_RDONLY);
     char *cont = getContent(fd);
     char mlength[20];
     memset(mlength,'\0',sizeof(mlength));
     sprintf(mlength,"%d",strlen(cont));
     char mContent[strlen(mlength)+strlen(":")+strlen(cont)];
     memset(mContent,'\0',sizeof(mContent));
     strcat(mContent,mlength);
     strcat(mContent,":");
     strcat(mContent,cont);
     int rb = write(newsockfd,mContent,strlen(mContent));
     //printf("Server Disconnected\n");
   }
   int isEnd=0;
   char *pathToCheck;
   do{
     pathToCheck = readClientMessage(&newsockfd);
    if(pathToCheck[strlen(pathToCheck)-1]=='^'){
          isEnd = 1;
          pathToCheck[strlen(pathToCheck)-1]='\0';
    }
    int fd = open(pathToCheck,O_RDONLY);
    char *fileContent = getContent(fd);
    int len = strlen(fileContent);
    char length[20];
    memset(length,'\0',sizeof(length));
    sprintf(length,"%d",len);
    char writeBack[strlen(":")+strlen(length)+strlen(fileContent)];
    memset(writeBack,'\0',sizeof(writeBack));
    strcat(writeBack,length);
    strcat(writeBack,":");
    strcat(writeBack,fileContent);
    int sr = write(newsockfd,writeBack,strlen(writeBack));
   }while(!isEnd);
   printf("successfully upgraded\n");
 }
 printf("Server Disconnected\n");
  //  printf(buff);
 //n = write(newsockfd,"got it",strlen("got it"));
return 0;
}
