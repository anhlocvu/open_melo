# Implementation Plan: Khắc Phục Mất Tiếng Track Vocal, Méo Tiếng Robot/Alien & Chuẩn Hóa Timeline DAW

**Branch**: `001-fix-ara-timeline-audio` | **Date**: 2026-11-10 | **Spec**: [spec.md](file:///C:/Users/LCBoy/Desktop/data/vst-projects/open_melo/specs/001-fix-ara-timeline-audio/spec.md)

**Input**: Feature specification from `/specs/001-fix-ara-timeline-audio/spec.md`

## Summary

Khắc phục triệt để 4 lỗi nghiêm trọng trong Open Melo v0.2 sau khi phân tích mã nguồn và dịch ngược `MelodyneCore-5.4.1.004.dll`:
1. **Lỗi câm track vocal khi chèn plugin**: Do `OpenMeloEditorRenderer::processBlock` gọi `buffer.clear()` ngay sau khi `OpenMeloPlaybackRenderer::processBlock` vừa render tiếng vocal vào cùng `buffer`, đồng thời `scanAudioSourceForRegion` chạy khi `isSampleAccessEnabled()` còn bằng `false` nhưng lại đánh dấu nguồn đã quét (`scannedSources.insert`).
2. **Lỗi duyệt nốt / kéo nốt bị rè và nghe như robot / người ngoài hành tinh**:
   - Do `AuditionVoice::renderNextBlock` bị gọi **3 lần liên tiếp trong cùng 1 audio block** (`PlaybackRenderer::processBlock` + `EditorRenderer::processBlock` + `PluginProcessor::processBlock`), khiến phần đầu bị xóa, 2 phần sau bị cộng đè lên nhau và tốc độ đọc buffer bị tua nhanh gấp 3 lần (3x granular chopping).
   - Do bộ tách nốt (`scanAudioBuffer` / `processIncomingTrackAudio`) bỏ qua không chép các khung âm thanh khi `silenceCount < 4`, làm rỗng/đứt gãy sóng ở giữa nốt.
   - Do cấu hình `SignalsmithStretch::presetDefault` dùng cửa sổ STFT quá lớn (120ms = ~5292 mẫu, dành cho nhạc đa âm) gây nhòe pha (phase smearing) trên các nốt vocal ngắn.
3. **Lỗi trễ Timeline mấy bar và tụt cao độ nửa cung**:
   - Do lệch tần số mẫu giữa file audio gốc (`audioSource->getSampleRate()`, VD: 44.1 kHz) và DAW (`sampleRate`, VD: 48 kHz) mà không nội suy/resample khi đọc ARA source và khi tính tọa độ mẫu. ($12 \log_2(44100/48000) = -1.46$ bán âm $\approx$ tụt hơn nửa cung và trôi thời gian!).
   - Do công thức tính `startTimelineSample` ép `if (startTimelineSample < 0) startTimelineSample = 0;` khi clip đã được trim đầu (`startInAudioModificationTime > startInPlaybackTime`), làm toàn bộ các nốt bị đẩy lùi về sau nhiều bar!
   - Trong chế độ Non-ARA (Realtime Fallback), `processIncomingTrackAudio` lưu mẫu theo vị trí cửa sổ trễ và cắt nốt rời rạc gây lệch vị trí so với timeline thực.

## Technical Context

**Language/Version**: C++20

**Primary Dependencies**: JUCE 8.0.4, Celemony ARA SDK 2.2.0 (`libs/ARA_SDK`), NVDA Controller Client (`libs/nvda`), Signalsmith Stretch (`libs/signalsmith`) + Bộ dịch cao độ Vocal chuyên dụng (Clean-room Vocal Pitch & Formant Engine / WORLD-style & WSOLA/PSOLA-grade vocal tuning).

**Storage**: Binary state serialization qua JUCE `MemoryOutputStream` / `ARAOutputStream`.

**Testing**: `pluginval` (Tracktion) Level 5 + Kiểm thử DSP/Timeline tự động.

**Target Platform**: Windows 64-bit VST3 (`Open Melo.vst3`).

**Project Type**: Audio Plugin (VST3 + ARA2) với trợ năng NVDA.

**Performance Goals**: Zero-latency timeline playback (0 sample drift), lock-free audio thread, < 5% CPU usage.

## Project Structure

### Documentation (this feature)

```text
specs/001-fix-ara-timeline-audio/
├── spec.md
├── plan.md
└── checklists/
    └── requirements.md
```

### Source Code (repository root)

```text
Source/
├── AuditionVoice.h          # Bộ phát nghe thử nốt nhạc (lock-free, chống rút mẫu trùng lặp trong cùng 1 block)
├── NoteManager.h            # Quản lý danh sách nốt, ánh xạ timeline chuẩn xác & chỉnh phô
├── NoteManager.cpp
├── NoteModel.h              # Cấu trúc dữ liệu nốt nhạc
├── NvdaBridge.h             # Giao tiếp trực tiếp với NVDA Controller Client DLL
├── OpenMeloARA.h            # Kiến trúc ARA2 (DocumentController, PlaybackRenderer, EditorRenderer, EditorView)
├── OpenMeloARA.cpp
├── PitchDetector.h          # Thuật toán dò cao độ McLeod Pitch Method (MPM)
├── PitchShiftEngine.h       # Lõi dịch cao độ & bảo toàn Formant tối ưu cho giọng hát
├── PitchShiftEngine.cpp
├── PluginEditor.h           # Giao diện & điều khiển phím tắt trợ năng
├── PluginEditor.cpp
├── PluginProcessor.h        # Xử lý luồng âm thanh chính VST3 & ARA
└── PluginProcessor.cpp
```
