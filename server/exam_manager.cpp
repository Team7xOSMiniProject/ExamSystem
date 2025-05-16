#include "exam_manager.h"

// Mutex to protect concurrent access to exam_list.txt when appending new exams
pthread_mutex_t file_mutex4 = PTHREAD_MUTEX_INITIALIZER;

/**
 * Parses an exam file and stores its questions, answers, and metadata.
 * 
 * @param exam_type Type of exam (e.g., "g" for scheduled, "p" for practice).
 * @param input_file Path to the input file containing questions and answers.
 * @param exam_name Name to assign to this exam.
 * @param instructor Name of the instructor who created the exam.
 * @param duration Duration of the exam in minutes.
 * @param start_time Scheduled start time of the exam.
 * 
 * @return True if parsing and storage were successful, false otherwise.
 */
bool ExamManager::parse_exam(const string& exam_type, const string& input_file, const string& exam_name, const string& instructor, int duration, const string& start_time){
    
    ifstream infile(input_file);  // Open the input exam file
    string line, currentQuestion, optionA, optionB, optionC, optionD, correctAnswer;
    vector<string> questions;  // Holds formatted questions with options
    vector<string> answers;    // Holds correct answers for each question
    int questionCount = 0;
    bool readingQuestion = false;  // Tracks whether we are currently reading a question block

    // Read file line-by-line to extract questions, options, and answers
    while (getline(infile, line)) {
        if (line.rfind("Q:", 0) == 0) {  // New question line detected
            if (!currentQuestion.empty()) {  
                // Store previous question with options and correct answer before starting new question
                questions.push_back(currentQuestion + "\nA) " + optionA + "\nB) " + optionB + "\nC) " + optionC + "\nD) " + optionD);
                answers.push_back(correctAnswer);
                questionCount++;
            }
            currentQuestion = line;  // Start a new question
            readingQuestion = true;
        } else if (line.rfind("A)", 0) == 0) {
            optionA = line.substr(2);  // Extract option A text
            readingQuestion = false;
        } else if (line.rfind("B)", 0) == 0) {
            optionB = line.substr(2);  // Extract option B text
        } else if (line.rfind("C)", 0) == 0) {
            optionC = line.substr(2);  // Extract option C text
        } else if (line.rfind("D)", 0) == 0) {
            optionD = line.substr(2);  // Extract option D text
        } else if (line.rfind("A:", 0) == 0) {
            correctAnswer = line.substr(2);  // Extract correct answer
        } else if (readingQuestion) {  
            // Append continuation lines to the current question text
            currentQuestion += "\n" + line;
        }
    }

    // Add last question after EOF if any
    if (!currentQuestion.empty()) {
        questions.push_back(currentQuestion + "\nA) " + optionA + "\nB) " + optionB + "\nC) " + optionC + "\nD) " + optionD);
        answers.push_back(correctAnswer);
        questionCount++;
    }

    infile.close();

    // Validate that at least one question was found
    if (questions.empty()) {
        cout << "[-] Error: No valid questions found in file.\n";
        return false;
    }

    // Define file paths for metadata, questions, and answers
    string metadataFile = "../data/exams/metadata_" + exam_name + ".txt";
    string questionsFile = "../data/exams/questions_" + exam_name + ".txt";
    string answersFile = "../data/exams/answers_" + exam_name + ".txt";

    // Write metadata file with exam details and paths to questions and answers files
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

    // Write questions to separate file
    ofstream questionFile(questionsFile);
    for (const string &q : questions) {
        questionFile << q << "\n\n";
    }
    questionFile.close();

    // Write answers to separate file
    ofstream answerFile(answersFile);
    for (const string &a : answers) {
        answerFile << a << "\n";
    }
    answerFile.close();

    // Append exam name and metadata file path to exam list file, thread-safe using mutex
    pthread_mutex_lock(&file_mutex4);
    ofstream examList("../data/exams/exam_list.txt", ios::app);
    examList << exam_name << "|" << metadataFile << "\n";
    examList.close();
    pthread_mutex_unlock(&file_mutex4);

    cout << "[+] Exam successfully parsed and stored!\n";
    return true;
}

/**
 * Loads metadata for all exams listed in the given exam list file.
 * 
 * @param exam_list_file Path to the file containing exam names and metadata paths.
 * @return Vector of metadata strings for each exam, excluding questions and answers file info.
 */
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

        // Parse exam name and metadata file path from each line
        if (getline(iss, examName, '|') && getline(iss, metadataPath)) {
            ifstream metadataFile(metadataPath);
            if (!metadataFile) {
                cerr << "Error: Unable to open metadata file " << metadataPath << "\n";
                continue;
            }

            ostringstream metadata;
            string metaLine;
            // Read metadata lines, stop before questions/answers file info
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

/**
 * Retrieves the metadata file path for a given exam name.
 * 
 * @param examName The name of the exam to find.
 * @return The file path to the exam's metadata file or empty string if not found.
 */
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

/**
 * Retrieves the questions file path from a given exam metadata file.
 * 
 * @param metadataPath Path to the exam metadata file.
 * @return Path to the questions file or empty string if not found.
 */
string ExamManager::getQuestionsFilePath(const string& metadataPath) {
    ifstream metadataFile(metadataPath);
    if (!metadataFile) {
        cerr << "Error: Unable to open metadata file " << metadataPath << "\n";
        return "";
    }

    string line, questionFilePath;
    while (getline(metadataFile, line)) {
        if (line.find("Questions File:") != string::npos) {
            questionFilePath = line.substr(line.find(":") + 2);
            break;
        }
    }

    return questionFilePath;
}

/**
 * Sends the questions of a specified exam over a socket.
 * 
 * @param sock The socket descriptor to send data over.
 * @param examName The name of the exam whose questions are to be sent.
 */
void ExamManager::sendExamQuestions(int sock, const string& examName) {
    // Retrieve the path to the metadata file for the exam
    string metadataPath = getMetadataFilePath(examName);
    if (metadataPath.empty()) {
        string errorMsg = "Error: Exam not found.\n";
        send(sock, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }

    // Retrieve the questions file path from metadata
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

    // Read entire questions file content
    string questionData, line;
    while (getline(questionFile, line)) {
        questionData += line + "\n";
    }
    
    // Handle empty questions file case
    if (questionData.empty()) {
        questionData = "Error: Questions file is empty.\n";
    }
    
    // Send questions data to client via socket
    send(sock, questionData.c_str(), questionData.size(), 0);
}
