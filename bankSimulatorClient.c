#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>

typedef struct {
    char clientName[32];
    char operation[32];
    char bank_id[32];
    int amount;
} Request;

char *server_fifoname = NULL;

void parse_line(char *line, Request *request, int client_count) {
    sprintf(request->clientName, "Client_%02d", client_count + 1);
    unlink(request->clientName);
    if (mkfifo(request->clientName, 0666) == -1) {
        perror("FIFO create failed");
        exit(1);
    }

    char *token = strtok(line, " ");
    if (token) strcpy(request->bank_id, token);

    token = strtok(NULL, " ");
    if (token) strcpy(request->operation, token);

    token = strtok(NULL, " ");
    request->amount = token ? atoi(token) : 0;
}


void send_request_to_server(Request *request, int client_id) {
    int server_fd = open(server_fifoname, O_WRONLY);
    if (server_fd < 0) {
        perror("Cannot connect to server FIFO");
        exit(1);
    }

    char buffer[128];
    printf( "%s %s %s %d", request->clientName, request->bank_id, request->operation, request->amount);
    sprintf(buffer, "%s %s %s %d", request->clientName, request->bank_id, request->operation, request->amount);
    if (write(server_fd, buffer, strlen(buffer) + 1) < 0) {
        perror("Error writing to server FIFO");
        exit(1);
    }

    close(server_fd);

    int client_fd = open(request->clientName, O_RDONLY);
    if (client_fd >= 0) {
        char response[128];
        read(client_fd, response, sizeof(response));
        printf("Client%d response: %s\n", client_id + 1, response);
        close(client_fd);
    } else {
        fprintf(stderr, "Client%d: Something went wrong reading response.\n", client_id + 1);
    }

    unlink(request->clientName);
} 

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <ClientFile> <ServerFIFO_Name>\n", argv[0]);
        exit(1);
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror("Failed to open client file");
        exit(1);
    }

    server_fifoname = argv[2];
    int server_fd = open(server_fifoname,O_WRONLY);
    if(server_fd < 0){
        perror("Failed to connect Server FIFO");
        exit(1);
    }
    printf("Reading %s...\n", argv[1]);
    printf("Creating clients...\n");

    Request requests[20];
    char buffer[256];
    int bytes_read = 0, client_count = 0;
    char line[256];
    int index = 0;

    while ((bytes_read = read(fd, &buffer[index], 1)) > 0) {
        if (buffer[index] == '\n') {
            buffer[index] = '\0';
            
            parse_line(buffer, &requests[client_count], client_count);
            //printf("%s %s %s %d", requests[index].clientName, requests[index].bank_id, requests[index].operation, requests[index].amount);
            //send_request_to_server(&request, client_count);
            client_count++;
            index = 0;
        } else {
            index++;
        }
    }
    if (index > 0) {
        buffer[index] = '\0';
        
        parse_line(buffer, &requests[client_count], client_count);
        //send_request_to_server(&request, client_count);
    }

    printf("%d clients to connect... creating clients...\n",client_count);
    printf("Connected to %s...\n",server_fifoname);

    write(server_fd,&client_count,sizeof(int));

    for(int i=0; i< client_count; i++){
        //send_request_to_server(&requests[i], i);
        write(server_fd,&requests[i],sizeof(Request));
        printf("%s %s %s %d\n", requests[i].clientName, requests[i].bank_id, requests[i].operation, requests[i].amount);
    }
    

    close(fd);
    printf("Exiting...\n");

    return 0;
}
