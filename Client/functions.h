//
// Created by razvan on 06.02.2017.
//


#include "string"
#include "iostream"
#include <algorithm>
#include <ctype.h>


using namespace std;

void showMenu() {
    cout << "\n1)Convert\n";
    cout << "2)Exit\n";
    cout << "Enter the index of command:";
}

bool containsOnlyDigits(const string &str) {
    return all_of(str.begin(), str.end(), ::isdigit);
}


inline bool fileExists(const string &name) {
    struct stat buffer;
    return (stat(name.c_str(), &buffer) == 0);
}

string getValidNameForConvertedFile(string fileName, string extension) {

    string convertedFileName(fileName);
    convertedFileName.append(".").append(extension);

    int index = 1;
    while (fileExists(convertedFileName)) {
        convertedFileName.clear();
        convertedFileName.append(fileName);
        convertedFileName.append("(").append(to_string(index)).append(")");
        convertedFileName.append(".").append(extension);
        index++;
    }

    return convertedFileName;
}

string extractFileName(string fileName) {
    return fileName.substr(0, fileName.find_last_of("."));
}

bool hasSameEnding(string const fullString, string const ending) {
    cout << fullString.c_str() << " " << ending.c_str() << "\n";
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
    } else {
        return false;
    }
}

