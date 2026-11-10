# Tasks: Fix ARA Muted Track, Timeline Drift, Sample Rate Pitch Drop & Robotic Audio

**Input**: Design documents from `/specs/001-fix-ara-timeline-audio/`
**Prerequisites**: `plan.md` (required), `spec.md` (required)

## Phase 1: AuditionVoice & Processor ARA Routing (Step 1)
- [x] T001 [US2] Add 64-sample fade-in/fade-out anti-click envelope and single-block consumption safety to `Source/AuditionVoice.h`
- [x] T002 [US1] [US2] Fix `OpenMeloAudioProcessor::didBindToARA`, `getNoteManager`, `getAuditionVoice`, and remove duplicate `renderNextBlock` call in `Source/PluginProcessor.cpp`

## Phase 2: ARA Lifecycle, Sample Rate Resampling & Region Timeline Sync (Step 2)
- [x] T003 [US1] [US3] Update `OpenMeloDocumentController`, `OpenMeloPlaybackRenderer`, and `OpenMeloEditorRenderer` declarations in `Source/OpenMeloARA.h` to support deferred sample access, DAW sample rate tracking, and region-accurate scanning
- [x] T004 [US1] [US3] Implement `didEnableAudioSourceSamplesAccess`, `didUpdatePlaybackRegionProperties`, `didEndEditing`, Lagrange sample-rate conversion, exact `PlaybackRegion` timeline mapping, and non-destructive `EditorRenderer` in `Source/OpenMeloARA.cpp`

## Phase 3: Contiguous Note Extraction & True Crossfade Playback (Step 3)
- [x] T005 [US2] [US3] Update `NoteManager::scanAudioBuffer` declaration to support multi-region `appendMode` and `restoreNoteTuning` in `Source/NoteManager.h`
- [x] T006 [US2] [US3] Fix contiguous audio slice extraction in `NoteManager::scanAudioBuffer` & `processIncomingTrackAudio`, and implement true linear crossfade in `NoteManager::renderTrackPlayback` in `Source/NoteManager.cpp`

## Phase 4: Vocal-Optimized Signalsmith Stretch Engine & MPM Key Maxima (Step 4)
- [x] T007 [US2] Replace 120ms default preset with 46ms/11.5ms vocal STFT config, raised-cosine boundary padding, micro-fade in `Source/PitchShiftEngine.cpp`, and MPM Key Maxima selection in `Source/PitchDetector.h`

## Phase 5: Verification, Release Build, VST3 Deployment & Logging (Step 5)
- [x] T008 [US1] [US2] [US3] Compile Release x64 VST3 target via CMake & MSVC
- [x] T009 [US1] [US2] [US3] Run DSP/Timeline unit verification suite, deploy to system VST3 folder, and update `GEMINI.md`
