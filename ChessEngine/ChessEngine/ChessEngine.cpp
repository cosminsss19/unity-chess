#include "pch.h"
#include "ChessEngine.h"
#include <string>

const char* GetBestMove(const char* boardState, int depth)
{
    static std::string bestMove = "e2e4";
	return bestMove.c_str();
}
