#include "ui.h"

void UI_elements::displayHeader(const string& title){
    cout << "╔";
    for (int i = 0; i < 30; i++) cout << "═";
    cout << "╗\n";
    cout << "║ " << left << setw(28) << title << " ║\n";

    cout << "╚";
    for (int i = 0; i < 30; i++) cout << "═";
    cout << "╝\n";
}

void UI_elements::displayMenu() {
    cout << "\n=========Main Menu=========\n";
    cout << "1. Login\n";
    cout << "2. Register\n";
    cout << "3. Exit\n";
    cout << "-------------------------------\n";
    cout << "Choose an option: ";
}

void UI_elements::displayStudentMenu() {
    cout << "\n==========Student Menu===========\n";
    cout << "1. Take Exam\n";
    cout << "2. Show Dashboard\n";
    cout << "3. Logout\n";
    cout << "----------------------------------\n";
    cout << "Choose an option: ";
}

void UI_elements::displayInstructorMenu() {
    cout << "\n=======Instructor Menu=======\n";
    cout << "1. Upload Exam\n";
    cout << "2. Upload Seating Pattern\n";
    cout << "3. Show Student Performance\n";
    cout << "4. View Uploaded Exams\n";
    cout << "5. Logout\n";
    cout << "------------------------------\n";
    cout << "Choose an option: ";
}

void UI_elements::displayExamOptions(){
    cout << "\n============choice============\n";
    cout << "1. Next Question\n";
    cout << "2. Previous Question\n";
    cout << "3. Answer this question\n";
    cout << "4. Clear your answer\n";
    cout << "5. Go to specific question\n";
    cout << "6. Not Attempted Questions\n";
    cout << "7. Submit Exam";
    cout << "\n--------------------------------\n";
    cout << "Enter your choice: ";
}