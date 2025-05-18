#include "server.h"
#include <cctype>
#define INT_MIN -1000

static vector<string> exams;
map<int, string> Server::socketToUsername;

pthread_mutex_t file_mutex1 = PTHREAD_MUTEX_INITIALIZER; // global variables
pthread_mutex_t file_mutex2 = PTHREAD_MUTEX_INITIALIZER; // exam log file
pthread_mutex_t file_mutex3 = PTHREAD_MUTEX_INITIALIZER; // analysis file

// Constructor to initialize and start the server on the specified port
Server::Server(int port) {
    // Create a TCP socket (IPv4, stream-oriented)
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        cerr << "Error: Could not create server socket\n";
        exit(EXIT_FAILURE);
    }

    // Define server address structure and zero-initialize
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;            // IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY;    // Accept connections on any IP address
    server_addr.sin_port = htons(port);          // Host-to-network short (port in network byte order)

    // Bind the socket to the specified IP and port
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        cerr << "Error: Could not bind to port\n";
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections (backlog size 5)
    if (listen(server_socket, 5) == -1) {
        cerr << "Error: Could not listen for connections\n";
        exit(EXIT_FAILURE);
    }

    cout << "[+] Server started on port " << port << endl;
}


// Starts the server loop to handle incoming client connections
void Server::start() {
    // Initialize the authentication manager (e.g., load user credentials)
    AuthManager();

    // Create an instance of ExamManager
    ExamManager em;

    // Load exam metadata from file into the exams list
    exams = em.load_exam_metadata("../data/exams/exam_list.txt");

    // Start an infinite loop to accept and handle incoming client connections
    while (true) {
        // Accept a new client connection
        int client_socket = accept(server_socket, nullptr, nullptr);

        // Create a new thread to handle the client
        pthread_t thread;
        pthread_create(&thread, nullptr, handle_client, &client_socket);

        // Detach the thread so that resources are automatically reclaimed when it exits
        pthread_detach(thread);
    }
}

void Server::analyzeExam(const string& examName, int sock, bool isStudent) {
    // Construct the path to the exam analysis results file based on examName
    string filePath = "../data/results/exam_" + examName + "_analysis.txt";
    // Open the analysis file for reading
    ifstream file(filePath);

    // Check if file was successfully opened
    if (!file.is_open()) {
        cerr << "Failed to open analysis file.\n";
        // Prepare messages to send to client socket indicating no analysis done
        string ms1 = "Analysis of this exam has not been done yet.";
        string ms2 = "The possible cause may be that no student has attempted this exam.";
        send(sock, ms1.c_str(), ms1.size(), 0);
        sleep(1);
        send(sock, ms2.c_str(), ms2.size(), 0);
        // Exit function early as no data to analyze
        return;
    }

    // Read the first line which contains correct answers for each question
    string line;
    getline(file, line);
    vector<string> correctAnswers;
    stringstream ss(line);
    char ans;
    // Extract each answer character and store as string in correctAnswers vector
    while (ss >> ans) {
        correctAnswers.push_back(string(1, ans));
    }

    // Store the total number of questions in the exam
    int numQuestions = correctAnswers.size();

    // Vector to hold student data: studentID and their answers with time spent per question
    vector<pair<string, vector<pair<string, int>>>> studentDataVector;

    // Read the rest of the file line by line containing each student's responses
    while (getline(file, line)) {
        if (line.empty()) continue; // Skip empty lines
        stringstream ls(line);
        string studentID;
        // Read student ID from the line
        ls >> studentID;

        vector<pair<string, int>> answers; // Vector to store answers and time for each question
        for (int i = 0; i < numQuestions; ++i) {
            string answer;
            int time;
            // Extract the answer and time spent from the line
            ls >> answer >> time;
            // Replace "-" with "NA" to indicate not attempted
            if (answer == "-") {
                answers.push_back({"NA", time});
            } else {
                answers.push_back({answer, time});
            }
        }
        // Store student ID and their answers in the main vector
        studentDataVector.push_back({studentID, answers});
    }

    // Close the file after reading all data
    file.close();
    // Total number of students who attempted the exam
    int totalStudents = studentDataVector.size();

    // Initialize vectors to store metrics for each question
    vector<int> questionAttempts(numQuestions, 0);      // Number of attempts per question
    vector<int> questionCorrects(numQuestions, 0);      // Number of correct answers per question
    vector<int> questionSkipped(numQuestions, 0);       // Number of skips per question
    vector<double> questionTotalTime(numQuestions, 0.0);// Total time spent per question
    vector<int> studentScores;                           // Score per student
    vector<double> studentTimes;                         // Total time per student
    // Count of options selected per question (A, B, C, D, NA)
    vector<vector<int>> optionCount(numQuestions, vector<int>(5, 0));

    // Iterate over each student's data to compute scores and statistics
    for (const auto& [studentID, answers] : studentDataVector) {
        int score = 0;            // Initialize student's score
        double totalTime = 0.0;   // Initialize total time spent by student

        // Iterate over each question for the current student
        for (int i = 0; i < numQuestions; ++i) {
            string ans = answers[i].first;      // Student's answer
            int timeSpent = answers[i].second;  // Time spent on question
            totalTime += timeSpent;              // Accumulate total time

            // Check if question was skipped (marked as "NA")
            if (ans == "NA") {
                questionSkipped[i]++;            // Increment skipped count for question
                optionCount[i][4]++;             // Increment NA option count
            } else {
                int idx = ans[0] - 'A';          // Convert answer character to index (0-based)
                optionCount[i][idx]++;           // Increment count of selected option
                questionAttempts[i]++;            // Increment attempt count for question
                // Check correctness and update score and correct count
                if (ans == correctAnswers[i]) {
                    score += 4;                   // Add 4 points for correct answer
                    questionCorrects[i]++;        // Increment correct count for question
                } else {
                    score -= 1;                   // Subtract 1 point for wrong answer
                }
            }
            // Accumulate total time for the question (all students)
            questionTotalTime[i] += timeSpent;
        }

        // Save computed score and total time for the current student
        studentScores.push_back(score);
        studentTimes.push_back(totalTime);
    }

    // Prepare a stringstream to build the report output
    stringstream report;

    // Calculate average score and average time spent across all students
    double avgScore = accumulate(studentScores.begin(), studentScores.end(), 0.0) / totalStudents;
    double avgTime = accumulate(studentTimes.begin(), studentTimes.end(), 0.0) / totalStudents;

    // Create a copy of scores and sort to compute median
    vector<int> sortedScores = studentScores;
    sort(sortedScores.begin(), sortedScores.end());
    // Calculate median score (handle even/odd number of students)
    double medianScore = (totalStudents % 2 == 0)
        ? (sortedScores[totalStudents / 2 - 1] + sortedScores[totalStudents / 2]) / 2.0
        : sortedScores[totalStudents / 2];

    // Write overall exam metrics header and summary
    report << "\n-------------------------------Overall Exam Metrics--------------------------------\n";
    report << "Total Students: " << totalStudents << "\n";
    report << "Average Score: " << fixed << setprecision(1) << avgScore << " / " << (numQuestions * 4) << "\n";
    report << "Median Score: " << medianScore << " / " << (numQuestions * 4) << "\n";
    report << "Average Time Spent: " << fixed << setprecision(1) << avgTime << " s\n";
    report << "-----------------------------------------------------------------------------------\n\n";

    // Write per-question performance header and column titles
    report << "------------------------------------Per-Question Performance------------------------------------------\n\n";
    report << "| Question | # Attempted | # Correct  | # Wrong | # Skipped | % Correct | Avg Time (s) | Difficulty  |\n";
    report << "------------------------------------------------------------------------------------------------------\n";

    // Loop through each question and write detailed stats
    for (int i = 0; i < numQuestions; ++i) {
        int attempted = questionAttempts[i];              // Number attempted
        int correct = questionCorrects[i];                // Number correct
        int skipped = questionSkipped[i];                 // Number skipped
        int wrong = attempted - correct;                   // Number wrong

        // Calculate percentage correct and average time per question
        double percentCorrect = (totalStudents > 0) ? (100.0 * correct / totalStudents) : 0.0;
        double avgQTime = (totalStudents > 0) ? (questionTotalTime[i] / totalStudents) : 0.0;

        // Determine difficulty label based on percentage correct
        string difficulty;
        if (percentCorrect >= 70.0) difficulty = "Easy";
        else if (percentCorrect >= 30.0) difficulty = "Medium";
        else difficulty = "Hard";

        // Format and write question stats row in the report
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

    // Write header for answer-option distribution per question
    report << "-------------------Answer-Option Distribution----------------------\n\n";
    report << "| Question |  A  |  B  |  C  |  D  | NA  |\n";
    report << "------------------------------------------\n";

    // Write counts of each option chosen for every question
    for (int i = 0; i < numQuestions; ++i) {
        report << "|   Q" << setw(2) << (i + 1) << "    |";
        for (int j = 0; j < 5; ++j) {
            report << " " << setw(3) << optionCount[i][j] << " |";
        }
        report << "\n";
        report << "------------------------------------------\n";
    }
    report << "\n";

    // Send the accumulated report string to the client socket
    send(sock, report.str().c_str(), report.str().size(), 0);
    // Clear the stringstream buffer after sending
    report.str("");
    report.clear();

    // Define a struct to hold ranking data for students
    struct StudentRankData {
        string id;         
        int score;        
        double time;       
        int attempted;     
        int wrong;         
        int originalIndex;
    };

    // Vector to hold all students' rank data for leaderboard
    vector<StudentRankData> leaderboard;
    for (int i = 0; i < studentDataVector.size(); ++i) {
        const auto& [id, responses] = studentDataVector[i];
        int score = studentScores[i];
        double totalTime = studentTimes[i];

        int attempted = 0, wrong = 0;
        // Count attempted and wrong answers for the student
        for (int j = 0; j < responses.size(); ++j) {
            string ans = responses[j].first;
            if (ans != "NA") {
                attempted++;
                if (ans != correctAnswers[j]) wrong++;
            }
        }

        // Add student's ranking data to leaderboard vector
        leaderboard.push_back({id, score, totalTime, attempted, wrong, i});
    }

    // Sort leaderboard by score descending, and if tie, by time ascending
    sort(leaderboard.begin(), leaderboard.end(), [](const StudentRankData& a, const StudentRankData& b) {
        if (a.score != b.score) return a.score > b.score;
        return a.time < b.time;
    });

    // Write leaderboard header and column titles
    report << "----------------------------------Leaderboard------------------------------------------\n\n";
    report << "|Sr No.| Student ID | Total Marks | Rank |  % Marks  | Avg Time/Q | Attempted | Wrong |\n";
    report << "---------------------------------------------------------------------------------------\n";

    int rank = 1, srno = 1;
    int totalMarks = correctAnswers.size() * 4;

    // Iterate over each student in leaderboard and write their ranking info
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
    sleep(1); // Pause for 1 second before sending leaderboard
    // Send leaderboard report to client socket
    send(sock, report.str().c_str(), report.str().size(), 0);

    // If this is a student client, return after sending report
    if(isStudent) return;

    // Server-side loop to allow querying individual student's attempt details
    while (true) {
        char buffer[10];
        // Receive input option from client (index to view student details)
        int recvBytes = recv(sock, buffer, sizeof(buffer), 0);
        if (recvBytes <= 0) return; // If connection closed or error, exit

        // Convert received buffer to integer option
        int opt = atoi(buffer);
        // Validate option; if invalid or out of range, exit loop
        if (opt == 0 || opt < 1 || opt > leaderboard.size()) return;

        // Retrieve the selected student's data from leaderboard
        const auto& selectedStudent = leaderboard[opt - 1];
        int idxInVector = selectedStudent.originalIndex;
        const auto& selectedResponses = studentDataVector[idxInVector].second;

        // Prepare output stream to build detailed attempt report
        ostringstream out;
        int totalQuestions = correctAnswers.size();
        int score = 0, attempted = 0, wrong = 0, totalTime = 0;

        // Write header for selected student's attempt details
        out << "\n========== Attempt Details for Student ID: " << selectedStudent.id << " ==========\n\n";
        out << "Qno. |     Status     | Marks | Selected | Correct | Time\n";
        out << "--------------------------------------------------------\n";

        // Loop through each question for detailed status and marks
        for (int i = 0; i < totalQuestions; ++i) {
            string selected = selectedResponses[i].first;   // Student's selected answer
            int timeSpent = selectedResponses[i].second;    // Time spent on question

            string status = "not attempted";
            string mark = "0";

            // Determine status and marks based on student's answer correctness
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

            // Format and write the details for each question
            out << setw(4) << right << i + 1 << " | ";
            out << setw(14) << left << status << " | ";
            out << setw(5) << right << mark << " | ";
            out << setw(8) << left << (selected == "NA" ? "-" : selected) << " | ";
            out << setw(7) << left << correctAnswers[i] << " | ";
            out << timeSpent << "s\n";
        }
        out << "----------------------------------------------------------\n";
        // Write summary of student's overall performance
        out << "\nTotal Marks Obtained   : " << score << " / " << totalMarks << "\n";
        out << "Total Questions        : " << totalQuestions << "\n";
        out << "Attempted Questions    : " << attempted << "\n";
        out << "Wrong Answers          : " << wrong << "\n";
        out << "Total Time Spent       : " << totalTime << "s\n";
        out << "----------------------------------------------------------\n";

        // Combine detailed attempt report with the main leaderboard report
        string res = out.str() + report.str();
        // Send the combined report to the client
        send(sock, res.c_str(), res.size(), 0);
    }
}

void Server::receiveStudentAnswers(int sock, const string& examName) {
    char buffer[256] = {0};

    // Receive raw answer data from the client
    int bytesReceived = recv(sock, buffer, sizeof(buffer), 0);
    if (bytesReceived <= 0) {
        cerr << "Error: Failed to receive answers from client.\n";
        return;
    }

    // Send acknowledgment to client
    char ack = 'y';
    send(sock, &ack, 1, 0);

    string data(buffer);

    // Verify the received data starts with "ANSWERS"
    if (data.substr(0, 7) != "ANSWERS") {
        cerr << "Invalid data received format.\n";
        return;
    }

    // Get student ID associated with this socket
    string studentId = socketToUsername[sock];

    // Load correct answers from the answer key file
    string answerFile = "../data/exams/answers_" + examName + ".txt";
    vector<int> correctAnswers;
    ifstream answerIn(answerFile);
    string line;
    while (getline(answerIn, line)) {
        line.erase(remove_if(line.begin(), line.end(), ::isspace), line.end());
        if (!line.empty()) 
            correctAnswers.push_back(line[0] - 'A'); // Convert char to index
    }
    answerIn.close();

    // Prepare to parse submitted answers
    istringstream dataStream(data.substr(8));  // Skip "ANSWERS "
    string entry;
    int totalQuestions = correctAnswers.size();
    vector<int> perQuestionMarks(totalQuestions, 0);
    vector<int> perQuestionTime(totalQuestions, 0);
    vector<int> perQuestionAnswer(totalQuestions, -1);
    int totalMarks = 0, totalTimeSpent = 0;
    int attemptedCount = 0, wrongCount = 0;

    const int positiveMark = 4, negativeMark = -1;

    // Parse each answer line and compute marks
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

    // Store basic metadata about this attempt
    string currDateTime = getCurrentDateTime();
    string perfFile = "../data/results/student_" + studentId + "_attempts.txt";
    ofstream perfOut(perfFile, ios::app);
    perfOut << examName << "|";
    perfOut << currDateTime << "|";
    perfOut << totalMarks << "|";
    perfOut << totalQuestions*4 << "|";
    perfOut << "../data/results/student_" + studentId + "_" + examName + "_performance.txt\n";
    perfOut.close();

    // Store detailed performance info
    string scoreFile = "../data/results/student_" + studentId + "_" + examName + "_performance.txt";
    ofstream scoreOut(scoreFile, ios::app);
    scoreOut << "START\n";
    scoreOut << currDateTime << "|";
    scoreOut << examName + "|";
    scoreOut << totalMarks << "|" << totalQuestions*4 << "|";
    scoreOut << totalQuestions << "|" << attemptedCount << "|" << wrongCount << "|";
    scoreOut << totalTimeSpent << "\nEND\n";

    // Store per-question details
    for (size_t i = 0; i < perQuestionMarks.size(); ++i) {
        scoreOut << "Q" << (i + 1) << "|";
        scoreOut << perQuestionMarks[i] << "|";
        if (perQuestionAnswer[i] != -1) {
            scoreOut  << static_cast<char>('A' + perQuestionAnswer[i]) << "|";
        }
        else {
            scoreOut << "NA|";
        }
        scoreOut << perQuestionTime[i] << "s\n";
    }
    scoreOut.close();

    // Log the attempt in a global attempt file (thread-safe)
    pthread_mutex_lock(&file_mutex2);
    string attemptFile = "../data/results/exam_log.txt";
    ofstream attemptOut(attemptFile, ios::app);
    attemptOut << studentId << ": " << examName << ": " << getCurrentDateTime() << "\n";
    attemptOut.close();
    pthread_mutex_unlock(&file_mutex2);

    // Append to the exam analysis file (or create if not exists)
    string analysisFile = "../data/results/exam_" + examName + "_analysis.txt";
    pthread_mutex_lock(&file_mutex3);
    ifstream infile(analysisFile);
    bool fileExists = infile.good();
    infile.close();

    ofstream analysisOut;
    if (!fileExists) {
        // Write the answer key on the first line
        analysisOut.open(analysisFile, ios::out);
        for (size_t i = 0; i < correctAnswers.size(); ++i) {
            analysisOut << static_cast<char>('A' + correctAnswers[i]) << " ";
        }
        analysisOut << "\n";
    } else {
        analysisOut.open(analysisFile, ios::app);
    }

    // Append the student's answers and time spent
    analysisOut << studentId;
    for (size_t i = 0; i < perQuestionAnswer.size(); ++i) {
        if (perQuestionAnswer[i] == -1) {
            analysisOut << " - " << perQuestionTime[i];
            continue;
        }
        analysisOut << " " << static_cast<char>('A' + perQuestionAnswer[i]) << " " << perQuestionTime[i];
    }
    analysisOut << "\n";
    analysisOut.close();
    pthread_mutex_unlock(&file_mutex3);

    // Log completion in server console
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

    // Receive the exam number selected by the student
    int bytesReceived = recv(sock, buffer, sizeof(buffer), 0);
    if (bytesReceived <= 0) {
        cerr << "Error: Failed to receive exam selection from client.\n";
        return;
    }

    int examNumber = atoi(buffer);
    if (examNumber == 0) return;  // Invalid selection

    // Check if exam file already exists on the client side
    bool fileExist = false;
    if (examNumber < 0) fileExist = true;

    string selectedExamName;
    istringstream iss(exams[abs(examNumber) - 1]);
    string line;

    // Extract the selected exam's name
    while (getline(iss, line)) {
        if (line.find("Exam Name:") != string::npos) {
            selectedExamName = line.substr(line.find(":") + 2);
            break;
        }
    }

    // Send the exam questions if not already present on client
    if (!fileExist) {
        exam.sendExamQuestions(sock, selectedExamName);
        cout << "[+] question paper send successfully !\n";
    } else {
        cout << "[+] file already exist on client side !\n";
    }

    memset(buffer, 0, sizeof(buffer));

    // Wait for student to confirm whether they want to proceed
    sleep(2);
    recv(sock, buffer, sizeof(buffer), 0);
    string response(buffer);

    if (response == "y" || response == "Y") {
        string studentId = socketToUsername[sock];

        // Receive exam type: 's' for scheduled, 'p' for practice
        char typeBuf[32] = {0};
        recv(sock, typeBuf, sizeof(typeBuf), 0);
        string examTypeBuffer(typeBuf);
        string examName = selectedExamName;
        bool alreadyAttempted = false;

        // For scheduled exams, check if the student has already attempted it
        if (examTypeBuffer == "s") {
            string perfFile = "../data/results/student_" + studentId + "_attempts.txt";
            ifstream infile(perfFile);
            if (infile.is_open()) {
                string line;
                while (getline(infile, line)) {
                    size_t pos = line.find('|');
                    if (pos != string::npos) {
                        string attemptedExam = line.substr(0, pos);
                        // Trim whitespace
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

            // Notify client if already attempted
            if (alreadyAttempted) {
                char message = 'y';
                send(sock, &message, 1, 0);
                sleep(1);
                return;
            } else {
                char message = 'n';
                send(sock, &message, 1, 0);
                sleep(1);
            }
        }

        // Allow some time before receiving answers
        sleep(3);

        // Receive and evaluate student's answers
        receiveStudentAnswers(sock, examName);
    }
}

bool Server::handle_authentication(int sock, const string& command, const string& user_type, const string& username, const string& password) {
    // Handle login request
    if (command == "LOGIN") {
        // Verify user credentials
        if (AuthManager::authenticate_user(username, password, user_type)) {
            // Inform client of successful login
            send(sock, "AUTHENTICATION_SUCCESS", strlen("AUTHENTICATION_SUCCESS"), 0);
            cout << username << " logged in successfully as " << user_type << endl;
            return true;
        } else {
            // Inform client of failed login
            send(sock, "AUTHENTICATION_FAILED", strlen("AUTHENTICATION_FAILED"), 0);
            cerr << "Authentication failed for " << username << endl;
            return false;
        }
    }

    // Handle user registration request
    else if (command == "REGISTER") {
        // Attempt to register the new user
        if (AuthManager::register_user(username, password, user_type)) {
            // Inform client of successful registration
            send(sock, "REGISTER_SUCCESS", strlen("REGISTER_SUCCESS"), 0);
            cout << username << " registered successfully as " << user_type << endl;
            return true;
        } else {
            // Inform client of failed registration
            send(sock, "REGISTER_FAILED", strlen("REGISTER_FAILED"), 0);
            cerr << "Registration failed for " << username << endl;
            return false;
        }
    }

    // If the command is neither LOGIN nor REGISTER, return false
    return false;
}

void Server::handleViewPerformance(int clientSock, const string& studentId) {
    // Build the filename storing student's exam attempts
    string filename = "../data/results/student_" + studentId + "_attempts.txt";
    ifstream file(filename);

    if (!file.is_open()) {
        string err = "[!] No exam data found for student.";
        send(clientSock, err.c_str(), err.size() + 1, 0);
        return;
    }

    // Map exam name to vector of attempts (timestamp, marks, performance file path)
    map<string, vector<tuple<string, string, string>>> examMap;
    string line;

    // Parse each line in attempts file to fill examMap
    while (getline(file, line)) {
        stringstream ss(line);
        string examName, timestamp, marksObtained, totalMarks, perfPath;
        getline(ss, examName, '|');
        getline(ss, timestamp, '|');
        getline(ss, marksObtained, '|');
        getline(ss, totalMarks, '|');
        getline(ss, perfPath);

        // Store all data except performance path separately; keep perfPath in the third tuple element after a '|'
        examMap[examName].emplace_back(timestamp, marksObtained, totalMarks + "|" + perfPath);
    }
    file.close();

    while (true) {
        // Build dashboard with list of attempted exams and number of attempts
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

        // Receive exam selection from client
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

        // Build list of attempts for selected exam
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
        
        // Receive attempt selection
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
        // Read performance file to find matching attempt's detailed record
        while (getline(perfFile, line)) {
            if (line == "START") {
                string summaryLine;
                if (!getline(perfFile, summaryLine)) break;

                stringstream ss(summaryLine);
                string timestamp, marksObtained, totalMarks, totalQuestions, attempted, wrong, totalTime;
                getline(ss, timestamp, '|');
                if (timestamp != selectedTimestamp) {
                    // Skip to next START if timestamp does not match
                    while (getline(perfFile, line) && line != "START");
                    if (line == "START") {
                        perfFile.seekg(-line.length()-1, ios::cur); 
                    }
                    continue;
                }

                // Parse summary line details
                getline(ss, examName, '|');
                getline(ss, marksObtained, '|');
                getline(ss, totalMarks, '|');
                getline(ss, totalQuestions, '|');
                getline(ss, attempted, '|');
                getline(ss, wrong, '|');
                getline(ss, totalTime, '|');

                // Check if exam is ongoing (for scheduled exams) and block viewing if so
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

                // Prepare question-wise summary table
                formatted += "Qno.  | Status  | Marks | Selected | Correct | Time\n";
                formatted += "--------------------------------------------------------\n";
                // Skip lines until the performance answers section
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

                // Send full question paper for this exam
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

    // --- Authentication loop ---
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        recv(sock, buffer, sizeof(buffer), 0);
        string request(buffer);
        if (request == "exit") break;
    
        istringstream iss(request);
        iss >> command >> user_type >> username >> password;
    
        // Ensure thread-safe access to shared resources during authentication
        pthread_mutex_lock(&file_mutex1);
        bool authenticated = handle_authentication(sock, command, user_type, username, password);
        if (authenticated) {
            Server::socketToUsername[sock] = username;
        }
        pthread_mutex_unlock(&file_mutex1);
    
        if (authenticated) break;
    }
    
    ExamManager exam_manager;

    // === Student-specific logic ===
    if (user_type == "student") {
        char mesg[32]={0};

        // Receive exam name (if submitting pending answers)
        int bytes_received = recv(sock, mesg, sizeof(mesg), 0);
        if (bytes_received <= 0) return nullptr;

        string examName(mesg);
        examName.erase(0, examName.find_first_not_of(" \t\n\r")); 
        examName.erase(examName.find_last_not_of(" \t\n\r") + 1); 

        if(examName!="n"){
            receiveStudentAnswers(sock, examName);
        }

        // Main loop for student interaction
        while (true){
            memset(buffer, 0, sizeof(buffer));
            int bytes_received = recv(sock, buffer, sizeof(buffer), 0);
            if (bytes_received <= 0) break;
            buffer[bytes_received] = '\0';
            string request(buffer);
            
            if (request == "1") {
                // Build a formatted list of available exams
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
            
                // Send available exams to student
                if (all_exams.empty())
                    all_exams = "No exams available.";
                
                send(sock, all_exams.c_str(), all_exams.size(), 0);
                // If exams are available, handle request
                if(all_exams!="No exams available.")
                    handleStudentExamRequest(sock, exam_manager);
            }
            
            else if (request == "2") {
                handleViewPerformance(sock, username);
            }
            else if(request == "3") break;
        }
    }
    // === Instructor-specific logic ===
    else if (user_type == "instructor") {
        while (true){
            memset(buffer, 0, sizeof(buffer));
            int bytes_received = recv(sock, buffer, sizeof(buffer), 0);
            buffer[bytes_received] = '\0';
            string request(buffer);
            string response = "";

            // === Upload new exam ===
            if (request == "1") {
                memset(buffer, 0, sizeof(buffer));
                recv(sock, buffer, sizeof(buffer), 0);
                string examData(buffer);
            
                // Parse exam details
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

                    // Validate date format
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

                // Check for existing exam with the same name
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
                    // Upload exam or show error
                    if (exam_manager.parse_exam(exam_type ,examFileName, examName, username, examDuration, start_time)) {
                        pthread_mutex_lock(&file_mutex1);
                        exams = exam_manager.load_exam_metadata("../data/exams/exam_list.txt");
                        pthread_mutex_unlock(&file_mutex1);
                        response = "Exam successfully uploaded!"; 
                    } else response = "Error: Invalid exam format!";      
                }
                send(sock,response.c_str(),response.size(),0);
            }
            // === View exam analysis ===
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
            // === View uploaded exams ===
            else if (request == "4") {
                vector<string> all_exams;
                sendAvailableExams(sock, username, all_exams);
            }
            else if (response=="5") break;
        }
    }

    // === Final cleanup ===
    close(sock);
    cout << "[-] client[ "<<username<<" ] disconnected!"<<endl;
    return nullptr;
}