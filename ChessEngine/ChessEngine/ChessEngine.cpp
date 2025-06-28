#include "pch.h"
#include "ChessEngine.h"
#include <string>
#include <vector>
#include <limits>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <unordered_map>

template <typename T>
T customMin(T a, T b) {
    return (a < b) ? a : b;
}

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

struct TTEntry {
    uint64_t zobristKey;     
    int depth;               
    int flag;                
    int score;               
    Move bestMove;           
};

struct MoveTreeNode {
    Move move;
    std::string boardState;           
    int evaluation = 0;                 
    bool isEvaluated = false;           
    std::vector<MoveTreeNode*> children;
    MoveTreeNode* parent = nullptr;

    MoveTreeNode(const std::string& state) :
        boardState(state) {
    }

    MoveTreeNode(const std::string& state, const Move& m, MoveTreeNode* p) :
        boardState(state), move(m), parent(p) {
    }

    ~MoveTreeNode() {
        for (MoveTreeNode* child : children) {
            delete child;
        }
    }
};

struct BoardPosition {
    std::string boardState;
    bool whiteCanCastleKingside;
    bool whiteCanCastleQueenside;
    bool blackCanCastleKingside;
    bool blackCanCastleQueenside;
    int enPassantTargetSquare;
    int halfMoveClock;
    int fullMoveNumber;
    bool whiteToMove;
};

const int TT_EXACT = 0;
const int TT_ALPHA = 1;
const int TT_BETA = 2;

const int MAX_PLY = 64;
Move killerMoves[MAX_PLY][2] = {};

// --- Start of Chess Personalities Settings --- \\

enum ChessPersonality {
    STANDARD = 0,
    AGGRESSIVE = 1,
    POSITIONAL = 2, 
    SOLID = 3,
    DYNAMIC = 4
};

ChessPersonality currentPersonality = STANDARD;
// --- End of Chess Personalities Settings --- \\

std::unordered_map<uint64_t, int> evaluationCache;
const size_t MAX_EVAL_CACHE_SIZE = 500000;

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
bool IsGoodCapture(const BoardPosition& position, const Move& move);
bool IsSquareAttacked(const BoardPosition& position, int square, bool byWhite);
int GetCheapestAttackerValue(const BoardPosition& position, int square, bool byWhite);
bool HasMaterialThreat(const BoardPosition& position, bool forWhite);
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
int EvaluateBoard(const BoardPosition& position, int searchDepth = 0);
BoardPosition ParseMoveHistory(const std::string& moveHistory);
BoardPosition ApplyAlgebraicMove(const BoardPosition& position, const std::string& algebraicMove);
int AlgebraicToIndex(const std::string& algebraic);
std::string ConvertToAlgebraic(const Move& move, const BoardPosition& position);
std::string IndexToAlgebraic(int index);
Move AlgebraicToInternalMove(const std::string& algebraicMove, const BoardPosition& position);
void PrintBoard(const std::string& boardState);
int EvaluateOpeningPrinciples(const BoardPosition& position);
int ApplyPersonalityToEvaluation(int baseScore, const BoardPosition& position, 
                               const std::vector<Move>* preCalculatedMoves,
                               bool fullCalculation = true);
bool IsMoveSafe(const BoardPosition& position, const Move& move);
bool IsValidMoveNotation(const Move& move); 
bool IsTacticalBlunder(const BoardPosition& position, const Move& move);
int EvaluatePawnStructure(const BoardPosition& position, bool forWhite);
// ---------------------------- End of Function declarations ---------------------------- \\

void StoreKillerMove(const Move& move, int ply) {
    if (!(killerMoves[ply][0].notation == move.notation)) {
        killerMoves[ply][1] = killerMoves[ply][0];
        killerMoves[ply][0] = move;
    }
}

bool IsKiller(const Move& move, int ply) {
    return (killerMoves[ply][0].notation == move.notation) || 
           (killerMoves[ply][1].notation == move.notation);
}

uint64_t GetZobristKey(const std::string& boardState) {
    uint64_t key = 0;
    for (size_t i = 0; i < boardState.size(); i++) {
        if (boardState[i] != ' ') {
            key ^= ((uint64_t)boardState[i] << (i % 8)) + i;
        }
    }
    return key;
}

const int TT_SIZE = 1 << 20;
std::vector<TTEntry> transpositionTable(TT_SIZE);

void StoreTranspositionTable(const std::string& boardState, int depth, 
                           int flag, int score, const Move& bestMove) {
    uint64_t key = GetZobristKey(boardState);
    int index = key % TT_SIZE;
    
    transpositionTable[index] = {key, depth, flag, score, bestMove};
}

bool ProbeTranspositionTable(const std::string& boardState, int depth, 
                           int& alpha, int& beta, int& score, Move& bestMove) {
    uint64_t key = GetZobristKey(boardState);
    int index = key % TT_SIZE;
    
    if (transpositionTable[index].zobristKey == key) {
        TTEntry& entry = transpositionTable[index];
        
        if (entry.depth >= depth) {
            bestMove = entry.bestMove;
            
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



MoveTreeNode* BuildMoveTree(const BoardPosition& position, int depth, bool isWhiteTurn) {
    MoveTreeNode* root = new MoveTreeNode(position.boardState);

    if (depth <= 0) {
        return root;
    }

    std::vector<Move> possibleMoves = GenerateMoves(position, isWhiteTurn);

    for (const Move& move : possibleMoves) {
        BoardPosition newPosition = ApplyMove(position, move);

        MoveTreeNode* childNode = new MoveTreeNode(newPosition.boardState, move, root);

        if (depth > 1) {
            MoveTreeNode* responseTree = BuildMoveTree(newPosition, depth - 1, !isWhiteTurn);
            for (MoveTreeNode* grandchild : responseTree->children) {
                childNode->children.push_back(grandchild);
            }
            delete responseTree;
        }

        root->children.push_back(childNode);
    }

    return root;
}

void ExpandNode(MoveTreeNode* node, int depth, bool isWhiteTurn, const BoardPosition& position) {
    if (depth <= 0) return;

    std::vector<Move> possibleMoves = GenerateMoves(position, isWhiteTurn);

    for (const Move& move : possibleMoves) {
        BoardPosition newPosition = position;
        newPosition = ApplyMove(position, move);
        MoveTreeNode* childNode = new MoveTreeNode(newPosition.boardState, move, node);

        if (depth > 1) {
            ExpandNode(childNode, depth - 1, !isWhiteTurn, newPosition);
        }

        node->children.push_back(childNode);
    }
}

void OrderMoves(std::vector<Move>& moves, int ply, const std::string& boardState, const Move& ttMove) {
    std::vector<std::pair<int, Move>> scoredMoves;
    
    for (const Move& move : moves) {
        int score = 0;
        
        if (!ttMove.notation.empty() && move.notation == ttMove.notation) {
            score = 20000;
        }
        else if (move.notation.length() >= 5) {
            int endPos = std::stoi(move.notation.substr(3, 2));
            char victim = boardState[endPos];
            
            if (victim != ' ' || move.isEnPassant) {
                BoardPosition tempPos;
                tempPos.boardState = boardState;
                
                if (IsGoodCapture(tempPos, move)) {
                    int captureScore = 10000;
                    
                    if (victim != ' ') {
                        char attacker = move.notation[0];
                        captureScore += GetPieceValue(victim) * 100 - GetPieceValue(attacker);
                    } else if (move.isEnPassant) {
                        captureScore += 100;
                    }
                    
                    score = captureScore;
                } else {
                    score = -100;
                }
            }
        }
        else if (IsKiller(move, ply)) {
            score = 9000;
        }
        else {
            score = 0;
        }
        
        scoredMoves.push_back({score, move});
    }
    
    std::sort(scoredMoves.begin(), scoredMoves.end(), 
        [](const auto& a, const auto& b) {
            return a.first > b.first;
        });
    
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
        case 'k': return 0;
        default: return 0;
    }
}

int Quiescence(const BoardPosition& position, int alpha, int beta, bool maximizingPlayer, int maxDepth) {
    ChessPersonality savedPersonality = currentPersonality;
    if (maxDepth <= 3) currentPersonality = STANDARD;

    int standPat = EvaluateBoard(position, -maxDepth); 

    currentPersonality = savedPersonality;
    

    if (maximizingPlayer) {
        if (standPat >= beta) return beta;
        if (standPat > alpha) alpha = standPat;
    } else {
        if (standPat <= alpha) return alpha;
        if (standPat < beta) beta = standPat;
    }
    
    if (maxDepth <= 0) return standPat;
    
    std::vector<Move> allMoves = GenerateMoves(position, maximizingPlayer);
    std::vector<std::pair<int, Move>> scoredCaptures;
    
    for (const Move& move : allMoves) {
        int endPos = std::stoi(move.notation.substr(move.notation.length() - 2));
        char target = position.boardState[endPos];
        
        if (target != ' ' || move.isEnPassant) {
            int score = 0;
            if (target != ' ') {
                int victimValue = GetPieceValue(target) * 10;
                int attackerValue = GetPieceValue(move.notation[0]);
                score = victimValue - attackerValue;
            } else if (move.isEnPassant) {
                score = 10;
            }
            scoredCaptures.push_back({score, move});
        }
    }
    
    std::sort(scoredCaptures.begin(), scoredCaptures.end(),
        [](const auto& a, const auto& b) { return a.first > b.first; });
    
    for (const auto& [score, move] : scoredCaptures) {
        BoardPosition newPosition = ApplyMove(position, move);
        int evalScore = -Quiescence(newPosition, -beta, -alpha, !maximizingPlayer, maxDepth - 1);
        
        if (maximizingPlayer) {
            if (evalScore >= beta) return beta;
            if (evalScore > alpha) alpha = evalScore;
        } else {
            if (evalScore <= alpha) return alpha;
            if (evalScore < beta) beta = evalScore;
        }
    }
    
    return maximizingPlayer ? alpha : beta;
}

bool IsGoodCapture(const BoardPosition& position, const Move& move) {
    if (move.notation.length() < 5) return false;
    
    int startPos = std::stoi(move.notation.substr(1, 2));
    int endPos = std::stoi(move.notation.substr(3, 2));
    
    char attacker = position.boardState[startPos];
    char victim = position.boardState[endPos];
    
    if (victim == ' ' && !move.isEnPassant) return false;
    
    int attackerValue = GetPieceValue(attacker);
    int victimValue = move.isEnPassant ? 1 : GetPieceValue(victim);
    
    bool isGoodValueCapture = (victimValue > attackerValue);
    bool hasCompensation = false;
    
    BoardPosition afterCapture = ApplyMove(position, move);
    std::vector<Move> responses = GenerateMoves(afterCapture, !position.whiteToMove);
    
    for (const Move& response : responses) {
        if (response.notation.length() >= 5) {
            int responseTargetPos = std::stoi(response.notation.substr(3, 2));
            
            if (responseTargetPos == endPos) {
                char recapturer = position.boardState[std::stoi(response.notation.substr(1, 2))];
                int recapturerValue = GetPieceValue(recapturer);
                
                if (recapturerValue <= attackerValue) {
                    return false;
                }
            }
        }
    }
    
    if (tolower(victim) == 'p' && (tolower(attacker) == 'n' || tolower(attacker) == 'b')) {
        bool isPionDefended = false;
        
        for (const Move& response : responses) {
            if (response.notation.length() >= 5) {
                int responseTargetPos = std::stoi(response.notation.substr(3, 2));
                if (responseTargetPos == endPos) {
                    isPionDefended = true;
                    break;
                }
            }
        }
        
        if (isPionDefended) {
            std::cout << "Caz special: Piesă minoră capturează pion apărat!" << std::endl;
            return false;
        }
    }
    
    if (!isGoodValueCapture) {
        bool hasCompensation = false;
        
        for (int i = 0; i < 64; i++) {
            if (i == endPos) continue;
            
            char targetPiece = afterCapture.boardState[i];
            bool isEnemyPiece = (position.whiteToMove && islower(targetPiece) && targetPiece != ' ') ||
                               (!position.whiteToMove && isupper(targetPiece) && targetPiece != ' ');
            
            if (isEnemyPiece && GetPieceValue(targetPiece) >= attackerValue) {
                bool wasDefended = IsSquareAttacked(position, i, !position.whiteToMove);
                bool isStillDefended = IsSquareAttacked(afterCapture, i, !position.whiteToMove);
                
                if (wasDefended && !isStillDefended) {
                    hasCompensation = true;
                    break;
                }
            }
        }
        
        if (!hasCompensation) {
            return false;
        }
    }
    
    if (tolower(attacker) == 'n' && endPos == 36) {
        for (int i = 0; i < 64; i++) {
            if (position.boardState[i] == 'N') {
                int knightRank = i / 8;
                int knightFile = i % 8;
                
                if ((knightRank == 5 && knightFile == 2) || 
                    (knightRank == 4 && knightFile == 3) || 
                    (knightRank == 4 && knightFile == 5) || 
                    (knightRank == 6 && knightFile == 5)) { 
                    std::cout << "DETECTOR SPECIAL: Blunder tactic Nf6xe4 cu recaptură de cal!" << std::endl;
                    return false;
                }
            }
        }
    }
    
    return isGoodValueCapture || hasCompensation;
}

bool IsSquareAttacked(const BoardPosition& position, int square, bool byWhite) {
    for (int i = 0; i < 64; i++) {
        char attacker = position.boardState[i];
        if (attacker == ' ') continue;
        
        bool isWhitePiece = isupper(attacker);
        if (byWhite != isWhitePiece) continue;
        
        switch (tolower(attacker)) {
            case 'p': {
                int rank = i / 8;
                int file = i % 8;
                int targetRank = square / 8;
                int targetFile = square % 8;
                
                if (isWhitePiece) {
                    if (rank - 1 == targetRank && 
                        (file - 1 == targetFile || file + 1 == targetFile)) {
                        return true;
                    }
                } else {
                    if (rank + 1 == targetRank && 
                        (file - 1 == targetFile || file + 1 == targetFile)) {
                        return true;
                    }
                }
                break;
            }
            case 'n': {
                int rank = i / 8;
                int file = i % 8;
                int targetRank = square / 8;
                int targetFile = square % 8;
                
                int rankDiff = std::abs(rank - targetRank);
                int fileDiff = std::abs(file - targetFile);
                
                if ((rankDiff == 1 && fileDiff == 2) || (rankDiff == 2 && fileDiff == 1)) {
                    return true;
                }
                break;
            }
            case 'b': {
                int rank = i / 8;
                int file = i % 8;
                int targetRank = square / 8;
                int targetFile = square % 8;
                
                int rankDiff = targetRank - rank;
                int fileDiff = targetFile - file;
                
                if (std::abs(rankDiff) == std::abs(fileDiff)) {
                    int rankStep = (rankDiff > 0) ? 1 : ((rankDiff < 0) ? -1 : 0);
                    int fileStep = (fileDiff > 0) ? 1 : ((fileDiff < 0) ? -1 : 0);
                    
                    bool blocked = false;
                    for (int r = rank + rankStep, f = file + fileStep; 
                         r != targetRank || f != targetFile; 
                         r += rankStep, f += fileStep) {
                        if (position.boardState[r*8+f] != ' ') {
                            blocked = true;
                            break;
                        }
                    }
                    if (!blocked) return true;
                }
                break;
            }
            case 'r': {
                int rank = i / 8;
                int file = i % 8;
                int targetRank = square / 8;
                int targetFile = square % 8;
                
                if (rank == targetRank || file == targetFile) {
                    int rankStep = (rank == targetRank) ? 0 : ((targetRank > rank) ? 1 : -1);
                    int fileStep = (file == targetFile) ? 0 : ((targetFile > file) ? 1 : -1);
                    
                    bool blocked = false;
                    for (int r = rank + rankStep, f = file + fileStep; 
                         r != targetRank || f != targetFile; 
                         r += rankStep, f += fileStep) {
                        if (position.boardState[r*8+f] != ' ') {
                            blocked = true;
                            break;
                        }
                    }
                    if (!blocked) return true;
                }
                break;
            }
            case 'q': {
                int rank = i / 8;
                int file = i % 8;
                int targetRank = square / 8;
                int targetFile = square % 8;
                
                int rankDiff = targetRank - rank;
                int fileDiff = targetFile - file;
                
                if (rank == targetRank || file == targetFile || std::abs(rankDiff) == std::abs(fileDiff)) {
                    int rankStep = (rank == targetRank) ? 0 : ((targetRank > rank) ? 1 : -1);
                    int fileStep = (file == targetFile) ? 0 : ((targetFile > file) ? 1 : -1);
                    
                    bool blocked = false;
                    for (int r = rank + rankStep, f = file + fileStep; 
                         r != targetRank || f != targetFile; 
                         r += rankStep, f += fileStep) {
                        if (position.boardState[r*8+f] != ' ') {
                            blocked = true;
                            break;
                        }
                    }
                    if (!blocked) return true;
                }
                break;
            }
            case 'k': {
                int rank = i / 8;
                int file = i % 8;
                int targetRank = square / 8;
                int targetFile = square % 8;
                
                int rankDiff = std::abs(rank - targetRank);
                int fileDiff = std::abs(file - targetFile);
                
                if (rankDiff <= 1 && fileDiff <= 1) {
                    return true;
                }
                break;
            }
        }
    }
    return false;
}

int GetCheapestAttackerValue(const BoardPosition& position, int square, bool byWhite) {
    int cheapestValue = 10000; 
    
    BoardPosition tempPos = position;
    tempPos.whiteToMove = byWhite;
    std::vector<Move> opponentMoves = GenerateMoves(tempPos, byWhite);
    
    for (const Move& move : opponentMoves) {
        if (move.notation.length() >= 5) {
            int endPos = std::stoi(move.notation.substr(3, 2));
            if (endPos == square) { 
                int startPos = std::stoi(move.notation.substr(1, 2));
                char attacker = position.boardState[startPos];
                int attackerValue = GetPieceValue(attacker);
                
                if (attackerValue < cheapestValue) {
                    cheapestValue = attackerValue;
                }
            }
        }
    }
    
    return (cheapestValue < 10000) ? cheapestValue : 0;
}

bool HasMaterialThreat(const BoardPosition& position, bool forWhite) {
    std::vector<Move> opponentMoves = GenerateMoves(position, !forWhite);
    
    for (const Move& move : opponentMoves) {
        int endPos = std::stoi(move.notation.substr(move.notation.length() - 2));
        char target = position.boardState[endPos];
        
        if ((forWhite && islower(target) && tolower(target) != 'p') ||
            (!forWhite && isupper(target) && toupper(target) != 'P')) {
            return true;
        }
    }
    
    return false;
}

int MinimaxOnTree(MoveTreeNode* node, int depth, int alpha, int beta, bool maximizingPlayer, bool allowNullMove) {
    static int nodeCount = 0;
    if (++nodeCount % 1000 == 0) {
        static auto startTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();
        if (elapsedMs > 10000) {
            throw std::runtime_error("Time limit exceeded");
        }
    }
    
    if (node->isEvaluated) {
        return node->evaluation;
    }

    Move ttMove;
    int ttScore;
    if (ProbeTranspositionTable(node->boardState, depth, alpha, beta, ttScore, ttMove)) {
        return ttScore;
    }

    if (depth <= 0) {
        BoardPosition tempPosition;
        tempPosition.boardState = node->boardState;
        ChessPersonality savedPersonality = currentPersonality;
        if (depth < -2) currentPersonality = STANDARD;
        
        node->evaluation = Quiescence(tempPosition, alpha, beta, maximizingPlayer, 3);
        
        currentPersonality = savedPersonality;
        node->isEvaluated = true;
        
        int flag = (node->evaluation <= alpha) ? TT_ALPHA : 
                  ((node->evaluation >= beta) ? TT_BETA : TT_EXACT);
        StoreTranspositionTable(node->boardState, depth, flag, node->evaluation, Move());
        return node->evaluation;
    }

    if (node->children.empty()) {
        BoardPosition tempPosition;
        tempPosition.boardState = node->boardState;
        ExpandNode(node, 1, maximizingPlayer, tempPosition);
        
        if (node->children.empty()) {
            bool isInCheck = IsKingInCheck(tempPosition, maximizingPlayer);
            node->evaluation = isInCheck ? 
                (maximizingPlayer ? -100000 + depth * 100 : 100000 - depth * 100) : 0;
            node->isEvaluated = true;
            return node->evaluation;
        }
    }

    std::vector<Move> moves;
    for (const auto& child : node->children) {
        moves.push_back(child->move);
    }
    OrderMoves(moves, depth, node->boardState, ttMove);

    int bestValue = -2147483647;
    Move bestMove;
    int nodeFlag = TT_ALPHA;

    BoardPosition currentPosition;
    currentPosition.boardState = node->boardState;
    currentPosition.whiteToMove = maximizingPlayer;

    for (int i = 0; i < node->children.size(); i++) {
        MoveTreeNode* childNode = node->children[i];
        BoardPosition tempPosition;
        tempPosition.boardState = childNode->boardState;
        
        if (!IsCapture(tempPosition.boardState, childNode->move) && 
            !IsMoveSafe(currentPosition, childNode->move)) {
            
            int blunderScore = maximizingPlayer ? -5000 : 5000;
            
            if (depth >= 3) {
                std::string moveText = ConvertToAlgebraic(childNode->move, currentPosition);
                std::cout << "Detected blunder: " << moveText << " at depth " << depth << std::endl;
            }
            
            childNode->evaluation = blunderScore;
            childNode->isEvaluated = true;
            
            int eval = -blunderScore;
            
            if (eval > bestValue) {
                bestValue = eval;
                bestMove = childNode->move;
                
                if (bestValue > alpha) {
                    alpha = bestValue;
                    nodeFlag = TT_EXACT;
                    
                    if (alpha >= beta) {
                        nodeFlag = TT_BETA;
                        break;
                    }
                }
            }
            
            continue;
        }
        
        int eval;
        if (i >= 2 && depth >= 3 && !IsCapture(tempPosition.boardState, childNode->move) && !IsCheck(tempPosition, childNode->move)) {
            int R = 1 + customMin(depth / 2, 3) + customMin(i / 5, 3);
            eval = -MinimaxOnTree(childNode, depth - 1 - R, -beta, -alpha, !maximizingPlayer, false);
            
            if (eval > alpha && eval < beta) {
                eval = -MinimaxOnTree(childNode, depth - 1, -beta, -alpha, !maximizingPlayer, false);
            }
        } else {
            eval = -MinimaxOnTree(childNode, depth - 1, -beta, -alpha, !maximizingPlayer, false);
        }
        
        if (eval > bestValue) {
            bestValue = eval;
            bestMove = childNode->move;
            
            if (bestValue > alpha) {
                alpha = bestValue;
                nodeFlag = TT_EXACT;
                
                if (!IsCapture(tempPosition.boardState, childNode->move)) {
                    StoreKillerMove(childNode->move, depth);
                }
                
                if (alpha >= beta) {
                    nodeFlag = TT_BETA;
                    break;
                }
            }
        }
    }
    
    node->evaluation = bestValue;
    node->isEvaluated = true;
    StoreTranspositionTable(node->boardState, depth, nodeFlag, bestValue, bestMove);
    return bestValue;
}

bool IsCapture(const std::string& boardState, const Move& move) {
    if (move.notation.empty() || move.notation.length() < 3) 
        return false;
        
    int endPos = std::stoi(move.notation.substr(move.notation.length() - 2));
    return boardState[endPos] != ' ' || move.isEnPassant;
}

bool IsCheck(const BoardPosition& position, const Move& move) {
    BoardPosition newPosition = ApplyMove(position, move);

    char piece = move.notation[0];
    bool isWhitePiece = isupper(piece);

    return IsKingInCheck(newPosition, !isWhitePiece);
}

bool IsDraw(const std::string& boardState) {
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
    
    if (whitePieceCount == 0 && blackPieceCount == 0)
        return true;
        
    if ((whitePieceCount == 1 && whiteHasMinor && blackPieceCount == 0) ||
        (blackPieceCount == 1 && blackHasMinor && whitePieceCount == 0))
        return true;
    
    return false;
}

BoardPosition ApplyMove(const BoardPosition& position, const Move& move) {
    if (move.notation.length() < 5) {
        std::cerr << "Warning: Invalid move notation: '" << move.notation 
                  << "', returning unchanged position" << std::endl;
        return position;
    }
    BoardPosition newPosition = position;
    const int BOARD_SIZE = 8;

    char piece = move.notation[0];
    std::string startPosStr = move.notation.substr(1, 2);
    std::string endPosStr = move.notation.substr(3, 2);
    int startPos = std::stoi(startPosStr);
    int endPos = std::stoi(endPosStr);

    newPosition.boardState[endPos] = newPosition.boardState[startPos];
    newPosition.boardState[startPos] = ' ';

    if (move.isEnPassant) {
        newPosition.boardState[move.enPassantCapturePos] = ' ';
    }

    if (move.isCastling) {
        if (move.isKingsideCastling) {
            if (position.whiteToMove) {
                newPosition.boardState[63] = ' ';
                newPosition.boardState[61] = 'R';
            } else {
                newPosition.boardState[7] = ' ';
                newPosition.boardState[5] = 'r';
            }
        } else {
            if (position.whiteToMove) {
                newPosition.boardState[56] = ' ';
                newPosition.boardState[59] = 'R';
            } else {
                newPosition.boardState[0] = ' ';
                newPosition.boardState[3] = 'r';
            }
        }
    }

    if (piece == 'K') {
        newPosition.whiteCanCastleKingside = false;
        newPosition.whiteCanCastleQueenside = false;
    } else if (piece == 'k') {
        newPosition.blackCanCastleKingside = false;
        newPosition.blackCanCastleQueenside = false;
    }
    if (startPos == 0 || endPos == 0) newPosition.whiteCanCastleQueenside = false;
    if (startPos == 7 || endPos == 7) newPosition.whiteCanCastleKingside = false;
    if (startPos == 56 || endPos == 56) newPosition.blackCanCastleQueenside = false;
    if (startPos == 63 || endPos == 63) newPosition.blackCanCastleKingside = false;

    newPosition.enPassantTargetSquare = -1;
    if ((piece == 'P' && startPos / 8 == 6 && endPos / 8 == 4) ||
        (piece == 'p' && startPos / 8 == 1 && endPos / 8 == 3)) {
        newPosition.enPassantTargetSquare = (startPos + endPos) / 2;
    }

    if (tolower(piece) == 'p' || (position.boardState[endPos] != ' ')) {
        newPosition.halfMoveClock = 0;
    } else {
        newPosition.halfMoveClock++;
    }

    if (!position.whiteToMove) {
        newPosition.fullMoveNumber++;
    }
    newPosition.whiteToMove = !position.whiteToMove;

    if (move.notation.length() > 5) {
        char promotionPiece = move.notation[5];
        if ((piece == 'P' && endPos / 8 == 0) || (piece == 'p' && endPos / 8 == 7)) {
            newPosition.boardState[endPos] = promotionPiece;
        }
    }

    return newPosition;
}

bool IsKingInCheck(const BoardPosition& position, bool isWhiteKing) {
    const std::string& boardState = position.boardState;
    char kingChar = isWhiteKing ? 'K' : 'k';

    int kingPos = -1;
    for (size_t i = 0; i < boardState.size(); i++) {
        if (boardState[i] == kingChar) {
            kingPos = i;
            break;
        }
    }
    if (kingPos == -1) return false;

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
        return EvaluateBoard(position, 0);
    }

    std::vector<Move> moves = GenerateMoves(position, maximizingPlayer);

    if (moves.empty()) {
        return maximizingPlayer ? -20000 : 20000;
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

bool IsValidMove(const std::string& boardState, int row, int col, bool isWhite) {
    const int BOARD_SIZE = 8;
    if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE) {
        return false;
    }

    int pos = row * BOARD_SIZE + col;
    char piece = boardState[pos];

    if (piece == ' ') {
        return true;
    }

    return (isWhite && islower(piece)) || (!isWhite && isupper(piece));
}

void AddMove(const std::string& boardState, int startPos, int endPos, char piece, 
             std::vector<Move>& moves) {
    std::string startPosStr = (startPos < 10) ? "0" + std::to_string(startPos) : std::to_string(startPos);
    std::string endPosStr = (endPos < 10) ? "0" + std::to_string(endPos) : std::to_string(endPos);
    
    std::string notation = piece + startPosStr + endPosStr;

    moves.push_back({notation});
}

void GeneratePawnMoves(const std::string& boardState, int row, int col, int pos,
    bool isWhite, std::vector<Move>& moves, int enPassantCol)
{
    const int BOARD_SIZE = 8;
    char piece = boardState[pos];
    int direction = isWhite ? -1 : 1;
    bool lastRank = (isWhite && row + direction == 0) || (!isWhite && row + direction == 7);

    int newRow = row + direction;
    if (newRow >= 0 && newRow < BOARD_SIZE) {
        int newPos = newRow * BOARD_SIZE + col;
        if (boardState[newPos] == ' ') {
            AddMove(boardState, pos, newPos, piece, moves);

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

    for (int dc : {-1, 1}) {
        int newCol = col + dc;
        newRow = row + direction;
        if (newRow >= 0 && newRow < BOARD_SIZE && newCol >= 0 && newCol < BOARD_SIZE) {
            int newPos = newRow * BOARD_SIZE + newCol;
            char targetPiece = boardState[newPos];

            bool isOpponentPiece = (isWhite && islower(targetPiece)) || (!isWhite && isupper(targetPiece));
            if (targetPiece != ' ' && isOpponentPiece) {
                AddMove(boardState, pos, newPos, piece, moves);
            }
        }
    }

    if ((isWhite && row == 3) || (!isWhite && row == 4)) {
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
            
            if (boardState[newPos] != ' ') {
                break;
            }
            
            newRow += dir.first;
            newCol += dir.second;
        }
    }
}

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
            
            if (boardState[newPos] != ' ') {
                break;
            }
            
            newRow += dir.first;
            newCol += dir.second;
        }
    }
}

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

    auto posStr = [](int p) { return (p < 10 ? "0" : "") + std::to_string(p); };

    if (!skipCastlingCheck && ((isWhite && row == 7 && col == 4) || (!isWhite && row == 0 && col == 4))) {
        int baseRow = isWhite ? 7 : 0;
        char rook = isWhite ? 'R' : 'r';

        bool canCastleKingside = isWhite ? position.whiteCanCastleKingside : position.blackCanCastleKingside;
        if (canCastleKingside &&
            boardState[baseRow * 8 + 5] == ' ' &&
            boardState[baseRow * 8 + 6] == ' ' &&
            boardState[baseRow * 8 + 7] == rook) {
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

        bool canCastleQueenside = isWhite ? position.whiteCanCastleQueenside : position.blackCanCastleQueenside;
        if (canCastleQueenside &&
            boardState[baseRow * 8 + 1] == ' ' &&
            boardState[baseRow * 8 + 2] == ' ' &&
            boardState[baseRow * 8 + 3] == ' ' &&
            boardState[baseRow * 8 + 0] == rook) {
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

int EvaluateBoard(const BoardPosition& position, int searchDepth) {
    const std::string& boardState = position.boardState;
    const int BOARD_SIZE = 8;
    uint64_t key = GetZobristKey(position.boardState);
    
    auto it = evaluationCache.find(key);
    if (it != evaluationCache.end()) {
        return it->second;
    }
    int score = 0;
    
    const int PAWN_VALUE = 100;
    const int KNIGHT_VALUE = 320;
    const int BISHOP_VALUE = 330;
    const int ROOK_VALUE = 500;
    const int QUEEN_VALUE = 900;
    const int KING_VALUE = 20000;

    int whitePawns[8] = {0};
    int blackPawns[8] = {0};
    bool whiteHasBishopPair = false;
    bool blackHasBishopPair = false;
    int whiteBishops = 0, blackBishops = 0;
    int whitePawnCount = 0, blackPawnCount = 0;
    int whiteRookCount = 0, blackRookCount = 0;
    int whiteKnightCount = 0, blackKnightCount = 0;
    int whiteQueenCount = 0, blackQueenCount = 0;
    int whiteKingPos = -1, blackKingPos = -1;
    
    int totalMaterial = 0;
    
    std::vector<int> whitePawnPositions;
    std::vector<int> blackPawnPositions;
    std::vector<int> whiteRookPositions;
    std::vector<int> blackRookPositions;
    std::vector<int> whiteKnightPositions;
    std::vector<int> blackKnightPositions;
    std::vector<int> whiteBishopPositions;
    std::vector<int> blackBishopPositions;
    
    for (size_t i = 0; i < boardState.size(); i++) {
        char piece = boardState[i];
        int file = i % 8;
        int rank = i / 8;
        int pos = rank * BOARD_SIZE + file;
        
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
    
    int gamePhase = 0;
    const int OPENING_THRESHOLD = 5000;
    const int MIDDLEGAME_THRESHOLD = 3000;
    
    if (totalMaterial <= MIDDLEGAME_THRESHOLD) {
        gamePhase = 2;
    } else if (totalMaterial <= OPENING_THRESHOLD) {
        gamePhase = 1;
    }
    
    if (gamePhase == 0) {
        int openingScore = EvaluateOpeningPrinciples(position);
        
        if (position.whiteToMove && openingScore < -1000) {
            return openingScore;
        }
        else if (!position.whiteToMove && openingScore > 1000) {
            return openingScore;
        }
        
        score += openingScore;
    }
    
    if (gamePhase == 0) {
        for (int i = 0; i < 64; i++) {
            int rank = i / 8;
            int file = i % 8;
            
            bool isCentralSquare = (rank >= 3 && rank <= 4 && file >= 3 && file <= 4);
            
            bool isNearCentralSquare = (rank >= 2 && rank <= 5 && file >= 2 && file <= 5) && !isCentralSquare;
            
            char piece = boardState[i];
            if (piece != ' ') {
                bool isWhitePiece = isupper(piece);
                
                if (isCentralSquare) {
                    if (isWhitePiece) {
                        score += 25;
                    } else {
                        score -= 25;
                    }
                }
                else if (isNearCentralSquare) {
                    if (isWhitePiece) {
                        score += 10;
                    } else {
                        score -= 10;
                    }
                }
            }
        }
    }
    
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
    
    for (int pawnPos : whitePawnPositions) {
        score += pawnTable[pawnPos];
    }
    for (int pawnPos : blackPawnPositions) {
        score -= pawnTable[63 - pawnPos];
    }
    
    for (int knightPos : whiteKnightPositions) {
        score += knightTable[knightPos];
    }
    for (int knightPos : blackKnightPositions) {
        score -= knightTable[63 - knightPos];
    }
    
    for (int bishopPos : whiteBishopPositions) {
        score += bishopTable[bishopPos];
    }
    for (int bishopPos : blackBishopPositions) {
        score -= bishopTable[63 - bishopPos];
    }
    
    for (int rookPos : whiteRookPositions) {
        score += rookTable[rookPos];
    }
    for (int rookPos : blackRookPositions) {
        score -= rookTable[63 - rookPos];
    }
    
    if (whiteKingPos >= 0) {
        if (gamePhase < 2) {
            score += kingMiddlegameTable[whiteKingPos];
        } else {
            score += kingEndgameTable[whiteKingPos] * 2;
        }
    }
    
    if (blackKingPos >= 0) {
        if (gamePhase < 2) {
            score -= kingMiddlegameTable[63 - blackKingPos];
        } else {
            score -= kingEndgameTable[63 - blackKingPos] * 2;
        }
    }
    
    for (int knightPos : whiteKnightPositions) {
        int file = knightPos % 8;
        int rank = knightPos / 8;
        
        if (file == 0 || file == 7) {
            score -= 50;
            if (rank == 2 || rank == 5) {
                score -= 30;
            }
        }
        
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
        
        score += actualMobility * 4;
    }
    
    for (int knightPos : blackKnightPositions) {
        int file = knightPos % 8;
        int rank = knightPos / 8;
        
        if (file == 0 || file == 7) {
            score += 100;
            if (rank == 2 || rank == 5) {
                score += 50;
            }
        }
        
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
        
        score -= actualMobility * 4;
    }
    
    if (whiteBishops >= 2) {
        score += 50;
        whiteHasBishopPair = true;
    }
    if (blackBishops >= 2) {
        score -= 50;
        blackHasBishopPair = true;
    }
    
    for (int bishopPos : whiteBishopPositions) {
        if ((bishopPos == 56 || bishopPos == 63) && 
            (boardState[bishopPos - 8] == 'P' || boardState[bishopPos - 9] == 'P' || boardState[bishopPos - 7] == 'P')) {
            score -= 100;
        }
    }
    
    for (int bishopPos : blackBishopPositions) {
        if ((bishopPos == 0 || bishopPos == 7) && 
            (boardState[bishopPos + 8] == 'p' || boardState[bishopPos + 9] == 'p' || boardState[bishopPos + 7] == 'p')) {
            score += 100;
        }
    }
    
    for (int knightPos : whiteKnightPositions) {
        int rank = knightPos / 8;
        int file = knightPos % 8;
        
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
                score += 25;
            }
        }
    }
    
    for (int knightPos : blackKnightPositions) {
        int rank = knightPos / 8;
        int file = knightPos % 8;
        
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
                score -= 25;
            }
        }
    }
    
    for (int file = 0; file < 8; file++) {
        if (whitePawns[file] > 1) {
            score -= 20 * (whitePawns[file] - 1);
        }
        if (blackPawns[file] > 1) {
            score += 20 * (blackPawns[file] - 1);
        }
    }
    
    for (int pawnPos : whitePawnPositions) {
        int file = pawnPos % 8;
        int rank = pawnPos / 8;
        
        bool isIsolated = true;
        if (file > 0 && whitePawns[file-1] > 0) isIsolated = false;
        if (file < 7 && whitePawns[file+1] > 0) isIsolated = false;
        
        if (isIsolated) {
            score -= 15;
        }
        
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
            int passedBonus = 20 + (7 - rank) * 10;
            if (gamePhase == 2) passedBonus *= 2;
            score += passedBonus;
        }
        
        if (gamePhase < 2 && whiteKingPos >= 0) {
            int kingFile = whiteKingPos % 8;
            if (abs(file - kingFile) <= 1 && rank < 6) {
                score -= (6 - rank) * 5;
            }
        }
    }
    
    for (int pawnPos : blackPawnPositions) {
        int file = pawnPos % 8;
        int rank = pawnPos / 8;
        
        bool isIsolated = true;
        if (file > 0 && blackPawns[file-1] > 0) isIsolated = false;
        if (file < 7 && blackPawns[file+1] > 0) isIsolated = false;
        
        if (isIsolated) {
            score += 15;
        }
        
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
            int passedBonus = 20 + rank * 10;
            if (gamePhase == 2) passedBonus *= 2;
            score -= passedBonus;
        }
        
        if (gamePhase < 2 && blackKingPos >= 0) {
            int kingFile = blackKingPos % 8;
            if (abs(file - kingFile) <= 1 && rank > 1) {
                score += (rank - 1) * 5;
            }
        }
    }
    
    for (int rookPos : whiteRookPositions) {
        int file = rookPos % 8;
        
        if (whitePawns[file] == 0 && blackPawns[file] == 0) {
            score += 25;
        }
        else if (whitePawns[file] == 0) {
            score += 15;
        }
        
        if (rookPos / 8 == 1 && blackKingPos / 8 == 0) {
            score += 30;
        }
    }
    
    for (int rookPos : blackRookPositions) {
        int file = rookPos % 8;
        
        if (whitePawns[file] == 0 && blackPawns[file] == 0) {
            score -= 25;
        }
        else if (blackPawns[file] == 0) {
            score -= 15;
        }
        
        if (rookPos / 8 == 6 && whiteKingPos / 8 == 7) {
            score -= 30;
        }
    }
    
    if (gamePhase < 2) {
        if (whiteKingPos >= 0) {
            int kingFile = whiteKingPos % 8;
            int kingRank = whiteKingPos / 8;
            
            int pawnShieldCount = 0;
            for (int f = customMax(0, kingFile-1); f <= customMin(7, kingFile+1); f++) {
                for (int r = customMax(0, kingRank-2); r <= kingRank-1; r++) {
                    if (boardState[r*8+f] == 'P') {
                        pawnShieldCount++;
                    }
                }
            }
            
            score += pawnShieldCount * 10;
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
    
    if (gamePhase == 0) {
        if (boardState[57] == 'N') score -= 15;
        if (boardState[62] == 'N') score -= 15;
        if (boardState[58] == 'B') score -= 10;
        if (boardState[61] == 'B') score -= 10;
        
        if (boardState[1] == 'n') score += 15;
        if (boardState[6] == 'n') score += 15; 
        if (boardState[2] == 'b') score += 10; 
        if (boardState[5] == 'b') score += 10; 
        
        for (size_t i = 0; i < boardState.size(); i++) {
            if (boardState[i] == 'Q' && i != 59) {
                int moveCount = position.fullMoveNumber;
                if (moveCount < 5) {
                    score -= 30;
                }
            }
            if (boardState[i] == 'q' && i != 3) {
                int moveCount = position.fullMoveNumber;
                if (moveCount < 5) {
                    score += 30; 
                }
            }
        }
        
        if (position.fullMoveNumber <= 2 && !position.whiteToMove) {
            if (boardState[36] == 'P' && boardState[52] == ' ') {
                if (boardState[12] == 'p') {
                    score -= 80; 
                }
                if (boardState[10] == 'p') {
                    score -= 75; 
                }
                if (boardState[20] == 'p') {
                    score -= 70;
                }
                if (boardState[18] == 'p') {
                    score -= 70;
                }
            }
        }
    }
    
    int whiteMobility = 0, blackMobility = 0;
    
    std::vector<Move> whiteMoves = GenerateMoves(position, true, true);
    std::vector<Move> blackMoves = GenerateMoves(position, false, true);
    
    whiteMobility = whiteMoves.size() - whiteKnightPositions.size() * 8;
    blackMobility = blackMoves.size() - blackKnightPositions.size() * 8;
    
    if (gamePhase == 0) {
        score += whiteMobility * 2;
        score -= blackMobility * 2;
    } else if (gamePhase == 1) {
        score += whiteMobility * 3;
        score -= blackMobility * 3;
    } else {
        score += whiteMobility * 2;
        score -= blackMobility * 2;
    }
    
    if (IsKingInCheck(position, false)) {
        score += 50;
    }
    if (IsKingInCheck(position, true)) {
        score -= 50;
    }

    if (currentPersonality != STANDARD) {
        bool useFullPersonality = (searchDepth <= 2);
        
        std::vector<Move>* movesToPass = nullptr;
        if (useFullPersonality && (currentPersonality == AGGRESSIVE || currentPersonality == DYNAMIC)) {
            static std::vector<Move> currentMoves;
            currentMoves = GenerateMoves(position, position.whiteToMove);
            movesToPass = &currentMoves;
        }
        
        score = ApplyPersonalityToEvaluation(score, position, movesToPass, useFullPersonality);
    }

    if (evaluationCache.size() < MAX_EVAL_CACHE_SIZE) {
        evaluationCache[key] = score;
    }

    return score;
}

BoardPosition ParseMoveHistory(const std::string& moveHistory) {
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
    
    std::vector<std::string> moves;
    std::istringstream moveStream(moveHistory);
    std::string moveStr;
    while (moveStream >> moveStr) {
        moves.push_back(moveStr);
    }
    
    for (const std::string& move : moves) {
        if (move.empty()) continue;
        try {
            Move internalMove = AlgebraicToInternalMove(move, position);
            position = ApplyMove(position, internalMove);
        } catch (const std::exception& e) {
            std::cerr << "Error applying move '" << move << "': " << e.what() << std::endl;
            throw;
        }
    }
    
    return position;
}

BoardPosition ApplyAlgebraicMove(const BoardPosition& position, const std::string& algebraicMove) {
    BoardPosition newPosition = position;
    
    if (algebraicMove == "O-O" || algebraicMove == "0-0") {
        if (position.whiteToMove) {
            newPosition.boardState[60] = ' ';
            newPosition.boardState[62] = 'K';
            newPosition.boardState[63] = ' ';
            newPosition.boardState[61] = 'R';
            newPosition.whiteCanCastleKingside = false;
            newPosition.whiteCanCastleQueenside = false;
        } else {
            newPosition.boardState[4] = ' ';
            newPosition.boardState[6] = 'k';
            newPosition.boardState[7] = ' ';
            newPosition.boardState[5] = 'r';
            newPosition.blackCanCastleKingside = false;
            newPosition.blackCanCastleQueenside = false;
        }
    } else if (algebraicMove == "O-O-O" || algebraicMove == "0-0-0") {
        if (position.whiteToMove) {
            newPosition.boardState[60] = ' ';
            newPosition.boardState[58] = 'K';
            newPosition.boardState[56] = ' ';
            newPosition.boardState[59] = 'R';
            newPosition.whiteCanCastleKingside = false;
            newPosition.whiteCanCastleQueenside = false;
        } else {
            newPosition.boardState[4] = ' ';
            newPosition.boardState[2] = 'k';
            newPosition.boardState[0] = ' ';
            newPosition.boardState[3] = 'r';
            newPosition.blackCanCastleKingside = false;
            newPosition.blackCanCastleQueenside = false;
        }
    } else {
        std::string moveWithoutCapture = algebraicMove;
        bool isCapture = false;
        
        size_t capturePos = moveWithoutCapture.find('x');
        if (capturePos != std::string::npos) {
            isCapture = true;
            moveWithoutCapture.erase(capturePos, 1);
        }
        
        char promotionPiece = '\0';
        size_t equalPos = moveWithoutCapture.find('=');
        if (equalPos != std::string::npos) {
            promotionPiece = moveWithoutCapture[equalPos + 1];
            moveWithoutCapture.erase(equalPos);
        } else if (moveWithoutCapture.length() > 2 && moveWithoutCapture[moveWithoutCapture.length() - 2] == '=') {
            promotionPiece = moveWithoutCapture.back();
            moveWithoutCapture.erase(moveWithoutCapture.length() - 2);
        }
        
        char movingPiece;
        std::string from, to;
        
        if (isalpha(moveWithoutCapture[0]) && isupper(moveWithoutCapture[0]) && moveWithoutCapture[0] != 'K') {
            movingPiece = moveWithoutCapture[0];
            moveWithoutCapture.erase(0, 1);
        } else {
            movingPiece = position.whiteToMove ? 'P' : 'p';
        }
        
        if (moveWithoutCapture.length() > 2) {
            int disambigLength = moveWithoutCapture.length() - 2;
            from = moveWithoutCapture.substr(0, disambigLength);
            to = moveWithoutCapture.substr(disambigLength);
        } else {
            to = moveWithoutCapture;
            
            int toSquare = AlgebraicToIndex(to);
            
            char searchPiece = position.whiteToMove ? movingPiece : tolower(movingPiece);
            
            std::vector<Move> possibleMoves = GenerateMoves(position, position.whiteToMove);
            for (const Move& move : possibleMoves) {
                int endPos = std::stoi(move.notation.substr(3, 2));
                if (endPos == toSquare && move.notation[0] == searchPiece) {
                    int fromPos = std::stoi(move.notation.substr(1, 2));
                    from = IndexToAlgebraic(fromPos);
                    break;
                }
            }
            
            if (from.empty()) {
                throw std::runtime_error("Cannot find source square for move: " + algebraicMove);
            }
        }
        
        int fromIndex = AlgebraicToIndex(from);
        int toIndex = AlgebraicToIndex(to);
        
        bool isEnPassant = false;
        int enPassantCapturePos = -1;
        
        if (tolower(movingPiece) == 'p' && (fromIndex % 8) != (toIndex % 8) && newPosition.boardState[toIndex] == ' ') {
            isEnPassant = true;
            if (position.whiteToMove) {
                enPassantCapturePos = toIndex + 8;
            } else {
                enPassantCapturePos = toIndex - 8;
            }
        }
        
        newPosition.boardState[toIndex] = position.boardState[fromIndex];
        newPosition.boardState[fromIndex] = ' ';
        
        if (isEnPassant && enPassantCapturePos >= 0) {
            newPosition.boardState[enPassantCapturePos] = ' ';
        }
        
        if (promotionPiece != '\0') {
            if (position.whiteToMove) {
                newPosition.boardState[toIndex] = toupper(promotionPiece);
            } else {
                newPosition.boardState[toIndex] = tolower(promotionPiece);
            }
        }
        
        if (movingPiece == 'K') {
            newPosition.whiteCanCastleKingside = false;
            newPosition.whiteCanCastleQueenside = false;
        } else if (movingPiece == 'k') {
            newPosition.blackCanCastleKingside = false;
            newPosition.blackCanCastleQueenside = false;
        } else if (movingPiece == 'R') {
            if (fromIndex == 56) {
                newPosition.whiteCanCastleQueenside = false;
            } else if (fromIndex == 63) {
                newPosition.whiteCanCastleKingside = false;
            }
        } else if (movingPiece == 'r') {
            if (fromIndex == 0) {
                newPosition.blackCanCastleQueenside = false;
            } else if (fromIndex == 7) {
                newPosition.blackCanCastleKingside = false;
            }
        }
        
        if (toIndex == 56) newPosition.whiteCanCastleQueenside = false;
        if (toIndex == 63) newPosition.whiteCanCastleKingside = false;
        if (toIndex == 0) newPosition.blackCanCastleQueenside = false;
        if (toIndex == 7) newPosition.blackCanCastleKingside = false;
        
        newPosition.enPassantTargetSquare = -1;
        if (tolower(movingPiece) == 'p') {
            int rankDiff = abs(fromIndex / 8 - toIndex / 8);
            if (rankDiff == 2) {
                newPosition.enPassantTargetSquare = position.whiteToMove ? 
                    (fromIndex - 8) : (fromIndex + 8);
            }
        }

        if (tolower(movingPiece) == 'p' || isCapture) {
            newPosition.halfMoveClock = 0;
        } else {
            newPosition.halfMoveClock++;
        }
    }
    
    newPosition.whiteToMove = !position.whiteToMove;
    if (!position.whiteToMove) {
        newPosition.fullMoveNumber++;
    }
    
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
    if (move.notation.length() < 5) {
        return "error";
    }
    
    int startPos;
    int endPos;
    
    try {
        startPos = std::stoi(move.notation.substr(1, move.notation.length() > 3 ? move.notation.length() - 3 : 1));
        endPos = std::stoi(move.notation.substr(move.notation.length() - 2, 2));
    } catch (const std::exception& e) {
        std::cerr << "Error parsing move notation: " << move.notation << " - " << e.what() << std::endl;
        return "error";
    }

    std::string startSquare = IndexToAlgebraic(startPos);
    std::string endSquare = IndexToAlgebraic(endPos);

    char piece = !move.notation.empty() ? move.notation[0] : 'P';
    bool isWhite = isupper(piece);

    char target = position.boardState[endPos];
    bool isOpponentPiece = (target != ' ' &&
        ((isWhite && islower(target)) ||
            (!isWhite && isupper(target))));

    bool isCapture = isOpponentPiece || move.isEnPassant;

    std::string algebraic;

    if (piece != 'P' && piece != 'p') {
        algebraic += toupper(piece);
    }

    algebraic += startSquare;

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
    if (algebraicMove.empty()) {
        throw std::invalid_argument("Empty move string provided");
    }
    
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
        move.notation = (position.whiteToMove ? "K" : "k") + std::string(position.whiteToMove ? "6058" : "0402");
        return move;
    }

    std::string moveWithoutCapture = algebraicMove;
    size_t capturePos = moveWithoutCapture.find('x');
    if (capturePos != std::string::npos) {
        moveWithoutCapture.erase(capturePos, 1);
    }

    if (algebraicMove.length() >= 5 && isalpha(algebraicMove[0]) && 
        !isdigit(moveWithoutCapture[1]) && isalpha(moveWithoutCapture[1])) {
        
        char piece = moveWithoutCapture[0];
        std::string from = moveWithoutCapture.substr(1, 2);
        std::string to = moveWithoutCapture.substr(3, 2);
        
        try {
            int fromIndex = AlgebraicToIndex(from);
            int toIndex = AlgebraicToIndex(to);
            
            Move move;
            std::string startPosStr = (fromIndex < 10) ? "0" + std::to_string(fromIndex) : std::to_string(fromIndex);
            std::string endPosStr = (toIndex < 10) ? "0" + std::to_string(toIndex) : std::to_string(toIndex);
            move.notation = piece + startPosStr + endPosStr;
            
            return move;
        } catch (const std::exception& e) {
            throw std::invalid_argument("Error parsing piece notation: " + algebraicMove + " - " + e.what());
        }
    }
    else if (algebraicMove.length() >= 4) {
        std::string from = moveWithoutCapture.substr(0, 2);
        std::string to = moveWithoutCapture.substr(2, 2);
        
        try {
            int fromIndex = AlgebraicToIndex(from);
            int toIndex = AlgebraicToIndex(to);
            
            char piece = position.boardState[fromIndex];
            
            if (piece == ' ') {
                piece = position.whiteToMove ? 'P' : 'p';
            }
            
            Move move;
            std::string startPosStr = (fromIndex < 10) ? "0" + std::to_string(fromIndex) : std::to_string(fromIndex);
            std::string endPosStr = (toIndex < 10) ? "0" + std::to_string(toIndex) : std::to_string(toIndex);
            move.notation = piece + startPosStr + endPosStr;
            
            if (tolower(piece) == 'p' && (fromIndex % 8) != (toIndex % 8) && position.boardState[toIndex] == ' ') {
                move.isEnPassant = true;
                move.enPassantCapturePos = position.whiteToMove ? (toIndex + 8) : (toIndex - 8);
            }
            
            if (algebraicMove.length() > 4) {
                char promotionPiece = algebraicMove[4];
                if (isalpha(promotionPiece)) {
                    move.notation += promotionPiece;
                }
            }
            
            return move;
        } catch (const std::exception& e) {
            throw std::invalid_argument("Error parsing coordinate notation: " + algebraicMove + " - " + e.what());
        }
    }
    
    throw std::invalid_argument("Unsupported move format: " + algebraicMove);
}

int EvaluateOpeningPrinciples(const BoardPosition& position) {
   int score = 0;
    const std::string& boardState = position.boardState;
    int moveNum = position.fullMoveNumber;

    if (moveNum == 1 && position.whiteToMove) {
        if (boardState[52] == ' ' && boardState[36] == 'P') {
            return 5000;
        }
        
        if (boardState[51] == ' ' && boardState[35] == 'P') {
            return 4800;
        }
        
        if (boardState[62] == ' ' && boardState[45] == 'N') {
            return 4600;
        }
        
        if (boardState[50] == ' ' && boardState[34] == 'P') {
            return 4400;
        }
    }

    if (moveNum <= 3) {
        if (position.whiteToMove) {
            if (boardState[48] == ' ' && boardState[40] == 'P') {
                return -3000; 
            }
            
            if (boardState[55] == ' ' && boardState[47] == 'P') {
                return -3000;
            }
            
            if (boardState[57] == ' ' && boardState[40] == 'N') {
                return -3000;
            }
            
            if (boardState[62] == ' ' && boardState[47] == 'N') {
                return -3000;
            }
            
            for (int i = 0; i < 64; i++) {
                int file = i % 8;
                if ((file == 0 || file == 7) && boardState[i] == 'N') {
                    return -2000;
                }
            }
        }
        else {
            if (boardState[8] == ' ' && boardState[16] == 'p') {
                return 3000;
            }
            
            if (boardState[15] == ' ' && boardState[23] == 'p') {
                return 3000;
            }
            
            if (boardState[36] == 'P' && boardState[1] == ' ' && boardState[16] == 'n') {
                return 3000;
            }
            
            if (boardState[36] == 'P' && boardState[6] == ' ' && boardState[23] == 'n') {
                return 3000;
            }
            
            for (int i = 0; i < 64; i++) {
                int file = i % 8;
                if ((file == 0 || file == 7) && boardState[i] == 'n') {
                    return 2000;
                }
            }
            
            if (boardState[36] == 'P' && boardState[52] == ' ') {
                if (boardState[12] == 'p' && boardState[20] == ' ') {
                    return -800;
                }
                
                if (boardState[10] == 'p' && boardState[18] == ' ') {
                    return -750;
                }
                
                if (boardState[28] == 'p') {
                    return -700;
                }
                
                if (boardState[26] == 'p') {
                    return -700;
                }
            }
            
            if (boardState[35] == 'P' && boardState[51] == ' ') {
                if (boardState[11] == 'p' && boardState[19] == ' ') {
                    return -800;
                }
                
                if (boardState[6] == ' ' && boardState[21] == 'n') {
                    return -750;
                }
            }
        }
    }
    
    if (moveNum <= 3) {
        if (position.whiteToMove) {
            if (boardState[52] == ' ' && boardState[36] == 'P') {
                score += 500;
            }
            
            if (boardState[51] == ' ' && boardState[35] == 'P') {
                score += 450;
            }
            
            if (boardState[50] == ' ' && boardState[34] == 'P') {
                score += 400;
            }
            
            if (boardState[62] == ' ' && boardState[45] == 'N') {
                score += 450;
            }
            
            if (boardState[57] == ' ' && boardState[42] == 'N') {
                score += 400;
            }
        }
        else {
            if (boardState[36] == 'P' && boardState[52] == ' ') {
                if (boardState[20] == ' ' && boardState[12] == 'p') {
                    score -= 500;
                }
                
                if (boardState[18] == ' ' && boardState[10] == 'p') {
                    score -= 450;
                }
                
                if (boardState[20] == ' ' && boardState[28] == 'p') {
                    score -= 400;
                }
                
                if (boardState[18] == ' ' && boardState[26] == 'p') {
                    score -= 400;
                }
            }
        }
    }
    
    if (moveNum <= 10) {
        int whiteCenterControl = 0;
        int blackCenterControl = 0;
        
        int centralSquares[4] = {35, 36, 27, 28};
        
        for (int sq : centralSquares) {
            if (sq-8 >= 0 && sq-8 < 64) {
                if (boardState[sq-8] == 'P') whiteCenterControl += 2;
                if (boardState[sq-8] == 'p') blackCenterControl += 2;
            }
            if (sq+8 >= 0 && sq+8 < 64) {
                if (boardState[sq+8] == 'p') blackCenterControl += 2;
                if (boardState[sq+8] == 'P') whiteCenterControl += 2;
            }
            
            if (boardState[sq] != ' ') {
                if (isupper(boardState[sq])) whiteCenterControl += 3;
                if (islower(boardState[sq])) blackCenterControl += 3;
            }
            
            for (const auto& offset : std::vector<int>{-17, -15, -10, -6, 6, 10, 15, 17}) {
                int pos = sq + offset;
                if (pos >= 0 && pos < 64) {
                    if (boardState[pos] == 'N') whiteCenterControl += 1;
                    if (boardState[pos] == 'n') blackCenterControl += 1;
                }
            }
        }
        
        score += (whiteCenterControl - blackCenterControl) * 5;
        
        int whiteDevelopedPieces = 0;
        int blackDevelopedPieces = 0;
        
        for (int i = 0; i < 64; i++) {
            int file = i % 8;
            if (file == 0 || file == 7) {
                if (boardState[i] == 'N') score -= 40;
                if (boardState[i] == 'n') score += 40;
            }
        }
        
        if (boardState[62] != 'N') whiteDevelopedPieces++;
        if (boardState[57] != 'N') whiteDevelopedPieces++;
        if (boardState[61] != 'B') whiteDevelopedPieces++;
        if (boardState[58] != 'B') whiteDevelopedPieces++;
        
        if (boardState[1] != 'n') blackDevelopedPieces++;
        if (boardState[6] != 'n') blackDevelopedPieces++;
        if (boardState[2] != 'b') blackDevelopedPieces++;
        if (boardState[5] != 'b') blackDevelopedPieces++;
        
        score += whiteDevelopedPieces * 15;
        score -= blackDevelopedPieces * 15;
        
        bool whiteKingsideCastled = (boardState[62] == 'K' && boardState[61] == 'R');
        bool blackKingsideCastled = (boardState[6] == 'k' && boardState[5] == 'r');
        
        if (whiteKingsideCastled) score += 50;
        if (blackKingsideCastled) score -= 50;
    }
    
    return score;
}

int ApplyPersonalityToEvaluation(int baseScore, const BoardPosition& position, 
                               const std::vector<Move>* preCalculatedMoves,
                               bool fullCalculation) {
    int score = baseScore;
    const std::string& boardState = position.boardState;
    
    int whitePieceCount = 0, blackPieceCount = 0;
    int whiteCentralPieces = 0, blackCentralPieces = 0;
    int whiteDevelopedPieces = 0, blackDevelopedPieces = 0;
    int whiteAttackingPieces = 0, blackAttackingPieces = 0;
    int whitePawnStructureScore = 0, blackPawnStructureScore = 0;

    if (fullCalculation) {
        for (int i = 0; i < 64; i++) {
            char piece = boardState[i];
            int file = i % 8;
            int rank = i / 8;
            bool isCentral = (file >= 2 && file <= 5 && rank >= 2 && rank <= 5);
            bool isForwardWhite = (rank < 4);
            bool isForwardBlack = (rank > 3);

            if (piece != ' ') {
                if (isupper(piece)) whitePieceCount++;
                else blackPieceCount++;

                if (isCentral) {
                    if (isupper(piece)) whiteCentralPieces++;
                    else blackCentralPieces++;
                }

                if ((rank <= 5 && isupper(piece) && piece != 'P' && piece != 'K') ||
                    (rank >= 2 && islower(piece) && piece != 'p' && piece != 'k')) {
                    if (isupper(piece)) whiteDevelopedPieces++;
                    else blackDevelopedPieces++;
                }
                
                if ((isForwardWhite && isupper(piece)) || 
                    (isForwardBlack && islower(piece))) {
                    if (isupper(piece)) whiteAttackingPieces++;
                    else blackAttackingPieces++;
                }
            }
        }
        
        whitePawnStructureScore = EvaluatePawnStructure(position, true);
        blackPawnStructureScore = EvaluatePawnStructure(position, false);
    }

    const int PERSONALITY_FACTOR = 20;

    switch (currentPersonality) {
        case AGGRESSIVE:
            if (fullCalculation) {
                score += whiteAttackingPieces * 300 * PERSONALITY_FACTOR;
                score -= blackAttackingPieces * 300 * PERSONALITY_FACTOR;
                
                score -= (8 - whiteAttackingPieces) * 100 * PERSONALITY_FACTOR;
                score += (8 - blackAttackingPieces) * 100 * PERSONALITY_FACTOR;

                score += (whiteCentralPieces - blackCentralPieces) * 30 * PERSONALITY_FACTOR;
                
                score += whiteDevelopedPieces * 25 * PERSONALITY_FACTOR;
                score -= blackDevelopedPieces * 25 * PERSONALITY_FACTOR;
                
                if (preCalculatedMoves) {
                    for (const Move& move : *preCalculatedMoves) {
                        if (move.notation.length() >= 5) {
                            int endPos = std::stoi(move.notation.substr(3, 2));
                            char target = position.boardState[endPos];
                            if (target != ' ') {
                                int attackerValue = GetPieceValue(move.notation[0]);
                                int targetValue = GetPieceValue(target);
                                
                                if (targetValue * 1.5 >= attackerValue) {
                                    if (position.whiteToMove && islower(target)) 
                                        score += targetValue * 15 * PERSONALITY_FACTOR;
                                    else if (!position.whiteToMove && isupper(target)) 
                                        score -= targetValue * 15 * PERSONALITY_FACTOR;
                                }
                            }
                        }
                    }
                }
                
                for (int i = 0; i < 64; i++) {
                    char piece = position.boardState[i];
                    int file = i % 8;
                    int rank = i / 8;
                    
                    if ((piece == 'R' || piece == 'r')) {
                        bool isOpenFile = true;
                        for (int r = 0; r < 8; r++) {
                            if (boardState[r*8 + file] == 'P' || boardState[r*8 + file] == 'p') {
                                isOpenFile = false;
                                break;
                            }
                        }
                        
                        if (isOpenFile) {
                            if (piece == 'R') score += 100 * PERSONALITY_FACTOR;
                            else score -= 100 * PERSONALITY_FACTOR;
                        }
                    }
                    
                    if ((piece == 'N' && rank < 4) || (piece == 'n' && rank > 3)) {
                        if (piece == 'N') score += 80 * PERSONALITY_FACTOR;
                        else score -= 80 * PERSONALITY_FACTOR;
                    }
                }
            } else {
                score += 100 * PERSONALITY_FACTOR;
            }
            break;
            
        case POSITIONAL:
            if (fullCalculation) {
                score += (whiteCentralPieces - blackCentralPieces) * 400 * PERSONALITY_FACTOR;
                
                if (whiteCentralPieces > blackCentralPieces) {
                    score += 600 * PERSONALITY_FACTOR;
                }

                score += whitePawnStructureScore * 3 * PERSONALITY_FACTOR;
                score -= blackPawnStructureScore * 3 * PERSONALITY_FACTOR;
                
                for (int i = 0; i < 64; i++) {
                    if (boardState[i] == 'B') score += 50 * PERSONALITY_FACTOR;
                    if (boardState[i] == 'b') score -= 50 * PERSONALITY_FACTOR;
                    if (boardState[i] == 'N') score += 30 * PERSONALITY_FACTOR;
                    if (boardState[i] == 'n') score -= 30 * PERSONALITY_FACTOR;
                }
                
                int whiteCoordination = 0, blackCoordination = 0;
                for (int i = 0; i < 64; i++) {
                    if (boardState[i] != ' ') {
                        if (isupper(boardState[i])) {
                            if (IsSquareAttacked(position, i, true)) {
                                whiteCoordination++;
                            }
                        } else {
                            if (IsSquareAttacked(position, i, false)) {
                                blackCoordination++;
                            }
                        }
                    }
                }
                
                score += whiteCoordination * 40 * PERSONALITY_FACTOR;
                score -= blackCoordination * 40 * PERSONALITY_FACTOR;
            } else {
                score += 150 * PERSONALITY_FACTOR;
            }
            break;
            
        case SOLID:
            if (fullCalculation) {
                if (position.whiteCanCastleKingside || position.whiteCanCastleQueenside) {
                    score -= 500 * PERSONALITY_FACTOR;
                }
                if (position.blackCanCastleKingside || position.blackCanCastleQueenside) {
                    score += 500 * PERSONALITY_FACTOR;
                }
                
                score += whitePieceCount * 50 * PERSONALITY_FACTOR; 
                score -= blackPieceCount * 50 * PERSONALITY_FACTOR;
                
                for (int i = 0; i < 64; i++) {
                    int file = i % 8;
                    char piece = boardState[i];
                    if (piece == 'P' && i > 7) {
                        if (file > 0 && boardState[i-1] == 'P') score += 70 * PERSONALITY_FACTOR;
                        if (file < 7 && boardState[i+1] == 'P') score += 70 * PERSONALITY_FACTOR;
                    }
                    if (piece == 'p' && i < 56) {
                        if (file > 0 && boardState[i-1] == 'p') score -= 70 * PERSONALITY_FACTOR;
                        if (file < 7 && boardState[i+1] == 'p') score -= 70 * PERSONALITY_FACTOR;
                    }
                }
                
                for (int i = 0; i < 64; i++) {
                    if (boardState[i] == 'N') score += 60 * PERSONALITY_FACTOR;
                    if (boardState[i] == 'n') score -= 60 * PERSONALITY_FACTOR;
                    if (boardState[i] == 'B') score += 40 * PERSONALITY_FACTOR;
                    if (boardState[i] == 'b') score -= 40 * PERSONALITY_FACTOR;
                }
                
                score -= whiteAttackingPieces * 75 * PERSONALITY_FACTOR;
                score += blackAttackingPieces * 75 * PERSONALITY_FACTOR;
            } else {
                score -= 120 * PERSONALITY_FACTOR;
            }
            break;
            
        case DYNAMIC:
            if (fullCalculation) {
                bool isOpenPosition = true;
                bool hasTacticalOpportunities = false;
                bool isEndgameNear = false;
        
                int centerPawns = 0;
                for (int i = 27; i <= 36; i++) {
                    if (tolower(boardState[i]) == 'p') {
                        centerPawns++;
                    }
                }
                isOpenPosition = (centerPawns <= 2);
                
                if (preCalculatedMoves) {
                    for (const Move& move : *preCalculatedMoves) {
                        if (move.notation.length() >= 5) {
                            int endPos = std::stoi(move.notation.substr(3, 2));
                            char target = position.boardState[endPos];
                            if (target != ' ') {
                                int attackerValue = GetPieceValue(move.notation[0]);
                                int targetValue = GetPieceValue(target);
                                
                                if (targetValue >= attackerValue) {
                                    hasTacticalOpportunities = true;
                                    break;
                                }
                            }
                        }
                    }
                }
                
                int pieceCount = 0;
                for (int i = 0; i < 64; i++) {
                    if (boardState[i] != ' ' && tolower(boardState[i]) != 'p') {
                        pieceCount++;
                    }
                }
                isEndgameNear = (pieceCount <= 12);
                
                if (hasTacticalOpportunities) {
                    std::cout << "DINAMIC: Stil agresiv pentru oportunități tactice" << std::endl;
                    score += whiteAttackingPieces * 180 * PERSONALITY_FACTOR;
                    score -= blackAttackingPieces * 180 * PERSONALITY_FACTOR;
                }
                else if (isOpenPosition) {
                    std::cout << "DINAMIC: Stil pozițional pentru poziție deschisă" << std::endl;
                    score += (whiteCentralPieces - blackCentralPieces) * 160 * PERSONALITY_FACTOR;
                    
                    for (int i = 0; i < 64; i++) {
                        if (boardState[i] == 'B') score += 70 * PERSONALITY_FACTOR;
                        if (boardState[i] == 'b') score -= 70 * PERSONALITY_FACTOR;
                    }
                }
                else if (!isOpenPosition) {
                    std::cout << "DINAMIC: Stil solid pentru poziție închisă" << std::endl;
                    score += whitePieceCount * 55 * PERSONALITY_FACTOR;
                    score -= blackPieceCount * 55 * PERSONALITY_FACTOR;
                    
                    for (int i = 0; i < 64; i++) {
                        if (boardState[i] == 'N') score += 80 * PERSONALITY_FACTOR;
                        if (boardState[i] == 'n') score -= 80 * PERSONALITY_FACTOR;
                    }
                }
                else if (isEndgameNear) {
                    std::cout << "DINAMIC: Stil de final" << std::endl;
                    
                    int whiteKingPos = -1, blackKingPos = -1;
                    for (int i = 0; i < 64; i++) {
                        if (boardState[i] == 'K') whiteKingPos = i;
                        else if (boardState[i] == 'k') blackKingPos = i;
                    }
                    
                    if (whiteKingPos >= 0 && blackKingPos >= 0) {
                        int whiteKingRank = whiteKingPos / 8;
                        int whiteKingFile = whiteKingPos % 8;
                        int blackKingRank = blackKingPos / 8;
                        int blackKingFile = blackKingPos % 8;
                        
                        int whiteDistToCenter = abs(whiteKingRank - 3.5) + abs(whiteKingFile - 3.5);
                        int blackDistToCenter = abs(blackKingRank - 3.5) + abs(blackKingFile - 3.5);
                        
                        score += (7 - whiteDistToCenter) * 90 * PERSONALITY_FACTOR;
                        score -= (7 - blackDistToCenter) * 90 * PERSONALITY_FACTOR;
                    }
                }
                
                if (preCalculatedMoves) {
                    score += preCalculatedMoves->size() * 25 * PERSONALITY_FACTOR;
                }
            } else {
                score += 80 * PERSONALITY_FACTOR;
            }
            break;
            
        case STANDARD:
        default:
            break;
    }
    
    return score;
}

bool IsMoveSafe(const BoardPosition& position, const Move& move) {
    int endPos = std::stoi(move.notation.substr(3, 2));
    bool isCapture = (position.boardState[endPos] != ' ' || move.isEnPassant);
    
    if (isCapture) {
        return IsGoodCapture(position, move);
    }
    
    BoardPosition newPosition = ApplyMove(position, move);
    
    char movedPiece = newPosition.boardState[endPos];
    int movedValue = GetPieceValue(movedPiece);
    
    std::vector<Move> responses = GenerateMoves(newPosition, !position.whiteToMove);
    
    for (const Move& response : responses) {
        int responseTarget = std::stoi(response.notation.substr(3, 2));
        
        if (responseTarget == endPos) {
            char recapturer = response.notation[0];
            int recapturerValue = GetPieceValue(recapturer);
            
            if (recapturerValue < movedValue) {
                std::string attackerType = (recapturerValue == 1) ? "pawn" : 
                                          (recapturerValue == 3) ? "minor piece" : 
                                          (recapturerValue == 5) ? "rook" : "queen";
                
                std::cout << "UNSAFE MOVE DETECTED: " << ConvertToAlgebraic(move, position) 
                          << " can be captured by " << attackerType 
                          << " (" << recapturerValue << " vs " << movedValue << ")" << std::endl;
                return false;
            }
        }
    }
    
    return true;
}

bool IsValidMoveNotation(const Move& move) {
    return (move.notation.length() >= 5 && 
            isalpha(move.notation[0]) && 
            isdigit(move.notation[1]) && 
            isdigit(move.notation[3]));
}

bool IsTacticalBlunder(const BoardPosition& position, const Move& move) {
    if (move.notation.length() < 5) return false;
    
    int startPos = std::stoi(move.notation.substr(1, 2));
    int endPos = std::stoi(move.notation.substr(3, 2));
    char attacker = position.boardState[startPos];
    char victim = position.boardState[endPos];
    bool isCapture = (victim != ' ' || move.isEnPassant);
    
    if (!isCapture) return false;
    
    int attackerValue = GetPieceValue(attacker);
    int victimValue = move.isEnPassant ? 1 : GetPieceValue(victim);
    
    BoardPosition afterMove = ApplyMove(position, move);
    
    int cheapestAttacker = GetCheapestAttackerValue(afterMove, endPos, !position.whiteToMove);
    
    if (cheapestAttacker > 0 && cheapestAttacker <= attackerValue) {
        std::cout << "BLUNDER TACTIC detectat: " << ConvertToAlgebraic(move, position) 
                  << " - piesa " << attacker << " (valoare " << attackerValue 
                  << ") va fi recapturată de o piesă de valoare " << cheapestAttacker << std::endl;
        return true;
    }
    
    if (position.fullMoveNumber <= 10 && tolower(victim) == 'p' && 
        (tolower(attacker) == 'n' || tolower(attacker) == 'b') &&
        endPos == 36) {
        
        if (IsSquareAttacked(position, endPos, position.whiteToMove)) {
            std::cout << "BLUNDER OPENING: Capturarea pionului apărat de pe e4 cu piesa minoră!" << std::endl;
            return true;
        }
    }
    
    return false;
}

int GetCentralityScore(const Move& move, bool isEarlyGame) {
    if (move.notation.length() < 5) return 0;
    
    int startPos = std::stoi(move.notation.substr(1, 2));
    int endPos = std::stoi(move.notation.substr(3, 2));
    int startRank = startPos / 8;
    int startFile = startPos % 8;
    int endRank = endPos / 8;
    int endFile = endPos % 8;
    
    int startCentrality = 4 - (customMin(abs(startRank - 3), abs(startRank - 4)) + 
                           customMin(abs(startFile - 3), abs(startFile - 4)));
    
    int endCentrality = 4 - (customMin(abs(endRank - 3), abs(endRank - 4)) + 
                         customMin(abs(endFile - 3), abs(endFile - 4)));
    
    int developmentFactor = 0;
    if (isEarlyGame) {
        char piece = move.notation[0];
        
        if ((isupper(piece) && endRank == 7) || (!isupper(piece) && endRank == 0)) {
            developmentFactor = -30;
        }
    }
    
    int multiplier = isEarlyGame ? 10 : 2;
    int centralityImprovement = (endCentrality - startCentrality) * multiplier;
    
    return centralityImprovement + developmentFactor;
}

int EvaluatePawnStructure(const BoardPosition& position, bool forWhite) {
    int score = 0;
    const std::string& boardState = position.boardState;
    
    int pawnsOnFile[8] = {0};
    
    char pawnChar = forWhite ? 'P' : 'p';
    
    for (int i = 0; i < 64; i++) {
        if (boardState[i] == pawnChar) {
            int file = i % 8;
            int rank = i / 8;
            pawnsOnFile[file]++;
            
            int advanceRank = forWhite ? (7 - rank) : rank;
            score += advanceRank * 5;
            
            bool isIsolated = true;
            if (file > 0 && pawnsOnFile[file-1] > 0) isIsolated = false;
            if (file < 7 && pawnsOnFile[file+1] > 0) isIsolated = false;
            
            if (isIsolated) {
                score -= 15;
            }
            
            if (pawnsOnFile[file] > 1) {
                score -= 10 * (pawnsOnFile[file] - 1);
            }
            
            bool isBackward = true;
            int nextRank = forWhite ? (rank - 1) : (rank + 1);
            if (nextRank >= 0 && nextRank < 8) {
                for (int f = max(0, file-1); f <= min(7, file+1); f++) {
                    int checkPos = nextRank * 8 + f;
                    if (boardState[checkPos] == pawnChar) {
                        isBackward = false;
                        break;
                    }
                }
            }
            
            if (isBackward) {
                score -= 12;
            }
            
            if (file > 0 && rank > 0 && boardState[(rank-1)*8 + (file-1)] == pawnChar) {
            }
            
            bool isPassed = true;
            int startRank = forWhite ? (rank - 1) : (rank + 1);
            int endRank = forWhite ? 0 : 7;
            
            for (int r = startRank; forWhite ? (r >= endRank) : (r <= endRank); forWhite ? r-- : r++) {
                if (r < 0 || r >= 8) continue;
                
                for (int f = max(0, file-1); f <= min(7, file+1); f++) {
                    char piece = boardState[r*8 + f];
                    if (piece == (forWhite ? 'p' : 'P')) {
                        isPassed = false;
                        break;
                    }
                }
                if (!isPassed) break;
            }
            
            if (isPassed) {
                int passedBonus = 10 + (forWhite ? (7 - rank) : rank) * 10;
                score += passedBonus;
            }
        }
    }
    
    return score;
}

extern "C" __declspec(dllexport) const char* GetBestMove(const char* moveHistoryStr, int maxDepth, bool isWhite)
{
    maxDepth = customMin(maxDepth, 3);

    std::string moveHistory(moveHistoryStr);

    ChessPersonality originalPersonality = currentPersonality;
    
    if (originalPersonality != STANDARD) {
        std::cout << "Temporarily using STANDARD personality for search" << std::endl;
        currentPersonality = STANDARD;
    }
    
    BoardPosition currentPosition;
    try {
        currentPosition = ParseMoveHistory(moveHistory);
    } catch (const std::exception& e) {
        std::cerr << "Error parsing move history: " << e.what() << std::endl;
        static std::string errorResult = "error";
        return errorResult.c_str();
    }

    PrintBoard(currentPosition.boardState);

    auto startTime = std::chrono::high_resolution_clock::now();
    const int MAX_SEARCH_TIME_MS = 10000;
    const int ABSOLUTE_FAILSAFE_TIME_MS = 20000;

    for (auto& entry : transpositionTable) {
        entry = TTEntry();
    }
    
    if (evaluationCache.size() > MAX_EVAL_CACHE_SIZE/5) {
        evaluationCache.clear();
    }

    std::vector<Move> allMoves = GenerateMoves(currentPosition, currentPosition.whiteToMove);

    std::vector<Move> legalMoves;
    for (const Move& move : allMoves) {
        BoardPosition newPosition = ApplyMove(currentPosition, move);
        if (!IsKingInCheck(newPosition, currentPosition.whiteToMove)) {
            legalMoves.push_back(move);
        }
    }

    if (legalMoves.empty()) {
        static std::string noMoveResult = "error";
        return noMoveResult.c_str();
    }
    
    if (originalPersonality != STANDARD) {
        std::vector<Move> filteredMoves;
        int initialMoveCount = legalMoves.size();
        int tension;
        switch (originalPersonality) {
            case AGGRESSIVE:
                for (const Move& move : legalMoves) {
                    if (move.notation.length() >= 5) {
                        int startPos = std::stoi(move.notation.substr(1, 2));
                        int endPos = std::stoi(move.notation.substr(3, 2));
                        int startRank = startPos / 8;
                        int endRank = endPos / 8;
                        
                        bool isAdvancing = (currentPosition.whiteToMove) ? 
                            (endRank < startRank) : (endRank > startRank);
                            
                        bool isCapture = (currentPosition.boardState[endPos] != ' ');
                        
                        if (isAdvancing || isCapture) {
                            filteredMoves.push_back(move);
                        }
                    }
                }
                break;
                
            case POSITIONAL:
                for (const Move& move : legalMoves) {
                    if (move.notation.length() >= 5) {
                        int endPos = std::stoi(move.notation.substr(3, 2));
                        int endRank = endPos / 8;
                        int endFile = endPos % 8;
                        
                        bool isNearCenter = (endRank >= 2 && endRank <= 5 && 
                                            endFile >= 2 && endFile <= 5);
                                            
                        if (isNearCenter) {
                            filteredMoves.push_back(move);
                        }
                    }
                }
                break;
                
            case SOLID:
                for (const Move& move : legalMoves) {
                    if (move.notation.length() >= 5) {
                        int startPos = std::stoi(move.notation.substr(1, 2));
                        int endPos = std::stoi(move.notation.substr(3, 2));
                        int startRank = startPos / 8;
                        int endRank = endPos / 8;
                        
                        bool isDefensive = (currentPosition.whiteToMove) ? 
                            (endRank >= 4) : (endRank <= 3);
                            
                        bool isCastling = move.isCastling;
                        
                        if (isDefensive || isCastling) {
                            filteredMoves.push_back(move);
                        }
                    }
                }
                break;
                
            case DYNAMIC:
                tension = 0;
                for (int i = 0; i < 64; i++) {
                    char p = currentPosition.boardState[i];
                    if (p != ' ') {
                        if (IsSquareAttacked(currentPosition, i, !isupper(p))) {
                            tension++;
                        }
                    }
                }
                
                for (const Move& move : legalMoves) {
                    if (move.notation.length() >= 5) {
                        int startPos = std::stoi(move.notation.substr(1, 2));
                        int endPos = std::stoi(move.notation.substr(3, 2));
                        int startRank = startPos / 8;
                        int endRank = endPos / 8;
                        
                        bool isAdvancing = (currentPosition.whiteToMove) ? 
                            (endRank < startRank) : (endRank > startRank);
                            
                        bool isCapture = (currentPosition.boardState[endPos] != ' ');
                        
                        bool isNearCenter = (endRank >= 2 && endRank <= 5 && 
                                            endPos % 8 >= 2 && endPos % 8 <= 5);
                        
                        if ((tension > 4 && (isAdvancing || isCapture)) || 
                            (tension <= 4 && isNearCenter)) {
                            filteredMoves.push_back(move);
                        }
                    }
                }
                break;
                
            default:
                break;
        }
        
        if (!filteredMoves.empty()) {
            std::cout << "Filtered moves for " << (int)originalPersonality 
                      << " personality: kept " << filteredMoves.size() 
                      << " out of " << initialMoveCount << " moves" << std::endl;
            legalMoves = filteredMoves;
        }
    }

    Move bestMove = legalMoves[0];
    bool hasBestMove = true;

    if (currentPosition.fullMoveNumber <= 4) {
        std::cout << "Applying opening move filters..." << std::endl;
        
        int initialMoveCount = legalMoves.size();
        
        for (auto it = legalMoves.begin(); it != legalMoves.end();) {
            bool removeBadMove = false;
            
            if (currentPosition.whiteToMove) {
                if (it->notation == "P4840") {
                    std::cout << "Filtering out a2a3" << std::endl;
                    removeBadMove = true;
                }
                else if (it->notation == "P5547") {
                    std::cout << "Filtering out h2h3" << std::endl;
                    removeBadMove = true;
                }
                else if (it->notation[0] == 'N') {
                    int endPos = std::stoi(it->notation.substr(3, 2));
                    int file = endPos % 8;
                    if (file == 0 || file == 7) {
                        std::cout << "Filtering out knight move to edge: " << it->notation << std::endl;
                        removeBadMove = true;
                    }
                }
            }
            else {
                if (it->notation == "n0116" && 
                    (moveHistory == "e2e4" || moveHistory == "e4")) {
                    std::cout << "Filtering out Na6 response to e4" << std::endl;
                    removeBadMove = true;
                }
                else if (it->notation == "n0623" && 
                    (moveHistory == "e2e4" || moveHistory == "e4")) {
                    std::cout << "Filtering out Nh6 response to e4" << std::endl;
                    removeBadMove = true;
                }
                else if (it->notation[0] == 'n') {
                    int endPos = std::stoi(it->notation.substr(3, 2));
                    int file = endPos % 8;
                    if (file == 0 || file == 7) {
                        std::cout << "Filtering out knight move to edge: " << it->notation << std::endl;
                        removeBadMove = true;
                    }
                }
            }
            
            if (removeBadMove && legalMoves.size() > 1) {
                it = legalMoves.erase(it);
            } else {
                ++it;
            }
        }
        
        std::cout << "Legal moves after filtering:" << std::endl;
        for (const Move& move : legalMoves) {
            std::cout << "  " << move.notation << " -> " 
                      << ConvertToAlgebraic(move, currentPosition) << std::endl;
        }
        
        if (legalMoves.empty() && initialMoveCount > 0) {
            std::cout << "Warning: Filtered all moves, restoring legal ones" << std::endl;
            legalMoves = allMoves;
            for (auto it = legalMoves.begin(); it != legalMoves.end();) {
                BoardPosition newPosition = ApplyMove(currentPosition, *it);
                if (IsKingInCheck(newPosition, !currentPosition.whiteToMove)) {
                    it = legalMoves.erase(it);
                } else {
                    ++it;
                }
            }
        }

        if (legalMoves.size() > 1) {
            auto blunderIter = std::remove_if(legalMoves.begin(), legalMoves.end(),
                [&currentPosition](const Move& move) { 
                    return IsTacticalBlunder(currentPosition, move); 
                });
    
            if (blunderIter != legalMoves.begin()) {
                int numBlunders = std::distance(blunderIter, legalMoves.end());
                if (numBlunders > 0 && blunderIter != legalMoves.end()) {
                    std::cout << "Am filtrat " << numBlunders << " mutări tactice proaste!" << std::endl;
                    legalMoves.erase(blunderIter, legalMoves.end());
                }
            }
        }
        
        if (legalMoves.size() > 1) {
            std::vector<std::pair<int, Move>> scoredMoves;
            for (const Move& move : legalMoves) {
                BoardPosition newPos = ApplyMove(currentPosition, move);

                newPos.whiteToMove = currentPosition.whiteToMove;
                int score = EvaluateBoard(newPos, 0);

                if (!currentPosition.whiteToMove) {
                    score = -score;
                }
                scoredMoves.push_back({score, move});
                std::cout << "  Evaluated " << ConvertToAlgebraic(move, currentPosition) 
                          << " = " << score << std::endl;
            }

            if (currentPosition.fullMoveNumber == 1 && currentPosition.whiteToMove) {
                for (auto& scoreMove : scoredMoves) {
                    std::string algebraic = ConvertToAlgebraic(scoreMove.second, currentPosition);
                    if (algebraic == "e4" || algebraic == "e2e4") {
                        scoreMove.first = 5000;
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
            
            if (scoredMoves.size() > 1) {
                auto cutoffIter = std::remove_if(scoredMoves.begin(), scoredMoves.end(), 
                    [&currentPosition](const std::pair<int, Move>& scoredMove) {
                        return (currentPosition.whiteToMove && scoredMove.first < -1000) || 
                               (!currentPosition.whiteToMove && scoredMove.first < -1000);
                    });
                
                if (cutoffIter != scoredMoves.begin() && scoredMoves.end() - cutoffIter > 1) {
                    scoredMoves.erase(cutoffIter, scoredMoves.end());
                }
            }
            
            std::sort(scoredMoves.begin(), scoredMoves.end(),
                [](const auto& a, const auto& b) { return a.first > b.first; });
            
            int keepCount = customMax(1, (int)(scoredMoves.size() / 2));
            if (scoredMoves.size() > keepCount) {
                scoredMoves.resize(keepCount);
            }
            
            legalMoves.clear();
            for (const auto& scoredMove : scoredMoves) {
                legalMoves.push_back(scoredMove.second);
            }
        }
    }

    const int FIXED_TIME_LIMIT_MS = 10000;
    startTime = std::chrono::high_resolution_clock::now();

    for (int currentDepth = 1; currentDepth <= maxDepth; currentDepth++) {
        MoveTreeNode* root = new MoveTreeNode(currentPosition.boardState);
        
        for (const Move& move : legalMoves) {
            BoardPosition newPosition = ApplyMove(currentPosition, move);
            MoveTreeNode* child = new MoveTreeNode(newPosition.boardState, move, root);
            
            if (currentDepth > 1) {
                ExpandNode(child, currentDepth-1, !currentPosition.whiteToMove, newPosition);
            }
            
            root->children.push_back(child);
        }

        int bestValue = -2147483647;
        Move currentBestMove;
        bool moveFound = false;

        try {
            for (MoveTreeNode* childNode : root->children) {
                int moveValue = -MinimaxOnTree(childNode, currentDepth - 1, -2147483647, 2147483647, !currentPosition.whiteToMove, true);
        
                if (moveValue > bestValue) {
                    bestValue = moveValue;
                    currentBestMove = childNode->move;
                    moveFound = true;
                }
            }
        }
        catch (const std::runtime_error& e) {
            std::cout << "Căutare întreruptă: " << e.what() << std::endl;
            if (moveFound) {
                bestMove = currentBestMove;
                hasBestMove = true;
            }
            break;
        }

        if (moveFound) {
            bestMove = currentBestMove;
            hasBestMove = true;

            if (bestValue > 90000 || bestValue < -90000) {
                break;
            }
        }

        delete root;

        auto currentTime = std::chrono::high_resolution_clock::now();
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();
        if (elapsedMs > MAX_SEARCH_TIME_MS) {
            std::cout << "Time limit reached at depth " << currentDepth << ": " << elapsedMs << "ms" << std::endl;
            break;
        }
        
        if (elapsedMs > ABSOLUTE_FAILSAFE_TIME_MS) {
            std::cout << "FAILSAFE: Search taking too long, terminating early" << std::endl;
            break;
        }
        
        if (currentDepth >= 2) {
            auto currentTime = std::chrono::high_resolution_clock::now();
            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTime - startTime).count();
            if (elapsedMs > FIXED_TIME_LIMIT_MS) {
                std::cout << "Time limit reached: " << elapsedMs << "ms" << std::endl;
                break;
            }
        }
    }

    if (originalPersonality != STANDARD) {
        std::cout << "Restoring " << (int)originalPersonality << " personality for final move selection" << std::endl;
        currentPersonality = originalPersonality;
    }

    if (currentPersonality != STANDARD && !legalMoves.empty()) {
        std::vector<std::pair<int, Move>> finalEvaluation;
    
        for (const Move& move : legalMoves) {
            BoardPosition newPos = ApplyMove(currentPosition, move);
        
            int score = EvaluateBoard(newPos, 1);
        
            if (!currentPosition.whiteToMove) {
                score = -score;
            }
        
            finalEvaluation.push_back({score, move});
        }

        bool isEarlyGame = (currentPosition.fullMoveNumber <= 10);
        
        const int PERSONALITY_FACTOR = 50;
    
        for (auto& eval : finalEvaluation) {
            int startPos = std::stoi(eval.second.notation.substr(1, 2));
            int endPos = std::stoi(eval.second.notation.substr(3, 2));
            char piece = eval.second.notation[0];
            int startRank = startPos / 8;
            int endRank = endPos / 8;
            int endFile = endPos % 8;
            bool isAdvancing = (currentPosition.whiteToMove) ? 
                (endRank < startRank) : (endRank > startRank);
            
            int centralityScore = GetCentralityScore(eval.second, isEarlyGame);
        
            switch (currentPersonality) {
                case AGGRESSIVE:
                    if (isAdvancing) {
                        centralityScore += 250 * PERSONALITY_FACTOR;
                    }
                
                    if ((currentPosition.whiteToMove && endRank < 4) || 
                        (!currentPosition.whiteToMove && endRank > 3)) {
                        centralityScore += 200 * PERSONALITY_FACTOR;
                    }
                    
                    if (currentPosition.boardState[endPos] != ' ') {
                        centralityScore += 300 * PERSONALITY_FACTOR;
                    }
                    
                    {
                        BoardPosition afterMove = ApplyMove(currentPosition, eval.second);
                        if (IsKingInCheck(afterMove, !currentPosition.whiteToMove)) {
                            centralityScore += 400 * PERSONALITY_FACTOR;
                        }
                    }
                    break;
                
                case POSITIONAL:
                    if (endRank >= 3 && endRank <= 4 && endFile >= 3 && endFile <= 4) {
                        centralityScore += 500 * PERSONALITY_FACTOR;
                    } else if (endRank >= 2 && endRank <= 5 && endFile >= 2 && endFile <= 5) {
                        centralityScore += 300 * PERSONALITY_FACTOR;
                    }
                    
                    if ((piece == 'B' || piece == 'b')) {
                        centralityScore += 150 * PERSONALITY_FACTOR;
                    }
                    else if ((piece == 'N' || piece == 'n')) {
                        centralityScore += 120 * PERSONALITY_FACTOR;
                    }
                    
                    if (isEarlyGame && ((isupper(piece) && endRank > 5) ||
                                       (!isupper(piece) && endRank < 2))) {
                        centralityScore -= 250 * PERSONALITY_FACTOR;
                    }
                    break;
                
                case SOLID:
                    centralityScore /= 4;
                
                    if ((currentPosition.whiteToMove && endRank < 3) || 
                        (!currentPosition.whiteToMove && endRank > 4)) {
                        centralityScore -= 300 * PERSONALITY_FACTOR;
                    }
                
                    if ((currentPosition.whiteToMove && endRank > 5) || 
                        (!currentPosition.whiteToMove && endRank < 2)) {
                        eval.first += 350 * PERSONALITY_FACTOR;
                    }
                    
                    if (eval.second.isCastling) {
                        eval.first += 500 * PERSONALITY_FACTOR;
                    }
                    
                    {
                        BoardPosition afterMove = ApplyMove(currentPosition, eval.second);
                        int protectionCount = 0;
                        
                        for (int i = 0; i < 64; i++) {
                            char p = afterMove.boardState[i];
                            if (p != ' ' && ((currentPosition.whiteToMove && isupper(p)) ||
                                            (!currentPosition.whiteToMove && islower(p)))) {
                                if (i != endPos && IsSquareAttacked(afterMove, i, currentPosition.whiteToMove)) {
                                    protectionCount++;
                                }
                            }
                        }
                        
                        if (protectionCount > 0) {
                            eval.first += 200 * protectionCount * PERSONALITY_FACTOR;
                        }
                    }
                    break;
                
                case DYNAMIC:
                    int tension = 0;
                    for (int i = 0; i < 64; i++) {
                        char p = currentPosition.boardState[i];
                        if (p != ' ') {
                            if (IsSquareAttacked(currentPosition, i, !isupper(p))) {
                                tension++;
                            }
                        }
                    }
                
                    if (tension > 3) {
                        if (isAdvancing) {
                            centralityScore += 150 * PERSONALITY_FACTOR;
                        } else if (currentPosition.boardState[endPos] != ' ') {
                            centralityScore += 200 * PERSONALITY_FACTOR;
                        } else {
                            centralityScore += 100 * PERSONALITY_FACTOR;
                        }
                    } else {
                        centralityScore += 180 * PERSONALITY_FACTOR;
                        
                        {
                            BoardPosition afterMove = ApplyMove(currentPosition, eval.second);
                            std::vector<Move> futureMoves = GenerateMoves(afterMove, currentPosition.whiteToMove);
                            
                            eval.first += futureMoves.size() * 25 * PERSONALITY_FACTOR;
                        }
                    }
                    break;
            }
        
            if (isEarlyGame) {
                if ((isupper(piece) && endRank == 7) || 
                    (!isupper(piece) && endRank == 0)) {
                    eval.first -= 600 * PERSONALITY_FACTOR;
                }
            }
            
            eval.first += centralityScore;
        
            std::cout << "Eval with " << (int)currentPersonality 
                    << " personality: " << ConvertToAlgebraic(eval.second, currentPosition) 
                    << " = " << eval.first << " (centrality: " << centralityScore << ")" << std::endl;
        }
    
        std::sort(finalEvaluation.begin(), finalEvaluation.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });
    
        int displayCount = customMin(3, (int)finalEvaluation.size());
        std::cout << "Top " << displayCount << " moves for personality " 
                << (int)currentPersonality << ":" << std::endl;
        for (int i = 0; i < displayCount; i++) {
            std::cout << "  " << (i+1) << ". " 
                    << ConvertToAlgebraic(finalEvaluation[i].second, currentPosition)
                    << " (score: " << finalEvaluation[i].first << ")" << std::endl;
        }
    
        if (!finalEvaluation.empty()) {
            bestMove = finalEvaluation[0].second;
            hasBestMove = true;
    
            if (currentPersonality == AGGRESSIVE && finalEvaluation.size() > 1) {
                bool foundGoodMove = false;
    
                for (const auto& evalMove : finalEvaluation) {
                    int startPos = std::stoi(evalMove.second.notation.substr(1, 2));
                    int endPos = std::stoi(evalMove.second.notation.substr(3, 2));
                    char piece = evalMove.second.notation[0];
        
                    if (tolower(piece) == 'n' && endPos == 36) {
                        for (int i = 0; i < 64; i++) {
                            if (currentPosition.boardState[i] == 'N') {
                                int knightFile = i % 8;
                                int knightRank = i / 8;
                    
                                if ((knightRank == 2 && knightFile == 2) || 
                                    (knightRank == 4 && knightFile == 3) || 
                                    (knightRank == 4 && knightFile == 5) || 
                                    (knightRank == 2 && knightFile == 5)) { 
                                    std::cout << "AGGRESSIVE avoided critical blunder: Nf6xe4 când există cal care recaptureazǎ" << std::endl;
                                    continue;
                                }
                            }
                        }
                    }
        
                    BoardPosition afterMove = ApplyMove(currentPosition, evalMove.second);
                    bool givesCheck = IsKingInCheck(afterMove, !currentPosition.whiteToMove);
        
                    if (givesCheck && IsMoveSafe(currentPosition, evalMove.second)) {
                        bestMove = evalMove.second;
                        std::cout << "AGGRESSIVE override: Preferring safe check move!" << std::endl;
                        foundGoodMove = true;
                        break;
                    }
                }
    
                if (!foundGoodMove) {
                    for (const auto& evalMove : finalEvaluation) {
                        int startPos = std::stoi(evalMove.second.notation.substr(1, 2));
                        int endPos = std::stoi(evalMove.second.notation.substr(3, 2));
                        char piece = evalMove.second.notation[0];
                        bool isCapture = (currentPosition.boardState[endPos] != ' ');
            
                        if (tolower(piece) == 'n' && endPos == 36) {
                            bool isUnsafe = false;
                            for (int i = 0; i < 64; i++) {
                                if (currentPosition.boardState[i] == 'N') {
                                    int knightRank = i / 8;
                                    int knightFile = i % 8;
                                    if (knightRank == 5 && knightFile == 2) {
                                        isUnsafe = true;
                                        break;
                                    }
                                }
                            }
                
                            if (isUnsafe) {
                                std::cout << "AGGRESSIVE avoided specific tactical blunder: Nf6xe4 când Nc3 există!" << std::endl;
                                continue;
                            }
                        }
            
                        if (isCapture) {
                            if (IsGoodCapture(currentPosition, evalMove.second)) {
                                bestMove = evalMove.second;
                                std::cout << "AGGRESSIVE override: Preferring safe capture!" << std::endl;
                                foundGoodMove = true;
                                break;
                            }
                        }
                    }
                }
    
                if (!foundGoodMove) {
                    for (const auto& evalMove : finalEvaluation) {
                        int startPos = std::stoi(evalMove.second.notation.substr(1, 2));
                        int endPos = std::stoi(evalMove.second.notation.substr(3, 2));
                        int startRank = startPos / 8;
                        int endRank = endPos / 8;
            
                        bool isAdvancing = (currentPosition.whiteToMove) ? 
                            (endRank < startRank) : (endRank > startRank);
                
                        if (isAdvancing && IsMoveSafe(currentPosition, evalMove.second)) {
                            bestMove = evalMove.second;
                            std::cout << "AGGRESSIVE override: Preferring safe advancing move!" << std::endl;
                            foundGoodMove = true;
                            break;
                        }
                    }
                }
    
                if (!foundGoodMove) {
                    std::cout << "AGGRESSIVE: No specific tactics found, using best evaluated move" << std::endl;
                }
            }
            else if (currentPersonality == POSITIONAL && finalEvaluation.size() > 1) {
                for (const auto& evalMove : finalEvaluation) {
                    int endPos = std::stoi(evalMove.second.notation.substr(3, 2));
                    int endRank = endPos / 8;
                    int endFile = endPos % 8;
                    
                    bool isCentralSquare = (endRank >= 3 && endRank <= 4 && 
                                           endFile >= 3 && endFile <= 4);
                    
                    if (isCentralSquare) {
                        bestMove = evalMove.second;
                        std::cout << "POSITIONAL override: Preferring absolute central control" << std::endl;
                        break;
                    }
                }
                
                bool foundCentralMove = false;
                for (const auto& evalMove : finalEvaluation) {
                    char piece = evalMove.second.notation[0];
                    
                    if (tolower(piece) == 'b') {
                        bestMove = evalMove.second;
                        std::cout << "POSITIONAL override: Preferring bishop development" << std::endl;
                        foundCentralMove = true;
                        break;
                    }
                }
                
                if (!foundCentralMove) {
                    for (const auto& evalMove : finalEvaluation) {
                        int endPos = std::stoi(evalMove.second.notation.substr(3, 2));
                        int endRank = endPos / 8;
                        int endFile = endPos % 8;
                        
                        bool isNearCentralSquare = (endRank >= 2 && endRank <= 5 && 
                                                   endFile >= 2 && endFile <= 5);
                        
                        if (isNearCentralSquare) {
                            bestMove = evalMove.second;
                            std::cout << "POSITIONAL override: Preferring extended central control" << std::endl;
                            break;
                        }
                    }
                }
            }
            else if (currentPersonality == SOLID && finalEvaluation.size() > 1) {
                bool foundCastling = false;
                for (const auto& evalMove : finalEvaluation) {
                    if (evalMove.second.isCastling) {
                        bestMove = evalMove.second;
                        std::cout << "SOLID override: Preferring castling (absolute priority)" << std::endl;
                        foundCastling = true;
                        break;
                    }
                }
                
                if (!foundCastling) {
                    bool foundDefensive = false;
                    for (const auto& evalMove : finalEvaluation) {
                        int endPos = std::stoi(evalMove.second.notation.substr(3, 2));
                        int startPos = std::stoi(evalMove.second.notation.substr(1, 2));
                        int endRank = endPos / 8;
                        
                        bool isDefensive = (currentPosition.whiteToMove) ? 
                            (endRank >= 4) : (endRank <= 3);
                            
                        if (isDefensive) {
                            BoardPosition afterMove = ApplyMove(currentPosition, evalMove.second);
                            int protectionCount = 0;
                            
                            for (int i = 0; i < 64; i++) {
                                char p = afterMove.boardState[i];
                                if (p != ' ' && ((currentPosition.whiteToMove && isupper(p)) ||
                                                (!currentPosition.whiteToMove && islower(p)))) {
                                    if (i != endPos && IsSquareAttacked(afterMove, i, currentPosition.whiteToMove)) {
                                        protectionCount++;
                                    }
                                }
                            }
                            
                            if (protectionCount > 0) {
                                bestMove = evalMove.second;
                                std::cout << "SOLID override: Preferring defensive move that protects " 
                                         << protectionCount << " pieces" << std::endl;
                                foundDefensive = true;
                                break;
                            }
                        }
                    }
                    
                    if (!foundDefensive) {
                        for (const auto& evalMove : finalEvaluation) {
                            int endPos = std::stoi(evalMove.second.notation.substr(3, 2));
                            int endRank = endPos / 8;
                            
                            bool staySafe = (currentPosition.whiteToMove) ? 
                                (endRank >= 3) : (endRank <= 4);
                                
                            if (staySafe) {
                                bestMove = evalMove.second;
                                std::cout << "SOLID override: Preferring safe positioning" << std::endl;
                                break;
                            }
                        }
                    }
                }
            } 
            else if (currentPersonality == DYNAMIC && finalEvaluation.size() > 1) {
                
                int tension = 0;
                for (int i = 0; i < 64; i++) {
                    char p = currentPosition.boardState[i];
                    if (p != ' ') {
                        if (IsSquareAttacked(currentPosition, i, !isupper(p))) {
                            tension++;
                        }
                    }
                }
                
                if (tension > 4) {
                    std::cout << "DYNAMIC: Detectată poziție cu tensiune ridicată (" << tension << ")" << std::endl;
                    
                    bool foundTactical = false;
                    for (const auto& evalMove : finalEvaluation) {
                        int endPos = std::stoi(evalMove.second.notation.substr(3, 2));
                        bool isCapture = (currentPosition.boardState[endPos] != ' ');
                        
                        BoardPosition afterMove = ApplyMove(currentPosition, evalMove.second);
                        bool givesCheck = IsKingInCheck(afterMove, !currentPosition.whiteToMove);
                        
                        if (givesCheck || isCapture) {
                            bestMove = evalMove.second;
                            std::cout << "DYNAMIC override (high tension): Preferring tactical move" << std::endl;
                            foundTactical = true;
                            break;
                        }
                    }
                    
                    if (!foundTactical) {
                        int bestMobility = -1;
                        Move bestMobilityMove;
                        
                        for (const auto& evalMove : finalEvaluation) {
                            BoardPosition afterMove = ApplyMove(currentPosition, evalMove.second);
                            std::vector<Move> futureMoves = GenerateMoves(afterMove, afterMove.whiteToMove);
                            
                            if ((int)futureMoves.size() > bestMobility) {
                                bestMobility = futureMoves.size();
                                bestMobilityMove = evalMove.second;
                            }
                        }
                        
                        if (bestMobility > 0) {
                            bestMove = bestMobilityMove;
                            std::cout << "DYNAMIC override (high tension): Preferring move with best mobility ("
                                     << bestMobility << " future moves)" << std::endl;
                        }
                    }
                } else {
                    std::cout << "DYNAMIC: Detectată poziție calmă (tensiune: " << tension << ")" << std::endl;
                    
                    bool foundPositional = false;
                    for (const auto& evalMove : finalEvaluation) {
                        int endPos = std::stoi(evalMove.second.notation.substr(3, 2));
                        int endRank = endPos / 8;
                        int endFile = endPos % 8;
                        
                        bool isNearCenter = (endRank >= 2 && endRank <= 5 && 
                                            endFile >= 2 && endFile <= 5);
                                            
                        if (isNearCenter) {
                            bestMove = evalMove.second;
                            std::cout << "DYNAMIC override (calm): Preferring central positioning" << std::endl;
                            foundPositional = true;
                            break;
                        }
                    }
                    
                    if (!foundPositional && isEarlyGame) {
                        for (const auto& evalMove : finalEvaluation) {
                            char piece = evalMove.second.notation[0];
                            int startPos = std::stoi(evalMove.second.notation.substr(1, 2));
                            int startRank = startPos / 8;
                            
                            bool isDevelopment = (currentPosition.whiteToMove && 
                                                (tolower(piece) == 'n' || tolower(piece) == 'b') && 
                                                startRank == 7) ||
                                               (!currentPosition.whiteToMove && 
                                                (tolower(piece) == 'n' || tolower(piece) == 'b') && 
                                                startRank == 0);
                                                
                            if (isDevelopment) {
                                bestMove = evalMove.second;
                                std::cout << "DYNAMIC override (calm): Preferring development" << std::endl;
                                break;
                            }
                        }
                    }
                }
            }
    
            std::cout << "Personality preferred move: " 
                    << ConvertToAlgebraic(bestMove, currentPosition) << std::endl;
        }
    }
    if (hasBestMove && bestMove.notation.length() >= 5) {
        int endPos = std::stoi(bestMove.notation.substr(3, 2));
        bool isCapture = (currentPosition.boardState[endPos] != ' ' || bestMove.isEnPassant);
        
        bool skipSafetyCheck = (currentPersonality == AGGRESSIVE || isCapture);
        
        if (!skipSafetyCheck && !IsMoveSafe(currentPosition, bestMove)) {
            std::cout << "EMERGENCY SAFETY OVERRIDE: About to return a blunder: " 
                    << ConvertToAlgebraic(bestMove, currentPosition) << std::endl;
        
            bool foundSafeMove = false;
            for (const Move& move : legalMoves) {
                if (move.notation != bestMove.notation) {
                    int moveEndPos = std::stoi(move.notation.substr(3, 2));
                    bool isMoveCapture = (currentPosition.boardState[moveEndPos] != ' ' || move.isEnPassant);
                    
                    if (isMoveCapture || IsMoveSafe(currentPosition, move)) {
                        bestMove = move;
                        foundSafeMove = true;
                        std::cout << "Replacing with safe move: " 
                                << ConvertToAlgebraic(bestMove, currentPosition) << std::endl;
                        break;
                    }
                }
            }
        
            if (!foundSafeMove && !legalMoves.empty()) {
                bestMove = legalMoves[0];
                std::cout << "WARNING: No safe moves found! Using first legal move: " 
                        << ConvertToAlgebraic(bestMove, currentPosition) << std::endl;
            }
        }
    }

    static std::string result;
    
    if (hasBestMove) {
        if (bestMove.notation.length() >= 5) {
            result = ConvertToAlgebraic(bestMove, currentPosition);
        } else {
            std::vector<std::pair<int, Move>> scoredMoves;
            for (const Move& move : legalMoves) {
                if (move.notation.length() >= 5) {
                    BoardPosition newPos = ApplyMove(currentPosition, move);
                    int score = EvaluateBoard(newPos, 0);
                    if (!currentPosition.whiteToMove) {
                        score = -score;
                    }
                    
                    bool isEarlyGame = (currentPosition.fullMoveNumber <= 10);
                    int centralityScore = GetCentralityScore(move, isEarlyGame);
                    score += centralityScore;
                    
                    scoredMoves.push_back({score, move});
                }
            }
            
            std::sort(scoredMoves.begin(), scoredMoves.end(),
                [](const auto& a, const auto& b) { return a.first > b.first; });
                
            if (!scoredMoves.empty()) {
                bestMove = scoredMoves[0].second;
                result = ConvertToAlgebraic(bestMove, currentPosition);
            } else {
                result = "error";
            }
        }
    } else {
        result = "error";
    }
    
    if (result == "error" && !legalMoves.empty()) {
        std::cout << "WARNING: Engine returned 'error' despite having legal moves. Using fallback." << std::endl;
        
        for (const Move& move : legalMoves) {
            if (move.notation.length() >= 5) {
                result = ConvertToAlgebraic(move, currentPosition);
                std::cout << "Fallback move selected: " << result << std::endl;
                break;
            }
        }
    }
    
    std::cout << "Selected move: " << result << std::endl;
    
    return result.c_str();
}

extern "C" __declspec(dllexport) void SetEnginePersonality(int personalityType) {
    if (personalityType >= STANDARD && personalityType <= DYNAMIC) {
        currentPersonality = static_cast<ChessPersonality>(personalityType);
        std::cout << "Engine personality set to: " << personalityType << std::endl;
    } else {
        std::cout << "Invalid personality type: " << personalityType << std::endl;
    }
}

void PrintMoveTree(MoveTreeNode* node, int depth = 0) {
    for (int i = 0; i < depth; i++) {
        std::cout << "  ";
    }
    
    if (depth == 0) {
        std::cout << "Root: ";
    } else {
        std::cout << "Move: " << node->move.notation;
        if (node->isEvaluated) {
            std::cout << " (Eval: " << node->evaluation << ")";
        }
    }
    std::cout << std::endl;
    
    for (MoveTreeNode* child : node->children) {
        PrintMoveTree(child, depth + 1);
    }
}

void DebugOpeningMoves() {
    /*
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

    // Test White's first move selection
    BoardPosition startPos;
    startPos.boardState = "rnbqkbnrpppppppp                                PPPPPPPPRNBQKBNR";
    startPos.whiteToMove = true;
    startPos.fullMoveNumber = 1;
    startPos.whiteCanCastleKingside = true;
    startPos.whiteCanCastleQueenside = true;
    startPos.blackCanCastleKingside = true;
    startPos.blackCanCastleQueenside = true;
    
    const char* firstMove = GetBestMove("", 3, true);
    std::cout << "White's first move with new evaluation: " << firstMove << std::endl;
    
    // Test Black's response to e4
    BoardPosition e4Pos2;
    e4Pos2.boardState = "rnbqkbnrpppppppp                P               PPP PPPPRNBQKBNR";
    e4Pos2.whiteToMove = false;
    e4Pos2.fullMoveNumber = 1;
    */
    const char* blackResponse = GetBestMove("e2e4 Ng8f6 Nb1c3" ,3, false);
    std::cout << "Black's response to e4: " << blackResponse << std::endl;
    /*
    // Test personalities
    std::cout << "\nTesting personalities:" << std::endl;
    
    // Test aggressive personality
    SetEnginePersonality(AGGRESSIVE);
    std::cout << "Aggressive personality move: " << GetBestMove("e2e4 e7e5", 3, true) << std::endl;
    
    // Test positional personality
    SetEnginePersonality(POSITIONAL);
    std::cout << "Positional personality move: " << GetBestMove("e2e4 e7e5", 3, true) << std::endl;
    
    // Reset to standard
    SetEnginePersonality(STANDARD);
    */
}

void TestPersonalities() {
    std::cout << "\n===== TESTAREA PERSONALITĂȚILOR DE ȘAH =====\n" << std::endl;
    
    // În loc să folosim direct FEN, folosim o secvență de mutări care ajunge într-o poziție de mijloc de joc
    const char* setupMoves = "e2e4 c7c5 Ng1f3 d7d6 d2d4 c5d4 Nf3d4 Ng8f6 Nb1c3";
    
    // Testează fiecare personalitate
    const char* personalities[] = {"Standard", "Agresiv", "Pozițional", "Solid", "Dinamic"};
    
    for (int i = 0; i < 5; i++) {
        SetEnginePersonality(i);
        auto startTime = std::chrono::high_resolution_clock::now();
        const char* bestMove = GetBestMove(setupMoves, 3, true);
        auto endTime = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        
        std::cout << personalities[i] << " a ales: " << bestMove 
                  << " (în " << elapsed << "ms)" << std::endl;
    }
    
    // Resetează la standard
    SetEnginePersonality(0);
}

/*
int main() {
    // Run the personality tests
    TestPersonalities();
    /*
    // Test some specific game situations
    std::cout << "\n===== TESTING SPECIFIC POSITIONS =====\n" << std::endl;
    
    // Test the Nf6xe4 position (that was causing problems)
    const char* moveHistory1 = "e2e4 Ng8f6 Nb1c3";
    SetEnginePersonality(2); // Positional
    std::cout << "Black's response to e4 Nf6 Nc3 (Positional): " 
              << GetBestMove(moveHistory1, 3, false) << std::endl;
    
    SetEnginePersonality(3); // Solid
    std::cout << "Black's response to e4 Nf6 Nc3 (Solid): " 
              << GetBestMove(moveHistory1, 3, false) << std::endl;
    
    // Test another opening
    const char* moveHistory2 = "d2d4 Ng8f6 c2c4";
    SetEnginePersonality(0); // Standard
    std::cout << "Black's response to d4 Nf6 c4: " 
              << GetBestMove(moveHistory2, 3, false) << std::endl;
    
    // Test endgame evaluation
    const char* simpleEndgame = "4k3/4P3/8/8/8/8/8/4K3 w - - 0 1";
    std::cout << "White's move in pawn endgame: " 
              << GetBestMove(simpleEndgame, 3, true) << std::endl;

    * /
    return 0;
}
*/