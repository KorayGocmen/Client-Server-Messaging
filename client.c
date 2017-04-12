/* 
 * File:   client.c
 * Author: guanjing
 *
 * Created on March 18, 2017, 5:21 PM
 */
#define MAXSIZE 1024
#define MAX_DATA 1024
#define MAX_NAME 128
#define MAX_CONNECTION 10
#define MAX_SEND 2048

#include <stdio.h>
#include <stdlib.h>
#include <string.h>    
#include <sys/socket.h>    
#include <arpa/inet.h> 
#include <inttypes.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

struct lab3message {
    unsigned int type;
    unsigned int size;
    unsigned char source[MAX_NAME];
    unsigned char data[MAX_DATA];
};
/*
 * 
 */
//-----------------------------function declaration-------------------------------
int bufferReader(char* buffer, char*type, char*source, char*data);
int authentication(const char* clientID, const char* password, const char* serverIP, const char* serverPort);
int logout(int socket, const char* clientID);
int joinsession(int socket, const char* clientID, char* session);
int leavesession(int socket, const char* clientID);
int createsession(int socket, const char*clientID, const char*session);
int list(int socket, const char*clientID);
int inviteGuest(int socket, const char*clientID, const char*session, const char*guest);
//--------------------------------------------------------------------------

int main(int argc, char** argv) {
    char* userInput;
    int inputSize = MAXSIZE;
    userInput = (char*) malloc(MAXSIZE);
    char*piece;
    piece = (char*) malloc(MAX_NAME);
    char*clientIDdya = (char *) malloc(MAX_NAME);  
    char*password = (char *) malloc(MAX_NAME);
    char*serverIP = (char *) malloc(MAX_NAME);
    char*serverPort = (char *) malloc(MAX_NAME);
    char*check = (char *) malloc(MAX_NAME);
    char*session = (char *) malloc(MAX_NAME);
    char*guest = (char *) malloc(MAX_NAME);
    int socket = 0;
    int connected = 0;
    int insession = 0;
    fd_set fds;
    int bytes_sent = 0;
    int error = 0;
    char type[MAX_NAME], source[MAX_NAME], data[MAX_NAME], servermsg[MAX_SEND], clientID[MAX_NAME], inv_res[MAX_NAME];
    struct lab3message msg;
    char*clientmsg = (char *) malloc(MAX_SEND);


    while (1) {

        if (connected == 0 || socket <= 0) {
            //read from user
            if (fgets(userInput, MAXSIZE, stdin) == NULL) {
                //skip the rest
                continue;
            } else {
                //padding '\0' to the end of the input
                inputSize = strlen(userInput);
                if (inputSize == MAXSIZE) {
                    printf("the UserInput is tooo long, please re-enter\n");
                    continue;
                } else {
                    userInput[inputSize - 1] = '\0';
                }
            }
            if (userInput[0] != '/') {
                printf("you must login first\n");
                continue;
            }
            //PARSING commands
            piece = strtok(userInput, " ");
            if (piece == NULL) {
                printf("error getting command\n");
                continue;
            }
          
            //--------------------login------------------------
            if (strcmp(piece, "/login") == 0) {
                if (connected == 1) {
                    printf("you are already connected!\n");
                    continue;
                }
                //ClientID
                clientIDdya = strtok(NULL, " ");
                if (clientIDdya == NULL) {
                    printf("error getting clientID\n");
                    continue;
                }
		strcpy(clientID, clientIDdya);
                //password
                password = strtok(NULL, " ");
                if (password == NULL) {
                    printf("error getting password\n");
                    continue;
                }
                //serverIP
                serverIP = strtok(NULL, " ");
                if (serverIP == NULL) {
                    printf("error getting serverIP\n");
                    continue;
                }

                //serverPort
                serverPort = strtok(NULL, " ");
                if (serverPort == NULL) {
                    printf("error getting serverPort\n");
                    continue;
                }

                //check if correct format of input
                check = strtok(NULL, " ");
                if (check != NULL) {
                    printf("too much inputs for login\n");
                    continue;
                }

                socket = authentication(clientID, password, serverIP, serverPort);
                if (socket <= 0) {
                    printf("error with authentication!\n");
                    continue;
                } else {
                    connected = 1;
                }

                //--------------quit-----------------------    
            } else if (strcmp(piece, "/quit") == 0) {
                printf("terminating...\n");
                break;
            } else if (connected == 0) {
                printf("please login first!\n");
                continue;
            }
        } else {
            FD_ZERO(&fds);
            FD_SET(0, &fds);
            FD_SET(socket, &fds);

            error = select(socket + 1, &fds, NULL, NULL, NULL);
            if (error < 0) {
                printf("error in IO multiplexing\n");
                return 0;
            } else if (error == 0) {
                continue;
                //----------------server sending message-------------
            } else if (FD_ISSET(socket, &fds)) {
                bzero((char*) &servermsg, sizeof (servermsg));
                error = recv(socket, servermsg, MAX_SEND, 0);
                if (error <= 0) {
                    printf("msg: error receiving from server\n");
                    return -1;
                }
                bzero((char*) &data, sizeof (data));
                bzero((char*) &source, sizeof (source));
                bzero((char*) &type, sizeof (type));
                bzero((char*) &inv_res, sizeof (inv_res));
                error = bufferReader(servermsg, type, source, data);
                if (error < 0) {
                    printf("error reading message from server!\n");
                    continue;
                }
                if (strcmp(type, "MESSAGE") == 0) {
                    printf("Receiving from server: %s\n", data);
                    continue;
                } else if(strcmp(type, "INVITATION") == 0){
                    printf("User %s invite you to join session %s, do you want to accept? y/n \n", source, data);
                    while(fgets(inv_res, MAXSIZE, stdin) == NULL) {
                        printf("please give a response\n");
                    }
                    if(strcmp(inv_res, "y\n") == 0 && data != NULL ){
                        if(joinsession(socket, clientID, data) == 0){
                            insession = 1;
                        }
                        continue;	
                    }else if(strcmp(inv_res, "n\n") == 0){
                        continue;
                    }else{
                        printf("wrong response, if you wish to join in the future,please use the /joinsession command!\n");
                        continue;
                    }  
                } else {
                    printf("Receive message error\n");
                    continue;
                }
                //---------------user typing---------------------
            } else if (FD_ISSET(0, &fds)) {
                if (fgets(userInput, MAXSIZE, stdin) == NULL) {
                    //skip the rest
                    continue;
                } else {
                    //padding '\0' to the end of the input
                    inputSize = strlen(userInput);
                    if (inputSize == MAXSIZE) {
                        printf("the UserInput is tooo long, please re-enter\n");
                        continue;
                    } else {
                        userInput[inputSize - 1] = '\0';
                    }
                }

                //-------------------commands--------------------
                if (userInput[0] == '/') {
                    piece = strtok(userInput, " ");
                    if (piece == NULL) {
                        printf("error getting command\n");
                        continue;
                    }

                    //-----------connected and not a log in/quit command-------------------
                    if (strcmp(piece, "/logout") == 0) {
                        if (connected == 0) {
                            printf("you are not logged in!\n");
                        } else if ((check = strtok(NULL, " ")) != NULL) {
                            printf("too much inputs for login\n");
                            continue;
                        } else {
                            if (logout(socket, clientID) < 0) {
                                printf("error logging out\n");
                            } else {
                                printf("logging out..\n");
                                connected = 0;
                                insession = 0;
                            }
                        }
                        //----------------join session-----------	
                    } else if (strcmp(piece, "/joinsession") == 0) {
                        session = strtok(NULL, " ");
                        if (session == NULL || (session[0] == '\0')) {
                            printf("JoinSession: error getting session number\n");
                            continue;
                        }
                        check = strtok(NULL, " ");
                        if (check != NULL) {
                            printf("JoinSession: too much input arguments!\n");
                            continue;
                        }
                        if (joinsession(socket, clientID, session) == 0) {
                            insession = 1;
                        }
                        continue;

                        //-----------	
                    } else if (strcmp(piece, "/leavesession") == 0) {

                        check = strtok(NULL, " ");
                        if (check != NULL) {
                            printf("Leave Session: too much input arguments!\n");
                            continue;
                        }

                        if (leavesession(socket, clientID) == 0) {
                            insession = 0;
                        }
                        continue;

                    } else if (strcmp(piece, "/createsession") == 0) {
                        session = strtok(NULL, " ");
                        if (session == NULL || (session[0] == '\0')) {
                            printf("Create Session: error getting session number\n");
                            continue;
                        }
                        check = strtok(NULL, " ");
                        if (check != NULL) {
                            printf("Create Session: too much input arguments!\n");
                            continue;
                        }
                        createsession(socket, clientID, session);
                        continue;
                    } else if (strcmp(piece, "/list") == 0) {
                        check = strtok(NULL, " ");
                        if (check != NULL) {
                            printf("Create Session: too much input arguments!\n");
                            continue;
                        }
                        list(socket, clientID);
                        continue;
                    }else if (strcmp(piece, "/quit") == 0) {
                        printf("terminating...\n");
                        close(socket);
                        break;
                    }else if(strcmp(piece, "/invite") == 0){
                        guest = strtok(NULL, " ");
                        if(guest == NULL || guest[0] == '\0'){
                            printf("invite: error getting guest ID!\n");
                            continue;
                        }
                        if(session == NULL || insession == 0){
                            printf("you have to be in a session to invite someone!\n");
                            continue;
                        }
                        inviteGuest(socket, clientID, session, guest);
                        continue;
                    }else{
                        printf("wrong commands\n");
                        continue;
                    }
                    //-----------------user sending msg------------
                } else {
                    if (insession == 0) {
                        printf("you must join in a session first\n");
                        continue;
                    } else {
                        memset(msg.source, 0, sizeof (msg.source));
			            memset(msg.data, 0, sizeof (msg.data));
                        msg.type = 8;
                        strcpy(msg.source, clientID);
                        strcpy(msg.data, userInput);
                        msg.size = strlen(msg.data);
                        bzero((char*) &servermsg, sizeof (servermsg));
                        sprintf(servermsg, "MESSAGE,%u,%s,%s", msg.size, msg.source, msg.data);
                        bytes_sent = send(socket, servermsg, strlen(servermsg), 0);
                        if (bytes_sent < 0) {
                            printf("MSG: error sending msg to server\n");
                            return -1;
                        }
                        continue;
                    }
                }
            }
        }
    }
    return (EXIT_SUCCESS);
}

int inviteGuest(int socket, const char*clientID, const char*session, const char*guest){
    struct lab3message invite;
    char buff[MAX_SEND];
    int bytes_sent, error;
    memset(invite.source, 0, sizeof (invite.source));
    memset(invite.data, 0, sizeof (invite.data));
    invite.type = 7;
    strcpy(invite.source, clientID);
    bzero((char*)&buff, sizeof(buff));
    sprintf(buff, "%s,%s", session, guest);
    strcpy(invite.data, buff);
    invite.size = strlen(invite.data);
    bzero((char*) &buff, sizeof (buff));
    sprintf(buff, "INVITE,%u,%s,%s", invite.size, invite.source, invite.data);
    bytes_sent = send(socket, buff, strlen(buff), 0);
    if (bytes_sent < 0) {
        printf("Create: error sending msg to server\n");
        return -1;
    }

    bzero((char*) &buff, sizeof (buff));
    error = recv(socket, buff, MAX_SEND, 0);
    if (error <= 0) {
        printf("Create Session: error receiving from server\n");
        return -1;
    }

    bytes_sent = strlen(buff);
    if (buff[bytes_sent - 1] == '\n') {
        buff[bytes_sent - 1] == '\0';
    }

    char type[MAX_NAME], source[MAX_NAME], data[MAX_NAME];
    memset(type, 0, sizeof (type));
    memset(source, 0, sizeof (source));
    memset(data, 0, sizeof (data));
    error = bufferReader(buff, type, source, data);
    if (error < 0) {
        printf("error reading buffer from server!\n");
        return -1;
    }
    if (strcmp(type, "INV_ACK") == 0) {
        printf("Invite client successful!\n");
        return 0;
    } else if(strcmp(type, "INV_NAK") == 0){
        printf("Invite client error: %s\n", data);
        return -1;
    }
    return 0;
}

int list(int socket, const char*clientID) {
    struct lab3message list;
    char buff[MAX_SEND];
    int bytes_sent, error;
    memset(list.source, 0, sizeof (list.source));
    list.type = 6;
    strcpy(list.source, clientID);
    list.size = 0;
    bzero((char*) &buff, sizeof (buff));
    sprintf(buff, "QUERY,%u,%s", list.size, list.source);
    bytes_sent = send(socket, buff, strlen(buff), 0);
    if (bytes_sent < 0) {
        printf("List: error sending msg to server\n");
        return -1;
    }

    bzero((char*) &buff, sizeof (buff));
    error = recv(socket, buff, MAX_SEND, 0);
    if (error <= 0) {
        printf("List: error receiving from server\n");
        return -1;
    }

    bytes_sent = strlen(buff);
    if (buff[bytes_sent - 1] == '\n') {
        buff[bytes_sent - 1] == '\0';
    }

    char type[MAX_NAME], source[MAX_NAME], data[MAX_NAME];
    memset(type, 0, sizeof (type));
    memset(source, 0, sizeof (source));
    memset(data, 0, sizeof (data));
    error = bufferReader(buff, type, source, data);
    if (error < 0) {
        printf("error reading buffer from server!\n");
        return -1;
    }
    if (strcmp(type, "QU_ACK") == 0) {
        printf("List successful: %s\n", data);
        return 0;
    } else {
        printf("List error\n");
        return -1;
    }
    return 0;
}

int createsession(int socket, const char*clientID, const char*session) {
    struct lab3message create;
    char buff[MAX_SEND];
    int bytes_sent, error;
    memset(create.source, 0, sizeof (create.source));
    memset(create.data, 0, sizeof (create.data));
    create.type = 5;
    strcpy(create.source, clientID);
    strcpy(create.data, session);
    create.size = strlen(create.data);
    bzero((char*) &buff, sizeof (buff));
    sprintf(buff, "NEW_SESS,%u,%s,%s", create.size, create.source, create.data);
    bytes_sent = send(socket, buff, strlen(buff), 0);
    if (bytes_sent < 0) {
        printf("Create: error sending msg to server\n");

        return -1;
    }

    bzero((char*) &buff, sizeof (buff));
    error = recv(socket, buff, MAX_SEND, 0);
    if (error <= 0) {
        printf("Create Session: error receiving from server\n");

        return -1;
    }

    bytes_sent = strlen(buff);
    if (buff[bytes_sent - 1] == '\n') {
        buff[bytes_sent - 1] == '\0';
    }

    char type[MAX_NAME], source[MAX_NAME], data[MAX_NAME];
    memset(type, 0, sizeof (type));
    memset(source, 0, sizeof (source));
    memset(data, 0, sizeof (data));
    
    error = bufferReader(buff, type, source, data);
    if (error < 0) {
        printf("error reading buffer from server!\n");
        return -1;
    }
    if (strcmp(type, "NS_ACK") == 0) {
        printf("Create session successful!\n");
        return 0;
    } else {
        printf("Create Session error\n");
        return -1;
    }
    return 0;
}

int leavesession(int socket, const char* clientID) {
    struct lab3message leave;
    char buff[MAX_SEND];
    int bytes_sent, error;
    memset(leave.source, 0, sizeof (leave.source));
    leave.type = 4;
    strcpy(leave.source, clientID);
    leave.size = 0;
    bzero((char*) &buff, sizeof (buff));
    sprintf(buff, "LEAVE_SESS,%u,%s", leave.size, leave.source);
    bytes_sent = send(socket, buff, strlen(buff), 0);
    if (bytes_sent < 0) {
        printf("Join: error sending msg to server\n");
        return -1;
    }
    return 0;
}

int joinsession(int socket, const char* clientID, char* session) {
    struct lab3message join;
    char buff[MAX_SEND];
    int bytes_sent, error;
    memset(join.source, 0, sizeof (join.source));
    memset(join.data, 0, sizeof (join.data));
    join.type = 3;
    strcpy(join.source, clientID);
    strcpy(join.data, session);
    join.size = strlen(join.data);
    bzero((char*) &buff, sizeof (buff));
    sprintf(buff, "JOIN,%u,%s,%s", join.size, join.source, join.data);
    bytes_sent = send(socket, buff, strlen(buff), 0);
    if (bytes_sent < 0) {
        printf("Join: error sending msg to server\n");
        return -1;
    }

    bzero((char*) &buff, sizeof (buff));
    error = recv(socket, buff, MAX_SEND, 0);
    if (error <= 0) {
        printf("JoinSession: error receiving from server\n");
        return -1;
    }

    bytes_sent = strlen(buff);
    if (buff[bytes_sent - 1] == '\n') {
        buff[bytes_sent - 1] == '\0';
    }

    char type[MAX_NAME], source[MAX_NAME], data[MAX_NAME];
    memset(type, 0, sizeof (type));
    memset(source, 0, sizeof (source));
    memset(data, 0, sizeof (data));
    error = bufferReader(buff, type, source, data);
    if (error < 0) {
        printf("error reading buffer from server!\n");
        return -1;
    }
    if (strcmp(type, "JN_ACK") == 0) {
        printf("join session successful!\n");
        return 0;
    } else if (strcmp(type, "JN_NAK") == 0) {
        printf("join session unsuccessful,reason:%s\n", data);
        return -1;
    } else {
        printf("Join Session error\n");
        return -1;
    }
    return 0;

}

int logout(int socket, const char* clientID) {
    struct lab3message logout;
    char buff[MAX_SEND];
    int bytes_sent, error;
    memset(logout.source, 0, sizeof (logout.source));
    logout.type = 2;
    strcpy(logout.source, clientID);
    logout.size = 0;
    bzero((char*) &buff, sizeof (buff));
    sprintf(buff, "EXIT,%u,%s", logout.size, logout.source);
    bytes_sent = send(socket, buff, strlen(buff), 0);
    if (bytes_sent < 0) {
        printf("Exit: error sending msg to server\n");
        return -1;
    }
    return 0;
}

int authentication(const char* clientID, const char* password, const char* serverIP, const char* serverPort) {
    int servPort = strtoimax(serverPort, NULL, 10);
    if ( servPort < 0 || servPort > 65535) {
        printf("wrong server port\n");
        return -1;
    }

    int welSock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server;
    if (welSock == -1) {
        printf("error creating socket\n");
        return -1;
    }

    server.sin_addr.s_addr = inet_addr(serverIP);
    server.sin_family = AF_INET;
    server.sin_port = htons(servPort);
    if (gethostbyname(serverIP) == NULL) {
        printf("error with server IP,can't find server name\n");
        return -1;
    }

    //connect to server
    if (connect(welSock, (struct sockaddr *) & server, sizeof (server)) < 0) {
        printf("error connecting with server\n");
        return -1;
    }

    //authentication
    struct lab3message auth;
    char buff[MAX_SEND];
    int bytes_sent, error;
    memset(auth.source, 0, sizeof (auth.source));
	memset(auth.data, 0, sizeof (auth.data));
	//memset(auth.source, 0, sizeof (auth.source));
    auth.type = 1;
    strcpy(auth.source, clientID);
    bzero((char*) &buff, sizeof (buff));
    sprintf(buff, "%s,%s", clientID, password);
    strcpy(auth.data, buff);
    auth.size = strlen(buff);
    bzero((char*) &buff, sizeof (buff));
    sprintf(buff, "LOGIN,%u,%s,%s", auth.size, auth.source, auth.data);
    bytes_sent = send(welSock, buff, strlen(buff), 0);
    if (bytes_sent < 0) {
        printf("Login: error sending msg to server\n");
        close(welSock);
        return -1;
    }
    //keep sending rest of the msg
    bzero((char*) &buff, sizeof (buff));
    error = recv(welSock, buff, MAX_SEND, 0);
    if (error <= 0) {
        printf("Login: error receiving from server\n");
        close(welSock);
        return -1;
    }

    bytes_sent = strlen(buff);
    if (buff[bytes_sent - 1] == '\n') {
        buff[bytes_sent - 1] == '\0';
    }

    char type[MAX_NAME], source[MAX_NAME], data[MAX_NAME];
    memset(type, 0, sizeof (type));
    memset(source, 0, sizeof (source));
    memset(data, 0, sizeof (data));
    error = bufferReader(buff, type, source, data);
    if (error < 0) {
        printf("error reading buffer from server!\n");
        close(welSock);
        return -1;
    }

    if (strcmp(type, "LO_ACK") == 0) {
        printf("login successful!\n");
        return welSock;
    } else if (strcmp(type, "LO_NAK") == 0) {
        printf("login unsuccessful,reason:%s\n", data);
        close(welSock);
        return -1;
    } else {
        printf("Login error\n");
        close(welSock);
        return -1;
    }

}

int bufferReader(char* buffer, char*type, char*source, char*data) {
    // Parsing the received packet

    char cSize[MAX_NAME];

    int dataLength;


    bzero((char *) &cSize, sizeof (cSize));

    int colon_count = 0;
    int last_colon = 0;
    int i;
    for (i = 0; i < strlen(buffer); i++) {
        if (buffer[i] == ',') {
            if (colon_count == 0) {
                memcpy(type, buffer, i);
            } else if (colon_count == 1) {
                memcpy(cSize, (buffer + last_colon + 1), (i - last_colon - 1));
            } else if (colon_count == 2) {
                memcpy(source, (buffer + last_colon + 1), (i - last_colon - 1));
                i++;
                break;
            }
            colon_count++;
            last_colon = i;
        }
    }
    if (type == NULL || cSize == NULL || source == NULL) {
        printf("error reading the response from server\n");
        return -1;
    }
    dataLength = atoi(cSize);
    if (dataLength < 0) {
        printf("error with the data size\n");
        return -1;
    }

    //data could be null
    if (dataLength > 0) {
        memcpy(data, (buffer + i), dataLength);
    }
    return dataLength;

}
