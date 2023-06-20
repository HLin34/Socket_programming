#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h> 
#include <sys/types.h>          // connect()
#include <sys/socket.h>         // connect()
#include <arpa/inet.h>
#include <vector>

#define BUFF_SIZE 256
#define IP_ADDRESS "127.0.0.1"
#define PORT 1234

using namespace std;

int main() {

    struct sockaddr_in server_addr;

	// Initialize socket structure
    bzero((char *) &server_addr, sizeof(server_addr));

    //setup the server address
	server_addr.sin_family = PF_INET;
	server_addr.sin_port = htons(PORT);
	server_addr.sin_addr.s_addr = inet_addr(IP_ADDRESS);

    int sockfd;

	//connect to the server	
	sockfd = socket (PF_INET, SOCK_STREAM, 0);

    // check if the socket is opened successfully, if failed it will return -1
    if(sockfd >= 0){
        printf("The socket is opened successfully!\n");
    } else{
        printf("The socket fails to be opened. \n");
        exit(1);
    }
   
    // connect ot the server
	int connect_value = connect (sockfd, (struct sockaddr *) &server_addr, sizeof(struct sockaddr_in));
    // check whether the connection is success
    if(connect_value < 0)
    {
        perror("There is an error on connection: ");
        printf("\n");
        exit(1);
    }else
        printf("Connect to the server siccessfully!\n");

	// Server_Request_Procedure(sockfd); //Call procedure you wish to perform
    

    // create two array use to send and receive messages
    char msg_recv[BUFF_SIZE], msg_send[BUFF_SIZE];

    while (1) {

        // clear the buffer and receive the message
        memset(msg_recv, '\0', sizeof(msg_recv));   
        read(sockfd, msg_recv, BUFF_SIZE);
        printf("%s", msg_recv);

        // clear the buffer and send the message
        memset(msg_send, '\0', sizeof(msg_send));
        scanf("%s", msg_send);
        write(sockfd, msg_send, BUFF_SIZE);

        if (!strcmp(msg_send, "3")) {
            break;
        }
    }
    
	close(sockfd);	
	return 0;

}