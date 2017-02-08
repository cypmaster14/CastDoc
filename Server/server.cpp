//
// Created by razvan on 06.02.2017.
//

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string>
#include "iostream"
#include "fstream"
#include "list"
#include <stdarg.h>
#include <regex.h>
#include <cstdlib>

using namespace std;

#define PORT 5555
#define BUFFER_SIZE 4096
pthread_t threads[100];
struct sockaddr_in serverSocket;
struct sockaddr_in clientSocket;
int serverSocketDescriptor;


struct threadData {
    int threadID;
    int clientSocketDescriptor;
};

ifstream configFile("config.cfg");

struct configConvert {
    string currentExtension;
    string futureExtension;
    string command;
};

list <configConvert> possibleConversion;


static void *treat(void *argv);

void uploadConversion() {

    regex_t preg[1];
    regmatch_t match[4];
    string patter = "(\\w+)->(\\w+)\\s+(\\w+|\\s+)";
    int rc = regcomp(preg, patter.c_str(), REG_EXTENDED);

    for (string line; getline(configFile, line);) {
        rc = regexec(preg, line.c_str(), 4, match, 0);
        if (rc != REG_NOMATCH) {
            struct configConvert config;
            config.currentExtension = line.substr(match[1].rm_so, match[1].rm_eo - match[1].rm_so);
            config.futureExtension = line.substr(match[2].rm_so, match[2].rm_eo - match[2].rm_so);
            config.command = line.substr(match[3].rm_so, match[3].rm_eo - match[3].rm_so);
            possibleConversion.push_back(config);
        } else {
            cout << "No match\n"; //it's for test to see if regex matches something
        }
    }
    configFile.close();
}

string getExtension(int clientSocketDescriptor) {

    char extension[30];
    if (read(clientSocketDescriptor, extension, 30) < 0) {
        perror("Failed to read the  extension");
        pthread_exit(NULL);
    }

    cout << "Extension read:" << extension << "\n";
    string aux(extension);
    return aux;

}

int checkConversion(string currentExtension, string futureExtension) {

    for (list<configConvert>::iterator begin = possibleConversion.begin(), end = possibleConversion.end();
         begin != end; begin++) {
        if (currentExtension.compare(begin->currentExtension) == 0 &&
            futureExtension.compare(begin->futureExtension) == 0) {
            return 1;
        }
    }
    return 0;
}

bool verifyExtensions(int clientSocketDescriptor, string firstExtension, string secondExtension) {

    int isConversionPossible = checkConversion(firstExtension, secondExtension);
    if (isConversionPossible == 1) {
        cout << "Conversion is possible\n";
    } else {
        cout << "Conversion isn't possible\n";
    }

    //We send the client if the conversion is possible or not
    if (send(clientSocketDescriptor, &isConversionPossible, sizeof(int), MSG_NOSIGNAL) == -1) {
        if (errno == EPIPE) {
            perror("Failed to send the result of possible conversion");
            close(clientSocketDescriptor);
            pthread_exit(NULL);
//            exit(10);
        }
    }

    cout << "Am trimis clientului posibilitatea de conversie\n";
    return isConversionPossible == 1;
}


void receiveData(int clientSocketDescriptor, string filename) {

    char receiveBuffer[BUFFER_SIZE];
    bzero(receiveBuffer, BUFFER_SIZE);

    FILE *file = fopen(filename.c_str(), "a");
    int readBlockSize = 0;
    cout << "Waiting to receive the file\n";
    while ((readBlockSize = read(clientSocketDescriptor, &receiveBuffer, BUFFER_SIZE)) > 0) {

//        cout << "Size received:" << readBlockSize << "\n";

        int writeBlockSize = fwrite(receiveBuffer, sizeof(char), readBlockSize, file);
        if (readBlockSize == 0 || readBlockSize != BUFFER_SIZE) {
            break;
        }
        if (writeBlockSize < readBlockSize) {
            perror("[ERROR]File write failed on server\n");
//            exit(11);
            pthread_exit(NULL);
        }
        bzero(receiveBuffer, BUFFER_SIZE);
    }

    if (readBlockSize < 0) {
        cout << "Failed to read from socket due to an error " << errno << "\n";
//        exit(12);
        pthread_exit(NULL);
    }

    cout << "File:" << filename << " was received from client";
    fclose(file);
}

string getConvertCommand(string currentExtension, string futureExtension) {

    for (list<configConvert>::iterator begin = possibleConversion.begin(), end = possibleConversion.end();
         begin != end; begin++) {
        if (begin->currentExtension.compare(currentExtension) == 0 &&
            begin->futureExtension.compare(futureExtension) == 0) {
            return begin->command;
        }
    }
}

void convertFile(string fileName, string newFileName, string currentExtension, string secondExtension) {

    string conversionCommand = getConvertCommand(currentExtension, secondExtension);
    if (conversionCommand.compare("abiword") == 0) {
        conversionCommand.append("  --to=").append(secondExtension.c_str()).append(" ").append(fileName.c_str());
    } else if (conversionCommand.compare("pdf2ps") == 0 || conversionCommand.compare("ps2pdf") == 0) {
        conversionCommand.append(" ").append(fileName.c_str());
    }

    cout << "Conversion command:" << conversionCommand.c_str() << "\n";
    cout << "New file name:" << newFileName.c_str() << "\n";

    system(conversionCommand.c_str());//run bash command that makes the conversion on file

}

void removeFile(string fileName) {
    if (remove(fileName.c_str()) == -1) {
        perror("[ERROR]Failed to remove a file");
    }
}

void sendConvertedFile(int clientSocketDescriptor, string fileName) {

    char buffer[BUFFER_SIZE];
    bzero(buffer, BUFFER_SIZE);

    cout << "Sending the file converted:" << fileName.c_str() << "\n";
    FILE *file = fopen(fileName.c_str(), "r");
    if (file == NULL) {
        printf("ERROR: File %s not found.\n", fileName.c_str());
        exit(1);
    }

    int bytesRead = 0;
    while ((bytesRead = fread(buffer, sizeof(char), BUFFER_SIZE, file)) > 0) {
        if (send(clientSocketDescriptor, buffer, bytesRead, MSG_NOSIGNAL) < 0) {

            cout << "[ERROR] Faled to send the file:" << fileName.c_str() << "due to an error" << errno << "\n";
            fclose(file);
            removeFile(fileName);
            pthread_exit(NULL);
        }
//        cout << "FileS ize Send:" << sizeFileSend << "\n";
        bzero(buffer, BUFFER_SIZE);
    }

    cout << "File:" << fileName.c_str() << " was sent\n";
    fclose(file);
}

int main(int argc, char *argv[]) {

    uploadConversion();

    //Create the server socket

    if ((serverSocketDescriptor = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("[ERROR] Failed to create the socket");
        exit(1);
    }

    //Set option SO_REUSEADDR
    int option = 1;
    setsockopt(serverSocketDescriptor, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(int));


    //Complte the structure of server with the aditional information
    serverSocket.sin_family = AF_INET;
    serverSocket.sin_addr.s_addr = htonl(INADDR_ANY);
    serverSocket.sin_port = htons(PORT);

    //We bind the socket
    if (bind(serverSocketDescriptor, (struct sockaddr *) &serverSocket, sizeof(struct sockaddr_in)) == -1) {
        perror("[ERROR] Failed to bind the address");
        exit(2);
    }

    if (listen(serverSocketDescriptor, 5) == -1) {
        perror("[ERROR] Failed to listen");
        exit(3);
    }

    //We use threads to serve the clients in a concurrent way
    int client = 1;
    while (1) {
        cout << "Waiting at port:" << PORT << "\n";
        threadData *data;

        socklen_t length = sizeof(clientSocket);

        if ((client = accept(serverSocketDescriptor, (struct sockaddr *) &clientSocket, &length)) == -1) {
            perror("[ERROR] Failed to accept a client");
            exit(4);
        }

        data = (struct threadData *) malloc(sizeof(struct threadData));
        data->clientSocketDescriptor = client;
        data->threadID = client++;
        pthread_create(&threads[client], NULL, &treat, data);
    }
}

static void *treat(void *argv) {

    struct threadData data;
    data = *((struct threadData *) argv);
    pthread_detach(pthread_self());

    string firstExtension, secondExtension;
    int indexOfCommand;
    bool EXIT_FLAG = false;

    while (!EXIT_FLAG) {

//        cout << "I wait to receive the index of the command\n";
        if (read(data.clientSocketDescriptor, &indexOfCommand, sizeof(int)) <= 0) {
            perror("[Error]Failed to read the index of the command");
            pthread_exit(NULL);
        }
//        cout << "I read the index of the command entered by the client\n";

        switch (indexOfCommand) {
            case 1:

                firstExtension = getExtension(data.clientSocketDescriptor);
                secondExtension = getExtension(data.clientSocketDescriptor);
                cout << firstExtension.c_str() << " " << secondExtension.c_str() << " " << "\n";

                if (verifyExtensions(data.clientSocketDescriptor, firstExtension, secondExtension)) {

                    string filename = "Client";
                    filename += to_string(data.threadID).append(".").append(firstExtension.c_str());
                    receiveData(data.clientSocketDescriptor, filename);

                    string newFileName = "Client";
                    newFileName += to_string(data.threadID).append(".").append(secondExtension.c_str());
                    convertFile(filename, newFileName, firstExtension, secondExtension);
                    removeFile(filename);
                    sendConvertedFile(data.clientSocketDescriptor, newFileName);
                    removeFile(newFileName);

                } else {
                    cout << "Conversion isn't possible\n";
                }
                break;
            case 2:
                EXIT_FLAG = true;
                cout << "Client wants to disconnect\n";
                break;
        }
    }

    close(data.clientSocketDescriptor);
    return NULL;
}

