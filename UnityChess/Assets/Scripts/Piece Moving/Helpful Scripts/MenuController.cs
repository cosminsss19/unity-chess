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
    
    [Header("Personality Selection")]
    [SerializeField] private GameObject personalityPanel;
    [SerializeField] private Toggle playerAsWhiteToggle;
    
    [SerializeField] private Button standardButton;
    [SerializeField] private Button aggressiveButton;
    [SerializeField] private Button positionalButton;
    [SerializeField] private Button solidButton;
    [SerializeField] private Button dynamicButton;
    private ChessPersonality selectedPersonality = ChessPersonality.STANDARD;
    private bool vsComputer = true;

    private void Start()
    {
        standardButton.onClick.AddListener(() => SetPersonality(ChessPersonality.STANDARD));
        aggressiveButton.onClick.AddListener(() => SetPersonality(ChessPersonality.AGGRESSIVE));
        positionalButton.onClick.AddListener(() => SetPersonality(ChessPersonality.POSITIONAL));
        solidButton.onClick.AddListener(() => SetPersonality(ChessPersonality.SOLID));
        dynamicButton.onClick.AddListener(() => SetPersonality(ChessPersonality.DYNAMIC));
    
        UpdatePersonalityButtons();
        
        if (GameSettingsManager.Instance == null)
        {
            GameObject settingsManager = new GameObject("GameSettingsManager");
            settingsManager.AddComponent<GameSettingsManager>();
        }
        
        playerVsComputerButton.onClick.AddListener(() => SelectGameMode(true));
        playerVsPlayerButton.onClick.AddListener(() => SelectGameMode(false));
        playButton.onClick.AddListener(StartGame);
        
        personalityPanel.SetActive(true);
        playerVsComputerButton.GetComponent<Image>().color = Color.green;
        playerVsPlayerButton.GetComponent<Image>().color = Color.white;
    }
    
    private void SetPersonality(ChessPersonality personality)
    {
        selectedPersonality = personality;
        UpdatePersonalityButtons();
    }

    private void UpdatePersonalityButtons()
    {
        Color selectedColor = new Color(0.7f, 1f, 0.7f);
        Color normalColor = Color.white;
    
        standardButton.GetComponent<Image>().color = selectedPersonality == ChessPersonality.STANDARD ? selectedColor : normalColor;
        aggressiveButton.GetComponent<Image>().color = selectedPersonality == ChessPersonality.AGGRESSIVE ? selectedColor : normalColor;
        positionalButton.GetComponent<Image>().color = selectedPersonality == ChessPersonality.POSITIONAL ? selectedColor : normalColor;
        solidButton.GetComponent<Image>().color = selectedPersonality == ChessPersonality.SOLID ? selectedColor : normalColor;
        dynamicButton.GetComponent<Image>().color = selectedPersonality == ChessPersonality.DYNAMIC ? selectedColor : normalColor;
    }
    
    private void SelectGameMode(bool vsComp)
    {
        vsComputer = vsComp;
        personalityPanel.SetActive(vsComp);
        
        playerVsComputerButton.GetComponent<Image>().color = vsComp ? Color.green : Color.white;
        playerVsPlayerButton.GetComponent<Image>().color = vsComp ? Color.white : Color.green;
    }
    
    private void StartGame()
    {
        GameSettingsManager.Instance.SetGameMode(vsComputer);
        
        if (vsComputer)
        {
            GameSettingsManager.Instance.SetComputerSide(!playerAsWhiteToggle.isOn);
            
            GameSettingsManager.Instance.SetComputerPersonality(selectedPersonality);
        }
        
        GameSettingsManager.Instance.StartGame();
    }
}
}