#include "auth.h"

unordered_map<string, string> AuthManager::student_db;
unordered_map<string, string> AuthManager::instructor_db;

string AuthManager::hash_password(const string& password) {
    hash<string> hasher;
    return to_string(hasher(password));
}

void AuthManager::load_users(const string& filename, unordered_map<string, string>& user_db) {
    int fd = open(filename.c_str(), O_RDONLY);

    if (fd == -1) {
        cerr << "Warning: " << filename << " not found. Creating a new one." << endl;
        fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) {
            cerr << "Error: Unable to create file " << filename << endl;
            return;
        }
        close(fd);
        return;
    }
    

    char buffer[2048];
    int bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);

    if (bytes_read <= 0) return;

    buffer[bytes_read] = '\0';
    stringstream file_content(buffer);
    string line, username, password;

    while (getline(file_content, line)) {
        istringstream iss(line);
        if (iss >> username >> password) {
            user_db[username] = password;
        }
    }
}

AuthManager::AuthManager() {
    cout << "[+] loading user data..." << endl;
    load_users("../data/students.txt", student_db);
    load_users("../data/instructors.txt", instructor_db);
}

bool AuthManager::save_user(const string& filename, const string& username, const string& password) {
    int fd = open(filename.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd == -1) {
        cerr << "Error: Unable to open file " << filename << endl;
        return false;
    }

    string entry = username + " " + password + "\n";
    write(fd, entry.c_str(), entry.length());
    close(fd);
    return true;
}

bool AuthManager::register_user(const string& username, const string& password, const string& user_type) {
    string hashed_pass = hash_password(password);
    if (user_type == "student") {
        if (student_db.find(username) != student_db.end()) {
            cerr << "Error: Student already exists!" << endl;
            return false;
        }
        student_db[username] = hashed_pass;
        return save_user("../data/students.txt", username, hashed_pass);
    } else if (user_type == "instructor") {
        if (instructor_db.find(username) != instructor_db.end()) {
            cerr << "Error: Instructor already exists!" << endl;
            return false;
        }
        instructor_db[username] = hashed_pass;
        return save_user("../data/instructors.txt", username, hashed_pass);
    }

    cerr << "Error: Invalid user type!" << endl;
    return false;
}

bool AuthManager::authenticate_user(const string& username, const string& password, const string& user_type) {
    string hashed_pass = hash_password(password);

    if (user_type == "student") {
        return (student_db.find(username) != student_db.end() && student_db[username] == hashed_pass);
    } else if (user_type == "instructor") {
        return (instructor_db.find(username) != instructor_db.end() && instructor_db[username] == hashed_pass);
    }

    cerr << "Error: Invalid user type!" << endl;
    return false;
}