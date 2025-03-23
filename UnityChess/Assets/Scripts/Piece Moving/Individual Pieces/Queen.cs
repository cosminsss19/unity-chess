using System.Collections.Generic;
using Piece_Moving.Helpful_Scripts;
using UnityEngine;

public class Queen : ChessPiece
{
    public override List<Vector2Int> GetValidMoves(Vector2Int currentPos)
    {
        List<Vector2Int> moves = new List<Vector2Int>();

        Vector2Int[] directions = {
            Vector2Int.up, Vector2Int.down, Vector2Int.left, Vector2Int.right,
            Vector2Int.up + Vector2Int.right, Vector2Int.up + Vector2Int.left,
            Vector2Int.down + Vector2Int.right, Vector2Int.down + Vector2Int.left
        };

        foreach (Vector2Int dir in directions)
        {
            Vector2Int target = currentPos + dir;
            while (IsInsideBoard(target))
            {
                if (gameManager.IsTileEmpty(target))
                {
                    moves.Add(target);
                }
                else
                {
                    if (gameManager.IsEnemyPiece(new PieceDetails(target, isWhite)))
                    {
                        moves.Add(target);
                    }
                    break;
                }
                target += dir;
            }
        }

        return moves;
    }
}
