// CSCI 4780 Spring 2021 Project 1
// Jisoo Kim, Will McCarthy, Tyler Scalzo

#include <iostream>
#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fstream>
#include <pthread.h>

#define BUFFSIZE 1000
#define NPORT 1 // Nport is argInfo[1] (argv[2]).
#define TPORT 2 // TPORT is argInfo[2] (argv[3]).

using namespace std;

int msgLen = 0;
struct addrinfo cltAddr, *conInfo;
char msgBuf[BUFFSIZE]; // Buffer for messages.
char msgBufCpy[BUFFSIZE];
char *com; // For checking commands.
bool done = false; // If commands should still be taken.
char* argInfo[3]; // To make passing cli arguments easier for threads.

int setup(int port)
{
        int sockfd = 0, check = 0;
        struct addrinfo cltAddr, *conInfo;

        // Setting up the host's IP information.
        memset(&cltAddr, 0, sizeof cltAddr);
        cltAddr.ai_family = AF_INET;
        cltAddr.ai_socktype = SOCK_STREAM;
        cltAddr.ai_flags = AI_PASSIVE;

        // Getting connection information.
        getaddrinfo(argInfo[0], argInfo[port], &cltAddr, &conInfo);

        sockfd = socket(AF_INET, SOCK_STREAM, 0); // Getting socket fd.
        if (sockfd < 0)
        {
                cout << "Error creating Socket." << endl;
                exit(EXIT_FAILURE);
        }

        // Connecting to server.
        check = connect(sockfd, conInfo->ai_addr, conInfo->ai_addrlen);
        if (check < 0)
        {
                cout << "Error connecting to server." << endl;
                exit(EXIT_FAILURE);
        }

        return sockfd;
} //end setup()

void* terminate(void* arg)
{
        char* comID = com; // Copy of the comID for termination.
        int sockfd = setup(TPORT); // Socket FD for server terminate port.

        // Sending comID to server.
        // Server extracts digit from char* sent.
        int check = send(sockfd, comID, strlen(comID), 0);
        if (check < 0)
        {
                cout << "Error sending data." << endl;
                exit(EXIT_FAILURE);
        }

        close(TPORT);

        return NULL; // Don't need to return anything.
} // terminate()

void* bgThread(void* arg) // Thread for background transfers.
{       
        // Copying some variables.
        char bgMessage[BUFFSIZE];
        char bgCWD[BUFFSIZE];
        memset(bgMessage, 0, BUFFSIZE); 
        memset(bgCWD, 0, BUFFSIZE); 
        strcpy(bgMessage, msgBufCpy);
        strcpy(bgCWD, msgBuf);
        char* bgCom = strtok(bgMessage, " ");

        int sockfd = setup(NPORT); // Socket FD for server connection.

        // Sending bg to server.
        int check = send(sockfd, "bg", 2, 0);
        if (check < 0)
        {
                cout << "Error sending data." << endl;
                exit(EXIT_FAILURE);
        }

        // Sending command to server.
        check = send(sockfd, bgCom, strlen(bgCom), 0);
        if (check < 0)
        {
                cout << "Error sending data." << endl;
                exit(EXIT_FAILURE);
        }

        bgCom = strtok(NULL, " "); // File name.
        strcat(bgCWD, "/"); // To seperate the file name.
        strcat(bgCWD, bgCom); // Full file path.

        if ((strcmp(com, "get") == 0))
        {
                FILE *fp = fopen(bgCom, "w");
                
                check = send(sockfd, bgCWD, strlen(bgCWD), 0); //send file path to the server
                if(check < 0)
                {
                        cout << "Error sending data." << endl;
                        exit(EXIT_FAILURE);
                }

                // Start signal, and printing comID.
                memset(bgMessage, 0, BUFFSIZE);
                check = recv(sockfd, bgMessage, BUFFSIZE, 0);
                if (check < 0)
                {
                        cout << "Error receiving data." << endl;
                        exit(EXIT_FAILURE);
                }

                cout << "comID: " << bgMessage << endl;

                memset(bgMessage, 0, BUFFSIZE); // Resetting the buffer.
                bool end = false;
                // Receiving output from server.
                while (!end)
                {
                        memset(bgMessage, 0, BUFFSIZE);
                        check = recv(sockfd, bgMessage, BUFFSIZE, 0);
                        if (check < 0)
                        {
                                cout << "Error receiving data." << endl;
                                exit(EXIT_FAILURE);
                        }

                        if (strcmp(bgMessage, "invalid") == 0) // End of file.
                        {
                                end = true;
                                fclose(fp);
                        }
                        else if (strcmp(bgMessage, "term") == 0)
                        {
                                fclose(fp);
                                remove(bgCom); // Delete the partially received file.
                                end = true;
                        }
                        else // Write data to file.
                        {
                                fwrite(bgMessage, 1, check, fp);
                                fflush(fp);
                        }
                }
        }
        else // Only other option is put.
        {
                FILE *fp = fopen(bgCom, "r");
                memset(msgBuf, 0, BUFFSIZE);

                check = send(sockfd, bgCWD, strlen(bgCWD), 0); //send file name to the server
                if(check < 0)
                {
                        cout << "Error sending data." << endl;
                        exit(EXIT_FAILURE);
                }
                
                if(fp == NULL)
                {
                        cout << "File does not exist" << endl;
                }
                else
                {
                        bool term = false; // For terminate signal.

                        // Start signal, and printing comID.
                        memset(bgMessage, 0, BUFFSIZE);
                        check = recv(sockfd, bgMessage, BUFFSIZE, 0);
                        if (check < 0)
                        {
                                cout << "Error receiving data." << endl;
                                exit(EXIT_FAILURE);
                        }

                        cout << "comID: " << bgMessage << endl;

                        //keep sending buffers untill the end of file
                        while (!feof(fp) && !ferror(fp) && !term)
                        {
                                memset(bgMessage, 0, BUFFSIZE);
                                fread(bgMessage, BUFFSIZE, 1, fp);
                                check = send(sockfd, bgMessage, sizeof(bgMessage), 0);
                                if (check < 0)
                                {
                                        cout << "Error sending data." << endl;
                                        exit(EXIT_FAILURE);
                                }

                                memset(bgMessage, 0, BUFFSIZE);

                                // Checking for terminate signal.
                                check = recv(sockfd, bgMessage, BUFFSIZE, 0);
                                if (check < 0)
                                {
                                        cout << "Error receiving data." << endl;
                                        exit(EXIT_FAILURE);
                                }

                                if (strcmp(bgMessage, "term") == 0) // Transfer terminated.
                                {
                                        fclose(fp);
                                        term = true;
                                }
                        }

                        if (term != true)
                        {
                                //send end of file signal
                                memset(bgMessage, 0, BUFFSIZE);
                                strcat(bgMessage, "invalid");
                                sleep(1); // Fixes a desync issue on the client.
                                check = send(sockfd, bgMessage, sizeof(bgMessage), 0);
                                if (check < 0)
                                {
                                        cout << "Error sending data." << endl;
                                        exit(EXIT_FAILURE);
                                }
                        }
                }
        }

        // Sending bg to server.
        check = send(sockfd, "quit", 4, 0);
        if (check < 0)
        {
                cout << "Error sending data." << endl;
                exit(EXIT_FAILURE);
        }

        close(sockfd);

        return NULL;
} // bgThread()

void createBGThread(int sockfd) // Creates thread for background transfers.
{
        int check = 0;
        // Sending PWD to server.
        check = send(sockfd, "pwd", 3, 0);
        if (check < 0)
        {
                cout << "Error sending data." << endl;
                exit(EXIT_FAILURE);
        }

        memset(msgBuf, 0, BUFFSIZE); // Resetting the buffer.

        // Receiving output from server.
        check = recv(sockfd, msgBuf, BUFFSIZE, 0);
        if (check < 0)
        {
                cout << "Error receiving data." << endl;
                exit(EXIT_FAILURE);
        }

        pthread_t backgroundThread;
        pthread_create(&backgroundThread, NULL, &bgThread, NULL);
        sleep(1); // Gives time for the thread to display comID.
} // createBGThread()

bool getInput() //runs the command line. Prompts for input and gives a command.
{ 
        cout << "myftp> "; // Prompt.

        // Getting input from user.
        memset(msgBuf, 0, BUFFSIZE); // Resetting the buffer.
        cin.getline(msgBuf, BUFFSIZE);
        msgLen = strlen(msgBuf);
        strcpy(msgBufCpy, msgBuf);
        bool background = false;

        // Searching for the command (text before whitespace).
        if(strcmp(msgBuf, "\0")!=0) 
        {
                background = ((msgBuf[(strlen(msgBuf) - 1)]) == '&');
                com = strtok(msgBuf, " "); // com = myftp command.
                return background;
        }
        else
        { //ask for another prompt when \n is entered
                return getInput();
        }
} //end getInput()

void runCommand(int sockfd)
{
        int check = 0;

        // Why do switches not support strings?
        // Going through the commands.
        if ((strcmp(com, "get") == 0))
        {
                com = strtok(NULL, " "); //com=filename only
                FILE *fp = fopen(com, "w");
                char* fileName = com;

                while(com != NULL)
                {
                        strcat(msgBufCpy, com); //store file name to get from remote into msgBufCpy
                        com = strtok(NULL, " ");
                }
                
                check = send(sockfd, msgBufCpy, msgLen, 0); //send command to the server
                if(check < 0)
                {
                        cout << "Error sending data." << endl;
                        exit(EXIT_FAILURE);
                }

                // Start signal, and printing comID.
                memset(msgBuf, 0, BUFFSIZE);
                check = recv(sockfd, msgBuf, BUFFSIZE, 0);
                if (check < 0)
                {
                        cout << "Error receiving data." << endl;
                        exit(EXIT_FAILURE);
                }

                cout << "comID: " << msgBuf << endl;

                memset(msgBuf, 0, BUFFSIZE); // Resetting the buffer.
                memset(msgBufCpy, 0, BUFFSIZE);
                bool end = false;
                // Receiving output from server.
                while (!end)
                {
                        memset(msgBufCpy, 0, BUFFSIZE);
                        check = recv(sockfd, msgBufCpy, BUFFSIZE, 0);
                        if (check < 0)
                        {
                                cout << "Error receiving data." << endl;
                                exit(EXIT_FAILURE);
                        }

                        if (strcmp(msgBufCpy, "invalid") == 0) // End of file.
                        {
                                end = true;
                                fclose(fp);
                        }
                        else if (strcmp(msgBufCpy, "term") == 0)
                        {
                                fclose(fp);
                                remove(fileName); // Delete the partially received file.
                                end = true;
                        }
                        else // Write data to file.
                        {
                                fwrite(msgBufCpy, 1, check, fp);
                                fflush(fp);
                        }
                }

                memset(msgBuf, 0, BUFFSIZE);
                memset(msgBufCpy, 0, BUFFSIZE);
        }
        else if (strcmp(com, "put") == 0)
        {
                com = strtok(NULL, " "); //get the name of file only

                check = send(sockfd, msgBufCpy, strlen(msgBufCpy), 0); //send command to the server
                if(check < 0)
                {
                        cout << "Error sending data." << endl;
                        exit(EXIT_FAILURE);
                }

                FILE *fp = fopen(com, "r");
                memset(msgBuf, 0, BUFFSIZE);
                memset(msgBufCpy, 0, BUFFSIZE);
                int sendSize = 0;

                if(fp == NULL)
                {
                        cout << "File does not exist" << endl;
                }
                else
                {
                        bool term = false; // For terminate signal.

                        // Start signal, and printing comID.
                        memset(msgBuf, 0, BUFFSIZE);
                        check = recv(sockfd, msgBuf, BUFFSIZE, 0);
                        if (check < 0)
                        {
                                cout << "Error receiving data." << endl;
                                exit(EXIT_FAILURE);
                        }

                        cout << "comID: " << msgBuf << endl;

                        //keep sending buffers untill the end of file
                        while (!feof(fp) && !ferror(fp) && !term)
                        {
                                memset(msgBuf, 0, BUFFSIZE);
                                fread(msgBuf, BUFFSIZE, 1, fp);
                                sendSize = sizeof(msgBuf);
                                check = send(sockfd, msgBuf, sendSize, 0);
                                if (check < 0)
                                {
                                        cout << "Error sending data." << endl;
                                        exit(EXIT_FAILURE);
                                }

                                memset(msgBuf, 0, BUFFSIZE);

                                // Checking for terminate signal.
                                check = recv(sockfd, msgBuf, BUFFSIZE, 0);
                                if (check < 0)
                                {
                                        cout << "Error receiving data." << endl;
                                        exit(EXIT_FAILURE);
                                }

                                if (strcmp(msgBuf, "term") == 0) // Transfer terminated.
                                {
                                        fclose(fp);
                                        term = true;
                                }
                        }

                        if (term != true)
                        {
                                //send end of file signal
                                memset(msgBuf, 0, BUFFSIZE);
                                strcat(msgBuf, "invalid");
                                sendSize = strlen(msgBuf);
                                sleep(1); // Fixes a desync issue on the client.
                                check = send(sockfd, msgBuf, sendSize, 0);
                                if (check < 0)
                                {
                                        cout << "Error sending data." << endl;
                                        exit(EXIT_FAILURE);
                                }
                        }
                }
        }
        else if (strcmp(com, "ls") == 0 || strcmp(com, "pwd") == 0)
        {
                // Sending command to server.
                check = send(sockfd, msgBufCpy, msgLen, 0);
                if (check < 0)
                {
                        cout << "Error sending data." << endl;
                        exit(EXIT_FAILURE);
                }

                memset(msgBuf, 0, BUFFSIZE); // Resetting the buffer.

                // Receiving output from server.
                check = recv(sockfd, msgBuf, BUFFSIZE, 0);
                if (check < 0)
                {
                        cout << "Error receiving data." << endl;
                        exit(EXIT_FAILURE);
                }

                if (strcmp(msgBuf, "invalid") != 0)
                        cout << msgBuf;
        }
        else if (strcmp(com, "delete") == 0 || strcmp(com, "cd") == 0 ||
                 strcmp(com, "mkdir") == 0)
        {
                //cout << "delete, cd, and mkdir not yet implemented." << endl;
                // Sending command to server.
                check = send(sockfd, msgBufCpy, msgLen, 0);
                if (check < 0)
                {
                        cout << "Error sending data." << endl;
                        exit(EXIT_FAILURE);
                }

                memset(msgBuf, 0, BUFFSIZE); // Resetting the buffer.

                // Receiving output from server.
                check = recv(sockfd, msgBuf, BUFFSIZE, 0);
                if (check < 0)
                {
                        cout << "Error receiving data." << endl;
                        exit(EXIT_FAILURE);
                }

                if (strcmp(msgBuf, "invalid") != 0)
                        cout << msgBuf;
        }
        else if (strcmp(com, "terminate") == 0)
        {
                com = strtok(NULL, " "); // com = comID.
                pthread_t termThread;
                pthread_create(&termThread, NULL, &terminate, NULL);
                sleep(0.5); // Gives time for the thread to start.
        }
        else if (strcmp(com, "quit") == 0)
        {
                done = true;
        }
        else
        {
                cout << "Invalid command" << endl;
        }

}//end runCommand()


int main(int argc, char* argv[])
{
        if (argc != 4) //insure the right number of arguments
        {
                cout << "Usage: myftp [hostname/IP] [normalPort] [terminatePort]" << endl;
                exit(EXIT_FAILURE);
        }

        argInfo[0] = argv[1]; // Server hostname/IP.
        argInfo[1] = argv[2]; // Server normal port.
        argInfo[2] = argv[3]; // Server terminate port.

        int sockfd = setup(NPORT); // Connection to server.
        bool back = false; // For & handling.

        while(!done) //run program until user quits
        {
                back = getInput();   //prompt user for input and get input from user

                if(!back)
                {
                        runCommand(sockfd); //run the command given by the user
                }
                else
                {
                        createBGThread(sockfd);
                } 
        }

        close(sockfd);

        return EXIT_SUCCESS;
} // main()