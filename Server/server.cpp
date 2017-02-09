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

/*!
 * Structure that is send to the thread<br>
 * Contains information about the client
 */
struct threadData {
    int threadID;
    int clientSocketDescriptor;
};

ifstream configFile("config.cfg");

/*!
 * Structure that contains regarding the conversion that the server can realise
 */
struct configConvert {
    string currentExtension;
    string futureExtension;
    string command;
};

list <configConvert> possibleConversion;

/*!
 * The function that will execute for each client that is connected
 * @param argv Arguments of the thread. Information about the client, Socket and ID.
 * @return
 */
static void *treat(void *argv);

/*!
 * Function that reads the config file and loads into memory the conversions that server can realise
 */
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

/*!
 * Function that reads an extension from the client
 * @param clientSocketDescriptor Socket used to communicate with the client
 * @return A string that represent the extension that was read
 */
string getExtension(int clientSocketDescriptor) {

    char extension[10];
    if (read(clientSocketDescriptor, extension, 10) < 0) {
        perror("Failed to read the  extension");
        pthread_exit(NULL);
    }

    cout << "Extension read:" << extension << endl;
    string aux(extension);
    return aux;

}


/*!
 * Function that checks if the conversion can be done by the server.
 * @param currentExtension The current extension of the file.
 * @param futureExtension The future extension of the file.
 * @return
 */
bool verifyExtensions(string currentExtension, string futureExtension) {

    for (list<configConvert>::iterator begin = possibleConversion.begin(), end = possibleConversion.end();
         begin != end; begin++) {
        if (currentExtension.compare(begin->currentExtension) == 0 &&
            futureExtension.compare(begin->futureExtension) == 0) {
            cout << "Conversion is possible\n";
            return true;
        }
    }

    cout << "Conversion isn't possible\n";
    return 0;
}

/*!
 * Function that sends to the client the possibility of the conversion that he wants to do.
 * @param clientSocketDescriptor The socket that is used for the communication with the client
 * @param isConversionPossible  The possibility of the conversion
 */
void sendPossibilityOfConversion(int clientSocketDescriptor, int isConversionPossible) {

    if (send(clientSocketDescriptor, &isConversionPossible, sizeof(int), MSG_NOSIGNAL) == -1) {
        if (errno == EPIPE) {
            perror("Failed to send the result of possible conversion");
            close(clientSocketDescriptor);
            pthread_exit(NULL);
        }
    }
    cout << "Am trimis clientului posibilitatea de conversie\n";
}


/*!
 * Function that receives the file that will be converted<br>
 * The file is split into pieces of 4096 bytes
 * @param clientSocketDescriptor The socket that is used for the communication with the client
 * @param filename The name that the file that was received from the client will have
 */
void receiveData(int clientSocketDescriptor, string filename) {

    char receiveBuffer[BUFFER_SIZE];
    bzero(receiveBuffer, BUFFER_SIZE); //Fill the buffer zero values

    FILE *file = fopen(filename.c_str(), "w");
    int readBlockSize = 0;
    int writeBlockSize = 0;
    cout << "Waiting to receive the file\n";
    while ((readBlockSize = read(clientSocketDescriptor, &receiveBuffer, BUFFER_SIZE)) > 0) {

//        cout << "Size received:" << readBlockSize << endl;
        //Write the bytes receive into the file
        writeBlockSize = fwrite(receiveBuffer, sizeof(char), readBlockSize, file);
        if (readBlockSize != BUFFER_SIZE) {
            break;
        }
        if (writeBlockSize < readBlockSize) {
            perror("[ERROR]File write failed on server\n");
            pthread_exit(NULL);
        }
        bzero(receiveBuffer, BUFFER_SIZE);
    }

    if (readBlockSize < 0) {
        perror("Failed to read the file from socket due to an error");
        pthread_exit(NULL);
    }

    cout << "File:" << filename << " was received from client\n";
    fclose(file);
}

/*!
 * Function that returns the module that will be used to convert the file
 * @param currentExtension The current extension of the file
 * @param futureExtension The future extension of the file
 * @return The module that will be used to convert the file
 */
string getConvertCommand(string currentExtension, string futureExtension) {

    for (list<configConvert>::iterator begin = possibleConversion.begin(), end = possibleConversion.end();
         begin != end; begin++) {
        if (begin->currentExtension.compare(currentExtension) == 0 &&
            begin->futureExtension.compare(futureExtension) == 0) {
            return begin->command;
        }
    }
}


/*!
 * Function that converts the file
 * @param fileName The name of the file that is being converted
 * @param newFileName The name of the converted file that will be send to client
 * @param currentExtension The current extension of the file
 * @param secondExtension The future extension of the file
 */
void convertFile(string fileName, string newFileName, string currentExtension, string secondExtension) {

    string conversionCommand = getConvertCommand(currentExtension, secondExtension);
    if (conversionCommand.compare("abiword") == 0) {
        conversionCommand.append("  --to=").append(secondExtension.c_str()).append(" ").append(fileName.c_str());
    } else if (conversionCommand.compare("pdf2ps") == 0 || conversionCommand.compare("ps2pdf") == 0) {
        conversionCommand.append(" ").append(fileName.c_str());
    }

    cout << "Conversion command:" << conversionCommand.c_str() << endl;
    cout << "New file name:" << newFileName.c_str() << endl;
    cout << "File is being converted\n";
    system(conversionCommand.c_str());//run bash command that makes the conversion on file
    cout << "File was converted\n";

}

/*!
 * Function that removes a file
 * @param fileName The path of the file that will be removed
 */
void removeFile(string fileName) {
    if (remove(fileName.c_str()) == -1) {
        perror("[ERROR]Failed to remove a file");
    }
}


/*!
 * Function that send to the client the converted file.
 * The file is split into pieces of 4096 bytes
 * @param clientSocketDescriptor The socket that is used for the communication with the client
 * @param fileName The path of the file that will be send to client
 */
void sendConvertedFile(int clientSocketDescriptor, string fileName) {

    char buffer[BUFFER_SIZE];
    bzero(buffer, BUFFER_SIZE);

    cout << "Sending the file converted:" << fileName.c_str() << endl;
    FILE *file = fopen(fileName.c_str(), "r");
    if (file == NULL) {
        printf("ERROR: File %s not found.\n", fileName.c_str());
        pthread_exit(NULL);
    }

    int bytesRead = 0;
    while ((bytesRead = fread(buffer, sizeof(char), BUFFER_SIZE, file)) > 0) {
        if (send(clientSocketDescriptor, buffer, bytesRead, MSG_NOSIGNAL) < 0) {
            cout << "[ERROR] Failed to send the file:" << fileName.c_str() << "due to an error" << errno << endl;
            fclose(file);
            removeFile(fileName);
            pthread_exit(NULL);
        }
//        cout << "FileS ize Send:" << sizeFileSend << endl;
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
    int id = 0;
    while (1) {
        cout << "Waiting at port:" << PORT << endl;


        socklen_t length = sizeof(clientSocket);

        if ((client = accept(serverSocketDescriptor, (struct sockaddr *) &clientSocket, &length)) == -1) {
            perror("[ERROR] Failed to accept a client");
            exit(4);
        }

        threadData *data;
        data = (struct threadData *) malloc(sizeof(struct threadData));
        data->clientSocketDescriptor = client;
        data->threadID = id++;
        pthread_create(&threads[client], NULL, &treat, data);
    }
}

/*!
 * The function that will execute for each client that is connected
 * @param argv Arguments of the thread. Information about the client, Socket and ID.
 * @return
 */
static void *treat(void *argv) {

    struct threadData data;
    data = *((struct threadData *) argv);
    pthread_detach(pthread_self());

    string firstExtension, secondExtension;
    int indexOfCommand;
    bool EXIT_FLAG = false;
    bool isConversionPossible;

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
                cout << firstExtension.c_str() << " " << secondExtension.c_str() << " " << endl;

                isConversionPossible = verifyExtensions(firstExtension, secondExtension);
                if (isConversionPossible) {
                    sendPossibilityOfConversion(data.clientSocketDescriptor, 1);

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
                    sendPossibilityOfConversion(data.clientSocketDescriptor, 0);
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

