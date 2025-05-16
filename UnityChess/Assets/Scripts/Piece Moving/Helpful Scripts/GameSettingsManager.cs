using UnityEngine;
using UnityEngine.SceneManagement;

namespace Piece_Moving.Helpful_Scripts
{
    public class GameSettingsManager : MonoBehaviour
    {
        // Singleton pattern to persist between scenes
        public static GameSettingsManager Instance;
    
        // Game settings
        public bool PlayAgainstComputer { get; private set; }
        public bool ComputerPlaysWhite { get; private set; }

        public ChessPersonality ComputerPersonality { get; private set; } = ChessPersonality.STANDARD; // Default personality
        private void Awake()
        {
            // Singleton implementation
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
            SceneManager.LoadScene("TwoPlayers"); // Load your main chess scene
        }
    }
}