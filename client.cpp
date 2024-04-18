#include <iostream>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

int main()
{
    // Create the socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        std::cerr << "Failed to create socket." << std::endl;
        return 1;
    }

    // Define the server address
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(12345);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Using localhost for example

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        std::cerr << "Connection to server failed." << std::endl;
        return 1;
    }

    std::cout << "Connected to the server successfully." << std::endl;

    // Input move
    std::string move;
    std::cout << "Enter your move (rock, paper, scissors): ";
    std::cin >> move;

    // Send move to the server
    send(sock, move.c_str(), move.length(), 0);

    // Receive the result from the server
    char buffer[1024] = {0};
    int bytesReceived = read(sock, buffer, sizeof(buffer) - 1);
    if (bytesReceived < 0)
    {
        std::cerr << "Failed to read data from server." << std::endl;
    }
    else
    {
        std::cout << "Result: " << buffer << std::endl;
    }

    // Close the socket
    close(sock);
    return 0;
}
