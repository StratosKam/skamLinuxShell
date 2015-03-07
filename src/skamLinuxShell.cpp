/*
 * This file is part of skamLinuxShell.
 *
 * skamLinuxShell is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * skamLinuxShell is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with skamLinuxShell.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright 2015 Stratos Kamadanis
 */
#include <iostream>
#include <vector>
#include <stdio.h> 
#include <string.h>
#include <stdlib.h>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#define INTERVAL 1000  //time in ms between backround processes switch

#define EXIT "exit"
#define CD "cd"
#define PIPELINE "|"
#define REDIN "<"
#define REDOUT ">"
#define REDOUTA ">>"
#define AMPERSAND "&"
#define SPACE " "
#define EMPTYSTR ""
#define PARENTHESISR ")"
#define PARENTHESISL "("
#define SYNTAX_ERROR "Syntax error"

 using namespace std;

 vector<pid_t> backgroundProcs;

 char* getDir();
 void changeDir(const char* newDir);
 void execute(const char *command, char *const args[],
  bool waitExecution,const char *redIn,const char* redOut,bool append);
 void pipe(const char *command1, char *const args1[],const char *command2, char *const args2[],
  bool waitExecution,const char *redIn,const char* redOut,bool append);
 int startRedirectIn(const char *file);
 int startRedirectOut(const char *file,bool append);
 void stopRedirect(int rdr);
 bool isFinished(pid_t pid,int &exitError);
 void backgroundHandler();
 void signalRecieved(int sig);  
 void showCLI();
 bool equals(string string1,string string2);
 bool startsWith(string that,string with);
 bool contains(string string1,string string2);
 vector<string> split(string that,string with);
 char** generateArgs(vector<string> v);
 void removeLastAmp(string& from);
 void trim(string& that);
 void trimEnd(string &that);
 string cutAfter(string& from,string flag);
 bool endsWith(string that,string with);
 bool containsIllegal(string str);
 void printError(int ex);
 void printError(char* message);
 void printPidFinished(pid_t pid);
 void printDirectory();

 int main(int argc, char **argv) {
  struct itimerval it_val; 
  if (signal(SIGALRM, signalRecieved) == SIG_ERR) {
    printError("Error catching SIGALRM.");
    exit(1);
  }
  it_val.it_value.tv_sec =     INTERVAL/1000;
  it_val.it_value.tv_usec =    (INTERVAL*1000) % 1000000; 
  it_val.it_interval = it_val.it_value;
  if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
    printError("Error calling setitimer.");
    exit(1);
  }

  showCLI(); 
  return 0;
}


void signalRecieved(int sig){
 backgroundHandler();
}

/**
*
* Handles the CLI environment. Does not return once it's called.
*
**/
void showCLI(){
  string line;
  do{
    printDirectory();

    getline(cin,line);
    if(contains(line,PARENTHESISR) || contains(line,PARENTHESISL)){
      printError(SYNTAX_ERROR);
      continue;
    }
    trim(line);
    
  if(startsWith(line,CD)){// cd handling
    vector<string> v=split(line,SPACE);
    if(v.size()==2){
     try{
       changeDir(v[1].c_str());
     }catch(int ex){
       printError(ex);
     }
   }else{
    printError(SYNTAX_ERROR);
    continue;
  }
}
else if(equals(line,EXIT)){// exit handling
 exit(0);
}else{// command handling
 if(contains(line,PIPELINE)){// pipeline hanlding
   vector<string> v=split(line,PIPELINE);
   if(v.size()!=2 || contains(v[0],AMPERSAND) || contains(v[0],REDOUT) || contains(v[1],REDIN)){
    printError(SYNTAX_ERROR);
    continue;
  }
  bool wait=true;
  string redIn;
  string redOut;
  bool append;
  if(endsWith(v[1],AMPERSAND)){
    removeLastAmp(v[1]);
    wait=false;
  }
  if(contains(v[0],REDIN)){
    redIn=cutAfter(v[0],REDIN);
  }
  if(contains(v[1],REDOUTA)){
    redOut=cutAfter(v[1],REDOUTA);
    append=true;
  }else if(contains(v[1],REDOUT)){
    redOut=cutAfter(v[1],REDOUT);
    append=false;
  }
  if(containsIllegal(v[0]) || containsIllegal(v[1])){
    printError(SYNTAX_ERROR);
    continue;
  }
  char** args1=generateArgs(split(v[0],SPACE));
  char** args2=generateArgs(split(v[1],SPACE));
  try{
    pipe(args1[0],args1,args2[0],args2,wait,
      (redIn.empty() ? NULL:redIn.c_str()),(redOut.empty() ? NULL:redOut.c_str()),append);
  }catch(int ex){
    printError(ex);
  }

}else{// single command handling
 bool wait=true;
 string redIn;
 string redOut;
 bool append;
 if(endsWith(line,AMPERSAND)){
   removeLastAmp(line);
   wait=false;
 }
 if(contains(line,REDIN)){
   redIn=cutAfter(line,REDIN);
 }
 if(contains(line,REDOUTA)){
   redOut=cutAfter(line,REDOUTA);
   append=true;
 }else if(contains(line,REDOUT)){
   redOut=cutAfter(line,REDOUT);
   append=false;
 }
 if(containsIllegal(line)){
  printError(SYNTAX_ERROR);
  continue;
}
char** args=generateArgs(split(line,SPACE));
try{
  execute(args[0],args,wait,
    (redIn.empty() ? NULL:redIn.c_str()),(redOut.empty() ? NULL:redOut.c_str()),append);
}catch(int ex){
  printError(ex);
}

}

}
}while(true);
}

/*
* Returns the working directory.
*/
char* getDir(){
  static char cCurrentPath[FILENAME_MAX];
  if (!getcwd(cCurrentPath, sizeof(cCurrentPath)))
  {
    return NULL;
  }
  cCurrentPath[sizeof(cCurrentPath) - 1] = '\0';
  return cCurrentPath;

}

/*
* Changes the working directory.
* Throws errno.
*/
void changeDir(const char* newDir){
  if(chdir(newDir)==-1){
    throw errno;
  }
}

/*
* 
* Executes the command using the arguments defined in the second parameter.
* If the command parameter does not contain "/" the executable is searched in PATH.
* If waitExecution is true the function waits for the process to be finished.
* Throws errno in case of failed forking,failed redirection when waitExecution is true 
* and failed background handling and initiation of the new process.
* redIn,redOut should be NULL if no redirection is required,append should be true 
* if output needs to be redirected to a file,appened at the end.
* 
*/
void execute(const char *command, char *const args[],bool waitExecution,
  const char *redIn,const char* redOut,bool append){
  
  pid_t pidChild;
  pidChild = fork();
if (pidChild== 0) {//executed in child
  // printf("child : %d\n", getpid());
  int in=-1,out=-1;
  if(redIn!=NULL){
    try{
      in=startRedirectIn(redIn);
    }catch(int ex){
      if(waitExecution){
        throw ex;
      }else{
        exit(ex);
      }
    }

  }
  if(redOut!=NULL){

   try{
    out=startRedirectOut(redOut,append);
  }catch(int ex){
    if(waitExecution){
      throw ex;
    }else{
      exit(ex);
    }
  }
}

stopRedirect(in);
stopRedirect(out);
execvp(command,args);
exit(1);


}else if (pidChild < 0) {//fork failed
  throw errno;
}
else{// executed in parent
  if(waitExecution){
   int childExitStatus;
   if(wait(&childExitStatus)!=-1){
    if(WIFEXITED(childExitStatus)){
      int exitStatus=WEXITSTATUS(childExitStatus);
      if(exitStatus!=0){
        throw exitStatus;
      }
    }else{
      throw errno;
    }
  }
}else{
  if(kill(pidChild,0)==0){
    if(kill(pidChild,SIGSTOP)==0){
      backgroundProcs.push_back(pidChild);
    }else{
      throw errno;
    }
  }else{
    throw errno;
  }


}

}
}

/*
* 
* Executes the first command and redirects it's outpun to teh second.

* False waitExecution causes the the first command to be executed at the time
* of the call and the second when the background handler allows it.
*
* redIn refers to the first command and redOut to the second.
*
* If the command parameters do not contain "/" the executable is searched in PATH.
*
* Throws errno in case of failed pipelining,failed forking,failed redirection when
* waitExecution is true and failed background handling and initiation of the new process.
* redIn,redOut should be NULL if no redirection is required,append should be true 
* if output needs to be redirected to a file,appened at the end.
* 
*/
void pipe(const char *command1, char *const args1[],const char *command2, char *const args2[],
  bool waitExecution,const char *redIn,const char* redOut,bool append){
  
  int pipeDesc[2];
  bool error=false;
  if(pipe(pipeDesc)<0){
    throw errno;
  }
  pid_t pid1,pid2;
  int in=-1,out=-1;
  if((pid1=fork())==0){//first command handling
    dup2(pipeDesc[1],1);
    close(pipeDesc[0]);
    close(pipeDesc[1]);
    if(redIn!=NULL){
      try{
        in=startRedirectIn(redIn);
      }catch(int ex){
        exit(ex);
      }
    }
    stopRedirect(in);
    execvp(command1,args1);
    exit(1);
  }
  if(pid1==-1){//fork error
    throw errno;
  }
  if((pid2=fork())==0){//second command handling
    dup2(pipeDesc[0],0);
    close(pipeDesc[0]);
    close(pipeDesc[1]);
    if(redOut!=NULL){
      try{
        in=startRedirectOut(redOut,append);
      }catch(int ex){
        exit(ex);
      }
    }
    stopRedirect(out);
    execvp(command2,args2);
    exit(1);
  }
  if(pid2==-1){//fork error
    throw errno;
  }
  close(pipeDesc[0]);
  close(pipeDesc[1]);

  if(waitExecution){
    int status;
    pid_t ws=wait(&status);
    if(ws!=-1){//first command is executed
      if(WIFEXITED(status)){
        int exitStatus=WEXITSTATUS(status);
        if(exitStatus!=0){
          kill(pid2,SIGKILL);
          throw exitStatus;
        }
      }
    }else{// error while executing the first command
      throw errno;
    }
    ws=wait(&status);
    if(ws!=-1){//second command is executed
      if(WIFEXITED(status)){
        int exitStatus=WEXITSTATUS(status);
        if(exitStatus!=0){
          throw exitStatus;
        }
      }
    }else{// error while executing the second command
      throw errno;
    }
  }else{// waitExecution is false
    int status;
    pid_t ws=wait(&status);
    bool pid1Finished=true;
    if(ws!=-1){//first command is executed
      if(WIFEXITED(status)){
        int exitStatus=WEXITSTATUS(status);
        if(exitStatus!=0){
          kill(pid2,SIGKILL);
          throw exitStatus;
        }
      }
    }else{// error while executing the first command
      throw errno;
    }
    if(kill(pid2,SIGSTOP)==0){
      backgroundProcs.push_back(pid2);
    }else{
      throw errno;
    }
  }


}

/**
*
* Opens the file and duplicates the file descriptor.
* Throws errno if either of those operations failed.
* Retuns the file descriptor.
*
*/
int startRedirectIn(const char *file){
  int in;
  in=open(file,O_RDONLY);
  if(in<0){
    throw errno;
  }
  int dup=dup2(in,0);
  if(dup<0){
    close(in);
    throw errno;
  }
  return in;
}

/**
*
* Opens/creates the file and duplicates the file descriptor.
* Throws errno if either of those operations failed.
* Retuns the file descriptor.
*
*/
int startRedirectOut(const char *file,bool append){
  int out;
  if(append){
    out=open(file,O_WRONLY | O_CREAT | O_APPEND);
  }else{
    out=open(file,O_WRONLY | O_CREAT);
  }
  if(out<0){
    throw errno;
  }
  int dup=dup2(out,1);
  if(dup<0){
    close(out);
    throw errno;
  }
  return out;
}

/**
*
* Closes the file descriptor.
*
*/
void stopRedirect(int rdr){
  if(rdr!=-1){
    close(rdr);
  }
}

/**
*
* Checks if the proces with pid is finished.
* If the proces finished exited with a status other than 0 the status is
* saved at exitError.
*
*/
bool isFinished(pid_t pid,int &exitError){
  int status;
  pid_t w;
  w=waitpid(pid,&status,WNOHANG|WUNTRACED);
  if(w==-1){
    throw errno;
  }
  if(w==0){
    return false;
  }
  if(WIFSTOPPED(status)){
    return false;
  }
  if(WIFCONTINUED(status)){
    return false;
  }
  if(WIFEXITED(status)){
    int exitStatus=WEXITSTATUS(status);
    if(exitStatus!=0){
      exitError=exitStatus;
    }
  }
  return true;

}

/**
*
* Handles the background execution,based on the Round Robin algorithm.
* Returns immediately if the previous call to the function was not finished.
*
*/
void backgroundHandler(){
  static bool locked=false;
  if(locked){
    return;
  }

  locked=true;
  static int running=-1;
  if(backgroundProcs.size()==0){//no processes
    locked=false;
    return;
  }
  int killStatus;
  // Pauses the current process and checks if it is finished
  if(running!=-1 ){
    pid_t pid=backgroundProcs.at(running);
    killStatus=kill(pid,SIGSTOP );
    try{
     int exitError=-1;
     if(isFinished(pid,exitError)){
      if(exitError!=-1){
        printError(exitError);
      }else{
        printPidFinished(pid);
      }
      backgroundProcs.erase(backgroundProcs.begin()+running);
      if(backgroundProcs.size()==0){
        running=-1;
        locked=false;
        return;
      }
    }
  }catch(int ex){
    // Unexpected exception caused on runtime
    // Probably rare since calls on the try block don't usually throw exceptions
    // and backgroundProcs is only accessed by one instance due to the synchronization
    // of the function with the locked variable.
    // No action is required
  }
}
// Picks the next process,loop ends when a process is successfully picked
// or all the processes in the vector are actually finished.
// There is one loop unless there are finished processes in the vector.
do{
  if(running>=backgroundProcs.size()-1){
    running=0;
  }else{
    ++running;
  }
  pid_t pid=backgroundProcs.at(running);
  if(kill(pid,0)==0){
    kill(backgroundProcs.at(running),SIGCONT);
    break;
  }else{
    backgroundProcs.erase(backgroundProcs.begin()+running);
    if(backgroundProcs.size()==0){
      running=-1;
      locked=false;
      return;
    }
  }
}while(true);
locked=false;
}

/**
*
* Checks if the two strings are equal.
*
*/
bool equals(string string1,string string2){
  return string1.compare(string2)==0;
}

/**
*
* Checks if the first string starts with the second.
*
*/
bool startsWith(string that,string with){
  return that.compare(0,with.length(),with)==0;
}

/**
*
* Checks if the first string ends with the second.
* Ignores spaces and tabs in the end of the first string.
*
*/
bool endsWith(string that,string with){
  size_t end = that.find_last_not_of(" \t");
  if(string::npos != end){
    that=that.substr(0,end+1);
  }
  if(that.length()>=with.length()){
    return( that.compare(that.length()-with.length(),with.length(),with)==0);
  }
  return false;
}

/**
*
* Checks if the first string contains the second.
*
*/
bool contains(string string1,string string2){
  return !(string1.find(string2)==string::npos);
}

/**
*
* Removes the rightmost ampersand of the string and trims the end.
* Does not check if the removed ampersand is in the end of the string.
*
*/
void removeLastAmp(string &from){
  size_t pos= from.find_last_of("&");
  if(pos==string::npos){
    return;
  }
  from.erase(pos,1);
  trimEnd(from);
}

/**
*
* Removes spaces and tabs from the beginning and the end of the string.
*
*/
void trim(string &that){
  size_t start = that.find_first_not_of(" \t");
  if(string::npos != start){
    that=that.substr(start);
  }
/*start = that.find_first_not_of(" ");
if(string::npos != start){
  that=that.substr(start);
}*/
  trimEnd(that);
}

/**
*
* Removes spaces and tabs from the end of the string.
*
*/
void trimEnd(string &that){
  size_t end = that.find_last_not_of(" \t");
  if(string::npos != end){
    that=that.substr(0,end+1);
  }
/*end = that.find_last_not_of(" ");
if(string::npos != end){
  that=that.substr(0,end+1);
}*/
}

/**
*
* Splits the first string into substrings based on the 
* existance of the second argument.The substrings are 
* trimed and don't include the delimiter.
*
*/
vector<string> split(string that,string with){
  vector<string> strings;
  string::size_type i = 0;
  string::size_type j = that.find(with);
  string::size_type oldj=0;
  string temp;
  if(j!=string::npos){
    do{
      temp=that.substr(i,j-oldj);
      trim(temp);
    // if(!equals(temp,with)){
      strings.push_back(temp);
    // }
  // i=j+1;
      i=that.find_first_not_of(with,j+1);
      if(i==string::npos){
        break;
      }
  // oldj=j;
      oldj=i-1;
      j=that.find(with,i);
    }while(j!=string::npos);
    if(i!=string::npos){
      temp=that.substr(i);
      trim(temp);
    // if(!equals(temp,with)){
     // strings.push_back(temp);
    // }
      strings.push_back(temp);
    }

  }else{
    strings.push_back(that);
  }
  return strings;
}

/**
*
* Creates the array of arguments as it is defined in the execvp documentation.
*
*/
char** generateArgs(vector<string> v){
  char** args;
  args=(char**)malloc(sizeof(char*)*(v.size()+1));
  for(int i=0;i<v.size();i++){
    args[i]=(char*)malloc(sizeof(char)*v[i].length());
    strcpy(args[i],v[i].c_str());
  }
  args[v.size()]=(char*)malloc(sizeof(char));
  args[v.size()]='\0';
  return args;
}

/**
*
* Cuts and returns the substring,found in the first argument,
* after the second argument.The first argument is modified.
* Returns EMPTYSTR if the delimiter can't be found.
*
*/
string cutAfter(string &from,string flag){
  string::size_type pos = from.find(flag);
  if(pos==string::npos){
    return EMPTYSTR;
  }
  if(pos+flag.length()>=from.length()){
    from.erase(pos,flag.length());
    trim(from);
    return EMPTYSTR;
  }
  string::size_type substrStart,substrEnd;

  if(from.at(pos+flag.length())==' '){
    string::size_type pos1,pos2;
    pos1=from.find_first_not_of(" ",pos+1);
    pos2=from.find_first_not_of("\t",pos+1);
    if(pos1<pos2){
      substrStart=pos1;
    }else{
      substrStart=pos2;
    }
  }else{
    substrStart=pos+flag.length();
  }
  if(substrStart==string::npos){
    from.erase(pos,flag.length());
    trim(from);
    return EMPTYSTR;
  }

  substrEnd=from.find(" ",substrStart);
  if(substrEnd==string::npos){
    substrEnd=from.find("\t",substrStart);
  }
  if(substrEnd==string::npos){
    substrEnd=from.length();
  }

  string cut=from.substr(substrStart,substrEnd-substrStart);
  trim(cut);
  from.erase(pos,substrEnd-pos+1);
  trim(from);
  return cut;
}

/**
*
* Checks if the string contains either one of &,<,>.
*
*/
bool containsIllegal(string str){
  if(contains(str,AMPERSAND) || contains(str,REDIN) || contains(str,REDOUT)){
    return true;
  }else{
    return false;
  }
}

/**
*
* Prints the current working directory.
*
*/
void printDirectory(){
  cout<<getDir()<<":";
}

/**
*
* Prints the error defined by the argument,taking for granted it is errno.
*
*/
void printError(int ex){
  // printf("%s\n",strerror(ex));
  cerr<<strerror(ex)<<endl;
}

/**
*
* Prints an error message as it is defined by the argument.
*
*/
void printError(char* message){
  // printf("%s\n",message);
  cerr<<message<<endl;
}

/**
*
* Prints a message that the process with the specified pid is finished.
*
*/
void printPidFinished(pid_t pid){
  // printf("\nPid %d completed \n",pid);
  cout<<endl<<"Pid "<<pid<<" completed"<<endl;
}