#ifndef _NVDA_CONTROLLER_H_
#define _NVDA_CONTROLLER_H_

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef error_status_t res_t;

__declspec(dllimport) error_status_t __stdcall nvdaController_testIfRunning(void);
__declspec(dllimport) error_status_t __stdcall nvdaController_speakText(const wchar_t* text);
__declspec(dllimport) error_status_t __stdcall nvdaController_cancelSpeech(void);
__declspec(dllimport) error_status_t __stdcall nvdaController_brailleMessage(const wchar_t* text);

#ifdef __cplusplus
}
#endif

#endif // _NVDA_CONTROLLER_H_
