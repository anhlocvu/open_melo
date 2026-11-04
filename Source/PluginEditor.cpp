#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "NvdaBridge.h"

OpenMeloAudioProcessorEditor::OpenMeloAudioProcessorEditor(OpenMeloAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(740, 500);
    setWantsKeyboardFocus(true);

    audioProcessor.getNoteManager().onNotesChanged = [this]()
    {
        repaint();
    };

    NvdaBridge::getInstance().speak("Open Melo 0.2 ready. Melodyne-class TD-PSOLA engine active.", false);
}

OpenMeloAudioProcessorEditor::~OpenMeloAudioProcessorEditor()
{
    audioProcessor.getNoteManager().onNotesChanged = nullptr;
}

void OpenMeloAudioProcessorEditor::focusGained(FocusChangeType /*cause*/)
{
    const auto* note = audioProcessor.getNoteManager().getSelectedNote();
    if (note != nullptr)
    {
        NvdaBridge::getInstance().speak("Open Melo focused. " + juce::String(audioProcessor.getNoteManager().getNumNotes()) +
                                        " notes on track. Selected: " + note->getSpeechDescription(), false);
    }
    else
    {
        NvdaBridge::getInstance().speak("Open Melo focused. No notes. Play track in DAW or press Ctrl+O to ingest vocal.", false);
    }
}

bool OpenMeloAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        if (f.endsWithIgnoreCase(".wav") || f.endsWithIgnoreCase(".mp3") ||
            f.endsWithIgnoreCase(".aif") || f.endsWithIgnoreCase(".aiff") ||
            f.endsWithIgnoreCase(".flac") || f.endsWithIgnoreCase(".ogg"))
        {
            return true;
        }
    }
    return false;
}

void OpenMeloAudioProcessorEditor::filesDropped(const juce::StringArray& files, int /*x*/, int /*y*/)
{
    if (files.size() > 0)
    {
        juce::File audioFile(files[0]);
        NvdaBridge::getInstance().speak("Ingesting audio file " + audioFile.getFileName() + "...", true);
        audioProcessor.getNoteManager().scanAudioFile(audioFile);
    }
}

void OpenMeloAudioProcessorEditor::openAudioFileDialog()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Select Vocal Audio File to Ingest",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        "*.wav;*.mp3;*.aif;*.aiff;*.flac;*.ogg");

    auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc)
    {
        auto result = fc.getResult();
        if (result.existsAsFile())
        {
            NvdaBridge::getInstance().speak("Ingesting audio file " + result.getFileName() + "...", true);
            audioProcessor.getNoteManager().scanAudioFile(result);
        }
    });
}

bool OpenMeloAudioProcessorEditor::keyPressed(const juce::KeyPress& key)
{
    auto& nm = audioProcessor.getNoteManager();

    // Ctrl + O: Open audio file dialog to ingest instantly
    if (key.getModifiers().isCtrlDown() && (key.getKeyCode() == 'O' || key.getKeyCode() == 'o'))
    {
        openAudioFileDialog();
        return true;
    }

    // Ctrl + R: Clear all notes on track to re-scan
    if (key.getModifiers().isCtrlDown() && (key.getKeyCode() == 'R' || key.getKeyCode() == 'r'))
    {
        nm.clearNotes();
        NvdaBridge::getInstance().speak("Track notes cleared. Ready to ingest.", true);
        repaint();
        return true;
    }

    // Left / Right: Navigate notes
    if (key == juce::KeyPress::leftKey)
    {
        nm.navigateLeft();
        return true;
    }
    if (key == juce::KeyPress::rightKey)
    {
        nm.navigateRight();
        return true;
    }

    // Up / Down:
    // Up / Down: adjust cents (+/- 1 ct)
    // Ctrl + Up / Down: adjust semitones (+/- 1 st)
    if (key.isKeyCode(juce::KeyPress::upKey))
    {
        if (key.getModifiers().isCtrlDown())
            nm.transposeCurrentNote(1);
        else
            nm.tuneCentsCurrentNote(1);
        return true;
    }
    if (key.isKeyCode(juce::KeyPress::downKey))
    {
        if (key.getModifiers().isCtrlDown())
            nm.transposeCurrentNote(-1);
        else
            nm.tuneCentsCurrentNote(-1);
        return true;
    }

    // P / Shift + P: Audition playback & snap to zero
    if (key.getKeyCode() == 'P' || key.getKeyCode() == 'p')
    {
        if (key.getModifiers().isShiftDown())
            nm.snapCurrentNoteToZero();
        else
            nm.playCurrentNote();
        return true;
    }

    // S / Shift + S: Split / Slice note
    if (key.getKeyCode() == 'S' || key.getKeyCode() == 's')
    {
        if (key.getModifiers().isShiftDown())
            nm.sliceCurrentNoteIntoPieces(60);
        else
            nm.splitCurrentNote();
        return true;
    }

    // Do NOT intercept Space key so DAW transport controls Play/Stop uninterruptedly
    return false;
}

void OpenMeloAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(juce::Colour(0xff181a20));

    // Top Header Banner
    g.setColour(juce::Colour(0xff222630));
    g.fillRect(0, 0, getWidth(), 64);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText("OPEN MELO v0.2", 20, 10, 300, 24, juce::Justification::left);

    g.setFont(juce::FontOptions(13.0f, juce::Font::plain));
    g.setColour(juce::Colour(0xffa0a5b5));
    g.drawText("Tech VST - Melodyne-Class TD-PSOLA Vocal & Pitch Editor", 20, 35, 450, 18, juce::Justification::left);

    // NVDA Status Indicator
    bool nvdaActive = NvdaBridge::getInstance().isNvdaRunning();
    g.setColour(nvdaActive ? juce::Colour(0xff2ecc71) : juce::Colour(0xfff39c12));
    g.fillEllipse((float)(getWidth() - 170), 24.0f, 12.0f, 12.0f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText(nvdaActive ? "NVDA ACTIVE" : "NVDA STANDBY", getWidth() - 150, 20, 130, 20, juce::Justification::left);

    // Main Current Note Display Box
    auto noteBox = juce::Rectangle<int>(20, 80, getWidth() - 40, 115);
    g.setColour(juce::Colour(0xff282c37));
    g.fillRoundedRectangle(noteBox.toFloat(), 8.0f);
    g.setColour(juce::Colour(0xff3f4556));
    g.drawRoundedRectangle(noteBox.toFloat(), 8.0f, 1.5f);

    const auto* curNote = audioProcessor.getNoteManager().getSelectedNote();
    if (curNote != nullptr)
    {
        g.setColour(juce::Colour(0xff4aa3df));
        g.setFont(juce::FontOptions(44.0f, juce::Font::bold));
        g.drawText(curNote->getNoteName(), noteBox.getX() + 25, noteBox.getY() + 15, 140, 50, juce::Justification::left);

        // Cents Pill
        int cents = curNote->getCurrentTotalCents();
        juce::Colour centsCol = (cents == 0) ? juce::Colour(0xff2ecc71) : (cents > 0 ? juce::Colour(0xffe67e22) : juce::Colour(0xffe74c3c));

        g.setColour(centsCol);
        g.setFont(juce::FontOptions(28.0f, juce::Font::bold));
        g.drawText(curNote->getCentsString(), noteBox.getX() + 175, noteBox.getY() + 24, 180, 40, juce::Justification::left);

        // TD-PSOLA Formant Tag
        g.setColour(juce::Colour(0xff27ae60));
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawText("TD-PSOLA FORMANTS", noteBox.getX() + noteBox.getWidth() - 210, noteBox.getY() + 20, 190, 20, juce::Justification::right);

        // DAW Playback Tag
        g.setColour(juce::Colour(0xff2980b9));
        g.drawText("SAMPLE-ACCURATE SYNC", noteBox.getX() + noteBox.getWidth() - 210, noteBox.getY() + 42, 190, 20, juce::Justification::right);

        g.setFont(juce::FontOptions(14.0f, juce::Font::plain));
        g.setColour(juce::Colour(0xffbdc3c7));
        juce::String details = "Note: " + juce::String(audioProcessor.getNoteManager().getSelectedNoteIndex() + 1) +
                               " of " + juce::String(audioProcessor.getNoteManager().getNumNotes()) +
                               "  |  Midi: " + juce::String(curNote->getNominalMidiNote()) +
                               "  |  Duration: " + juce::String((double)curNote->lengthSamples / 44100.0, 2) + "s";
        g.drawText(details, noteBox.getX() + 25, noteBox.getY() + 75, noteBox.getWidth() - 50, 22, juce::Justification::left);
    }
    else
    {
        g.setColour(juce::Colour(0xffbdc3c7));
        g.setFont(juce::FontOptions(20.0f, juce::Font::bold));
        g.drawText("NO NOTES ON TRACK", noteBox.getX() + 25, noteBox.getY() + 25, noteBox.getWidth() - 50, 30, juce::Justification::left);

        g.setFont(juce::FontOptions(13.0f, juce::Font::plain));
        g.setColour(juce::Colour(0xff95a5a6));
        g.drawText("Option 1: Play vocal track in DAW (continuously ingests entire track as it plays)\n"
                   "Option 2: Drag & drop audio file here or press Ctrl+O to ingest entire track in 0.2s.",
                   noteBox.getX() + 25, noteBox.getY() + 60, noteBox.getWidth() - 50, 42, juce::Justification::left);
    }

    // Horizontal Notes Visualization Strip
    auto stripBox = juce::Rectangle<int>(20, 210, getWidth() - 40, 80);
    g.setColour(juce::Colour(0xff20242d));
    g.fillRoundedRectangle(stripBox.toFloat(), 6.0f);

    const auto& allNotes = audioProcessor.getNoteManager().getAllNotes();
    if (!allNotes.empty())
    {
        int64_t totalLength = 0;
        for (const auto& n : allNotes) totalLength += n.lengthSamples;

        float currentX = (float)stripBox.getX() + 4.0f;
        float availableW = (float)stripBox.getWidth() - 8.0f;

        for (int i = 0; i < (int)allNotes.size(); ++i)
        {
            const auto& n = allNotes[i];
            float noteW = ((float)n.lengthSamples / (float)totalLength) * availableW;
            if (noteW < 12.0f) noteW = 12.0f;

            auto blockRect = juce::Rectangle<float>(currentX, (float)stripBox.getY() + 6.0f, noteW - 2.0f, (float)stripBox.getHeight() - 12.0f);

            bool isSelected = (i == audioProcessor.getNoteManager().getSelectedNoteIndex());
            g.setColour(isSelected ? juce::Colour(0xff3498db) : juce::Colour(0xff454d5e));
            g.fillRoundedRectangle(blockRect, 4.0f);

            if (isSelected)
            {
                g.setColour(juce::Colours::white);
                g.drawRoundedRectangle(blockRect, 4.0f, 2.0f);
            }

            if (noteW >= 24.0f)
            {
                g.setColour(juce::Colours::white);
                g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
                g.drawText(n.getNoteName(), blockRect.toNearestInt(), juce::Justification::centred);
            }

            currentX += noteW;
        }
    }
    else
    {
        g.setColour(juce::Colour(0xff555d6e));
        g.setFont(juce::FontOptions(13.0f, juce::Font::italic));
        g.drawText("Track is empty. Play vocal track in DAW or press Ctrl+O to ingest...", stripBox, juce::Justification::centred);
    }

    // Keyboard Shortcuts Guide Box
    auto guideBox = juce::Rectangle<int>(20, 305, getWidth() - 40, 175);
    g.setColour(juce::Colour(0xff1f2229));
    g.fillRoundedRectangle(guideBox.toFloat(), 6.0f);

    g.setColour(juce::Colour(0xffe0e4ee));
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText("ACCESSIBILITY KEYBOARD GUIDE (NVDA READY)", guideBox.getX() + 15, guideBox.getY() + 10, 400, 20, juce::Justification::left);

    g.setFont(juce::FontOptions(13.0f, juce::Font::plain));
    g.setColour(juce::Colour(0xffa5acbc));

    juce::String col1 = "• Left / Right: Navigate notes & audition\n"
                        "• Up / Down: Fine-tune CENTS (+/- 1 cent)\n"
                        "• Ctrl + Up / Down: Transpose PITCH (+/- 1 semitone)\n"
                        "• Ctrl + O: Open audio file to ingest instantly\n"
                        "• Ctrl + R: Reset / Clear all notes to re-ingest";

    juce::String col2 = "• P: Play / Audition current note\n"
                        "• Shift + P: Snap cents to 0 (Tuned)\n"
                        "• S: Split note in half (50% cut)\n"
                        "• Shift + S: Slice note into micro segments\n"
                        "• Space: Trả lại cho DAW điều khiển Play/Stop";

    g.drawMultiLineText(col1, guideBox.getX() + 15, guideBox.getY() + 45, 340);
    g.drawMultiLineText(col2, guideBox.getX() + 370, guideBox.getY() + 45, 340);
}

void OpenMeloAudioProcessorEditor::resized()
{
}
