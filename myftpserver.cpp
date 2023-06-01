// CSCI 4780 Spring 2021 Project 1
// Jisoo Kim, Will McCarthy, Tyler Scalzo

#include <iostream>
#include <cstdlib>
#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <mutex>


#define BACKLOG 10
#define BUFFSIZE 8192

using namespace std;

mutex comIDMutex, fileMutex;
const int arrSize = 5;
char commandId[arrSize];
char* filesUsed[arrSize];

char* termPort;

int sockfd = 0, check= 0, accfd = 0;
socklen_t cltAddrSize = 0;
struct sockaddr cltAddr;

struct ThreadInfo {
        pthread_t thread_ID;
        int sockfd;
        char curr_dir[500];
        int arrIndex;
};

int commandIds(int type, bool read = false);
int filesInUse(char* filePath, int type);

int arrIndex = 0;
ThreadInfo arr[100];

void setup(int argc, char* argv[]){
        struct addrinfo srvAddr, *conInfo;
        if (argc != 3) // Checking for port being supplied.
        {
                cout << "Usage: myftpserver [nport] [tport]" << endl;
                exit(EXIT_FAILURE);
        }

        //nport = argv[1];
        //tport = argv[2];


        //create the socket
        sockfd = socket(AF_INET, SOCK_STREAM, 0); // Getting socket fd.
        if (sockfd < 0)
        {
                cout << "Error creating Socket." << endl;
                exit(EXIT_FAILURE);
        }

        //Getting stuff setup for socket binding
        memset(&srvAddr, 0, sizeof srvAddr);
        srvAddr.ai_family = AF_INET;
        srvAddr.ai_socktype = SOCK_STREAM;
        srvAddr.ai_flags = AI_PASSIVE;

        // Getting connection information.
        getaddrinfo(NULL, argv[1], &srvAddr, &conInfo);
        // Binding to the port.
        check = bind(sockfd, conInfo->ai_addr, conInfo->ai_addrlen);
        if (check < 0)
        {
                cout << "Error binding port." << endl;
                exit(EXIT_FAILURE);
        }

        // Listening for connections.
        check = listen(sockfd, BACKLOG);
        if (check < 0)
        {
                cout << "Error listening." << endl;
                exit(EXIT_FAILURE);
        }

}//end setup()


void *runCommand(void *fd){
        // Searching for the command (text before whitespace).
        ThreadInfo threadinfo = *(struct ThreadInfo *)fd; //get access to args

        char buf[BUFFSIZE], comBuf[256]; // Buffer for received data.
        char bufCpy[BUFFSIZE]; // For running the commands.
        char fullPath[BUFFSIZE];
        int msgLen = 0, comSize = 0; // For some command outputs.
        bool done = false;
        char *com; // For checking commands.
        int fileMutexNumber = 0, comMutexNumber = 0;
        string str;
        char const *intChar;

        while(!done) { // run command continously
                //clear the buffer
                memset(buf, 0, BUFFSIZE);
                memset(bufCpy, 0, BUFFSIZE);
                memset(fullPath, 0, BUFFSIZE);

                check = recv(threadinfo.sockfd, buf, BUFFSIZE, 0);

                cout << buf << endl;

                if (check < 0)
                {
                        cout << "Error receiving data." << endl;
                        exit(EXIT_FAILURE);
                }
                else if (check == 0) //Connection was lost with the client
                {
                        done = true;
                }

                strcpy(bufCpy, buf); // Keeping strtok from truncating buf.
                com = strtok(buf, " ");

                // Why do switches not support strings?
                // Going through the commands.
                if (strcmp(com, "get") == 0)
                {
                        //cout << "get in progress (server)" << endl;

                        com = strtok(NULL, " "); //get the name of file only
                        cout << com << "\n";

                        strcpy(fullPath, threadinfo.curr_dir);
                        strcat(fullPath, "/");
                        strcat(fullPath, com);

                        fileMutexNumber = filesInUse(fullPath, -1);
                        if (fileMutexNumber < 0)
                                sleep(1); // Try again in a second.

                        comMutexNumber = (commandIds(-1));
                        if (comMutexNumber < 0)
                                sleep(1); // Try again in a second.

                        FILE *fp = fopen(fullPath, "r");
                        memset(buf, 0, BUFFSIZE);
                        memset(comBuf, 0, 128);

                        str = to_string(comMutexNumber);
                        intChar = str.c_str();

                        if(fp == NULL)
                        {
                                strcat(buf, "file does not exist");
                        }
                        else
                        {
                                // Sending comID.
                                check = send(threadinfo.sockfd, intChar, 1, 0);
                                if (check < 0)
                                {
                                        cout << "Error sending data." << endl;
                                        exit(EXIT_FAILURE);
                                }

                                bool term = false;

                                //keep sending buffers untill the end of file
                                while (!feof(fp) && !ferror(fp) && !term)
                                {
                                        memset(buf, 0, BUFFSIZE);
                                        fread(buf, BUFFSIZE, 1, fp);
                                        comSize = sizeof(buf);
                                        check = send(threadinfo.sockfd, buf, comSize, 0);
                                        if (check < 0)
                                        {
                                                cout << "Error sending data." << endl;
                                                exit(EXIT_FAILURE);
                                        }

                                        check = commandIds(comMutexNumber, true);
                                        if (check == -1)
                                        {
                                                fclose(fp);
                                                term = true;

                                                check = send(threadinfo.sockfd, "term", 4, 0);
                                                if (check < 0)
                                                {
                                                        cout << "Error sending data." << endl;
                                                        exit(EXIT_FAILURE);
                                                }
                                        }
                                }

                                if (!term)
                                {
                                        //send end of file signal
                                        memset(buf, 0, BUFFSIZE);
                                        strcat(buf, "invalid");
                                        comSize = strlen(buf);
                                        sleep(1); // Fixes a desync issue on the client.
                                        check = send(threadinfo.sockfd, buf, comSize, 0);
                                        if (check < 0)
                                        {
                                                cout << "Error sending data." << endl;
                                                exit(EXIT_FAILURE);
                                        }
                                }

                                // Relinquish locks.
                                commandIds(comMutexNumber);
                                filesInUse(fullPath, fileMutexNumber);
                        }
                }
                else if (strcmp(com, "put") == 0)
                {
                        com = strtok(NULL, " "); //get the name of file only
                        cout << com << "\n";

                        strcpy(fullPath, threadinfo.curr_dir);
                        strcat(fullPath, "/");
                        strcat(fullPath, com);

                        fileMutexNumber = filesInUse(fullPath, -1);
                        if (fileMutexNumber < 0)
                                sleep(1); // Try again in a second.

                        comMutexNumber = (commandIds(-1));
                        if (comMutexNumber < 0)
                                sleep(1); // Try again in a second.

                        memset(buf, 0, BUFFSIZE);
                        memset(comBuf, 0, 128);

                        str = to_string(comMutexNumber);
                        intChar = str.c_str();

                        FILE *fp = fopen(fullPath, "w");

                        memset(buf, 0, BUFFSIZE); // Resetting the buffer.
                        memset(bufCpy, 0, BUFFSIZE);
                        bool end = false, term = false;

                        // Sending comID.
                        check = send(threadinfo.sockfd, intChar, 1, 0);
                        if (check < 0)
                        {
                                cout << "Error sending data." << endl;
                                exit(EXIT_FAILURE);
                        }

                        // Receiving output from server.
                        while (!end)
                        {
                                memset(bufCpy, 0, BUFFSIZE);
                                check = recv(threadinfo.sockfd, bufCpy, BUFFSIZE, 0);
                                if (check < 0)
                                {
                                        cout << "Error receiving data." << endl;
                                        exit(EXIT_FAILURE);
                                }

                                if (strcmp(bufCpy, "invalid") != 0)
                                {
                                        fwrite(bufCpy, 1, check, fp);
                                        fflush(fp);
                                }
                                else
                                {
                                        end = true;
                                }

                                check = commandIds(comMutexNumber, true);
                                if (check == -1)
                                {
                                        fclose(fp);
                                        remove(fullPath);
                                        end = true;
                                        term = false;

                                        check = send(threadinfo.sockfd, "term", 4, 0);
                                        if (check < 0)
                                        {
                                                cout << "Error sending data." << endl;
                                                exit(EXIT_FAILURE);
                                        }
                                }
                        }

                        if (term)
                                fclose(fp);

                        memset(buf, 0, BUFFSIZE);
                        memset(bufCpy, 0, BUFFSIZE);

                        // Relinquish locks.
                        commandIds(comMutexNumber);
                        filesInUse(fullPath, fileMutexNumber);
                }
                else if (strcmp(com, "ls") == 0)
                {
                        strcpy(fullPath, "ls ");
                        strcat(fullPath, threadinfo.curr_dir);
                        strcat(fullPath, " 2>&1");

                        FILE *pipe = popen(bufCpy, "r");

                        // Resetting the buffers.
                        memset(buf, 0, BUFFSIZE);
                        memset(comBuf, 0, 128);

                        // Reading the data from the stream.
                        while (!feof(pipe))
                        {
                                if (fgets(comBuf, 128, pipe) == NULL)
                                        break;

                                comSize = sizeof(comBuf);
                                strncat(buf, comBuf, comSize);
                        }

                        if (buf[0] == '\0') // If no output from command.
                        {
                                strcat(buf, "invalid");
                        }

                        msgLen = strlen(buf);
                        // Sending output to client.
                        check = send(threadinfo.sockfd, buf, msgLen, 0);
                        if (check < 0)
                        {
                                cout << "Error sending data." << endl;
                                exit(EXIT_FAILURE);
                        }

                        pclose(pipe);
                }
                else if (strcmp(com, "pwd") == 0)
                {
                  check = send(threadinfo.sockfd, threadinfo.curr_dir, 500, 0);
                  if (check < 0)
                  {
                          cout << "Error sending data." << endl;
                          exit(EXIT_FAILURE);
                  }
                }
                else if (strcmp(com, "delete") == 0)
                {
                        memset(bufCpy, 0, BUFFSIZE);
                        strcpy(bufCpy, "rm "); // Using the actual delete command.

                        strcat(fullPath, threadinfo.curr_dir);
                        com = strtok(NULL, " ");
                        strcat(fullPath, "/");
                        strcat(fullPath, com);

                        fileMutexNumber = filesInUse(fullPath, -1);
                        if (fileMutexNumber < 0)
                                sleep(1); // Try again in a second.

                        strcat(bufCpy, fullPath);
                        strcat(bufCpy, " 2>&1");

                        FILE *pipe = popen(bufCpy, "r");

                        // Resetting the buffers.
                        memset(buf, 0, BUFFSIZE);
                        memset(comBuf, 0, 128);

                        // Reading the data from the stream.
                        while (!feof(pipe))
                        {
                                if (fgets(comBuf, 256, pipe) == NULL)
                                        break;

                                comSize = sizeof(comBuf);
                                strncat(buf, comBuf, comSize);
                        }

                        if (buf[0] == '\0') // If no output from command.
                        {
                                strcat(buf, "invalid");
                        }

                        msgLen = strlen(buf);
                        // Sending output to client.
                        check = send(threadinfo.sockfd, buf, msgLen, 0);
                        if (check < 0)
                        {
                                cout << "Error sending data." << endl;
                                exit(EXIT_FAILURE);
                        }

                        pclose(pipe);

                        filesInUse(fullPath, fileMutexNumber); // Free lock.
                }
                else if (strcmp(com, "cd") == 0 )
                {
                        /*//cout << "cd not implemented yet." << endl;
                        check = chdir(strtok(NULL, " "));
                        if (check == -1)
                        {
                                // Sending output to client.
                                check = send(threadinfo.sockfd, "cd error\n", 9, 0);
                                if (check < 0)
                                {
                                        cout << "Error sending data." << endl;
                                        exit(EXIT_FAILURE);
                                }
                        }
                        else
                        {
                                // Sending output to client.
                                cout << "inside else" << endl;
                                check = send(threadinfo.sockfd, "invalid", 7, 0);
                                if (check < 0)
                                {
                                        cout << "Error sending data." << endl;
                                        exit(EXIT_FAILURE);
                                }
                        }*/

                        strcat(threadinfo.curr_dir, "/");
                        com = strtok(NULL, " "); //get the name of file only
                        strcat(threadinfo.curr_dir, com);
                }
                else if (strcmp(com, "mkdir") == 0)
                {
                        memset(bufCpy, 0, BUFFSIZE);
                        strcpy(bufCpy, "mkdir "); // Using the actual delete command.

                        strcat(fullPath, threadinfo.curr_dir);
                        com = strtok(NULL, " ");
                        strcat(fullPath, "/");
                        strcat(fullPath, com);

                        strcat(bufCpy, fullPath);
                        strcat(bufCpy, " 2>&1");

                        FILE *pipe = popen(bufCpy, "r");

                        // Resetting the buffers.
                        memset(buf, 0, BUFFSIZE);
                        memset(comBuf, 0, 128);

                        // Reading the data from the stream.
                        while (!feof(pipe))
                        {
                                if (fgets(comBuf, 128, pipe) == NULL)
                                        break;

                                comSize = sizeof(comBuf);
                                strncat(buf, comBuf, comSize);
                        }

                        if (buf[0] == '\0') // If no output from command.
                        {
                                strcat(buf, "invalid");
                        }

                        msgLen = strlen(buf);
                        // Sending output to client.
                        check = send(threadinfo.sockfd, buf, msgLen, 0);
                        if (check < 0)
                        {
                                cout << "Error sending data." << endl;
                                exit(EXIT_FAILURE);
                        }

                        pclose(pipe);
                }
                else if (strcmp(com, "bg") == 0)
                {
                        memset(buf, 0, BUFFSIZE);
                        memset(bufCpy, 0, BUFFSIZE);
                        memset(fullPath, 0, BUFFSIZE);

                        check = recv(threadinfo.sockfd, bufCpy, BUFFSIZE, 0);
                        if (check < 0)
                        {
                                cout << "Error receiving data." << endl;
                                exit(EXIT_FAILURE);
                        }

                        if ((strcmp(bufCpy, "get") == 0))
                        {
                                check = recv(threadinfo.sockfd, fullPath, BUFFSIZE, 0);
                                if (check < 0)
                                {
                                        cout << "Error receiving data." << endl;
                                        exit(EXIT_FAILURE);
                                }

                                fileMutexNumber = filesInUse(fullPath, -1);
                                if (fileMutexNumber < 0)
                                        sleep(1); // Try again in a second.

                                comMutexNumber = (commandIds(-1));
                                if (comMutexNumber < 0)
                                        sleep(1); // Try again in a second.

                                FILE *fp = fopen(fullPath, "r");
                                memset(buf, 0, BUFFSIZE);
                                memset(comBuf, 0, 128);

                                str = to_string(comMutexNumber);
                                intChar = str.c_str();

                                if(fp == NULL)
                                {
                                        strcat(buf, "file does not exist");
                                }
                                else
                                {
                                        // Sending comID.
                                        check = send(threadinfo.sockfd, intChar, 1, 0);
                                        if (check < 0)
                                        {
                                                cout << "Error sending data." << endl;
                                                exit(EXIT_FAILURE);
                                        }

                                        bool term = false;

                                        //keep sending buffers untill the end of file
                                        while (!feof(fp) && !ferror(fp) && !term)
                                        {
                                                memset(buf, 0, BUFFSIZE);
                                                fread(buf, BUFFSIZE, 1, fp);
                                                comSize = sizeof(buf);
                                                check = send(threadinfo.sockfd, buf, comSize, 0);
                                                if (check < 0)
                                                {
                                                        cout << "Error sending data." << endl;
                                                        exit(EXIT_FAILURE);
                                                }

                                                check = commandIds(comMutexNumber, true);
                                                if (check == -1)
                                                {
                                                        fclose(fp);
                                                        term = true;

                                                        check = send(threadinfo.sockfd, "term", 4, 0);
                                                        if (check < 0)
                                                        {
                                                                cout << "Error sending data." << endl;
                                                                exit(EXIT_FAILURE);
                                                        }
                                                }
                                        }

                                        if (!term)
                                        {
                                                //send end of file signal
                                                memset(buf, 0, BUFFSIZE);
                                                strcat(buf, "invalid");
                                                comSize = strlen(buf);
                                                sleep(1); // Fixes a desync issue on the client.
                                                check = send(threadinfo.sockfd, buf, comSize, 0);
                                                if (check < 0)
                                                {
                                                        cout << "Error sending data." << endl;
                                                        exit(EXIT_FAILURE);
                                                }
                                        }

                                        // Relinquish locks.
                                        commandIds(comMutexNumber);
                                        filesInUse(fullPath, fileMutexNumber);
                                }
                        }
                        else // Can only be put if not get.
                        {
                                check = recv(threadinfo.sockfd, fullPath, BUFFSIZE, 0);
                                if (check < 0)
                                {
                                        cout << "Error receiving data." << endl;
                                        exit(EXIT_FAILURE);
                                }

                                fileMutexNumber = filesInUse(fullPath, -1);
                                if (fileMutexNumber < 0)
                                        sleep(1); // Try again in a second.

                                comMutexNumber = (commandIds(-1));
                                if (comMutexNumber < 0)
                                        sleep(1); // Try again in a second.

                                memset(buf, 0, BUFFSIZE);

                                str = to_string(comMutexNumber);
                                intChar = str.c_str();

                                FILE *fp = fopen(fullPath, "w");

                                memset(buf, 0, BUFFSIZE); // Resetting the buffer.
                                memset(bufCpy, 0, BUFFSIZE);
                                bool end = false, term = false;

                                // Sending comID.
                                check = send(threadinfo.sockfd, intChar, 1, 0);
                                if (check < 0)
                                {
                                        cout << "Error sending data." << endl;
                                        exit(EXIT_FAILURE);
                                }

                                // Receiving output from server.
                                while (!end)
                                {
                                        memset(bufCpy, 0, BUFFSIZE);
                                        check = recv(threadinfo.sockfd, bufCpy, BUFFSIZE, 0);
                                        if (check < 0)
                                        {
                                                cout << "Error receiving data." << endl;
                                                exit(EXIT_FAILURE);
                                        }

                                        if (strcmp(bufCpy, "invalid") != 0)
                                        {
                                                fwrite(bufCpy, 1, check, fp);
                                                fflush(fp);
                                        }
                                        else
                                        {
                                                end = true;
                                        }

                                        check = commandIds(comMutexNumber, true);
                                        if (check == -1)
                                        {
                                                fclose(fp);
                                                remove(fullPath);
                                                end = true;
                                                term = false;

                                                check = send(threadinfo.sockfd, "term", 4, 0);
                                                if (check < 0)
                                                {
                                                        cout << "Error sending data." << endl;
                                                        exit(EXIT_FAILURE);
                                                }
                                        }
                                }

                                if (term)
                                        fclose(fp);

                                memset(buf, 0, BUFFSIZE);
                                memset(bufCpy, 0, BUFFSIZE);

                                // Relinquish locks.
                                commandIds(comMutexNumber);
                                filesInUse(fullPath, fileMutexNumber);
                                }
                }
                else if (strcmp(com, "quit") == 0)
                {
                        done = true;
                }
                else
                {
                        cout << "Invalid command" << endl;
                        cout << com << endl;
                }
        }//end while
        return NULL;
} //end runCommand()

void* termThread(void* arg)
{
        struct addrinfo srvAddr, *conInfo;
        int termFd = 0;

        //nport = argv[1];
        //tport = argv[2];


        //create the socket
        termFd = socket(AF_INET, SOCK_STREAM, 0); // Getting socket fd.
        if (termFd < 0)
        {
                cout << "Error creating Socket." << endl;
                exit(EXIT_FAILURE);
        }

        //Getting stuff setup for socket binding
        memset(&srvAddr, 0, sizeof srvAddr);
        srvAddr.ai_family = AF_INET;
        srvAddr.ai_socktype = SOCK_STREAM;
        srvAddr.ai_flags = AI_PASSIVE;

        // Getting connection information.
        getaddrinfo(NULL, termPort, &srvAddr, &conInfo);
        // Binding to the port.
        check = bind(termFd, conInfo->ai_addr, conInfo->ai_addrlen);
        if (check < 0)
        {
                cout << "Error binding port." << endl;
                exit(EXIT_FAILURE);
        }

        // Listening for connections.
        check = listen(termFd, BACKLOG);
        if (check < 0)
        {
                cout << "Error listening." << endl;
                exit(EXIT_FAILURE);
        }

        int termAccFd, termId, termCheck;
        char termBuf[4];
        while (true)
        {
                // Accepting a connection.
                termAccFd = accept(termFd, &cltAddr, &cltAddrSize);
                if (termAccFd < 0)
                {
                        cout << "Error accepting." << endl;
                        exit(EXIT_FAILURE);
                }

                termCheck = recv(termFd, termBuf, 4, 0);
                if (termCheck < 0)
                {
                        cout << "Error receiving data." << endl;
                        exit(EXIT_FAILURE);
                }

                termId = stoi(termBuf);

                commandIds((termId + 5));

                close(termAccFd);
        }
}

int commandIds(int type, bool read)
{
    int rtn = -1;
    comIDMutex.lock();

    if (type < 0) // Get unused comID. -1 on full.
    {
        bool end = false;
        int i = 0;

        while (!end)
        {
            if (commandId[i] == 'I')
            {
                rtn = i;
                commandId[i] = 'A';
                end = true;
            }

            i++;
        }
    }
    else if (type >= arrSize) // Terminate if active.
    {
        if (commandId[(type - arrSize)] == 'A')
            commandId[(type - arrSize)] = 'T';
    }
    else 
    {
        if (!read) // Terminate.
            commandId[type] = 'I';
        else // Check for "T". -1 = 'T', type = 'A'.
            if (commandId[type] != 'A')
                rtn = -1;
            else
                rtn = type;
    }

    comIDMutex.unlock();
    return rtn;
} // commandIds()

int filesInUse(char* filePath, int type)
{
    int rtn = -1;
    fileMutex.lock();

    if (type < 0) // Add file name.
    {
        bool end = false;

        for (int j = 0; j < arrSize; j++)
        {
            if (strcmp(filesUsed[j], filePath) == 0) // No duplicates.
                end = true; // -1 returned on duplicate.
        }

        int i = 0;
        while (!end && i < arrSize)
        {
            if (strcmp(filesUsed[i], "empty") == 0)
            {
                rtn = i;
                strcpy(filesUsed[i], filePath);
                end = true;
            }

            i++;
        }
    }
    else // Remove file.
    {
        memset(filesUsed[type], 0, 1000);
        strcat(filesUsed[type], "empty");
        rtn = type;
    }

    fileMutex.lock();
    return rtn;
} // filesInUse()

int main(int argc, char* argv[])
{
        for (int i = 0; i < arrSize; i++)
        {
                filesUsed[i] = new char[1000]; // Initialising the arrays.
                memset(filesUsed[i], 0, 1000);
                strcat(filesUsed[i], "empty");
        }

        for (int i = 0; i < arrSize; i++)
        {
                commandId[i] = 'I'; // Initializing the comID array.
        }

        termPort = argv[2];
        pthread_t terminateThread;
        pthread_create(&terminateThread, NULL, &termThread, NULL);

        setup(argc, argv);

        while (1)
        {
                // Accepting a connection.
                accfd = accept(sockfd, &cltAddr, &cltAddrSize);
                if (accfd < 0)
                {
                        cout << "Error accepting." << endl;
                        exit(EXIT_FAILURE);
                }

                cout << "New Client accepted" << endl;
                //create the thread info struct
                struct ThreadInfo threadinfo;
                threadinfo.sockfd = accfd; //store fd in struct

                //store the working directory into the struct
                char cwd[500];
                getcwd(cwd, 500);
                strcpy(threadinfo.curr_dir, cwd);
                cout << cwd << endl;

                //store the struct into an array
                arr[arrIndex] = threadinfo; //add struct to arr
                threadinfo.arrIndex = arrIndex; //store index in struct
                arrIndex++; //increment arrIndex for next threadinfo to be added



                pthread_create(&threadinfo.thread_ID, NULL, &runCommand, (void *)&threadinfo);
        }

        close(sockfd);

        return EXIT_SUCCESS;
}
