#include "auth.h"

// Static unordered_maps to store username-password pairs for students and instructors
unordered_map<string, string> AuthManager::student_db;
unordered_map<string, string> AuthManager::instructor_db;

/**
 * Hashes a plaintext password into a string representation.
 * Uses std::hash for demonstration purposes (not cryptographically secure).
 * 
 * @param password The plaintext password to hash.
 * @return The hashed password as a string.
 */
string AuthManager::hash_password(const string& password) {
    hash<string> hasher;
    return to_string(hasher(password));
}

/**
 * Loads users from a given file into the provided user_db map.
 * If the file does not exist, it creates an empty one.
 * 
 * @param filename Path to the file containing username-password entries.
 * @param user_db Reference to the map where user data will be loaded.
 */
void AuthManager::load_users(const string& filename, unordered_map<string, string>& user_db) {
    int fd = open(filename.c_str(), O_RDONLY);

    if (fd == -1) {
        // File does not exist, create an empty file
        cerr << "Warning: " << filename << " not found. Creating a new one." << endl;
        fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) {
            cerr << "Error: Unable to create file " << filename << endl;
            return;
        }
        close(fd);
        return;
    }
    
    // Buffer to read file contents
    char buffer[2048];
    int bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);

    // If nothing was read, return early
    if (bytes_read <= 0) return;

    buffer[bytes_read] = '\0';  // Null-terminate buffer
    stringstream file_content(buffer);
    string line, username, password;

    // Parse file line-by-line to extract username and password pairs
    while (getline(file_content, line)) {
        istringstream iss(line);
        if (iss >> username >> password) {
            user_db[username] = password;
        }
    }
}

/**
 * Constructor - loads user data for students and instructors at initialization.
 */
AuthManager::AuthManager() {
    cout << "[+] loading user data..." << endl;
    load_users("../data/students.txt", student_db);
    load_users("../data/instructors.txt", instructor_db);
}

/**
 * Appends a new user entry to the specified file.
 * 
 * @param filename Path to the file where user data should be saved.
 * @param username Username of the new user.
 * @param password Hashed password of the new user.
 * @return True if successfully saved, false otherwise.
 */
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

/**
 * Registers a new user of given type with username and password.
 * Rejects duplicate usernames.
 * 
 * @param username New user's username.
 * @param password New user's plaintext password.
 * @param user_type "student" or "instructor".
 * @return True if registration was successful, false otherwise.
 */
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

    // Invalid user type
    cerr << "Error: Invalid user type!" << endl;
    return false;
}

/**
 * Authenticates a user by verifying the username and password hash.
 * 
 * @param username The username to authenticate.
 * @param password The plaintext password to verify.
 * @param user_type The type of user ("student" or "instructor").
 * @return True if authentication succeeds, false otherwise.
 */
bool AuthManager::authenticate_user(const string& username, const string& password, const string& user_type) {
    string hashed_pass = hash_password(password);

    if (user_type == "student") {
        return (student_db.find(username) != student_db.end() && student_db[username] == hashed_pass);

    } else if (user_type == "instructor") {
        return (instructor_db.find(username) != instructor_db.end() && instructor_db[username] == hashed_pass);
    }

    // Invalid user type
    cerr << "Error: Invalid user type!" << endl;
    return false;
}
