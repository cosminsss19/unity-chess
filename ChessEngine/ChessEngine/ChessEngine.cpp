#include "pch.h"
#include "ChessEngine.h"
#include <string>
#include <vector>
#include <limits>
#include <sstream>
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

struct BoardPosition {
    std::string boardState; // The 64-character board representation
    bool whiteCanCastleKingside;
    bool whiteCanCastleQueenside;
    bool blackCanCastleKingside;
    bool blackCanCastleQueenside;
    int enPassantTargetSquare; // -1 if none
    int halfMoveClock; // For 50-move rule
    int fullMoveNumber;
    bool whiteToMove;
};

// Constants for the transposition table flags
const int TT_EXACT = 0;      // Exact score
const int TT_ALPHA = 1;      // Upper bound
const int TT_BETA = 2;       // Lower bound

// Track the last two killer moves at each depth
const int MAX_PLY = 64;  // Maximum search depth
Move killerMoves[MAX_PLY][2] = {};  // Initialize to empty moves

// ---------------------------- Start of Function declarations ---------------------------- \\
void StoreKillerMove(const Move& move, int ply);
bool IsKiller(const Move& move, int ply);
uint64_t GetZobristKey(const std::string& boardState);
void StoreTranspositionTable(const std::string& boardState, int depth,
	int flag, int score, const Move& bestMove);
bool ProbeTranspositionTable(const std::string& boardState, int depth,
	int& alpha, int& beta, int& score, Move& bestMove);
MoveTreeNode* BuildMoveTree(const BoardPosition& position, int depth, bool isWhiteTurn);
void ExpandNode(MoveTreeNode* node, int depth, bool isWhiteTurn, const BoardPosition& position);
void OrderMoves(std::vector<Move>& moves, int ply, const std::string& boardState, const Move& ttMove = Move());
int GetPieceValue(char piece);
int Quiescence(const BoardPosition& position, int alpha, int beta, bool maximizingPlayer, int maxDepth);
int MinimaxOnTree(MoveTreeNode* node, int depth, int alpha, int beta, bool maximizingPlayer, bool allowNullMove = true);
bool IsCapture(const std::string& boardState, const Move& move);
bool IsCheck(const BoardPosition& position, const Move& move);
bool IsDraw(const std::string& boardState);
BoardPosition ApplyMove(const BoardPosition& position, const Move& move);
bool IsKingInCheck(const BoardPosition& position, bool isWhiteKing);
std::vector<Move> GenerateMoves(const BoardPosition& position, bool isWhite, bool skipCastlingCheck = false);
int Minimax(const BoardPosition& position, int depth, int alpha, int beta, bool maximizingPlayer);
bool IsValidMove(const std::string& boardState, int row, int col, bool isWhite);
void AddMove(const std::string& boardState, int startPos, int endPos, char piece,
    std::vector<Move>& moves);
void GeneratePawnMoves(const std::string& boardState, int row, int col, int pos,
    bool isWhite, std::vector<Move>& moves, int enPassantCol = -1);
void GenerateKnightMoves(const std::string& boardState, int row, int col, int pos,
    bool isWhite, std::vector<Move>& moves);
void GenerateBishopMoves(const std::string& boardState, int row, int col, int pos,
    bool isWhite, std::vector<Move>& moves, char actualPiece = '\0');
void GenerateRookMoves(const std::string& boardState, int row, int col, int pos,
    bool isWhite, std::vector<Move>& moves, char actualPiece = '\0');
void GenerateKingMoves(const std::string& boardState, int row, int col, int pos,
    bool isWhite, std::vector<Move>& moves, const BoardPosition& position, bool skipCastlingCheck = false);
int EvaluateBoard(const BoardPosition& position);
BoardPosition ParseMoveHistory(const std::string& moveHistory);
BoardPosition ApplyAlgebraicMove(const BoardPosition& position, const std::string& algebraicMove);
int AlgebraicToIndex(const std::string& algebraic);
std::string IndexToAlgebraic(int index);
Move AlgebraicToInternalMove(const std::string& algebraicMove, const BoardPosition& position);
void PrintBoard(const std::string& boardState);
int EvaluateOpeningPrinciples(const BoardPosition& position);
// ---------------------------- End of Function declarations ---------------------------- \\

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
MoveTreeNode* BuildMoveTree(const BoardPosition& position, int depth, bool isWhiteTurn) {
    // Create root node with current board state
    MoveTreeNode* root = new MoveTreeNode(position.boardState);

    // Base case: if depth is 0, return just the root
    if (depth <= 0) {
        return root;
    }

    // Generate all possible moves from this position
    std::vector<Move> possibleMoves = GenerateMoves(position, isWhiteTurn);

    // Create child nodes for each move
    for (const Move& move : possibleMoves) {
        // Apply the move to get new position (updates all state)
        BoardPosition newPosition = ApplyMove(position, move);

        // Create a child node for this move
        MoveTreeNode* childNode = new MoveTreeNode(newPosition.boardState, move, root);

        // Recursively build the tree for this move with reduced depth
        if (depth > 1) {
            MoveTreeNode* responseTree = BuildMoveTree(newPosition, depth - 1, !isWhiteTurn);
            // Attach all children of responseTree to childNode
            for (MoveTreeNode* grandchild : responseTree->children) {
                childNode->children.push_back(grandchild);
            }
            // Clean up the temporary responseTree node itself (not its children)
            delete responseTree;
        }

        // Add the child to the parent
        root->children.push_back(childNode);
    }

    return root;
}

// Expand a node to create its children up to a certain depth
void ExpandNode(MoveTreeNode* node, int depth, bool isWhiteTurn, const BoardPosition& position) {
    if (depth <= 0) return;

    // Generate all possible moves from this position
    std::vector<Move> possibleMoves = GenerateMoves(position, isWhiteTurn);

    // Create child nodes for each move
    for (const Move& move : possibleMoves) {
        // Apply the move to get new position (update all state)
        BoardPosition newPosition = position;
        newPosition = ApplyMove(position, move);
        // TODO: update castling rights, en passant, halfmove clock, etc. in newPosition here!

        MoveTreeNode* childNode = new MoveTreeNode(newPosition.boardState, move, node);

        // Recursively expand this node with reduced depth
        if (depth > 1) {
            ExpandNode(childNode, depth - 1, !isWhiteTurn, newPosition);
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

int Quiescence(const BoardPosition& position, int alpha, int beta, bool maximizingPlayer, int maxDepth) {
    // Base evaluation
    int standPat = EvaluateBoard(position);

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
    std::vector<Move> allMoves = GenerateMoves(position, maximizingPlayer);

    // Filter to only keep captures
    for (const Move& move : allMoves) {
        int endPos = std::stoi(move.notation.substr(move.notation.length() - 2));
        char target = position.boardState[endPos];
        if (target != ' ' || move.isEnPassant) {
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
            BoardPosition newPosition = ApplyMove(position, move);
            int score = Quiescence(newPosition, alpha, beta, false, maxDepth - 1);
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
            BoardPosition newPosition = ApplyMove(position, move);
            int score = Quiescence(newPosition, alpha, beta, true, maxDepth - 1);
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
        // You need a full BoardPosition for quiescence
        BoardPosition tempPosition;
        tempPosition.boardState = node->boardState;
        // If you store more state in the node, set it here!
        node->evaluation = Quiescence(tempPosition, alpha, beta, maximizingPlayer, 8);
        node->isEvaluated = true;

        // Store in transposition table
        int flag = (node->evaluation <= alpha) ? TT_ALPHA :
                  ((node->evaluation >= beta) ? TT_BETA : TT_EXACT);
        StoreTranspositionTable(node->boardState, depth, flag, node->evaluation, Move());

        return node->evaluation;
    }

    // Null move pruning
    {
        BoardPosition tempPosition;
        tempPosition.boardState = node->boardState;
        // Set other fields if you store them in the node
        if (allowNullMove && depth >= 3 && !IsKingInCheck(tempPosition, maximizingPlayer)) {
            int R = 2 + depth / 6; // Adaptive reduction
            int nullMoveScore = -MinimaxOnTree(node, depth - 1 - R, -beta, -beta + 1, !maximizingPlayer, false);

            if (nullMoveScore >= beta) {
                if (depth >= 5) {
                    int verificationScore = MinimaxOnTree(node, depth - 4, alpha, beta, maximizingPlayer, false);
                    if (verificationScore >= beta)
                        return beta;
                } else {
                    return beta;
                }
            }
        }
    }

    // If leaf node, expand it
    if (node->children.empty()) {
        BoardPosition tempPosition;
        tempPosition.boardState = node->boardState;
        // Set other fields if you store them in the node
        ExpandNode(node, 1, maximizingPlayer, tempPosition);
        if (node->children.empty()) {
            // If still no children after expansion, evaluate the position
            bool isInCheck = IsKingInCheck(tempPosition, maximizingPlayer);
            if (isInCheck) {
                node->evaluation = maximizingPlayer ? -100000 + depth * 100 : 100000 - depth * 100;
            } else {
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

    Move bestMove;
    int nodeFlag = TT_ALPHA;

    if (maximizingPlayer) {
        int maxEval = -2147483647;

        for (int i = 0; i < node->children.size(); i++) {
            MoveTreeNode* childNode = node->children[i];

            // Reconstruct BoardPosition for the child
            BoardPosition tempPosition;
            tempPosition.boardState = childNode->boardState;
            // Set other fields if you store them in the node

            int eval;
            // Late Move Reduction (LMR)
            if (i >= 3 && depth >= 3 && !IsCapture(tempPosition.boardState, childNode->move) && !IsCheck(tempPosition, childNode->move)) {
                int R = 1 + customMin(depth / 3, 3) + customMin(i / 6, 2);
                eval = -MinimaxOnTree(childNode, depth - 1 - R, -alpha - 1, -alpha, false);

                if (eval > alpha && eval < beta) {
                    eval = -MinimaxOnTree(childNode, depth - 1, -beta, -alpha, false);
                }
            } else {
                eval = -MinimaxOnTree(childNode, depth - 1, -beta, -alpha, false);
            }

            maxEval = customMax(maxEval, eval);

            if (maxEval > alpha) {
                alpha = maxEval;
                bestMove = childNode->move;
                nodeFlag = TT_EXACT;

                if (!IsCapture(tempPosition.boardState, childNode->move)) {
                    StoreKillerMove(childNode->move, depth);
                }
            }

            if (beta <= alpha) {
                nodeFlag = TT_BETA;
                break;
            }
        }

        node->evaluation = maxEval;
        node->isEvaluated = true;
        StoreTranspositionTable(node->boardState, depth, nodeFlag, maxEval, bestMove);
        return maxEval;
    } else {
        int minEval = 2147483647;

        for (int i = 0; i < node->children.size(); i++) {
            MoveTreeNode* childNode = node->children[i];

            // Reconstruct BoardPosition for the child
            BoardPosition tempPosition;
            tempPosition.boardState = childNode->boardState;
            // Set other fields if you store them in the node

            int eval;
            if (i >= 3 && depth >= 3 && !IsCapture(tempPosition.boardState, childNode->move) && !IsCheck(tempPosition, childNode->move)) {
                int R = 1 + customMin(depth / 3, 3) + customMin(i / 6, 2);
                eval = -MinimaxOnTree(childNode, depth - 1 - R, -beta, -alpha, true);

                if (eval > alpha && eval < beta) {
                    eval = -MinimaxOnTree(childNode, depth - 1, -beta, -alpha, true);
                }
            } else {
                eval = -MinimaxOnTree(childNode, depth - 1, -beta, -alpha, true);
            }

            minEval = customMin(minEval, eval);

            if (minEval < beta) {
                beta = minEval;
                bestMove = childNode->move;
                nodeFlag = TT_EXACT;

                if (!IsCapture(tempPosition.boardState, childNode->move)) {
                    StoreKillerMove(childNode->move, depth);
                }
            }

            if (beta <= alpha) {
                nodeFlag = TT_ALPHA;
                break;
            }
        }

        node->evaluation = minEval;
        node->isEvaluated = true;
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
bool IsCheck(const BoardPosition& position, const Move& move) {
    // Apply the move to get the new position
    BoardPosition newPosition = ApplyMove(position, move);

    // Get the color of the piece that moved
    char piece = move.notation[0];
    bool isWhitePiece = isupper(piece);

    // Check if the opponent's king is in check after this move
    return IsKingInCheck(newPosition, !isWhitePiece);
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
int EvaluateBoard(const BoardPosition& position);

// Apply a move to the board and return the new board state
BoardPosition ApplyMove(const BoardPosition& position, const Move& move) {
    if (move.notation.length() < 5) {
        throw std::invalid_argument("Invalid move notation: '" + move.notation + "'");
    }
    BoardPosition newPosition = position;
    const int BOARD_SIZE = 8;

    // Parse move details
    char piece = move.notation[0];
    std::string startPosStr = move.notation.substr(1, 2);
    std::string endPosStr = move.notation.substr(3, 2);
    int startPos = std::stoi(startPosStr);
    int endPos = std::stoi(endPosStr);

    // Update the board
    newPosition.boardState[endPos] = newPosition.boardState[startPos];
    newPosition.boardState[startPos] = ' ';

    // Handle en passant
    if (move.isEnPassant) {
        newPosition.boardState[move.enPassantCapturePos] = ' ';
    }

    // Handle castling
    if (move.isCastling) {
        if (move.isKingsideCastling) {
            if (position.whiteToMove) {
                // Move rook for white kingside
                newPosition.boardState[63] = ' ';
                newPosition.boardState[61] = 'R';
            } else {
                // Move rook for black kingside
                newPosition.boardState[7] = ' ';
                newPosition.boardState[5] = 'r';
            }
        } else {
            if (position.whiteToMove) {
                // Move rook for white queenside
                newPosition.boardState[56] = ' ';
                newPosition.boardState[59] = 'R';
            } else {
                // Move rook for black queenside
                newPosition.boardState[0] = ' ';
                newPosition.boardState[3] = 'r';
            }
        }
    }

    // Update castling rights if king or rook moves/captures
    if (piece == 'K') {
        newPosition.whiteCanCastleKingside = false;
        newPosition.whiteCanCastleQueenside = false;
    } else if (piece == 'k') {
        newPosition.blackCanCastleKingside = false;
        newPosition.blackCanCastleQueenside = false;
    }
    // If a rook moves or is captured, update castling rights
    if (startPos == 0 || endPos == 0) newPosition.whiteCanCastleQueenside = false; // White queenside rook
    if (startPos == 7 || endPos == 7) newPosition.whiteCanCastleKingside = false;  // White kingside rook
    if (startPos == 56 || endPos == 56) newPosition.blackCanCastleQueenside = false; // Black queenside rook
    if (startPos == 63 || endPos == 63) newPosition.blackCanCastleKingside = false;  // Black kingside rook

    // Update en passant target square
    newPosition.enPassantTargetSquare = -1;
    if ((piece == 'P' && startPos / 8 == 6 && endPos / 8 == 4) ||
        (piece == 'p' && startPos / 8 == 1 && endPos / 8 == 3)) {
        // Pawn double move
        newPosition.enPassantTargetSquare = (startPos + endPos) / 2;
    }

    // Update halfmove clock
    if (tolower(piece) == 'p' || (position.boardState[endPos] != ' ')) {
        newPosition.halfMoveClock = 0;
    } else {
        newPosition.halfMoveClock++;
    }

    // Update fullmove number and side to move
    if (!position.whiteToMove) {
        newPosition.fullMoveNumber++;
    }
    newPosition.whiteToMove = !position.whiteToMove;

    // Handle promotion
    if (move.notation.length() > 5) {
        char promotionPiece = move.notation[5];
        // Only promote if the moved piece is a pawn and it reached the last rank
        if ((piece == 'P' && endPos / 8 == 0) || (piece == 'p' && endPos / 8 == 7)) {
            newPosition.boardState[endPos] = promotionPiece;
        }
    }

    return newPosition;
}

// Function to check if a king is in check
bool IsKingInCheck(const BoardPosition& position, bool isWhiteKing) {
    const std::string& boardState = position.boardState;
    char kingChar = isWhiteKing ? 'K' : 'k';

    // Find the king's position
    int kingPos = -1;
    for (size_t i = 0; i < boardState.size(); i++) {
        if (boardState[i] == kingChar) {
            kingPos = i;
            break;
        }
    }
    if (kingPos == -1) return false;

    // Generate all opponent's moves, but skip castling logic!
    std::vector<Move> opponentMoves = GenerateMoves(position, !isWhiteKing, /*skipCastlingCheck=*/true);

    for (const Move& move : opponentMoves) {
        int endPos = std::stoi(move.notation.substr(move.notation.length() - 2));
        if (endPos == kingPos) return true;
    }
    return false;
}

std::vector<Move> GenerateMoves(const BoardPosition& position, bool isWhite, bool skipCastlingCheck) {
    const std::string& boardState = position.boardState;
    std::vector<Move> moves;
    const int BOARD_SIZE = 8;
    for (int i = 0; i < boardState.size(); ++i) {
        char piece = boardState[i];
        if (piece == ' ' || (isWhite && islower(piece)) || (!isWhite && isupper(piece))) continue;
        int row = i / BOARD_SIZE;
        int col = i % BOARD_SIZE;
        if ((isWhite && piece == 'P') || (!isWhite && piece == 'p')) {
            GeneratePawnMoves(boardState, row, col, i, isWhite, moves, position.enPassantTargetSquare % 8);
        } else if ((isWhite && piece == 'N') || (!isWhite && piece == 'n')) {
            GenerateKnightMoves(boardState, row, col, i, isWhite, moves);
        } else if ((isWhite && piece == 'B') || (!isWhite && piece == 'b')) {
            GenerateBishopMoves(boardState, row, col, i, isWhite, moves, piece);
        } else if ((isWhite && piece == 'R') || (!isWhite && piece == 'r')) {
            GenerateRookMoves(boardState, row, col, i, isWhite, moves, piece);
        } else if ((isWhite && piece == 'Q') || (!isWhite && piece == 'q')) {
            GenerateBishopMoves(boardState, row, col, i, isWhite, moves, piece);
            GenerateRookMoves(boardState, row, col, i, isWhite, moves, piece);
        } else if ((isWhite && piece == 'K') || (!isWhite && piece == 'k')) {
            GenerateKingMoves(boardState, row, col, i, isWhite, moves, position, skipCastlingCheck);
        }
    }
    return moves;
}

int Minimax(const BoardPosition& position, int depth, int alpha, int beta, bool maximizingPlayer) {
    if (depth == 0) {
        return EvaluateBoard(position);
    }

    std::vector<Move> moves = GenerateMoves(position, maximizingPlayer);

    // No legal moves - could be stalemate or checkmate
    if (moves.empty()) {
        // Ideally would check if king is in check
        return maximizingPlayer ? -20000 : 20000; // Checkmate
    }

    if (maximizingPlayer) {
        int maxEval = -2147483647;
        for (const Move& move : moves) {
            BoardPosition newPosition = ApplyMove(position, move);
            int eval = Minimax(newPosition, depth - 1, alpha, beta, false);
            maxEval = customMax(maxEval, eval);
            alpha = customMax(alpha, eval);
            if (beta <= alpha) {
                break;
            }
        }
        return maxEval;
    } else {
        int minEval = 2147483647;
        for (const Move& move : moves) {
            BoardPosition newPosition = ApplyMove(position, move);
            int eval = Minimax(newPosition, depth - 1, alpha, beta, true);
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
    std::string startPosStr = (startPos < 10) ? "0" + std::to_string(startPos) : std::to_string(startPos);
    std::string endPosStr = (endPos < 10) ? "0" + std::to_string(endPos) : std::to_string(endPos);
    
    std::string notation = piece + startPosStr + endPosStr;

    moves.push_back({notation});
}

// Enhanced pawn move generation with promotion
void GeneratePawnMoves(const std::string& boardState, int row, int col, int pos,
    bool isWhite, std::vector<Move>& moves, int enPassantCol)
{
    const int BOARD_SIZE = 8;
    char piece = boardState[pos];
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

            // Check explicitly that target is an opponent's piece
            bool isOpponentPiece = (isWhite && islower(targetPiece)) || (!isWhite && isupper(targetPiece));
            if (targetPiece != ' ' && isOpponentPiece) {
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
                         bool isWhite, std::vector<Move>& moves, char actualPiece) {
    char piece = actualPiece ? actualPiece : (isWhite ? 'B' : 'b');
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
                       bool isWhite, std::vector<Move>& moves, char actualPiece) {
    char piece = actualPiece ? actualPiece : (isWhite ? 'R' : 'r');
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
                      bool isWhite, std::vector<Move>& moves, const BoardPosition& position, bool skipCastlingCheck) {
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

    // Helper to format position as two digits
    auto posStr = [](int p) { return (p < 10 ? "0" : "") + std::to_string(p); };

    // --- Castling legality checks ---
    if (!skipCastlingCheck && ((isWhite && row == 7 && col == 4) || (!isWhite && row == 0 && col == 4))) {
        int baseRow = isWhite ? 7 : 0;
        char rook = isWhite ? 'R' : 'r';

        // Kingside castling
        bool canCastleKingside = isWhite ? position.whiteCanCastleKingside : position.blackCanCastleKingside;
        if (canCastleKingside &&
            boardState[baseRow * 8 + 5] == ' ' &&
            boardState[baseRow * 8 + 6] == ' ' &&
            boardState[baseRow * 8 + 7] == rook) {
            // Check king not in check, and squares not attacked
            bool safe = true;
            for (int c = 4; c <= 6; ++c) {
                BoardPosition tempPosition = position;
                tempPosition.boardState[baseRow * 8 + 4] = ' ';
                tempPosition.boardState[baseRow * 8 + c] = piece;
                if (IsKingInCheck(tempPosition, isWhite)) {
                    safe = false;
                    break;
                }
                tempPosition.boardState[baseRow * 8 + c] = ' ';
                tempPosition.boardState[baseRow * 8 + 4] = piece;
            }
            if (safe) {
                Move castlingMove;
                castlingMove.notation = piece + posStr(pos) + posStr(baseRow * 8 + 6);
                castlingMove.isCastling = true;
                castlingMove.isKingsideCastling = true;
                moves.push_back(castlingMove);
            }
        }

        // Queenside castling
        bool canCastleQueenside = isWhite ? position.whiteCanCastleQueenside : position.blackCanCastleQueenside;
        if (canCastleQueenside &&
            boardState[baseRow * 8 + 1] == ' ' &&
            boardState[baseRow * 8 + 2] == ' ' &&
            boardState[baseRow * 8 + 3] == ' ' &&
            boardState[baseRow * 8 + 0] == rook) {
            // Check king not in check, and squares not attacked
            bool safe = true;
            for (int c = 2; c <= 4; ++c) {
                BoardPosition tempPosition = position;
                tempPosition.boardState[baseRow * 8 + 4] = ' ';
                tempPosition.boardState[baseRow * 8 + c] = piece;
                if (IsKingInCheck(tempPosition, isWhite)) {
                    safe = false;
                    break;
                }
                tempPosition.boardState[baseRow * 8 + c] = ' ';
                tempPosition.boardState[baseRow * 8 + 4] = piece;
            }
            if (safe) {
                Move castlingMove;
                castlingMove.notation = piece + posStr(pos) + posStr(baseRow * 8 + 2);
                castlingMove.isCastling = true;
                castlingMove.isKingsideCastling = false;
                moves.push_back(castlingMove);
            }
        }
    }
}

int EvaluateBoard(const BoardPosition& position) {
    const std::string& boardState = position.boardState;
    const int BOARD_SIZE = 8;
    int score = 0;
    
    // Piece values
    const int PAWN_VALUE = 100;
    const int KNIGHT_VALUE = 320;
    const int BISHOP_VALUE = 330;
    const int ROOK_VALUE = 500;
    const int QUEEN_VALUE = 900;
    const int KING_VALUE = 20000;

    // Count material and store piece positions
    int whitePawns[8] = {0}; // Count of pawns per file
    int blackPawns[8] = {0}; // Count of pawns per file
    bool whiteHasBishopPair = false;
    bool blackHasBishopPair = false;
    int whiteBishops = 0, blackBishops = 0;
    int whitePawnCount = 0, blackPawnCount = 0;
    int whiteRookCount = 0, blackRookCount = 0;
    int whiteKnightCount = 0, blackKnightCount = 0;
    int whiteQueenCount = 0, blackQueenCount = 0;
    int whiteKingPos = -1, blackKingPos = -1;
    
    // Phase of game detection (for different piece values in endgame)
    int totalMaterial = 0;
    
    // Store piece positions for further analysis
    std::vector<int> whitePawnPositions;
    std::vector<int> blackPawnPositions;
    std::vector<int> whiteRookPositions;
    std::vector<int> blackRookPositions;
    std::vector<int> whiteKnightPositions;
    std::vector<int> blackKnightPositions;
    std::vector<int> whiteBishopPositions;
    std::vector<int> blackBishopPositions;
    
    // ----------------- Basic Material Counting ----------------- //
    for (size_t i = 0; i < boardState.size(); i++) {
        char piece = boardState[i];
        int file = i % 8;
        int rank = i / 8;
        int pos = rank * BOARD_SIZE + file;
        
        // Count material and track piece positions
        switch (piece) {
            case 'P': 
                score += PAWN_VALUE; 
                whitePawns[file]++;
                whitePawnCount++;
                whitePawnPositions.push_back(pos);
                totalMaterial += PAWN_VALUE;
                break;
            case 'p': 
                score -= PAWN_VALUE; 
                blackPawns[file]++;
                blackPawnCount++;
                blackPawnPositions.push_back(pos);
                totalMaterial += PAWN_VALUE;
                break;
            case 'N': 
                score += KNIGHT_VALUE;
                whiteKnightCount++;
                whiteKnightPositions.push_back(pos);
                totalMaterial += KNIGHT_VALUE;
                break;
            case 'n': 
                score -= KNIGHT_VALUE;
                blackKnightCount++;
                blackKnightPositions.push_back(pos);
                totalMaterial += KNIGHT_VALUE;
                break;
            case 'B': 
                score += BISHOP_VALUE;
                whiteBishopPositions.push_back(pos);
                whiteBishops++;
                totalMaterial += BISHOP_VALUE;
                break;
            case 'b': 
                score -= BISHOP_VALUE;
                blackBishopPositions.push_back(pos);
                blackBishops++;
                totalMaterial += BISHOP_VALUE;
                break;
            case 'R': 
                score += ROOK_VALUE;
                whiteRookCount++;
                whiteRookPositions.push_back(pos);
                totalMaterial += ROOK_VALUE;
                break;
            case 'r': 
                score -= ROOK_VALUE;
                blackRookCount++;
                blackRookPositions.push_back(pos);
                totalMaterial += ROOK_VALUE;
                break;
            case 'Q': 
                score += QUEEN_VALUE;
                whiteQueenCount++;
                totalMaterial += QUEEN_VALUE;
                break;
            case 'q': 
                score -= QUEEN_VALUE;
                blackQueenCount++;
                totalMaterial += QUEEN_VALUE;
                break;
            case 'K': 
                score += KING_VALUE;
                whiteKingPos = pos;
                break;
            case 'k': 
                score -= KING_VALUE;
                blackKingPos = pos;
                break;
        }
    }
    
    // Game phase detection (0 = opening, 1 = middlegame, 2 = endgame)
    int gamePhase = 0;
    const int OPENING_THRESHOLD = 5000; // Full material minus some pieces
    const int MIDDLEGAME_THRESHOLD = 3000;
    
    if (totalMaterial <= MIDDLEGAME_THRESHOLD) {
        gamePhase = 2; // Endgame
    } else if (totalMaterial <= OPENING_THRESHOLD) {
        gamePhase = 1; // Middlegame
    }
    
    // FIXED: Special pattern recognition for early opening moves
    if (gamePhase == 0) {
        // Apply opening principles with extreme weight
        int openingScore = EvaluateOpeningPrinciples(position);
        
        // If a critical penalty is detected, let it dominate the evaluation
        if (position.whiteToMove && openingScore < -1000) {
            return openingScore; // Immediate penalty for white's bad move
        }
        else if (!position.whiteToMove && openingScore > 1000) {
            return openingScore; // Immediate penalty for black's bad move
        }
        
        // Otherwise, add to existing score
        score += openingScore;
    }
    
    // ----------------- Central Control Bonus (Opening) ----------------- //
    if (gamePhase == 0) {
        // Central control bonus - reward pieces and pawns in the center
        for (int i = 0; i < 64; i++) {
            int rank = i / 8;
            int file = i % 8;
            
            // Define central squares (e4, d4, e5, d5)
            bool isCentralSquare = (rank >= 3 && rank <= 4 && file >= 3 && file <= 4);
            
            // Define near-central squares
            bool isNearCentralSquare = (rank >= 2 && rank <= 5 && file >= 2 && file <= 5) && !isCentralSquare;
            
            char piece = boardState[i];
            if (piece != ' ') {
                bool isWhitePiece = isupper(piece);
                
                if (isCentralSquare) {
                    if (isWhitePiece) {
                        score += 25; // Central bonus for white pieces
                    } else {
                        score -= 25; // Central bonus for black pieces
                    }
                }
                else if (isNearCentralSquare) {
                    if (isWhitePiece) {
                        score += 10; // Near-center bonus for white pieces
                    } else {
                        score -= 10; // Near-center bonus for black pieces
                    }
                }
            }
        }
    }
    
    // ----------------- Positional Evaluation ----------------- //
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
    
    // King endgame table - encourages king activity in endgames
    static const int kingEndgameTable[64] = {
        -50,-40,-30,-20,-20,-30,-40,-50,
        -30,-20,-10,  0,  0,-10,-20,-30,
        -30,-10, 20, 30, 30, 20,-10,-30,
        -30,-10, 30, 40, 40, 30,-10,-30,
        -30,-10, 30, 40, 40, 30,-10,-30,
        -30,-10, 20, 30, 30, 20,-10,-30,
        -30,-30,  0,  0,  0,  0,-30,-30,
        -50,-30,-30,-30,-30,-30,-30,-50
    };
    
    // Apply piece-square tables
    for (int pawnPos : whitePawnPositions) {
        score += pawnTable[pawnPos];
    }
    for (int pawnPos : blackPawnPositions) {
        score -= pawnTable[63 - pawnPos]; // Flip for black
    }
    
    for (int knightPos : whiteKnightPositions) {
        score += knightTable[knightPos];
    }
    for (int knightPos : blackKnightPositions) {
        score -= knightTable[63 - knightPos]; // Flip for black
    }
    
    for (int bishopPos : whiteBishopPositions) {
        score += bishopTable[bishopPos];
    }
    for (int bishopPos : blackBishopPositions) {
        score -= bishopTable[63 - bishopPos]; // Flip for black
    }
    
    for (int rookPos : whiteRookPositions) {
        score += rookTable[rookPos];
    }
    for (int rookPos : blackRookPositions) {
        score -= rookTable[63 - rookPos]; // Flip for black
    }
    
    // Apply king tables based on game phase
    if (whiteKingPos >= 0) {
        if (gamePhase < 2) {
            score += kingMiddlegameTable[whiteKingPos];
        } else {
            score += kingEndgameTable[whiteKingPos] * 2;
        }
    }
    
    if (blackKingPos >= 0) {
        if (gamePhase < 2) {
            score -= kingMiddlegameTable[63 - blackKingPos]; // Flip for black
        } else {
            score -= kingEndgameTable[63 - blackKingPos] * 2;
        }
    }
    
    // ----------------- SEVERE Knight Edge Penalties ----------------- //
    for (int knightPos : whiteKnightPositions) {
        int file = knightPos % 8;
        int rank = knightPos / 8;
        
        // UPDATED: Severe penalty for knights on the rim (a & h files)
        if (file == 0 || file == 7) {
            score -= 50; // Increased penalty for edge knight
            if (rank == 2 || rank == 5) { // a3/a6 or h3/h6 are particularly bad
                score -= 30; // Additional severe penalty for worst squares
            }
        }
        
        // Calculate actual knight mobility
        int actualMobility = 0;
        const std::vector<std::pair<int, int>> knightMoves = {
            {-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
            {1, -2}, {1, 2}, {2, -1}, {2, 1}
        };
        
        for (const auto& move : knightMoves) {
            int newRow = rank + move.first;
            int newCol = file + move.second;
            
            if (newRow >= 0 && newRow < 8 && newCol >= 0 && newCol < 8) {
                char targetSquare = boardState[newRow * 8 + newCol];
                if (targetSquare == ' ' || islower(targetSquare)) {
                    actualMobility++;
                }
            }
        }
        
        score += actualMobility * 4; // Significant bonus for knight mobility
    }
    
    for (int knightPos : blackKnightPositions) {
        int file = knightPos % 8;
        int rank = knightPos / 8;
        
        // UPDATED: Severe penalty for knights on the rim
        if (file == 0 || file == 7) {
            score += 100; // INCREASED: Much stronger penalty for black's edge knight
            if (rank == 2 || rank == 5) { // a3/a6 or h3/h6 are particularly bad
                score += 50; // Additional severe penalty for worst squares
            }
        }
        
        // Calculate actual knight mobility
        int actualMobility = 0;
        const std::vector<std::pair<int, int>> knightMoves = {
            {-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
            {1, -2}, {1, 2}, {2, -1}, {2, 1}
        };
        
        for (const auto& move : knightMoves) {
            int newRow = rank + move.first;
            int newCol = file + move.second;
            
            if (newRow >= 0 && newRow < 8 && newCol >= 0 && newCol < 8) {
                char targetSquare = boardState[newRow * 8 + newCol];
                if (targetSquare == ' ' || isupper(targetSquare)) {
                    actualMobility++;
                }
            }
        }
        
        score -= actualMobility * 4; // Penalize black knight mobility
    }
    
    // ----------------- Bishop Pair Bonus ----------------- //
    if (whiteBishops >= 2) {
        score += 50; // Bonus for having the bishop pair
        whiteHasBishopPair = true;
    }
    if (blackBishops >= 2) {
        score -= 50; // Same bonus for black
        blackHasBishopPair = true;
    }
    
    // ----------------- Trapped Pieces Detection ----------------- //
    // Check for trapped bishops
    for (int bishopPos : whiteBishopPositions) {
        // Bishop trapped in corner with pawn blocking it
        if ((bishopPos == 56 || bishopPos == 63) && 
            (boardState[bishopPos - 8] == 'P' || boardState[bishopPos - 9] == 'P' || boardState[bishopPos - 7] == 'P')) {
            score -= 100; // Severe penalty
        }
    }
    
    for (int bishopPos : blackBishopPositions) {
        // Bishop trapped in corner with pawn blocking it
        if ((bishopPos == 0 || bishopPos == 7) && 
            (boardState[bishopPos + 8] == 'p' || boardState[bishopPos + 9] == 'p' || boardState[bishopPos + 7] == 'p')) {
            score += 100; // Severe penalty for black
        }
    }
    
    // ----------------- Knight Outposts ----------------- //
    for (int knightPos : whiteKnightPositions) {
        int rank = knightPos / 8;
        int file = knightPos % 8;
        
        // Only consider outposts in enemy territory (ranks 0-3)
        if (rank < 4) {
            bool isPawnProtected = false;
            if (file > 0 && rank < 7 && boardState[(rank+1)*8 + file-1] == 'P') {
                isPawnProtected = true;
            }
            if (file < 7 && rank < 7 && boardState[(rank+1)*8 + file+1] == 'P') {
                isPawnProtected = true;
            }
            
            bool isSafeFromPawns = true;
            if (file > 0 && rank > 0 && boardState[(rank-1)*8 + file-1] == 'p') {
                isSafeFromPawns = false;
            }
            if (file < 7 && rank > 0 && boardState[(rank-1)*8 + file+1] == 'p') {
                isSafeFromPawns = false;
            }
            
            if (isPawnProtected && isSafeFromPawns) {
                score += 25; // Bonus for knight outpost
            }
        }
    }
    
    for (int knightPos : blackKnightPositions) {
        int rank = knightPos / 8;
        int file = knightPos % 8;
        
        // Only consider outposts in enemy territory (ranks 4-7)
        if (rank > 3) {
            bool isPawnProtected = false;
            if (file > 0 && rank > 0 && boardState[(rank-1)*8 + file-1] == 'p') {
                isPawnProtected = true;
            }
            if (file < 7 && rank > 0 && boardState[(rank-1)*8 + file+1] == 'p') {
                isPawnProtected = true;
            }
            
            bool isSafeFromPawns = true;
            if (file > 0 && rank < 7 && boardState[(rank+1)*8 + file-1] == 'P') {
                isSafeFromPawns = false;
            }
            if (file < 7 && rank < 7 && boardState[(rank+1)*8 + file+1] == 'P') {
                isSafeFromPawns = false;
            }
            
            if (isPawnProtected && isSafeFromPawns) {
                score -= 25; // Bonus for black knight outpost
            }
        }
    }
    
    // ----------------- Pawn Structure Evaluation ----------------- //
    // Doubled pawns (same file)
    for (int file = 0; file < 8; file++) {
        if (whitePawns[file] > 1) {
            score -= 20 * (whitePawns[file] - 1); // Penalty for each doubled pawn
        }
        if (blackPawns[file] > 1) {
            score += 20 * (blackPawns[file] - 1); // Same penalty for black
        }
    }
    
    // Isolated and passed pawns
    for (int pawnPos : whitePawnPositions) {
        int file = pawnPos % 8;
        int rank = pawnPos / 8;
        
        // Check for isolated pawns (no friendly pawns on adjacent files)
        bool isIsolated = true;
        if (file > 0 && whitePawns[file-1] > 0) isIsolated = false;
        if (file < 7 && whitePawns[file+1] > 0) isIsolated = false;
        
        if (isIsolated) {
            score -= 15; // Penalty for isolated pawn
        }
        
        // Check for passed pawns (no enemy pawns ahead or on adjacent files)
        bool isPassed = true;
        for (int r = rank - 1; r >= 0; r--) {
            for (int f = customMax(0, file-1); f <= customMin(7, file+1); f++) {
                if (boardState[r*8+f] == 'p') {
                    isPassed = false;
                    break;
                }
            }
            if (!isPassed) break;
        }
        
        if (isPassed) {
            // Progressive bonus based on rank (more advanced = more valuable)
            int passedBonus = 20 + (7 - rank) * 10;
            if (gamePhase == 2) passedBonus *= 2; // Even more valuable in endgame
            score += passedBonus;
        }
        
        // Penalty for pawn shield advancement that weakens king safety
        if (gamePhase < 2 && whiteKingPos >= 0) {
            int kingFile = whiteKingPos % 8;
            if (abs(file - kingFile) <= 1 && rank < 6) {
                score -= (6 - rank) * 5; // Penalty for advancing shield pawns
            }
        }
    }
    
    // Do the same for black pawns
    for (int pawnPos : blackPawnPositions) {
        int file = pawnPos % 8;
        int rank = pawnPos / 8;
        
        // Check for isolated pawns
        bool isIsolated = true;
        if (file > 0 && blackPawns[file-1] > 0) isIsolated = false;
        if (file < 7 && blackPawns[file+1] > 0) isIsolated = false;
        
        if (isIsolated) {
            score += 15; // Penalty for black's isolated pawn
        }
        
        // Check for passed pawns
        bool isPassed = true;
        for (int r = rank + 1; r < 8; r++) {
            for (int f = customMax(0, file-1); f <= customMin(7, file+1); f++) {
                if (boardState[r*8+f] == 'P') {
                    isPassed = false;
                    break;
                }
            }
            if (!isPassed) break;
        }
        
        if (isPassed) {
            int passedBonus = 20 + rank * 10; // Progressive bonus based on rank
            if (gamePhase == 2) passedBonus *= 2;
            score -= passedBonus;
        }
        
        // Penalty for pawn shield advancement that weakens king safety
        if (gamePhase < 2 && blackKingPos >= 0) {
            int kingFile = blackKingPos % 8;
            if (abs(file - kingFile) <= 1 && rank > 1) {
                score += (rank - 1) * 5; // Penalty for advancing shield pawns
            }
        }
    }
    
    // ----------------- Rook Positioning ----------------- //
    // Rooks on open files (files with no pawns)
    for (int rookPos : whiteRookPositions) {
        int file = rookPos % 8;
        
        // Check if the file is completely open
        if (whitePawns[file] == 0 && blackPawns[file] == 0) {
            score += 25; // Bonus for rook on open file
        }
        // Check if the file is semi-open (no friendly pawns)
        else if (whitePawns[file] == 0) {
            score += 15; // Smaller bonus for semi-open file
        }
        
        // Bonus for rook on 7th rank (if enemy king is on 8th)
        if (rookPos / 8 == 1 && blackKingPos / 8 == 0) {
            score += 30;
        }
    }
    
    // Same for black rooks
    for (int rookPos : blackRookPositions) {
        int file = rookPos % 8;
        
        if (whitePawns[file] == 0 && blackPawns[file] == 0) {
            score -= 25;
        }
        else if (blackPawns[file] == 0) {
            score -= 15;
        }
        
        // Bonus for rook on 2nd rank (if enemy king is on 1st)
        if (rookPos / 8 == 6 && whiteKingPos / 8 == 7) {
            score -= 30;
        }
    }
    
    // ----------------- King Safety ----------------- //
    // King pawn shield (only in opening/middlegame)
    if (gamePhase < 2) {
        if (whiteKingPos >= 0) {
            int kingFile = whiteKingPos % 8;
            int kingRank = whiteKingPos / 8;
            
            // Check pawns in front of king
            int pawnShieldCount = 0;
            for (int f = customMax(0, kingFile-1); f <= customMin(7, kingFile+1); f++) {
                for (int r = customMax(0, kingRank-2); r <= kingRank-1; r++) {
                    if (boardState[r*8+f] == 'P') {
                        pawnShieldCount++;
                    }
                }
            }
            
            score += pawnShieldCount * 10; // Bonus for each pawn in shield
        }
        
        if (blackKingPos >= 0) {
            int kingFile = blackKingPos % 8;
            int kingRank = blackKingPos / 8;
            
            int pawnShieldCount = 0;
            for (int f = customMax(0, kingFile-1); f <= customMin(7, kingFile+1); f++) {
                for (int r = kingRank+1; r <= customMin(7, kingRank+2); r++) {
                    if (boardState[r*8+f] == 'p') {
                        pawnShieldCount++;
                    }
                }
            }
            
            score -= pawnShieldCount * 10;
        }
    }
    
    // ----------------- Development in Opening ----------------- //
    if (gamePhase == 0) { // Only in opening phase
        // Penalize unmoved knights and bishops, but not as severely to prevent bad development
        if (boardState[57] == 'N') score -= 15; // Queenside knight (reduced from 20)
        if (boardState[62] == 'N') score -= 15; // Kingside knight (reduced from 20)
        if (boardState[58] == 'B') score -= 10; // Queenside bishop (reduced from 15)
        if (boardState[61] == 'B') score -= 10; // Kingside bishop (reduced from 15)
        
        // Same for black
        if (boardState[1] == 'n') score += 15; // Queenside knight (reduced from 20)
        if (boardState[6] == 'n') score += 15; // Kingside knight (reduced from 20) 
        if (boardState[2] == 'b') score += 10; // Queenside bishop (reduced from 15)
        if (boardState[5] == 'b') score += 10; // Kingside bishop (reduced from 15)
        
        // Penalty for queen development too early
        for (size_t i = 0; i < boardState.size(); i++) {
            if (boardState[i] == 'Q' && i != 59) {
                int moveCount = position.fullMoveNumber;
                if (moveCount < 5) {
                    score -= 30; // Penalty for early queen development
                }
            }
            if (boardState[i] == 'q' && i != 3) {
                int moveCount = position.fullMoveNumber;
                if (moveCount < 5) {
                    score += 30; // Penalty for black's early queen development
                }
            }
        }
        
        // UPDATED: Much stronger bonuses for key opening responses to e4
        if (position.fullMoveNumber <= 2 && !position.whiteToMove) {
            // Check if white played e4
            if (boardState[36] == 'P' && boardState[52] == ' ') {
                // Bonus for e5 response
                if (boardState[12] == 'p') {
                    score -= 80; // Much stronger bonus for e5
                }
                // Bonus for c5 (Sicilian)
                if (boardState[10] == 'p') {
                    score -= 75; // Stronger bonus for c5
                }
                // Bonus for e6 (French)
                if (boardState[20] == 'p') {
                    score -= 70; // Stronger bonus for e6
                }
                // Bonus for c6 (Caro-Kann)
                if (boardState[18] == 'p') {
                    score -= 70; // Stronger bonus for c6
                }
            }
        }
    }
    
    // ----------------- Mobility ----------------- //
    // Only count piece mobility other than knights (already handled separately)
    int whiteMobility = 0, blackMobility = 0;
    
    // Generate moves for each side and count them
    std::vector<Move> whiteMoves = GenerateMoves(position, true, true);
    std::vector<Move> blackMoves = GenerateMoves(position, false, true);
    
    whiteMobility = whiteMoves.size() - whiteKnightPositions.size() * 8;
    blackMobility = blackMoves.size() - blackKnightPositions.size() * 8;
    
    // Apply mobility bonus (scaled by phase)
    if (gamePhase == 0) { // Opening - moderate mobility importance
        score += whiteMobility * 2;
        score -= blackMobility * 2;
    } else if (gamePhase == 1) { // Middlegame - higher mobility importance
        score += whiteMobility * 3;
        score -= blackMobility * 3;
    } else { // Endgame - moderate mobility importance
        score += whiteMobility * 2;
        score -= blackMobility * 2;
    }
    
    // Bonus for checking the opponent's king
    if (IsKingInCheck(position, false)) {
        score += 50; // Bonus for checking the black king
    }
    if (IsKingInCheck(position, true)) {
        score -= 50; // Penalty for white king being in check
    }
    
    return score;
}

BoardPosition ParseMoveHistory(const std::string& moveHistory) {
    // Start with initial position
    BoardPosition position;
    position.boardState = "rnbqkbnrpppppppp                                PPPPPPPPRNBQKBNR";
    position.whiteCanCastleKingside = true;
    position.whiteCanCastleQueenside = true;
    position.blackCanCastleKingside = true;
    position.blackCanCastleQueenside = true;
    position.enPassantTargetSquare = -1;
    position.halfMoveClock = 0;
    position.fullMoveNumber = 1;
    position.whiteToMove = true;
    
    if (moveHistory.empty()) {
        return position;
    }
    
    // Split move history into individual moves
    std::vector<std::string> moves;
    std::istringstream moveStream(moveHistory);
    std::string moveStr;
    while (moveStream >> moveStr) {
        moves.push_back(moveStr);
    }
    
    // Apply each move to update the position
    for (const std::string& move : moves) {
        if (move.empty()) continue;
        try {
            Move internalMove = AlgebraicToInternalMove(move, position);
            position = ApplyMove(position, internalMove);
            PrintBoard(position.boardState);
            //position.whiteToMove = !position.whiteToMove;
        } catch (const std::exception& e) {
            std::cerr << "Error applying move '" << move << "': " << e.what() << std::endl;
            throw; // or continue;
        }
    }
    
    return position;
}

BoardPosition ApplyAlgebraicMove(const BoardPosition& position, const std::string& algebraicMove) {
    BoardPosition newPosition = position;
    
    // Check for castling
    if (algebraicMove == "O-O") {
        // Kingside castling
        if (position.whiteToMove) {
            // Update the board state for white kingside castling
            // Update castling rights
            newPosition.whiteCanCastleKingside = false;
            newPosition.whiteCanCastleQueenside = false;
        } else {
            // Update the board state for black kingside castling
            // Update castling rights
            newPosition.blackCanCastleKingside = false;
            newPosition.blackCanCastleQueenside = false;
        }
    } else if (algebraicMove == "O-O-O") {
        // Queenside castling
        // Similar implementation
    } else {
        // Regular move
        char piece = 'P'; // Default to pawn
        std::string from, to;
        bool isCapture = false;
        
        // Parse the algebraic move
        int index = 0;
        if (isupper(algebraicMove[0]) && !isdigit(algebraicMove[1])) {
            index = 1;
        }
        
        // Extract from and to squares
        from = algebraicMove.substr(index, 2);
        
        // Check for capture
        if (algebraicMove.find('x') != std::string::npos) {
            isCapture = true;
            to = algebraicMove.substr(algebraicMove.find('x') + 1, 2);
        } else {
            to = algebraicMove.substr(from.length() + index, 2);
        }
        
        // Convert from algebraic coordinates to board indices
        if (from.length() != 2 || to.length() != 2) {
            throw std::invalid_argument("Invalid algebraic move format: " + algebraicMove);
        }
        int fromIndex = AlgebraicToIndex(from);
        int toIndex = AlgebraicToIndex(to);
        
        piece = newPosition.boardState[fromIndex];
        
        // Update the board state
        newPosition.boardState[toIndex] = newPosition.boardState[fromIndex];
        newPosition.boardState[fromIndex] = ' ';
        
        // Update castling rights if king or rook moves
        if (piece == 'K') {
            if (position.whiteToMove) {
                newPosition.whiteCanCastleKingside = false;
                newPosition.whiteCanCastleQueenside = false;
            } else {
                newPosition.blackCanCastleKingside = false;
                newPosition.blackCanCastleQueenside = false;
            }
        } else if (piece == 'R') {
            // Update rook-specific castling rights
        }
        
        // Handle en passant
        // Handle pawn promotion
    }
    
    newPosition.whiteToMove = !position.whiteToMove;
    newPosition.fullMoveNumber += position.whiteToMove ? 1 : 0;
    
    return newPosition;
}

int AlgebraicToIndex(const std::string& algebraic) {
    if (algebraic.length() != 2 ||
        algebraic[0] < 'a' || algebraic[0] > 'h' ||
        algebraic[1] < '1' || algebraic[1] > '8') {
        throw std::invalid_argument("Invalid algebraic square: " + algebraic);
    }
    int file = algebraic[0] - 'a';
    int rank = 8 - (algebraic[1] - '0');
    return rank * 8 + file;
}

std::string ConvertToAlgebraic(const Move& move, const BoardPosition& position) {
    // Extract start and end positions
    int startPos = std::stoi(move.notation.substr(1, move.notation.length() - 3));
    int endPos = std::stoi(move.notation.substr(move.notation.length() - 2));

    // Convert to algebraic coordinates
    std::string startSquare = IndexToAlgebraic(startPos);
    std::string endSquare = IndexToAlgebraic(endPos);

    char piece = move.notation[0];
    bool isWhite = isupper(piece);

    // Check if the target square contains an opponent's piece
    char target = position.boardState[endPos];
    bool isOpponentPiece = (target != ' ' &&
        ((isWhite && islower(target)) ||
            (!isWhite && isupper(target))));

    // Only mark as capture if it's a real capture or en passant
    bool isCapture = isOpponentPiece || move.isEnPassant;

    // Build the algebraic notation
    std::string algebraic;

    // Add piece letter (except for pawns)
    if (piece != 'P' && piece != 'p') {
        algebraic += toupper(piece);
    }

    // Add the move coordinates
    algebraic += startSquare;

    // Add capture symbol if needed
    if (isCapture) {
        algebraic += 'x';
    }

    algebraic += endSquare;

    return algebraic;
}

std::string IndexToAlgebraic(int index) {
    int rank = 8 - (index / 8);
    int file = index % 8;
    
    std::string result;
    result += 'a' + file;
    result += '0' + rank;
    
    return result;
}

void PrintBoard(const std::string& boardState) {
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            std::cout << boardState[r * 8 + c] << ' ';
        }
        std::cout << std::endl;
    }
}

Move AlgebraicToInternalMove(const std::string& algebraicMove, const BoardPosition& position) {
    // Handle castling
    if (algebraicMove == "O-O" || algebraicMove == "0-0") {
        Move move;
        move.isCastling = true;
        move.isKingsideCastling = true;
        move.notation = (position.whiteToMove ? "K" : "k") + std::string(position.whiteToMove ? "6062" : "0406");
        return move;
    }
    if (algebraicMove == "O-O-O" || algebraicMove == "0-0-0") {
        Move move;
        move.isCastling = true;
        move.isKingsideCastling = false;
        move.notation = (position.whiteToMove ? "K" : "k") + std::string(position.whiteToMove ? "6062" : "0402");
        return move;
    }

    // Handle promotion (e.g., e7e8Q or e7e8=Q)
    std::string moveStr = algebraicMove;
    char promotion = '\0';
    size_t promoPos = moveStr.find('=');
    if (promoPos != std::string::npos) {
        promotion = moveStr[promoPos + 1];
        moveStr = moveStr.substr(0, promoPos);
    } else if (moveStr.length() == 5 && isalpha(moveStr[4])) {
        promotion = moveStr[4];
        moveStr = moveStr.substr(0, 4);
    }

    // Parse from and to squares
    std::string from = moveStr.substr(0, 2);
    std::string to = moveStr.substr(2, 2);
    int fromIndex = AlgebraicToIndex(from);
    int toIndex = AlgebraicToIndex(to);
    char piece = position.boardState[fromIndex];

    Move move;
    std::string startPosStr = (fromIndex < 10) ? "0" + std::to_string(fromIndex) : std::to_string(fromIndex);
    std::string endPosStr = (toIndex < 10) ? "0" + std::to_string(toIndex) : std::to_string(toIndex);
    move.notation = piece + startPosStr + endPosStr;

    // Handle en passant
    if ((tolower(piece) == 'p') && (fromIndex % 8 != toIndex % 8) && position.boardState[toIndex] == ' ') {
        move.isEnPassant = true;
        move.enPassantCapturePos = position.whiteToMove ? (toIndex + 8) : (toIndex - 8);
    }

    // Handle promotion
    if (promotion) {
        move.notation += promotion;
    }

    return move;
}

// Add this helper function before EvaluateBoard
// Enhanced opening principles evaluation function
int EvaluateOpeningPrinciples(const BoardPosition& position) {
   int score = 0;
    const std::string& boardState = position.boardState;
    int moveNum = position.fullMoveNumber;

    // === WHITE'S FIRST MOVE - Direct Score Override ===
    // First moves for White are so critically important we want to 
    // enforce specific move choices rather than relying on evaluation
    if (moveNum == 1 && position.whiteToMove) {
        // e4 - highest priority
        if (boardState[52] == ' ' && boardState[36] == 'P') {
            return 5000; // Direct fixed score, not a bonus
        }
        
        // d4 - second priority
        if (boardState[51] == ' ' && boardState[35] == 'P') {
            return 4800;
        }
        
        // Nf3 - third priority
        if (boardState[62] == ' ' && boardState[45] == 'N') {
            return 4600;
        }
        
        // c4 - fourth priority
        if (boardState[50] == ' ' && boardState[34] == 'P') {
            return 4400;
        }
    }

    // ===== BAD MOVES - EXTREMELY SEVERE PENALTIES =====
    if (moveNum <= 3) {  // Only in first few moves
        // ----- WHITE'S BAD MOVES -----
        if (position.whiteToMove) {
            // BAD PAWN MOVES (edge pawns that don't control center)
            // a2-a3 check
            if (boardState[48] == ' ' && boardState[40] == 'P') {
                return -3000;  // Direct extreme penalty, not a bonus adjustment
            }
            
            // h2-h3 check
            if (boardState[55] == ' ' && boardState[47] == 'P') {
                return -3000;
            }
            
            // BAD KNIGHT MOVES (to edges)
            // Knight to a3
            if (boardState[57] == ' ' && boardState[40] == 'N') {
                return -3000;
            }
            
            // Knight to h3
            if (boardState[62] == ' ' && boardState[47] == 'N') {
                return -3000;
            }
            
            // Knight to edge (a-file or h-file)
            for (int i = 0; i < 64; i++) {
                int file = i % 8;
                if ((file == 0 || file == 7) && boardState[i] == 'N') {
                    return -2000;
                }
            }
        }
        // ----- BLACK'S BAD MOVES -----
        else {
            // BAD PAWN MOVES
            // a7-a6 check
            if (boardState[8] == ' ' && boardState[16] == 'p') {
                return 3000;  // Extreme penalty (positive hurts Black)
            }
            
            // h7-h6 check
            if (boardState[15] == ' ' && boardState[23] == 'p') {
                return 3000;
            }
            
            // Na6 check (specific response to e4)
            if (boardState[36] == 'P' && boardState[1] == ' ' && boardState[16] == 'n') {
                return 3000;  // CORRECTED: Positive score (bad for Black)
            }
            
            // Nh6 check (specific response to e4)
            if (boardState[36] == 'P' && boardState[6] == ' ' && boardState[23] == 'n') {
                return 3000;  // CORRECTED: Positive score (bad for Black)
            }
            
            // Knight to edge (a-file or h-file)
            for (int i = 0; i < 64; i++) {
                int file = i % 8;
                if ((file == 0 || file == 7) && boardState[i] == 'n') {
                    return 2000;  // Positive score (bad for Black)
                }
            }
            
            // === BLACK'S RESPONSES TO WHITE OPENINGS ===
            // After e4, these are the best responses
            if (boardState[36] == 'P' && boardState[52] == ' ') {
                // e5 (1. e4 e5)
                if (boardState[12] == 'p' && boardState[20] == ' ') {
                    return -800;  // Extremely good for Black
                }
                
                // c5 - Sicilian (1. e4 c5)
                if (boardState[10] == 'p' && boardState[18] == ' ') {
                    return -750;
                }
                
                // e6 - French (1. e4 e6)
                if (boardState[28] == 'p') {
                    return -700;
                }
                
                // c6 - Caro-Kann (1. e4 c6)
                if (boardState[26] == 'p') {
                    return -700;
                }
            }
            
            // After d4, these are the best responses
            if (boardState[35] == 'P' && boardState[51] == ' ') {
                // d5 (1. d4 d5)
                if (boardState[11] == 'p' && boardState[19] == ' ') {
                    return -800;
                }
                
                // Nf6 - Indian Defenses (1. d4 Nf6)
                if (boardState[6] == ' ' && boardState[21] == 'n') {
                    return -750;
                }
            }
        }
    }
    
    // ===== GOOD MOVES - STRONG BONUSES =====
    if (moveNum <= 3) {
        // ----- WHITE'S GOOD MOVES -----
        if (position.whiteToMove) {
            // Central pawn advances
            // e4
            if (boardState[52] == ' ' && boardState[36] == 'P') {
                score += 500;  // Much stronger bonus
            }
            
            // d4
            if (boardState[51] == ' ' && boardState[35] == 'P') {
                score += 450;
            }
            
            // c4
            if (boardState[50] == ' ' && boardState[34] == 'P') {
                score += 400;
            }
            
            // Good knight development
            if (boardState[62] == ' ' && boardState[45] == 'N') { // Nf3
                score += 450;
            }
            
            if (boardState[57] == ' ' && boardState[42] == 'N') { // Nc3
                score += 400;
            }
        }
        // ----- BLACK'S GOOD MOVES -----
        else {
            // e4 has been played by White
            if (boardState[36] == 'P' && boardState[52] == ' ') {
                // e5 response
                if (boardState[20] == ' ' && boardState[12] == 'p') {
                    score -= 500; // Strong bonus for e5 (negative helps Black)
                }
                
                // c5 response (Sicilian)
                if (boardState[18] == ' ' && boardState[10] == 'p') {
                    score -= 450;
                }
                
                // e6 response (French)
                if (boardState[20] == ' ' && boardState[28] == 'p') {
                    score -= 400;
                }
                
                // c6 response (Caro-Kann)
                if (boardState[18] == ' ' && boardState[26] == 'p') {
                    score -= 400;
                }
            }
        }
    }
    
    // ----- GENERAL OPENING PRINCIPLES (first ~10 moves) -----
    if (moveNum <= 10) {
        // CENTER CONTROL - Add bonuses for central pawns and pieces
        int whiteCenterControl = 0;
        int blackCenterControl = 0;
        
        // The central squares (d4, e4, d5, e5)
        int centralSquares[4] = {35, 36, 27, 28};
        
        // Count control of center squares
        for (int sq : centralSquares) {
            // Pawn control
            if (sq-8 >= 0 && sq-8 < 64) {
                if (boardState[sq-8] == 'P') whiteCenterControl += 2;
                if (boardState[sq-8] == 'p') blackCenterControl += 2;
            }
            if (sq+8 >= 0 && sq+8 < 64) {
                if (boardState[sq+8] == 'p') blackCenterControl += 2;
                if (boardState[sq+8] == 'P') whiteCenterControl += 2;
            }
            
            // Direct occupation
            if (boardState[sq] != ' ') {
                if (isupper(boardState[sq])) whiteCenterControl += 3;
                if (islower(boardState[sq])) blackCenterControl += 3;
            }
            
            // Knight control
            for (const auto& offset : std::vector<int>{-17, -15, -10, -6, 6, 10, 15, 17}) {
                int pos = sq + offset;
                if (pos >= 0 && pos < 64) {
                    if (boardState[pos] == 'N') whiteCenterControl += 1;
                    if (boardState[pos] == 'n') blackCenterControl += 1;
                }
            }
        }
        
        // Award bonus for center control
        score += (whiteCenterControl - blackCenterControl) * 5;
        
        // ----- DEVELOPMENT QUALITY -----
        // Knights and bishops should be developed to good squares
        
        // Developed piece count
        int whiteDevelopedPieces = 0;
        int blackDevelopedPieces = 0;
        
        // Penalize specific bad piece placements
        
        // Knights should not be on edges in opening
        for (int i = 0; i < 64; i++) {
            int file = i % 8;
            if (file == 0 || file == 7) { // a or h file
                if (boardState[i] == 'N') score -= 40;
                if (boardState[i] == 'n') score += 40;
            }
        }
        
        // Count developed minor pieces
        if (boardState[62] != 'N') whiteDevelopedPieces++;
        if (boardState[57] != 'N') whiteDevelopedPieces++;
        if (boardState[61] != 'B') whiteDevelopedPieces++;
        if (boardState[58] != 'B') whiteDevelopedPieces++;
        
        if (boardState[1] != 'n') blackDevelopedPieces++;
        if (boardState[6] != 'n') blackDevelopedPieces++;
        if (boardState[2] != 'b') blackDevelopedPieces++;
        if (boardState[5] != 'b') blackDevelopedPieces++;
        
        // Reward development
        score += whiteDevelopedPieces * 15;
        score -= blackDevelopedPieces * 15;
        
        // ----- CASTLING AND KING SAFETY -----
        // Kingside castling is generally preferred in opening
        
        // Check for castled kings
        bool whiteKingsideCastled = (boardState[62] == 'K' && boardState[61] == 'R');
        bool blackKingsideCastled = (boardState[6] == 'k' && boardState[5] == 'r');
        
        if (whiteKingsideCastled) score += 50;
        if (blackKingsideCastled) score -= 50;
    }
    
    return score;
}

extern "C" __declspec(dllexport) const char* GetBestMove(const char* moveHistoryStr, int maxDepth, bool isWhite)
{
    std::string moveHistory(moveHistoryStr);
    BoardPosition currentPosition = ParseMoveHistory(moveHistory);

    // Debug info
    PrintBoard(currentPosition.boardState);

    Move bestMove;

    // Starting time for time management
    auto startTime = std::chrono::high_resolution_clock::now();
    const int MAX_SEARCH_TIME_MS = 5000; // 5 seconds max search time

    // Clear transposition table at the start of a new search
    for (auto& entry : transpositionTable) {
        entry = TTEntry();
    }

    // Generate moves for the CURRENT player
    std::vector<Move> allMoves = GenerateMoves(currentPosition, currentPosition.whiteToMove);

    // Filter out illegal moves that would leave the king in check
    std::vector<Move> legalMoves;
    for (const Move& move : allMoves) {
        BoardPosition newPosition = ApplyMove(currentPosition, move);
        // Make sure the move doesn't leave the king in check
        if (!IsKingInCheck(newPosition, currentPosition.whiteToMove)) {
            legalMoves.push_back(move);
        }
    }

    // If no legal moves, return an empty string
    if (legalMoves.empty()) {
        static std::string noMoveResult = "";
        return noMoveResult.c_str();
    }

    // ----- OPENING MOVE FILTERING -----
    // Only apply to first few moves (opening phase)
    if (currentPosition.fullMoveNumber <= 4) {
        std::cout << "Applying opening move filters..." << std::endl;
        
        // Store initial move count to check if we filtered any moves
        int initialMoveCount = legalMoves.size();
        
        // Filter out known bad opening moves
        for (auto it = legalMoves.begin(); it != legalMoves.end();) {
            bool removeBadMove = false;
            
            // WHITE BAD MOVES
            if (currentPosition.whiteToMove) {
                // a2-a3 (P4840)
                if (it->notation == "P4840") {
                    std::cout << "Filtering out a2a3" << std::endl;
                    removeBadMove = true;
                }
                // h2-h3 (P5547)
                else if (it->notation == "P5547") {
                    std::cout << "Filtering out h2h3" << std::endl;
                    removeBadMove = true;
                }
                // Knight to edge files (a or h)
                else if (it->notation[0] == 'N') {
                    int endPos = std::stoi(it->notation.substr(3, 2));
                    int file = endPos % 8;
                    if (file == 0 || file == 7) { // Knight on a-file or h-file
                        std::cout << "Filtering out knight move to edge: " << it->notation << std::endl;
                        removeBadMove = true;
                    }
                }
            }
            // BLACK BAD MOVES
            else {
                // Na6 (n0116) after e4
                if (it->notation == "n0116" && 
                    (moveHistory == "e2e4" || moveHistory == "e4")) {
                    std::cout << "Filtering out Na6 response to e4" << std::endl;
                    removeBadMove = true;
                }
                // Nh6 (n0623) after e4
                else if (it->notation == "n0623" && 
                    (moveHistory == "e2e4" || moveHistory == "e4")) {
                    std::cout << "Filtering out Nh6 response to e4" << std::endl;
                    removeBadMove = true;
                }
                // Edge knights in general
                else if (it->notation[0] == 'n') {
                    int endPos = std::stoi(it->notation.substr(3, 2));
                    int file = endPos % 8;
                    if (file == 0 || file == 7) { // Knight on a-file or h-file
                        std::cout << "Filtering out knight move to edge: " << it->notation << std::endl;
                        removeBadMove = true;
                    }
                }
            }
            
            // Remove the move if it's bad (but keep at least one legal move)
            if (removeBadMove && legalMoves.size() > 1) {
                it = legalMoves.erase(it);
            } else {
                ++it;
            }
        }
        
        // Debug the remaining moves
        std::cout << "Legal moves after filtering:" << std::endl;
        for (const Move& move : legalMoves) {
            std::cout << "  " << move.notation << " -> " 
                      << ConvertToAlgebraic(move, currentPosition) << std::endl;
        }
        
        // Verify we didn't filter ALL moves
        if (legalMoves.empty() && initialMoveCount > 0) {
            std::cout << "Warning: Filtered all moves, restoring legal ones" << std::endl;
            // If we filtered everything, restore the best of the bad moves
            legalMoves = allMoves;
            // Just keep filtering illegal moves
            for (auto it = legalMoves.begin(); it != legalMoves.end();) {
                BoardPosition newPosition = ApplyMove(currentPosition, *it);
                if (IsKingInCheck(newPosition, !currentPosition.whiteToMove)) {
                    it = legalMoves.erase(it);
                } else {
                    ++it;
                }
            }
        }
        
        // Secondary evaluation-based filtering
        if (legalMoves.size() > 1) {
            // Evaluate and filter based on opening principles scores
            std::vector<std::pair<int, Move>> scoredMoves;
            for (const Move& move : legalMoves) {
                BoardPosition newPos = ApplyMove(currentPosition, move);

                // Evaluate from the current player's perspective
                newPos.whiteToMove = currentPosition.whiteToMove;
                int score = EvaluateBoard(newPos);

                // Negate scores for Black so higher is always better
                if (!currentPosition.whiteToMove) {
                    score = -score;
                }
                scoredMoves.push_back({score, move});
                std::cout << "  Evaluated " << ConvertToAlgebraic(move, currentPosition) 
                          << " = " << score << std::endl;
            }

            // Special case: First move by White - ensure classical opening moves are preferred
            if (currentPosition.fullMoveNumber == 1 && currentPosition.whiteToMove) {
                for (auto& scoreMove : scoredMoves) {
                    std::string algebraic = ConvertToAlgebraic(scoreMove.second, currentPosition);
                    if (algebraic == "e4" || algebraic == "e2e4") {
                        scoreMove.first = 5000; // Override the score completely
                        std::cout << "Boosting e4 to score 5000" << std::endl;
                    }
                    else if (algebraic == "d4" || algebraic == "d2d4") {
                        scoreMove.first = 4800;
                        std::cout << "Boosting d4 to score 4800" << std::endl;
                    }
                    else if (algebraic == "Nf3" || algebraic == "Ng1f3") {
                        scoreMove.first = 4600;
                        std::cout << "Boosting Nf3 to score 4600" << std::endl;
                    }
                }
            }
            
            // Remove moves with extreme penalties
            if (scoredMoves.size() > 1) {
                auto cutoffIter = std::remove_if(scoredMoves.begin(), scoredMoves.end(), 
                    [&currentPosition](const std::pair<int, Move>& scoredMove) {
                        return (currentPosition.whiteToMove && scoredMove.first < -1000) || 
                               (!currentPosition.whiteToMove && scoredMove.first < -1000);
                    });
                
                // Only remove if we'll still have at least one move left
                if (cutoffIter != scoredMoves.begin() && scoredMoves.end() - cutoffIter > 1) {
                    scoredMoves.erase(cutoffIter, scoredMoves.end());
                }
            }
            
            // Sort moves by score (best first) - ALWAYS use descending order
            std::sort(scoredMoves.begin(), scoredMoves.end(),
                [](const auto& a, const auto& b) { return a.first > b.first; });
            
            // Only keep the top 50% of moves for search
            int keepCount = customMax(1, (int)(scoredMoves.size() / 2));
            if (scoredMoves.size() > keepCount) {
                scoredMoves.resize(keepCount);
            }
            
            // Convert back to move list
            legalMoves.clear();
            for (const auto& scoredMove : scoredMoves) {
                legalMoves.push_back(scoredMove.second);
            }
        }
    }

    // Iterative deepening - start from depth 1 and increase
    for (int currentDepth = 1; currentDepth <= maxDepth; currentDepth++) {
        // Create root node manually with only filtered moves
        MoveTreeNode* root = new MoveTreeNode(currentPosition.boardState);
        
        // Add only the filtered legal moves as children
        for (const Move& move : legalMoves) {
            BoardPosition newPosition = ApplyMove(currentPosition, move);
            MoveTreeNode* child = new MoveTreeNode(newPosition.boardState, move, root);
            
            // If this isn't the last depth, expand the children of this move
            if (currentDepth > 1) {
                ExpandNode(child, currentDepth-1, !currentPosition.whiteToMove, newPosition);
            }
            
            root->children.push_back(child);
        }

        // Find the best move using minimax - ALWAYS MAXIMIZE for both sides since we normalized scores
        int bestValue = -2147483647; // Initialize to very low value for both players
        Move currentBestMove;
        bool moveFound = false;

        // Search all filtered root children
        for (MoveTreeNode* childNode : root->children) {
            int moveValue;
            if (currentPosition.whiteToMove) {
                moveValue = -MinimaxOnTree(childNode, currentDepth - 1, -2147483647, -bestValue, false);
            } else {
                moveValue = -MinimaxOnTree(childNode, currentDepth - 1, -bestValue, 2147483647, true);
            }
            
            // For both players, higher score is better (after our normalization)
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
    result = ConvertToAlgebraic(bestMove, currentPosition);
    
    // Final debug output
    std::cout << "Selected move: " << result << std::endl;
    
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

// Add this to main() or in a separate test function
void DebugOpeningMoves() {
    // Test a3 position detection
    BoardPosition testPos;
    testPos.boardState = "rnbqkbnrpppppppp                                PPPPPPPPR BQKBNR";
    testPos.boardState[48] = ' '; // Empty a2
    testPos.boardState[40] = 'P'; // Pawn at a3
    testPos.whiteToMove = true;
    testPos.fullMoveNumber = 1;
    
    std::cout << "a3 evaluation: " << EvaluateOpeningPrinciples(testPos) << std::endl;
    
    // Test e4 position
    BoardPosition e4Pos;
    e4Pos.boardState = "rnbqkbnrpppppppp                P               PPP PPPPRNBQKBNR";
    e4Pos.whiteToMove = false;
    e4Pos.fullMoveNumber = 1;
    
    std::cout << "Position after e4: " << EvaluateOpeningPrinciples(e4Pos) << std::endl;
    
    // Test Na6 after e4
    BoardPosition na6Pos = e4Pos;
    na6Pos.boardState[1] = ' '; // Knight moved from b8
    na6Pos.boardState[16] = 'n'; // Knight at a6
    
    std::cout << "Na6 after e4 evaluation: " << EvaluateOpeningPrinciples(na6Pos) << std::endl;
}

int main() {
    // Test 1: Generate moves for the starting position
    BoardPosition startPosition;
    startPosition.boardState = "rnbqkbnrpppppppp                                PPPPPPPPRNBQKBNR";
    startPosition.whiteCanCastleKingside = true;
    startPosition.whiteCanCastleQueenside = true;
    startPosition.blackCanCastleKingside = true;
    startPosition.blackCanCastleQueenside = true;
    startPosition.enPassantTargetSquare = -1;
    startPosition.halfMoveClock = 0;
    startPosition.fullMoveNumber = 1;
    startPosition.whiteToMove = true;

    std::vector<Move> whiteMoves = GenerateMoves(startPosition, true);
    std::cout << "White has " << whiteMoves.size() << " legal moves from the starting position." << std::endl;

    // Test 2: Apply a move (e2 to e4)
    Move move = { "P4840" };
    BoardPosition afterMovePosition = ApplyMove(startPosition, move);
    std::cout << "After e2-e4, board[40] = " << afterMovePosition.boardState[40]
              << ", board[48] = " << afterMovePosition.boardState[48] << std::endl;

    // Test 3: Get best move from starting position
    const char* bestMove = GetBestMove("", 3, true);
    std::cout << "Best move for white at depth 3: " << bestMove << std::endl;

    // Test 4: Get best move after a couple of moves (e4 Nb8a6 d2d4)
    const char* moveHistory = "e2e4";
    const char* bestMoveAfter = GetBestMove(moveHistory, 3, false); // Black to move
    std::cout << "Best move for black: " << bestMoveAfter << std::endl;

    std::cout<< "Debugging opening moves..." << std::endl;
    DebugOpeningMoves();

    return 0;
}