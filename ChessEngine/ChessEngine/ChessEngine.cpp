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
    bool isCastling = false;
    bool isKingsideCastling = false;
};

// Forward declaration for the EvaluateBoard function
int EvaluateBoard(const std::string& boardState);

// Apply a move to the board and return the new board state
std::string ApplyMove(const std::string& boardState, const Move& move) {
    std::string newBoardState = boardState;
    
    // Parse the move notation - format is [piece][startPos][endPos]
    char piece = move.notation[0];
    int startPos = std::stoi(move.notation.substr(1, move.notation.length() - 3));
    int endPos = std::stoi(move.notation.substr(move.notation.length() - 2));
    
    // Move the piece
    newBoardState[endPos] = piece;
    newBoardState[startPos] = ' ';
    
    // Handle castling
    if (move.isCastling) {
        const int BOARD_SIZE = 8;
        int row = startPos / BOARD_SIZE;
        int kingStartCol = startPos % BOARD_SIZE; // Should be 4
        
        if (move.isKingsideCastling) {
            // Move the rook from h-file (7) to f-file (5)
            int rookStartPos = row * BOARD_SIZE + 7;
            int rookEndPos = row * BOARD_SIZE + 5;
            char rook = (piece == 'K') ? 'R' : 'r';
            newBoardState[rookEndPos] = rook;
            newBoardState[rookStartPos] = ' ';
        } else {
            // Move the rook from a-file (0) to d-file (3)
            int rookStartPos = row * BOARD_SIZE + 0;
            int rookEndPos = row * BOARD_SIZE + 3;
            char rook = (piece == 'K') ? 'R' : 'r';
            newBoardState[rookEndPos] = rook;
            newBoardState[rookStartPos] = ' ';
        }
    }
    
    // Handle pawn promotion
    const int BOARD_SIZE = 8;
    int row = endPos / BOARD_SIZE;
    
    // Check if pawn reached the end of the board
    if ((piece == 'P' && row == 0) || (piece == 'p' && row == 7)) {
        // Promote to queen by default
        newBoardState[endPos] = (piece == 'P') ? 'Q' : 'q';
    }
    
    return newBoardState;
}

// Function to check if a king is in check
bool IsKingInCheck(const std::string& boardState, bool isWhiteKing) {
    const int BOARD_SIZE = 8;
    char kingChar = isWhiteKing ? 'K' : 'k';
    
    // Find the king's position
    int kingPos = -1;
    for (size_t i = 0; i < boardState.size(); i++) {
        if (boardState[i] == kingChar) {
            kingPos = i;
            break;
        }
    }
    
    if (kingPos == -1) {
        // King not found (shouldn't happen in a valid game)
        return false;
    }
    
    // Generate all opponent's moves
    std::vector<Move> opponentMoves = GenerateMoves(boardState, !isWhiteKing);
    
    // Check if any opponent move can capture the king
    for (const Move& move : opponentMoves) {
        int endPos = std::stoi(move.notation.substr(move.notation.length() - 2));
        if (endPos == kingPos) {
            return true; // King is in check
        }
    }
    
    return false; // King is not in check
}

std::vector<Move> GenerateMoves(const std::string& boardState, bool isWhite) {
    std::vector<Move> moves;
    
    // Board dimensions
    const int BOARD_SIZE = 8;
    
    // Generate moves for all pieces
    for (int i = 0; i < boardState.size(); ++i) {
        char piece = boardState[i];
        
        // Skip empty squares and opponent pieces
        if (piece == ' ' || (isWhite && islower(piece)) || (!isWhite && isupper(piece))) {
            continue;
        }
        
        // Current position
        int row = i / BOARD_SIZE;
        int col = i % BOARD_SIZE;
        
        // Pawn moves
        if ((isWhite && piece == 'P') || (!isWhite && piece == 'p')) {
            GeneratePawnMoves(boardState, row, col, i, isWhite, moves);
        }
        // Knight moves
        else if ((isWhite && piece == 'N') || (!isWhite && piece == 'n')) {
            GenerateKnightMoves(boardState, row, col, i, isWhite, moves);
        }
        // Bishop moves
        else if ((isWhite && piece == 'B') || (!isWhite && piece == 'b')) {
            GenerateBishopMoves(boardState, row, col, i, isWhite, moves);
        }
        // Rook moves
        else if ((isWhite && piece == 'R') || (!isWhite && piece == 'r')) {
            GenerateRookMoves(boardState, row, col, i, isWhite, moves);
        }
        // Queen moves (combination of bishop and rook)
        else if ((isWhite && piece == 'Q') || (!isWhite && piece == 'q')) {
            GenerateBishopMoves(boardState, row, col, i, isWhite, moves); // Diagonal moves
            GenerateRookMoves(boardState, row, col, i, isWhite, moves);   // Straight moves
        }
        // King moves
        else if ((isWhite && piece == 'K') || (!isWhite && piece == 'k')) {
            GenerateKingMoves(boardState, row, col, i, isWhite, moves);
        }
    }
    
    return moves;
}

int Minimax(std::string boardState, int depth, int alpha, int beta, bool maximizingPlayer) {
    if (depth == 0) {
        return EvaluateBoard(boardState);
    }

    std::vector<Move> moves = GenerateMoves(boardState, maximizingPlayer);
    
    // No legal moves - could be stalemate or checkmate
    if (moves.empty()) {
        // This is a simple approach - ideally would check if king is in check
        return maximizingPlayer ? -20000 : 20000; // Checkmate
    }
    
    if (maximizingPlayer) {
        int maxEval = -2147483648;
        for (const Move& move : moves) {
            // Actually apply the move
            std::string newBoardState = ApplyMove(boardState, move);
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
            // Actually apply the move
            std::string newBoardState = ApplyMove(boardState, move);
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

// Helper to check if position is valid and not occupied by a friendly piece
bool IsValidMove(const std::string& boardState, int row, int col, bool isWhite) {
    const int BOARD_SIZE = 8;
    if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE) {
        return false;
    }
    
    int pos = row * BOARD_SIZE + col;
    char piece = boardState[pos];
    
    // Empty square is always valid
    if (piece == ' ') {
        return true;
    }
    
    // Check if the piece is an opponent's
    return (isWhite && islower(piece)) || (!isWhite && isupper(piece));
}

// Add a valid move to the list
void AddMove(const std::string& boardState, int startPos, int endPos, char piece, 
             std::vector<Move>& moves) {
    std::string notation = piece + std::to_string(startPos) + std::to_string(endPos);
    moves.push_back({notation});
}

// Enhanced pawn move generation with promotion
void GeneratePawnMoves(const std::string& boardState, int row, int col, int pos, 
                        bool isWhite, std::vector<Move>& moves)   
{
    const int BOARD_SIZE = 8;
    char piece = isWhite ? 'P' : 'p';
    int direction = isWhite ? -1 : 1; // White moves up (decreasing row), Black moves down
    bool lastRank = (isWhite && row + direction == 0) || (!isWhite && row + direction == 7);

    // Forward move
    int newRow = row + direction;
    if (newRow >= 0 && newRow < BOARD_SIZE) {
        int newPos = newRow * BOARD_SIZE + col;
        if (boardState[newPos] == ' ') {
            AddMove(boardState, pos, newPos, piece, moves);

            // Double forward move from starting position
            if (!lastRank && ((isWhite && row == 6) || (!isWhite && row == 1))) {
                newRow = row + 2 * direction;
                if (newRow >= 0 && newRow < BOARD_SIZE) {
                    newPos = newRow * BOARD_SIZE + col;
                    if (boardState[newPos] == ' ') {
                        AddMove(boardState, pos, newPos, piece, moves);
                    }
                }
            }
        }
    }

    // Captures
    for (int dc : {-1, 1}) {
        int newCol = col + dc;
        newRow = row + direction;
        if (newRow >= 0 && newRow < BOARD_SIZE && newCol >= 0 && newCol < BOARD_SIZE) {
            int newPos = newRow * BOARD_SIZE + newCol;
            char targetPiece = boardState[newPos];
            if (targetPiece != ' ' && ((isWhite && islower(targetPiece)) || (!isWhite && isupper(targetPiece)))) {
                AddMove(boardState, pos, newPos, piece, moves);
            }
        }
    }

    // TODO: Add en passant logic
}

// Knight move generation
void GenerateKnightMoves(const std::string& boardState, int row, int col, int pos, 
                         bool isWhite, std::vector<Move>& moves) {
    char piece = isWhite ? 'N' : 'n';
    const std::vector<std::pair<int, int>> knightMoves = {
        {-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
        {1, -2}, {1, 2}, {2, -1}, {2, 1}
    };
    
    const int BOARD_SIZE = 8;
    for (const auto& move : knightMoves) {
        int newRow = row + move.first;
        int newCol = col + move.second;
        
        if (IsValidMove(boardState, newRow, newCol, isWhite)) {
            int newPos = newRow * BOARD_SIZE + newCol;
            AddMove(boardState, pos, newPos, piece, moves);
        }
    }
}

// Bishop move generation
void GenerateBishopMoves(const std::string& boardState, int row, int col, int pos, 
                         bool isWhite, std::vector<Move>& moves) {
    char piece = isWhite ? 'B' : 'b';
    const std::vector<std::pair<int, int>> directions = {
        {-1, -1}, {-1, 1}, {1, -1}, {1, 1}
    };
    
    const int BOARD_SIZE = 8;
    for (const auto& dir : directions) {
        int newRow = row + dir.first;
        int newCol = col + dir.second;
        
        while (IsValidMove(boardState, newRow, newCol, isWhite)) {
            int newPos = newRow * BOARD_SIZE + newCol;
            AddMove(boardState, pos, newPos, piece, moves);
            
            // If we captured a piece, stop sliding in this direction
            if (boardState[newPos] != ' ') {
                break;
            }
            
            newRow += dir.first;
            newCol += dir.second;
        }
    }
}

// Rook move generation
void GenerateRookMoves(const std::string& boardState, int row, int col, int pos, 
                       bool isWhite, std::vector<Move>& moves) {
    char piece = isWhite ? 'R' : 'r';
    const std::vector<std::pair<int, int>> directions = {
        {-1, 0}, {1, 0}, {0, -1}, {0, 1}
    };
    
    const int BOARD_SIZE = 8;
    for (const auto& dir : directions) {
        int newRow = row + dir.first;
        int newCol = col + dir.second;
        
        while (IsValidMove(boardState, newRow, newCol, isWhite)) {
            int newPos = newRow * BOARD_SIZE + newCol;
            AddMove(boardState, pos, newPos, piece, moves);
            
            // If we captured a piece, stop sliding in this direction
            if (boardState[newPos] != ' ') {
                break;
            }
            
            newRow += dir.first;
            newCol += dir.second;
        }
    }
}

// Enhanced King move generation with castling
void GenerateKingMoves(const std::string& boardState, int row, int col, int pos, 
                        bool isWhite, std::vector<Move>& moves) {
    char piece = isWhite ? 'K' : 'k';
    const std::vector<std::pair<int, int>> directions = {
        {-1, -1}, {-1, 0}, {-1, 1},
        {0, -1},           {0, 1},
        {1, -1},  {1, 0},  {1, 1}
    };

    const int BOARD_SIZE = 8;
    for (const auto& dir : directions) {
        int newRow = row + dir.first;
        int newCol = col + dir.second;

        if (IsValidMove(boardState, newRow, newCol, isWhite)) {
            int newPos = newRow * BOARD_SIZE + newCol;
            AddMove(boardState, pos, newPos, piece, moves);
        }
    }

    // Castling logic
    // For simplicity, we're not tracking if the king or rook has moved previously
    // A complete implementation would need to track this

    // Check if the king is in the starting position
    if ((isWhite && row == 7 && col == 4) || (!isWhite && row == 0 && col == 4)) {
        // Kingside castling
        bool canCastleKingside = true;
        // Check if the squares between king and rook are empty
        for (int c = col + 1; c < col + 3; c++) {
            if (boardState[row * BOARD_SIZE + c] != ' ') {
                canCastleKingside = false;
                break;
            }
        }

        // Check if the rook is in place
        int rookCol = col + 3;
        char expectedRook = isWhite ? 'R' : 'r';
        if (canCastleKingside && boardState[row * BOARD_SIZE + rookCol] == expectedRook) {
            // Add castling move
            Move castlingMove;
            castlingMove.notation = piece + std::to_string(pos) + std::to_string(row * BOARD_SIZE + (col + 2));
            castlingMove.isCastling = true;
            castlingMove.isKingsideCastling = true;
            moves.push_back(castlingMove);
        }

        // Queenside castling
        bool canCastleQueenside = true;
        // Check if the squares between king and rook are empty
        for (int c = col - 1; c > col - 4; c--) {
            if (boardState[row * BOARD_SIZE + c] != ' ') {
                canCastleQueenside = false;
                break;
            }
        }

        // Check if the rook is in place
        rookCol = col - 4;
        if (canCastleQueenside && boardState[row * BOARD_SIZE + rookCol] == expectedRook) {
            // Add castling move
            Move castlingMove;
            castlingMove.notation = piece + std::to_string(pos) + std::to_string(row * BOARD_SIZE + (col - 2));
            castlingMove.isCastling = true;
            castlingMove.isKingsideCastling = false;
            moves.push_back(castlingMove);
        }
    }
}

// Enhanced evaluation function with more piece-square tables
int EvaluateBoard(const std::string& boardState) {
    const int BOARD_SIZE = 8;
    int score = 0;
    
    // Piece-square tables for positional evaluation
    static const int pawnTable[64] = {
        0,  0,  0,  0,  0,  0,  0,  0,
        50, 50, 50, 50, 50, 50, 50, 50,
        10, 10, 20, 30, 30, 20, 10, 10,
        5,  5, 10, 25, 25, 10,  5,  5,
        0,  0,  0, 20, 20,  0,  0,  0,
        5, -5,-10,  0,  0,-10, -5,  5,
        5, 10, 10,-20,-20, 10, 10,  5,
        0,  0,  0,  0,  0,  0,  0,  0
    };
    
    static const int knightTable[64] = {
        -50,-40,-30,-30,-30,-30,-40,-50,
        -40,-20,  0,  0,  0,  0,-20,-40,
        -30,  0, 10, 15, 15, 10,  0,-30,
        -30,  5, 15, 20, 20, 15,  5,-30,
        -30,  0, 15, 20, 20, 15,  0,-30,
        -30,  5, 10, 15, 15, 10,  5,-30,
        -40,-20,  0,  5,  5,  0,-20,-40,
        -50,-40,-30,-30,-30,-30,-40,-50
    };
    
    static const int bishopTable[64] = {
        -20,-10,-10,-10,-10,-10,-10,-20,
        -10,  0,  0,  0,  0,  0,  0,-10,
        -10,  0, 10, 10, 10, 10,  0,-10,
        -10,  5,  5, 10, 10,  5,  5,-10,
        -10,  0,  5, 10, 10,  5,  0,-10,
        -10,  5,  5,  5,  5,  5,  5,-10,
        -10,  0,  5,  0,  0,  5,  0,-10,
        -20,-10,-10,-10,-10,-10,-10,-20
    };
    
    static const int rookTable[64] = {
         0,  0,  0,  0,  0,  0,  0,  0,
         5, 10, 10, 10, 10, 10, 10,  5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
         0,  0,  0,  5,  5,  0,  0,  0
    };
    
    static const int queenTable[64] = {
        -20,-10,-10, -5, -5,-10,-10,-20,
        -10,  0,  0,  0,  0,  0,  0,-10,
        -10,  0,  5,  5,  5,  5,  0,-10,
         -5,  0,  5,  5,  5,  5,  0, -5,
          0,  0,  5,  5,  5,  5,  0, -5,
        -10,  5,  5,  5,  5,  5,  0,-10,
        -10,  0,  5,  0,  0,  0,  0,-10,
        -20,-10,-10, -5, -5,-10,-10,-20
    };
    
    static const int kingMiddlegameTable[64] = {
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -20,-30,-30,-40,-40,-30,-30,-20,
        -10,-20,-20,-20,-20,-20,-20,-10,
         20, 20,  0,  0,  0,  0, 20, 20,
         20, 30, 10,  0,  0, 10, 30, 20
    };
    
    // Count material and add positional bonuses
    for (size_t i = 0; i < boardState.size(); i++) {
        char piece = boardState[i];
        int row = i / BOARD_SIZE;
        int col = i % BOARD_SIZE;
        int pos = row * BOARD_SIZE + col;
        
        switch (piece) {
            case 'P': 
                score += 100; 
                score += pawnTable[pos];
                break;
            case 'p': 
                score -= 100; 
                score -= pawnTable[63 - pos]; // Flip for black
                break;
            case 'N': 
                score += 320;
                score += knightTable[pos];
                break;
            case 'n': 
                score -= 320;
                score -= knightTable[63 - pos];
                break;
            case 'B': 
                score += 330;
                score += bishopTable[pos];
                break;
            case 'b': 
                score -= 330;
                score -= bishopTable[63 - pos];
                break;
            case 'R': 
                score += 500;
                score += rookTable[pos];
                break;
            case 'r': 
                score -= 500;
                score -= rookTable[63 - pos];
                break;
            case 'Q': 
                score += 900;
                score += queenTable[pos];
                break;
            case 'q': 
                score -= 900;
                score -= queenTable[63 - pos];
                break;
            case 'K': 
                score += 20000;
                score += kingMiddlegameTable[pos];
                break;
            case 'k': 
                score -= 20000;
                score -= kingMiddlegameTable[63 - pos];
                break;
        }
    }
    
    // Bonus for checking the opponent's king
    if (IsKingInCheck(boardState, false)) {
        score += 50; // Bonus for checking the black king
    }
    if (IsKingInCheck(boardState, true)) {
        score -= 50; // Penalty for white king being in check
    }
    
    return score;
}

extern "C" __declspec(dllexport) const char* GetBestMove(const char* boardState, int depth, bool isWhite)
{
    std::string board(boardState);

    // Now we use the isWhite parameter passed from Unity
    std::vector<Move> moves = GenerateMoves(board, isWhite);
    std::string bestMove;
    int bestValue = (std::numeric_limits<int>::min)();

    for (const Move& move : moves) {
        // Apply move to get the new board state
        std::string newBoardState = ApplyMove(board, move);
        int moveValue = Minimax(newBoardState, depth - 1, (std::numeric_limits<int>::min)(),
                               (std::numeric_limits<int>::max)(), !isWhite);

        if (moveValue > bestValue) {
            bestValue = moveValue;
            bestMove = move.notation;
        }
    }

    static std::string result;
    result = bestMove;
    return result.c_str();
}