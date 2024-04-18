#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>
#include <cctype>

// Declare the Move enum before it's used in any class or function
enum class Move
{
    Rock,
    Paper,
    Scissors,
    Invalid
};

// Forward declare utility functions before the GameSession class
Move stringToMove(const std::string &moveStr);
std::string moveToString(Move move);

class GameSession
{
public:
    GameSession(int player1Socket, int player2Socket)
        : player1Socket(player1Socket), player2Socket(player2Socket) {}

    void start()
    {
        receiveMoves();
        std::string result = determineWinner();
        sendResults(result);
        closeSockets();
    }

private:
    int player1Socket;
    int player2Socket;
    Move player1Move;
    Move player2Move;

    void receiveMoves()
    {
        char buffer[1024] = {0};
        read(player1Socket, buffer, sizeof(buffer) - 1);
        std::string move1 = trimInput(std::string(buffer));
        player1Move = stringToMove(move1);

        memset(buffer, 0, sizeof(buffer));
        read(player2Socket, buffer, sizeof(buffer) - 1);
        std::string move2 = trimInput(std::string(buffer));
        player2Move = stringToMove(move2);
    }

    std::string trimInput(const std::string &input)
    {
        size_t first = input.find_first_not_of(" \t\n\r");
        size_t last = input.find_last_not_of(" \t\n\r");
        return (first == std::string::npos) ? "" : input.substr(first, last - first + 1);
    }

    std::string determineWinner()
    {
        if (player1Move == player2Move)
            return "Draw";
        if ((player1Move == Move::Rock && player2Move == Move::Scissors) ||
            (player1Move == Move::Paper && player2Move == Move::Rock) ||
            (player1Move == Move::Scissors && player2Move == Move::Paper))
            return "Player 1 wins";
        return "Player 2 wins";
    }

    void sendResults(const std::string &result)
    {
        std::string player1Response = "You played " + moveToString(player1Move) + ". Player 2 played " + moveToString(player2Move) + ". " + result;
        std::string player2Response = "You played " + moveToString(player2Move) + ". Player 1 played " + moveToString(player1Move) + ". " + result;

        send(player1Socket, player1Response.c_str(), player1Response.length(), 0);
        send(player2Socket, player2Response.c_str(), player2Response.length(), 0);
    }

    void closeSockets()
    {
        close(player1Socket);
        close(player2Socket);
    }
};

Move stringToMove(const std::string &moveStr)
{
    std::string lower = moveStr;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower == "rock")
        return Move::Rock;
    if (lower == "paper")
        return Move::Paper;
    if (lower == "scissors")
        return Move::Scissors;
    return Move::Invalid;
}

std::string moveToString(Move move)
{
    switch (move)
    {
    case Move::Rock:
        return "rock";
    case Move::Paper:
        return "paper";
    case Move::Scissors:
        return "scissors";
    default:
        return "invalid";
    }
}

std::queue<int> waitingQueue;
std::mutex queueMutex;
std::condition_variable queueCondVar;

void acceptConnections(int serverSocket)
{
    while (true)
    {
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientAddrLen);
        if (clientSocket < 0)
        {
            std::cerr << "Failed to accept connection" << std::endl;
            continue;
        }

        std::lock_guard<std::mutex> lock(queueMutex);
        waitingQueue.push(clientSocket);
        queueCondVar.notify_one();
    }
}

int main()
{
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0)
    {
        std::cerr << "Failed to create socket" << std::endl;
        return 1;
    }

    int yes = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
    {
        std::cerr << "Failed to set socket options" << std::endl;
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(12345);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        std::cerr << "Failed to bind socket" << std::endl;
        return 1;
    }

    if (listen(serverSocket, 10) < 0)
    {
        std::cerr << "Failed to listen on socket" << std::endl;
        return 1;
    }

    std::thread acceptThread(acceptConnections, serverSocket);
    acceptThread.detach();

    while (true)
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        queueCondVar.wait(lock, []
                          { return !waitingQueue.empty(); });

        if (waitingQueue.size() >= 2)
        {
            int player1Socket = waitingQueue.front();
            waitingQueue.pop();
            int player2Socket = waitingQueue.front();
            waitingQueue.pop();

            std::thread gameThread([player1Socket, player2Socket]()
                                   {
                GameSession session(player1Socket, player2Socket);
                session.start(); });
            gameThread.detach();
        }
    }

    close(serverSocket);
    return 0;
}
