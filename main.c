#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

//Setting my globals
int maxData = 2048;
pid_t childPid;
int childStatus = 0;
int allowedBackgroudProc = 1;

//Struct for arguments
struct clArg{

    int realArgs;   //Amount of args that is not < or > or &

    char* inputF;   //Input file
    char* outputF;  //Output file

    char* clArgs[512];  //Array to store all of the args
    char clRaw[2048];   //Raw input from the command line
    int backgroundProcesses[100];   //Array to store bg procs
    int numBgProc;  //Number of background procs
    int numArgs;    //Number of arguments

    int isBackground;   //Determines if the current proc is a background proc

};

//Initialize all of the values in the struct when created
void initialize_struct(struct clArg* data){

    data->realArgs = 0;
    data->inputF = NULL;
    data->outputF = NULL;
    data->numArgs = 0;
    data->isBackground = 0;
    data->numBgProc = 0;
    childStatus = 0;

    int i;
    for (i = 0; i < 512; i++){
        data->clArgs[i] = NULL;
    }

     for (i = 0; i < maxData; i++){
         data->clRaw[i] = '\0';
     }

     for (i = 0; i < 100; i++){
         data->backgroundProcesses[i] = 0;
     }

}

//Gives the exit status depending on the childStatus and the childPid
void exit_status(){
    if(WIFEXITED(childStatus)){
			printf("PID: %d exited normally with status %d\n", childPid, WEXITSTATUS(childStatus));
		} else{
			printf("PID %d exited abnormally due to signal %d\n", childPid, WTERMSIG(childStatus));
		}
        fflush(stdout);
        handler();
}

//Run the arguments
void run_processes(struct clArg* data){

    int i;
    childPid = fork();  //fork
    pid_t subPid;

    switch(childPid){

        case -1: ;
            perror("Error forking!\n");
            exit(1);
            break;

        //This case (0) was taken from the Processes and I/O module, hence why variable names are the same
        case 0: ;

            subPid = getpid();
            if(data->inputF != NULL){
                int sourceFD = open(data->inputF, O_RDONLY);
                if (sourceFD == -1) { 
		            perror("Error: Couldn't open source\n"); 
		            exit(1); 
	            }
                int result = dup2(sourceFD, 0);
	            if (result == -1) { 
		            perror("source dup2()"); 
		            exit(2); 
	            }
            }

            if(data->outputF != NULL){
                int targetFD = open(data->outputF, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	            if (targetFD == -1) { 
		            perror("target open()"); 
		            exit(1); 
	            }    
                int result = dup2(targetFD, 1);
	            if (result == -1) { 
		            perror("target dup2()"); 
		            exit(2); 
	            }
            }

            for (i = data->realArgs; i < 512; i++){
                data->clArgs[i] = NULL;
            }

       
            if(execvp(data->clArgs[0], data->clArgs)){
                perror("Hull breach\n");
                fflush(stdout);
                
            }

            //Increment the number of background processes
            data->numBgProc += 1;
            data->backgroundProcesses[data->numBgProc] = childPid;  //Add the childPid to the array of bg procs

            break;
    
        default:
            //If the current process is a bg proc and we allow it:
            if(data->isBackground == 1 && allowedBackgroudProc == 1){
                printf("background pid is %d\n", childPid);
                data->numBgProc += 1;   
                data->backgroundProcesses[data->numBgProc] = childPid;
                childPid = waitpid(childPid, &childStatus, WNOHANG);   //Wait for the child, no hang so we can go straight to the command line
                strcpy(data->clArgs[0], "status");  //set the first arg as status so we can call run_args to display the status
                data->clArgs[1] = NULL; //Next arg is null; just need status
                fflush(stdout);
                run_args(data);     //call run args to display status
                handler();          
                exit(0);
                break;
            }

            else{
                data->numBgProc += 1;
                data->backgroundProcesses[data->numBgProc] = childPid;
                childPid = waitpid(childPid, &childStatus, 0);      //Wait for the process to finish, don't allow any more input
                handler();
                fflush(stdout);
		        exit(0);
		        break;
            }

    }
    
}

//This function simply calls run_args() again
void reload_run_args(struct clArg* data){
    run_args(data);
}

//This function checks for input such as "status" "cd" and "exit": handles them accordingly
void run_args(struct clArg* data){

    int i, j, k;
    int pid;
    char buffer[12];
    char* saved;

    //If the command line was empty, recall the handler to get new input
    if (data->clArgs[0] == NULL){
        handler();
    }

    //If arg[0] was status, print the status
    if (strcmp(data->clArgs[0], "status") == 0){
 
        if(WIFEXITED(childStatus)){
			printf("PID: %d exited normally with status %d\n", childPid, WEXITSTATUS(childStatus));
		} else{
			printf("PID %d exited abnormally due to signal %d\n", childPid, WTERMSIG(childStatus));
		}
        fflush(stdout);
        handler();
    }

    //If the second arg is NULL
    if (data->clArgs[1] == NULL){
        for (i = 0; i < strlen(data->clArgs[0]); i++){
            if (data->clArgs[0][i] == '$' && data->clArgs[0][i+1] == '$'){  //Check for two dollar signs (expansion)
                saved = calloc(strlen(data->clArgs[0]) - i, sizeof(char));  //Save the remainder
                for (j = 0; j < strlen(data->clArgs[0]) - i; j++){
                    saved[j] == data->clArgs[0][j + i + 2];
                }
                data->clArgs[0][i] = '\0';
                pid = getpid(); //Get the current pid
                sprintf(buffer,"%d", pid);  //cast the pid to a string (buffer)
                strcat(data->clArgs[0], buffer); //Combine the strings
                strcat(data->clArgs[0], "\0");  //Add null terminator
                if (saved[0] == '$' && saved[1] == '$'){ //if there's still two more $ signs, then recall this function
                    strcpy(data->clArgs[0], saved); //Save the rest
                    fflush(stdout);
                    reload_run_args(data);  //reload this function
                }
                else{   //If not, print what's left and call the handler
                    printf("%s%s\n", data->clArgs[0], saved);   //print the rest
                    fflush(stdout);
                    data->clRaw[0] = '\0'; //make clRaw empty
                    handler();  //call the handler
                }
            }
        }
    }

    
    //Basically the same, just works if there are more arguments behind the $$
    for (i = 0; i < data->numArgs; i++){
        for (j = 0; j < strlen(data->clArgs[i]) + 1; j++){
            if (data->clArgs[i][j] == '$' && data->clArgs[i][j+1] == '$'){ //Check for double $$
                saved = calloc(strlen(data->clArgs[i]) - j, sizeof(char));  //Save the rest
                for (k = 0; k < strlen(data->clArgs[i]) - j; k++){
                    saved[k] = data->clArgs[i][k + j + 2];
                }
                data->clArgs[i][j] = '\0';
                pid = getpid();     //Get the current pid
                sprintf(buffer,"%d", pid);  //store it in buffer
                strcat(data->clArgs[i], buffer); //Combine the strings
                strcat(data->clArgs[i], "\0");  //Add null terminator
                if (saved[0] == '$' && saved[1] == '$'){ //if there's still two more $ signs, then recall this function
                    strcpy(data->clArgs[i], saved); //Save the rest
                    fflush(stdout);
                    reload_run_args(data);  //reload
                }
                else{   //If not, print what's left and call the handler
                    printf("%s", saved); //print the rest
                    fflush(stdout);
                    data->clRaw[0] = '\0';
                    reload_run_args(data);  //reload
                }
            }
        }
    }
    
    //Checks for comments. Calls handler again to get more input
    if (data->clArgs[0][0] == '#'){
        handler();
    }

    //If exit
    if (strcmp(data->clArgs[0], "exit") == 0){
        int i;
        for (i = 0; i < data->numBgProc; i++){      //Kill all the bg procs
            kill(data->backgroundProcesses[i], SIGKILL);
        }
        exit(0);    //exit
    }
    
    //Check for cd
    if (strcmp(data->clArgs[0], "cd") == 0){ 
        if (data->clArgs[1] == NULL){   //if no more args
            chdir(getenv("HOME"));  //go to the home dir
        }

        else{
            if (chdir(data->clArgs[1]) != 0){   //If we cannot find the directory
                printf("%s: directory not found!\n", data->clArgs[1]);
                fflush(stdout);
            }
            else{
                fflush(stdout);
                handler();
            }
        }
        
    }

}


//Handler for SIGTSTP
void handle_SIGSTOP(int signo){
    if (allowedBackgroudProc == 1){
        allowedBackgroudProc = 0;
        char* message = "Entering foreground-only mode (& is now ignored)\n: ";
	    write(STDOUT_FILENO, message, 51);

    }
    else{
        allowedBackgroudProc = 1;
        char* message = "Foreground-only mode disabled. (& is now recognized)\n: ";
	    write(STDOUT_FILENO, message, 55);
    }
}

//This function just reloads the handler
void reload_handler(){
    handler();
}

void handler(){
    //Make out sigactions
    struct sigaction SIGINT_action = {{0}};
    struct sigaction SIGSTOP_action = {{0}};


    SIGINT_action.sa_handler = SIG_IGN;
	sigfillset(&SIGINT_action.sa_mask);
	SIGINT_action.sa_flags = 0;
	sigaction(SIGINT, &SIGINT_action, NULL);

    SIGSTOP_action.sa_handler = handle_SIGSTOP;
	sigfillset(&SIGSTOP_action.sa_mask);
	SIGSTOP_action.sa_flags = SA_RESTART;
	sigaction(SIGTSTP, &SIGSTOP_action, NULL);

    int i;
    int index = 0;
    char* savePtr;
    struct clArg arguments; //Make the new struct
    initialize_struct(&arguments);  //Init it

    printf("\n: ");
    fflush(stdout);
    fgets(arguments.clRaw, maxData, stdin); 

    arguments.clRaw[strcspn(arguments.clRaw, "\r\n")] = '\0'; //replace the "\r\n" lines with a null terminator


    //Tokenize our arguments, separated by spaces
    savePtr = NULL;
    char* token = strtok_r(arguments.clRaw, " ", &savePtr);
    while(token){
        arguments.clArgs[index] = calloc(strlen(token) + 1, sizeof(char));
        strcpy(arguments.clArgs[index], token); //Put this current token into the arg at index
        token = strtok_r(NULL, " ", &savePtr);  //Update token
        index++;
        arguments.numArgs = index; //Save number of args
    }

    //This will set the output file, input file, and turn on isBackground
    for (i = 0; i < arguments.numArgs; i++){
        //Update output file
        if (strcmp(arguments.clArgs[i], ">") == 0){
            arguments.outputF = calloc(strlen(arguments.clArgs[i+1]) + 1, sizeof(char));
            strcpy(arguments.outputF, arguments.clArgs[i+1]);
        }

        //Update input file
        if (strcmp(arguments.clArgs[i], "<") == 0){
            arguments.inputF = calloc(strlen(arguments.clArgs[i+1]) + 1, sizeof(char));
            strcpy(arguments.inputF, arguments.clArgs[i+1]);
        }

        //Turn isBackground on if needed
        if (strcmp(arguments.clArgs[arguments.numArgs-1], "&") == 0){
            arguments.isBackground = 1;
        }
    }

    //Getting the number of real args by subtracting the instances of < > and & and subtracting that from numArgs
    for (i = 0; i < arguments.numArgs; i++){
        if (strcmp(arguments.clArgs[i], "<") == 0 || strcmp(arguments.clArgs[i], ">") == 0  || strcmp(arguments.clArgs[i], "&") == 0){
            break;
        }
    }
    arguments.realArgs = i;

    run_args(&arguments); //run the args if cd exit or status occurs
    run_processes(&arguments);  //run the rest
    
}

int main(){

    //Call the handler
    handler();
    
    return EXIT_SUCCESS;
}