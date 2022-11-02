#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>


//message to be displayed when an error occurs
char error_message[30]="An error has occurred\n";
//path to be used, by default its /bin/ but can be changed with path command
char* path="/bin/";
//store the num of processes, 0 by default
int noProcesses=0;
char* env[50];

int paralFileHandler(int file, char* fileName)
{
    //file is open for read and write O_RDWR
    //create file if it dosent exist O_CREAT
    //trunc file if ti exists adn is regular
    file = open(fileName, O_RDWR|O_CREAT|O_TRUNC, 0600);
    //create copy of file descripter
    dup2(file, 1);
    close(file);
    return file;
}

void parallel(char *argVector[],int argcCount, char* fileName){
    int i = 0;
    int file= 0;
    //loop over num processes
    for(i=0;i<noProcesses;i++){
        //create some space on the heap
        char *fullname = malloc(strlen(env[i])+strlen(argVector[0])+1);
        //set null charcater
        fullname[0] = '\0';
        //copy curr env into fullName
        strcpy(fullname, env[i]);
        //add / incase user dosent
        strcat(fullname, "/");
        //add the argvec
        strcat(fullname, argVector[0]);
        //check access
        if(access(fullname,X_OK)==0){
            //fork a n process
            int pid = fork();
            //if the process id is 0-> child has been created and so perform the function
            if(pid==0){
                //set last elem of args to null
                argVector[argcCount] = NULL;
                //hamdle if there is a file
                if(fileName!=NULL){
                    file=paralFileHandler(file, fileName);
                }
                //excute the command
                execv(fullname, argVector);
                exit(0);
            }
            break;
        }
        else
        {
            if(i==noProcesses-1){
                write(STDERR_FILENO, error_message, strlen(error_message));
            }
        }
    }
}


void redirection(char* currargs[], int argCount)
{
    //cannt have 0 args
    if(argCount==0)
    {
        return;
    }
    
    //init loop counter
    int i=0;
    //init new arg vector
    char* newargv[50];
    
    //loop while i is less than num of args in arg vec and while we are not reading '>' indicating a redirect
    while(i<argCount && strcmp(currargs[i], ">")!=0)
    {
        newargv[i]=currargs[i];
        i++;
    }
    //assign last elem of argv to be null
    newargv[i]=NULL;
    //if we are at end of argv when we exit the while loop, need to now execute -> send to parallel incase there is parallel command
    if(i==argCount)
    {
        parallel(newargv, argCount, NULL);
        return;
    }
    //if we are one before the end or 2 before the end-> error
    else if(i==argCount-1 || i<argCount-2 || i==0)
    {
        write(STDERR_FILENO, error_message, strlen(error_message));
        return;
    }
    else
    {
        int args=argCount-2;
        char* passedArgs=currargs[argCount-1];
        //otherwise if nothng else executes
        parallel(newargv, args, passedArgs);
        return;
    }
}

void exec(char* argVec[], int argCount)
{
    //if no arguments given just return
    if(argCount==0)
    {
        return;
    }
    
    /*check if command is a built in command*/
    
    //if that command is exit
    if(strcmp(argVec[0], "exit")==0)
    {
        //for exit command, if there are more args than just exit, error
        if(argCount!=1)
        {
            write(STDERR_FILENO, error_message, strlen(error_message));
        }
        else
        {
            exit(0);
        }
    }
    //if command is cd
    else if(strcmp(argVec[0], "cd")==0)
    {
        //if we have 2 args-> cd=1 and the new direciry =2
        if(argCount==2)
        {
            //if chdir returns a 0 then error
            if(chdir(argVec[1])!=0)
            {
                write(STDERR_FILENO, error_message, strlen(error_message));
            }
        }
        //if argc!=2 then we have an error
        else
        {
            write(STDERR_FILENO, error_message, strlen(error_message));
            return;
        }
    }
    
    //handle path
    else if(strcmp(argVec[0], "path")==0)
    {
        //get num of processes
        noProcesses=argCount-1;
        //create loop counter
        int j;
        for(j=0; j<argCount-1; j++)
        {
            //overwitre any env variable
            env[j]=argVec[j+1];
        }
    }
    
    //not a built in function
    else
    {
        //check if we have processes>0
        if(noProcesses==0)
        {
            //write error
            write(STDERR_FILENO, error_message, strlen(error_message));
            return;
        }
        
        //init loop counter i
        int i=0;
        //init the number of command
        int numCommand=0;
        while(i<argCount)
        {
            //create array of current args
            char* currAgrs[50];
            //another loop counter
            int j=0;
            //loop while we are less than the num of elems nad none of them are indicating  parallel command
            while(i<argCount && strcmp(argVec[i], "&")!=0)
            {
                //assign our curr arguments vec at index j to be the arg in our arg vec at i
                currAgrs[j]=argVec[i];
                //increment both counters
                i++;
                j++;
            }
            //increment the number of commands
            numCommand++;
            //set currargs to null (erase what the inner while did)
            currAgrs[j]=NULL;
            //handle redirect
            redirection(currAgrs,j);
            i++;
        }
        while(numCommand-- >0)
        {
            wait(NULL);
        }
    }
}

//execute function, takes in input line
void stringHanlde(char* input)
{
    int i = 0;
    //vector of arguments
    char *argVec[256  * sizeof(char)];
    //number of arguments in the line of input
    int argCount = 0;
    
    //handle splitting the input string
    //loop while i is less than the inut strings len and while we are not at the end of the line
    while(i<strlen(input) && input[i]!='\n')
    {
        //skip over where the input is just a space
        if(input[i]!=' ')
        {
            //check for '>' indicating there is a redirect
            if(input[i]=='>')
            {
                //create a temp arg
                char *arg=malloc(256*sizeof(char));
                arg[0]='>';
                //null character value
                arg[1]='\0';
                //place this arg in the arg vector at argc
                argVec[argCount]=arg;
                //increment num of args
                argCount++;
                //increent loop counter
                i++;
                continue;
            }
            
            //check if '&' indicating parallel commands
            if(input[i]=='&')
            {
                //create a temp arg
                char *arg=malloc(256*sizeof(char));
                arg[0]='&';
                //null character value
                arg[1]='\0';
                //place this arg in the arg vector at argc
                argVec[argCount]=arg;
                //increment num of args
                argCount++;
                //increent loop counter
                i++;
                continue;
            }
            
            //declare a new var t, this will be used
            int t = 0;
            //create new arg varibale, will hold arguments
            char *arg = malloc(256  * sizeof(char));
            //loop while less than size of string (access every elem) and whle we not at the emd of the line
            while(i<strlen(input) && input[i]!=' ' && input[i]!='\n')
            {
                //if we have no special char (parallel and redirct)
                if(input[i]!= '&' && input[i]!='>')
                {
                    //set arg[t] to be the word in the input strig at i, from outer loop
                    arg[t] = input[i];
                    //increment t and i
                    i++;
                    t++;
                }
                else
                {
                    break;
                }
            }
            //after we are done adding all the words from the input, add the nul character value
            arg[t] = '\0';
            //add the arg to the arg vector at the pos
            argVec[argCount] =arg;
            //increment the num of args
            argCount++;
        }
        //otherwise increment i if not an empty space
        else
        {
            i++;
        }
        
    }
    exec(argVec, argCount);
}

int main(int MainArgc, char *MainArgv[]){
    

    //check if in batch or interactive mode
    //interactive mode
    if(MainArgc==1)
    {
        //init variables
        env[0]=path;
        noProcesses=1;
        //handle shell in interactv mode
        while(1)
        {
            printf("witsshell> ");
            //get input command
            char* line=NULL;
            size_t len=0;
            ssize_t lineSize=getline(&line, &len, stdin);
            if(lineSize!=-1)
            {
                //call the execute function and pass the line
                stringHanlde(line);
            }
            else
            {
                break;
            }
        }
    }
    //batch mode
    else if(MainArgc==2)
    {
        //init variables
        env[0]=path;
        noProcesses=1;
        //handle batch mode
        const char* fileName=MainArgv[1];
        
        //open file
        FILE *file=fopen(fileName, "r");
        size_t len=0;
        
        if(file==NULL)
        {
            //if no file, produce error and exit
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        }
        //init line
        char* line=NULL;
        while(getline(&line, &len, file)!=-1)
        {
            //get line, then exec command
            stringHanlde(line);
        }
    }
    else
    {
        //init variables
        env[0]=path;
        noProcesses=1;
        //an error has occured
        write(STDERR_FILENO, error_message,strlen(error_message));
        //exit program if error
        exit(1);
    }
    return 0;
}
