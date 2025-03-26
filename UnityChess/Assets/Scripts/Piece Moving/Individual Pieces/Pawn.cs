using System.Collections.Generic;
using Piece_Moving.Helpful_Scripts;
using UnityEngine;

public class Pawn : ChessPiece
{
    public bool enPassantable { get; set; }
    public override List<Vector2Int> GetValidMoves(Vector2Int currentPos)
    {
        List<Vector2Int> moves = new List<Vector2Int>();
        Vector2Int forward = isWhite ? Vector2Int.right : Vector2Int.left;
        Vector2Int forwardLeft = isWhite ? forward + Vector2Int.up : forward + Vector2Int.down;
        Vector2Int forwardRight = isWhite ? forward + Vector2Int.down : forward + Vector2Int.up;

        // Move forward
        Vector2Int pos = currentPos + forward;
        if (IsInsideBoard(pos) && gameManager.IsTileEmpty(pos))
        {
            moves.Add(pos);

            // Move two squares forward
            pos += forward;
            if (IsInsideBoard(pos) && gameManager.IsTileEmpty(pos) && !hasMoved)
                moves.Add(pos);
        }

        // Capture diagonally
        pos = currentPos + forwardLeft;
        if (IsInsideBoard(pos) && gameManager.IsEnemyPiece(new PieceDetails(pos, isWhite)))
            moves.Add(pos);
        pos = currentPos + forwardRight;
        if (IsInsideBoard(pos) && gameManager.IsEnemyPiece(new PieceDetails(pos, isWhite)))
            moves.Add(pos);

        // En Passant

        var testPiece = gameManager.GetPieceAtPosition(currentPos + Vector2Int.up);
        if (testPiece is Pawn && testPiece.isWhite != isWhite)
        {
            var pieceScript = testPiece as Pawn;
            if (pieceScript.enPassantable && IsInsideBoard(forward+Vector2Int.up))
                moves.Add(forward+Vector2Int.up);
        }
        
        var testPiece2 = gameManager.GetPieceAtPosition(currentPos + Vector2Int.down);
        if (testPiece2 is Pawn && testPiece2.isWhite != isWhite)
        {
            var pieceScript = testPiece2 as Pawn;
            if (pieceScript.enPassantable && IsInsideBoard(forward+Vector2Int.down))
                moves.Add(forward+Vector2Int.down);
        }

        return moves;
    }
}