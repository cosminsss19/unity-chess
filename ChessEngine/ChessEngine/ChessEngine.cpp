#include "pch.h"
#include "ChessEngine.h"
#include <string>
#include <vector>
#include <limits>
#include <algorithm>

template <typename T>
T customMin(T a, T b) {
    return (a < b) ? a : b;
}

// Custom max function
template <typename T>
T customMax(T a, T b) {
    return (a > b) ? a : b;
}

struct Move {
    std::string notation;
};

// Forward declaration for the EvaluateBoard function
int EvaluateBoard(const std::string& boardState);

std::vector<Move> GenerateMoves(const std::string& boardState, bool isWhite) {
    std::vector<Move> moves;

    // Example: Generate pawn moves (this should be replaced with actual move generation logic)
    for (size_t i = 0; i < boardState.size(); ++i) {
        char piece = boardState[i];
        if (isWhite && piece == 'P') {
            // Generate white pawn moves
            // For simplicity, let's assume pawns can only move forward one square
            if (i + 8 < boardState.size() && boardState[i + 8] == ' ') {
                moves.push_back({ "P" + std::to_string(i) + std::to_string(i + 8) });
            }
        }
        else if (!isWhite && piece == 'p') {
            // Generate black pawn moves
            // For simplicity, let's assume pawns can only move forward one square
            if (i - 8 >= 0 && boardState[i - 8] == ' ') {
                moves.push_back({ "p" + std::to_string(i) + std::to_string(i - 8) });
            }
        }
        // Add logic for other pieces (knights, bishops, rooks, queens, kings)
    }

    return moves;
}

int Minimax(std::string boardState, int depth, int alpha, int beta, bool maximizingPlayer) {
    if (depth == 0) {
        return EvaluateBoard(boardState);
    }

    std::vector<Move> moves = GenerateMoves(boardState, maximizingPlayer);
    if (maximizingPlayer) {
        int maxEval = -2147483648;
        for (const Move& move : moves) {
            // Apply move (this should be replaced with actual move application logic)
            std::string newBoardState = boardState; // Dummy new board state
            int eval = Minimax(newBoardState, depth - 1, alpha, beta, false);
            maxEval = customMax(maxEval, eval);
            alpha = customMax(alpha, eval);
            if (beta <= alpha) {
                break;
            }
        }
        return maxEval;
    }
    else {
        int minEval = 2147483647;
        for (const Move& move : moves) {
            // Apply move (this should be replaced with actual move application logic)
            std::string newBoardState = boardState; // Dummy new board state
            int eval = Minimax(newBoardState, depth - 1, alpha, beta, true);
            minEval = customMin(minEval, eval);
            beta = customMin(beta, eval);
            if (beta <= alpha) {
                break;
            }
        }
        return minEval;
    }
}

// Simple evaluation function
int EvaluateBoard(const std::string& boardState) {
    // Implement a simple evaluation function
    // For simplicity, let's assume positive values for white advantage and negative for black
    int score = 0;
    // Example: count material (this should be replaced with actual evaluation logic)
    for (char piece : boardState) {
        switch (piece) {
        case 'P': score += 1; break; // White pawn
        case 'p': score -= 1; break; // Black pawn
        case 'N': score += 3; break; // White knight
        case 'n': score -= 3; break; // Black knight
        case 'B': score += 3; break; // White bishop
        case 'b': score -= 3; break; // Black bishop
        case 'R': score += 5; break; // White rook
        case 'r': score -= 5; break; // Black rook
        case 'Q': score += 9; break; // White queen
        case 'q': score -= 9; break; // Black queen
        case 'K': score += 100; break; // White king
        case 'k': score -= 100; break; // Black king
        }
    }
    return score;
}

// Modified function signature to accept isWhite parameter
extern "C" __declspec(dllexport) const char* GetBestMove(const char* boardState, int depth, bool isWhite)
{
    std::string board(boardState);

    // Now we use the isWhite parameter passed from Unity
    std::vector<Move> moves = GenerateMoves(board, isWhite);
    std::string bestMove;
    int bestValue = -2147483648;

    for (const Move& move : moves) {
        // Apply move (this should be replaced with actual move application logic)
        std::string newBoardState = board; // Dummy new board state
        int moveValue = Minimax(newBoardState, depth - 1, -2147483648, 2147483647, !isWhite);

        int bestValue = (std::numeric_limits<int>::min)();
        if (moveValue > bestValue) {
            bestValue = moveValue;
            bestMove = move.notation;
        }
    }

    static std::string result;
    result = bestMove;
    return result.c_str();
}