#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <thread>
#include "interface.h"

using namespace std;

/*
 * TODO: IMPLEMENT BELOW THREE FUNCTIONS
 */
int connect_to(const char *host, const int port);
struct Reply process_command(const int sockfd, char* command);
void process_chatmode(const char* host, const int port);

int REPLY_SIZE = sizeof(struct Reply);

int main(int argc, char** argv) 
{
	// Error handle command line input
	if (argc != 3) {
		fprintf(stderr,
				"usage: enter host address and port number\n");
		exit(1);
	}

    display_title();
    
	// Utilize input to setup socket connections
	bool closedsockfd = false;   
	int sockfd = connect_to(argv[1], atoi(argv[2]));
	
	while (1) {
		char command[MAX_DATA];
        get_command(command, MAX_DATA);

		struct Reply reply = process_command(sockfd, command);
		display_reply(command, reply);
		
		touppercase(command, strlen(command) - 1);
		if (strncmp(command, "JOIN", 4) == 0 && reply.status == SUCCESS) {
			close(sockfd);     // EDIT
			closedsockfd = true;
			printf("Now you are in the chatmode\n");
			process_chatmode(argv[1], reply.port);
		} // if
    } // while (1)
    
    if (closedsockfd == true) {
    	close(sockfd);
    } 

    return 0;
} // main

/*
 * Connect to the server using given host and port information
 *
 * @parameter host    host address given by command line argument
 * @parameter port    port given by command line argument
 * 
 * @return socket fildescriptor
 */
int connect_to(const char *host, const int port)
{
	// ------------------------------------------------------------
	// GUIDE :
	// In this function, you are suppose to connect to the server.
	// After connection is established, you are ready to send or
	// receive the message to/from the server.
	// 
	// Finally, you should return the socket fildescriptor
	// so that other functions such as "process_command" can use it
	// ------------------------------------------------------------

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror ("Cannot create socket");
		return -1;
	} // if
	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(host);
	serv_addr.sin_port = htons(port);
	if (connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) {
		perror("Cannot connect to server");
		return -1;
	} // if

	return sockfd;
} // connect_to

/* 
 * Send an input command to the server and return the result
 *
 * @parameter sockfd   socket file descriptor to commnunicate
 *                     with the server
 * @parameter command  command will be sent to the server
 *
 * @return    Reply    
 */
struct Reply process_command(const int sockfd, char* command)
{
	// ------------------------------------------------------------
	// GUIDE 1:
	// In this function, you are supposed to parse a given command
	// and create your own message in order to communicate with
	// the server. Surely, you can use the input command without
	// any changes if your server understand it. The given command
    // will be one of the followings:
	//
	// CREATE <name>
	// DELETE <name>
	// JOIN <name>
    // LIST
	//
	// -  "<name>" is a chatroom name that you want to create, delete,
	// or join.
	// 
	// - CREATE/DELETE/JOIN and "<name>" are separated by one space.
	// ------------------------------------------------------------


	// ------------------------------------------------------------
	// GUIDE 2:
	// After you create the message, you need to send it to the
	// server and receive a result from the server.
	// ------------------------------------------------------------


	// ------------------------------------------------------------
	// GUIDE 3:
	// Then, you should create a variable of Reply structure
	// provided by the interface and initialize it according to
	// the result.
	//
	// For example, if a given command is "JOIN room1"
	// and the server successfully created the chatroom,
	// the server will reply a message including information about
	// success/failure, the number of members and port number.
	// By using this information, you should set the Reply variable.
	// the variable will be set as following:
	//
	// Reply reply;
	// reply.status = SUCCESS;
	// reply.num_member = number;
	// reply.port = port;
	// 
	// "number" and "port" variables are just an integer variable
	// and can be initialized using the message fomr the server.
	//
	// For another example, if a given command is "CREATE room1"
	// and the server failed to create the chatroom becuase it
	// already exists, the Reply varible will be set as following:
	//
	// Reply reply;
	// reply.status = FAILURE_ALREADY_EXISTS;
    // 
    // For the "LIST" command,
    // You are suppose to copy the list of chatroom to the list_room
    // variable. Each room name should be seperated by comma ','.
    // For example, if given command is "LIST", the Reply variable
    // will be set as following.
    //
    // Reply reply;
    // reply.status = SUCCESS;
    // strcpy(reply.list_room, list);
    // 
    // "list" is a string that contains a list of chat rooms such 
    // as "r1,r2,r3,"
	// ------------------------------------------------------------

	struct Reply reply;
	reply.status = FAILURE_UNKNOWN;
	reply.num_member = 5;
	reply.port = 1024;

	if (sockfd == -1) {
		reply.status = FAILURE_INVALID;
		return reply;
	}

	int sendbytes = send(sockfd, command, MAX_DATA, 0);
	if (sendbytes <= 0) {
		perror("Cannot write to server");
		return reply;
	}

	int recvbytes = recv(sockfd, &reply, sizeof(reply), 0);       // REPLY_EDIT
	if (recvbytes <= 0) {         
		perror("Cannot read from server/No response from server");
		return reply;
	}

	return reply;
} // process_command

void command_line_listener(int sock_fd) {
	char message[MAX_DATA];
	do {
		printf("Enter a message: \n");
		get_message(message, MAX_DATA);

		if (strncmp(message, "QUIT", 4) == 0) {                 // strncmp(command, "JOIN", 4) == 0
			break;
		}
	} while (send(sock_fd, message, MAX_DATA, 0) > 0);

	close(sock_fd);    
} // command_line_listener

void chatroom_listener(int sock_fd) {
	char message[MAX_DATA];
	while (recv(sock_fd, message, MAX_DATA, 0) > 0) {      // recv(sock_fd, message, MAX_DATA, 0) >= 0     // FINAL_EDIT
		display_message(message);
	} // while
	
	close(sock_fd);     
} // chatroom_listener

/* 
 * Get into the chat mode
 * 
 * @parameter host     host address
 * @parameter port     port
 */
void process_chatmode(const char* host, const int port)
{
	// ------------------------------------------------------------
	// GUIDE 1:
	// In order to join the chatroom, you are supposed to connect
	// to the server using host and port.
	// You may re-use the function "connect_to".
	// ------------------------------------------------------------

	// ------------------------------------------------------------
	// GUIDE 2:
	// Once the client have been connected to the server, we need
	// to get a message from the user and send it to server.
	// At the same time, the client should wait for a message from
	// the server.
	// ------------------------------------------------------------
	
    // ------------------------------------------------------------
    // IMPORTANT NOTICE:
    // 1. To get a message from a user, you should use a function
    // "void get_message(char*, int);" in the interface.h file
    // 
    // 2. To print the messages from other members, you should use
    // the function "void display_message(char*)" in the interface.h
    //
    // 3. Once a user entered to one of chatrooms, there is no way
    //    to command mode where the user  enter other commands
    //    such as CREATE,DELETE,LIST.
    //    Don't have to worry about this situation, and you can 
    //    terminate the client program by pressing CTRL-C (SIGINT)
	// ------------------------------------------------------------

	// Connect to port of the chatroom, not the server
	// server_fd should be named chatroom_fd instead since its connecting to chatroom port
	int server_fd = connect_to(host, port);

	printf("IN CHATMODE\n");

	thread cmdlistener(command_line_listener, server_fd);
	thread chatroomlistener(chatroom_listener, server_fd);

	cmdlistener.join();
	chatroomlistener.join();

	printf("EXITED CHATMODE\n");
} // process_chatmode