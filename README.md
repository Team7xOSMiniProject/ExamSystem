# 🧠 MCQ Exam System (Terminal-Based)

A terminal-based **MCQ (Multiple Choice Questions) Exam System** built in C++ using socket programming and file management. This system supports both **students** and **instructors**, allowing practice and scheduled exams with detailed performance analysis.

---
## 🎥 Project Demo

Watch the full project and ppt description here: https://youtu.be/n-OU9DSvR0w

## 📁 Project Structure

```
MCQ_EXAM_SYSTEM/
├── client/              # Client-side code for both students and instructors
│   ├── client.cpp/h     # Client logic
│   ├── main.cpp         # Entry point for client
│   ├── ui.cpp/h         # UI elements for CLI
│   ├── exam_questions.txt  # Sample question file
├── server/              # Server-side logic
│   ├── auth.cpp/h       # Authentication logic
│   ├── exam_manager.cpp/h  # Exam handling logic
│   ├── main.cpp         # Entry point for server
│   ├── server.cpp/h     # Server-side socket handling
├── data/                # Storage for exam and user data
│   ├── exams/           # Uploaded exams
│   ├── results/         # Student result files
│   ├── instructors.txt  # Instructor credentials
│   ├── students.txt     # Student credentials
├── README.md            # Project documentation
```

---

## 🧑‍💻 Features

### 👨‍🎓 Student Panel
- Take **Practice Test** (unlimited attempts)
- Take **Scheduled Test** (only at scheduled time, one attempt)
- View **Dashboard** with:
  - Exam score summary
  - Time taken per question
  - Answer review (correct/wrong, selected option)
  - Attempt history
  - Performance in the exam
  - Leaderboard

### 👨‍🏫 Instructor Panel
- Upload new exams (in structured `.txt` format)
- Upload seating pattern(not functional)
- View student performance with per-question statistics
- View all uploaded exams
- Schedule exams with a specific date and time

### 🔐 Authentication
- Register/Login with hashed password storage
- Role-based access (student/instructor)

### 📊 Exam Types
- **Practice Test**: Free attempt anytime
- **Scheduled Test**: Set by instructor with start time, single attempt only

---

## 📄 Question File Format

Each question must follow this structure:

```
Q: Who was the first President of India?
A) Dr. Rajendra Prasad
B) Dr. S. Radhakrishnan
C) Jawaharlal Nehru
D) Mahatma Gandhi
A: A
```

- `Q:` — marks the beginning of a question
- `A)` to `D)` — answer options
- `A:` — correct answer (must be A, B, C, or D)

Multi-line questions are supported under `Q:` until an option (`A)` to `D)`) begins.

---

## ⚙️ How It Works

1. **Instructor uploads** an exam file and selects exam type (Practice/Scheduled)
2. **Student logs in**, views available exams
3. **Student takes the exam** and submits
4. **Immediate result** shown after submission
5. **Results stored** with timestamp and performance metrics
6. **Per question analysis and leaderboard** shown once exam is over
---

## 🛠 Technologies Used

- C++ (Core logic)
- POSIX Sockets (Client-Server Communication)
- File I/O (No database used)
- Terminal-based UI
- Password Hashing (e.g., SHA or simple hash function)

---

## 🚀 Getting Started

### 1. Compile Server
```bash
cd server
make
./server
```

### 2. Compile Client
```bash
cd client
make
./client
```

---

## 📌 Future Enhancements

- GUI with Qt or Web Frontend
- Encrypted file transmission
- Admin dashboard
- Result export to PDF or CSV
- OTP-based exam access

---

## 🙌 Author
 
Developed by NIT Durgapur Students- Sounak Bhattacharya, Raj Patel, Sneha Saha, Mampi Mahato, Aman Kachhap, Ahmed Khalid Hafiz, Aman Brijbhushan Chauhan, Aayush, Rage Sai Santosh, Abir Soren
