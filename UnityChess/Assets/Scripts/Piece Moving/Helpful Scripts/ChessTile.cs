using UnityEngine;

namespace Piece_Moving.Helpful_Scripts
{
    public class ChessTile : MonoBehaviour
    {
        public string tileName;
        public Vector2Int tilePos;

        private ChessGameManager gameManager;

        public void Initialize(string nameTile, Vector2Int position, ChessGameManager manager)
        {
            tileName = nameTile;
            tilePos = position;
            gameManager = manager;
        }

        private void OnMouseDown()
        {
            gameManager.OnTileClicked(tilePos);
        }
    }
}
