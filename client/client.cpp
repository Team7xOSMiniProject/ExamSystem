#include "client.h"
#include "ui.h"

map<int, int> Client::timeSpentPerQuestion;
map<int, int> Client::shuffledQuestionMap;
vector<vector<int>> Client::shuffledOptionMap;
vector<string> Client::shuffledQuestions;
vector<vector<string>> Client::shuffledOptions;
vector<ExamInfo> availableExams;
namespace fs = filesystem;

int lastIndex = 0;
auto lastTime = steady_clock::now();
bool Client::timeUp = false;
pthread_mutex_t Client::timerMutex = PTHREAD_MUTEX_INITIALIZER;

Client::Client(const string& server_ip, int server_port) {
    // Create a TCP socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        cerr << "Error: Could not create socket\n";
        exit(EXIT_FAILURE);
    }

    // Setup the server address structure
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);  // Convert port to network byte order
    inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr);  // Convert IP to binary form

    // Attempt to connect to the server
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        cerr << "Error: Connection to server failed\n";
        exit(EXIT_FAILURE);
    }
}

int Client::userInput(const string& prompt, int minVal, int maxVal) {
    string input;
    int opt;

    while (true) {
        // Display the prompt message to the user
        cout << prompt;

        // Read a full line of input from the user
        getline(cin, input);

        // Use a stringstream to parse the input as an integer
        stringstream ss(input);

        // Check if input is a valid integer and no extra characters remain
        if (ss >> opt && ss.eof()) {
            // Check if the integer is within the specified range
            if (opt >= minVal && opt <= maxVal) {
                return opt;  // Valid input, return it
            } else {
                cout << "[✖] Please enter a number between " << minVal << " and " << maxVal << ".\n";
            }
        } else {
            // Input was not a valid integer
            cout << "[✖] Invalid input. Please enter an integer only.\n";
        }
    }
}

void Client::xorEncryptDecrypt(const string& filePath, char key) {
    fstream file(filePath, ios::in | ios::out | ios::binary);
    if (!file) {
        cerr << "Error: Unable to open file for encryption: " << filePath << "\n";
        return;
    }

    // Move read position to the beginning explicitly
    file.seekg(0, ios::beg);

    vector<char> fileData((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());

    // XOR encrypt/decrypt each byte
    for (char &c : fileData) {
        c ^= key;
    }

    // Move write position to beginning before writing
    file.seekp(0, ios::beg);
    file.write(fileData.data(), fileData.size());
    
    // In case the new data is smaller (shouldn't happen here), truncate file
    file.flush();
    file.close();
}

void Client::decryptAndPrepareExam(const string& filePath, char key) {
    // Open the encrypted exam file in binary mode
    ifstream infile(filePath, ios::binary);
    if (!infile) {
        cerr << "[-] Error: Could not open file " << filePath << endl;
        return;
    }

    // Read entire file content into a string
    string encryptedContent((istreambuf_iterator<char>(infile)), istreambuf_iterator<char>());
    infile.close();

    // XOR decrypt the content with the given key
    for (char &ch : encryptedContent) {
        ch ^= key;
    }

    // Vectors to hold parsed questions and their corresponding options
    vector<string> questions;
    vector<vector<string>> options;

    istringstream iss(encryptedContent);
    string line, currentQuestion;
    bool readingOptions = false;
    vector<string> tempOptions;

    // Parse the decrypted content line by line
    while (getline(iss, line)) {
        // If line starts with "Q:", it marks the start of a new question
        if (line.rfind("Q:", 0) == 0) {
            // If there is an existing question with exactly 4 options, save them
            if (!currentQuestion.empty() && tempOptions.size() == 4) {
                questions.push_back(currentQuestion);
                options.push_back(tempOptions);
            }
            // Initialize the new question (excluding the "Q:" prefix)
            currentQuestion = line.substr(2);
            tempOptions.clear();
            readingOptions = false;
        }
        // Lines starting with "A)", "B)", "C)", or "D)" are answer options
        else if (line.rfind("A)", 0) == 0 || line.rfind("B)", 0) == 0 ||
                 line.rfind("C)", 0) == 0 || line.rfind("D)", 0) == 0) {
            // Store the option text (excluding the option prefix)
            tempOptions.push_back(line.substr(2));
            readingOptions = true;
        }
        // For lines that are part of a multiline question text (not options)
        else if (!readingOptions) {
            if (!currentQuestion.empty()) currentQuestion += "\n";
            currentQuestion += line;
        }
    }

    // After parsing all lines, add the last question and options if valid
    if (!currentQuestion.empty() && tempOptions.size() == 4) {
        questions.push_back(currentQuestion);
        options.push_back(tempOptions);
    }

    // If no valid questions were found, print an error and return
    if (questions.empty()) {
        cerr << "[-] Error: No valid questions found in decrypted content.\n";
        return;
    }

    // Shuffle the question indices to randomize question order
    int n = questions.size();
    vector<int> qIndices(n);
    iota(qIndices.begin(), qIndices.end(), 0);

    random_device rd;
    mt19937 g(rd());
    shuffle(qIndices.begin(), qIndices.end(), g);

    // Clear any existing shuffled exam data
    shuffledQuestionMap.clear();
    shuffledQuestions.clear();
    shuffledOptions.clear();
    shuffledOptionMap.clear();

    // Rebuild the shuffled questions and options using the shuffled indices
    for (int i = 0; i < n; ++i) {
        int origIdx = qIndices[i];

        // Map the shuffled index to original question index
        shuffledQuestionMap[i] = origIdx;

        // Add the shuffled question text
        shuffledQuestions.push_back(questions[origIdx]);

        // Shuffle the order of the options for this question
        vector<int> optIdx = {0, 1, 2, 3};
        shuffle(optIdx.begin(), optIdx.end(), g);

        vector<string> shuffledOpts(4);
        vector<int> optMapping(4);
        for (int j = 0; j < 4; ++j) {
            shuffledOpts[j] = options[origIdx][optIdx[j]];
            optMapping[j] = optIdx[j];  // Keep track of original option index for answer mapping
        }

        // Store the shuffled options and option mapping
        shuffledOptions.push_back(shuffledOpts);
        shuffledOptionMap.push_back(optMapping);
    }
}

void* examTimer(void* arg) {
    int duration = *(int*)arg;
    int total = duration;

    // Hide cursor for better UI (optional)
    cout << "\033[?25l";

    for (int i = 0; i < total; ++i) {
        sleep(1);

        int percent = (100 * i) / total;
        int barWidth = 50;
        int pos = (barWidth * i) / total;

        // Save cursor position
        cout << "\033[s";

        // Move cursor to top-left (or wherever you want the progress bar)
        cout << "\033[1;1H";

        cout << "[";
        for (int j = 0; j < barWidth; ++j) {
            if (j < pos) cout << "=";
            else if (j == pos) cout << ">";
            else cout << " ";
        }
        cout << "] " << percent << "% " << (total - i) << "s left";

        // Restore cursor position
        cout << "\033[u";

        cout.flush();

        // Check if timeUp flag is set from the main thread
        pthread_mutex_lock(&Client::timerMutex);
        if (Client::timeUp) {
            pthread_mutex_unlock(&Client::timerMutex);
            break; // Stop timer early if exam ended/submitted
        }
        pthread_mutex_unlock(&Client::timerMutex);
    }

    // Set timeUp flag after timer finishes
    pthread_mutex_lock(&Client::timerMutex);
    Client::timeUp = true;
    pthread_mutex_unlock(&Client::timerMutex);

    // Show message after time is up
    cout << "\n[!] Time is up.\n";
    cout << "Press any key...\n";

    // Show cursor again before exiting
    cout << "\033[?25h";

    return nullptr;
}

void Client::displayPreparedQuestion(int index) {
    cout << "\n\n--------------------------------QUESTION "<<index+1<<"-------------------------------\n";
    if (index < 0 || index >= shuffledQuestions.size()) {
        cout << "Invalid question index.\n";
        return;
    }

    cout << "Q" << (index + 1) << ": " << shuffledQuestions[index] << "\n";
    for (int i = 0; i < 4; ++i) {
        char label = 'A' + i;
        cout << label << ") " << shuffledOptions[index][i] << "\n";
    }
    cout << "-----------------------------QUESTION END--------------------------------\n";
}

void Client::ensureDirectoryExists(const string &path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        if (mkdir(path.c_str(), 0700) == 0) {
            cout << "[✔] Directory created: " <<endl;
        } else {
            perror("[✖] Failed to create directory\n");
        }
    }
}

/**
 * Backs up the student's answer sheet data locally.
 * 
 * @param examName Name of the exam (used as filename).
 * @param finalData The content of the answer sheet to be saved.
 */
void Client::backupExamData(string& examName, const string& finalData) {
    // Construct hidden directory path in user's home config folder
    const char* home = getenv("HOME");
    string hiddenDir = string(home) + "/.config/.ans_sheet";
    
    // Ensure the backup directory exists, create if needed
    ensureDirectoryExists(hiddenDir);

    // File path for backup file: ~/.config/.ans_sheet/<examName>.txt
    string filePath = hiddenDir + "/" + examName + ".txt";

    // Open file in write mode (overwrites existing)
    ofstream backupFile(filePath);
    if (backupFile.is_open()) {
        backupFile << finalData;  // Write answer sheet data
        backupFile.close();
        cout << "[✔] Answer sheet backed up successfully.\n";
    } else {
        cerr << "[✖] Failed to create answer sheet file.\n";
    }
}

void Client::manageExam(int durationMinutes, Client* client, string examName) {
    int durationSeconds = durationMinutes * 60;
    vector<int> studentAnswers(shuffledQuestions.size(), -1); // store answers, -1 = unanswered
    vector<int> timeSpent(shuffledQuestions.size(), 0);      // time spent on each question (seconds)

    Client::timeSpentPerQuestion.clear(); // reset any previous data

    int currentIndex = 0;
    auto questionStartTime = chrono::steady_clock::now();

    // Start exam timer thread that will set `timeUp` when exam time finishes
    pthread_t timerThread;
    pthread_create(&timerThread, nullptr, examTimer, &durationSeconds);

    system("clear");
    cout << "\n📘 Exam started. Good luck!\n";

    int opt = -1;
    string message = "";
    string input;

    while (true) {
        // Check if time is up
        pthread_mutex_lock(&timerMutex);
        if (timeUp) {
            pthread_mutex_unlock(&timerMutex);
            break;
        }
        pthread_mutex_unlock(&timerMutex);

        system("clear");

        // Display the current question
        displayPreparedQuestion(currentIndex);
        cout << message;
        message.clear();

        UI_elements::displayExamOptions();

        // Get valid input for options 1-7
        while (true) {
            cout << "\n➡️  Enter your choice (1-7): ";
            getline(cin, input);

            pthread_mutex_lock(&timerMutex);
            bool localTimeUp = timeUp;
            pthread_mutex_unlock(&timerMutex);
            if(localTimeUp) break;

            stringstream ss(input);
            if (ss >> opt && ss.eof() && opt >= 1 && opt <= 7) {
                break;
            } else {
                cout << "[✖] Invalid input. Please enter a valid integer between 1 and 7.\n";
            }
        }

        pthread_mutex_lock(&timerMutex);
        if (timeUp) {
            pthread_mutex_unlock(&timerMutex);
            break;
        }
        pthread_mutex_unlock(&timerMutex);

        // Calculate and add time spent on the previous question before switching
        auto now = chrono::steady_clock::now();
        int timeElapsed = chrono::duration_cast<chrono::seconds>(now - questionStartTime).count();
        timeSpent[currentIndex] += timeElapsed;
        questionStartTime = now;

        // Handle user choice
        switch (opt) {
            case 1: // Next question
                if (currentIndex < (int)shuffledQuestions.size() - 1) {
                    currentIndex++;
                } else {
                    message = "\n[!] You are on the last question.\n";
                }
                break;

            case 2: // Previous question
                if (currentIndex > 0) {
                    currentIndex--;
                } else {
                    message = "\n[!] You are on the first question.\n";
                }
                break;

            case 3: { // Attempt/Answer question
                cout << "\n✏️  Enter your answer (A/B/C/D): ";
                char answer;
                cin >> answer;
                answer = toupper(answer);
                cin.ignore(numeric_limits<streamsize>::max(), '\n');

                if (answer >= 'A' && answer <= 'D') {
                    int shuffledIndex = answer - 'A';
                    int originalOptionIndex = shuffledOptionMap[currentIndex][shuffledIndex];
                    studentAnswers[currentIndex] = originalOptionIndex;

                    if (currentIndex < (int)shuffledQuestions.size() - 1) {
                        currentIndex++;
                    } else {
                        message = "\n[!] You are on the last question.\n";
                    }
                } else {
                    message = "[✖] Invalid choice. Please enter A/B/C/D.\n";
                }
                break;
            }

            case 4: // Clear answer for current question
                studentAnswers[currentIndex] = -1;
                message = "[✔] Answer cleared.\n";
                break;

            case 5: { // Jump to question number
                int qno;
                cout << "\n🔢 Enter question number (1 to " << shuffledQuestions.size() << "): ";
                qno = userInput("", 1, shuffledQuestions.size());
                currentIndex = qno - 1;
                break;
            }

            case 6: { // Show unanswered questions
                stringstream ss;
                ss << "\nNot Answered: ";
                for (size_t i = 0; i < studentAnswers.size(); ++i) {
                    if (studentAnswers[i] == -1) {
                        ss << " Q" << (i + 1);
                    }
                }
                message = ss.str();
                break;
            }

            case 7: // Submit exam
                cout << "\n📝 Submitting your exam...\n";
                pthread_mutex_lock(&timerMutex);
                timeUp = true;
                pthread_mutex_unlock(&timerMutex);
                break;

            default:
                message = "[✖] Invalid choice. Try again.\n";
                break;
        }
    }

    // Wait for timer thread to end cleanly
    pthread_join(timerThread, nullptr);
    timeUp = false;

    // Prepare answers and time spent to send to server
    ostringstream dataToSend;
    dataToSend << "ANSWERS\n";
    for (int i = 0; i < (int)studentAnswers.size(); ++i) {
        int originalIndex = shuffledQuestionMap[i];
        dataToSend << originalIndex << "," << studentAnswers[i] << "," << timeSpent[i] << "\n";
    }
    
    string finalData = dataToSend.str();
    send(client->sock, finalData.c_str(), finalData.size(), 0);

    char mesg[32] = {0};
    int bytesReceived = recv(client->sock, mesg, sizeof(mesg), 0);
    if (bytesReceived <= 0) {
        cerr << "[!] Failed to send data to server. Error or connection closed.\n";
        finalData = examName + "\n" + finalData;
        backupExamData(examName, finalData);
    }
}

void Client::dashboard(Client* client) {
    int sockfd = client->sock;
    char buffer[1028];

    while (true) {
        memset(buffer, 0, sizeof(buffer));
        
        // Receive exam list or message from server
        recv(sockfd, buffer, sizeof(buffer), 0);
        
        // Clear terminal and small pause for UI smoothness
        system("clear");
        usleep(200000);
        cout << buffer;

        string str(buffer);

        // Exit if no exam data found for student
        if (str == "[!] No exam data found for student.")
            break;

        // User selects an exam (or 0 to go back)
        int input = userInput("", 0, 100);
        string examSelection = to_string(input);
        send(sockfd, examSelection.c_str(), examSelection.size(), 0);

        if (input == 0) break;

        memset(buffer, 0, sizeof(buffer));
        
        // Receive exam attempt info or error message
        recv(sockfd, buffer, sizeof(buffer), 0);
        cout << buffer;
        string mesg(buffer);

        // Handle invalid exam selection
        if (mesg == "[!] Invalid option! please select a valid exam.") {
            cout << endl;
            continue;  // Go back to start of loop
        }

        // User selects an attempt number (or 0 to go back)
        input = userInput("", 0, 100);
        examSelection = to_string(input);
        send(sockfd, examSelection.c_str(), examSelection.size(), 0);

        if (input == 0) continue;

        memset(buffer, 0, sizeof(buffer));
        
        // Receive attempt details or status message
        recv(sockfd, buffer, sizeof(buffer), 0);
        system("clear");
        cout << buffer << endl;

        string mesg1(buffer);
        if (mesg1 == "[!] Invalid option! please select a valid attempt.") {
            cout << endl;
            continue;
        }
        if (mesg1 == "Exam is still going on.") {
            return;  // Exit dashboard if exam is in progress
        }

        sleep(2);

        // Receive exam questions or data dump
        char questionBuffer[5120] = {0};
        recv(sockfd, questionBuffer, sizeof(questionBuffer), 0);
        string receivedData(questionBuffer);

        stringstream ss(receivedData);
        string examName;
        getline(ss, examName);

        string filename = examName + "_questions.txt";

        // Save exam questions to a file if it doesn't already exist
        ifstream checkFile(filename);
        if (!checkFile.good()) {
            ofstream outFile(filename);
            outFile << receivedData;
            outFile.close();
            chmod(filename.c_str(), S_IRUSR | S_IRGRP | S_IROTH);  // Read permissions for all
        }
        cout << "\n[+] Exam questions saved to : " << filename << endl;

        memset(questionBuffer, 0, sizeof(questionBuffer));

        // Present options for analysis or back to list
        string options = "\n--------------------------------------------\n";
        options += "[1] View Exam Analysis\n";
        options += "\n[0] Back to Exam List\n";
        options += "-------------------------------------------\n";
        options += "Select from above option: ";
        cout << options;

        input = userInput("", 0, 1);
        examSelection = to_string(input);
        send(sockfd, examSelection.c_str(), examSelection.size(), 0);

        if (input == 0) continue;

        // Receive and display exam analysis parts
        memset(questionBuffer, 0, sizeof(questionBuffer));
        recv(sockfd, questionBuffer, sizeof(questionBuffer), 0);
        cout << questionBuffer << endl;

        memset(questionBuffer, 0, sizeof(questionBuffer));
        recv(sockfd, questionBuffer, sizeof(questionBuffer), 0);
        cout << questionBuffer << endl;

        cout << "\npress any key...\n";
        cin.get();
        cin.ignore();

        break;  // Exit dashboard after viewing analysis
    }
}

void Client::parseAvailableExams(const string& examData) {
    availableExams.clear();
    istringstream iss(examData);
    string line;

    while (getline(iss, line)) {
        if (line.empty()) continue;
        
        size_t pos1 = line.find("Exam Name:");
        size_t pos2 = line.find("| Exam type:");
        size_t pos3 = line.find("| Start Time:");
        size_t pos4 = line.find("| Duration (minutes):");
        size_t pos5 = line.find("| Total Questions:");
        size_t pos6 = line.find("| Instructor:");

        if (pos1 != string::npos && pos2 != string::npos && pos3 != string::npos && pos4 != string::npos && pos5 != string::npos && pos6 != string::npos) {
            string name       = line.substr(pos1 + 10, pos2 - (pos1 + 10));
            string type       = line.substr(pos2 + 13, pos3 - (pos2 + 13));
            string startTime  = line.substr(pos3 + 13, pos4 - (pos3 + 13));
            int duration      = stoi(line.substr(pos4 + 22, pos5 - (pos4 + 22)));
            int totalQ        = stoi(line.substr(pos5 + 19, pos6 - (pos5 + 19)));
            string instructor = line.substr(pos6 + 13);
            availableExams.emplace_back(name, type, startTime, duration, totalQ, instructor);        }
    }
}

void Client::handleExamSelection(Client* client, int& choice) {
    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));

    // Receive list of available exams from server
    int bytes_read = recv(client->sock, buffer, sizeof(buffer), 0);
    if (bytes_read <= 0) {
        cerr << "[✖] Error: Failed to read data from server\n";
        close(client->sock);
        return;
    }

    string examData(buffer);

    // Handle case when no exams are available
    if (examData == "No exams available.") {
        cout << "\n[!] No exams available at the moment.\n\n";
        return;
    }

    // Parse and store the available exams from server data
    parseAvailableExams(examData);

    // Display formatted list of available exams
    std::cout << "\n================================== Available Exams =================================\n";
    for (size_t i = 0; i < availableExams.size(); ++i) {
        const ExamInfo& ex = availableExams[i];
        string exam_type = (ex.type == "g ") ? "Scheduled Test" : "Practice Test";

        printf("%2zu. %-15s | Type: %-8s | Start: %-19s | Duration: %3d min | Questions: %2d | Instructor: %-10s\n",
               i + 1,
               ex.name.c_str(),
               exam_type.c_str(),
               ex.start_time.c_str(),
               ex.duration,
               ex.totalQuestions,
               ex.instructor.c_str());
    }
    cout << "------------------------------------------------------------------------------------\n";

    // Prompt user to select exam or go back
    choice = userInput("Select exam number to start the exam (press 0 to go back): ", 0, availableExams.size());
    if (choice == 0) {
        // Send cancellation to server and return to menu
        string examSelection = to_string(choice);
        send(client->sock, examSelection.c_str(), examSelection.size(), 0);
        return;
    }

    // Prepare local storage path for exam paper
    const char* home = getenv("HOME");
    string hiddenDir = string(home) + "/.config/.exam";
    string filePath = hiddenDir + "/" + to_string(choice) + ".txt";
    const ExamInfo& selectedExam = availableExams[choice - 1];
    ensureDirectoryExists(hiddenDir);

    // Check if exam paper already downloaded locally
    struct stat stats;
    bool fileExist = stat(filePath.c_str(), &stats) == 0;

    if (!fileExist) {
        // Download exam paper from server if not present locally
        cout << "[!] Downloading exam paper...\n";
        receiveAndStoreExamQuestions(client->sock, choice);
    } else {
        // Notify server that local exam copy will be used (negative exam number)
        int examnumber = -choice;
        send(client->sock, to_string(examnumber).c_str(), to_string(examnumber).size(), 0);
    }

    // Decrypt and prepare the exam paper for the student
    decryptAndPrepareExam(filePath, 'X');

    // Display selected exam details before starting
    string examType = (selectedExam.type == "g ") ? "Scheduled Test" : "Practice Test";
    cout << "\n==================== Exam Details ====================\n";
    cout << "- Exam Name         : " << selectedExam.name << "\n";
    cout << "- Exam type         : " << examType << "\n";
    cout << "- Start Date & Time : " << selectedExam.start_time << "\n";
    cout << "- Total Questions   : " << selectedExam.totalQuestions << "\n";
    cout << "- Duration          : " << selectedExam.duration << " minutes\n";
    cout << "- Marking Scheme    : +4 for correct, -1 for incorrect\n";
    cout << "---------------------------------------------------------\n";

    // Confirm if user wants to start the exam now
    cout << "Start the exam now? (y for yes, n for no): ";
    char confirm;
    cin >> confirm;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    if (tolower(confirm) == 'y') {
        string time = selectedExam.start_time;

        // Check if exam start time is valid (scheduled test)
        if (std::any_of(time.begin(), time.end(), ::isdigit)) {
            // Parse start time into tm structure
            tm tm_input = {};
            istringstream ss(selectedExam.start_time);
            ss >> get_time(&tm_input, "%Y-%m-%d %H:%M:%S");
            time_t input_time = std::mktime(&tm_input);
            time_t current_time = std::time(nullptr);
            time_t end_time = input_time + 5 * 60;  // 5-minute grace period

            // Validate if current time is within exam start window
            if (current_time < input_time || end_time < current_time) {
                if (end_time < current_time) {
                    cout << "[!] You joined too late.\nEntry is only allowed within the first 5 minutes of the exam.\n";
                } else {
                    // Exam not started yet - show countdown timer
                    cout << "\n========Exam not started yet========\n";

                    time_t diff = input_time - current_time;
                    int days = diff / (24 * 3600);
                    diff %= (24 * 3600);
                    int hours = diff / 3600;
                    diff %= 3600;
                    int minutes = diff / 60;
                    int seconds = diff % 60;

                    cout << "Time left: ";
                    if (days > 0) std::cout << days << "d ";
                    if (hours > 0 || days > 0) std::cout << hours << "h ";
                    if (minutes > 0 || hours > 0 || days > 0) std::cout << minutes << "m ";
                    cout << seconds << "s\n";
                    cout << "------------------------------------\n";
                }
                // Notify server that exam will not be started
                char ch = 'n';
                send(client->sock, &ch, 1, 0);
            } else {
                // Proceed to start the exam
                send(client->sock, &confirm, 1, 0);
                sleep(2);

                // Indicate scheduled exam type to server
                char type = 's';
                send(client->sock, &type, 1, 0);

                // Check if exam already attempted
                char retMeg[32] = {0};
                recv(client->sock, retMeg, sizeof(retMeg), 0);
                string attempted(retMeg);

                if (attempted == "y") {
                    cout << "[✖] You have already attempted this exam. Reattempt is not allowed.\n";
                } else {
                    // Manage exam session
                    manageExam(selectedExam.duration, client, selectedExam.name.c_str());
                }
            }
        } else {
            // For practice tests without specific schedule
            send(client->sock, &confirm, 1, 0);
            sleep(2);

            // Indicate practice exam type to server
            char type = 'm';
            send(client->sock, &type, 1, 0);

            // Start exam session
            manageExam(selectedExam.duration, client, selectedExam.name.c_str());
        }
    } else {
        // User declined to start exam; notify server and return to menu
        send(client->sock, &confirm, 1, 0);
        cout << "Returning to student menu.\n";
    }
}

void Client::sendPendingAnswerSheet(int clientSocket) {
    const char* home = getenv("HOME");
    string hiddenDir = string(home) + "/.config/.ans_sheet/";
    ensureDirectoryExists(hiddenDir);

    string foundFile = "";

    // Search for a pending answer sheet file
    for (const auto& entry : fs::directory_iterator(hiddenDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".txt") {
            foundFile = entry.path().string();
            break;
        }
    }

    // If no pending answer sheet is found, notify server and return
    if (foundFile.empty()) {
        char message = 'n';
        send(clientSocket, &message, 1, 0);
        return;
    }

    // Open the found file
    ifstream inFile(foundFile);
    if (!inFile.is_open()) {
        cerr << "[!] Failed to open: " << foundFile << endl;
        char message = 'n';
        send(clientSocket, &message, 1, 0);
        return;
    }

    // Read exam name and answers from the file
    string examName;
    getline(inFile, examName);

    string answers, line;
    while (getline(inFile, line)) {
        answers += line + "\n";
    }
    inFile.close();

    // Send exam name and answers to the server
    send(clientSocket, examName.c_str(), examName.size(), 0);
    sleep(1);
    send(clientSocket, answers.c_str(), answers.size(), 0);
    sleep(1);

    // Wait for confirmation from server
    char mesg[32] = {0};
    int byte_recv = recv(clientSocket, mesg, sizeof(mesg), 0);
    if (byte_recv <= 0) {
        cout << "[!] Failed to send the answer sheet\n";
        return;
    }

    // Delete the local answer sheet file after successful submission
    try {
        fs::remove(foundFile);
    } catch (const fs::filesystem_error& e) {
        // Ignore deletion errors silently
    }
}

void* Client::studentHandler(void* arg) {
    Client* client = static_cast<Client*>(arg);
    char buffer[1024] = {0};
    int choice;

    // Automatically send any saved answer sheet before proceeding
    sendPendingAnswerSheet(client->sock);

    while (true) {
        // Display student options menu
        UI_elements::displayStudentMenu();
        choice = userInput("", 1, 3);

        // Send user's choice to the server
        sprintf(buffer, "%d", choice);
        send(client->sock, buffer, strlen(buffer), 0);

        if (choice == 3) {
            // Logout
            cout << "Logging out...\n";
            close(client->sock);
            return nullptr;
        } else if (choice == 1) {
            // Start or resume exam
            handleExamSelection(client, choice);
        } else if (choice == 2) {
            // Show student dashboard
            dashboard(client);
        } else {
            cout << "[✖] Invalid choice! Please select a valid option.\n";
        }
    }

    return nullptr;
}


void Client::receiveAndStoreExamQuestions(int sock, int examNumber) {
    char buffer[4096] = {0};

    // Convert examNumber to string and send selection to server
    string examSelection = to_string(examNumber);
    send(sock, examSelection.c_str(), examSelection.size(), 0);

    // Wait a bit to ensure server has time to prepare the data
    sleep(1);

    // Receive exam questions from the server
    int bytesReceived = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytesReceived <= 0) {
        cerr << "Error: Failed to receive exam questions from server.\n";
        return;
    }

    // Convert received buffer to string for validation
    string server_reply(buffer);

    // Check for server error response
    if (server_reply == "Error: Invalid exam selection") {
        cout << "[+] " << server_reply << endl;
        return;
    }

    // Null-terminate buffer at received bytes position
    buffer[bytesReceived] = '\0';

    // Prepare hidden directory path to store exam questions
    const char* home = getenv("HOME");
    string hiddenDir = string(home) + "/.config/.exam";
    string fileName = hiddenDir + "/" + to_string(examNumber) + ".txt";

    // Open file to save exam questions
    ofstream outFile(fileName);
    if (!outFile) {
        cerr << "Error: Unable to create file " << fileName << "\n";
        return;
    }

    // Write received questions to file
    outFile << buffer;
    outFile.close();

    // Clear buffer after writing to file
    memset(buffer, 0, sizeof(buffer));

    // Encrypt the saved file using XOR encryption with key 'X'
    xorEncryptDecrypt(fileName, 'X');

    cout << "[+] Question paper received successfully\n";
}


void* Client::instructorHandler(void* arg) {
    Client* client = static_cast<Client*>(arg);
    char buffer[1024] = {0};
    int choice;

    while (true) {
        UI_elements::displayInstructorMenu();  // Show instructor menu options
        choice = userInput("", 1, 5);           // Get valid user choice (1-5)
        sprintf(buffer, "%d", choice);          // Convert choice to string
        send(client->sock, buffer, strlen(buffer), 0); // Send choice to server

        if (choice == 5) {  // Logout
            cout << "Logging out...\n";
            close(client->sock);
            return nullptr;
        } 
        else if (choice == 1) {  // Upload new exam
            string examName, duration, fileName, type, start_time;
            cout << "\n=============Enter exam details=============\n\n";
            cout << "Enter Exam Name: ";
            getline(cin, examName);
            cout << "Enter exam type (q for Practice Test, g for Schedule Test): ";
            getline(cin, type);
            cout << "Enter Exam Duration (minutes): ";
            getline(cin, duration);
            cout << "Enter Exam File Name: ";
            getline(cin, fileName);

            // If scheduled exam, get start time
            if (type == "g" || type == "G") {
                cout << "Enter exam start date and time (YYYY-MM-DD HH:MM:SS): : ";
                getline(cin, start_time);
            }
            cout << "-------------------------------------------------\n";

            // Compose exam details string to send to server
            examName += "|" + type + "|" + duration + "|" + fileName + "|";
            if (type == "g" || type == "G") {
                examName += start_time;
            }
            send(client->sock, examName.c_str(), examName.size(), 0);

            // Receive and display server response
            memset(buffer, 0, sizeof(buffer));
            recv(client->sock, buffer, sizeof(buffer), 0);
            cout << buffer << endl;
        } 
        else if (choice == 2) {  // Service not available placeholder
            cout << "\nCurrently this service is not avaliable.\n";
        }
        else if (choice == 3) {  // View exam analysis
            char examBuffer[1024];
            int bytes_recv = recv(client->sock, examBuffer, sizeof(examBuffer), 0);
            if (bytes_recv <= 0) break;

            string exams(examBuffer);
            cout << "\n===============================================Available exams for Analysis=============================================\n";
            cout << exams << endl;
            cout << "-----------------------------------------------------------------------------------------------------------------------------\n";

            if (exams == "[!] You have not uploaded any exam.") continue;

            int option = userInput("Enter exam number to view analysis: ", 1, 100);

            // Send selected exam number to server
            char choiceBuffer[10] = {0};
            sprintf(choiceBuffer, "%d", option);
            send(client->sock, choiceBuffer, strlen(choiceBuffer), 0);

            // Receive and display first part of analysis
            char analysisBuffer[4096];
            bytes_recv = recv(client->sock, analysisBuffer, sizeof(analysisBuffer), 0);
            if (bytes_recv <= 0) break;

            string firstAnalysis(analysisBuffer);
            if (firstAnalysis == "[!] Invalid exam selection.") {
                cout << firstAnalysis << endl;
                continue;
            }

            system("clear");
            cout << firstAnalysis << endl;
            firstAnalysis = "";

            // Receive and display second part of analysis
            memset(analysisBuffer, 0, sizeof(analysisBuffer));
            bytes_recv = recv(client->sock, analysisBuffer, sizeof(analysisBuffer), 0);
            if (bytes_recv <= 0) break;

            cout << analysisBuffer << endl;
            string output(analysisBuffer);
            if (output == "The possible cause may be that no student has attempted this exam.") {
                continue;
            }

            memset(analysisBuffer, 0, sizeof(analysisBuffer));

            // Loop to view analysis of individual students
            while (true) {
                choice = userInput("Select student sr no. to view analysis (press 0 to go back to main menu): ", 0, 100);

                sprintf(buffer, "%d", choice);
                send(client->sock, buffer, strlen(buffer), 0);

                if (choice == 0) break;

                memset(analysisBuffer, 0, sizeof(analysisBuffer));
                bytes_recv = recv(client->sock, analysisBuffer, sizeof(analysisBuffer), 0);
                if (bytes_recv <= 0) return nullptr;

                cout << analysisBuffer << endl;
                memset(analysisBuffer, 0, sizeof(analysisBuffer));
            }
        }
        else if (choice <= 4) {  // Show list of uploaded exams
            memset(buffer, 0, sizeof(buffer));
            recv(client->sock, buffer, sizeof(buffer), 0);
            cout << "\n\n=====================================Your uploaded exams=====================================\n";
            cout << buffer << endl;
            cout << "-----------------------------------------------------------------------------------------------\n";
        } 
        else {  // Invalid input fallback
            cout << "Invalid choice! Please select a valid option.\n";
        }
    }
    return nullptr;
}


void Client::authenticate() {
    int choice;
    system("clear");
    usleep(200000);  // Brief pause for smoother transition
    UI_elements::displayHeader("Welcome to the Exam System");

    while (true) {
        // Show login/register menu and get user choice
        UI_elements::displayMenu();
        choice = userInput("", 1, 3);

        if (choice == 3) {
            // Exit option selected
            cout << "Exiting...\n";
            string request = "exit";
            send(sock, request.c_str(), request.length(), 0);
            close(sock);
            exit(0);
        }

        // Get user role and credentials
        cout << "Enter role (S for Student, I for Instructor): ";
        cin >> role;
        transform(role.begin(), role.end(), role.begin(), ::tolower);

        if (role != "s" && role != "i") {
            cout << "[✖] Invalid role! Please enter S or I." << endl;
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            continue;
        }

        cout << "Enter username: "; cin >> username;
        cout << "Enter password: "; cin >> password;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        // Prepare authentication or registration request
        string user_type = (role == "s") ? "student" : "instructor";
        string request = (choice == 1) ? "LOGIN " : "REGISTER ";
        request += user_type + " " + username + " " + password;
        send(sock, request.c_str(), request.length(), 0);

        // Receive and handle server response
        char response[128] = {0};
        int bytes_read = recv(sock, response, sizeof(response), 0);
        if (bytes_read <= 0) {
            cout << "[✖] Error: Failed to read data from server." << endl;
            close(sock);
            return;
        }

        string server_reply(response);
        if (server_reply == "AUTHENTICATION_SUCCESS" || server_reply == "REGISTER_SUCCESS") {
            cout << "[✔] " << server_reply << endl;
            usleep(1200000);  // Pause before moving to next screen
            break;
        } else {
            cout << ((choice == 1) ? "[✖] Login failed! Try again." : "[✖] Registration failed! Username may already exist.") << endl;
        }
    }
}


void Client::start() {
    // Handle login and determine user role (student or instructor)
    authenticate();

    // Clear the terminal screen and pause briefly
    system("clear");
    usleep(200000);  // Sleep for 200ms to improve UX

    pthread_t thread;

    // Launch appropriate handler in a separate thread based on user role
    if (role == "s") {
        pthread_create(&thread, nullptr, studentHandler, this);
    } else {
        pthread_create(&thread, nullptr, instructorHandler, this);
    }

    // Wait for the thread to finish before exiting
    pthread_join(thread, nullptr);
}
