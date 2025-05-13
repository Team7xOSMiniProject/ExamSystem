#include "client.h"
#define SERVER_IP "127.0.0.1"
#define PORT 8080

int main() {
    Client client(SERVER_IP, PORT);
    client.start();
    return 0;
}
