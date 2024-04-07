#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <netinet/in.h>
#include <unistd.h>
#include <string>


class GameSession {
public:
    GameSession(int player1Socket, int player2Socket)
        : player1Socket(player1Socket), player2Socket(player2Socket) {}

    void start() {
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

    void receiveMoves() {
        char buffer[1024];
        read(player1Socket, buffer, sizeof(buffer));
        player1Move = stringToMove(std::string(buffer));

        read(player2Socket, buffer, sizeof(buffer));
        player2Move = stringToMove(std::string(buffer));
    }

    std::string determineWinner() {
        if (player1Move == player2Move) return "Draw";
        if ((player1Move == Move::Rock && player2Move == Move::Scissors) ||
            (player1Move == Move::Paper && player2Move == Move::Rock) ||
            (player1Move == Move::Scissors && player2Move == Move::Paper)) {
            return "Player 1 wins";
        }
        return "Player 2 wins";
    }

    void sendResults(const std::string& result) {
        std::string player1Response = "You played " + moveToString(player1Move) + ". Player 2 played " + moveToString(player2Move) + ". " + result;
        std::string player2Response = "You played " + moveToString(player2Move) + ". Player 1 played " + moveToString(player1Move) + ". " + result;

        send(player1Socket, player1Response.c_str(), player1Response.length(), 0);
        send(player2Socket, player2Response.c_str(), player2Response.length(), 0);
    }

    void closeSockets() {
        close(player1Socket);
        close(player2Socket);
    }
};
std::queue<int> waitingQueue;
std::mutex queueMutex;
std::condition_variable queueCondVar;

enum class Move {
    Rock,
    Paper,
    Scissors,
    Invalid
};

Move stringToMove(const std::string& moveStr) {
    if (moveStr == "rock") return Move::Rock;
    if (moveStr == "paper") return Move::Paper;
    if (moveStr == "scissors") return Move::Scissors;
    return Move::Invalid;
}

std::string moveToString(Move move) {
    switch (move) {
        case Move::Rock: return "rock";
        case Move::Paper: return "paper";
        case Move::Scissors: return "scissors";
        default: return "invalid";
    }
}

std::string determineWinner(Move player1Move, Move player2Move) {
    if (player1Move == player2Move) return "Draw";
    if ((player1Move == Move::Rock && player2Move == Move::Scissors) ||
        (player1Move == Move::Paper && player2Move == Move::Rock) ||
        (player1Move == Move::Scissors && player2Move == Move::Paper)) {
        return "Player 1 wins";
    }
    return "Player 2 wins";
}

void handleClient(int player1Socket, int player2Socket) {
    char buffer[1024];
    std::string player1MoveStr, player2MoveStr;
    Move player1Move, player2Move;

    // Receive moves from both players
    read(player1Socket, buffer, sizeof(buffer));
    player1MoveStr = std::string(buffer);
    player1Move = stringToMove(player1MoveStr);

    read(player2Socket, buffer, sizeof(buffer));
    player2MoveStr = std::string(buffer);
    player2Move = stringToMove(player2MoveStr);

    // Determine the winner
    std::string result = determineWinner(player1Move, player2Move);

    // Send the results back to the players
    std::string player1Response = "You played " + moveToString(player1Move) + ". Player 2 played " + moveToString(player2Move) + ". " + result;
    std::string player2Response = "You played " + moveToString(player2Move) + ". Player 1 played " + moveToString(player1Move) + ". " + result;

    send(player1Socket, player1Response.c_str(), player1Response.length(), 0);
    send(player2Socket, player2Response.c_str(), player2Response.length(), 0);

    // Close the sockets
    close(player1Socket);
    close(player2Socket);
}

void acceptConnections(int serverSocket) {
    while (true) {
        int clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket < 0) {
            std::cerr << "Failed to accept connection" << std::endl;
            continue;
        }
        std::lock_guard<std::mutex> lock(queueMutex);
        waitingQueue.push(clientSocket);
        queueCondVar.notify_one();
    }
}

int main() {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return 1;
    }

    // Set up the server address
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(12345);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket to the address
    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Failed to bind socket" << std::endl;
        return 1;
    }

    // Listen for connections
    if (listen(serverSocket, 10) < 0) {
        std::cerr << "Failed to listen on socket" << std::endl;
        return 1;
    }

    // Start the thread to accept connections
    std::thread acceptThread(acceptConnections, serverSocket);

    // Main game loop
    while (true) {
        std::unique_lock<std::mutex> lock(queueMutex);
        queueCondVar.wait(lock, [] { return !waitingQueue.empty(); });
    
        // Start a new game session with two players from the queue
        if (waitingQueue.size() >= 2) {
            int player1 = waitingQueue.front();
            waitingQueue.pop();
            int player2 = waitingQueue.front();
            waitingQueue.pop();
    
            std::thread gameThread([&]{
                GameSession session(player1, player2);
                session.start();
            });
            gameThread.detach();
        }
    }


    acceptThread.join();
    close(serverSocket);
    return 0;
}
