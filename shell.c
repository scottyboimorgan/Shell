/* This is the only file you should update and submit. */

/* Fill in your Name and GNumber in the following two comment fields
 * Name: Scott Morgan
 * GNumber: G01050795
 */

#include "logging.h"
#include "shell.h"
/**
 * This is the struct we will be using for all jobs in the shell
 */
typedef struct Command_struct{
    Cmd_aux aux;//built in struct for each command
    char* entireCmd;//the entire command line
    char* name;//essentially argv[0] for easier accessing
    char* argv[MAXARGS];//the array of arguments
    char* jobStatus;//running or stopped
    int countArgv;//the number of arguments
    int pid;//the process ID
    int isFilled;//whether the array has a job here or not
}Command;

/* Constants */
static const char *shell_path[] = {"./", "/usr/bin/", NULL};
static const char *built_ins[] = 
			{"quit", "help", "kill", "jobs", "fg", "bg", NULL};

/**
 * Globals:
 */
static int QUIT = 1;//used to determine when the shell should end
static int FG = 0;//used to determine if there is an active foreground process or not
//when it is equal to 1, the user will not be able to enter a new command, but will be able
//to send signals using ctrl c and ctrl z
static Command COMMANDS[100];//job control array, will be dynamically deleted from and added to
//allows for the index of the job in this array to be its jobID


/**
 * clears a command enough so we know there isnt an active command
 * @param cmd
 */
void resetCommand(Command* cmd){
    strcpy(cmd->jobStatus, "");
    strcpy(cmd->name, "");
    strcpy(cmd->entireCmd, "");
    cmd->pid = -1;
    cmd->isFilled = 0;
}

/**
 * adds the next background job to the list or adds the foreground job to the first spot in the list
 * this function is littered with copy statements
 * @param pid
 * @param aux
 * @param argv
 * @param fullCommand
 * @param countArgv
 * @param isStopped
 * @param isBg
 * @return
 */
int addToJobList(int pid, Cmd_aux* aux, char* argv[], char* fullCommand, int countArgv, int isStopped, int isBg){
    int i = 0;//indexors
    int j = 1;
    if(isBg == 1) {//if it is a bg process, put it in the next available slot
        while (COMMANDS[j].isFilled == 1) {
            j++;
        }
        COMMANDS[j].pid = pid;
        strcpy(COMMANDS[j].aux.in_file, aux->in_file);
        strcpy(COMMANDS[j].aux.out_file, aux->out_file);
        COMMANDS[j].aux.is_append = aux->is_append;
        COMMANDS[j].aux.is_bg = aux->is_bg;
        COMMANDS[j].countArgv = countArgv;

        for (i = 0; i < countArgv; i++) {//copy argv over
            COMMANDS[j].argv[i] = argv[i];
        }
        strcpy(COMMANDS[j].entireCmd, fullCommand);
        strcpy(COMMANDS[j].name, argv[0]);
        if(isStopped == 0) {
            strcpy(COMMANDS[j].jobStatus, "Running");
        }
        else{
            strcpy(COMMANDS[j].jobStatus, "Stopped");
        }
        COMMANDS[j].isFilled = 1;
    }
    else if(isBg == 0){//if it is a fg process, put it in the first element of the array
        COMMANDS[0].pid = pid;
        strcpy(COMMANDS[0].aux.in_file, aux->in_file);
        strcpy(COMMANDS[0].aux.out_file, aux->out_file);
        COMMANDS[0].aux.is_append = aux->is_append;
        COMMANDS[0].aux.is_bg = aux->is_bg;
        COMMANDS[0].countArgv = countArgv;

        for(i = 0; i < countArgv; i++) {
            COMMANDS[0].argv[i] = argv[i];
        }
        strcpy(COMMANDS[0].entireCmd, fullCommand);
        strcpy(COMMANDS[0].name, argv[0]);
        if(isStopped == 0) {
            strcpy(COMMANDS[0].jobStatus, "Running");
        }
        else{
            strcpy(COMMANDS[0].jobStatus, "Stopped");
        }
        COMMANDS[0].isFilled = 1;
        j = 0;
    }
    return j;
}

/**
 * used for handling ctrl c inputs in the shell program
 * @param a
 */
void ctrlC_handler(int a){
    log_ctrl_c();
    if(FG == 0){//no fg process, dont do anything
        return;
    }
    kill(COMMANDS[0].pid, SIGINT);
}

/**
 * used for handling ctrl z inputs in the shell program
 * @param a
 */
void ctrlZ_handler(int a){
    log_ctrl_z();
    if(FG == 0){//no fg process, dont do anything
        return;
    }
    kill(COMMANDS[0].pid, SIGTSTP);
}

/**
 * deletes a job from the job list and moves the other jobs down an id
 * @param jobID
 */
void deleteFromJobList(int jobID) {
    int deleted = 0;//whether we have deleted the index at jobID yet
    for(int i = 1; i < 100; i++){//COMMANDS[0] is a fg process,so we don't need to check that
        if(jobID == i){
            deleted = 1;
        }
        if(deleted == 1){
            if(COMMANDS[i+1].isFilled == 1) {
                COMMANDS[i] = COMMANDS[i + 1];
            }
            else{
                COMMANDS[i].isFilled = 0;
                resetCommand(&COMMANDS[i]);
                break;
            }
        }
    }
}
/**
 * changes job status to stopped
 * @param cmd
 */
void changeToStopped(Command* cmd){
    strcpy(cmd->jobStatus, "Stopped");
}
/**
 * changes job status to running
 * @param cmd
 */
void changeToRunning(Command* cmd){
    strcpy(cmd->jobStatus, "Running");
}
/**
 * handles all SIGCHLD events
 * @param a
 */
void child_handler(int a) {
    pid_t pid;//the current process ID
    int status;//the status after waiting

    int i = 0;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        if (pid == COMMANDS[0].pid) {//FG PROCESS
            FG = 0;//were done with this process in the FG, so change back

            //log statements depending on different Macros
            if (WIFEXITED(status) == 1) {
                log_job_fg_term(COMMANDS[0].pid, COMMANDS[0].entireCmd);
                break;
            }
            else if(WIFSTOPPED(status) == 1){
                log_job_fg_stopped(COMMANDS[0].pid, COMMANDS[0].entireCmd);
                addToJobList(COMMANDS[0].pid, &COMMANDS[0].aux, COMMANDS[0].argv,
                             COMMANDS[0].entireCmd, COMMANDS[0].countArgv, 1, 1); //need to make it a bg job now
                resetCommand(&COMMANDS[0]);
                break;
            }
            else if (WIFSIGNALED(status) == 1) {
                log_job_fg_term_sig(COMMANDS[0].pid, COMMANDS[0].entireCmd);
                break;
            }

        }
        for (i = 1; i < 100; i++) {
            //log statements depending n different macros
            if (pid == COMMANDS[i].pid) {//some background job
                if (WIFEXITED(status) == 1) {
                    deleteFromJobList(i);//remove it from the background job list
                    log_job_bg_term(pid, COMMANDS[i].entireCmd);
                    break;
                } else if (WTERMSIG(status) == 1) {
                    log_job_bg_term_sig(COMMANDS[i].pid, COMMANDS[i].entireCmd);
                    break;
                }
                else if(WIFSTOPPED(status) == 1){
                    changeToStopped(&COMMANDS[i]);//change the status to stopped
                    log_job_bg_stopped(COMMANDS[i].pid, COMMANDS[i].entireCmd);
                    break;
                }
                else if(WIFCONTINUED(status) == 1){
                    changeToRunning(&COMMANDS[i]);//change the status to running
                    log_job_bg_cont(COMMANDS[i].pid, COMMANDS[i].entireCmd);
                    break;
                }
            }
        }
    }

}
/**
 *Built in command for jobs
 * logs all the job details
 * @return
 */
int jobs(){
    int j = 1;
    int num_jobs = 0;
    while(COMMANDS[j].isFilled != 0){
        num_jobs++;
        j++;
    }
    log_job_number(num_jobs);
    j = 1;
    while(COMMANDS[j].isFilled != 0){//log each filled job
        log_job_details(j, COMMANDS[j].pid, COMMANDS[j].jobStatus, COMMANDS[j].entireCmd);
        j++;
    }
    return 1;
}


/* main */
/* The entry of your shell program */
int main() 
{
    char cmdline[MAXLINE]; /* Command line */

    /* Intial Prompt and Welcome */
    log_prompt();
    log_help();

    //Cmd_aux aux;

    struct sigaction ctrlC; /* sigaction structures */
    struct sigaction ctrlZ;
    struct sigaction child;
    memset(&ctrlC, 0, sizeof(struct sigaction));
    memset(&ctrlZ, 0, sizeof(struct sigaction));
    memset(&child, 0, sizeof(struct sigaction));
    ctrlC.sa_handler = ctrlC_handler; /* set the handler */
    ctrlZ.sa_handler = ctrlZ_handler; /* set the handler */
    child.sa_handler = child_handler; /* set the handler */
    sigaction(SIGINT, &ctrlC, NULL); /* register the handler for SIGINT */
    sigaction(SIGTSTP, &ctrlZ, NULL); /* register the handler for SIGTSTP*/
    sigaction(SIGCHLD, &child, NULL); /* register the handler for SIGCHLD */

    int i = 0;
    for(i = 0; i < 100; i++){//initialize all the spots in our dynamic array, these will be freed later
        COMMANDS[i].name = (char*)malloc(sizeof(char)*100);
        COMMANDS[i].entireCmd = (char*)malloc(sizeof(char)*100);
        COMMANDS[i].jobStatus = (char*)malloc(sizeof(char)*100);
        strcpy(COMMANDS[i].jobStatus, "");
        strcpy(COMMANDS[i].name, "");
        strcpy(COMMANDS[i].entireCmd, "");
        COMMANDS[i].aux.in_file = (char*)malloc(sizeof(char)*100);
        COMMANDS[i].aux.out_file = (char*)malloc(sizeof(char)*100);
        COMMANDS[i].pid = -1;
        COMMANDS[i].isFilled = 0;
    }

    Cmd_aux aux;
    char* argv[MAXARGS];
    /* Shell looping here to accept user command and execute */
    while (QUIT == 1) {//stay in the shell until the quit command is given
        /* Read a line */
        // note: fgets will keep the ending '\n'
        if(FG == 0) {//only get the next input if no fg processes are running
            log_prompt();/* Print prompt */
            if (fgets(cmdline, MAXLINE, stdin) == NULL) {
                if (errno == EINTR)
                    continue;
                exit(-1);
            }
            if (feof(stdin)) {
                exit(0);
            }
            /* Parse command line */
            cmdline[strlen(cmdline) - 1] = '\0';  /* remove trailing '\n' */
            parse(cmdline, argv, &aux);
        }
    }
	//free everything back up
    for(i = 0; i < 100; i++){
        free(COMMANDS[i].name);
        free(COMMANDS[i].entireCmd);
        free(COMMANDS[i].jobStatus);
        free(COMMANDS[i].aux.in_file);
        free(COMMANDS[i].aux.out_file);
    }

	exit(0);
}
/* end main */


/**
 * checks and sees if a given string is in the built in commands array
 * @param command
 * @return null if it is not, the name of the command if it is
 */
char* isBuiltInCmd(char* command){
    int i = 0;
    char* ans = NULL;
    while(built_ins[i] != NULL){//loop thru the built in commands and return the name of the command
        //if it is built in
        if (strcmp(command, built_ins[i]) == 0) {
            ans = command;
            break;
        }
        i++;
    }

    return ans;
}

/**
 * Runs a new, non built in process based on the passed in Command
 * @param argv
 * @param cmd
 * @return
 */
int runNewProcess(char* argv[], Command* cmd){
    char* path = malloc(100);//our path that we will check
    char* file1 = malloc(100);
    char* file2 = malloc(100);
    strcpy(path, shell_path[0]);
    strcpy(file1, path);
    strcpy(file2, path);
   // int fileFinder = -1;//0 for in file, 1 for outfile, 2 for append

    int fd = 0;

    strcat(path, argv[0]);


    int pid = fork();
    cmd->pid = pid;//the updates our job list
    if(pid != 0 && cmd->aux.is_bg == 0) {//fg process
        log_start_fg(pid, cmd->entireCmd);
        FG = 1;
    }
    else if(pid != 0 && cmd->aux.is_bg == 1){//bg process
        log_start_bg(pid, cmd->entireCmd);
    }

    if(cmd->pid == 0){//if were in the child process, we need to run the specified program
        setpgid(0,0);

        if(strcmp("\0", cmd->aux.in_file) != 0){//this opens an in_file
            strcat(file1, cmd->aux.in_file);
            fd = open(file1, O_RDONLY, 0600);
            if(fd > 0){
                dup2(fd, STDIN_FILENO);//redirect input from stdin to the file
                close(fd);
            }
            else{
                log_file_open_error(cmd->aux.in_file);//log an error if the file cant be opened
                exit(0);
            }
        }
        if(cmd->aux.is_append == 0){//this is for standard out files(>)
            strcat(file2, cmd->aux.out_file);
            fd = open(file2, O_WRONLY | O_TRUNC | O_CREAT, 0600);
            if(fd > 0){
                dup2(fd, STDOUT_FILENO);//redirect the stream to the file
                close(fd);
            }
            else{
                log_file_open_error(cmd->aux.in_file);
                exit(0);
            }
        }
        if(cmd->aux.is_append == 1){//this is for append files (>>)
            strcat(file2, cmd->aux.out_file);
            fd = open(file2, O_WRONLY | O_APPEND | O_CREAT, 0600);
            if(fd > 0){
                dup2(fd, STDOUT_FILENO);//redirect the stream to the file
                close(fd);
            }
            else{
                log_file_open_error(cmd->aux.in_file);
                exit(0);
            }
        }
        int e = execv(path, argv);
        if(e == -1){//if first path doesnt work, try the other path
            strcpy(path, shell_path[1]);
            strcat(path, argv[0]);
            if(execv(path, argv) == -1){//if neither path works, log an error and
                //end this process
                log_command_error(cmd->entireCmd);
                exit(pid);
            }
        }
    }
    else{//need to send a sigchild so we can wait
        kill(-1, SIGCHLD);
    }
    free(file1);
    free(file2);
    free(path);
    return 1;
}
/**
 * Moves a BG process specified by jobID to the fg and continues it
 * @param jobID
 * @return
 */
int fg(int jobID){
    if(COMMANDS[jobID].isFilled == 0){
        log_jobid_error(jobID);
    }
    else{
        log_job_fg(COMMANDS[jobID].pid, COMMANDS[jobID].entireCmd);
        COMMANDS[jobID].aux.is_bg = 0;//no longer a bg process
        changeToRunning(&COMMANDS[jobID]);//change it to running if if was not already
        COMMANDS[0] = COMMANDS[jobID];//move it to id 0 and remove it from the joblist
        deleteFromJobList(jobID);
        kill(COMMANDS[0].pid, SIGCONT);//continue
        FG = 1;
    }
    return 1;
}

/**
 * continues a bg process
 * @param jobID
 * @return
 */
int bg(int jobID){
    if(COMMANDS[jobID].isFilled == 0){//this jobID doesnt exist
        log_jobid_error(jobID);
    }
    else {
        log_job_bg(COMMANDS[jobID].pid, COMMANDS[jobID].entireCmd);
        changeToRunning(&COMMANDS[jobID]);
        kill(COMMANDS[0].pid, SIGCONT);//continue
    }
    return 1;
}

/**
 * uses argv and aux to do what the command line has instructed us to do
 * @param argv
 * @param aux
 * @return
 */
int useCLI(char* argv[], Command* cmd){
    char* builtIn;
    builtIn = isBuiltInCmd(argv[0]);
    if(builtIn != NULL){
        //need to add the other built in commands but we havent gotten there just yet
        if(strcmp(builtIn, "quit") == 0){
            log_quit();
            QUIT = 0;//this will end the loop in main
        }
        else if(strcmp(builtIn, "help") == 0){
            log_help();
        }
        else if(strcmp(builtIn, "jobs") == 0){
            jobs();
        }
        else if(strcmp(builtIn, "kill") == 0){
            int x = -1;
            int y = -1;
            sscanf(argv[1], "%d", &x);
            sscanf(argv[2], "%d", &y);//just simply call kill, the required input is backwards from the kill function
            log_kill(y, x);
            kill(y, x);
        }
        else if(strcmp(builtIn, "fg") == 0){
            int jobID = 0;
            sscanf(argv[1], "%d", &jobID);
            fg(jobID);//run fg method
        }
        else if(strcmp(builtIn, "bg") == 0){
            int jobID = 0;
            sscanf(argv[1], "%d", &jobID);
            bg(jobID);//run bg method
        }
    }
    else if(argv[0] != NULL){
        runNewProcess(argv, cmd);//if we have an argument and it is not built in, run a new process
    }
    return 1;
}


/* required function as your staring point; 
 * check shell.h for details
 */

void parse(char *cmd_line, char *argv[], Cmd_aux *aux){
    char* token = strtok(cmd_line, " ");//parsing the cmd line
    if(token == NULL){
        return;
    }
    int countArgv = 0;
    //the 'ForNull' variables are for determining if the garbage values for the aux struct
    //need to be nullified or not because their counterpart variables are reset prior to that check
    int inFile = 0;
    int inFileForNull = 0;
    int outFile = 0;
    int outFileForNull = 0;
    int isAppend = 0;
    int isAppendForNull = 0;
    char* fullCommand = (char*)malloc(sizeof(char)*MAXLINE);
    strcpy(fullCommand, "");
    int stop = 0;//used to catching in and out files

    //initialize the aux cmd struct
    aux->in_file = (char *)malloc(sizeof(char)* MAXLINE);
    aux->out_file = (char *)malloc(sizeof(char) * MAXLINE);
    aux->is_bg = 0;
    aux->is_append = -1;

    while(token != NULL){//loop until the end
        stop = 0;
        if(countArgv > 0) {
            strcat(fullCommand, " ");//for copying the full command
        }
        strcat(fullCommand, token);
        if(strcmp(token, ">") == 0){//outfile detected
            outFile = 1;
            outFileForNull = 1;
            stop = 1;
        }
        else if(strcmp(token, "<") == 0){//infile detected
            inFile = 1;
            inFileForNull = 1;
            stop = 1;
        }
        else if(strcmp(token, ">>") == 0){//append file detected
            isAppend = 1;
            isAppendForNull = 1;
            stop = 1;
        }
        if(strcmp(token, "&") == 0){//this has to be the end of the command, break the loop and set bg
            aux->is_bg = 1;
            argv[countArgv] = NULL;
            break;
        }
//the purpose of the stop variable allows us to make sure that we dont enter these if statements
//on the same iteration as the ">" command etc
        if(outFile == 1 && stop == 0){//copies the next argument to the outfile
            strcpy(aux->out_file, token);
            aux->is_append = 0;
            outFile = 0;
        }
        else if(inFile == 1 && stop == 0){//copies the next argument to the infile
            strcpy(aux->in_file, token);
            inFile = 0;
        }
        else if(isAppend == 1 && stop == 0){//copies the next argument to an appendFile
            strcpy(aux->out_file, token);
            aux->is_append = 1;
            isAppend = 0;
        }
        else if(stop == 0){
            argv[countArgv] = token;
        }

        token = strtok(NULL, " ");//gets the next value

        if(token == NULL){//the end of the commands
            countArgv++;
            argv[countArgv] = NULL;
        }
        if(stop == 0) {
            countArgv++;
        }
    }
    //gets rid of the garbage values
    if(isAppendForNull == 0 && outFileForNull == 0){
        strcpy(aux->out_file, "\0");
    }
    if(inFileForNull == 0){
        strcpy(aux->in_file, "\0");
    }
    int x = -1;
    if(aux->is_bg == 0){
        x = addToJobList(-1, aux, argv, fullCommand, countArgv, 0, 0);//have to add it to the joblist
    }
    else{
        x = addToJobList(-1, aux, argv, fullCommand, countArgv, 0, 1);
    }

    useCLI(COMMANDS[x].argv, &COMMANDS[x]);//call a method to use the CLI
//free allocated memory
    free(aux->in_file);
    free(aux->out_file);
    free(fullCommand);
}






