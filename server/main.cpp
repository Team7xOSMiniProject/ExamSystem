// main.cpp
// Entry point for the server-side application.

#include "server.h"  // Include server class definition

int main() {
    // Create a server object listening on port 8080
    Server server(8080);

    // Start the server (bind, listen, accept connections, etc.)
    server.start();

    return 0;  // Exit successfully
}
