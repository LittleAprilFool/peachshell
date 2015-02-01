#include<sys/types.h>
#include<sys/wait.h>
#include<termios.h>
#include<iostream>
#include<cstdio>
#include<unistd.h>
#include<string.h>
#include<stdlib.h>
#include<signal.h>

#define space " 	"
#define line "|"

using namespace std;

pid_t shell_pgid;
struct termios shell_tmodes;
int shell_terminal;
int shell_active;

typedef struct Process{
	struct Process *next;	//next process
	char *arg[100];		//arglist
	pid_t pid;		//process id
	int completed;		//1 if it has completed
}Process;

typedef struct Job{
    struct Job *next;
    pid_t pgid;
    Process *first_process;
    char *name;
    int id;
    int stdin, stdout, stderr;
    int stop;     //0 if job is stopped, 1 if job is running,-1 if job is killed
    int foreground; //0 if job is background, 1 if job is foreground
}Job;


//init first_job
Job * first_job = NULL;

//init current_job
Job * current_job = NULL;


void sig_handler(int signo){
}

void init_shell(){
	
	//get the file info
	shell_terminal = STDIN_FILENO;
	
	//whether shell_terminal refers to the terminal
	shell_active = isatty(shell_terminal);

	if(shell_active){
		
		//set process group
		shell_pgid = getpid();
		setpgid(shell_pgid, shell_pgid);

		//grap control of the terminal
		tcsetpgrp(shell_terminal, shell_pgid);

		//save default terminal attributes for shell
		tcgetattr(shell_terminal, &shell_tmodes);
        
        //welcome
        cout<<"Hello, April's shell is running happily!"<<endl;
	}
    
}

void exit_shell(){
	cout<<"April's shell is closing now. See ya!"<<endl;
	exit(EXIT_SUCCESS);
}

void get_input(){
    //current directory
    char current_direct[1024];
    //current user
    char current_user[1024];
    
    //update current directory
    getcwd(current_direct, sizeof(current_direct));
    
    //update current user
    getlogin_r(current_user,sizeof(current_user));
    
    //input format:username:current_directory$ input
    cout<<current_user<<": "<<current_direct<<"$ ";
    
}


void do_foreground(Job *job, int is_continue){

}

void do_background(Job *job, int is_continue){
    
}

void wait_job(Job *job){
}

void launch_job(Job *j){
    Process *p;
    pid_t pid;
    int ppipe[2],outfile,infile,errfile;
    
    //set initial infile and errfile
    errfile = j->stderr;
    infile = j->stdin;
    
    //do every process in a job
    for(p=j->first_process;p;p=p->next){
        
        //set pipes
        if(p->next){
            //handle error
            if(pipe(ppipe)<0){
                cout<<"create pipe wrong"<<endl;
                //exit(EXIT_FAILURE);
                return;
            }
            else{
                outfile = ppipe[1];
            }
        }
        else outfile = j->stdout;
        
        //fork the child processes
        pid = fork();
        
        //if error occurred
        if(pid < 0){
            cout<<"Fork Failed"<<endl;
            //exit(EXIT_FAILURE);
            return;
        }
        
        //child process
        else if(pid == 0){
            
            //if shell is active, set the terminal to job, set the signal default value
            if(shell_active){
                pid_t get_pid;
                
                //get process id
                get_pid = getpid();
                
                //job id is the first process id
                if(j->pgid == 0) j->pgid = get_pid;
                
                //tell system this process is in this job
                setpgid(get_pid, j->pgid);
                
                //if foreground, set the terminal to the job
                if(j->foreground)
                    tcsetpgrp(shell_terminal,j->pgid);
                
                //set ctrl+c to terminate
                signal(SIGINT, SIG_DFL);
                
                //set ctrl+z to suspend
                signal(SIGTSTP, SIG_DFL);
                
            }
            
            //set i/o channels of new process
            if(infile != STDIN_FILENO){
                dup2(infile, STDIN_FILENO);
                close(infile);
            }
            if(outfile != STDOUT_FILENO){
                dup2(outfile, STDOUT_FILENO);
                close(outfile);
            }
            if(errfile !=STDERR_FILENO){
                dup2(errfile,STDERR_FILENO);
                close(errfile);
            }
            execvp(p->arg[0],p->arg);
            cout<<"execvp wrong"<<endl;
            //exit(EXIT_FAILURE);
            return;
        }
            //parent process, wait until finished
        else{
            wait(NULL);
        }
        
        //close file descriptors if it is not job's std i/o
        if(infile!=j->stdin)
            close(infile);
        if(outfile!=j->stdout)
            close(outfile);
        if(errfile!=j->stderr)
            close(errfile);
        
        //set infile with pipe
        infile = ppipe[0];
    }
    
    //wait for job
    if(!shell_active)
        wait_job(j);
    else if(j->foreground)
        do_foreground(j,0);
    else
        do_background(j,0);
}

void list_jobs(){
    Job *j;
    for(j=first_job;j;j=j->next){
        //if job is not killed, print job
        if(j->stop!=-1){
            cout<<"["<<j->id<<"]   ";
            if(j->stop == 1) cout<<"Stopped                 ";
            else cout<<"Running                 ";
            cout<<j->name;
            //if job is background, print &
            if(j->foreground!=1) cout<<" &";
            cout<<endl;
        }
    }
}

void bg_jobs(char *saveptr){

}

void fg_jobs(char *saveptr){

}

void change_directory(char* saveptr){
    char *str2;
    str2 = strtok_r(NULL, space, &saveptr);
    // special deal with "~"
    if(strcmp(str2,"~")==0)
        str2 = getenv("HOME");
    
    //ret=0 means change the directory successfully
    int ret = chdir(str2);
    //ret!=0 means no such file or directory
    if(ret != 0)
        cout<<"April's shell: cd: "<<str2<<": No such file or directory"<<endl;

}

void kill_jobs(char* saveptr){

}


void create_job(string raw){
    
    //convert string to c string
    char *input = new char[raw.length()+1];
    strcpy(input, raw.c_str());
    
    //init process chain
    Process *first_process = NULL;
    Process *current_process = NULL;
    
    //create a job
    Job *j = new Job;
    j->next = NULL;
    j->first_process = first_process;
    j->foreground = 1;
    
    //parsing the string by |
    char *str1,*token,*subtoken,*saveptr;
    
    for(str1 = input; ; str1 = NULL){
        token = strtok_r(str1, line, &saveptr);
        if(token==NULL) break;
        if(strcmp(token,"&")==0){
             j->foreground=0;
            break;
        }
        
        
        //create process
        Process *p = new Process;
        p->next = NULL;
        
        int i=0;
        char *str2, *saveptr2;
        for(str2 = token;;str2=NULL){
            subtoken = strtok_r(str2, space, &saveptr2);
            if(subtoken == NULL) break;
            p->arg[i] = subtoken;
            i++;
        }
        
        //link process chain
        if(first_process == NULL){
            first_process = p;
            current_process = p;
        }
        else{
            current_process->next = p;
            current_process = p;
        }
    }
    
    //link job chain
    if(first_job == NULL){
        j->id = 1;
        j->name = j->first_process->arg[0];
        first_job = j;
        current_job = j;
    }
    else{
        j->id = current_job->id+1;
        current_job->next = j;
        current_job = j;
    }
    
}

void dispatch(string raw){
    
    //convert string to c string
    char *input = new char[raw.length()+1];
    strcpy(input, raw.c_str());
    
    //parsing first word by space or \t
    char *str1,*saveptr;
    str1 = strtok_r(input, space, &saveptr);
    
    //================================================
    //if the first word is exit then call exit_shell()
    //================================================
    if(strcmp(str1,"exit")==0) exit_shell();
    
    //================================================
    //if the first word is cd, parse the second word
    //and change directory
    //================================================
    if (strcmp(str1,"cd")==0){
        change_directory(saveptr);
        return;
    }
    
    //=================================================
    //if the first word is jobs, print jobs
    //=================================================
    
    if(strcmp(str1,"jobs")==0){
        list_jobs();
        return;
    }
    
    //=================================================
    //if the first word is kill, kill jobs
    //=================================================
    
    if(strcmp(str1,"kill")==0){
        kill_jobs(saveptr);
        return;
    }
    
    /*todo*/
    
    //=================================================
    //if the first word is bg, put job into background
    //=================================================
    
    if(strcmp(str1,"bg")==0){
        bg_jobs(saveptr);
        return;
    }
    
    /*todo*/
    
    //=================================================
    //if the first word is fg, put job into foreground
    //=================================================
    
    if(strcmp(str1, "fg")==0){
        fg_jobs(saveptr);
        return;
    }
    
    /*todo*/
    
    //=================================================
    //otherwise, create a new job
    //=================================================
    
    create_job(raw);
    launch_job(current_job);
    return;

}

int main(){
    
	//init the shell
	init_shell();
	
	//run the shell
	while(1){
        
        //get input
        get_input();
        string raw_input = "";
        getline(cin,raw_input);
		
        //if ctrl+d quit shell
        if(!cin.eof()){
            dispatch(raw_input);
        }
        else exit_shell();
	}
	return 0;
}