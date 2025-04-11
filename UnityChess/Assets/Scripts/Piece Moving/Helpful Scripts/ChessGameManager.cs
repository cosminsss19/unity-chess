using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using DefaultNamespace;
using Piece_Moving.Helpful_Scripts;
using UnityEngine;
using UnityEngine.UI;

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
    private Dictionary<string, GameObject> _tiles = new();
    private Dictionary<Vector2Int, GameObject> _pieces = new();

    private List<string> boardStateHistory = new List<string>();
    private int movesSinceCapturePawn = 0;
    
    public ChessPiece selectedPiece;
    public bool isWhiteTurn = true;

    public Material defaultBlackMaterial;
    public Material defaultWhiteMaterial;
    public Material highlightMaterial;
    
    [Header("Promotion UI")]
    public GameObject promotionPanel;
    public Transform promotionPiecesContainer;
    public GameObject whitePromotionPrefab; // Parent object containing prefabs
    public GameObject blackPromotionPrefab; // Parent object containing prefabs
    private Vector2Int _promotionPosition;
    private bool _isWaitingForPromotion = false;
    
    [Header("Promotion Piece Prefabs")]
    public GameObject whiteQueen;
    public GameObject blackQueen;
    public GameObject whiteRook;
    public GameObject blackRook;
    public GameObject whiteBishop;
    public GameObject blackBishop;
    public GameObject whiteKnight;
    public GameObject blackKnight;

    [DllImport("ChessEngine")]
    private static extern IntPtr GetBestMove(string boardState, int depth);
    private void Start()
    {
        StartFunction();
        if (promotionPanel != null)
            promotionPanel.SetActive(false);
        RectTransform promotionPanelRect = promotionPanel.GetComponent<RectTransform>();
        promotionPanelRect.anchorMin = new Vector2(0.5f, 0.5f);
        promotionPanelRect.anchorMax = new Vector2(0.5f, 0.5f);
        promotionPanelRect.pivot = new Vector2(0.5f, 0.5f);
        promotionPanelRect.anchoredPosition = Vector2.zero;
        
        string boardState = "start position in FEN"; // Replace with actual FEN string
        int depth = 3; // Example depth
        IntPtr bestMovePtr = GetBestMove(boardState, depth);
        string bestMove = Marshal.PtrToStringAnsi(bestMovePtr);
        Debug.Log("Best Move: " + bestMove);
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
            _tiles[tileName] = child.gameObject;
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
                _pieces[piecePos] = piece.gameObject;
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
        string tileName = _tiles.FirstOrDefault(t => t.Value.GetComponent<ChessTile>().tilePos == tilePos).Key;
        Debug.Log("Tile clicked: " + tileName);
    
        if (selectedPiece == null)
        {
            if (_pieces.TryGetValue(tilePos, out GameObject pieceObject))
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
            GameObject clickedTile = _tiles.Values.FirstOrDefault(t => t.GetComponent<ChessTile>().tilePos == tilePos);
            if (clickedTile != null)
            {
                var rendererMesh = clickedTile.GetComponentInChildren<MeshRenderer>();
                if (rendererMesh != null && rendererMesh.material.name == highlightMaterial.name + " (Instance)")
                {
                    if (selectedPiece.IsValidMove(tilePos))
                    {
                        MovePiece(selectedPiece, tilePos);
                        // Remove the EndTurn() call here - it's already called in MovePiece
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
    
    private void SetBoardInteraction(bool enabled)
    {
        // Disable/enable all chess piece colliders
        foreach (var piece in _pieces.Values)
        {
            if (piece != null)
            {
                Collider[] colliders = piece.GetComponentsInChildren<Collider>();
                foreach (Collider col in colliders)
                {
                    col.enabled = enabled;
                }
            }
        }

        // Disable/enable all tile colliders
        foreach (var tile in _tiles.Values)
        {
            if (tile != null)
            {
                Collider[] colliders = tile.GetComponentsInChildren<Collider>();
                foreach (Collider col in colliders)
                {
                    col.enabled = enabled;
                }
            }
        }
    }
    
    private void ShowPromotionUI(bool isWhite)
{
    Debug.Log("ShowPromotionUI called for " + (isWhite ? "white" : "black"));

    SetBoardInteraction(false);
    // Clear existing promotion panel contents
    foreach (Transform child in promotionPanel.transform)
    {
        Destroy(child.gameObject);
    }

    // Make sure promotion panel covers the entire screen
    RectTransform promotionPanelRect = promotionPanel.GetComponent<RectTransform>();
    promotionPanelRect.anchorMin = Vector2.zero;
    promotionPanelRect.anchorMax = Vector2.one;
    promotionPanelRect.offsetMin = Vector2.zero;
    promotionPanelRect.offsetMax = Vector2.zero;

    // Add a canvas group to the panel to control input blocking
    CanvasGroup panelCanvasGroup = promotionPanel.GetComponent<CanvasGroup>();
    if (panelCanvasGroup == null)
        panelCanvasGroup = promotionPanel.AddComponent<CanvasGroup>();
    
    panelCanvasGroup.blocksRaycasts = true;
    panelCanvasGroup.interactable = true;
    panelCanvasGroup.ignoreParentGroups = true;

    // Create overlay with input blocker
    GameObject overlay = new GameObject("DarkeningOverlay");
    overlay.transform.SetParent(promotionPanel.transform, false);
    
    // Add image component for visual effect
    Image overlayImage = overlay.AddComponent<Image>();
    overlayImage.color = new Color(0, 0, 0, 0.75f);
    overlayImage.raycastTarget = true;
    
    // Force the overlay to be full screen
    RectTransform overlayRect = overlay.GetComponent<RectTransform>();
    overlayRect.anchorMin = Vector2.zero;
    overlayRect.anchorMax = Vector2.one;
    overlayRect.offsetMin = Vector2.zero;
    overlayRect.offsetMax = Vector2.zero;
    overlayRect.SetAsFirstSibling();

    // Create dedicated input blocker that sits behind buttons but over the overlay
    GameObject inputBlocker = new GameObject("InputBlocker");
    inputBlocker.transform.SetParent(promotionPanel.transform, false);
    
    // Add transparent image as the raycast target
    Image blockerImage = inputBlocker.AddComponent<Image>();
    blockerImage.color = new Color(0, 0, 0, 0.01f); // Nearly invisible
    blockerImage.raycastTarget = true;
    
    RectTransform blockerRect = inputBlocker.GetComponent<RectTransform>();
    blockerRect.anchorMin = Vector2.zero;
    blockerRect.anchorMax = Vector2.one;
    blockerRect.offsetMin = Vector2.zero;
    blockerRect.offsetMax = Vector2.zero;

    // Create button panel on top
    GameObject buttonPanel = new GameObject("ButtonPanel");
    buttonPanel.transform.SetParent(promotionPanel.transform, false);
    buttonPanel.AddComponent<CanvasGroup>().blocksRaycasts = true;
    
    Image buttonPanelImage = buttonPanel.AddComponent<Image>();
    buttonPanelImage.color = new Color(0.2f, 0.2f, 0.2f, 0.9f);
    
    RectTransform buttonPanelRect = buttonPanel.GetComponent<RectTransform>();
    buttonPanelRect.anchorMin = new Vector2(0.5f, 0.5f);
    buttonPanelRect.anchorMax = new Vector2(0.5f, 0.5f);
    buttonPanelRect.pivot = new Vector2(0.5f, 0.5f);
    buttonPanelRect.sizeDelta = new Vector2(400, 120);
    
    // Add layout group
    HorizontalLayoutGroup layout = buttonPanel.AddComponent<HorizontalLayoutGroup>();
    layout.spacing = 10;
    layout.childAlignment = TextAnchor.MiddleCenter;
    layout.padding = new RectOffset(20, 20, 20, 20);
    layout.childControlWidth = false;
    layout.childControlHeight = false;
    
    // Create promotion buttons
    string spritePath = isWhite ? "WhitePieces/" : "BlackPieces/";
    CreatePromotionButton(buttonPanel.transform, spritePath + "Queen", "Queen");
    CreatePromotionButton(buttonPanel.transform, spritePath + "Rook", "Rook");
    CreatePromotionButton(buttonPanel.transform, spritePath + "Bishop", "Bishop");
    CreatePromotionButton(buttonPanel.transform, spritePath + "Knight", "Knight");
    
    // Ensure the panel is active and on top
    promotionPanel.SetActive(true);
    promotionPanel.transform.SetAsLastSibling();
}

    private void CreatePromotionButton(Transform parent, string spritePath, string pieceType)
    {
        GameObject button = new GameObject(pieceType + "Button");
        button.transform.SetParent(parent, false);
    
        // Add button component and image
        Image image = button.AddComponent<Image>();
        image.color = Color.white;
    
        // Load sprite from Resources
        Sprite sprite = Resources.Load<Sprite>(spritePath);
        if (sprite != null)
            image.sprite = sprite;
        else
            Debug.LogError($"Failed to load sprite: {spritePath}");
    
        // Configure RectTransform
        RectTransform rect = button.GetComponent<RectTransform>();
        rect.sizeDelta = new Vector2(80, 80);
    
        // Add button component
        Button buttonComp = button.AddComponent<Button>();
        ColorBlock colors = buttonComp.colors;
        colors.normalColor = Color.white;
        colors.highlightedColor = new Color(0.9f, 0.9f, 1f);
        buttonComp.colors = colors;
    
        // Add listener
        buttonComp.onClick.AddListener(() => PromotePawn(pieceType));
    }

    private void PromotePawn(string pieceType)
    {
        promotionPanel.SetActive(false);

        SetBoardInteraction(true);
        // Remove the pawn
        if (_pieces.TryGetValue(_promotionPosition, out GameObject pawn))
        {
            bool isWhite = pawn.GetComponent<ChessPiece>().isWhite;
            Destroy(pawn);
            _pieces.Remove(_promotionPosition);

            // Create the new piece
            GameObject newPiece = null;
            switch (pieceType.ToLower())
            {
                case "queen":
                    newPiece = Instantiate(isWhite ? whiteQueen : blackQueen);
                    break;
                case "rook":
                    newPiece = Instantiate(isWhite ? whiteRook : blackRook);
                    break;
                case "bishop":
                    newPiece = Instantiate(isWhite ? whiteBishop : blackBishop);
                    break;
                case "knight":
                    newPiece = Instantiate(isWhite ? whiteKnight : blackKnight);
                    break;
            }

            if (newPiece != null)
            {
                // Initialize the new piece
                ChessPiece chessPiece = newPiece.GetComponent<ChessPiece>();
                chessPiece.Initialize(isWhite, this);

                // Position it
                newPiece.transform.position = GetTileCenter(_promotionPosition) + Constants.offset;

                // Update the pieces dictionary
                _pieces[_promotionPosition] = newPiece;
            }
        }

        _isWaitingForPromotion = false;
        // Need to end the turn here since MovePiece doesn't do it for promotion cases
        EndTurn();
    }

    private void MovePiece(ChessPiece piece, Vector2Int targetPos)
    {
        bool isCapture = _pieces.ContainsKey(targetPos);

        // Reset En Passant for all pawns of current player
        foreach (var item in _pieces.Values)
        {
            ChessPiece chessPiece = item.GetComponent<ChessPiece>();
            if (chessPiece != null && chessPiece is Pawn && chessPiece.isWhite == isWhiteTurn)
            {
                var pawnScript = chessPiece as Pawn;
                pawnScript.EnPassantable = false;
            }
        }

        Vector2Int oldPos = GetPiecePosition(piece);
        if (oldPos == new Vector2Int(-1, -1))
        {
            Debug.LogError("Old position not found for piece: " + piece.name);
            return;
        }

        // Remove the piece from its old position in the dictionary
        _pieces.Remove(oldPos);

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
        if (piece is Pawn && Mathf.Abs(targetPos.y - oldPos.y) == 1 && _pieces.TryGetValue(targetPos, out var targetPiece) == false)
        {
            Vector2Int enPassantPos = new Vector2Int(oldPos.x, targetPos.y);
            if (_pieces.TryGetValue(enPassantPos, out var enPassantPiece) && enPassantPiece.GetComponent<Pawn>().EnPassantable)
            {
                Destroy(enPassantPiece);
                _pieces.Remove(enPassantPos);
            }
        }

        // Capture the target piece if it exists
        if (_pieces.TryGetValue(targetPos, out var targetPiece1))
        {
            Destroy(targetPiece1);
            _pieces.Remove(targetPos);
        }

        // Add the piece to its new position in the dictionary
        _pieces[targetPos] = piece.gameObject;
        piece.Move(targetPos);

        // Set En Passant target square
        if (piece is Pawn)
        {
            var pieceScript = piece as Pawn;
            if (Mathf.Abs(targetPos.x - oldPos.x) == 2)
                pieceScript.EnPassantable = true;
            else
            {
                pieceScript.EnPassantable = false;
            }
        }

        // Update move counter
        if (piece is Pawn || isCapture)
        {
            movesSinceCapturePawn = 0;
        }
        else
        {
            movesSinceCapturePawn++;
        }

        // Add the new board state to history
        boardStateHistory.Add(GenerateBoardStateKey());

        // Check for pawn promotion
        if (piece is Pawn && ((piece.isWhite && targetPos.x == 7) || (!piece.isWhite && targetPos.x == 0)))
        {
            _promotionPosition = targetPos;
            _isWaitingForPromotion = true;
            ShowPromotionUI(piece.isWhite);
        }
        else
        {
            // End turn normally if no promotion
            EndTurn();
        }
    }

    private void MoveRookForCastling(Vector2Int oldPos, Vector2Int newPos)
    {
        if (_pieces.TryGetValue(oldPos, out var rook))
        {
            _pieces.Remove(oldPos);
            _pieces[newPos] = rook;
            rook.GetComponent<ChessPiece>().Move(newPos);
        }
    }

    public Vector2Int GetPiecePosition(ChessPiece piece)
    {
        foreach (var kvp in _pieces)
        {
            if (kvp.Value == piece.gameObject)
                return kvp.Key;
        }
        Debug.LogError("Piece not found in dictionary!");
        return new Vector2Int(-1, -1);
    }

    public ChessPiece GetPieceAtPosition(Vector2Int position)
    {
        if (_pieces.TryGetValue(position, out GameObject pieceObject))
        {
            return pieceObject.GetComponent<ChessPiece>();
        }
        return null;
    }

    public bool IsTileEmpty(Vector2Int tilePos) =>
        //verify if the tile has any piece child without using "ContainsKey" method
        _pieces.All(p => p.Key != tilePos);

    public bool IsEnemyPiece(PieceDetails pieceDetails)
    {
        if (_pieces.TryGetValue(pieceDetails.TilePos, out GameObject pieceObject))
        {
            ChessPiece piece = pieceObject.GetComponent<ChessPiece>();
            return piece != null && piece.isWhite != pieceDetails.IsWhite;
        }

        return false;
    }

    public Vector3 GetTileCenter(Vector2Int tilePos)
    {
        foreach (var tile in _tiles.Values)
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
                    if (_pieces.TryGetValue(currentPos, out GameObject pieceObject))
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
                        if (_pieces.TryGetValue(currentPos, out GameObject pieceObject))
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
            foreach (var tile in _tiles.Values)
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
        foreach (var tile in _tiles.Values)
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
        else if (IsStalemate(isWhiteTurn))
        {
            Debug.Log("The game is a draw by stalemate!");
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
        foreach (var piece in _pieces.Values)
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
        foreach (var piece in _pieces)
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
        // 1. Check if the king has any valid moves
        Vector2Int kingPos = FindKingPosition(isWhite);
        ChessPiece king = GetPieceAtPosition(kingPos);
    
        // Check if king can move safely
        List<Vector2Int> kingMoves = king.GetValidMoves(kingPos);
        foreach (Vector2Int move in kingMoves)
        {
            if (!IsTileAttacked(move, isWhite))
            {
                return false; // King can escape check
            }
        }
    
        // 2. Identify attacking pieces and their attack paths
        List<ChessPiece> attackingPieces = new List<ChessPiece>();
        Dictionary<ChessPiece, List<Vector2Int>> attackPaths = new Dictionary<ChessPiece, List<Vector2Int>>();
    
        foreach (var piece in _pieces.Values)
        {
            ChessPiece chessPiece = piece.GetComponent<ChessPiece>();
            if (chessPiece != null && chessPiece.isWhite != isWhite)
            {
                Vector2Int attackerPos = GetPiecePosition(chessPiece);
            
                if (IsAttackingTile(chessPiece, attackerPos, kingPos))
                {
                    attackingPieces.Add(chessPiece);
                
                    // For sliding pieces, calculate the attack path
                    List<Vector2Int> path = new List<Vector2Int>();
                    if (chessPiece is Queen || chessPiece is Rook || chessPiece is Bishop)
                    {
                        int deltaX = kingPos.x - attackerPos.x;
                        int deltaY = kingPos.y - attackerPos.y;
                    
                        Vector2Int direction = new Vector2Int(
                            deltaX == 0 ? 0 : deltaX / Mathf.Abs(deltaX),
                            deltaY == 0 ? 0 : deltaY / Mathf.Abs(deltaY)
                        );
                    
                        Vector2Int pos = attackerPos + direction;
                        while (pos != kingPos)
                        {
                            path.Add(pos);
                            pos += direction;
                        }
                    }
                    attackPaths[chessPiece] = path;
                }
            }
        }
    
        // If more than one piece is attacking, the check can only be escaped by moving the king
        if (attackingPieces.Count > 1)
        {
            return true; // Checkmate (king can't move and multiple attackers)
        }
    
        // 3. Check if any friendly piece can block the attack or capture the attacker
        if (attackingPieces.Count == 1)
        {
            ChessPiece attacker = attackingPieces[0];
            Vector2Int attackerPos = GetPiecePosition(attacker);
            List<Vector2Int> blockingSquares = attackPaths[attacker];
            blockingSquares.Add(attackerPos); // Add attacker position to consider captures
        
            foreach (var piece in _pieces.Values)
            {
                ChessPiece defender = piece.GetComponent<ChessPiece>();
                if (defender != null && defender.isWhite == isWhite && !(defender is King))
                {
                    Vector2Int defenderPos = GetPiecePosition(defender);
                    List<Vector2Int> validMoves = defender.GetValidMoves(defenderPos);
                
                    foreach (Vector2Int move in validMoves)
                    {
                        if (blockingSquares.Contains(move))
                        {
                            // Check if this blocking move would be safe for the king
                            if (IsMoveSafeForKing(defender, defenderPos, move))
                            {
                                return false; // Check can be blocked or attacker captured
                            }
                        }
                    }
                }
            }
        }
    
        return true; // Checkmate (king can't move and check can't be blocked/captured)
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
    
    private bool HasInsufficientMaterial()
    {
        // Count pieces by type
        int whitePieces = 0, blackPieces = 0;
        int whiteBishops = 0, blackBishops = 0;
        int whiteKnights = 0, blackKnights = 0;
        bool whiteHasOtherPieces = false, blackHasOtherPieces = false;
        List<Vector2Int> whiteBishopPositions = new List<Vector2Int>();
        List<Vector2Int> blackBishopPositions = new List<Vector2Int>();

        foreach (var piece in _pieces.Values)
        {
            ChessPiece chessPiece = piece.GetComponent<ChessPiece>();
            if (chessPiece != null)
            {
                if (chessPiece.isWhite)
                {
                    whitePieces++;
                    if (chessPiece is Bishop)
                    {
                        whiteBishops++;
                        whiteBishopPositions.Add(GetPiecePosition(chessPiece));
                    }
                    else if (chessPiece is Knight) whiteKnights++;
                    else if (!(chessPiece is King)) whiteHasOtherPieces = true;
                }
                else
                {
                    blackPieces++;
                    if (chessPiece is Bishop)
                    {
                        blackBishops++;
                        blackBishopPositions.Add(GetPiecePosition(chessPiece));
                    }
                    else if (chessPiece is Knight) blackKnights++;
                    else if (!(chessPiece is King)) blackHasOtherPieces = true;
                }
            }
        }

        // King vs King
        if (whitePieces == 1 && blackPieces == 1)
            return true;

        // King and Bishop/Knight vs King
        if ((whitePieces == 2 && (whiteBishops == 1 || whiteKnights == 1) && blackPieces == 1) ||
            (blackPieces == 2 && (blackBishops == 1 || blackKnights == 1) && whitePieces == 1))
            return true;

        // King and Bishop vs King and Bishop (same color bishops)
        if (whitePieces == 2 && blackPieces == 2 && whiteBishops == 1 && blackBishops == 1)
        {
            // Check if bishops are on same colored squares
            // In chess, squares have the same color if (x+y)%2 gives the same result
            bool whiteBishopOnDarkSquare = (whiteBishopPositions[0].x + whiteBishopPositions[0].y) % 2 == 0;
            bool blackBishopOnDarkSquare = (blackBishopPositions[0].x + blackBishopPositions[0].y) % 2 == 0;
        
            if (whiteBishopOnDarkSquare == blackBishopOnDarkSquare)
                return true;
        }

        // King and two Knights vs King
        if ((whitePieces == 3 && whiteKnights == 2 && !whiteHasOtherPieces && blackPieces == 1) ||
            (blackPieces == 3 && blackKnights == 2 && !blackHasOtherPieces && whitePieces == 1))
            return true;

        return false;
    }
    
    private string GenerateBoardStateKey()
    {
        StringBuilder key = new StringBuilder();
    
        // 1. Board position - use Forsyth-Edwards Notation (FEN) style
        for (int x = 0; x < 8; x++)
        {
            int emptySquares = 0;
            for (int y = 0; y < 8; y++)
            {
                Vector2Int pos = new Vector2Int(x, y);
                if (_pieces.TryGetValue(pos, out GameObject pieceObject))
                {
                    // If there were empty squares before this piece, append the count
                    if (emptySquares > 0)
                    {
                        key.Append(emptySquares);
                        emptySquares = 0;
                    }
                
                    ChessPiece piece = pieceObject.GetComponent<ChessPiece>();
                    char pieceChar = GetPieceChar(piece);
                    key.Append(piece.isWhite ? char.ToUpper(pieceChar) : pieceChar);
                }
                else
                {
                    emptySquares++;
                }
            }
        
            // Append any remaining empty squares at the end of the rank
            if (emptySquares > 0)
            {
                key.Append(emptySquares);
            }
        
            // Add rank separator (except for the last rank)
            if (x < 7)
            {
                key.Append('/');
            }
        }
    
        // 2. Active player
        key.Append(' ').Append(isWhiteTurn ? 'w' : 'b');
    
        // 3. Castling availability
        key.Append(' ');
        bool castlingAvailable = false;
    
        // Check white kingside castling
        if (CanCastle(true, true))
        {
            key.Append('K');
            castlingAvailable = true;
        }
    
        // Check white queenside castling
        if (CanCastle(true, false))
        {
            key.Append('Q');
            castlingAvailable = true;
        }
    
        // Check black kingside castling
        if (CanCastle(false, true))
        {
            key.Append('k');
            castlingAvailable = true;
        }
    
        // Check black queenside castling
        if (CanCastle(false, false))
        {
            key.Append('q');
            castlingAvailable = true;
        }
    
        if (!castlingAvailable)
        {
            key.Append('-');
        }
    
        // 4. En passant target square
        key.Append(' ');
        bool enPassantAvailable = false;
    
        foreach (var piece in _pieces.Values)
        {
            Pawn pawn = piece.GetComponent<Pawn>();
            if (pawn != null && pawn.EnPassantable)
            {
                Vector2Int pawnPos = GetPiecePosition(pawn);
                int epRow = pawn.isWhite ? 2 : 5; // Row where the en passant capture would occur
                char file = (char)('a' + pawnPos.y);
                key.Append(file).Append(epRow + 1); // +1 because chess notation is 1-based
                enPassantAvailable = true;
                break; // Only one pawn can be en passantable at a time
            }
        }
    
        if (!enPassantAvailable)
        {
            key.Append('-');
        }
    
        // 5. Halfmove clock for 50-move rule
        key.Append(' ').Append(movesSinceCapturePawn);
    
        return key.ToString();
    }

    // Helper method to get character representation of a piece
    private char GetPieceChar(ChessPiece piece)
    {
        if (piece is Pawn) return 'p';
        if (piece is Rook) return 'r';
        if (piece is Knight) return 'n';
        if (piece is Bishop) return 'b';
        if (piece is Queen) return 'q';
        if (piece is King) return 'k';
        return '?';
    }

    // Helper method to check if castling is available
    private bool CanCastle(bool isWhite, bool kingSide)
    {
        // Find the king
        King king = null;
        Vector2Int kingPos = new Vector2Int(-1, -1);
    
        foreach (var piece in _pieces.Values)
        {
            ChessPiece chessPiece = piece.GetComponent<ChessPiece>();
            if (chessPiece is King && chessPiece.isWhite == isWhite)
            {
                king = (King)chessPiece;
                kingPos = GetPiecePosition(king);
                break;
            }
        }
    
        if (king == null || kingPos.x != (isWhite ? 0 : 7) || kingPos.y != 4)
        {
            return false; // King has moved or isn't in the starting position
        }
    
        // Check for rook
        Vector2Int rookPos = new Vector2Int(isWhite ? 0 : 7, kingSide ? 7 : 0);
        if (_pieces.TryGetValue(rookPos, out GameObject rookObj))
        {
            return rookObj.GetComponent<Rook>() != null;
        }
    
        return false;
    }
    
    private bool IsThreefoldRepetition()
    {
        string currentState = GenerateBoardStateKey();
        int count = 0;
    
        foreach (string state in boardStateHistory)
        {
            if (state == currentState)
                count++;
        }
    
        return count >= 2; // Current position + 2 previous occurrences
    }
    
    private bool IsFiftyMoveRule()
    {
        return movesSinceCapturePawn >= 100; // 50 moves by each player
    }
    
    private bool IsStalemate(bool isWhite)
    {
        // First check if king is in check - if so, it's not stalemate
        if (IsKingInCheck(isWhite))
        {
            return false;
        }

        // Check for threefold repetition
        if (IsThreefoldRepetition())
        {
            return true;
        }

        // Check for fifty-move rule
        if (IsFiftyMoveRule())
        {
            return true;
        }

        // Check for insufficient material
        if (HasInsufficientMaterial())
        {
            return true;
        }

        // Check if any piece has a legal move
        foreach (var piece in _pieces.Values)
        {
            ChessPiece chessPiece = piece.GetComponent<ChessPiece>();
            if (chessPiece != null && chessPiece.isWhite == isWhite)
            {
                Vector2Int currentPos = GetPiecePosition(chessPiece);
                List<Vector2Int> validMoves = chessPiece.GetValidMoves(currentPos);

                foreach (Vector2Int move in validMoves)
                {
                    // Check if this move would be safe for the king
                    if (IsMoveSafeForKing(chessPiece, currentPos, move))
                    {
                        return false; // Found a legal move, not stalemate
                    }
                }
            }
        }

        // No legal moves and not in check = stalemate
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

    public bool IsTileAttacked(Vector2Int targetPos, bool isWhite, Vector2Int? ignorePos = null)
{
    foreach (var piece in _pieces.Values)
    {
        ChessPiece chessPiece = piece.GetComponent<ChessPiece>();
        if (chessPiece != null && chessPiece.isWhite != isWhite)
        {
            Vector2Int piecePos = GetPiecePosition(chessPiece);
            if (IsAttackingTile(chessPiece, piecePos, targetPos, ignorePos))
            {
                return true;
            }
        }
    }
    return false;
}

private bool IsAttackingTile(ChessPiece piece, Vector2Int piecePos, Vector2Int targetPos, Vector2Int? ignorePos = null)
{
    // King attacks (1 square in any direction)
    if (piecePos == targetPos)
        return false;
    if (piece is King)
    {
        int dx = Mathf.Abs(targetPos.x - piecePos.x);
        int dy = Mathf.Abs(targetPos.y - piecePos.y);
        return dx <= 1 && dy <= 1 && (dx > 0 || dy > 0);
    }

    // Knight attacks
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
        return false;
    }

    // Pawn attacks
    if (piece is Pawn)
    {
        int direction = piece.isWhite ? 1 : -1;
        Vector2Int forwardLeft = new Vector2Int(direction, -1);
        Vector2Int forwardRight = new Vector2Int(direction, 1);
        return piecePos + forwardLeft == targetPos || piecePos + forwardRight == targetPos;
    }

    // Sliding pieces (Queen, Rook, Bishop)
    int deltaX = targetPos.x - piecePos.x;
    int deltaY = targetPos.y - piecePos.y;

    // Check if target is on same rank/file (Rook, Queen)
    bool onSameRankOrFile = deltaX == 0 || deltaY == 0;
    if (onSameRankOrFile && (piece is Rook || piece is Queen))
    {
        Vector2Int direction = new Vector2Int(
            deltaX == 0 ? 0 : deltaX / Mathf.Abs(deltaX),
            deltaY == 0 ? 0 : deltaY / Mathf.Abs(deltaY)
        );

        // Check if path is clear, ignoring the specified position
        Vector2Int checkPos = piecePos + direction;
        while (checkPos != targetPos)
        {
            if (checkPos != ignorePos && _pieces.ContainsKey(checkPos))
                return false; // Blocked
            checkPos += direction;
        }
        return true; // Clear path to target
    }

    // Check if target is on same diagonal (Bishop, Queen)
    bool onSameDiagonal = Mathf.Abs(deltaX) == Mathf.Abs(deltaY);
    if (onSameDiagonal && (piece is Bishop || piece is Queen))
    {
        Vector2Int direction = new Vector2Int(
            deltaX / Mathf.Abs(deltaX),
            deltaY / Mathf.Abs(deltaY)
        );

        // Check if path is clear, ignoring the specified position
        Vector2Int checkPos = piecePos + direction;
        while (checkPos != targetPos)
        {
            if (checkPos != ignorePos && _pieces.ContainsKey(checkPos))
                return false; // Blocked
            checkPos += direction;
        }
        return true; // Clear path to target
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
            return !IsTileAttacked(to, piece.isWhite, from);
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
                if (_pieces.TryGetValue(currentPos, out GameObject pieceObject))
                {
                    if (pieceObject != null)
                    {
                        ChessPiece blockingPiece = pieceObject.GetComponent<ChessPiece>();
                        if (blockingPiece is not King)
                            return true;
                        break;
                    }
                }

                currentPos += directionFromPiece;
            }

            directionFromPiece *= -1;
            currentPos = from + directionFromPiece;
            
            while (IsInsideBoard(currentPos))
            {
                if (_pieces.TryGetValue(currentPos, out GameObject pieceObject))
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
        foreach (var piece in _pieces.Values)
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