using System.Collections.Generic;
using Piece_Moving.Helpful_Scripts;
using UnityEngine;

public class Pawn : ChessPiece
{
    public bool EnPassantable { get; set; }
    public override List<Vector2Int> GetValidMoves(Vector2Int currentPos)
    {
        List<Vector2Int> moves = new List<Vector2Int>();
        Vector2Int forward = isWhite ? Vector2Int.right : Vector2Int.left;
        Vector2Int right = isWhite ? Vector2Int.down : Vector2Int.up;
        Vector2Int left = isWhite ? Vector2Int.up : Vector2Int.down;
        Vector2Int forwardLeft = isWhite ? forward + Vector2Int.up : forward + Vector2Int.down;
        Vector2Int forwardRight = isWhite ? forward + Vector2Int.down : forward + Vector2Int.up;
        

        Vector2Int pos = currentPos + forward;
        if (IsInsideBoard(pos) && gameManager.IsTileEmpty(pos))
        {
            moves.Add(pos);

            pos += forward;
            if (IsInsideBoard(pos) && gameManager.IsTileEmpty(pos) && !hasMoved)
                moves.Add(pos);
        }

        pos = currentPos + forwardLeft;
        if (IsInsideBoard(pos) && gameManager.IsEnemyPiece(new PieceDetails(pos, isWhite)))
            moves.Add(pos);
        pos = currentPos + forwardRight;
        if (IsInsideBoard(pos) && gameManager.IsEnemyPiece(new PieceDetails(pos, isWhite)))
            moves.Add(pos);
        
        var testPiece = gameManager.GetPieceAtPosition(currentPos + left);
        if (testPiece is Pawn && testPiece.isWhite != isWhite)
        {
            var pieceScript = testPiece as Pawn;
            if (pieceScript.EnPassantable && IsInsideBoard(currentPos+forwardLeft))
                moves.Add(currentPos+forwardLeft);
        }
        
        var testPiece2 = gameManager.GetPieceAtPosition(currentPos + right);
        if (testPiece2 is Pawn && testPiece2.isWhite != isWhite)
        {
            var pieceScript = testPiece2 as Pawn;
            if (pieceScript.EnPassantable && IsInsideBoard(currentPos+forwardRight))
                moves.Add(currentPos+forwardRight);
        }

        return moves;
    }
}