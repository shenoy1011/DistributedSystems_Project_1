#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <thread>
#include <mutex>       // MUTEX EDIT
#include <vector>
#include <unordered_map>
#include "interface.h"

using namespace std;

/* Global variables */
struct client_struct {
    int sockfd;
};

struct chatroom_struct {
    string name;
    int port;
    int num_members;
    bool active;
    int master_fd;
    vector<int> slave_socket_fds;
};

vector<chatroom_struct> chatroom_vector;  
vector<int> chatroom_fds;
int chatroom_port = 50000;
unordered_map<string, int> name_index_map;  
int REPLY_SIZE = sizeof(struct Reply);

mutex m;     // MUTEX EDIT

/* Functions */
string get_chatroom_name(char* command) {
    char* pch;
    string name = "";
    pch = strtok(command, " ");
    pch = strtok(NULL, " ");
    name = string(pch);

    return name;
} // get_chatroom_name

// Thread function to handle clients
void draft_client_listener(int clientfd, unsigned long host, int port, bool chatroom) {   
    char message[MAX_DATA];
    
    while (true) {  
        int serverrecvbytes = recv(clientfd, message, MAX_DATA, 0);
        
        if (serverrecvbytes > 0) {
            
        } else if (serverrecvbytes == 0){
            continue;
        } else if (serverrecvbytes == -1) {
            return;
        }
        
        if (strncmp(message, "CREATE", 5) == 0) {
            string chatroom_name = get_chatroom_name(message);
            if (name_index_map.find(chatroom_name) == name_index_map.end()) {
                // Create a new port and send Reply struct
                struct chatroom_struct chatroom;
                chatroom.name = chatroom_name;
                chatroom.port = chatroom_port;
                chatroom_port = chatroom_port + 1;
                chatroom.num_members = 0;
                chatroom.active = true;
                chatroom.master_fd = -1;
                chatroom_vector.push_back(chatroom);     
                name_index_map[chatroom_name] = chatroom_vector.size() - 1;

                char port_array[10];
                sprintf(port_array, "%d", chatroom.port);            
                char* args[] = {"./crsd", port_array, NULL};      
                if (fork() == 0) {
                    execv("./crsd", args);
                }

                struct Reply reply;
                reply.status = SUCCESS;
                send(clientfd, &reply, sizeof(reply), 0);  
            } else {
                // Make Reply struct as failure
                struct Reply reply;
                reply.status = FAILURE_ALREADY_EXISTS;
                send(clientfd, &reply, sizeof(reply), 0);    
            } 
        } else if (strncmp(message, "JOIN", 4) == 0) {
            string chatroom_name = get_chatroom_name(message);
            if (name_index_map.find(chatroom_name) == name_index_map.end()) {
                // Make Reply struct as failure
                struct Reply reply;
                reply.status = FAILURE_NOT_EXISTS;
                reply.num_member = 0;
                send(clientfd, &reply, sizeof(reply), 0);        
            } else {
                // Send a successful Reply struct for JOIN
                struct Reply reply;
                reply.status = SUCCESS;
                int index = name_index_map[chatroom_name];        
                chatroom_vector[index].num_members = chatroom_vector[index].num_members + 1;
                reply.num_member = chatroom_vector[index].num_members;
                reply.port = chatroom_vector[index].port; 
                send(clientfd, &reply, sizeof(reply), 0);   
            }
        } else if (strncmp(message, "LIST", 4) == 0) {
            struct Reply reply;
            char list[MAX_DATA];
            list[0] = '\0';
            for (auto i : name_index_map) {
                strcat(list, (i.first).c_str());
                strcat(list, ", ");
            }
            strcpy(reply.list_room, list);
            reply.status = SUCCESS;
            send(clientfd, &reply, sizeof(reply), 0);
        } else if (strncmp(message, "DELETE", 6) == 0) {
            string chatroom_name = get_chatroom_name(message);
            if (name_index_map.find(chatroom_name) == name_index_map.end()) {
                // Make Reply struct as failure
                struct Reply reply;
                reply.status = FAILURE_NOT_EXISTS;
                send(clientfd, &reply, sizeof(reply), 0);        
            } else {
                // Make main server a client of chatroom by connecting to chatroom port via socket() and connect()
                int index = name_index_map[chatroom_name];           
                int chatroom_fd = socket(AF_INET, SOCK_STREAM, 0);

                struct sockaddr_in chatroom_addr;
                memset(&chatroom_addr, 0, sizeof(chatroom_addr));
                chatroom_addr.sin_family = AF_INET;
                chatroom_addr.sin_addr.s_addr = host;
                chatroom_addr.sin_port = htons(chatroom_vector[index].port);  

                struct Reply reply;
                if (chatroom_fd < 0 || connect(chatroom_fd, (struct sockaddr*) &chatroom_addr, sizeof(chatroom_addr)) < 0) {
                    reply.status = FAILURE_INVALID;
                } else {
                    // Main server sends a message indicating that chatroom should broadcast the warning message for DELETE
                    string chatroom_name_msg = "QUIT " + chatroom_name;
                    char quit[MAX_DATA];
                    strcpy(quit, chatroom_name_msg.c_str());
                    if (send(chatroom_fd, quit, MAX_DATA, 0) < 0) {
                        reply.status = FAILURE_INVALID;
                    } else {
                        chatroom_vector[index].num_members = 0;     
                        chatroom_vector[index].active = false;     
                        reply.status = SUCCESS;
                        name_index_map.erase(chatroom_name);
                    } // else
                } // else
                send(clientfd, &reply, sizeof(reply), 0);         
            }
        } else if (strncmp(message, "QUIT", 4) == 0) {
            // Chatroom servers will use this to handle DELETE commands
            string chatroom_name = get_chatroom_name(message);
            char warningmsg[MAX_DATA] = "Chat room being deleted, shutting down connection...";
            int vector_length = chatroom_fds.size();       
            for (int i = 0; i < vector_length; i++) {
                send(chatroom_fds.at(i), warningmsg, MAX_DATA, 0);  
            } 
            for (int i = 0; i < vector_length; i++) {
                close(chatroom_fds.at(i));        
            } 
        } else {
            // Handle chatroom messages
            int vector_length = chatroom_fds.size();        
            for (int i = 0; i < vector_length; i++) {
                if (chatroom_fds.at(i) != clientfd) {        
                    send(chatroom_fds.at(i), message, MAX_DATA, 0);
                } // if
            } // for
        } // else
    } // while(true)
    
    // Close socket fds of clients connected to server
    if (chatroom) {
        int vector_length = chatroom_fds.size();       
        vector<int>::iterator it = chatroom_fds.begin();    
        
        for (int i = 0; i < vector_length; i++) {                                     
            if (chatroom_fds.at(i) == clientfd) {
                chatroom_fds.erase(it+i); // Erase i-th element
                break;
            } // if
        } // for

        close(clientfd);
    } else {
        close(clientfd);
    } // else
} // draft_client_listener

int main(int argc, char** argv) {
    // Error handle command line input
    if (argc != 2) {
        fprintf(stderr, "usage: enter port number\n");
        exit(1);
    }

    // Utilize input to setup socket connections
    int port = atoi(argv[1]);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Cannot create socket");
        return -1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (::bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {          // Might need ::bind
        close(sockfd);
        perror("Crsd : bind");
        return -1;
    }

    bool is_chatroom = (port >= 50000);

    if (listen(sockfd, 10) < 0) {
        perror("Crsd : listen");
        return -1;
    }

    // Setup multiple threads, each to handle one client
    vector<thread> client_threads;
    while (true) {
        int client_fd = accept(sockfd, NULL, NULL);
        
        if (client_fd < 0) {
            perror("Crsd : accept");
            continue;
        }

        struct client_struct client;
        client.sockfd = client_fd;

        if (is_chatroom) {
            chatroom_fds.push_back(client_fd);
        }
        
        client_threads.push_back(thread(draft_client_listener, client_fd, serv_addr.sin_addr.s_addr, port, is_chatroom));
    } // while(true)
    
    for (int i = 0; i < client_threads.size(); i++) {
        (client_threads.at(i)).join();
    } // for

    close(sockfd);   //Probably won't reach it but makes sense

    return 0;
}