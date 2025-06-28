using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using DefaultNamespace;
using Piece_Moving.Helpful_Scripts;
using UnityEngine;
using UnityEngine.SceneManagement;
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
    public GameObject board;
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
    public GameObject whitePromotionPrefab;
    public GameObject blackPromotionPrefab;
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

    [Header("Audio")]
    [SerializeField] private AudioSource audioSource;
    [SerializeField] private AudioClip pieceMoveSound;
    [SerializeField] private AudioClip checkSound;
    [SerializeField] private AudioClip gameOverSound;
    
    [DllImport("ChessEngine")]
    private static extern IntPtr GetBestMove(string movesHistory, int depth, bool isWhite);
    [DllImport("ChessEngine")]
    private static extern void SetEnginePersonality(int personalityType);
    private List<string> moveHistory = new List<string>();
    
    [SerializeField] private bool playAgainstComputer = true;
    [SerializeField] private bool computerPlaysBlack = true;
    [SerializeField] private ChessPersonality computerPersonality = ChessPersonality.STANDARD;
    [SerializeField] private int computerSearchDepth = 3;
    
    [SerializeField] private bool useOpeningSequence = true;
    private readonly string[] openingSequence = new string[]
    {
        "e2e4", "c7c5", "Ng1f3", "d7d6", "d2d4", "c5d4", "Nf3d4", "Ng8f6", "Nb1c3"
    };
    
    [Header("Game Over UI")]
    [SerializeField] private GameObject gameOverPanel;
    [SerializeField] private Text gameOverMessage;
    [SerializeField] private Button playAgainButton;
    private void Start()
    {
        if (GameSettingsManager.Instance != null)
        {
            playAgainstComputer = GameSettingsManager.Instance.PlayAgainstComputer;
            computerPlaysBlack = !GameSettingsManager.Instance.ComputerPlaysWhite;
            computerPersonality = GameSettingsManager.Instance.ComputerPersonality;
        }
        SetEnginePersonality((int)computerPersonality);
        if (useOpeningSequence && playAgainstComputer)
        {
            StartCoroutine(ApplyOpeningSequence());
        }
        else
        {
            StartFunction();
        }
        if (promotionPanel != null)
            promotionPanel.SetActive(false);
        RectTransform promotionPanelRect = promotionPanel.GetComponent<RectTransform>();
        promotionPanelRect.anchorMin = new Vector2(0.5f, 0.5f);
        promotionPanelRect.anchorMax = new Vector2(0.5f, 0.5f);
        promotionPanelRect.pivot = new Vector2(0.5f, 0.5f);
        promotionPanelRect.anchoredPosition = Vector2.zero;

        string moves = string.Join(" ", moveHistory);
        int depth = 3;
        IntPtr bestMovePtr = GetBestMove(moves, depth, !computerPlaysBlack);
        string bestMove = Marshal.PtrToStringAnsi(bestMovePtr);
        Debug.Log("Best Move: " + bestMove);
        if (playAgainstComputer && !computerPlaysBlack)
        {
            StartCoroutine(MakeComputerMove());
        }
        
    }
    
    private IEnumerator ApplyOpeningSequence()
    {
        StartFunction();

        yield return new WaitUntil(() => _pieces.Count > 0);
        yield return new WaitForSeconds(0.2f);

        Debug.Log("Applying opening sequence...");
    
        foreach (string moveNotation in openingSequence)
        {
            (Vector2Int fromPos, Vector2Int toPos) = ParseNotation(moveNotation);
        
            if (!_pieces.ContainsKey(fromPos))
            {
                Debug.LogWarning($"Skipping move {moveNotation}: No piece at {fromPos}");
                continue;
            }
        
            GameObject movingPiece = _pieces[fromPos];
            ChessPiece piece = movingPiece.GetComponent<ChessPiece>();
        
            if (_pieces.ContainsKey(toPos))
            {
                Destroy(_pieces[toPos]);
                _pieces.Remove(toPos);
            }
        
            _pieces.Remove(fromPos);
            _pieces[toPos] = movingPiece;
        
            movingPiece.transform.position = GetTileCenter(toPos) + Constants.offset;
        
            if (piece != null)
            {
                piece.hasMoved = true;
            }
        
            moveHistory.Add(moveNotation);
        }
    
        isWhiteTurn = false;
    
        Debug.Log("Opening position set up. Black to move.");
    
        if (playAgainstComputer && computerPlaysBlack)
        {
            StartCoroutine(MakeComputerMove());
        }
    }

    private (Vector2Int from, Vector2Int to) ParseNotation(string move)
    {
        if (move == "O-O")
        {
            return isWhiteTurn ? 
                (new Vector2Int(0, 4), new Vector2Int(0, 6)) : 
                (new Vector2Int(7, 4), new Vector2Int(7, 6));
        }
        else if (move == "O-O-O")
        {
            return isWhiteTurn ? 
                (new Vector2Int(0, 4), new Vector2Int(0, 2)) : 
                (new Vector2Int(7, 4), new Vector2Int(7, 2));
        }
    
        string cleanMove = move;
        if (move.Length > 4 && char.IsUpper(move[0]))
        {
            cleanMove = move.Substring(1);
        }
    
        int fromFile = cleanMove[0] - 'a';
        int fromRank = cleanMove[1] - '1';
        int toFile = cleanMove[2] - 'a';
        int toRank = cleanMove[3] - '1';
    
        return (new Vector2Int(fromRank, fromFile), new Vector2Int(toRank, toFile));
    }
    private (Vector2Int from, Vector2Int to, string promotion) ParseUciMove(string uciMove)
    {
        if (uciMove == "O-O")
        {
            return isWhiteTurn ? 
                (new Vector2Int(0, 4), new Vector2Int(0, 6), null) : 
                (new Vector2Int(7, 4), new Vector2Int(7, 6), null);
        }
        else if (uciMove == "O-O-O")
        {
            return isWhiteTurn ? 
                (new Vector2Int(0, 4), new Vector2Int(0, 2), null) : 
                (new Vector2Int(7, 4), new Vector2Int(7, 2), null);
        }
    
        string cleanMove = uciMove;
        if (uciMove.Length > 0 && char.IsUpper(uciMove[0]) && "NBRQK".Contains(uciMove[0]))
        {
            cleanMove = uciMove.Substring(1);
        }
        cleanMove = cleanMove.Replace("x", "");
    
        if (cleanMove.Length < 4) return (Vector2Int.zero, Vector2Int.zero, null);
    
        char fromFileChar = cleanMove[0];
        char fromRankChar = cleanMove[1];
        char toFileChar = cleanMove[2];
        char toRankChar = cleanMove[3];
    
        int fromFile = fromFileChar - 'a';
        int fromRank = fromRankChar - '1';
        int toFile = toFileChar - 'a';
        int toRank = toRankChar - '1';
    
        Vector2Int fromPos = new Vector2Int(fromRank, fromFile);
        Vector2Int toPos = new Vector2Int(toRank, toFile);
    
        string promotion = null;
        if (cleanMove.Length > 4)
        {
            promotion = GetPromotionTypeFromChar(cleanMove[4]);
        }
    
        return (fromPos, toPos, promotion);
    }
    private void LogMoveHistory()
    {
        if (moveHistory.Count > 0)
        {
            int moveNumber = (moveHistory.Count + 1) / 2;
            string lastMove = moveHistory[moveHistory.Count - 1];
            bool isWhiteMove = moveHistory.Count % 2 == 1;
            string playerColor = isWhiteMove ? "White" : "Black";

            if (lastMove == "O-O" || lastMove == "O-O-O" || 
                (lastMove.Length >= 5 && lastMove.Contains("=")))
            {
                Debug.Log($"Move {moveNumber}{(isWhiteMove ? "." : "...")} {playerColor}: {lastMove}");
                return;
            }

            Debug.Log($"Move {moveNumber}{(isWhiteMove ? "." : "...")} {playerColor}: {lastMove}");
        }
    }
    private async Task StartFunction()
    {
        await Task.Delay(1000);

        while (!ArePiecesGenerated())
        {
            Debug.Log("Waiting for pieces to be generated...");
            await Task.Delay(100);
        }

        Debug.Log("Pieces generated. Initializing board and pieces...");
        InitializeBoard();
        InitializePieces();
    }

    private bool ArePiecesGenerated()
    {
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

    private void PlayMoveSound()
    {
        if (audioSource != null && pieceMoveSound != null)
        {
            audioSource.PlayOneShot(pieceMoveSound);
        }
    }
    
    private void PlayCheckSound()
    {
        if (audioSource != null && checkSound != null)
        {
            audioSource.PlayOneShot(checkSound);
        }
    }
    
    private void PlayGameOverSound()
    {
        if (audioSource != null && gameOverSound != null)
        {
            audioSource.PlayOneShot(gameOverSound);
        }
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
        foreach (Transform child in promotionPanel.transform)
        {
            Destroy(child.gameObject);
        }

        RectTransform promotionPanelRect = promotionPanel.GetComponent<RectTransform>();
        promotionPanelRect.anchorMin = Vector2.zero;
        promotionPanelRect.anchorMax = Vector2.one;
        promotionPanelRect.offsetMin = Vector2.zero;
        promotionPanelRect.offsetMax = Vector2.zero;

        CanvasGroup panelCanvasGroup = promotionPanel.GetComponent<CanvasGroup>();
        if (panelCanvasGroup == null)
            panelCanvasGroup = promotionPanel.AddComponent<CanvasGroup>();
    
        panelCanvasGroup.blocksRaycasts = true;
        panelCanvasGroup.interactable = true;
        panelCanvasGroup.ignoreParentGroups = true;

        GameObject overlay = new GameObject("DarkeningOverlay");
        overlay.transform.SetParent(promotionPanel.transform, false);
    
        Image overlayImage = overlay.AddComponent<Image>();
        overlayImage.color = new Color(0, 0, 0, 0.75f);
        overlayImage.raycastTarget = true;
    
        RectTransform overlayRect = overlay.GetComponent<RectTransform>();
        overlayRect.anchorMin = Vector2.zero;
        overlayRect.anchorMax = Vector2.one;
        overlayRect.offsetMin = Vector2.zero;
        overlayRect.offsetMax = Vector2.zero;
        overlayRect.SetAsFirstSibling();

        GameObject inputBlocker = new GameObject("InputBlocker");
        inputBlocker.transform.SetParent(promotionPanel.transform, false);
    
        Image blockerImage = inputBlocker.AddComponent<Image>();
        blockerImage.color = new Color(0, 0, 0, 0.01f);
        blockerImage.raycastTarget = true;
    
        RectTransform blockerRect = inputBlocker.GetComponent<RectTransform>();
        blockerRect.anchorMin = Vector2.zero;
        blockerRect.anchorMax = Vector2.one;
        blockerRect.offsetMin = Vector2.zero;
        blockerRect.offsetMax = Vector2.zero;

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
    
        HorizontalLayoutGroup layout = buttonPanel.AddComponent<HorizontalLayoutGroup>();
        layout.spacing = 10;
        layout.childAlignment = TextAnchor.MiddleCenter;
        layout.padding = new RectOffset(20, 20, 20, 20);
        layout.childControlWidth = false;
        layout.childControlHeight = false;
    
        string spritePath = isWhite ? "WhitePieces/" : "BlackPieces/";
        CreatePromotionButton(buttonPanel.transform, spritePath + "Queen", "Queen");
        CreatePromotionButton(buttonPanel.transform, spritePath + "Rook", "Rook");
        CreatePromotionButton(buttonPanel.transform, spritePath + "Bishop", "Bishop");
        CreatePromotionButton(buttonPanel.transform, spritePath + "Knight", "Knight");
    
        promotionPanel.SetActive(true);
        promotionPanel.transform.SetAsLastSibling();
    }

    private void CreatePromotionButton(Transform parent, string spritePath, string pieceType)
    {
        GameObject button = new GameObject(pieceType + "Button");
        button.transform.SetParent(parent, false);
    
        Image image = button.AddComponent<Image>();
        image.color = Color.white;
    
        Sprite sprite = Resources.Load<Sprite>(spritePath);
        if (sprite != null)
            image.sprite = sprite;
        else
            Debug.LogError($"Failed to load sprite: {spritePath}");
    
        RectTransform rect = button.GetComponent<RectTransform>();
        rect.sizeDelta = new Vector2(80, 80);
    
        Button buttonComp = button.AddComponent<Button>();
        ColorBlock colors = buttonComp.colors;
        colors.normalColor = Color.white;
        colors.highlightedColor = new Color(0.9f, 0.9f, 1f);
        buttonComp.colors = colors;
    
        buttonComp.onClick.AddListener(() => PromotePawn(pieceType));
    }

    private void PromotePawn(string pieceType)
    {
        promotionPanel.SetActive(false);

        SetBoardInteraction(true);
        
        RecordPromotion(pieceType);
        if (_pieces.TryGetValue(_promotionPosition, out GameObject pawn))
        {
            bool isWhite = pawn.GetComponent<ChessPiece>().isWhite;
            Destroy(pawn);
            _pieces.Remove(_promotionPosition);

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
                ChessPiece chessPiece = newPiece.GetComponent<ChessPiece>();
                chessPiece.Initialize(isWhite, this);

                newPiece.transform.position = GetTileCenter(_promotionPosition) + Constants.offset;

                _pieces[_promotionPosition] = newPiece;
            }
        }

        _isWaitingForPromotion = false;
        EndTurn();
    }

    private IEnumerator AnimatePieceMovement(GameObject pieceObject, Vector3 startPos, Vector3 endPos)
    {
        float duration = 0.3f;
        float elapsedTime = 0f;
        float maxHeight = 1.5f;

        while (elapsedTime < duration)
        {
            elapsedTime += Time.deltaTime;
            float t = elapsedTime / duration;
        
            float normalizedHeight = Mathf.Sin(t * Mathf.PI);
            float yOffset = normalizedHeight * maxHeight;
        
            Vector3 currentPos = Vector3.Lerp(startPos, endPos, t);
            currentPos.y = currentPos.y + yOffset;
        
            pieceObject.transform.position = currentPos;
            yield return null;
        }
    
        pieceObject.transform.position = endPos;
    }
    
    private void MovePiece(ChessPiece piece, Vector2Int targetPos)
    {
        bool isCapture = _pieces.ContainsKey(targetPos);

        Vector2Int oldPos = GetPiecePosition(piece);
        Vector3 startPos = piece.transform.position;
        Vector3 endPos = GetTileCenter(targetPos) + Constants.offset;

        if (oldPos == new Vector2Int(-1, -1))
        {
            Debug.LogError("Old position not found for piece: " + piece.name);
            return;
        }

        string moveNotation = GetMoveNotation(oldPos, targetPos);
        moveHistory.Add(moveNotation);
        LogMoveHistory(); 
        
        foreach (var item in _pieces.Values)
        {
            ChessPiece chessPiece = item.GetComponent<ChessPiece>();
            if (chessPiece != null && chessPiece is Pawn && chessPiece.isWhite == isWhiteTurn)
            {
                var pawnScript = chessPiece as Pawn;
                pawnScript.EnPassantable = false;
            }
        }

        _pieces.Remove(oldPos);

        if (piece is King && Mathf.Abs(targetPos.y - oldPos.y) == 2)
        {
            if (targetPos.y > oldPos.y)
            {
                Vector2Int rookOldPos = new Vector2Int(oldPos.x, 7);
                Vector2Int rookNewPos = new Vector2Int(oldPos.x, 5);
                MoveRookForCastling(rookOldPos, rookNewPos);
            }
            else
            {
                Vector2Int rookOldPos = new Vector2Int(oldPos.x, 0);
                Vector2Int rookNewPos = new Vector2Int(oldPos.x, 3);
                MoveRookForCastling(rookOldPos, rookNewPos);
            }
        }

        if (piece is Pawn && Mathf.Abs(targetPos.y - oldPos.y) == 1 && _pieces.TryGetValue(targetPos, out var targetPiece) == false)
        {
            Vector2Int enPassantPos = new Vector2Int(oldPos.x, targetPos.y);
            if (_pieces.TryGetValue(enPassantPos, out var enPassantPiece) && enPassantPiece.GetComponent<Pawn>().EnPassantable)
            {
                Destroy(enPassantPiece);
                _pieces.Remove(enPassantPos);
            }
        }

        if (_pieces.TryGetValue(targetPos, out var targetPiece1))
        {
            Destroy(targetPiece1);
            _pieces.Remove(targetPos);
        }

        _pieces[targetPos] = piece.gameObject;
    
        PlayMoveSound();
    
        StartCoroutine(AnimatePieceMovement(piece.gameObject, startPos, endPos));
    
        piece.hasMoved = true;

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

        if (piece is Pawn || isCapture)
        {
            movesSinceCapturePawn = 0;
        }
        else
        {
            movesSinceCapturePawn++;
        }

        boardStateHistory.Add(GenerateBoardStateKey());

        if (piece is Pawn && ((piece.isWhite && targetPos.x == 7) || (!piece.isWhite && targetPos.x == 0)))
        {
            _promotionPosition = targetPos;
            _isWaitingForPromotion = true;
            ShowPromotionUI(piece.isWhite);
        }
        else
        {
            EndTurn();
        }
    }

    private string GetMoveNotation(Vector2Int from, Vector2Int to)
    {
        ChessPiece piece = _pieces[from].GetComponent<ChessPiece>();
    
        if (piece is King && Mathf.Abs(to.y - from.y) == 2)
        {
            if (to.y > from.y)
                return "O-O";
            else
                return "O-O-O";
        }
    
        string pieceChar = GetPieceNotationChar(piece);
    
        string fromSquare = PositionToChessNotation(from);
        string toSquare = PositionToChessNotation(to);
    
        bool isCapture = _pieces.ContainsKey(to);
        string captureSymbol = isCapture ? "x" : "";
    
        return $"{pieceChar}{fromSquare}{captureSymbol}{toSquare}";
    }

    private string GetPieceNotationChar(ChessPiece piece)
    {
        if (piece is King) return "K";
        if (piece is Queen) return "Q";
        if (piece is Rook) return "R";
        if (piece is Bishop) return "B";
        if (piece is Knight) return "N";
        if (piece is Pawn) return "";
        return "?";
    }

    private string PositionToChessNotation(Vector2Int pos)
    {
        char file = (char)('a' + pos.y);
        int rank = pos.x + 1;
        return $"{file}{rank}";
    }
    
    private void RecordPromotion(string pieceType)
    {
        if (moveHistory.Count > 0)
        {
            string lastMove = moveHistory[moveHistory.Count - 1];
            char promotionChar = GetPromotionChar(pieceType);
            moveHistory[moveHistory.Count - 1] = lastMove + promotionChar;
        }
    }

    private char GetPromotionChar(string pieceType)
    {
        switch (pieceType.ToLower())
        {
            case "queen": return 'q';
            case "rook": return 'r';
            case "bishop": return 'b';
            case "knight": return 'n';
            default: return 'q';
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

    public void RestartGame()
    {
        SceneManager.LoadScene("GameModeSelection");
    }
    private void EndTurn()
    {
        isWhiteTurn = !isWhiteTurn;

        if (IsKingInCheck(isWhiteTurn))
        {
            if (IsCheckmate(isWhiteTurn))
            {
                string winner = isWhiteTurn ? "Black" : "White";
                ShowPopup($"{winner} wins by checkmate!");
                EndGame();
                return;
            }
            PlayCheckSound();
            Debug.Log($"{(isWhiteTurn ? "White" : "Black")} is in check.");
        }
        else if (IsStalemate(isWhiteTurn))
        {
            string reason = "by stalemate";
        
            if (IsThreefoldRepetition())
                reason = "by threefold repetition";
            else if (IsFiftyMoveRule())
                reason = "by fifty-move rule";
            else if (HasInsufficientMaterial())
                reason = "by insufficient material";
            
            ShowPopup($"Game drawn {reason}");
            EndGame();
            return;
        }

        selectedPiece = null;
        ClearHighlights();

        if (playAgainstComputer && ((isWhiteTurn && !computerPlaysBlack) || (!isWhiteTurn && computerPlaysBlack)))
        {
            StartCoroutine(MakeComputerMove());
        }
    }
    
    private IEnumerator MakeComputerMove()
    {
        yield return new WaitForSeconds(0.5f);

        string moves = string.Join(" ", moveHistory);
        Debug.Log($"Sending moves to engine: {moves}");

        bool computerIsWhite = !computerPlaysBlack;
    
        IntPtr bestMovePtr = GetBestMove(moves, computerSearchDepth, computerIsWhite);
        string bestMove = Marshal.PtrToStringAnsi(bestMovePtr);

        if (string.IsNullOrEmpty(bestMove))
        {
            Debug.LogError("Engine returned null or empty move");
            yield break;
        }

        Debug.Log("Computer plays: " + bestMove);
    
        string cleanMove = bestMove;
        if (bestMove.Length > 0 && char.IsUpper(bestMove[0]) &&
            "NBRQK".Contains(bestMove[0]))
        {
            cleanMove = bestMove.Substring(1);
        }
    
        if (cleanMove.Length < 4) yield break;
    
        char fromFileChar = bestMove[0];
        char fromRankChar = bestMove[1];
        char toFileChar = bestMove[2];
        char toRankChar = bestMove[3];

        int fromFile = fromFileChar - 'a';
        int fromRank = fromRankChar - '1';
        int toFile = toFileChar - 'a';
        int toRank = toRankChar - '1';

        Vector2Int fromPos = new Vector2Int(fromRank, fromFile);
        Vector2Int toPos = new Vector2Int(toRank, toFile);

        foreach (var tile in _tiles.Values)
        {
            ChessTile tileScript = tile.GetComponent<ChessTile>();
            if (tileScript.tilePos == fromPos)
            {
                var rendererMesh = tile.GetComponentInChildren<MeshRenderer>();
                if (rendererMesh != null)
                    rendererMesh.material = highlightMaterial;
            }
        }

        yield return new WaitForSeconds(0.3f);
    
        foreach (var tile in _tiles.Values)
        {
            ChessTile tileScript = tile.GetComponent<ChessTile>();
            if (tileScript.tilePos == toPos)
            {
                var rendererMesh = tile.GetComponentInChildren<MeshRenderer>();
                if (rendererMesh != null)
                    rendererMesh.material = highlightMaterial;
            }
        }
    
        yield return new WaitForSeconds(0.2f);

        ExecuteUciMove(bestMove);
    
        yield return new WaitForSeconds(0.2f);
        ClearHighlights();
    }

    public void ReturnToMenu()
    {
        SceneManager.LoadScene("GameModeSelection");
    }

    private void ExecuteUciMove(string uciMove)
    {
        Debug.Log($"Executing UCI move: {uciMove}");

        string cleanMove = uciMove;
        if (uciMove.Length > 0 && char.IsUpper(uciMove[0]) &&
            "NBRQK".Contains(uciMove[0]))
        {
            cleanMove = uciMove.Substring(1);
        }

        cleanMove = cleanMove.Replace("x", "");

        if (cleanMove.Length < 4)
        {
            Debug.LogError($"Invalid move format: {uciMove}");
            return;
        }

        char fromFileChar = cleanMove[0];
        char fromRankChar = cleanMove[1];
        char toFileChar = cleanMove[2];
        char toRankChar = cleanMove[3];

        int fromFile = fromFileChar - 'a';
        int fromRank = fromRankChar - '1';
        int toFile = toFileChar - 'a';
        int toRank = toRankChar - '1';

        Vector2Int fromPos = new Vector2Int(fromRank, fromFile);
        Vector2Int toPos = new Vector2Int(toRank, toFile);

        if (_pieces.TryGetValue(fromPos, out GameObject pieceObj))
        {
            ChessPiece piece = pieceObj.GetComponent<ChessPiece>();
            if (piece != null)
            {
                string promotionType = null;
                if (cleanMove.Length > 4)
                {
                    char promotionChar = cleanMove[4];
                    promotionType = GetPromotionTypeFromChar(promotionChar);
                }

                MovePiece(piece, toPos);

                if (promotionType != null && _isWaitingForPromotion)
                {
                    PromotePawn(promotionType);
                }
            }
        }
        else
        {
            Debug.LogError($"No piece found at position {fromPos} for move {uciMove}");
        }
    }

    private string GetPromotionTypeFromChar(char c)
    {
        switch (char.ToLower(c))
        {
            case 'q': return "Queen";
            case 'r': return "Rook";
            case 'b': return "Bishop";
            case 'n': return "Knight";
            default: return "Queen";
        }
    }
    
    private void ShowPopup(string message)
    {
        PlayGameOverSound();
        
        SetBoardInteraction(false);
    
        gameOverMessage.text = message;
    
        gameOverPanel.SetActive(true);
    
        gameOverPanel.transform.SetAsLastSibling();
    
        Debug.Log("Game Over: " + message);
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
        Vector2Int kingPos = FindKingPosition(isWhite);
        ChessPiece king = GetPieceAtPosition(kingPos);
    
        List<Vector2Int> kingMoves = king.GetValidMoves(kingPos);
        foreach (Vector2Int move in kingMoves)
        {
            if (!IsTileAttacked(move, isWhite))
            {
                return false;
            }
        }
    
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
    
        if (attackingPieces.Count > 1)
        {
            return true;
        }
    
        if (attackingPieces.Count == 1)
        {
            ChessPiece attacker = attackingPieces[0];
            Vector2Int attackerPos = GetPiecePosition(attacker);
            List<Vector2Int> blockingSquares = attackPaths[attacker];
            blockingSquares.Add(attackerPos);
        
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
                            if (IsMoveSafeForKing(defender, defenderPos, move))
                            {
                                return false;
                            }
                        }
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
    
    private bool HasInsufficientMaterial()
    {
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

        if (whitePieces == 1 && blackPieces == 1)
            return true;

        if ((whitePieces == 2 && (whiteBishops == 1 || whiteKnights == 1) && blackPieces == 1) ||
            (blackPieces == 2 && (blackBishops == 1 || blackKnights == 1) && whitePieces == 1))
            return true;

        if (whitePieces == 2 && blackPieces == 2 && whiteBishops == 1 && blackBishops == 1)
        {
            bool whiteBishopOnDarkSquare = (whiteBishopPositions[0].x + whiteBishopPositions[0].y) % 2 == 0;
            bool blackBishopOnDarkSquare = (blackBishopPositions[0].x + blackBishopPositions[0].y) % 2 == 0;
        
            if (whiteBishopOnDarkSquare == blackBishopOnDarkSquare)
                return true;
        }

        if ((whitePieces == 3 && whiteKnights == 2 && !whiteHasOtherPieces && blackPieces == 1) ||
            (blackPieces == 3 && blackKnights == 2 && !blackHasOtherPieces && whitePieces == 1))
            return true;

        return false;
    }
    
    private string GenerateBoardStateKey()
    {
        StringBuilder key = new StringBuilder();
    
        for (int x = 0; x < 8; x++)
        {
            int emptySquares = 0;
            for (int y = 0; y < 8; y++)
            {
                Vector2Int pos = new Vector2Int(x, y);
                if (_pieces.TryGetValue(pos, out GameObject pieceObject))
                {
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
        
            if (emptySquares > 0)
            {
                key.Append(emptySquares);
            }
        
            if (x < 7)
            {
                key.Append('/');
            }
        }
    
        key.Append(' ').Append(isWhiteTurn ? 'w' : 'b');
    
        key.Append(' ');
        bool castlingAvailable = false;
    
        if (CanCastle(true, true))
        {
            key.Append('K');
            castlingAvailable = true;
        }
    
        if (CanCastle(true, false))
        {
            key.Append('Q');
            castlingAvailable = true;
        }
    
        if (CanCastle(false, true))
        {
            key.Append('k');
            castlingAvailable = true;
        }
    
        if (CanCastle(false, false))
        {
            key.Append('q');
            castlingAvailable = true;
        }
    
        if (!castlingAvailable)
        {
            key.Append('-');
        }
    
        key.Append(' ');
        bool enPassantAvailable = false;
    
        foreach (var piece in _pieces.Values)
        {
            Pawn pawn = piece.GetComponent<Pawn>();
            if (pawn != null && pawn.EnPassantable)
            {
                Vector2Int pawnPos = GetPiecePosition(pawn);
                int epRow = pawn.isWhite ? 2 : 5;
                char file = (char)('a' + pawnPos.y);
                key.Append(file).Append(epRow + 1);
                enPassantAvailable = true;
                break;
            }
        }
    
        if (!enPassantAvailable)
        {
            key.Append('-');
        }
    
        key.Append(' ').Append(movesSinceCapturePawn);
    
        return key.ToString();
    }

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

    private bool CanCastle(bool isWhite, bool kingSide)
    {
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
            return false;
        }
    
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
    
        return count >= 2;
    }
    
    private bool IsFiftyMoveRule()
    {
        return movesSinceCapturePawn >= 100;
    }
    
    private bool IsStalemate(bool isWhite)
    {
        if (IsKingInCheck(isWhite))
        {
            return false;
        }

        if (IsThreefoldRepetition())
        {
            return true;
        }

        if (IsFiftyMoveRule())
        {
            return true;
        }

        if (HasInsufficientMaterial())
        {
            return true;
        }

        foreach (var piece in _pieces.Values)
        {
            ChessPiece chessPiece = piece.GetComponent<ChessPiece>();
            if (chessPiece != null && chessPiece.isWhite == isWhite)
            {
                Vector2Int currentPos = GetPiecePosition(chessPiece);
                List<Vector2Int> validMoves = chessPiece.GetValidMoves(currentPos);

                foreach (Vector2Int move in validMoves)
                {
                    if (IsMoveSafeForKing(chessPiece, currentPos, move))
                    {
                        return false;
                    }
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
        Vector2Int clickedPiecePos = GetPiecePosition(piece);

        if (selectedPiece == null)
        {
            if (piece.isWhite == isWhiteTurn)
            {
                selectedPiece = piece;
                HighlightMoves(selectedPiece.GetValidMoves(clickedPiecePos));
            }
        }
        else if (piece.isWhite != isWhiteTurn)
        {
            bool isHighlighted = false;
            foreach (var tile in _tiles.Values)
            {
                ChessTile tileScript = tile.GetComponent<ChessTile>();
                if (tileScript.tilePos == clickedPiecePos)
                {
                    var rendererMesh = tile.GetComponentInChildren<MeshRenderer>();
                    if (rendererMesh != null && 
                        rendererMesh.material.name == highlightMaterial.name + " (Instance)")
                    {
                        isHighlighted = true;
                        break;
                    }
                }
            }

            if (isHighlighted && selectedPiece.IsValidMove(clickedPiecePos))
            {
                MovePiece(selectedPiece, clickedPiecePos);
            }
        }
        else if (piece.isWhite == isWhiteTurn)
        {
            ClearHighlights();
            selectedPiece = piece;
            HighlightMoves(selectedPiece.GetValidMoves(clickedPiecePos));
        }
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
        if (piecePos == targetPos)
            return false;
        if (piece is King)
        {
            int dx = Mathf.Abs(targetPos.x - piecePos.x);
            int dy = Mathf.Abs(targetPos.y - piecePos.y);
            return dx <= 1 && dy <= 1 && (dx > 0 || dy > 0);
        }

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

        if (piece is Pawn)
        {
            int direction = piece.isWhite ? 1 : -1;
            Vector2Int forwardLeft = new Vector2Int(direction, -1);
            Vector2Int forwardRight = new Vector2Int(direction, 1);
            return piecePos + forwardLeft == targetPos || piecePos + forwardRight == targetPos;
        }

        int deltaX = targetPos.x - piecePos.x;
        int deltaY = targetPos.y - piecePos.y;

        bool onSameRankOrFile = deltaX == 0 || deltaY == 0;
        if (onSameRankOrFile && (piece is Rook || piece is Queen))
        {
            Vector2Int direction = new Vector2Int(
                deltaX == 0 ? 0 : deltaX / Mathf.Abs(deltaX),
                deltaY == 0 ? 0 : deltaY / Mathf.Abs(deltaY)
            );

            Vector2Int checkPos = piecePos + direction;
            while (checkPos != targetPos)
            {
                if (checkPos != ignorePos && _pieces.ContainsKey(checkPos))
                    return false;
                checkPos += direction;
            }
            return true;
        }

        bool onSameDiagonal = Mathf.Abs(deltaX) == Mathf.Abs(deltaY);
        if (onSameDiagonal && (piece is Bishop || piece is Queen))
        {
            Vector2Int direction = new Vector2Int(
                deltaX / Mathf.Abs(deltaX),
                deltaY / Mathf.Abs(deltaY)
            );

            Vector2Int checkPos = piecePos + direction;
            while (checkPos != targetPos)
            {
                if (checkPos != ignorePos && _pieces.ContainsKey(checkPos))
                    return false;
                checkPos += direction;
            }
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