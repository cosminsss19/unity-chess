using System.Collections.Generic;
using Piece_Moving.Helpful_Scripts;
using UnityEngine;

public class Knight : ChessPiece
{
    public override List<Vector2Int> GetValidMoves(Vector2Int currentPos)
    {
        List<Vector2Int> moves = new List<Vector2Int>();
        Vector2Int[] offsets =
        {
            new Vector2Int(2, 1), new Vector2Int(2, -1),
            new Vector2Int(-2, 1), new Vector2Int(-2, -1),
            new Vector2Int(1, 2), new Vector2Int(1, -2),
            new Vector2Int(-1, 2), new Vector2Int(-1, -2)
        };

        foreach (Vector2Int offset in offsets)
        {
            Vector2Int pos = currentPos + offset;
            if (IsInsideBoard(pos) && (gameManager.IsTileEmpty(pos) || gameManager.IsEnemyPiece(new PieceDetails(pos, isWhite))))
                moves.Add(pos);
        }
        return moves;
    }
}
