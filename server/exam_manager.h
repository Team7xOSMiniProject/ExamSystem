#ifndef EXAM_MANAGER_H
#define EXAM_MANAGER_H

#include <string>
#include <vector>
#include <sstream>
#include <netinet/in.h>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fstream>
#include<pthread.h>

using namespace std;

class ExamManager {
public:
    bool parse_exam(const string& exam_type, const string& input_file, const string& exam_name, const string& instructor, int duration, const string& start_time);
    vector<string> load_exam_metadata(const string& exam_list_file);
    string getMetadataFilePath(const string& examName);
    string getQuestionsFilePath(const string& metadataPath) ;
    void sendExamQuestions(int sock, const string& examName);
};

#endif
