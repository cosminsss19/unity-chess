using UnityEngine;
using UnityEngine.UI;

namespace Piece_Moving.Helpful_Scripts
{
    public class MenuController : MonoBehaviour
{
    [Header("Buttons")]
    [SerializeField] private Button playerVsComputerButton;
    [SerializeField] private Button playerVsPlayerButton;
    [SerializeField] private Button playButton;
    
    [Header("Difficulty Selection")]
    [SerializeField] private GameObject difficultyPanel;
    [SerializeField] private Toggle playerAsWhiteToggle;
    
    [SerializeField] private Button easyButton;
    [SerializeField] private Button mediumButton;
    [SerializeField] private Button hardButton;
    private int selectedDifficulty = 3;
    private bool vsComputer = true;

    private void Start()
    {
        easyButton.onClick.AddListener(() => SetDifficulty(2));
        mediumButton.onClick.AddListener(() => SetDifficulty(3));
        hardButton.onClick.AddListener(() => SetDifficulty(5));
    
        // Highlight medium as default
        UpdateDifficultyButtons();
        
        // Ensure GameSettingsManager exists
        if (GameSettingsManager.Instance == null)
        {
            GameObject settingsManager = new GameObject("GameSettingsManager");
            settingsManager.AddComponent<GameSettingsManager>();
        }
        
        // Set up button listeners
        playerVsComputerButton.onClick.AddListener(() => SelectGameMode(true));
        playerVsPlayerButton.onClick.AddListener(() => SelectGameMode(false));
        playButton.onClick.AddListener(StartGame);
        
        // Initial state
        difficultyPanel.SetActive(true);
        playerVsComputerButton.GetComponent<Image>().color = Color.green;
        playerVsPlayerButton.GetComponent<Image>().color = Color.white;
    }
    
    private void SetDifficulty(int depth)
    {
        selectedDifficulty = depth;
        UpdateDifficultyButtons();
    }

    private void UpdateDifficultyButtons()
    {
        Color selectedColor = new Color(0.7f, 1f, 0.7f);
        Color normalColor = Color.white;
    
        easyButton.GetComponent<Image>().color = selectedDifficulty == 2 ? selectedColor : normalColor;
        mediumButton.GetComponent<Image>().color = selectedDifficulty == 3 ? selectedColor : normalColor;
        hardButton.GetComponent<Image>().color = selectedDifficulty == 5 ? selectedColor : normalColor;
    }
    
    private void SelectGameMode(bool vsComp)
    {
        vsComputer = vsComp;
        difficultyPanel.SetActive(vsComp);
        
        // Visual feedback
        playerVsComputerButton.GetComponent<Image>().color = vsComp ? Color.green : Color.white;
        playerVsPlayerButton.GetComponent<Image>().color = vsComp ? Color.white : Color.green;
    }
    
    private void StartGame()
    {
        GameSettingsManager.Instance.SetGameMode(vsComputer);
        
        if (vsComputer)
        {
            // Set who plays as white
            GameSettingsManager.Instance.SetComputerSide(!playerAsWhiteToggle.isOn);
            
            // You can add difficulty selection here
            GameSettingsManager.Instance.SetDifficulty(selectedDifficulty);
        }
        
        GameSettingsManager.Instance.StartGame();
    }
}
}