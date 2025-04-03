using DefaultNamespace;
using Piece_Moving.Helpful_Scripts;
using UnityEngine;

public class ChessPieceSpawner : MonoBehaviour
{
    [Header("Prefabs for Chess Pieces")]
    public GameObject whitePawn;
    public GameObject blackPawn;
    public GameObject whiteRook;
    public GameObject blackRook;
    public GameObject whiteKnight;
    public GameObject blackKnight;
    public GameObject whiteBishop;
    public GameObject blackBishop;
    public GameObject whiteQueen;
    public GameObject blackQueen;
    public GameObject whiteKing;
    public GameObject blackKing;

    [Header("Chessboard Parent Object")]
    public GameObject board;

    private Transform[,] _boardSquares = new Transform[8, 8];

    private void Start()
    {
        InitializeBoardSquares();
        SpawnPieces();
    }

    private void InitializeBoardSquares()
    {
        Transform[] squares = new Transform[board.transform.childCount];

        for (int i = 0; i < board.transform.childCount; i++)
        {
            squares[i] = board.transform.GetChild(i);
        }

        int index = 0;
        for (int row = 0; row < 8; row++)
        {
            for (int col = 0; col < 8; col++)
            {
                _boardSquares[row, col] = squares[index];
                index++;
            }
        }
    }

    private void SpawnPieces()
    {
        ChessGameManager gameManager = GetComponent<ChessGameManager>();

        for (int col = 0; col < 8; col++)
        {
            InstantiateAndInitialize(whitePawn, 1, col, true, gameManager);
            InstantiateAndInitialize(blackPawn, 6, col, false, gameManager);
        }

        InstantiateAndInitialize(whiteRook, 0, 0, true, gameManager);
        InstantiateAndInitialize(whiteRook, 0, 7, true, gameManager);

        InstantiateAndInitialize(blackRook, 7, 0, false, gameManager);
        InstantiateAndInitialize(blackRook, 7, 7, false, gameManager);

        InstantiateAndInitialize(whiteKnight, 0, 1, true, gameManager);
        InstantiateAndInitialize(whiteKnight, 0, 6, true, gameManager);

        InstantiateAndInitialize(blackKnight, 7, 1, false, gameManager);
        InstantiateAndInitialize(blackKnight, 7, 6, false, gameManager);

        InstantiateAndInitialize(whiteBishop, 0, 2, true, gameManager);
        InstantiateAndInitialize(whiteBishop, 0, 5, true, gameManager);

        InstantiateAndInitialize(blackBishop, 7, 2, false, gameManager);
        InstantiateAndInitialize(blackBishop, 7, 5, false, gameManager);

        InstantiateAndInitialize(whiteQueen, 0, 3, true, gameManager);
        InstantiateAndInitialize(whiteKing, 0, 4, true, gameManager);

        InstantiateAndInitialize(blackQueen, 7, 3, false, gameManager);
        InstantiateAndInitialize(blackKing, 7, 4, false, gameManager);
    }

    private void InstantiateAndInitialize(GameObject prefab, int row, int col, bool isWhite, ChessGameManager gameManager)
    {
        GameObject piece = Instantiate(prefab, GetPiecePosition(row, col), Quaternion.identity, _boardSquares[row, col]);
        ChessPiece chessPiece = piece.GetComponent<ChessPiece>();
        if (chessPiece != null)
        {
            chessPiece.Initialize(isWhite, gameManager);
        }
    }


    private Vector3 GetPiecePosition(int row, int col)
    {
        Vector3 position = _boardSquares[row, col].position;
        position += Constants.offset;
        return position;
    }
}
