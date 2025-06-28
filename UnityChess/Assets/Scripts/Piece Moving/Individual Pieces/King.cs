using System.Collections.Generic;
using DefaultNamespace;
using Piece_Moving.Helpful_Scripts;
using UnityEngine;

public class King : ChessPiece
{
    public override List<Vector2Int> GetValidMoves(Vector2Int currentPos)
    {
        List<Vector2Int> moves = new List<Vector2Int>();
        Vector2Int[] offsets =
        {
            Constants.forward, Constants.back,
            Constants.left, Constants.right,
            Constants.forwardLeft, Constants.forwardRight,
            Constants.backLeft, Constants.backRight
        };

        foreach (Vector2Int offset in offsets)
        {
            Vector2Int pos = currentPos + offset;
            if (IsInsideBoard(pos) && (gameManager.IsTileEmpty(pos)
                                       || gameManager.IsEnemyPiece(new PieceDetails(pos, isWhite)))
                                   && !gameManager.IsTileAttacked(pos, isWhite))
                moves.Add(pos);
        }

        if (!hasMoved)
        {
            if (IsKingsideCastlingPossible(currentPos))
                moves.Add(currentPos + Vector2Int.up * 2);

            if (IsQueensideCastlingPossible(currentPos))
                moves.Add(currentPos + Vector2Int.down * 2);
        }

        return moves;
    }

    private bool IsKingsideCastlingPossible(Vector2Int currentPos)
    {
        Vector2Int rookPos = new Vector2Int(currentPos.x, 7);
        if (gameManager.GetPieceAtPosition(rookPos) is Rook rook && !rook.hasMoved)
        {
            Vector2Int pos1 = new Vector2Int(currentPos.x, 5);
            Vector2Int pos2 = new Vector2Int(currentPos.x, 6);
            if (gameManager.IsTileEmpty(pos1) && gameManager.IsTileEmpty(pos2) &&
                !gameManager.IsTileAttacked(currentPos, isWhite) &&
                !gameManager.IsTileAttacked(pos1, isWhite) &&
                !gameManager.IsTileAttacked(pos2, isWhite))
            {
                return true;
            }
        }
        return false;
    }

    private bool IsQueensideCastlingPossible(Vector2Int currentPos)
    {
        Vector2Int rookPos = new Vector2Int(currentPos.x, 0);
        if (gameManager.GetPieceAtPosition(rookPos) is Rook rook && !rook.hasMoved)
        {
            Vector2Int pos1 = new Vector2Int(currentPos.x, 1);
            Vector2Int pos2 = new Vector2Int(currentPos.x, 2);
            Vector2Int pos3 = new Vector2Int(currentPos.x, 3);
            if (gameManager.IsTileEmpty(pos1) && gameManager.IsTileEmpty(pos2) && gameManager.IsTileEmpty(pos3) &&
                !gameManager.IsTileAttacked(currentPos, isWhite) &&
                !gameManager.IsTileAttacked(pos1, isWhite) &&
                !gameManager.IsTileAttacked(pos2, isWhite))
            {
                return true;
            }
        }
        return false;
    }
}