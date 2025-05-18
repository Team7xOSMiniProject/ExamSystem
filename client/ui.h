#ifndef UI_H
#define UI_H

#include <iostream>
#include <string>
#include <unistd.h>
#include <cstring>
#include <algorithm>
#include <iomanip>
#include <vector>
#include <stdlib.h>

using namespace std;

class UI_elements{
    public:
    static void displayHeader(const string& title);
    static void displayMenu();
    static void displayExamOptions();
    static void displayStudentMenu();
    static void displayInstructorMenu(); 
};

#endif