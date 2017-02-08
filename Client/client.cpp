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
#include "functions.h"

using namespace std;

int PORT;
char *IP;
int socketDescriptor;
struct sockaddr_in serverSocket;

#define BUFFER_SIZE 4096

void sendIndexCommand(int index) {

    cout << "I send the index of the command\n";
    if (write(socketDescriptor, &index, sizeof(int)) == -1) {
        perror("[Error] Failed to send the index of the command");
        exit(5);
    }
}

void sendExtension(char extension[30]) {

    if (write(socketDescriptor, extension, 30) < 0) {
        perror("Failed to send the extension");
        exit(22);
    }

    cout << "I sent extension:" << extension << "\n";

}

int receiveExtensionPossibility() {

    int possibility = 0;
    if (read(socketDescriptor, &possibility, sizeof(int)) < 0) {
        perror("Failed to receive the availability of conversion");
        return 0;
    }
    cout << "Possibility:" << possibility << "\n";
    return possibility;
}

void sendFile(string fileName) {

    char buffer[BUFFER_SIZE];
    bzero(buffer, BUFFER_SIZE);

    cout << "Sending the file to the server\n";
    FILE *file = fopen(fileName.c_str(), "r");
    int bytesRead = 0;
    while ((bytesRead = fread(buffer, sizeof(char), BUFFER_SIZE, file)) > 0) {
        if (send(socketDescriptor, buffer, bytesRead, 0) < 0) {
            perror("[ERROR]Some error occurred.Failed to send the file\n");
            fclose(file);
            exit(22);
        }
//        cout << "Size send:" << bytesRead << "\n";
        bzero(buffer, BUFFER_SIZE);
    }

    cout << "File:" << fileName.c_str() << "was send\n";
    fclose(file);
}

void receiveConvertedFile(string fileName, string extension) {

    char receiveBuffer[BUFFER_SIZE];
    bzero(receiveBuffer, BUFFER_SIZE);

    string convertedFileName = getValidNameForConvertedFile(fileName, extension);
    cout << "File converted name:" << convertedFileName.c_str() << "\n";
    FILE *file = fopen(convertedFileName.c_str(), "w");
    if (file == NULL) {
        // printf("File %s Cannot be opened file.\n", fileName.c_str());
        cout << "File" << fileName.c_str() << " cannon be open\n";
    }

    int readBlockSize;
    while ((readBlockSize = read(socketDescriptor, receiveBuffer, BUFFER_SIZE)) > 0) {

        int writeBlockSize = fwrite(receiveBuffer, sizeof(char), readBlockSize, file);
        if (writeBlockSize < readBlockSize) {
            perror("File write failed.\n");
        }
        bzero(receiveBuffer, BUFFER_SIZE);

        if (readBlockSize == 0 || readBlockSize != BUFFER_SIZE) {
            break;
        }
    }

    if (readBlockSize < 0) {
        cout << "Failed to read the file due to an error" << " " << errno << "\n";
        exit(25);
    }
    cout << "File was converted successfully\n";
    fclose(file);

}

void signalHandler(int signum) {

    cout << "\nInterrupt signal (" << signum << ") received.\n";
    close(socketDescriptor);
    exit(signum);
}

int main(int argc, char *argv[]) {

    if (argc != 3) {
        printf("Syntax error!! %s <IP> <PORT>\n", argv[0]);
        exit(1);
    }

    IP = argv[1];
    PORT = atoi(argv[2]);

    signal(SIGINT, signalHandler);

    //Create the socket

    if ((socketDescriptor = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Error.Failed to create the socket");
        exit(1);
    }

    //Complete the structure used for the communication with the server

    serverSocket.sin_family = AF_INET;
    serverSocket.sin_addr.s_addr = inet_addr(IP);
    serverSocket.sin_port = htons(PORT);


    //We connect to the server

    if (connect(socketDescriptor, (struct sockaddr *) &serverSocket, sizeof(struct sockaddr)) == -1) {
        perror("[Error]Failed to connect to server");
        exit(2);
    }


    char currentExtension[30];
    char futureExtension[30];
    string fileName;
    int menuIndex;
    string commandEntered;
    string ending;


    cout << "Welcome to CastDoc\n";
    while (1) {

        showMenu();
        cin >> commandEntered;
        if (containsOnlyDigits(commandEntered)) {
            menuIndex = stoi(commandEntered);
        } else {
            cout << "Wrong syntax.Enter an index\n";
            continue;
        }

        switch (menuIndex) {
            case 1:

                cout << "Current Extension:";
                cin >> currentExtension;
                cout << "Future Extension:";
                cin >> futureExtension;
                cout << "FileName:";
                cin >> fileName;

                ending = ".";
                ending.append(currentExtension);
                if (!fileExists(fileName)) {
                    cout << "File you want to convert doesn't exists\n";
                    break;
                }

                if (hasSameEnding(fileName, ending)) {

                    sendIndexCommand(1);
                    sendExtension(currentExtension);
                    sendExtension(futureExtension);

                    int isExtensionPossible = receiveExtensionPossibility();
                    if (isExtensionPossible == 1) {
                        sendFile(fileName);
                        receiveConvertedFile(extractFileName(fileName), futureExtension);

                    } else {
                        cout << "Conversion isn't supported\n";
                    }
                } else {
                    cout << "File entered has different extension\n";
                    break;
                }
                break;
            case 2:
                sendIndexCommand(2);
                close(socketDescriptor);
                return 1;
            default:
                cout << "Command doesn't exists\n";

        }
    }

}

