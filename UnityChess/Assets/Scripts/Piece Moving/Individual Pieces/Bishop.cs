using System.Collections.Generic;
using DefaultNamespace;
using Piece_Moving.Helpful_Scripts;
using UnityEngine;

public class Bishop : ChessPiece
{
    public override List<Vector2Int> GetValidMoves(Vector2Int currentPos)
    {
        return GetLineMoves(currentPos, new[] {
            Constants.forwardLeft,
            Constants.forwardRight,
            Constants.backLeft,
            Constants.backRight
        });
    }

    private List<Vector2Int> GetLineMoves(Vector2Int startPos, Vector2Int[] directions)
    {
        List<Vector2Int> moves = new List<Vector2Int>();

        foreach (Vector2Int direction in directions)
        {
            Vector2Int current = startPos + direction;

            while (IsInsideBoard(current))
            {
                if (gameManager.IsTileEmpty(current))
                {
                    moves.Add(current);
                }
                else
                {
                    if (gameManager.IsEnemyPiece(new PieceDetails(current, isWhite)))
                        moves.Add(current);

                    break;
                }
                current += direction;
            }
        }

        return moves;
    }
}
