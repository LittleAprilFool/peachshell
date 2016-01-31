#include<sys/types.h>
#include<sys/wait.h>
#include<termios.h>
#include<unistd.h>
#include<string.h>
#include<stdlib.h>
#include<signal.h>

#define space " 	"
#define line "|"

pid_t shell_pgid;
int shell_terminal;
struct termios shell_tmodes;

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
    struct termios tmodes;  //saved terminal modes
    char *name;
    int id;
    int stdin, stdout, stderr;
    int run;     //0 if job is stopped, 1 if job is running,-1 if job is killed
    int foreground; //0 if job is background, 1 if job is foreground
}Job;


//init first_job
Job * first_job = NULL;

//init current_job
Job * current_job = NULL;

void init_shell(){
	
	//get the file info
	shell_terminal = STDIN_FILENO;
    
    signal (SIGINT, SIG_IGN);
    signal (SIGQUIT, SIG_IGN);
    signal (SIGTSTP, SIG_IGN);
    signal (SIGTTIN, SIG_IGN);
    signal (SIGTTOU, SIG_IGN);
    signal (SIGCHLD, SIG_IGN);
    
    //set process group
    shell_pgid = getpid();
    setpgid(shell_pgid, shell_pgid);

    //grab control of the terminal
    tcsetpgrp(shell_terminal, shell_pgid);
    
    //save default terminal attributes
    tcgetattr(shell_terminal, &shell_tmodes);
    
    //welcome
    printf("Hello, April's shell is running happily!\n");
    
}

void exit_shell(){
	printf("April's shell is closing now. See ya!\n");
	exit(EXIT_SUCCESS);
}

void pre_input(){
    //current directory
    char current_direct[1024];
    //current user
    char current_user[1024];
    
    //update current directory
    getcwd(current_direct, sizeof(current_direct));
    
    //update current user
    getlogin_r(current_user,sizeof(current_user));
    
    //input format:username:current_directory$ input
    printf("%s: %s$ ", current_user, current_direct);
    
}

void wait_job(Job *j){
    int status;
    int pid;
    status = 0;
    do{
        pid = waitpid(j->pgid,&status,WUNTRACED|WNOHANG);
        if(WIFSTOPPED(status)){
            j->run = 0;
            printf("\n");    
            printf("[%d]   Stopped	%s\n", j->id, j->name);
            return;
        }
     }
    while(pid == 0);   //while there are children waited

    j->run = -1;    //if job finish, set status to -1
    printf("\n");    
}

void do_foreground(Job *j, int is_continue){
    tcsetpgrp(shell_terminal, j->pgid);
    if(is_continue){
        tcsetattr(shell_terminal, TCSADRAIN, &j->tmodes);
        //send every process continue signal
        kill(-j->pgid, SIGCONT);
    }
    wait_job(j);

    //put the shell back in the foreground
    //no idea how signal pass through different process group
    //if don't add ignore SIGTTOU, shell will quit
    signal(SIGTTOU, SIG_IGN);
    tcsetpgrp (shell_terminal, shell_pgid);
    
    //Restore the shell’s terminal modes.
    tcgetattr (shell_terminal, &j->tmodes);
    tcsetattr (shell_terminal, TCSADRAIN, &shell_tmodes);
}

void do_background(Job *j, int is_continue){
    if(is_continue){
        kill(-j->pgid, SIGCONT);
    }
    else{
	printf("[%d]%d\n", j->id, j->pgid);
    }
}

void launch_process(Process *p, pid_t pgid, int infile, int outfile, int errfile, int foreground){
    pid_t pid;
    
    pid = getpid();
    if(pgid == 0) pgid = pid;
    
    //put process into process group
    setpgid(pid,pgid);
    if(foreground)
        tcsetpgrp(shell_terminal, pgid);
    
    //set ctrl+c to terminate
    signal(SIGINT, SIG_DFL);
    
    //set ctrl+z to suspend
    signal(SIGTSTP, SIG_DFL);
    
    //set i/o channels of new process
    if(foreground){
        if(infile != STDIN_FILENO){
            dup2(infile, STDIN_FILENO);
            close(infile);
        }
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
    printf("execvp wrong\n");
    //exit(EXIT_FAILURE);
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
		printf("create pipe wrong\n");
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
	    printf("Fork Failed\n");
            //exit(EXIT_FAILURE);
            return;
        }
        
        //child process
        else if(pid == 0){
            launch_process(p, j->pgid, infile, outfile, errfile, j->foreground);
            
        }
        //parent process, wait until finished
        else{
            p->pid = pid;
            if(j->pgid == 0) j->pgid = pid;
            setpgid(pid, j->pgid);
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
    
    if(j->foreground)
        do_foreground(j,0);
    else
        do_background(j,0);
}

void list_jobs(){
    Job *j;
    for(j=first_job;j;j=j->next){
        //if job is not killed, print job
        if(j->run!=-1){
	    printf("[%d] ", j->id);
            if(j->run == 0) printf("Stopped                 ");
            else printf("Running                 ");
            
	    printf("%s", j->name);
            //if job is background, print &
            if(j->foreground!=1) printf(" &");
	    printf("\n");            
        }
    }
}

int get_number(char *saveptr){
    int i;
    int j;
    int sum;
    sum = 0;
    j = 1;
    if(saveptr == NULL) return -1;
    for(i = (int)strlen(saveptr);i>1; i--){
        int na = saveptr[i-1]-'0';
        if((na>=0)&&(na<=9)) sum += na*j;
        else{
            return -1;
        }
        j*=10;
    }
    return sum;
}

void bg_jobs(char *saveptr){
    int j_num;
    j_num = get_number(saveptr);
    if(j_num == -1){
        printf("Input wrong\n");
        return;
    }
    else{
        Job *j;
        int flag = 0;
        for(j=first_job;j;j=j->next){
            if(j->id==j_num){
                flag = 1;
                if(j->run==-1){
		    printf("job %d is terminated\n", j_num);
                }
                else if(j->foreground == 0){
		    printf("job %d is already in background\n", j_num);
                }
                else{
                    j->foreground =0;
		    printf("[%d] %s &\n", j->id, j->name);
                    j->run = 1;
                    do_background(j, 1);
                }
            }
        }
        
        if(flag == 0){
	    printf("no such job\n");
        }
    }
}

void fg_jobs(char *saveptr){
    int j_num;
    j_num = get_number(saveptr);
    if(j_num == -1){
	printf("Input wrong\n");
        return;
    }
    else{
        Job *j;
        int flag = 0;
        for(j=first_job;j;j=j->next){
            if(j->id==j_num){
                flag = 1;
                if(j->run==-1){
		    printf("job %d is terminated\n", j_num);
                }
                else{
                    j->run = 1;
                    j->foreground =1;
                    cout<<j->name<<endl;
                    do_foreground(j, 1);
                }
            }
        }
        if(flag == 0){
	    printf("no such job\n");
        }
    }
}

void kill_jobs(char* saveptr){
    int j_num;
    j_num = get_number(saveptr);
    if(j_num == -1){
	printf("Input wrong\n");
        return;
    }
    else{
        Job *j;
        int flag = 0;
        for(j=first_job;j;j=j->next){
            if(j->id==j_num){
                flag = 1;
                if(j->run==-1){
		    printf("job %d is already terminated\n", j_num);
                }
                else{
		    printf("[%d]   ", j->id);
                    if(j->run == 0) printf("Stopped                 ");
                    else printf("Running                 ");
                    
                    printf("%s", j->name);
                    //if job is background, print &
                    if(j->foreground!=1) printf(" &");
                    printf("\n");
		}
            }
        }
        if(flag == 0){
	    printf("no such job\n");
        }
    }
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
	printf("April's shell: cd: %s: No such file or directory\n", str2);
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
    j->foreground = 1;
    j->run = 1;
    j->stdin = STDIN_FILENO;
    j->stdout = STDOUT_FILENO;
    j->stderr = STDERR_FILENO;
    
    //parsing the string by |
    char *str1,*token,*subtoken,*saveptr;
    
    for(str1 = input; ; str1 = NULL){
        token = strtok_r(str1, line, &saveptr);
        if(token==NULL) break;
        
        
        //create process
        Process *p = new Process;
        p->next = NULL;
        
        int i=0;
        char *str2, *saveptr2;
        for(str2 = token;;str2=NULL){
            subtoken = strtok_r(str2, space, &saveptr2);
            if(subtoken == NULL) break;
            if(strcmp(subtoken,"&")==0){
                j->foreground=0;
                break;
            }
            p->arg[i] = subtoken;
            i++;
        }
        
        //link process chain
        if(first_process == NULL){
            first_process = p;
            current_process = p;
            j->first_process = first_process;
        }
        else{
            current_process->next = p;
            current_process = p;
        }
    }
    
    j->name = j->first_process->arg[0];
    
    //link job chain
    if(first_job == NULL){
        j->id = 1;
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
    
    //================================================
    //if it is enter
    //================================================
    if(raw=="") return;
    
    
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

void job_update(){
    Job *j;
    for(j = first_job;j;j=j->next){
        int status;
        //if a new job is done
        if((j->run ==1) && (waitpid(j->pgid, &status, WNOHANG)!=0))
        {
            j->run = -1;
	    printf("[%d]  Done     %s\n", j->id, j->name);
        }
    }
}

int main(){
    
	//init the shell
	init_shell();
	
	//run the shell
	while(1){
        
        //get input
        pre_input();
        string raw_input = "";
        getline(cin,raw_input);
		
        //if ctrl+d quit shell
        if(!cin.eof()){
            dispatch(raw_input);
        }
        else exit_shell();
        
        //check whether job in background is done
        job_update();
	}
	return 0;
}
