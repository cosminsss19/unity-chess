using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Piece_Moving.Helpful_Scripts;
using UnityEngine;

public class PieceDetails
{
    public PieceDetails(Vector2Int tilePos, bool isWhite)
    {
        TilePos = tilePos;
        IsWhite = isWhite;
    }

    public Vector2Int TilePos { get; private set; }
    public bool IsWhite { get; private set; }
}

public class ChessGameManager : MonoBehaviour
{
    public GameObject board; // The parent board GameObject
    private Dictionary<string, GameObject> tiles = new();
    private Dictionary<Vector2Int, GameObject> pieces = new();

    public ChessPiece selectedPiece;
    public bool isWhiteTurn = true;

    public Material defaultBlackMaterial;
    public Material defaultWhiteMaterial;
    public Material highlightMaterial;

    private void Start()
    {
        StartFunction();
    }
    private async Task StartFunction()
    {
        await Task.Delay(1000);

        // Wait for the piece generation to complete
        while (!ArePiecesGenerated())
        {
            Debug.Log("Waiting for pieces to be generated...");
            await Task.Delay(100); // Check every 100ms
        }

        Debug.Log("Pieces generated. Initializing board and pieces...");
        InitializeBoard();
        InitializePieces();
    }

    private bool ArePiecesGenerated()
    {
        // Implement this method to return true when pieces are generated
        bool piecesGenerated = board.transform.childCount > 0 && board.transform.GetComponentsInChildren<ChessPiece>().Length > 0;
        Debug.Log($"ArePiecesGenerated: {piecesGenerated}");
        return piecesGenerated;
    }
    private void InitializeBoard()
    {
        foreach (Transform child in board.transform)
        {
            var tileName = child.gameObject.name;
            var tilePos = ChessNotationToVector(tileName);
            var tileScript = child.gameObject.AddComponent<ChessTile>();

            tileScript.Initialize(tileName, tilePos, this);
            tiles[tileName] = child.gameObject;
        }
    }

    private void InitializePieces()
    {
        foreach (Transform child in board.transform)
        {
            ChessPiece piece = child.GetComponentInChildren<ChessPiece>();
            if (piece != null)
            {
                Vector2Int piecePos = ChessNotationToVector(child.gameObject.name);
                pieces[piecePos] = piece.gameObject;
                Debug.Log($"Added piece {piece.name} at position {piecePos}");
            }
            else
            {
                Debug.Log($"No piece found on tile {child.gameObject.name}");
            }
        }
    }

    private Vector2Int ChessNotationToVector(string notation)
    {
        int x = notation[0] - 'a';
        int y = int.Parse(notation[1].ToString()) - 1;
        return new Vector2Int(x, y);
    }

    public void OnTileClicked(Vector2Int tilePos)
    {
        // Send a Debug Log with the name of the tile clicked
        string tileName = tiles.FirstOrDefault(t => t.Value.GetComponent<ChessTile>().tilePos == tilePos).Key;
        Debug.Log("Tile clicked: " + tileName);

        if (selectedPiece == null)
        {
            if (pieces.TryGetValue(tilePos, out GameObject pieceObject))
            {
                ChessPiece piece = pieceObject.GetComponent<ChessPiece>();
                if (piece != null && piece.isWhite == isWhiteTurn)
                {
                    selectedPiece = piece;
                    HighlightMoves(piece.GetValidMoves(tilePos));
                }
            }
        }
        else
        {
            GameObject clickedTile = tiles.Values.FirstOrDefault(t => t.GetComponent<ChessTile>().tilePos == tilePos);
            if (clickedTile != null)
            {
                var rendererMesh = clickedTile.GetComponentInChildren<MeshRenderer>();
                if (rendererMesh != null && rendererMesh.material.name == highlightMaterial.name + " (Instance)")
                {
                    if (selectedPiece.IsValidMove(tilePos))
                    {
                        MovePiece(selectedPiece, tilePos);
                        EndTurn();
                    }
                }
                else
                {
                    ClearHighlights();
                    selectedPiece = null;
                }
            }
        }
    }

    private void MovePiece(ChessPiece piece, Vector2Int targetPos)
    {
        foreach (var item in pieces.Values)
        {
            ChessPiece chessPiece = item.GetComponent<ChessPiece>();
            if (chessPiece != null && chessPiece is Pawn)
            {
                var pawnScript = chessPiece as Pawn;
                pawnScript.enPassantable = false;
            }
        }
        
        Vector2Int oldPos = GetPiecePosition(piece);
        if (oldPos == new Vector2Int(-1, -1))
        {
            Debug.LogError("Old position not found for piece: " + piece.name);
            return;
        }

        // Remove the piece from its old position in the dictionary
        pieces.Remove(oldPos);

        // Handle castling
        if (piece is King && Mathf.Abs(targetPos.y - oldPos.y) == 2)
        {
            // Kingside castling
            if (targetPos.y > oldPos.y)
            {
                Vector2Int rookOldPos = new Vector2Int(oldPos.x, 7);
                Vector2Int rookNewPos = new Vector2Int(oldPos.x, 5);
                MoveRookForCastling(rookOldPos, rookNewPos);
            }
            // Queenside castling
            else
            {
                Vector2Int rookOldPos = new Vector2Int(oldPos.x, 0);
                Vector2Int rookNewPos = new Vector2Int(oldPos.x, 3);
                MoveRookForCastling(rookOldPos, rookNewPos);
            }
        }

        // Handle En Passant capture
        if (piece is Pawn && Mathf.Abs(targetPos.y - oldPos.y) == 1 && pieces.TryGetValue(targetPos, out var targetPiece) == false)
        {
            Vector2Int enPassantPos = new Vector2Int(oldPos.x, targetPos.y);
            if (pieces.TryGetValue(enPassantPos, out var enPassantPiece) && enPassantPiece.GetComponent<Pawn>().enPassantable)
            {
                Destroy(enPassantPiece);
                pieces.Remove(enPassantPos);
            }
        }

        // Capture the target piece if it exists
        if (pieces.TryGetValue(targetPos, out var targetPiece1))
        {
            Destroy(targetPiece1);
            pieces.Remove(targetPos);
        }

        // Add the piece to its new position in the dictionary
        pieces[targetPos] = piece.gameObject;
        piece.Move(targetPos);

        // Set En Passant target square
        if (piece is Pawn)
        {
            var pieceScript = piece as Pawn;
            if (Mathf.Abs(targetPos.x - oldPos.x) == 2)
                pieceScript.enPassantable = true;
            else
            {
                pieceScript.enPassantable = false;
            }
        }
    }

    private void MoveRookForCastling(Vector2Int oldPos, Vector2Int newPos)
    {
        if (pieces.TryGetValue(oldPos, out var rook))
        {
            pieces.Remove(oldPos);
            pieces[newPos] = rook;
            rook.GetComponent<ChessPiece>().Move(newPos);
        }
    }

    public Vector2Int GetPiecePosition(ChessPiece piece)
    {
        foreach (var kvp in pieces)
        {
            if (kvp.Value == piece.gameObject)
                return kvp.Key;
        }
        Debug.LogError("Piece not found in dictionary!");
        return new Vector2Int(-1, -1);
    }

    public ChessPiece GetPieceAtPosition(Vector2Int position)
    {
        if (pieces.TryGetValue(position, out GameObject pieceObject))
        {
            return pieceObject.GetComponent<ChessPiece>();
        }
        return null;
    }

    public bool IsTileEmpty(Vector2Int tilePos) =>
        //verify if the tile has any piece child without using "ContainsKey" method
        pieces.All(p => p.Key != tilePos);

    public bool IsEnemyPiece(PieceDetails pieceDetails)
    {
        if (pieces.TryGetValue(pieceDetails.TilePos, out GameObject pieceObject))
        {
            ChessPiece piece = pieceObject.GetComponent<ChessPiece>();
            return piece != null && piece.isWhite != pieceDetails.IsWhite;
        }

        return false;
    }

    public Vector3 GetTileCenter(Vector2Int tilePos)
    {
        foreach (var tile in tiles.Values)
        {
            ChessTile tileScript = tile.GetComponent<ChessTile>();
            if (tileScript.tilePos == tilePos)
            {
                return tile.transform.position;
            }
        }
        return Vector3.zero;
    }

    private void HighlightMoves(List<Vector2Int> moves)
    {
        List<Vector2Int> validMoves = new List<Vector2Int>();
        Vector2Int originalPosition = GetPiecePosition(selectedPiece);

        if (IsKingInCheck(isWhiteTurn))
        {
            Vector2Int kingPos = FindKingPosition(isWhiteTurn);
            List<Vector2Int> attackPath = GetAttackPathToKing(kingPos, isWhiteTurn);
            ChessPiece attackingPiece = GetAttackingPiece(kingPos, isWhiteTurn);
            Vector2Int attackingPos = GetPiecePosition(attackingPiece);

            foreach (Vector2Int move in moves)
            {
                if (selectedPiece is King)
                {
                    if (IsMoveSafeForKing(selectedPiece, originalPosition, move))
                    {
                        validMoves.Add(move);
                    }
                }
                else if (attackPath.Contains(move) || move == attackingPos)
                {
                    if (IsMoveSafeForKing(selectedPiece, originalPosition, move))
                    {
                        validMoves.Add(move);
                    }
                }
            }
        }
        else
        {
            Vector2Int kingPos = FindKingPosition(selectedPiece.isWhite);
            List<Vector2Int> directions = new List<Vector2Int>
            {
                new Vector2Int(1, 0), new Vector2Int(-1, 0), new Vector2Int(0, 1), new Vector2Int(0, -1),
                new Vector2Int(1, 1), new Vector2Int(-1, -1), new Vector2Int(1, -1), new Vector2Int(-1, 1)
            };

            foreach (Vector2Int direction in directions)
            {

                bool kingFound = false;
                Vector2Int currentPos = originalPosition + direction;
                while (IsInsideBoard(currentPos))
                {
                    if (pieces.TryGetValue(currentPos, out GameObject pieceObject))
                    {
                        ChessPiece piece = pieceObject.GetComponent<ChessPiece>();
                        if (piece is King && piece.isWhite == selectedPiece.isWhite)
                        {
                            kingFound = true;
                        }

                        break;
                    }
                    currentPos += direction;
                }

                if (kingFound)
                {
                    currentPos = originalPosition - direction;
                    while (IsInsideBoard(currentPos))
                    {
                        if (pieces.TryGetValue(currentPos, out GameObject pieceObject))
                        {
                            ChessPiece piece = pieceObject.GetComponent<ChessPiece>();
                            if (piece.isWhite != selectedPiece.isWhite && (piece is Rook || piece is Queen || piece is Bishop))
                            {
                                foreach (Vector2Int move in moves)
                                {
                                    if (move == currentPos || move == originalPosition + direction)
                                    {
                                        validMoves.Add(move);
                                    }
                                }
                                break;
                            }
                            else
                            {
                                break;
                            }
                        }
                        currentPos -= direction;
                    }
                }
            }

            if (validMoves.Count == 0)
            {
                foreach (Vector2Int move in moves)
                {
                    if (IsMoveSafeForKing(selectedPiece, originalPosition, move))
                    {
                        validMoves.Add(move);
                    }
                }
            }
        }

        foreach (Vector2Int move in validMoves)
        {
            foreach (var tile in tiles.Values)
            {
                ChessTile tileScript = tile.GetComponent<ChessTile>();
                if (tileScript.tilePos == move)
                {
                    var rendererMesh = tile.GetComponentInChildren<MeshRenderer>();
                    if (rendererMesh != null)
                        rendererMesh.material = highlightMaterial;
                }
            }
        }
    }

    private void ClearHighlights()
    {
        foreach (var tile in tiles.Values)
        {
            Renderer renderereMesh = tile.GetComponentInChildren<MeshRenderer>();
            if (renderereMesh != null)
            {
                if (tile.CompareTag("BlackTile"))
                {
                    renderereMesh.material = defaultBlackMaterial;
                }
                else if (tile.CompareTag("WhiteTile"))
                {
                    renderereMesh.material = defaultWhiteMaterial;
                }
            }
        }
    }

    private void EndTurn()
    {
        isWhiteTurn = !isWhiteTurn;

        if (IsKingInCheck(isWhiteTurn))
        {
            if (IsCheckmate(isWhiteTurn))
            {
                Debug.Log(!isWhiteTurn ? "White wins by checkmate!" : "Black wins by checkmate!");
                EndGame();
            }
            else
            {
                Debug.Log(!isWhiteTurn ? "Black is in check!" : "White is in check!");
            }
        }
        else if (IsStalemate(!isWhiteTurn))
        {
            ShowPopup("The game is a draw by stalemate!");
            EndGame();
        }

        selectedPiece = null;
        ClearHighlights();
    }

    private void ShowPopup(string message)
    {
        
    }

    private bool IsKingInCheck(bool isWhite)
    {
        Vector2Int kingPos = FindKingPosition(isWhite);
        foreach (var piece in pieces.Values)
        {
            ChessPiece chessPiece = piece.GetComponent<ChessPiece>();
            if (chessPiece != null && chessPiece.isWhite != isWhite)
            {
                Vector2Int currentPos = GetPiecePosition(chessPiece);
                List<Vector2Int> validMoves = chessPiece.GetValidMoves(currentPos);

                if (validMoves.Contains(kingPos))
                {
                    return true;
                }
            }
        }

        return false;
    }
    private Vector2Int FindKingPosition(bool isWhite)
    {
        foreach (var piece in pieces)
        {
            ChessPiece chessPiece = piece.Value.GetComponent<ChessPiece>();
            if (chessPiece != null && chessPiece is King && chessPiece.isWhite == isWhite)
            {
                return piece.Key;
            }
        }
        Debug.LogError("King not found!");
        return new Vector2Int(-1, -1);
    }
    private bool IsCheckmate(bool isWhite)
    {
        // Check if the king has any valid moves
        Vector2Int kingPos = FindKingPosition(isWhite);
        ChessPiece king = GetPieceAtPosition(kingPos);
        List<Vector2Int> kingMoves = king.GetValidMoves(kingPos);

        if (kingMoves.Count > 0)
        {
            return false;
        }

        // Get all attacking pieces
        List<ChessPiece> attackingPieces = new List<ChessPiece>();
        foreach (var piece in pieces.Values)
        {
            ChessPiece chessPiece = piece.GetComponent<ChessPiece>();
            if (chessPiece != null && chessPiece.isWhite != isWhite)
            {
                Vector2Int currentPos = GetPiecePosition(chessPiece);
                List<Vector2Int> validMoves = chessPiece.GetValidMoves(currentPos);

                if (validMoves.Contains(kingPos))
                {
                    attackingPieces.Add(chessPiece);
                }
            }
        }

        // If there is more than one attacking piece, the check cannot be blocked
        if (attackingPieces.Count > 1)
        {
            return true;
        }

        // Check if any other piece can block the check or capture the attacking piece
        ChessPiece attackingPiece = attackingPieces[0];
        Vector2Int attackingPos = GetPiecePosition(attackingPiece);
        List<Vector2Int> attackPath = GetAttackPath(attackingPiece, attackingPos, kingPos);

        foreach (var piece in pieces.Values)
        {
            ChessPiece chessPiece = piece.GetComponent<ChessPiece>();
            if (chessPiece != null && chessPiece.isWhite == isWhite)
            {
                Vector2Int currentPos = GetPiecePosition(chessPiece);
                List<Vector2Int> validMoves = chessPiece.GetValidMoves(currentPos);

                foreach (Vector2Int move in validMoves)
                {
                    if (move == attackingPos || attackPath.Contains(move))
                    {
                        ChessPiece capturedPiece = SimulateMove(chessPiece, currentPos, move);

                        if (!IsKingInCheck(isWhite))
                        {
                            UndoSimulatedMove(chessPiece, currentPos, move, capturedPiece);
                            return false;
                        }

                        UndoSimulatedMove(chessPiece, currentPos, move, capturedPiece);
                    }
                }
            }
        }

        return true;
    }

    private List<Vector2Int> GetAttackPath(ChessPiece attackingPiece, Vector2Int attackingPos, Vector2Int kingPos)
    {
        List<Vector2Int> path = new List<Vector2Int>();

        if (attackingPiece is Bishop || attackingPiece is Rook || attackingPiece is Queen)
        {
            Vector2Int direction = new Vector2Int(
                Mathf.Clamp(kingPos.x - attackingPos.x, -1, 1),
                Mathf.Clamp(kingPos.y - attackingPos.y, -1, 1)
            );

            Vector2Int currentPos = attackingPos + direction;
            while (currentPos != kingPos)
            {
                path.Add(currentPos);
                currentPos += direction;
            }
        }

        return path;
    }

    private ChessPiece SimulateMove(ChessPiece piece, Vector2Int from, Vector2Int to)
    {
        ChessPiece capturedPiece = null;
       
        if (pieces.TryGetValue(to, out GameObject capturedObject))
        {
            capturedPiece = capturedObject.GetComponent<ChessPiece>();
            pieces.Remove(to);
        }

        pieces.Remove(from);
        pieces[to] = piece.gameObject;

        return capturedPiece;
    }

    private void UndoSimulatedMove(ChessPiece piece, Vector2Int from, Vector2Int to, ChessPiece capturedPiece)
    {
        pieces.Remove(to);
        pieces[from] = piece.gameObject;

        if (capturedPiece != null)
        {
            pieces[to] = capturedPiece.gameObject;
        }

        // Restore En Passant state
        if (piece is Pawn && Mathf.Abs(to.x - from.x) == 2)
        {
            //EnPassantMove = new Vector2Int((from.x + to.x) / 2, from.y);
        }
        else
        {
            //EnPassantMove = null;
        }
    }
    private bool IsStalemate(bool isWhite)
    {
        foreach (var piece in pieces.Values)
        {
            ChessPiece chessPiece = piece.GetComponent<ChessPiece>();
            if (chessPiece != null && chessPiece.isWhite == isWhite)
            {
                Vector2Int currentPos = GetPiecePosition(chessPiece);
                List<Vector2Int> validMoves = chessPiece.GetValidMoves(currentPos);

                foreach (Vector2Int move in validMoves)
                {
                    ChessPiece capturedPiece = SimulateMove(chessPiece, currentPos, move);

                    if (!IsKingInCheck(isWhite))
                    {
                        UndoSimulatedMove(chessPiece, currentPos, move, capturedPiece);
                        return false;
                    }

                    UndoSimulatedMove(chessPiece, currentPos, move, capturedPiece);
                }
            }
        }

        return true;
    }
    private void EndGame()
    {
        Debug.Log("Game Over");
        enabled = false;
    }

    public void OnPieceClicked(ChessPiece piece)
    {
        ClearHighlights();
        if (piece.isWhite != isWhiteTurn)
        {
            Debug.Log("It's not your turn!");
            return;
        }

        Debug.Log("Correct turn!");

        selectedPiece = piece;
        //send a Debug Log that tells if the selectedPiece is null or not, and the piece name if it isnt null
        Debug.Log(selectedPiece == null ? "Selected piece is null" : "Selected piece is " + selectedPiece.name);
        Vector2Int currentPos = GetPiecePosition(piece);

        HighlightMoves(selectedPiece.GetValidMoves(currentPos));
    }

    public bool IsTileAttacked(Vector2Int currentPos, bool isWhite)
    {
        foreach (var piece in pieces.Values)
        {
            ChessPiece chessPiece = piece.GetComponent<ChessPiece>();
            if (chessPiece != null && chessPiece.isWhite != isWhite && !(chessPiece is King))
            {
                Vector2Int piecePos = GetPiecePosition(chessPiece);
                if (IsAttackingTile(chessPiece, piecePos, currentPos))
                {
                    return true;
                }
            }
        }
        return false;
    }

    private bool IsAttackingTile(ChessPiece piece, Vector2Int piecePos, Vector2Int targetPos)
    {
        if (piece is Knight)
        {
            Vector2Int[] offsets =
            {
                new Vector2Int(2, 1), new Vector2Int(2, -1),
                new Vector2Int(-2, 1), new Vector2Int(-2, -1),
                new Vector2Int(1, 2), new Vector2Int(1, -2),
                new Vector2Int(-1, 2), new Vector2Int(-1, -2)
            };
            foreach (Vector2Int offset in offsets)
            {
                if (piecePos + offset == targetPos)
                    return true;
            }
        }
        else if (piece is Pawn)
        {
            Vector2Int forwardLeft = piece.isWhite ? new Vector2Int(-1, 1) : new Vector2Int(1, -1);
            Vector2Int forwardRight = piece.isWhite ? new Vector2Int(1, 1) : new Vector2Int(-1, -1);
            if (piecePos + forwardLeft == targetPos || piecePos + forwardRight == targetPos)
                return true;
        }
        else
        {
            List<Vector2Int> moves = piece.GetValidMoves(piecePos);
            if (moves.Contains(targetPos))
                return true;
        }
        return false;
    }

    private bool IsMoveSafeForKing(ChessPiece piece, Vector2Int from, Vector2Int to)
    {
        Vector2Int kingPos = piece is King ? to : FindKingPosition(piece.isWhite);
        Vector2Int directionToKing = kingPos - from;
        Vector2Int directionOfMove = to - from;
        Vector2Int directionOfMoveNormalized = new Vector2Int(
            directionOfMove.x == 0 ? 0 : directionOfMove.x / Mathf.Abs(directionOfMove.x),
            directionOfMove.y == 0 ? 0 : directionOfMove.y / Mathf.Abs(directionOfMove.y)
        );

        if (piece is King)
        {
            return !IsTileAttacked(to, piece.isWhite);
        }

        if (directionToKing.x == 0 || directionToKing.y == 0 ||
            Mathf.Abs(directionToKing.x) == Mathf.Abs(directionToKing.y))
        {
            Vector2Int directionFromPiece = new Vector2Int(
                directionToKing.x == 0 ? 0 : directionToKing.x / Mathf.Abs(directionToKing.x),
                directionToKing.y == 0 ? 0 : directionToKing.y / Mathf.Abs(directionToKing.y)
            );

            Vector2Int currentPos = from + directionFromPiece;
            while (IsInsideBoard(currentPos))
            {
                if (pieces.TryGetValue(currentPos, out GameObject pieceObject))
                {
                    if (pieceObject != null)
                    {
                        ChessPiece blockingPiece = pieceObject.GetComponent<ChessPiece>();
                        if (blockingPiece is not King)
                            return true;
                    }
                }

                currentPos += directionFromPiece;
            }

            directionFromPiece *= -1;
            currentPos = from + directionFromPiece;
            
            while (IsInsideBoard(currentPos))
            {
                if (pieces.TryGetValue(currentPos, out GameObject pieceObject))
                {
                    ChessPiece blockingPiece = pieceObject.GetComponent<ChessPiece>();
                    if (blockingPiece.isWhite != piece.isWhite)
                    {
                        if ((directionToKing.x == 0 || directionToKing.y == 0) &&
                            (blockingPiece is Rook || blockingPiece is Queen))
                            return directionOfMoveNormalized == directionFromPiece;

                        if (Mathf.Abs(directionToKing.x) == Mathf.Abs(directionToKing.y) &&
                            (blockingPiece is Bishop || blockingPiece is Queen))
                            return directionOfMoveNormalized == directionFromPiece;
                        return true;
                    }
                    else if (blockingPiece.isWhite == piece.isWhite)
                        return true;
                }

                currentPos += directionFromPiece;
            }

            return true;
        }

        return true;
    }

    private bool IsInsideBoard(Vector2Int currentPos)
    {
        return currentPos.x >= 0 && currentPos.x < 8 && currentPos.y >= 0 && currentPos.y < 8;
    }
    
    private List<Vector2Int> GetAttackPathToKing(Vector2Int kingPos, bool isWhite)
    {
        List<Vector2Int> path = new List<Vector2Int>();
        ChessPiece attackingPiece = GetAttackingPiece(kingPos, isWhite);
        if (attackingPiece == null || attackingPiece is Knight)
        {
            return path;
        }

        Vector2Int attackingPos = GetPiecePosition(attackingPiece);
        Vector2Int direction = new Vector2Int(
            Mathf.Clamp(kingPos.x - attackingPos.x, -1, 1),
            Mathf.Clamp(kingPos.y - attackingPos.y, -1, 1)
        );

        Vector2Int currentPos = attackingPos + direction;
        while (currentPos != kingPos)
        {
            path.Add(currentPos);
            currentPos += direction;
        }

        return path;
    }
    
    private ChessPiece GetAttackingPiece(Vector2Int kingPos, bool isWhite)
    {
        foreach (var piece in pieces.Values)
        {
            ChessPiece chessPiece = piece.GetComponent<ChessPiece>();
            if (chessPiece != null && chessPiece.isWhite != isWhite)
            {
                Vector2Int currentPos = GetPiecePosition(chessPiece);
                List<Vector2Int> validMoves = chessPiece.GetValidMoves(currentPos);

                if (validMoves.Contains(kingPos))
                {
                    return chessPiece;
                }
            }
        }
        return null;
    }
}