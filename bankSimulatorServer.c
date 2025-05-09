#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/mman.h>

#define MAX_ACCOUNTS 100

char *log_file = NULL;
char *fifoName = NULL;
int account_count = 0;

typedef struct {
    char clientName[32];
    char operation[32];
    char bank_id[32];
    int amount;
} Request;

typedef struct {
    char clientName[32];
    char bank_id[32];
    int deposited;
    int withdrawn;
} Account;

typedef struct {
    Account accounts[MAX_ACCOUNTS];
    int account_count;
} SharedData;

SharedData *shared_data;


void generate_bank_id(char *buffer, int id) {
    sprintf(buffer, "BankID_%d", id);
    printf("\n\n%d\n\n",account_count);
}


int create_account(char *bank_id){
    if(bank_id == NULL){
        sprintf(shared_data->accounts[shared_data->account_count].bank_id,"BankID_%02d", shared_data->account_count+1);
    }
    else{
        strcpy(shared_data->accounts[shared_data->account_count].bank_id, bank_id);
    }
    
    shared_data->accounts[shared_data->account_count].deposited = 0;
    shared_data->accounts[shared_data->account_count].withdrawn = 0;
    sprintf(shared_data->accounts[shared_data->account_count].clientName,"Client_%02d", shared_data->account_count+1);

    printf("%s created. account count: %d\n", shared_data->accounts[shared_data->account_count].bank_id, shared_data->account_count);
    return shared_data->account_count++;
}
int get_account_by_id(const char *id) {
    for (int i = 0; i < shared_data->account_count; i++) {
        if (strcmp(shared_data->accounts[i].bank_id, id) == 0) {
            return i;
        }
    }
    return -1;
}

void update_log1(){
    int fd = open(log_file , O_WRONLY | O_CREAT | O_TRUNC,0644);
    if(fd == -1){
        perror("Log file write error");
        return;
    }

    for(int i=0; i<shared_data->account_count; ++i){
        dprintf(fd, "# %s D %d W %d %d\n",
            shared_data->accounts[i].bank_id,
            shared_data->accounts[i].deposited,
            shared_data->accounts[i].withdrawn,
            shared_data->accounts[i].deposited - shared_data->accounts[i].withdrawn);

    }

    dprintf(fd,"## end of log.\n");
    close(fd);
}

void handle_client1(Request req) {
    int accountIdx = -1;
    int is_new =0;

    //printf("req bank id : %s\n",req.bank_id);
    if((strcmp(req.bank_id,"N") == 0) || (strcmp(req.bank_id,"BankID_None") ==0)){
        accountIdx = create_account(NULL);
        strcpy(req.bank_id, shared_data->accounts[accountIdx].bank_id);
        is_new=1;
        if (strcmp(req.operation, "withdraw") == 0) {
    
            printf("%s withdraws %d credit.. operation not permitted.\n",req.clientName,req.amount);
            return;
        }
        
    }
    else{

        accountIdx = get_account_by_id(req.bank_id);

        if(accountIdx ==-1){
            return;
        }
    }

    Account *acc = &shared_data->accounts[accountIdx];
    printf("%d %d %d\n",acc->deposited,acc->withdrawn,req.amount);

    if(strcmp(req.operation,"deposit") == 0){
        acc->deposited += req.amount;
        printf("%s deposited %d credits... updating log\n", req.clientName, req.amount);
    }
    else if (strcmp(req.operation, "withdraw") == 0) {
        printf("account bank id: %s %s ",acc->bank_id,req.bank_id);
        if(is_new == 1){
            printf("%s withdraws %d credit.. operation not permitted.\n",req.clientName,req.amount);
        }
        else{
            if((acc->deposited - req.amount) >= 0){
            acc->withdrawn += req.amount;
            printf("%s withdraws %d credits...updating log\n",req.clientName,req.amount);

            if(acc->deposited == acc->withdrawn){
                printf("...Bye %s\n",req.clientName);
            }
        }
            else{
                printf("%s withdraws %d credit.. operation not permitted.\n",req.clientName,req.amount);
            }
        }
        

    }
printf("%d %d %d\n",acc->deposited,acc->withdrawn,req.amount);
    update_log1();
    
    //printf("%s served.. %s\n",req.clientName,acc->bank_id);
}

pid_t Teller(void *func, void *arg_func) {
    pid_t pid = fork();

    if (pid == 0) {
        void (*f)(void*) = func;
        f(arg_func);
        _exit(0);
    }

    return pid;
}

int waitTeller(pid_t pid, int *status) {
    return waitpid(pid, status, 0);
}


void parse_request(char *line, Request *request) {
    char *token = strtok(line, " ");
    printf("%s ",token);
    if (token) strcpy(request->clientName, token);
    
    token = strtok(NULL, " ");
    printf("%s ",token);
    if (token) strcpy(request->bank_id, token);
    token = strtok(NULL, " ");
    printf("%s ",token);
    if (token) strcpy(request->operation, token);
    token = strtok(NULL, " ");
    printf("%s ",token);
    if (token) request->amount = atoi(token);
    else request->amount = 0;
}

void custom_exit(int signo) {
    
    printf("Signal received, closing active Tellers\n");
    unlink(fifoName);
    printf("Removing ServerFIFO...Updating log file...\n");
    printf("AdaBank says 'Bye'...\n");
    

    munmap(shared_data, sizeof(SharedData));
    exit(0);
}

int main(int argc, char *argv[]) {
    char bankName[20];
    if (argc < 3) {
        const char *msg = "Error: Not enough arguments.\n";
        write(2, msg, strlen(msg));
        return 1;    
    }

    signal(SIGINT, custom_exit);
    signal(SIGTERM, custom_exit);


    shared_data = mmap(NULL, sizeof(SharedData),
                PROT_READ | PROT_WRITE,
                MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if(shared_data == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }

    
    shared_data->account_count = 0;

    strcpy(bankName, argv[1]);
    fifoName = argv[2];
    log_file = argv[1];

    if(access(log_file,F_OK) !=0){
        printf("No previous logs..Creating the bank database\n");
        int fd = open(log_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd == -1) {
            perror("Error creating database");
            exit(1);
        }
        close(fd); 
    }
    //printf("(bankName: %s , fifoName: %s)\n", bankName, fifoName);

    if (mkfifo(fifoName, 0666) == -1) {
        write(2, "Error creating FIFO\n", 20);
        exit(1);
    }

    char msg[100];
    snprintf(msg, sizeof(msg), "%s is active.. Waiting for clients @%s...\n", bankName, fifoName);
    write(1, msg, strlen(msg));

    int server_fd = open(fifoName, O_RDONLY);
    if (server_fd == -1) {
        write(2, "Error opening FIFO\n", 20);
        exit(1);
    }

    while (1) {

        
        int client_count =0;
        
        
        ssize_t bytes = read(server_fd, &client_count, sizeof(int));
        if(bytes == 0) continue;
        else if(bytes <0 ) perror("read server fifo");

        pid_t ppid = getpid();
        printf("Received %d clients from PIDClient%d\n",client_count,ppid);

        for(int i=0; i<client_count;i++){

            char line[100];
            Request req;
            read(server_fd, &req, sizeof(Request));
            //read(server_fd,&line,sizeof(line));
            //parse_request(line,&req);
            
            pid_t teller = fork();
            if(teller ==0){


                int in = get_account_by_id(req.bank_id);
                if(in != -1){
                strcpy(req.clientName, shared_data->accounts[in].clientName);
                printf("Teller PID %d is active serving %s...Welcome back %s\n", getpid(), req.clientName,req.clientName);
                    
                }
                 
                 
                 else{
                    sprintf(req.clientName,"Client_%02d",shared_data->account_count+1);
                    
                    printf("Teller PID %d is active serving %s...\n", getpid(), req.clientName);
                }
                handle_client1(req);
                exit(0);
            } 

            else if(teller > 0){
                waitpid(teller, NULL, 0);
            }

            else {
                perror("fork");
                exit(EXIT_FAILURE);
            }
            
        }

        for(int i=0; i< client_count; i++){
            wait(NULL);
        }
       
    }

    close(server_fd);
    unlink(fifoName);
    munmap(shared_data, sizeof(SharedData));
    return 0;
}
