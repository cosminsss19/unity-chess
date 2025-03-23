using System.Collections.Generic;
using DefaultNamespace;
using UnityEngine;

namespace Piece_Moving.Helpful_Scripts
{
    public class ChessPiece : MonoBehaviour
    {
        public bool isWhite;
        public bool hasMoved = false;

        public ChessGameManager gameManager;

        public virtual void Initialize(bool white, ChessGameManager manager)
        {
            isWhite = white;
            gameManager = manager;
        }

        public void OnMouseDown()
        {
            gameManager.OnPieceClicked(this);
        }

        public virtual List<Vector2Int> GetValidMoves(Vector2Int currentPos)
        {
            return new List<Vector2Int>();
        }

        public void Move(Vector2Int newPos)
        {
            transform.position = gameManager.GetTileCenter(newPos)+Constants.offset;
            hasMoved = true;
        }

        public bool IsValidMove(Vector2Int targetPos)
        {
            return GetValidMoves(gameManager.GetPiecePosition(this)).Contains(targetPos);
        }

        protected bool IsInsideBoard(Vector2Int position)
        {
            return position.x >= 0 && position.x < 8 && position.y >= 0 && position.y < 8;
        }
    }
}