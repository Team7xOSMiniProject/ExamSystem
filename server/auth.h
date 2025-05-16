#ifndef AUTH_H
#define AUTH_H

#include <iostream>
#include <unordered_map>
#include <string>
#include <fcntl.h>     
#include <unistd.h>   
#include <sstream>   
#include <functional> 

using namespace std;

class AuthManager {
private:
    static unordered_map<string, string> student_db;
    static unordered_map<string, string> instructor_db;

    static string hash_password(const string& password);
    static void load_users(const string& filename, unordered_map<string, string>& user_db);
    static bool save_user(const string& filename, const string& username, const string& password);

public:
    AuthManager();
    static bool register_user(const string& username, const string& password, const string& user_type);
    static bool authenticate_user(const string& username, const string& password, const string& user_type);
};

#endif
