Smart Expense Tracker Pro 🍫
C++ SQLite License UI

A premium, high-performance desktop finance management platform. This application combines the raw speed and efficiency of a C++ backend with the modern, refined aesthetics of a Chocolate Brown & White web interface.

✨ Key Features
Instant Access: No mandatory login required. Jump straight into your financial dashboard.
Luxury Dashboard: A high-end overview of your Total Income, Expenses, and Net Savings.
Interactive Analytics: Elegant Donut and Bar charts providing deep insights into your spending habits.
Smart Transaction Management:
Rapid transaction logging with categorized entries.
Inline editing and deletion.
Full searchable history.
Premium Aesthetics: A curated color palette designed for focus and professional appeal.
Privacy First: 100% offline local storage using SQLite. Your data never leaves your machine.
🚀 Getting Started
1. Prerequisites
Windows 10/11
(Optional) MinGW-w64 (g++) if you wish to recompile from source.
2. Fast Launch (Ready-to-Run)
The project comes with a pre-compiled executable for convenience.

Download or clone this repository.
Double-click on run.bat.
The server will start, and your default browser will automatically open to http://localhost:8080.
3. Build from Source
To modify the code and rebuild the application:

Open PowerShell/Terminal in the project directory.
Run the build script:
.\build.bat
This will compile main.cpp and link it with the optimized SQLite3 object.
🛠️ Technical Stack
Backend: Custom single-threaded Winsock HTTP server written in C++11/14.
Database: SQLite3 for robust, ACID-compliant data persistence.
Frontend: Clean, Modern Web UI using Vanilla HTML5, CSS3, and ES6 JavaScript.
API: RESTful interface handling JSON payloads via the nlohmann/json library.
Styling: Custom CSS architecture with a "Chocolate & White" professional design system.
📂 Project Structure
/frontend: All web assets (HTML, CSS, JS).
main.cpp: Core C++ logic and HTTP server implementation.
database.h: C++ wrapper for SQLite operations.
server.h: Lightweight HTTP server framework.
expenses.db: Local database file (generated on first run).
run.bat: One-click startup script.
🎓 About
This project demonstrates a Hybrid Software Architecture, showcasing how to bridge low-level systems programming with high-level user interface design to create a seamless, professional desktop experience.

© 2024 Smart Expense Tracker Pro | Excellence in Personal Finance
