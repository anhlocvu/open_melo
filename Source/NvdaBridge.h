#pragma once

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
    typedef error_status_t(__stdcall* TestIfRunningFn)(void);
    typedef error_status_t(__stdcall* SpeakTextFn)(const wchar_t*);
    typedef error_status_t(__stdcall* CancelSpeechFn)(void);
    typedef error_status_t(__stdcall* BrailleMessageFn)(const wchar_t*);

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
        // 1. Try directory of current DLL / VST3 module
        HMODULE hModule = nullptr;
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCWSTR)&NvdaBridge::getInstance, &hModule))
        {
            wchar_t path[MAX_PATH];
            if (GetModuleFileNameW(hModule, path, MAX_PATH))
            {
                juce::String pathStr(path);
                juce::File currentDll = juce::File::createFileWithoutCheckingPath(pathStr);
                auto candidate = currentDll.getParentDirectory().getChildFile("nvdaControllerClient64.dll");
                if (candidate.existsAsFile())
                {
                    dllHandle = LoadLibraryW(candidate.getFullPathName().toWideCharPointer());
                }
            }
        }

        // 2. Try application directory / working directory
        if (dllHandle == nullptr)
        {
            dllHandle = LoadLibraryW(L"nvdaControllerClient64.dll");
        }

        // 3. Try standard NVDA installation directory
        if (dllHandle == nullptr)
        {
            dllHandle = LoadLibraryW(L"C:\\Program Files\\NVDA\\nvdaControllerClient64.dll");
        }

        // 4. Resolve function pointers
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
