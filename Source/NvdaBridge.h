#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <juce_core/juce_core.h>
#include <windows.h>

class NvdaBridge
{
public:
    static NvdaBridge& getInstance()
    {
        static NvdaBridge instance;
        return instance;
    }

    bool isNvdaRunning()
    {
        if (testIfRunningFn != nullptr)
        {
            return testIfRunningFn() == 0;
        }
        return false;
    }

    void cancelSpeech()
    {
        if (cancelSpeechFn != nullptr)
        {
            cancelSpeechFn();
        }
    }

    void speak(const juce::String& text, bool cancelCurrent = true)
    {
        if (cancelCurrent)
        {
            cancelSpeech();
        }

        if (speakTextFn != nullptr)
        {
            speakTextFn(text.toWideCharPointer());
        }

        if (brailleMessageFn != nullptr)
        {
            brailleMessageFn(text.toWideCharPointer());
        }
    }

    void braille(const juce::String& text)
    {
        if (brailleMessageFn != nullptr)
        {
            brailleMessageFn(text.toWideCharPointer());
        }
    }

    bool isLoaded() const
    {
        return dllHandle != nullptr && speakTextFn != nullptr;
    }

private:
    typedef unsigned long (__stdcall *TestIfRunningFn)(void);
    typedef unsigned long (__stdcall *SpeakTextFn)(const wchar_t*);
    typedef unsigned long (__stdcall *CancelSpeechFn)(void);
    typedef unsigned long (__stdcall *BrailleMessageFn)(const wchar_t*);

    HMODULE dllHandle = nullptr;
    TestIfRunningFn testIfRunningFn = nullptr;
    SpeakTextFn speakTextFn = nullptr;
    CancelSpeechFn cancelSpeechFn = nullptr;
    BrailleMessageFn brailleMessageFn = nullptr;

    NvdaBridge()
    {
        loadDll();
    }

    ~NvdaBridge()
    {
        if (dllHandle != nullptr)
        {
            FreeLibrary(dllHandle);
            dllHandle = nullptr;
        }
    }

    void loadDll()
    {
        juce::File currentExe = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
        juce::File currentDir = currentExe.getParentDirectory();

        juce::String dllName = "nvdaControllerClient64.dll";
        juce::File dllPath = currentDir.getChildFile(dllName);

        if (!dllPath.existsAsFile())
        {
            dllPath = juce::File::getSpecialLocation(juce::File::currentApplicationFile).getChildFile(dllName);
        }

        if (dllPath.existsAsFile())
        {
            dllHandle = LoadLibraryW(dllPath.getFullPathName().toWideCharPointer());
        }
        else
        {
            dllHandle = LoadLibraryW(L"nvdaControllerClient64.dll");
        }

        if (dllHandle != nullptr)
        {
            testIfRunningFn = (TestIfRunningFn)GetProcAddress(dllHandle, "nvdaController_testIfRunning");
            speakTextFn = (SpeakTextFn)GetProcAddress(dllHandle, "nvdaController_speakText");
            cancelSpeechFn = (CancelSpeechFn)GetProcAddress(dllHandle, "nvdaController_cancelSpeech");
            brailleMessageFn = (BrailleMessageFn)GetProcAddress(dllHandle, "nvdaController_brailleMessage");
        }
    }

    JUCE_DECLARE_NON_COPYABLE(NvdaBridge)
};
