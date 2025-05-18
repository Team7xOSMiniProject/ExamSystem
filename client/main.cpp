#include "client.h"

#define SERVER_IP "127.0.0.1"
#define PORT 8080

int main() {
    // Create a client object with the specified server IP and port
    Client client(SERVER_IP, PORT);

    // Start the client (handles connection, login, and interaction)
    client.start();

    return 0;
}
