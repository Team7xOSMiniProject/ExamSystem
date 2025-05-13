#ifndef CLIENT_H
#define CLIENT_H

#include <iostream>
#include <string>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <map>
#include <unordered_map>
#include <random>
#include <algorithm>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <sys/stat.h>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <regex>
#include <mutex> 
#include <thread>
#include <atomic>

using namespace std;
using namespace std::chrono;

class ExamInfo {
    public:
        string name;
        string type;
        string start_time;
        int duration;
        int totalQuestions;
        string instructor;
        ExamInfo(string name, string type, string start_time,int duration, int totalQ, string instructor): name(name),type(type), start_time(start_time), duration(duration), totalQuestions(totalQ), instructor(instructor) {}
};

class Client {
private:
    int sock;
    string role, username, password;

    static map<int, int> shuffledQuestionMap; 
    static vector<vector<int>> shuffledOptionMap; 
    static vector<string> shuffledQuestions;
    static vector<vector<string>> shuffledOptions;
    static map<int, int> timeSpentPerQuestion;

    static void* studentHandler(void* arg);
    static void* instructorHandler(void* arg);

    static void manageExam(int duration, Client* client, string examname);
    static void decryptAndPrepareExam(const string& filePath, char key);
    static void xorEncryptDecrypt(const string& filePath, char key);
    static void receiveAndStoreExamQuestions(int sock, int examNumber); 
    static void dashboard(Client * client);
    static void displayPreparedQuestion(int index);
    static void handleExamSelection(Client* client, int& choice);
    static void parseAvailableExams(const string& examData);
    static void ensureDirectoryExists(const string &path);
    static void backupExamData(string &examName, const string &finalData);
    static void sendPendingAnswerSheet(int clientSocket);
    static int userInput(const string& prompt, int minVal, int maxVal);
    void authenticate();

public:
    static bool timeUp;
    static pthread_mutex_t timerMutex;
    Client(const string& ip, int port);
    void start();
};

#endif
