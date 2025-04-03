using UnityEngine;

namespace Piece_Moving.Helpful_Scripts
{
    public class ChessTile : MonoBehaviour
    {
        public string tileName;
        public Vector2Int tilePos;

        private ChessGameManager _gameManager;

        public void Initialize(string nameTile, Vector2Int position, ChessGameManager manager)
        {
            tileName = nameTile;
            tilePos = position;
            _gameManager = manager;
        }

        private void OnMouseDown()
        {
            _gameManager.OnTileClicked(tilePos);
        }
    }
}
