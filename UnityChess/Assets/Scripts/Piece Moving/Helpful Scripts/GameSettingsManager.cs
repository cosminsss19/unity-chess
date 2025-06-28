using UnityEngine;
using UnityEngine.SceneManagement;

namespace Piece_Moving.Helpful_Scripts
{
    public class GameSettingsManager : MonoBehaviour
    {
        public static GameSettingsManager Instance;
    
        public bool PlayAgainstComputer { get; private set; }
        public bool ComputerPlaysWhite { get; private set; }

        public ChessPersonality ComputerPersonality { get; private set; } = ChessPersonality.STANDARD;
        private void Awake()
        {
            if (Instance == null)
            {
                Instance = this;
                DontDestroyOnLoad(gameObject);
            }
            else
            {
                Destroy(gameObject);
            }
        }
    
        public void SetGameMode(bool vsComputer)
        {
            PlayAgainstComputer = vsComputer;
        }
    
        public void SetComputerSide(bool computerPlaysWhite)
        {
            ComputerPlaysWhite = computerPlaysWhite;
        }
        
        public void SetComputerPersonality(ChessPersonality personality)
        {
            ComputerPersonality = personality;
        }
    
        public void StartGame()
        {
            SceneManager.LoadScene("TwoPlayers");
        }
    }
}