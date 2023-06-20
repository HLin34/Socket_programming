#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h> 
#include <sys/types.h>       // socket(), bind(), accept()
#include <sys/socket.h>      // socket(), bind(), listen(), accept()
#include <arpa/inet.h>
#include <vector>

#define BUFF_SIZE 256
#define PORT 1234

using namespace std;

// build a structure to record student id and email in query.txt
class Student {
        public:
            char id[10], email[50];
            Student() {}
    };

vector<Student> st_list;

// read query.txt file and store the data into the list
void Student_List(){
    FILE *fp;
    if ((fp = fopen("query.txt", "r")) == NULL) {
        printf("File 'query.txt' doesn't exist in the directory.\n");
        exit(1);
    }
    else {
        Student st;
        while (fscanf(fp, "%s %s", st.id, st.email) != EOF) {
            st_list.push_back(st);
        }
    }
}


int main () {

    struct sockaddr_in myaddr;
    struct sockaddr_in client_addr;
    struct hostent *hent;

    // Initialize socket structure
	bzero((char*) &myaddr, sizeof(myaddr));

    //setup address
    myaddr.sin_family = PF_INET;
	myaddr.sin_port = htons(PORT);
	myaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	int sockfd, streamfd, status, addr_size;

    // call to socket() function to create the socket
    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    
    // check if TCP connection is setup successfully, if failed it will return -1
    if(sockfd >= 0){
        printf("The socket is created successfully!\n");
    } else{
        printf("The socket fails to be created. \n");
        exit(1);
    }


    // Ensure that address will not be already in use
    int reuseaddr = 1;
    socklen_t reuseaddr_len;
	reuseaddr_len = sizeof(reuseaddr);
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, reuseaddr_len);

    // bind the server port and address
    int bind_value = bind(sockfd, (struct sockaddr*) &myaddr, sizeof(myaddr));

    // Check whether the binding is successful
    if( bind_value < 0){
        perror("There is an error on binding: ");
        printf("\n");
        exit(1);
    }

    // listen to socket connections and limilt the amount in the connection queue
    // here process will go in sleep mode and will wait for the incoming connection
	listen(sockfd, 10);
	addr_size = sizeof(client_addr);	

    Student_List();

	while(1) {
        printf("Waiting for connection...\n");

        // use accept() to deal with connection requests, only call accept() in the server side.
	    streamfd = accept(sockfd, (struct sockaddr *) &client_addr, (socklen_t *) &addr_size);

        // check whether accpet succesfully
        if (streamfd < 0)
        {
            perror("There is an error on accepting: ");
            printf("\n");
            exit(1);
        }

        // create two array use to send and receive messages
        char msg_recv[BUFF_SIZE], msg_send[BUFF_SIZE];

        while (1) {
            
            // clear the buffer and send the message
            memset(msg_send, '\0', sizeof(msg_send));  
            strcpy(msg_send, "What's your requirement? 1.DNS 2.QUERY 3.QUIT : ");
            write(streamfd, msg_send, strlen(msg_send));
            
            // clear the buffer and store the message received
            memset(msg_recv, '\0', sizeof(msg_recv));   
            read(streamfd, msg_recv, BUFF_SIZE);
            
            // DNS requirement
            if (!strcmp(msg_recv, "1")) {

                // reply 
                memset(msg_send, '\0', sizeof(msg_send));
                strcpy(msg_send, "Input URL address : ");
                write(streamfd, msg_send, strlen(msg_send));

                // clear the buffer and receive the messages
                memset(msg_recv, '\0', sizeof(msg_recv));   
                read(streamfd, msg_recv, BUFF_SIZE);
                
                // get the DNS and try to return the address
                hent = gethostbyname(msg_recv);
                if (hent == NULL) {
                    printf("No such DNS\n");
                    memset(msg_send, '\0', sizeof(msg_send));
                    strcpy(msg_send, "There is an error: No such DNS!!!\n\n");
                    write(streamfd, msg_send, strlen(msg_send));
                }
                else {
                    printf("IP Address: %s\n", inet_ntoa(*(struct in_addr*) hent->h_addr));
                    memset(msg_send, '\0', sizeof(msg_send));
                    strcpy(msg_send, "address get from the domain name : ");
                    strcat(msg_send, inet_ntoa(*(struct in_addr*) hent->h_addr));
                    strcat(msg_send, "\n\n");
                    write(streamfd, msg_send, strlen(msg_send));
                }
            } // query request
            else if (!strcmp(msg_recv, "2")) {

                // reply 
                memset(msg_send, '\0', sizeof(msg_send));
                strcpy(msg_send, "Input student ID : ");
                write(streamfd, msg_send, strlen(msg_send));

                // clear the buffer and receive the messages
                memset(msg_recv, '\0', sizeof(msg_recv));
                read(streamfd, msg_recv, BUFF_SIZE);

                // try to find
                bool find = false;
                for (int i = 0; i < st_list.size(); i++) {
                    if (!strcmp(st_list[i].id, msg_recv)) {
                        memset(msg_send, '\0', sizeof(msg_send));
                        strcpy(msg_send, "Email get from the server : ");
                        strcat(msg_send, st_list[i].email);
                        strcat(msg_send, "\n\n");
                        write(streamfd, msg_send, strlen(msg_send));
                        printf("%s: %s\n", st_list[i].id, st_list[i].email);
                        find = true;
                        break;
                    }
                }
                if (!find) {
                    printf("No such student ID\n");
                    memset(msg_send, '\0', sizeof(msg_send));
                    strcpy(msg_send, "Email get from the server : No such student ID\n\n");
                    write(streamfd, msg_send, strlen(msg_send));
                }
            } // Quit
            else if (!strcmp(msg_recv, "3")) {
                break;
            }
            else {
                memset(msg_send, '\0', sizeof(msg_send));
                strcpy(msg_send, "Invalid requirement! (the request should between 1~3)\n\n");
                write(streamfd, msg_send, strlen(msg_send));
            }
        }        
	}
    close(streamfd);
    return 0;
}
