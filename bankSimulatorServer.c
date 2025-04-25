#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>

#define MAX_ACCOUNTS 100

char *log_file = NULL;
char *fifoName = NULL;
int account_count = 0;

typedef struct {
    char client_fifo[32];
    char operation[32];
    char bank_id[32];
    int amount;
} Request;

typedef struct {
    char bank_id[16];
    int balance;
    int active;
} Account;

Account bank_db[MAX_ACCOUNTS];


void generate_bank_id(char *buffer, int id) {
    sprintf(buffer, "BankID_%d", id);
    printf("\n\n%d\n\n",account_count);
}

char *create_new_account(int initial_deposit) {
    if (account_count >= MAX_ACCOUNTS) return NULL;

    Account *acc = &bank_db[account_count];
    acc->balance = initial_deposit;
    acc->active = 1;
    generate_bank_id(acc->bank_id, account_count + 1);
    account_count++;
    return acc->bank_id;
}

Account *get_account_by_id(const char *id) {
    for (int i = 0; i < account_count; i++) {
        if (strcmp(bank_db[i].bank_id, id) == 0 && bank_db[i].active)
            return &bank_db[i];
    }
    return NULL;
}

void append_to_log(const char *entry) {
    int fd = open(log_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        perror("Log file open");
        return;
    }

    write(fd, entry, strlen(entry));
    close(fd);
}

void update_log(const char *bank_id, int deposit, int withdraw) {
    char log_entry[50];
    snprintf(log_entry, sizeof(log_entry), "%s D %d W %d\n", bank_id, deposit, withdraw);
    append_to_log(log_entry);
}

void handle_client(char *fifo_name, char *operation, char *bank_id, int amount) {
    int client_fd = open(fifo_name, O_WRONLY);
    printf("CLIENT FIFO NAME: %s\n", fifo_name);
    if (client_fd < 0) {
        perror("Client FIFO open");
        return;
    }

        Account *acc = get_account_by_id(bank_id);
        if (!acc) {
            dprintf(client_fd, "FAIL Invalid account\n");
        } else if (strcmp(operation, "deposit") == 0) {
            acc->balance += amount;
            dprintf(client_fd, "SUCCESS %s %d\n", acc->bank_id, acc->balance);
            printf("%s deposited %d credits... updating log\n", acc->bank_id, amount);
            update_log(acc->bank_id, amount, 0);
        } else if (strcmp(operation, "withdraw") == 0) {
            if (acc->balance >= amount) {
                acc->balance -= amount;
                if (acc->balance == 0) acc->active = 0;
                dprintf(client_fd, "SUCCESS %s %d\n", acc->bank_id, acc->balance);
                printf("%s withdraws %d credits... updating log.. Bye %s\n", acc->bank_id, amount, acc->bank_id);
                update_log(acc->bank_id, 0, amount);
            } else {
                dprintf(client_fd, "FAIL Insufficient funds\n");
                printf("%s withdraws %d credit...operation not permitted.\n", acc->bank_id, amount);
            }
        } else {
            dprintf(client_fd, "FAIL Unknown operation\n");
        }
    
    close(client_fd);
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
    if (token) strcpy(request->client_fifo, token);
    token = strtok(NULL, " ");
    if (token) strcpy(request->bank_id, token);
    token = strtok(NULL, " ");
    if (token) strcpy(request->operation, token);
    token = strtok(NULL, " ");
    if (token) request->amount = atoi(token);
    else request->amount = 0;
}

void custom_exit(int signo) {
    write(1, "\nSignal received, closing active Tellers\n", 40);
    unlink(fifoName);
    write(1, "Removing ServerFIFO...Updating log file...\n", 40);
    write(1, "AdaBank says 'Bye'...\n", 22);
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

    strcpy(bankName, argv[1]);
    fifoName = argv[2];
    log_file = argv[1];

    printf("(bankName: %s , fifoName: %s)\n", bankName, fifoName);

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
        Request request;
        char line[50];
        ssize_t bytes = read(server_fd, &line, sizeof(line));
        if (bytes <= 0) continue;

        parse_request(line, &request);

        if ((strcmp(request.bank_id, "N") == 0 || strcmp(request.bank_id, "BankID_None") == 0) &&
	    strcmp(request.operation, "deposit") == 0) {
	    
	    // Create account in parent
	    char *new_id = create_new_account(request.amount);
	    if (new_id) {
	        strcpy(request.bank_id, new_id); // Set the new bank_id for the child
	    }
	}
        pid_t pid = fork();
        if (pid == 0) {
            printf("Teller PID %d is active serving %s... %s\n", getpid(), request.client_fifo,
                (strcmp(request.bank_id, "N") == 0 || strcmp(request.bank_id, "BankID_None") == 0 ? "" : "Welcome back"));
            handle_client(request.client_fifo, request.operation, request.bank_id, request.amount);
            exit(0);
        } else {
            wait(NULL);
        }
    }

    close(server_fd);
    unlink(fifoName);
    return 0;
}
