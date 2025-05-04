#include "pch.h"
#include "ChessEngine.h"
#include <string>
#include <vector>
#include <limits>
#include <algorithm>
#include <chrono>

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
    bool isEnPassant = false;
    int enPassantCapturePos = -1;
};

// A simple transposition table entry
struct TTEntry {
    uint64_t zobristKey;     // Unique position identifier
    int depth;               // Search depth
    int flag;                // Type of node (exact, upper bound, lower bound)
    int score;               // Position evaluation
    Move bestMove;           // Best move from this position
};

// Tree node structure for move generation
struct MoveTreeNode {
    Move move;                          // The move that led to this position
    std::string boardState;             // Current board state at this node
    int evaluation = 0;                 // Evaluation score for this position
    bool isEvaluated = false;           // Whether this node has been evaluated
    std::vector<MoveTreeNode*> children; // Child nodes (possible responses)
    MoveTreeNode* parent = nullptr;     // Parent node

    // Constructor for root node (no move yet)
    MoveTreeNode(const std::string& state) :
        boardState(state) {
    }

    // Constructor for child nodes
    MoveTreeNode(const std::string& state, const Move& m, MoveTreeNode* p) :
        boardState(state), move(m), parent(p) {
    }

    // Destructor to clean up memory
    ~MoveTreeNode() {
        for (MoveTreeNode* child : children) {
            delete child;
        }
    }
};

// Constants for the transposition table flags
const int TT_EXACT = 0;      // Exact score
const int TT_ALPHA = 1;      // Upper bound
const int TT_BETA = 2;       // Lower bound

// Track the last two killer moves at each depth
const int MAX_PLY = 64;  // Maximum search depth
Move killerMoves[MAX_PLY][2] = {};  // Initialize to empty moves

void StoreKillerMove(const Move& move, int ply);
bool IsKiller(const Move& move, int ply);
uint64_t GetZobristKey(const std::string& boardState);
void StoreTranspositionTable(const std::string& boardState, int depth,
	int flag, int score, const Move& bestMove);
bool ProbeTranspositionTable(const std::string& boardState, int depth,
	int& alpha, int& beta, int& score, Move& bestMove);
MoveTreeNode* BuildMoveTree(const std::string& boardState, int depth, bool isWhiteTurn);
void ExpandNode(MoveTreeNode* node, int depth, bool isWhiteTurn);
void OrderMoves(std::vector<Move>& moves, int ply, const std::string& boardState, const Move& ttMove = Move());
int GetPieceValue(char piece);
int Quiescence(const std::string& boardState, int alpha, int beta, bool maximizingPlayer, int maxDepth = 4);
int MinimaxOnTree(MoveTreeNode* node, int depth, int alpha, int beta, bool maximizingPlayer, bool allowNullMove = true);
bool IsCapture(const std::string& boardState, const Move& move);
bool IsCheck(const std::string& boardState, const Move& move);
bool IsDraw(const std::string& boardState);
std::string ApplyMove(const std::string& boardState, const Move& move);
bool IsKingInCheck(const std::string& boardState, bool isWhiteKing);
std::vector<Move> GenerateMoves(const std::string& boardState, bool isWhite);
int Minimax(std::string boardState, int depth, int alpha, int beta, bool maximizingPlayer);
bool IsValidMove(const std::string& boardState, int row, int col, bool isWhite);
void AddMove(const std::string& boardState, int startPos, int endPos, char piece,
    std::vector<Move>& moves);
void GeneratePawnMoves(const std::string& boardState, int row, int col, int pos,
    bool isWhite, std::vector<Move>& moves, int enPassantCol = -1);
void GenerateKnightMoves(const std::string& boardState, int row, int col, int pos,
    bool isWhite, std::vector<Move>& moves);
void GenerateBishopMoves(const std::string& boardState, int row, int col, int pos,
    bool isWhite, std::vector<Move>& moves);
void GenerateRookMoves(const std::string& boardState, int row, int col, int pos,
    bool isWhite, std::vector<Move>& moves);
void GenerateKingMoves(const std::string& boardState, int row, int col, int pos,
    bool isWhite, std::vector<Move>& moves);
int EvaluateBoard(const std::string& boardState);


// Store a killer move
void StoreKillerMove(const Move& move, int ply) {
    if (!(killerMoves[ply][0].notation == move.notation)) {
        killerMoves[ply][1] = killerMoves[ply][0];
        killerMoves[ply][0] = move;
    }
}

// Check if a move is a killer move
bool IsKiller(const Move& move, int ply) {
    return (killerMoves[ply][0].notation == move.notation) || 
           (killerMoves[ply][1].notation == move.notation);
}

// Simple hash function for generating Zobrist keys
uint64_t GetZobristKey(const std::string& boardState) {
    // In a real implementation, you'd use precomputed random values
    uint64_t key = 0;
    for (size_t i = 0; i < boardState.size(); i++) {
        if (boardState[i] != ' ') {
            key ^= ((uint64_t)boardState[i] << (i % 8)) + i;
        }
    }
    return key;
}

// The transposition table itself - sized to a power of 2 for efficient indexing
const int TT_SIZE = 1 << 20; // 1 million entries
std::vector<TTEntry> transpositionTable(TT_SIZE);

// Store a position in the transposition table
void StoreTranspositionTable(const std::string& boardState, int depth, 
                           int flag, int score, const Move& bestMove) {
    uint64_t key = GetZobristKey(boardState);
    int index = key % TT_SIZE;
    
    // Always replace strategy (could be enhanced with depth or other criteria)
    transpositionTable[index] = {key, depth, flag, score, bestMove};
}

// Probe the transposition table
bool ProbeTranspositionTable(const std::string& boardState, int depth, 
                           int& alpha, int& beta, int& score, Move& bestMove) {
    uint64_t key = GetZobristKey(boardState);
    int index = key % TT_SIZE;
    
    // Check if we have this position stored
    if (transpositionTable[index].zobristKey == key) {
        TTEntry& entry = transpositionTable[index];
        
        // Only use entries with sufficient depth
        if (entry.depth >= depth) {
            // Always use the move, even if we don't use the score
            bestMove = entry.bestMove;
            
            // Determine how to use the score based on the node type
            if (entry.flag == TT_EXACT) {
                score = entry.score;
                return true;
            } else if (entry.flag == TT_ALPHA && entry.score <= alpha) {
                score = alpha;
                return true;
            } else if (entry.flag == TT_BETA && entry.score >= beta) {
                score = beta;
                return true;
            }
        }
    }
    return false;
}



// Build a move tree to a specified depth
MoveTreeNode* BuildMoveTree(const std::string& boardState, int depth, bool isWhiteTurn) {
    // Create root node with current board state
    MoveTreeNode* root = new MoveTreeNode(boardState);
    
    // Base case: if depth is 0, return just the root
    if (depth <= 0) {
        return root;
    }
    
    // Generate all possible moves from this position
    std::vector<Move> possibleMoves = GenerateMoves(boardState, isWhiteTurn);
    
    // Create child nodes for each move
    for (const Move& move : possibleMoves) {
        // Apply the move to get new board state
        std::string newBoardState = ApplyMove(boardState, move);
        
        // Create child node
        MoveTreeNode* childNode = new MoveTreeNode(newBoardState, move, root);
        
        // Recursively build the tree for this move with reduced depth
        if (depth > 1) {
            // Generate responses (opponent's moves)
            std::vector<Move> responses = GenerateMoves(newBoardState, !isWhiteTurn);
            
            for (const Move& response : responses) {
                std::string responseState = ApplyMove(newBoardState, response);
                MoveTreeNode* grandchildNode = new MoveTreeNode(responseState, response, childNode);
                
                // Continue building the tree recursively
                if (depth > 2) {
                    ExpandNode(grandchildNode, depth - 2, isWhiteTurn);
                }
                
                childNode->children.push_back(grandchildNode);
            }
        }
        
        // Add the child to the parent
        root->children.push_back(childNode);
    }
    
    return root;
}

// Expand a node to create its children up to a certain depth
void ExpandNode(MoveTreeNode* node, int depth, bool isWhiteTurn) {
    if (depth <= 0) return;
    
    // Generate all possible moves from this position
    std::vector<Move> possibleMoves = GenerateMoves(node->boardState, isWhiteTurn);
    
    // Create child nodes for each move
    for (const Move& move : possibleMoves) {
        std::string newBoardState = ApplyMove(node->boardState, move);
        MoveTreeNode* childNode = new MoveTreeNode(newBoardState, move, node);
        
        // Recursively expand this node with reduced depth
        if (depth > 1) {
            ExpandNode(childNode, depth - 1, !isWhiteTurn);
        }
        
        node->children.push_back(childNode);
    }
}

// Helper function to sort nodes by evaluation (for move ordering)
void OrderMoves(std::vector<Move>& moves, int ply, const std::string& boardState, const Move& ttMove) {
    // Score each move for sorting
    std::vector<std::pair<int, Move>> scoredMoves;
    
    for (const Move& move : moves) {
        int score = 0;
        
        // 1. Transposition table move
        if (!ttMove.notation.empty() && move.notation == ttMove.notation) {
            score = 20000;
        }
        // 2. Captures (MVV-LVA: Most Valuable Victim - Least Valuable Aggressor)
        else if (!move.notation.empty() && move.notation.length() >= 3) {
            int endPos = std::stoi(move.notation.substr(move.notation.length() - 2));
            char victim = boardState[endPos];
            char aggressor = move.notation[0];
            
            if (victim != ' ') {
                score = 10000 + GetPieceValue(victim) * 10 - GetPieceValue(aggressor);
            }
        }
        // 3. Killer moves
        else if (IsKiller(move, ply)) {
            score = 9000;
        }
        // 4. Other moves
        else {
            score = 0;
        }
        
        scoredMoves.push_back({score, move});
    }
    
    // Sort moves by score (descending)
    std::sort(scoredMoves.begin(), scoredMoves.end(), 
        [](const auto& a, const auto& b) {
            return a.first > b.first;
        });
    
    // Extract sorted moves
    moves.clear();
    for (const auto& [score, move] : scoredMoves) {
        moves.push_back(move);
    }
}

int GetPieceValue(char piece) {
    switch (std::tolower(piece)) {
        case 'p': return 1;
        case 'n': return 3;
        case 'b': return 3;
        case 'r': return 5;
        case 'q': return 9;
        case 'k': return 0;  // Kings are not captured
        default: return 0;
    }
}

int Quiescence(const std::string& boardState, int alpha, int beta, bool maximizingPlayer, int maxDepth) {
    // Base evaluation
    int standPat = EvaluateBoard(boardState);
    
    // Return immediately if we've reached maximum quiescence depth
    if (maxDepth <= 0) {
        return standPat;
    }
    
    // Beta cutoff
    if (standPat >= beta) {
        return beta;
    }
    
    // Update alpha
    if (alpha < standPat) {
        alpha = standPat;
    }
    
    // Generate only capture moves
    std::vector<Move> captureMoves;
    std::vector<Move> allMoves = GenerateMoves(boardState, maximizingPlayer);
    
    // Filter to only keep captures
    for (const Move& move : allMoves) {
        int endPos = std::stoi(move.notation.substr(move.notation.length() - 2));
        if (boardState[endPos] != ' ' || move.isEnPassant) {
            captureMoves.push_back(move);
        }
    }
    
    // If no captures are available, return the stand pat score
    if (captureMoves.empty()) {
        return standPat;
    }
    
    // Recursively search capture sequences
    if (maximizingPlayer) {
        for (const Move& move : captureMoves) {
            std::string newBoardState = ApplyMove(boardState, move);
            int score = Quiescence(newBoardState, alpha, beta, false, maxDepth - 1);
            if (score >= beta) {
                return beta;
            }
            if (score > alpha) {
                alpha = score;
            }
        }
        return alpha;
    } else {
        for (const Move& move : captureMoves) {
            std::string newBoardState = ApplyMove(boardState, move);
            int score = Quiescence(newBoardState, alpha, beta, true, maxDepth - 1);
            if (score <= alpha) {
                return alpha;
            }
            if (score < beta) {
                beta = score;
            }
        }
        return beta;
    }
}

// Enhanced MinimaxOnTree with move ordering and quiescence search
// Enhanced MinimaxOnTree with null move pruning
int MinimaxOnTree(MoveTreeNode* node, int depth, int alpha, int beta, bool maximizingPlayer, bool allowNullMove) {
    // If node is already evaluated, return its score
    if (node->isEvaluated) {
        return node->evaluation;
    }
    
    // Try the transposition table first
    Move ttMove;
    int ttScore;
    
    if (ProbeTranspositionTable(node->boardState, depth, alpha, beta, ttScore, ttMove)) {
        return ttScore;
    }
    
    // If at max depth, use quiescence search
    if (depth == 0) {
        node->evaluation = Quiescence(node->boardState, alpha, beta, maximizingPlayer);
        node->isEvaluated = true;
        
        // Store in transposition table
        int flag = (node->evaluation <= alpha) ? TT_ALPHA : 
                  ((node->evaluation >= beta) ? TT_BETA : TT_EXACT);
        StoreTranspositionTable(node->boardState, depth, flag, node->evaluation, Move());
        
        return node->evaluation;
    }
    
    // Null move pruning
    if (allowNullMove && depth >= 3 && !IsKingInCheck(node->boardState, maximizingPlayer)) {
        int R = 2 + depth / 6; // Adaptive reduction
        // Skip a move and see if position is still good
        int nullMoveScore = -MinimaxOnTree(node, depth - 1 - R, -beta, -beta + 1, !maximizingPlayer, false);
        
        if (nullMoveScore >= beta) {
            // Verify with a shallower search to avoid zugzwang problems
            if (depth >= 5) {
                int verificationScore = MinimaxOnTree(node, depth - 4, alpha, beta, maximizingPlayer, false);
                if (verificationScore >= beta)
                    return beta;
            } else {
                return beta; // Position is good enough to cause a cutoff
            }
        }
    }
    
    // If leaf node, expand it
    if (node->children.empty()) {
        ExpandNode(node, 1, maximizingPlayer);
        if (node->children.empty()) {
            // If still no children after expansion, evaluate the position
            // This could be checkmate or stalemate
            bool isInCheck = IsKingInCheck(node->boardState, maximizingPlayer);
            if (isInCheck) {
                // Checkmate (worst score, adjusted by depth to find faster mates)
                node->evaluation = maximizingPlayer ? -100000 + depth * 100 : 100000 - depth * 100;
            } else {
                // Stalemate is a draw
                node->evaluation = 0;
            }
            node->isEvaluated = true;
            return node->evaluation;
        }
    }
    
    // Order moves to improve alpha-beta pruning efficiency
    std::vector<Move> moves;
    for (const auto& child : node->children) {
        moves.push_back(child->move);
    }
    OrderMoves(moves, depth, node->boardState, ttMove);
    
    // Best move so far
    Move bestMove;
    int nodeFlag = TT_ALPHA;
    
    // Apply minimax with alpha-beta pruning
    if (maximizingPlayer) {
        int maxEval = -2147483647;
        
        for (int i = 0; i < node->children.size(); i++) {
            MoveTreeNode* childNode = node->children[i];
            int eval;
            
            // Late Move Reduction (LMR)
            if (i >= 3 && depth >= 3 && !IsCapture(node->boardState, childNode->move) && !IsCheck(node->boardState, childNode->move)) {
                // Reduced depth search for likely poor moves
                int R = 1 + customMin(depth / 3, 3) + customMin(i / 6, 2);
                eval = -MinimaxOnTree(childNode, depth - 1 - R, -alpha - 1, -alpha, false);
                
                // If reduced search looks promising, do a full search
                if (eval > alpha && eval < beta) {
                    eval = -MinimaxOnTree(childNode, depth - 1, -beta, -alpha, false);
                }
            } else {
                // Full-depth search
                eval = -MinimaxOnTree(childNode, depth - 1, -beta, -alpha, false);
            }
            
            maxEval = customMax(maxEval, eval);
            
            if (maxEval > alpha) {
                alpha = maxEval;
                bestMove = childNode->move;
                nodeFlag = TT_EXACT;
                
                // Store killer move if it's a good quiet move
                if (!IsCapture(node->boardState, childNode->move)) {
                    StoreKillerMove(childNode->move, depth);
                }
            }
            
            if (beta <= alpha) {
                nodeFlag = TT_BETA;
                break; // Beta cut-off
            }
        }
        
        node->evaluation = maxEval;
        node->isEvaluated = true;
        
        // Store position in transposition table
        StoreTranspositionTable(node->boardState, depth, nodeFlag, maxEval, bestMove);
        
        return maxEval;
    } else {
        int minEval = 2147483647;
        
        for (int i = 0; i < node->children.size(); i++) {
            MoveTreeNode* childNode = node->children[i];
            int eval;
            
            // Late Move Reduction (LMR)
            if (i >= 3 && depth >= 3 && !IsCapture(node->boardState, childNode->move) && !IsCheck(node->boardState, childNode->move)) {
                // Reduced depth search for likely poor moves
                int R = 1 + customMin(depth / 3, 3) + customMin(i / 6, 2);
                eval = -MinimaxOnTree(childNode, depth - 1 - R, -beta, -alpha, true);
                
                // If reduced search looks promising, do a full search
                if (eval > alpha && eval < beta) {
                    eval = -MinimaxOnTree(childNode, depth - 1, -beta, -alpha, true);
                }
            } else {
                // Full-depth search
                eval = -MinimaxOnTree(childNode, depth - 1, -beta, -alpha, true);
            }
            
            minEval = customMin(minEval, eval);
            
            if (minEval < beta) {
                beta = minEval;
                bestMove = childNode->move;
                nodeFlag = TT_EXACT;
                
                // Store killer move if it's a good quiet move
                if (!IsCapture(node->boardState, childNode->move)) {
                    StoreKillerMove(childNode->move, depth);
                }
            }
            
            if (beta <= alpha) {
                nodeFlag = TT_ALPHA;
                break; // Alpha cut-off
            }
        }
        
        node->evaluation = minEval;
        node->isEvaluated = true;
        
        // Store position in transposition table
        StoreTranspositionTable(node->boardState, depth, nodeFlag, minEval, bestMove);
        
        return minEval;
    }
}

// Check if a move is a capture
bool IsCapture(const std::string& boardState, const Move& move) {
    if (move.notation.empty() || move.notation.length() < 3) 
        return false;
        
    int endPos = std::stoi(move.notation.substr(move.notation.length() - 2));
    return boardState[endPos] != ' ' || move.isEnPassant;
}

// Check if a move gives check
bool IsCheck(const std::string& boardState, const Move& move) {
    // Apply the move to get the new board state
    std::string newState = ApplyMove(boardState, move);
    
    // Get the color of the piece that moved
    char piece = move.notation[0];
    bool isWhitePiece = isupper(piece);
    
    // Check if the opponent's king is in check after this move
    return IsKingInCheck(newState, !isWhitePiece);
}

// Draw detection
bool IsDraw(const std::string& boardState) {
    // Check for insufficient material
    int whitePieceCount = 0;
    int blackPieceCount = 0;
    bool whiteHasMinor = false;
    bool blackHasMinor = false;
    
    for (char piece : boardState) {
        switch (piece) {
            case 'P': case 'R': case 'Q':
                whitePieceCount++;
                break;
            case 'p': case 'r': case 'q':
                blackPieceCount++;
                break;
            case 'B': case 'N':
                whitePieceCount++;
                whiteHasMinor = true;
                break;
            case 'b': case 'n':
                blackPieceCount++;
                blackHasMinor = true;
                break;
        }
    }
    
    // King vs King
    if (whitePieceCount == 0 && blackPieceCount == 0)
        return true;
        
    // King + minor piece vs King
    if ((whitePieceCount == 1 && whiteHasMinor && blackPieceCount == 0) ||
        (blackPieceCount == 1 && blackHasMinor && whitePieceCount == 0))
        return true;
        
    // Would need position history to detect repetition and 50-move rule
    
    return false;
}

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
    
    // Handle en passant capture
    if (move.isEnPassant) {
        newBoardState[move.enPassantCapturePos] = ' ';  // Remove the captured pawn
    }
    
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
                        bool isWhite, std::vector<Move>& moves, int enPassantCol)   
{
    const int BOARD_SIZE = 8;
    char piece = isWhite ? 'P' : 'p';
    int direction = isWhite ? -1 : 1;
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

    // Regular Captures
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

    // En Passant Capture
    if ((isWhite && row == 3) || (!isWhite && row == 4)) {  // Fifth rank for white, fourth for black
        if (enPassantCol >= 0 && abs(col - enPassantCol) == 1) {
            int newPos = (row + direction) * BOARD_SIZE + enPassantCol;
            int capturePos = row * BOARD_SIZE + enPassantCol;

            Move enPassantMove;
            enPassantMove.notation = piece + std::to_string(pos) + std::to_string(newPos);
            enPassantMove.isEnPassant = true;
            enPassantMove.enPassantCapturePos = capturePos;
            moves.push_back(enPassantMove);
        }
    }
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



extern "C" __declspec(dllexport) const char* GetBestMove(const char* boardState, int maxDepth, bool isWhite)
{
    std::string board(boardState);
    Move bestMove;
    
    // Starting time for time management
    auto startTime = std::chrono::high_resolution_clock::now();
    const int MAX_SEARCH_TIME_MS = 5000; // 5 seconds max search time
    
    // Clear transposition table at the start of a new search
    for (auto& entry : transpositionTable) {
        entry = TTEntry();
    }
    
    // Iterative deepening - start from depth 1 and increase
    for (int currentDepth = 1; currentDepth <= maxDepth; currentDepth++) {
        // Create the move tree from the current board position
        MoveTreeNode* root = BuildMoveTree(board, 1, isWhite);
        
        // Find the best move using minimax on the tree at current depth
        int bestValue = -2147483647;
        Move currentBestMove;
        bool moveFound = false;

        // Only search root children (our moves)
        for (MoveTreeNode* childNode : root->children) {
            // Use a full-window search for all moves at the root
            int moveValue = -MinimaxOnTree(childNode, currentDepth - 1, -2147483647, -bestValue, !isWhite);
            
            if (moveValue > bestValue) {
                bestValue = moveValue;
                currentBestMove = childNode->move;
                moveFound = true;
            }
        }
        
        // Save the best move at this depth
        if (moveFound) {
            bestMove = currentBestMove;
            
            // If we found a forced checkmate, no need to search deeper
            if (bestValue > 90000 || bestValue < -90000) {
                break;
            }
        }
        
        // Clean up the tree
        delete root;
        
        // Check if we've spent too much time already
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();
        if (elapsedMs > MAX_SEARCH_TIME_MS) {
            break; // Stop iterative deepening if we're out of time
        }
    }

    static std::string result;
    result = bestMove.notation;
    return result.c_str();
}

// Print the move tree for debugging (optional)
void PrintMoveTree(MoveTreeNode* node, int depth = 0) {
    // Print indentation based on depth
    for (int i = 0; i < depth; i++) {
        std::cout << "  ";
    }
    
    // Print node information
    if (depth == 0) {
        std::cout << "Root: ";
    } else {
        std::cout << "Move: " << node->move.notation;
        if (node->isEvaluated) {
            std::cout << " (Eval: " << node->evaluation << ")";
        }
    }
    std::cout << std::endl;
    
    // Print children recursively
    for (MoveTreeNode* child : node->children) {
        PrintMoveTree(child, depth + 1);
    }
}

int main() {
    const char* boardState = "rnbqkbnrpppppppp                                PPPPPPPPRNBQKBNR";
    int maxDepth = 3;
    bool isWhite = true;

    const char* bestMove = GetBestMove(boardState, maxDepth, isWhite);
    std::cout << "Best Move: " << bestMove << std::endl;

    return 0;
}