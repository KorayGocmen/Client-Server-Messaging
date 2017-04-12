/*
 *  
 * File:   server.c
 * Author: Koray Gocmen
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <inttypes.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>


// Define the max values
#define MAXSIZE 1024
#define MAX_DATA 1024
#define MAX_NAME 128
#define MAX_CONNECTION 10
#define MAX_SEND 2048

// Command Types
#define LOGIN 1
#define LO_ACK 2
#define LO_NAK 3
#define EXIT 4
#define JOIN 5
#define JN_ACK 6
#define JN_NAK 7
#define LEAVE_SESS 8
#define NEW_SESS 9
#define NS_ACK 10
#define MESSAGE 11
#define QUERY 12
#define QU_ACK 13
#define INVITE 14


struct message {
    unsigned int type;
    unsigned int size;
    unsigned char source[MAX_NAME];
    unsigned char data[MAXSIZE];
};

struct client_node {
    char client_id[MAX_NAME];
    char pass[MAX_NAME];
    char session_id[MAX_NAME];
    int client_socket;
    struct client_node *next;
};

struct session_node {
    char session_id[MAX_NAME];
    struct client_node *ses_client_list;
    struct session_node *next;
};

struct session_list {
    struct session_node *head;
};

struct client_list {
    struct client_node *head;
};

//Keeps all the sessions in a linked list.
struct session_list session_list;
struct client_list client_list;

//Initialize the helper functions
void initialize_connection(struct sockaddr_in client, int server_socket);
void existing_connection(struct client_node *existing_client, int server_socket);
int bufferReader(char *buffer, char *type, char *source, char *data);

//command handlers
void command_handle_exit(struct client_node *existing_client, struct message *new_message);
void command_handle_join(struct client_node *existing_client, struct message *new_message);
void command_handle_new_sess(struct client_node *existing_client, struct message *new_message);
void command_handle_leave_sess(struct client_node *existing_client, struct message *new_message);
void command_handle_query(struct client_node *existing_client, struct message *new_message);
void command_handle_message(struct client_node *existing_client, struct message *new_message);
void command_handle_login(struct client_node *existing_client, struct message *new_message);
void command_handle_invite(struct client_node *existing_client, struct message *new_message);
void command_handle_unknown(struct client_node *existing_client, struct message *new_message);

//client_list helpers
void insert_client_to_client_list(struct client_node *new_client);
struct client_node* remove_client_from_client_list(char *client_id);
void remove_session_id_from_client_in_client_list(struct client_node *new_client);
struct client_node* duplicate_client(struct client_node *_client);

//session_list helpers
void insert_session_to_session_list(struct session_node *new_session);
struct session_node* remove_session_from_session_list(char *session_id);
struct client_node* remove_client_from_session(char *session_id, char *client_id);
struct session_node* find_session_from_session_list(char *session_id);
void insert_client_to_session_list(struct session_node* session, struct client_node* new_client);


int main(int argc, char** argv) {
    
    int port;
    
    // Check the usage of the server
    if(argc != 2){
        printf("Error in usage of server program\n");
        return 0;
    }
    
    //initialize the client and session lists
    session_list.head = NULL;
    client_list.head = NULL;
    
    
    port = atoi(argv[1]);
    
    //initialize socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(server_socket < 0){
        printf("Error creating the socket\n");
        return 0;
    }
    
    //bind the socket
    struct sockaddr_in server_address;
    struct sockaddr_in client_address;
    
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    
    memset(server_address.sin_zero, '\0', sizeof(server_address.sin_zero));
    
    int bind_status = bind(server_socket, (struct sockaddr*) &server_address, sizeof(server_address));
    if(bind_status < 0){
        printf("Error binding the socket\n");
        return 0;
    }
    
    //start listening with MAX_CONNECTION
    if(listen(server_socket, MAX_CONNECTION) < 0){
        printf("Error listening the socket\n");
        return 0;
    }
    
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(server_socket, &rfds);
    int max_fd = server_socket;
    
    while(1){
        
        printf("Listening...\n");

		fd_set rfds;
    	FD_ZERO(&rfds);
    	FD_SET(server_socket, &rfds);
    	max_fd = server_socket;
        
        struct client_node *cur = client_list.head;
        while(cur != NULL){
            FD_SET(cur->client_socket, &rfds);
            if(max_fd < cur->client_socket)
                max_fd = cur->client_socket;
            cur = cur->next;
        }
        
        int ret_val = select(max_fd + 1, &rfds, NULL, NULL, NULL);
        if(ret_val < 0){
            printf("Error in select\n");
            return 0;
        }

        // No errors in select
        if(FD_ISSET(server_socket, &rfds)){
            //this is a new connection coming from server_socket
            //initialize the new connection
            initialize_connection(client_address, server_socket);
        }
        else {
            //this is a known socket, we have to find it from client_list
            struct client_node *cur = client_list.head;
            while(cur != NULL){
                if(FD_ISSET(cur->client_socket, &rfds)){
                    //found the node
                    //serve the known socket
                    existing_connection(cur, server_socket);
                    break;
                }
                cur = cur->next;
            }
            if(cur == NULL){
                //The socket was a known socket but the client can not be found
                printf("Client associated with socket not found\n");
                return 0;
            }
        }
        
    }//end of while(1)
    
    return (EXIT_SUCCESS);
}

void initialize_connection(struct sockaddr_in client_address, int server_socket){
    
    int client_len = sizeof(client_address);
    int new_socket = accept(server_socket, (struct sockaddr*) &client_address, &client_len);
    
    if(new_socket < 0){
        printf("Error with connection to client\n");
        return;
    }
    
    char read_buffer[MAX_SEND];
    memset(read_buffer, 0, sizeof(read_buffer));
    int read_data = recv(new_socket, read_buffer, MAX_SEND, 0);
    
    if(read_data < 0){
        printf("Error reading from initialize connection\n");
        return;
    }
    
    struct message *new_message;
    new_message = (struct message*)malloc(sizeof(struct message));
    char type[MAX_NAME];
    bzero((char*)type, sizeof(type));
    int size = bufferReader(read_buffer, type, new_message->source, new_message->data);
    
    if(strcmp(type, "EXIT") == 0){
        new_message->type = EXIT;
    }
    else if(strcmp(type, "JOIN") == 0){
        new_message->type = JOIN;
    }
    else if(strcmp(type, "NEW_SESS") == 0){
        new_message->type = NEW_SESS;
    }
    else if(strcmp(type, "LEAVE_SESS") == 0){
        new_message->type = LEAVE_SESS;
    }
    else if(strcmp(type, "QUERY") == 0){
        new_message->type = QUERY;
    }
    else if(strcmp(type, "MESSAGE") == 0){
        new_message->type = MESSAGE;
    }
    else if(strcmp(type, "LOGIN") == 0){
        new_message->type = LOGIN;
    }
    
    
    if(new_message->type == LOGIN){
        //login request
        new_message->type = LOGIN;
        new_message->size = size;
        strcpy(new_message->source, new_message->source);
        strcpy(new_message->data, new_message->data);
        
        struct client_node *cur = client_list.head;
        while(cur != NULL){
            if(strcmp(cur->client_id, new_message->source) == 0){
                
                char response_buffer[MAX_SEND];
                memset(read_buffer, 0, sizeof(read_buffer));
                
                char data[MAXSIZE];
                memset(data, 0, sizeof(data));
                strcpy(data, "user already logged in");
                int data_size = strlen(data);
                sprintf(response_buffer, "LO_NAK,%d,SERVER,%s",data_size,data);
                send(new_socket, response_buffer, strlen(response_buffer), 0);
                return;
            }
            cur = cur->next;
        }
        if(cur == NULL){
            //we couldn't find the user as online
            //check db to match user
            
            FILE *fp;
            char *client_auths = "client_auths.txt";
            fp = fopen(client_auths, "r");
            char line[MAXSIZE];
            memset(line, 0, sizeof(line));
            
            char *incoming_name;
            char *incoming_pass;
            incoming_name = strtok(new_message->data, ",");
            incoming_pass = strtok(NULL, ",");
            incoming_pass[strlen(incoming_pass)] = '\0';
            
            while(fgets(line, MAXSIZE, fp) != NULL){
                //keep reading the file
                char *name;
                char *pass;
                name = strtok(line, ",");
                pass = strtok(NULL, ",");
                pass[strlen(pass)-1] = '\0';
                
                if(strcmp(name, incoming_name) == 0){
                    //found the client id
                    if(strcmp(pass, incoming_pass) == 0){
                        //password matches
                        //create the new user object 
                        //add the new user to the client list
                        //send LO_ACK to the client
                        
                        struct client_node *new_client;
                        new_client = (struct client_node*) malloc(sizeof(struct client_node));
                        memset(new_client->client_id, 0, sizeof(new_client->client_id));
                        memset(new_client->pass, 0, sizeof(new_client->pass));
                        memset(new_client->session_id, 0, sizeof(new_client->session_id));
                        strcpy(new_client->client_id, name);
                        strcpy(new_client->pass, pass);
                        new_client->client_socket = new_socket;
                        new_client->next = NULL;
                        
                        //add the new client to the end of client list
                        insert_client_to_client_list(new_client);
                        
                        char response_buffer[MAX_SEND];
                        memset(response_buffer, 0, sizeof(response_buffer));
                        sprintf(response_buffer, "LO_ACK,%d,SERVER",0);
                        send(new_socket, response_buffer, strlen(response_buffer), 0);
                        return;
                    }
                    else{
                        //password does not match
                        //respond to the user
                        char response_buffer[MAX_SEND];
                        memset(response_buffer, 0, sizeof(response_buffer));
                        char data[MAXSIZE];
                        memset(data, 0, sizeof(data));
                        strcpy(data, "invalid password");
                        int data_size = strlen(data);
                        sprintf(response_buffer, "LO_NAK,%d,SERVER,%s",data_size,data);
                        send(new_socket, response_buffer, strlen(response_buffer), 0);
                        return;
                    }
                }
            }
            
            //can't find the username
            char response_buffer[MAX_SEND];
            memset(response_buffer, 0, sizeof(response_buffer));
            char data[MAXSIZE];
            memset(data, 0, sizeof(data));
            strcpy(data, "invalid username");
            int data_size = strlen(data);
            sprintf(response_buffer, "LO_NAK,%d,SERVER,%s",data_size,data);
            send(new_socket, response_buffer, strlen(response_buffer), 0);
            return;
            
        }
        
    } 
    
}

void existing_connection(struct client_node *existing_client, int server_socket){
    
    char read_buffer[MAX_SEND];
    memset(read_buffer, 0, sizeof(read_buffer));
    int read_data = recv(existing_client->client_socket, read_buffer, MAX_SEND, 0);
    
    if(read_data < 0){
        printf("Error reading from existing connection\n");
        return;
    }
    
    struct message *new_message;
    new_message = (struct message*)malloc(sizeof(struct message));

    char type[MAX_NAME];
    bzero((char*)type, sizeof(type));
    int size = bufferReader(read_buffer, type, new_message->source, new_message->data);
    
    if(strcmp(type, "EXIT") == 0){
        //leave the sessions and exit the user
        printf("serving command EXIT\n");
        new_message->type = EXIT;
        command_handle_exit(existing_client, new_message);
    }
    else if(strcmp(type, "JOIN") == 0){
        //join to a new session
        printf("serving command JOIN\n");
        new_message->type = JOIN;
        command_handle_join(existing_client, new_message);
    }
    else if(strcmp(type, "NEW_SESS") == 0){
        //create a new session
        printf("serving command NEW_SESS\n");
        new_message->type = NEW_SESS;
        command_handle_new_sess(existing_client, new_message);
    }
    else if(strcmp(type, "LEAVE_SESS") == 0){
        //leave the session
        printf("serving command LEAVE_SESS\n");
        new_message->type = LEAVE_SESS;
        command_handle_leave_sess(existing_client, new_message);
    }
    else if(strcmp(type, "QUERY") == 0){
        //get the queries
        printf("serving command QUERY\n");
        new_message->type = QUERY;
        command_handle_query(existing_client, new_message);
    }
    else if(strcmp(type, "MESSAGE") == 0){
        printf("serving command MESSAGE\n");
        new_message->type = MESSAGE;
        command_handle_message(existing_client, new_message);
    }
    else if(strcmp(type, "LOGIN") == 0){
        printf("serving command LOGIN\n");
        new_message->type = LOGIN;
        command_handle_login(existing_client, new_message);
    }
    else if(strcmp(type, "INVITE") == 0){
        printf("serving command INVITE\n");
        new_message->type = INVITE;
        command_handle_invite(existing_client, new_message);
    }
    else {
        printf("unknown command type=%s\n", type);
        command_handle_unknown(existing_client, new_message);
    }

	printf("done\n");
    
    memset(read_buffer, 0, sizeof(read_buffer));
    
    return;
    
}

// Command Handlers
void command_handle_exit(struct client_node *existing_client, struct message *new_message){
	
	char client_look_id[MAX_NAME];
	bzero((char*)&client_look_id, sizeof(client_look_id));
	strcpy(client_look_id, existing_client->client_id);

	char session_look_id[MAX_NAME];
	bzero((char*)&session_look_id, sizeof(session_look_id));
	strcpy(session_look_id, existing_client->session_id);
    
	struct client_node* exited_client = remove_client_from_client_list(client_look_id);

	struct client_node* removed_client;
	if( strcmp(session_look_id, "") != 0){
        removed_client = remove_client_from_session(session_look_id, client_look_id);
    }

    close(exited_client->client_socket);
    return;
    
}

void command_handle_join(struct client_node *existing_client, struct message *new_message){
    
    if(strcmp(existing_client->session_id, "") != 0){
        // the user is already in a session
        char response_buffer[MAX_SEND];
        memset(response_buffer, 0, sizeof(response_buffer));
        char data[MAXSIZE];
        memset(data, 0, sizeof(data));
        strcpy(data, "user already in session");
        int data_size = strlen(data);
        sprintf(response_buffer, "JN_NAK,%d,SERVER,%s",data_size,data);
        send(existing_client->client_socket, response_buffer, strlen(response_buffer), 0);
        return;
    }
    
    //new_message->data is the id of the session
    struct session_node *found_ses = find_session_from_session_list(new_message->data);
    
    if(found_ses == NULL){
        //can not find session
        char response_buffer[MAX_SEND];
        memset(response_buffer, 0, sizeof(response_buffer));
        char data[MAXSIZE];
        memset(data, 0, sizeof(data));
        strcpy(data, "session not found");
        int data_size = strlen(data);
        sprintf(response_buffer, "JN_NAK,%d,SERVER,%s",data_size,data);
        send(existing_client->client_socket, response_buffer, strlen(response_buffer), 0);
        return;
    }
    
    //all good, add the user to the session_list
    //update the user's session_id on the client_list
    strcpy(existing_client->session_id, found_ses->session_id);
    
    assert(strcmp(found_ses->session_id, new_message->data) == 0);
    
    //set up the new client object
    struct client_node *new_client_for_session_list = duplicate_client(existing_client);
    
    
    //add the set up client to the session list
    insert_client_to_session_list(found_ses, new_client_for_session_list);
    
    char response_buffer[MAX_SEND];
    memset(response_buffer, 0, sizeof(response_buffer));
    char data[MAXSIZE];
    memset(data, 0, sizeof(data));
    strcpy(data, found_ses->session_id);
    int data_size = strlen(data);
    sprintf(response_buffer, "JN_ACK,%d,SERVER,%s",data_size,data);
    send(existing_client->client_socket, response_buffer, strlen(response_buffer), 0);
    return;
    
}

void command_handle_new_sess(struct client_node *existing_client, struct message *new_message){
    
    struct session_node *found_sess = find_session_from_session_list(new_message->data);
    
    if(found_sess != NULL){
        //there is already a session with given id
        char response_buffer[MAX_SEND];
        memset(response_buffer, 0, sizeof(response_buffer));
        char data[MAXSIZE];
        memset(data, 0, sizeof(data));
        strcpy(data, "session name already exists");
        int data_size = strlen(data);
        sprintf(response_buffer, "NS_NAK,%d,SERVER,%s",data_size,data);
        send(existing_client->client_socket, response_buffer, strlen(response_buffer), 0);
        return;
    }
    
    //create the new sess
    struct session_node *new_sess;
    new_sess = (struct session_node*)malloc(sizeof(struct session_node));
    memset(new_sess->session_id, 0, sizeof(new_sess->session_id));
    strcpy(new_sess->session_id, new_message->data);
    new_sess->ses_client_list = NULL;
    new_sess->next = NULL;
    
    
    if(session_list.head == NULL){
        //empty session_list
        session_list.head = new_sess;
    }else{
        struct session_node *cur = session_list.head;
        while(cur->next != NULL){
            cur = cur->next;
        }
        cur->next = new_sess;
    }
    
    char response_buffer[MAX_SEND];
    memset(response_buffer, 0, sizeof(response_buffer));
    sprintf(response_buffer, "NS_ACK,%d,SERVER",0);
    send(existing_client->client_socket, response_buffer, strlen(response_buffer), 0);
    return;
     
}

void command_handle_leave_sess(struct client_node *existing_client, struct message *new_message){
    
    if(strcmp(existing_client->session_id, "") == 0){
        //the user is not in a session
        return;
    }
    
    struct client_node *removed_client = remove_client_from_session(existing_client->session_id, existing_client->client_id);
    
    struct session_node *found_sess = find_session_from_session_list(existing_client->session_id);
    
    struct client_node* cur_cli = found_sess->ses_client_list;
    while(cur_cli != NULL){
        cur_cli = cur_cli->next;
    }
    
    remove_session_id_from_client_in_client_list(existing_client);
    return;
    
}

void command_handle_query(struct client_node *existing_client, struct message *new_message){
    
    char users[MAXSIZE];
    memset(users, 0, sizeof (users));
    strcpy(users, "ONLINE_USERS:");
    char sessions[MAXSIZE];
    memset(sessions, 0, sizeof (sessions));
    strcpy(sessions, "ONLINE_SESSIONS:");
    
    struct client_node *cur_cli = client_list.head;
    if(cur_cli != NULL){ 
        while(cur_cli != NULL){
            strcat(users, cur_cli->client_id);
            strcat(users, "|");
            cur_cli = cur_cli->next;
       }
    }
    
    struct session_node *cur_sess = session_list.head;
    if(cur_sess != NULL){
        while(cur_sess != NULL){
	strcat(sessions, cur_sess->session_id);
	strcat(sessions, ":");
		struct client_node *cur = cur_sess->ses_client_list;
		while(cur != NULL){
			strcat(sessions, cur->client_id);
			strcat(sessions, ",");
			cur = cur->next;
		}
            
            strcat(sessions, "|");
            cur_sess = cur_sess->next;
        }
    }
    
    strcat(users, sessions);
    
    char response_buffer[MAX_SEND];
    memset(response_buffer, 0, sizeof(response_buffer));
    char data[MAXSIZE];
    memset(data, 0, sizeof(data));
    strcpy(data, users);
    int data_size = strlen(data);
    sprintf(response_buffer, "QU_ACK,%d,SERVER,%s",data_size,data);
    send(existing_client->client_socket, response_buffer, strlen(response_buffer), 0);
    return;
    
}

void command_handle_message(struct client_node *existing_client, struct message *new_message){
     
    if(strcmp(existing_client->session_id, "") == 0){
        //the user is not in a session
        return;
    }
    
    struct session_node* found_sess = find_session_from_session_list(existing_client->session_id);
    
    if(found_sess == NULL){
        return;
    }
    
    struct client_node* cur_cli = found_sess->ses_client_list;
    
    while(cur_cli != NULL ){
		if(strcmp(cur_cli->client_id, existing_client->client_id) != 0){
		    char response_buffer[MAX_SEND];
		    memset(response_buffer, 0, sizeof(response_buffer));
		    char message[MAXSIZE];
		    memset(message, 0, sizeof (message));
		    strcpy(message, existing_client->client_id);
		    strcat(message, ": ");
		    strcat(message, new_message->data);
		    int data_size = strlen(message);
		    sprintf(response_buffer, "MESSAGE,%d,SERVER,%s",data_size, message);
		    send(cur_cli->client_socket, response_buffer, strlen(response_buffer), 0);
		}
        
        cur_cli = cur_cli->next;
    }
    
    return;
}

void command_handle_login(struct client_node *existing_client, struct message *new_message){
    
    char response_buffer[MAX_SEND];
    memset(response_buffer, 0, sizeof(response_buffer));
    char data[MAXSIZE];
    memset(data, 0, sizeof(data));
    strcpy(data, "user already logged in");
    int data_size = strlen(data);
    sprintf(response_buffer, "LO_NAK,%d,SERVER,%s",data_size,data);
    send(existing_client->client_socket, response_buffer, strlen(response_buffer), 0);
    return;
    
}

void command_handle_invite(struct client_node *existing_client, struct message *new_message){
    
    char *invite_username;
    char *invite_sess;
    invite_sess = strtok(new_message->data, ",");
    invite_username = strtok(NULL, ",");
    
    
    
    if(strcmp(existing_client->client_id, invite_username) == 0){
        //trying to invite himself
        char response_buffer[MAX_SEND];
        memset(response_buffer, 0, sizeof(response_buffer));
        char data[MAXSIZE];
        memset(data, 0, sizeof(data));
        strcpy(data, "you can not invite yourself");
        int data_size = strlen(data);
        sprintf(response_buffer, "INV_NAK,%d,SERVER,%s",data_size,data);
        send(existing_client->client_socket, response_buffer, strlen(response_buffer), 0);
        return;
    }
    
    struct session_node* found_ses = find_session_from_session_list(invite_sess);
    
    if(found_ses == NULL){
        //session does not exist
        char response_buffer[MAX_SEND];
        memset(response_buffer, 0, sizeof(response_buffer));
        char data[MAXSIZE];
        memset(data, 0, sizeof(data));
        strcpy(data, "session not found");
        int data_size = strlen(data);
        sprintf(response_buffer, "INV_NAK,%d,SERVER,%s",data_size,data);
        send(existing_client->client_socket, response_buffer, strlen(response_buffer), 0);
        return;
    }
    
    struct client_node* cur_cli = client_list.head;
    while(cur_cli != NULL ){
        if(strcmp(cur_cli->client_id, invite_username) == 0){
            
            if(strcmp(cur_cli->session_id, "") != 0){
                //couldn't find the user
                char response_buffer[MAX_SEND];
                memset(response_buffer, 0, sizeof(response_buffer));
                char data[MAXSIZE];
                memset(data, 0, sizeof(data));
                strcpy(data, "user is already in a session");
                int data_size = strlen(data);
                sprintf(response_buffer, "INV_NAK,%d,SERVER,%s",data_size,data);
                send(existing_client->client_socket, response_buffer, strlen(response_buffer), 0);
            }
            
            char response_buffer[MAX_SEND];
		    memset(response_buffer, 0, sizeof(response_buffer));
		    int data_size = strlen(invite_sess);
		    sprintf(response_buffer, "INVITATION,%d,%s,%s",data_size,invite_username,invite_sess);
		    send(cur_cli->client_socket, response_buffer, strlen(response_buffer), 0);
            return;
        }
        cur_cli = cur_cli->next;
    }
    
    //couldn't find the user
    char response_buffer[MAX_SEND];
    memset(response_buffer, 0, sizeof(response_buffer));
    char data[MAXSIZE];
    memset(data, 0, sizeof(data));
    strcpy(data, "user not found");
    int data_size = strlen(data);
    sprintf(response_buffer, "INV_NAK,%d,SERVER,%s",data_size,data);
    send(existing_client->client_socket, response_buffer, strlen(response_buffer), 0);
    return;
    
}

void command_handle_unknown(struct client_node *existing_client, struct message *new_message){
    char response_buffer[MAX_SEND];
    memset(response_buffer, 0, sizeof(response_buffer));
    char data[MAXSIZE];
    memset(data, 0, sizeof(data));
    strcpy(data, "unknown error");
    int data_size = strlen(data);
    sprintf(response_buffer, "NAK,%d,SERVER,%s",data_size,data);
    send(existing_client->client_socket, response_buffer, strlen(response_buffer), 0);
    return;
}

// Buffer Reader Helper
int bufferReader(char *buffer, char *type, char *source, char *data){
    // Parsing the received packet
        
        char cSize[MAX_NAME];
        
        int dataLength;
        
        memset(cSize, 0, sizeof(cSize));
        int colon_count = 0;
        int last_colon = 0;
        int i;
        for(i=0; i< strlen(buffer); i++){
            if (buffer[i] == ','){
                if(colon_count == 0){
                    memcpy(type, buffer, i);
                }
                else if(colon_count == 1){
                    memcpy(cSize, (buffer + last_colon + 1), (i - last_colon - 1));
                }
                else if(colon_count == 2){
                    memcpy(source, (buffer + last_colon + 1), (i - last_colon - 1));
                    i++;
                    break;
                }
                colon_count++;
                last_colon = i;
            }
        }
        if(type == NULL || cSize == NULL || source == NULL ){
            printf("error reading the response from buffer reader\n");
            return 0;
        }
        dataLength = atoi(cSize);
        if(dataLength < 0 ){
            printf("error with the data size, buffer reader\n");
            return 0;
        }
        memcpy(data, (buffer + i), dataLength);
        if(data == NULL){
            printf("error reading the data from buffer reader\n");
            return 0;
        }
        return dataLength;
        
}

// client_list Helpers
void insert_client_to_client_list(struct client_node *new_client){
    
    if(client_list.head == NULL){
        client_list.head = new_client;
        return;
    }
    
    struct client_node *cur = client_list.head;
    while(cur->next != NULL){
        cur = cur->next;
    }
    
    cur->next = new_client;
    return;
    
}

struct client_node* remove_client_from_client_list(char *client_id){

	struct client_node *cur = client_list.head;
    	struct client_node *pre = NULL;
	while(cur != NULL && strcmp(cur->client_id, client_id) != 0){
		pre=cur;
        cur=cur->next;
	}

	if(cur == NULL) return NULL;
	if(pre == NULL) client_list.head = cur->next;
	else
		pre->next = cur->next;

	cur->next = NULL;
	return cur;
    
}

void remove_session_id_from_client_in_client_list(struct client_node *found_client){
    
    if(client_list.head == NULL){
        return;
    }
    
    struct client_node *cur = client_list.head;
    while(cur != NULL){
        if(strcmp(cur->client_id, found_client->client_id) == 0){
            memset(cur->session_id, 0, sizeof(cur->session_id));
            return;
        }
	cur = cur->next;
    }
    
}

struct client_node* duplicate_client(struct client_node *_client){
    //set up the new client object
    struct client_node *new_client;
    new_client = (struct client_node*) malloc(sizeof(struct client_node));
    memset(new_client->client_id, 0, sizeof(new_client->client_id));
    memset(new_client->pass, 0, sizeof(new_client->pass));
    memset(new_client->session_id, 0, sizeof(new_client->session_id));
    
    strcpy(new_client->client_id, _client->client_id);
    strcpy(new_client->pass, _client->pass);
    strcpy(new_client->session_id, _client->session_id);
    new_client->client_socket = _client->client_socket;
    new_client->next = NULL;
    
    return new_client;
}


// session_list Helpers
void insert_session_to_session_list(struct session_node *new_session){

    if(session_list.head == NULL){
        session_list.head = new_session;
        return;
    }
    
    struct session_node *cur = session_list.head;
    while(cur->next != NULL){
        cur = cur->next;
    }
    
    cur->next = new_session;
    return;
    
}

struct session_node* remove_session_from_session_list(char *session_id){
    
	struct session_node *cur = session_list.head;
    	struct session_node *pre = NULL;
	while(cur != NULL && strcmp(cur->session_id, session_id) != 0){
		pre=cur;
        cur=cur->next;
	}

	if(cur == NULL) return NULL;
	if(pre == NULL) session_list.head = cur->next;
	else
		pre->next = cur->next;

	cur->next = NULL;
	return cur;    

}

struct client_node* remove_client_from_session(char *session_id, char *client_id){

	struct session_node *cur_ses = session_list.head;
	struct session_node *pre_ses = NULL;
	struct session_node *found_ses = NULL;
	while(cur_ses != NULL && strcmp(cur_ses->session_id, session_id) != 0){
		pre_ses=cur_ses;
        cur_ses=cur_ses->next;
	}

	if(cur_ses == NULL) return NULL;

	found_ses = cur_ses;
    
    
    struct client_node *pre_cli = NULL;
    struct client_node *cur_cli = found_ses->ses_client_list;
    
    while(cur_cli != NULL && strcmp(cur_cli->client_id, client_id) != 0){
		pre_cli=cur_cli;
        cur_cli=cur_cli->next;
	}

	if(cur_cli == NULL) return NULL;
	if(pre_cli == NULL) found_ses->ses_client_list = cur_cli->next;
	else
		pre_cli->next = cur_cli->next;
    

	cur_cli->next = NULL;
	return cur_cli;
    
}

struct session_node* find_session_from_session_list(char *session_id){
    
    if(session_list.head == NULL){
        return NULL;
    }
    
    struct session_node *cur_ses = session_list.head;
    while(cur_ses != NULL){
        if(strcmp(cur_ses->session_id, session_id) == 0){
            return cur_ses;
        }
        cur_ses = cur_ses->next;
    }
    
    return NULL;
    
}

void insert_client_to_session_list(struct session_node* session, struct client_node* new_client){
    
    struct client_node *cur = session->ses_client_list;
    
    if(cur == NULL){
        //ses_client_list is empty
        session->ses_client_list = new_client;
        return;
    }
    
    while(cur->next != NULL){
        cur = cur->next;
    }
    
    cur->next = new_client;
    
}
