#include "server.h"
#include <cctype>
#define INT_MIN -1000

static vector<string> exams;
map<int, string> Server::socketToUsername;

pthread_mutex_t file_mutex1 = PTHREAD_MUTEX_INITIALIZER; // global variables
pthread_mutex_t file_mutex2 = PTHREAD_MUTEX_INITIALIZER; // exam log file
pthread_mutex_t file_mutex3 = PTHREAD_MUTEX_INITIALIZER; // analysis file

Server::Server(int port) {
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        cerr << "Error: Could not create server socket\n";
        exit(EXIT_FAILURE);
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        cerr << "Error: Could not bind to port\n";
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 5) == -1) {
        cerr << "Error: Could not listen for connections\n";
        exit(EXIT_FAILURE);
    }
    cout << "[+] Server started on port " << port << endl;
}

void Server::start() {
    AuthManager();
    ExamManager em;
    exams = em.load_exam_metadata("../data/exams/exam_list.txt");
    while (true) {
        int client_socket = accept(server_socket, nullptr, nullptr);
        pthread_t thread;
        pthread_create(&thread, nullptr, handle_client, &client_socket);
        pthread_detach(thread);
    }
}

void Server::analyzeExam(const string& examName, int sock, bool isStudent) {
    string filePath = "../data/results/exam_" + examName + "_analysis.txt";
    ifstream file(filePath);

    if (!file.is_open()) {
        cerr << "Failed to open analysis file.\n";
        string ms1 = "Analysis of this exam has not been done yet.";
        string ms2 = "The possible cause may be that no student has attempted this exam.";
        send(sock, ms1.c_str(), ms1.size(), 0);
        sleep(1);
        send(sock, ms2.c_str(), ms2.size(), 0);
        return;
    }

    string line;
    getline(file, line);
    vector<string> correctAnswers;
    stringstream ss(line);
    char ans;
    while (ss >> ans) {
        correctAnswers.push_back(string(1, ans));
    }

    int numQuestions = correctAnswers.size();

    vector<pair<string, vector<pair<string, int>>>> studentDataVector;

    while (getline(file, line)) {
        if (line.empty()) continue;
        stringstream ls(line);
        string studentID;
        ls >> studentID;

        vector<pair<string, int>> answers;
        for (int i = 0; i < numQuestions; ++i) {
            string answer;
            int time;
            ls >> answer >> time;
            if (answer == "-") {
                answers.push_back({"NA", time});
            } else {
                answers.push_back({answer, time});
            }
        }
        studentDataVector.push_back({studentID, answers});
    }

    file.close();
    int totalStudents = studentDataVector.size();

    vector<int> questionAttempts(numQuestions, 0);
    vector<int> questionCorrects(numQuestions, 0);
    vector<int> questionSkipped(numQuestions, 0);
    vector<double> questionTotalTime(numQuestions, 0.0);
    vector<int> studentScores;
    vector<double> studentTimes;
    vector<vector<int>> optionCount(numQuestions, vector<int>(5, 0));

    for (const auto& [studentID, answers] : studentDataVector) {
        int score = 0;
        double totalTime = 0.0;

        for (int i = 0; i < numQuestions; ++i) {
            string ans = answers[i].first;
            int timeSpent = answers[i].second;
            totalTime += timeSpent;

            if (ans == "NA") {
                questionSkipped[i]++;
                optionCount[i][4]++;
            } else {
                int idx = ans[0] - 'A';
                optionCount[i][idx]++;
                questionAttempts[i]++;
                if (ans == correctAnswers[i]) {
                    score += 4;
                    questionCorrects[i]++;
                } else {
                    score -= 1;
                }
            }
            questionTotalTime[i] += timeSpent;
        }

        studentScores.push_back(score);
        studentTimes.push_back(totalTime);
    }

    stringstream report;

    double avgScore = accumulate(studentScores.begin(), studentScores.end(), 0.0) / totalStudents;
    double avgTime = accumulate(studentTimes.begin(), studentTimes.end(), 0.0) / totalStudents;

    vector<int> sortedScores = studentScores;
    sort(sortedScores.begin(), sortedScores.end());
    double medianScore = (totalStudents % 2 == 0)
        ? (sortedScores[totalStudents / 2 - 1] + sortedScores[totalStudents / 2]) / 2.0
        : sortedScores[totalStudents / 2];

    report << "\n-------------------------------Overall Exam Metrics--------------------------------\n";
    report << "Total Students: " << totalStudents << "\n";
    report << "Average Score: " << fixed << setprecision(1) << avgScore << " / " << (numQuestions * 4) << "\n";
    report << "Median Score: " << medianScore << " / " << (numQuestions * 4) << "\n";
    report << "Average Time Spent: " << fixed << setprecision(1) << avgTime << " s\n";
    report << "-----------------------------------------------------------------------------------\n\n";

    report << "------------------------------------Per-Question Performance------------------------------------------\n\n";
    report << "| Question | # Attempted | # Correct  | # Wrong | # Skipped | % Correct | Avg Time (s) | Difficulty  |\n";
    report << "------------------------------------------------------------------------------------------------------\n";

    for (int i = 0; i < numQuestions; ++i) {
        int attempted = questionAttempts[i];
        int correct = questionCorrects[i];
        int skipped = questionSkipped[i];
        int wrong = attempted - correct;

        double percentCorrect = (totalStudents > 0) ? (100.0 * correct / totalStudents) : 0.0;
        double avgQTime = (totalStudents > 0) ? (questionTotalTime[i] / totalStudents) : 0.0;

        string difficulty;
        if (percentCorrect >= 70.0) difficulty = "Easy";
        else if (percentCorrect >= 30.0) difficulty = "Medium";
        else difficulty = "Hard";

        report << "|    Q" << setw(2) << (i + 1) << "   | ";
        report << setw(11) << attempted << " | ";
        report << setw(9) << correct << " | ";
        report << setw(7) << wrong << " | ";
        report << setw(9) << skipped << " | ";
        report << setw(9) << fixed << setprecision(1) << percentCorrect << "% | ";
        report << setw(12) << fixed << setprecision(1) << avgQTime << " | ";
        report << setw(11) << difficulty << " |\n";
        report << "------------------------------------------------------------------------------------------------------\n";
    }
    report << "\n";

    report << "-------------------Answer-Option Distribution----------------------\n\n";
    report << "| Question |  A  |  B  |  C  |  D  | NA  |\n";
    report << "------------------------------------------\n";

    for (int i = 0; i < numQuestions; ++i) {
        report << "|   Q" << setw(2) << (i + 1) << "    |";
        for (int j = 0; j < 5; ++j) {
            report << " " << setw(3) << optionCount[i][j] << " |";
        }
        report << "\n";
        report << "------------------------------------------\n";
    }
    report << "\n";

    send(sock, report.str().c_str(), report.str().size(), 0);
    report.str("");
    report.clear();

    struct StudentRankData {
        string id;
        int score;
        double time;
        int attempted;
        int wrong;
        int originalIndex;
    };

    vector<StudentRankData> leaderboard;
    for (int i = 0; i < studentDataVector.size(); ++i) {
        const auto& [id, responses] = studentDataVector[i];
        int score = studentScores[i];
        double totalTime = studentTimes[i];

        int attempted = 0, wrong = 0;
        for (int j = 0; j < responses.size(); ++j) {
            string ans = responses[j].first;
            if (ans != "NA") {
                attempted++;
                if (ans != correctAnswers[j]) wrong++;
            }
        }

        leaderboard.push_back({id, score, totalTime, attempted, wrong, i});
    }

    sort(leaderboard.begin(), leaderboard.end(), [](const StudentRankData& a, const StudentRankData& b) {
        if (a.score != b.score) return a.score > b.score;
        return a.time < b.time;
    });

    report << "----------------------------------Leaderboard------------------------------------------\n\n";
    report << "|Sr No.| Student ID | Total Marks | Rank |  % Marks  | Avg Time/Q | Attempted | Wrong |\n";
    report << "---------------------------------------------------------------------------------------\n";

    int rank = 1, srno = 1;
    int totalMarks = correctAnswers.size() * 4;

    for (const auto& s : leaderboard) {
        double percentMarks = (100.0 * s.score) / totalMarks;
        double avgTimePerQ = s.time / correctAnswers.size();

        report << "| " << setw(5) << left << srno++ << "|";
        report << setw(11) << left << s.id << " | ";
        report << setw(6) << right << s.score << " / " << totalMarks << " | ";
        report << setw(4) << right << rank++ << " | ";
        report << setw(8) << fixed << setprecision(1) << percentMarks << "% | ";
        report << setw(8) << fixed << setprecision(1) << avgTimePerQ << " s | ";
        report << setw(9) << right << s.attempted << " | ";
        report << setw(5) << right << s.wrong << " |\n";
        report << "---------------------------------------------------------------------------------------\n";
    }

    report << "\n";
    sleep(1);
    send(sock, report.str().c_str(), report.str().size(), 0);

    if(isStudent) return;
    while (true) {
        char buffer[10];
        int recvBytes = recv(sock, buffer, sizeof(buffer), 0);
        if (recvBytes <= 0) return;

        int opt = atoi(buffer);
        if (opt == 0 || opt < 1 || opt > leaderboard.size()) return;

        const auto& selectedStudent = leaderboard[opt - 1];
        int idxInVector = selectedStudent.originalIndex;
        const auto& selectedResponses = studentDataVector[idxInVector].second;

        ostringstream out;
        int totalQuestions = correctAnswers.size();
        int score = 0, attempted = 0, wrong = 0, totalTime = 0;

        out << "\n========== Attempt Details for Student ID: " << selectedStudent.id << " ==========\n\n";
        out << "Qno. |     Status     | Marks | Selected | Correct | Time\n";
        out << "--------------------------------------------------------\n";
        for (int i = 0; i < totalQuestions; ++i) {
            string selected = selectedResponses[i].first;
            int timeSpent = selectedResponses[i].second;

            string status = "not attempted";
            string mark = "0";

            if (selected != "NA") {
                attempted++;
                if (selected == correctAnswers[i]) {
                    status = "correct";
                    mark = "+4";
                    score += 4;
                } else {
                    status = "wrong";
                    mark = "-1";
                    score -= 1;
                    wrong++;
                }
            }

            totalTime += timeSpent;

            out << setw(4) << right << i + 1 << " | ";
            out << setw(14) << left << status << " | ";
            out << setw(5) << right << mark << " | ";
            out << setw(8) << left << (selected == "NA" ? "-" : selected) << " | ";
            out << setw(7) << left << correctAnswers[i] << " | ";
            out << timeSpent << "s\n";
        }
        out << "----------------------------------------------------------\n";
        out << "\nTotal Marks Obtained   : " << score << " / " << totalMarks << "\n";
        out << "Total Questions        : " << totalQuestions << "\n";
        out << "Attempted Questions    : " << attempted << "\n";
        out << "Wrong Answers          : " << wrong << "\n";
        out << "Total Time Spent       : " << totalTime << "s\n";
        out << "----------------------------------------------------------\n";

        string res = out.str() + report.str();
        send(sock, res.c_str(), res.size(), 0);
    }
}

void Server::receiveStudentAnswers(int sock, const string& examName) {
    char buffer[256] = {0};
    int bytesReceived = recv(sock, buffer, sizeof(buffer), 0);
    if (bytesReceived <= 0) {
        cerr << "Error: Failed to receive answers from client.\n";
        return;
    }
    char ack = 'y';
    send(sock, &ack, 1, 0);
    string data(buffer);
    if (data.substr(0, 7) != "ANSWERS") {
        cerr << "Invalid data received format.\n";
        return;
    }

    string studentId = socketToUsername[sock];

    string answerFile = "../data/exams/answers_" + examName + ".txt";
    vector<int> correctAnswers;
    ifstream answerIn(answerFile);
    string line;
    while (getline(answerIn, line)) {
        line.erase(remove_if(line.begin(), line.end(), ::isspace), line.end());
        if (!line.empty()) 
            correctAnswers.push_back(line[0] - 'A');
        
    }
    answerIn.close();

    istringstream dataStream(data.substr(8));
    string entry;
    int totalQuestions = correctAnswers.size();
    vector<int> perQuestionMarks(totalQuestions, 0);
    vector<int> perQuestionTime(totalQuestions, 0);
    vector<int> perQuestionAnswer(totalQuestions, -1);
    int totalMarks = 0, totalTimeSpent = 0;
    int attemptedCount = 0, wrongCount = 0;
    const int positiveMark = 4, negativeMark = -1;

    while (getline(dataStream, entry)) {
        int qIdx, answer, timeSpent;
        char delim;
        istringstream entryStream(entry);
        entryStream >> qIdx >> delim >> answer >> delim >> timeSpent;
        int marks = 0;
        if (answer != -1) {
            attemptedCount++;
            if (answer == correctAnswers[qIdx]) {
                marks = positiveMark;
            } else {
                marks = negativeMark;
                wrongCount++;
            }
        }

        perQuestionMarks[qIdx] = marks;
        perQuestionTime[qIdx] = timeSpent;
        perQuestionAnswer[qIdx] = answer;
        totalMarks += marks;
        totalTimeSpent += timeSpent;
    }

    string currDateTime = getCurrentDateTime();
    string perfFile = "../data/results/student_" + studentId + "_attempts.txt";
    ofstream perfOut(perfFile, ios::app);
    perfOut << examName << "|";
    perfOut << currDateTime << "|";
    perfOut << totalMarks << "|";
    perfOut << totalQuestions*4 << "|";
    perfOut << "../data/results/student_"+studentId+"_"+examName+"_performance.txt\n";
    perfOut.close();

    string scoreFile = "../data/results/student_"+studentId+"_"+examName+"_performance.txt";
    ofstream scoreOut(scoreFile, ios::app);
    scoreOut << "START\n";
    scoreOut << currDateTime << "|";
    scoreOut << examName + "|";
    scoreOut << totalMarks << "|" << totalQuestions*4 << "|";
    scoreOut << totalQuestions<< "|" <<attemptedCount << "|" << wrongCount <<"|";
    scoreOut << totalTimeSpent <<"\nEND\n";

    for (size_t i = 0; i < perQuestionMarks.size(); ++i) {
        scoreOut << "Q" << (i + 1) << "|";
        scoreOut << perQuestionMarks[i] << "|";
        if (perQuestionAnswer[i] != -1) {
            scoreOut  << static_cast<char>('A' + perQuestionAnswer[i]) << "|";
        }
        else{
            scoreOut << "NA|";
        }
        scoreOut << perQuestionTime[i] << "s\n";
    }
    scoreOut.close();

    pthread_mutex_lock(&file_mutex2);
    string attemptFile = "../data/results/exam_log.txt";
    ofstream attemptOut(attemptFile, ios::app);
    attemptOut <<studentId<<": " <<examName << ": " << getCurrentDateTime() << "\n";
    attemptOut.close();
    pthread_mutex_unlock(&file_mutex2);

    string analysisFile = "../data/results/exam_" + examName + "_analysis.txt";
    pthread_mutex_lock(&file_mutex3);
    ifstream infile(analysisFile);
    bool fileExists = infile.good();
    infile.close();

    ofstream analysisOut;
    if (!fileExists) {
        analysisOut.open(analysisFile, ios::out);
        for (size_t i = 0; i < correctAnswers.size(); ++i) {
            analysisOut << static_cast<char>('A' + correctAnswers[i]) << " ";
        }
        analysisOut << "\n";
    } else {
        analysisOut.open(analysisFile, ios::app);
    }
    analysisOut << studentId;
    for (size_t i = 0; i < perQuestionAnswer.size(); ++i) {
        if (perQuestionAnswer[i] == -1) {
            analysisOut << " - " << perQuestionTime[i];
            continue;
        }
        analysisOut <<" " << static_cast<char>('A' + perQuestionAnswer[i]) << " " << perQuestionTime[i];
    }
    analysisOut << "\n";
    analysisOut.close();
    pthread_mutex_unlock(&file_mutex3);
    cout << "[✔] Evaluation complete for " << studentId << " on '" << examName << "'.\n";
}

string Server::getCurrentDateTime() {
    time_t now = time(nullptr);
    tm* localTime = localtime(&now);

    ostringstream oss;
    oss << put_time(localTime, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

void Server::handleStudentExamRequest(int sock, ExamManager exam) {
    char buffer[1024] = {0};
    int bytesReceived = recv(sock, buffer, sizeof(buffer), 0);
    if (bytesReceived <= 0) {
        cerr << "Error: Failed to receive exam selection from client.\n";
        return;
    }

    int examNumber = atoi(buffer);
    if(examNumber==0) return;

    bool fileExist = false;
    if(examNumber < 0) fileExist = true;
    string selectedExamName;
    istringstream iss(exams[abs(examNumber) - 1]);
    string line;
    
    while (getline(iss, line)) {
        if (line.find("Exam Name:") != string::npos) {
            selectedExamName = line.substr(line.find(":") + 2);
            break;
        }
    }

    if(!fileExist){
        exam.sendExamQuestions(sock, selectedExamName);
        cout << "[+] question paper send successfully !\n";
    }
    else{
        cout << "[+] file already exist on client side !\n";
    }
    
    memset(buffer, 0, sizeof(buffer));
    sleep(2);
    recv(sock, buffer, sizeof(buffer), 0);
    string response(buffer);

        if (response == "y" || response == "Y") {
            string studentId = socketToUsername[sock];
            
            char typeBuf[32] = {0};
            recv(sock, typeBuf, sizeof(typeBuf), 0);
            string examTypeBuffer(typeBuf);
            string examName = selectedExamName;
            bool alreadyAttempted = false;
        
            if (examTypeBuffer == "s") {
                string perfFile = "../data/results/student_" + studentId + "_attempts.txt";
                ifstream infile(perfFile);
                if (infile.is_open()) {
                    string line;
                    while (getline(infile, line)) {
                        size_t pos = line.find('|');
                        if (pos != string::npos) {
                            string attemptedExam = line.substr(0, pos);
                            attemptedExam.erase(0, attemptedExam.find_first_not_of(" \t\n\r"));
                            attemptedExam.erase(attemptedExam.find_last_not_of(" \t\n\r") + 1);
        
                            if (attemptedExam == examName) {
                                alreadyAttempted = true;
                                break;
                            }
                        }
                    }
                    infile.close();
                }
                if (alreadyAttempted) {
                    char message = 'y';
                    send(sock, &message, 1, 0);
                    sleep(1);
                    return;
                }
                else{
                    char message = 'n';
                    send(sock, &message, 1, 0);
                    sleep(1);
                }
            }
            sleep(3);
            receiveStudentAnswers(sock, examName);
        }
}

bool Server::handle_authentication(int sock, const string& command, const string& user_type, const string& username, const string& password) {
    if (command == "LOGIN") {
        if (AuthManager::authenticate_user(username, password, user_type)) {
            send(sock, "AUTHENTICATION_SUCCESS", strlen("AUTHENTICATION_SUCCESS"), 0);
            cout << username << " logged in successfully as " << user_type << endl;
            return true;
        } else {
            send(sock, "AUTHENTICATION_FAILED", strlen("AUTHENTICATION_FAILED"), 0);
            cerr << "Authentication failed for " << username << endl;
            return false;
        }
    } else if (command == "REGISTER") {
        if (AuthManager::register_user(username, password, user_type)) {
            send(sock, "REGISTER_SUCCESS", strlen("REGISTER_SUCCESS"), 0);
            cout << username << " registered successfully as " << user_type << endl;
            return true;
        } else {
            send(sock, "REGISTER_FAILED", strlen("REGISTER_FAILED"), 0);
            cerr << "Registration failed for " << username << endl;
            return false;
        }
        
    }
    return false;
}

void Server::handleViewPerformance(int clientSock, const string& studentId) {
    string filename = "../data/results/student_" + studentId + "_attempts.txt";
    ifstream file(filename);
    if (!file.is_open()) {
        string err = "[!] No exam data found for student.";
        send(clientSock, err.c_str(), err.size() + 1, 0);
        return;
    }

    map<string, vector<tuple<string, string, string>>> examMap;
    string line;
    while (getline(file, line)) {
        stringstream ss(line);
        string examName, timestamp, marksObtained, totalMarks, perfPath;
        getline(ss, examName, '|');
        getline(ss, timestamp, '|');
        getline(ss, marksObtained, '|');
        getline(ss, totalMarks, '|');
        getline(ss, perfPath);

        examMap[examName].emplace_back(timestamp, marksObtained, totalMarks + "|" + perfPath);
    }
    file.close();

    while (true) {
        string dashboard = "\n========== Attempted Exams ==========\n\n";
        vector<string> examNames;
        int index = 1;
        for (auto& pair : examMap) {
            dashboard += "[" + to_string(index++) + "] " + pair.first + " (" + to_string(pair.second.size()) + " attempts)\n";
            examNames.push_back(pair.first);
        }
        dashboard += "\n[0] Back to Main Menu\n--------------------------------------\n";
        dashboard += "select from above: ";

        send(clientSock, dashboard.c_str(), dashboard.size() + 1, 0);

        char examChoiceBuf[10] = {0};
        int bytesReceived = recv(clientSock, examChoiceBuf, sizeof(examChoiceBuf), 0);
        if (bytesReceived <= 0) {
            cerr << "Error: Failed to receive exam selection from client.\n";
            return;
        }
        int examChoice = atoi(examChoiceBuf);
        if (examChoice == 0) break;
        if (examChoice < 1 || examChoice > examNames.size()){
            string mesg = "[!] Invalid option! please select a valid exam.";
            send(clientSock, mesg.c_str(), mesg.size(), 0);
            usleep(1200000);
            continue;
        }

        string selectedExam = examNames[examChoice - 1];
        auto& attempts = examMap[selectedExam];

        string attemptList = "\n=============="+selectedExam+" attempts==============\n\n";
        for (int i = 0; i < attempts.size(); ++i) {
            string timestamp = get<0>(attempts[i]);
            string marksObtained = get<1>(attempts[i]);
            string totalMarks = get<2>(attempts[i]).substr(0, get<2>(attempts[i]).find('|'));

            attemptList += "[" + to_string(i + 1) + "] Attempt on: " + timestamp +
                            " Marks Obtained: " + marksObtained + " / " + totalMarks + "\n";
        }
        attemptList += "\n[0] Back to Exam List\n";
        attemptList += "--------------------------------------------------------\n";
        attemptList += "Select an attempt to view details: ";

        send(clientSock, attemptList.c_str(), attemptList.size() + 1, 0);
        
        char attemptChoiceBuf[10] = {0};
        recv(clientSock, attemptChoiceBuf, sizeof(attemptChoiceBuf), 0);
        int attemptChoice = atoi(attemptChoiceBuf);

        if (attemptChoice == 0) continue;
        if (attemptChoice < 1 || attemptChoice > attempts.size()){
            string mesg = "[!] Invalid option! please select a valid attempt.";
            send(clientSock, mesg.c_str(), mesg.size(), 0);
            usleep(1200000);
            continue;
        } 

        string selectedTimestamp = get<0>(attempts[attemptChoice - 1]);
        string perfFilePath = get<2>(attempts[attemptChoice - 1]);
        perfFilePath = perfFilePath.substr(perfFilePath.find('|') + 1);

        ifstream perfFile(perfFilePath);
        if (!perfFile.is_open()) {
            string error = "Error: Performance file not found.\n";
            attemptList += "\n[0] Back to Exam List\n";
            error += "--------------------------------------------------------\n";
            error += "select from above: ";
            send(clientSock, error.c_str(), error.size() + 1, 0);
            continue;
        }

        string formatted, line, examName;
        while (getline(perfFile, line)) {
            if (line == "START") {
                string summaryLine;
                if (!getline(perfFile, summaryLine)) break;

                stringstream ss(summaryLine);
                string timestamp, marksObtained, totalMarks, totalQuestions, attempted, wrong, totalTime;
                getline(ss, timestamp, '|');
                if (timestamp != selectedTimestamp) {
                    while (getline(perfFile, line) && line != "START");
                    if (line == "START") {
                        perfFile.seekg(-line.length()-1, ios::cur); 
                    }
                    continue;
                }

                getline(ss, examName, '|');
                getline(ss, marksObtained, '|');
                getline(ss, totalMarks, '|');
                getline(ss, totalQuestions, '|');
                getline(ss, attempted, '|');
                getline(ss, wrong, '|');
                getline(ss, totalTime, '|');

                for (const auto& exam : exams) {
                    istringstream iss(exam);
                    string exam_name, exam_type, start_time, duration, line;
                    bool include_exam = false;
                    while (getline(iss, line)) {
                        if (line.find("Exam Name:") != string::npos) {
                            exam_name = line.substr(line.find(":") + 2);
                            if (exam_name == examName) 
                                include_exam = true;
                        }
                        if (line.find("Duration (minutes):") != string::npos)
                            duration = line.substr(line.find(":") + 2);
                        if (line.find("Exam type:") != string::npos) 
                            exam_type = line.substr(line.find(":") + 2);
                        if (line.find("Start Time:") != string::npos) 
                            start_time = line.substr(line.find(":") + 2);
                    }
                    if (include_exam) {
                        if(exam_type == "q") break;
                        tm tm = {};
                        istringstream ss(start_time);
                        int dur = stoi(duration);
                    
                        ss >> get_time(&tm, "%Y-%m-%d %H:%M:%S");
                        if (ss.fail()) {
                            std::cerr << "Parse failed\n";
                            break;
                        }
                        time_t time_value = std::mktime(&tm);
                        time_t later = time_value + dur*60;
                        time_t current_time = std::time(nullptr);
                        if(current_time < later){
                            string msg = "Exam is still going on.";
                            send(clientSock, msg.c_str(), msg.size(), 0);
                            sleep(1);
                            return;
                        }
                        break;
                    }
                }
                
                formatted = "\n========== Attempt Details ==========\n\n";
                formatted += "Exam: " + examName + "\n";
                formatted += "Attempt Date: " + timestamp + "\n\n";
                formatted += "Total Marks Obtained   : " + marksObtained + " / " + totalMarks + "\n";
                formatted += "Total Questions        : " + totalQuestions + "\n";
                formatted += "Attempted Questions    : " + attempted + "\n";
                formatted += "Wrong Answers          : " + wrong + "\n";
                formatted += "Total Time Spent       : " + totalTime + "s\n\n";

                string ansFilePath = "../data/exams/answers_" + examName + ".txt";
                ifstream ansFile(ansFilePath);
                vector<string> answers;
                if (ansFile.is_open()) {
                    string ansLine;
                    while (getline(ansFile, ansLine)) {
                        answers.push_back(ansLine);
                    }
                    ansFile.close();
                }

                formatted += "Qno.  | Status  | Marks | Selected | Correct | Time\n";
                formatted += "--------------------------------------------------------\n";
                while (getline(perfFile, line) && line != "END");
                int qNum = 1;
                while (getline(perfFile, line)) {
                    if (line == "START") break;

                    stringstream qss(line);
                    string questionStr, markStr, optStr, timeStr;
                    getline(qss, questionStr, '|');
                    getline(qss, markStr, '|');
                    getline(qss, optStr, '|');
                    getline(qss, timeStr, 's');

                    string status, markDisplay, selected, correct;
                    int mark = stoi(markStr);
                    selected = (optStr == "NA") ? "-" : optStr;
                    correct = (qNum - 1 < answers.size()) ? answers[qNum - 1] : "?";

                    if (optStr == "NA") {
                        status = "NA";
                        markDisplay = "-";
                    } else {
                        status = (mark == -1) ? "wrong" : "correct";
                        markDisplay = (mark > 0 ? "+" : "") + markStr;
                    }

                    stringstream row;
                    row << setw(5)  << qNum << " | "
                        << setw(7) << status << " | "
                        << setw(5)  << markDisplay << " | "
                        << setw(8)  << selected << " | "
                        << setw(7)  << correct << " | "
                        << timeStr << "s\n";
                    formatted += row.str();

                    qNum++;
                }
                formatted += "\n--------------------------------------------------------\n";
                perfFile.close();
                send(clientSock, formatted.c_str(), formatted.size() + 1, 0);

                string examFilePath = "../data/exams/questions_" + examName + ".txt";
                ifstream examFile(examFilePath);
                if (examFile.is_open()) {
                    formatted = examName +"\n";
                    string qLine;
                    int qNum = 1;
                    while (getline(examFile, qLine)) {
                        if (qLine.empty()) {
                            formatted += "\n";
                            continue;
                        }
                        if (qLine[0] == ' ') {
                            formatted += "Q" + to_string(qNum++) + "." + qLine + "\n";
                        } else {
                            formatted += qLine + "\n";
                        }
                    }
                    formatted += "--------------------------END OF QUESTION PAPER------------------------------\n";
                    examFile.close();
                }
                break;
            }
        }
        sleep(1);
        send(clientSock, formatted.c_str(), formatted.size() + 1, 0);
       
        char leaderboardbuf[10] = {0};
        bytesReceived = recv(clientSock, leaderboardbuf, sizeof(leaderboardbuf), 0);
        if (bytesReceived <= 0) {
            cerr << "Error: Failed to receive exam selection from client.\n";
            return;
        }
        int leaderboard = atoi(leaderboardbuf);
        if(leaderboard==0 || leaderboard!=1) continue;

        analyzeExam(selectedExam, clientSock, true);
        break;
    }
}

void Server::sendAvailableExams(int sock, const string& username, vector<string>& examNames) {
    string all_exams;
    int qno = 1;
    examNames.clear();

    for (const auto& exam : exams) {
        istringstream iss(exam);
        ostringstream filteredExam;
        string line;
        string instructor_name;
        string exam_name, exam_type;
        bool include_exam = false;

        while (getline(iss, line)) {
            if (line.find("Instructor:") != string::npos) {
                instructor_name = line.substr(line.find(":") + 2);
                if (instructor_name == username) {
                    include_exam = true;
                }
            }

            if (line.find("Exam Name:") != string::npos)
                exam_name = line.substr(line.find(":") + 2);
            if (line.find("Exam type:") != string::npos){
                exam_type = line.substr(line.find(":") + 2);
                if(exam_type == "g") exam_type = "Exam type: Scheduled Test";
                else exam_type = "Exam type: Practice Test";
                filteredExam << exam_type << " | ";
                continue;
            }
            if (line.find("Instructor:") != std::string::npos) continue;
            filteredExam << line << " | ";
        }
        if (include_exam) {
            examNames.push_back(exam_name);
            all_exams += to_string(qno++) + ". " + filteredExam.str() + "\n";
        }
    }

    if (all_exams.empty()) {
        all_exams = "[!] You have not uploaded any exam.";
    }

    all_exams += "\0";
    send(sock, all_exams.c_str(), all_exams.size(), 0);
}

void* Server::handle_client(void* client_socket) {
    
    int sock = *(int*)client_socket;
    char buffer[1024] = {0};
    string command, user_type, username, password;
    int attempts=0;
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        recv(sock, buffer, sizeof(buffer), 0);
        string request(buffer);
        if (request == "exit") break;
    
        istringstream iss(request);
        iss >> command >> user_type >> username >> password;
    
        pthread_mutex_lock(&file_mutex1);
        bool authenticated = handle_authentication(sock, command, user_type, username, password);
        if (authenticated) {
            Server::socketToUsername[sock] = username;
        }
        pthread_mutex_unlock(&file_mutex1);
    
        if (authenticated) break;
    }
    
    ExamManager exam_manager;
    if (user_type == "student") {
        char mesg[32]={0};
        int bytes_received = recv(sock, mesg, sizeof(mesg), 0);
        if (bytes_received <= 0) return nullptr;
        string examName(mesg);
        examName.erase(0, examName.find_first_not_of(" \t\n\r")); 
        examName.erase(examName.find_last_not_of(" \t\n\r") + 1); 
        if(examName!="n"){
            receiveStudentAnswers(sock, examName);
        }

        while (true){
            memset(buffer, 0, sizeof(buffer));
            int bytes_received = recv(sock, buffer, sizeof(buffer), 0);
            if (bytes_received <= 0) break;
            buffer[bytes_received] = '\0';
            string request(buffer);
            
            if (request == "1") {
                string all_exams;
                int qno = 1;
                for (auto& exam : exams) {
                    string formattedExam;
                    istringstream iss(exam);
                    string line;
                    
                    while (getline(iss, line)) 
                        formattedExam += line + " | ";
        
                    if (!formattedExam.empty() && formattedExam.back() == ' ')
                        formattedExam.pop_back();

                    all_exams += to_string(qno) + ". " + formattedExam + "\n";
                    qno++;
                }
            
                if (all_exams.empty())
                    all_exams = "No exams available.";
                
                send(sock, all_exams.c_str(), all_exams.size(), 0);
                if(all_exams!="No exams available.")
                    handleStudentExamRequest(sock, exam_manager);
            }
            
            else if (request == "2") {
                handleViewPerformance(sock, username);
            }
            else if(request == "3") break;
        }
    }
    else if (user_type == "instructor") {
        while (true){
            memset(buffer, 0, sizeof(buffer));
            int bytes_received = recv(sock, buffer, sizeof(buffer), 0);
            buffer[bytes_received] = '\0';
            string request(buffer);
            string response = "";

            if (request == "1") {
                memset(buffer, 0, sizeof(buffer));
                recv(sock, buffer, sizeof(buffer), 0);
                string examData(buffer);
            
                size_t pos1 = examData.find("|");
                string examName = examData.substr(0, pos1);

                size_t pos2 = examData.find("|", pos1 + 1);
                string exam_type = examData.substr(pos1+1, pos2-pos1-1);  

                size_t pos3 = examData.find("|", pos2 + 1);
                int examDuration = atoi(examData.substr(pos2 + 1, pos3 - pos2 - 1).c_str());

                size_t pos4 = examData.find("|", pos3 + 1);
                string examFileName = "../data/exams/" + examData.substr(pos3 + 1, pos4 - pos3 - 1);

                string start_time = "";
                if(exam_type=="g" || exam_type=="G"){
                    start_time = examData.substr(pos4+1);
                    tm tm_input = {};
                    istringstream ss(start_time);
                    ss >> get_time(&tm_input, "%Y-%m-%d %H:%M:%S");
                    if (ss.fail()) {
                        std::cerr << "Invalid format. Please use YYYY-MM-DD HH:MM:SS\n";
                        response = "Invalid format. Please use YYYY-MM-DD HH:MM:SS";
                        send(sock,response.c_str(),response.size(),0);
                        continue;
                    }
                }
                ifstream file("../data/exams/exam_list.txt");
                bool found = false;

                if (file.is_open()) {
                    string line;
                    while (getline(file, line)) {
                        size_t pos = line.find('|');
                        if (pos != string::npos) {
                            string name = line.substr(0, pos);
                            if (name == examName) {
                                found = true;
                                break;
                            }
                        }
                    }
                    file.close();
                } else {
                    cerr << "Failed to open exam list file.\n";
                }

                if (found) {
                    response = "Exam '" + examName + "' already exists.\n";
                } else {
                    if (exam_manager.parse_exam(exam_type ,examFileName, examName, username, examDuration, start_time)) {
                        pthread_mutex_lock(&file_mutex1);
                        exams = exam_manager.load_exam_metadata("../data/exams/exam_list.txt");
                        pthread_mutex_unlock(&file_mutex1);
                        response = "Exam successfully uploaded!"; 
                    } else response = "Error: Invalid exam format!";      
                }
                send(sock,response.c_str(),response.size(),0);
            }
            else if (request == "3"){
                vector<string> examNames;
                sendAvailableExams(sock, username, examNames);
                if (examNames.empty()) continue;

                char buffer[10];
                int len = recv(sock, buffer, sizeof(buffer), 0);
                if (len <= 0) {
                    cerr << "Failed to receive selection.\n";
                    break;
                }
            
                buffer[len] = '\0';
                int selection = atoi(buffer);
                            if (selection <= 0 || selection > (int)examNames.size()) {
                    string err = "[!] Invalid exam selection.";
                    send(sock, err.c_str(), err.size(), 0);
                    continue;
                }
                string selectedExamName = examNames[selection - 1];
                analyzeExam(selectedExamName, sock, false);
            }
            else if (request == "4") {
                vector<string> all_exams;
                sendAvailableExams(sock, username, all_exams);
            }
            else if (response=="5") break;
        }
    }
    close(sock);
    cout << "[-] client[ "<<username<<" ] disconnected!"<<endl;
    return nullptr;
}