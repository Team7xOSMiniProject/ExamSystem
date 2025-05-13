#include "exam_manager.h"
pthread_mutex_t file_mutex4 = PTHREAD_MUTEX_INITIALIZER; // analysis file

bool ExamManager::parse_exam(const string& exam_type, const string& input_file, const string& exam_name, const string& instructor, int duration, const string& start_time){
    
    ifstream infile(input_file);

    string line, currentQuestion, optionA, optionB, optionC, optionD, correctAnswer;
    vector<string> questions, answers;
    int questionCount = 0;
    bool readingQuestion = false;

    while (getline(infile, line)) {
        if (line.rfind("Q:", 0) == 0) {
            if (!currentQuestion.empty()) {  
                questions.push_back(currentQuestion + "\nA) " + optionA + "\nB) " + optionB + "\nC) " + optionC + "\nD) " + optionD);
                answers.push_back(correctAnswer);
                questionCount++;
            }
            currentQuestion = line;  
            readingQuestion = true;
        } else if (line.rfind("A)", 0) == 0) {
            optionA = line.substr(2);
            readingQuestion = false;
        } else if (line.rfind("B)", 0) == 0) {
            optionB = line.substr(2);
        } else if (line.rfind("C)", 0) == 0) {
            optionC = line.substr(2);
        } else if (line.rfind("D)", 0) == 0) {
            optionD = line.substr(2);
        } else if (line.rfind("A:", 0) == 0) {
            correctAnswer = line.substr(2);
        } else if (readingQuestion) {  
            currentQuestion += "\n" + line;
        }
    }

    if (!currentQuestion.empty()) {
        questions.push_back(currentQuestion + "\nA) " + optionA + "\nB) " + optionB + "\nC) " + optionC + "\nD) " + optionD);
        answers.push_back(correctAnswer);
        questionCount++;
    }

    infile.close();

    if (questions.empty()) {
        cout << "[-] Error: No valid questions found in file.\n";
        return false;
    }

    string metadataFile = "../data/exams/metadata_" + exam_name + ".txt";
    string questionsFile = "../data/exams/questions_" + exam_name + ".txt";
    string answersFile = "../data/exams/answers_" + exam_name + ".txt";

    ofstream metaFile(metadataFile);
    metaFile << "Exam Name: " << exam_name << "\n";
    metaFile << "Exam type: " << exam_type << "\n";
    metaFile << "Start Time: " << start_time << "\n";
    metaFile << "Duration (minutes): " << duration << "\n";
    metaFile << "Total Questions: " << questionCount << "\n";
    metaFile << "Instructor: " << instructor << "\n";
    metaFile << "Questions File: " << questionsFile << "\n";
    metaFile << "Answers File: " << answersFile << "\n";
    metaFile.close();

    // Store parsed questions
    ofstream questionFile(questionsFile);
    for (const string &q : questions) {
        questionFile << q << "\n\n";
    }
    questionFile.close();

    // Store correct answers separately
    ofstream answerFile(answersFile);
    for (const string &a : answers) {
        answerFile << a << "\n";
    }
    answerFile.close();

    // Add exam entry to a central list
    pthread_mutex_lock(&file_mutex4);
    ofstream examList("../data/exams/exam_list.txt", ios::app);
    examList << exam_name << "|" << metadataFile << "\n";
    examList.close();
    pthread_mutex_unlock(&file_mutex4);

    cout << "[+] Exam successfully parsed and stored!\n";
    return true;
}

vector<string> ExamManager::load_exam_metadata(const string& exam_list_file) {
    vector<string> examMetadata;
    ifstream examList(exam_list_file);
    if (!examList) {
        cerr << "Error: Unable to open " << exam_list_file << "\n";
        return examMetadata;
    }

    string line;
    while (getline(examList, line)) {
        istringstream iss(line);
        string examName, metadataPath;

        if (getline(iss, examName, '|') && getline(iss, metadataPath)) {
            ifstream metadataFile(metadataPath);
            if (!metadataFile) {
                cerr << "Error: Unable to open metadata file " << metadataPath << "\n";
                continue;
            }

            ostringstream metadata;
            string metaLine;
            while (getline(metadataFile, metaLine)) {
                if (metaLine.find("Questions File:") != string::npos || 
                    metaLine.find("Answers File:") != string::npos) {
                    break;
                }
                metadata << metaLine << "\n";
            }
            metadataFile.close();
            examMetadata.push_back(metadata.str());
            
        }
    

    }
    examList.close();
    return examMetadata;
}

string ExamManager::getMetadataFilePath(const string& examName) {
    ifstream examList("../data/exams/exam_list.txt");
    if (!examList) {
        cerr << "Error: Unable to open exam_list.txt\n";
        return "";
    }

    string line, name, metadataPath;
    while (getline(examList, line)) {
        istringstream iss(line);
        if (getline(iss, name, '|') && getline(iss, metadataPath)) {
            if (name == examName) {
                return metadataPath;
            }
        }
    }

    return "";
}

string ExamManager::getQuestionsFilePath(const string& metadataPath) {
    ifstream metadataFile(metadataPath);
    if (!metadataFile) {
        cerr << "Error: Unable to open metadata file " << metadataPath << "\n";
        return "";
    }

    string line, questionFilePath;
    while (getline(metadataFile, line)) {
        if (line.find("Questions File:") != string::npos) {
            questionFilePath = line.substr(line.find(":") + 2);  // Extract path
            break;
        }
    }

    return questionFilePath;
}

void ExamManager::sendExamQuestions(int sock, const string& examName) {
    string metadataPath = getMetadataFilePath(examName);
    if (metadataPath.empty()) {
        string errorMsg = "Error: Exam not found.\n";
        send(sock, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }

    string questionFilePath = getQuestionsFilePath(metadataPath);
    if (questionFilePath.empty()) {
        string errorMsg = "Error: No questions file found.\n";
        send(sock, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }

    ifstream questionFile(questionFilePath);
    if (!questionFile) {
        string errorMsg = "Error: Unable to open questions file.\n";
        send(sock, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }

    string questionData, line;
    while (getline(questionFile, line)) {
        questionData += line + "\n";
    }
    
    if (questionData.empty()) {
        questionData = "Error: Questions file is empty.\n";
    }
    send(sock, questionData.c_str(), questionData.size(), 0);
}
