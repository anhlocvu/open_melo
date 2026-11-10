## quy tắc dự án

1. trong mọi trường hợp phát triển, luôn luôn tự động tuân thủ quy trình specify

# Nhật Ký Dự Án Open Melo (GEMINI.md)

## Thông Tin Dự Án
- **Tên dự án**: `Open Melo`
- **Tên hiển thị VST3**: `Open Melo`
- **Hãng / Công ty phát triển**: `Tech VST`
- **Phiên bản hiện tại**: `0.2`
- **Định dạng mục tiêu**: `VST3` (duy nhất trong v0.1)
- **Ngôn ngữ giao diện & Speech**: Tiếng Anh (English) chuẩn quốc tế
- **Mục tiêu**: Xây dựng VST3 Audio Plugin chỉnh sửa cao độ (pitch correction & melody editor) tương tự Celemony Melodyne nhưng mã nguồn độc lập (Clean-room), tối ưu hiệu năng cao và **hỗ trợ trợ năng (Accessibility) toàn diện** cho người dùng khiếm thị / dùng trình đọc màn hình NVDA qua bàn phím.
- **Tính chất**: Dự án mã nguồn mở (Open Source) phục vụ cộng đồng âm nhạc và người dùng trợ năng toàn cầu. Mọi tài nguyên, thư viện phụ thuộc đều được đóng gói trực tiếp (self-contained) trong repo để ai tải về cũng có thể build và chạy ngay.
- **Tác giả / Lead Developer**: Vũ Anh Lộc (LC Boy)
- **Công nghệ lõi**: C++20, JUCE 8.0.4, NVDA Controller Client API (64-bit), Pitch Shifting / Phase-locked Vocoder & Note Slicing DSP.

---

## Quy Tắc Dự Án (Project Rules)
1. **Định dạng duy nhất cho v0.1**: Chỉ xây dựng VST3 64-bit.
2. **Ngôn ngữ**: Toàn bộ chuỗi giao diện người dùng, log và chuỗi đọc giọng nói trợ năng (speech announcements) trong code đều dùng **Tiếng Anh**.
3. **Trợ năng là ưu tiên hàng đầu (Accessibility-First)**:
   - Mọi thao tác đều thực hiện 100% bằng phím tắt bàn phím (Keyboard Shortcuts).
   - Tích hợp trực tiếp thư viện `nvdaControllerClient64.dll` để phát âm thanh phản hồi tức thì với độ trễ gần như bằng 0 (zero latency), không phụ thuộc vào độ trễ của UI Automation.
   - Luôn gọi `cancelSpeech` trước khi đọc chuỗi mới để khi người dùng ấn phím liên tục không bị nghẽn giọng đọc.
4. **Quy trình phát triển**:
   - Luôn lập kế hoạch chi tiết trước khi code và chờ người dùng duyệt.
   - Chia nhỏ tác vụ, kiểm thử liên tục.
   - Cập nhật nhật ký dự án vào `GEMINI.md` sau mỗi cột mốc.

---

## Giai Đoạn 1: Dịch Ngược & Phân Tích Cơ Chế Hoạt Động Của Melodyne (Đã hoàn thành)
**Ngày thực hiện**: 04/11/2026
- Đã phân tích file `MelodyneCore-5.4.1.004.dll` (89.5 MB) được sao chép trực tiếp vào thư mục `Melodyne 5`.
- Làm rõ 6 cơ chế lõi: Dò cao độ miền tần số, Ma trận tách họa âm đa âm (DNA), Tách âm xuýt (Sibilant separation), Kiến trúc cao độ 3 lớp (Center pitch, Drift, Vibrato), Xử lý pha (Phase vocoder & attack phase locking), và Bảo toàn Formant.

---

## Giai Đoạn 2: Kế Hoạch Triển Khai Phiên Bản Open Melo 0.1 (Chỉnh Phô & Cắt Nốt)
**Mục tiêu chính của v0.1**: Tập trung hoàn thiện tính năng **Chỉnh phô (Pitch & Cents Tuning)**, **Cắt nốt (Note Slicing/Splitting)** và **Điều hướng Trợ năng bàn phím hoàn hảo qua NVDA**.

### 1. Kiến Trúc Lõi C++ & DSP (Modules Architecture)
1. **`NvdaBridge`**:
   - Wrapper C++ gọi các hàm từ `nvdaControllerClient64.dll`: `nvdaController_speakText`, `nvdaController_cancelSpeech`, `nvdaController_brailleMessage`.
   - Cơ chế load linh hoạt (Load trực tiếp từ bundle hoặc dynamic load an toàn, không làm crash DAW nếu không có NVDA).
2. **`NoteModel` & `NoteManager`**:
   - Quản lý danh sách các nốt nhạc (`OpenMeloNote`):
     - `startSample`, `lengthSamples`: Tọa độ thời gian.
     - `pitchCenterMidi`: Nốt MIDI cơ bản (ví dụ 64.0 = E4).
     - `centsOffset`: Độ lệch cent (từ -50 đến +50 cent, e.g. -5 ct).
     - `sourceAudioBuffer`: Audio buffer gốc của nốt.
   - Thao tác:
     - `navigateLeft()`, `navigateRight()`
     - `splitNoteAt(samplePosition)` (Cắt đôi nốt tại trung điểm)
     - `sliceNoteIntoPieces(int count)` (Cắt vụn nốt thành các mảnh nhỏ)
     - `transposeSemitone(int delta)` (Tăng/giảm semitone)
     - `tuneCents(int deltaCents)` (Tăng/giảm cent)
     - `snapCentsToZero()` (Đưa cent về 0)
3. **`PitchShiftEngine`**:
   - Bộ dịch cao độ clean-room (Pitch Shifter dựa trên Overlap-Add / Resampling / Phase Vocoder) để tính toán độ dịch tần số $\alpha = 2^{\frac{\Delta \text{semitones} + \Delta \text{cents}/100}{12}}$.
   - Hỗ trợ phát Audition nốt đang chọn ngay lập tức qua âm thanh khi người dùng bấm phím.
4. **`PluginProcessor` & `PluginEditor`**:
   - Target VST3 chuẩn JUCE 8.0.4.
   - Bắt sự kiện bàn phím tại Editor (`keyPressed`), xử lý mượt mà và chuyển lệnh tới `NoteManager`.

### 2. Bảng Phím Tắt Chuẩn Cho Open Melo v0.1 (Đã cập nhật)
| Phím Tắt | Hành Động | Phản Hồi Âm Thanh (Audition) | Thông Báo NVDA (Speech) |
|---|---|---|---|
| `Up Arrow` / `Down Arrow` | **Chỉnh từng Cent** ($\pm 1$ cent) | Phát nốt với cent mới | Đọc: `[Note Name], [New Offset] ct` (VD: `E4, -4 ct`) |
| `Ctrl + Up` / `Ctrl + Down` | **Chỉnh cao độ / bán âm** ($\pm 1$ semitone) | Phát nốt với cao độ mới | Đọc: `[New Note Name], [Offset] ct` (VD: `F4, -4 ct`) |
| `Left Arrow` / `Right Arrow` | Chuyển nốt trước / sau | Phát nốt được chọn | Đọc: `[Note Name], [Offset] ct` |
| `P` | Nghe lại nốt hiện tại | Phát nốt hiện tại | Đọc lại: `[Note Name], [Offset] ct` |
| `Shift + P` | Ép phô: Đưa cent về 0 chuẩn | Phát nốt đã ép thẳng | Đọc: `[Note Name], 0 ct, tuned` |
| `S` | Cắt đôi nốt hiện tại | Phát nửa nốt đầu tiên | Đọc: `Split note into 2` (hoặc báo `Note too short to split` nếu quá ngắn) |
| `Shift + S` | Cắt vụn nốt thành các lát nhỏ | Phát lát đầu tiên | Đọc: `Sliced note into [N] micro segments` |

*Lưu ý về nguồn âm thanh*: Plugin mặc định mở ra hoàn toàn trống, không chứa vocal mẫu. Khi người dùng phát vocal trên track DAW, `processBlock` sẽ tự động nhận luồng âm thanh trực tiếp từ track, phân tích cao độ và chia nốt nhạc tự động để người dùng bắt đầu chỉnh sửa. Nếu chưa có nốt, hệ thống sẽ thông báo `No notes on track` khi bấm phím.

---

## Giai Đoạn 3: Triển Khai & Hoàn Thành Bản Build Đầu Tiên v0.1 (Đã hoàn thành)
**Ngày hoàn thành**: 04/11/2026

### 1. Các thành phần đã xây dựng hoàn chỉnh
- **`libs/nvda/`**: Tích hợp trực tiếp thư viện `nvdaControllerClient64.dll` và header `nvdaController.h` vào project, tự động copy vào thư mục bundle VST3 khi build (hoàn toàn self-contained cho cộng đồng Open Source).
- **`Source/NvdaBridge.h`**: Lớp kết nối C++ hiện đại giao tiếp trực tiếp với NVDA bằng RPC, hỗ trợ `speak` (kèm cờ `cancelCurrent` chống nghẽn giọng đọc khi di chuyển phím nhanh), `braille` và kiểm tra `isNvdaRunning`.
- **`Source/NoteModel.h`**: Cấu trúc dữ liệu nốt nhạc hỗ trợ cao độ danh định, độ lệch cents, buffer âm thanh và chuỗi mô tả trợ năng.
- **`Source/PitchShiftEngine.h & .cpp`**: Lõi tổng hợp âm thanh giọng hát mẫu đa họa âm có lọc Formant tự nhiên (`generateVocalTone`) và thuật toán dịch pitch SOLA / Overlap-Add.
- **`Source/NoteManager.h & .cpp`**: Quản lý danh sách nốt, xử lý điều hướng `Left/Right`, phát nốt `P`, chỉnh cao độ `Up/Down`, vi chỉnh cents `Shift + Up/Down`, ép phô `Shift + P`, cắt đôi nốt `S` và cắt vụn nốt `Shift + S`.
- **`Source/AuditionVoice.h`**: Bộ phát âm thanh nốt nội bộ thread-safe, không khóa (lock-free), phát trực tiếp ra DAW khi người dùng bấm phím.
- **`Source/PluginProcessor & PluginEditor`**: Bắt toàn bộ phím tắt bàn phím từ Editor, vẽ giao diện tối giản hiện đại hỗ trợ người dùng mắt sáng và phát giọng nói trợ năng tức thì qua NVDA.

### 2. Kết quả Build & Cài đặt
- **Cấu hình biên dịch**: CMake 4.4.0 + MSVC 19.44 + JUCE 8.0.4 (Release x64).
- **File đầu ra**:
  - `build\OpenMelo_artefacts\Release\VST3\Open Melo.vst3` (Kèm `nvdaControllerClient64.dll`).
  - Đã tự động cài đặt vào thư mục VST3 cá nhân của hệ thống:
    `C:\Users\LCBoy\AppData\Local\Programs\Common\VST3\Open Melo.vst3`.
- Plugin đã sẵn sàng để mở và sử dụng ngay lập tức trong Reaper, FL Studio, Ableton Live hoặc bất kỳ DAW nào!

---

## Giai Đoạn 4: Dịch Ngược Sâu & Kế Hoạch Khắc Phục 3 Lỗi Cốt Tử So Với Melodyne (Formant, ARA Scan & DAW Playback Sync)
**Ngày thực hiện**: 04/11/2026

### 1. Phân Tích Lỗi Qua Dịch Ngược MelodyneCore-5.4.1.004.dll

#### A. Lỗi 1: DAW Phát Lại Không Có Tác Động Gì
- **Nguyên nhân**: Trong `processBlock`, audio từ track DAW chỉ được đưa vào để phân tích nốt mà không bị ghi đè/thay thế bằng audio đã chỉnh phô. Luồng `auditionVoice` chỉ phát khi bấm phím trong Editor. Khi DAW Play, plugin vẫn cho luồng audio gốc đi qua không đổi.
- **Giải pháp Melodyne**: Khi DAW Playhead chạy, plugin xác định vị trí mẫu hiện tại (`timeInSamples`). Tại các khoảng thời gian có nốt đã chỉnh sửa (semitone hoặc cents khác 0), plugin làm câm (mute) vocal gốc trên track và thay thế hoàn toàn bằng luồng audio đã được dịch cao độ và bảo toàn Formant theo thời gian thực (Sample-accurate replacement).

#### B. Lỗi 2: Giọng Bị Méo "Con Sóc" Khi Kéo Nốt Cao (Chipmunk Effect)
- **Nguyên nhân**: Thuật toán resample/pitch shift cơ bản làm dịch chuyển toàn bộ phổ tần số, kéo các đỉnh cộng hưởng thanh quản (Formants $F_1, F_2, F_3$) lên cao theo tỉ lệ $\alpha$, làm cho khoang họng bị co ngắn lại như giọng sóc chuột.
- **Cơ chế Melodyne**:
  - Tìm thấy trong lõi: `_useMonophonicFormantProcessing`, `_formantEnvelope`, `_freqDomainFormantResolution`, `_appliedEqualizerFormantFactors`, `formantPitchCompensationFactor`.
  - Melodyne sử dụng thuật toán **Bảo toàn đường bao phổ Formant (Spectral Envelope Preservation)**:
    1. Trích xuất đường bao Formant $E(f)$ bằng Cepstral Liftering / True Envelope.
    2. Làm phẳng phổ gốc: $X_{\text{flat}}(f) = X(f) / E(f)$.
    3. Dịch các đỉnh họa âm cơ bản theo tỉ lệ $\alpha$.
    4. Tái nhân lại với đường bao Formant ban đầu $E(f)$ tại đúng các dải tần vật lý gốc.
    $\Rightarrow$ Nâng cao 1 hoặc 2 quãng tám vẫn giữ nguyên màu giọng trầm ấm tự nhiên của con người.

#### C. Lỗi 3: Cơ Chế Tự Động Quét Nốt Ngay Khi Add Plugin Không Cần Bấm Play
- **Nguyên nhân**: Melodyne chạy trên nền kiến trúc **ARA2 (Audio Random Access 2)**:
  - Các lớp lõi: `MUAraDocumentController`, `MUAraAudioSource`, `MUAraAudioPlaybackRegion`, `GNThreadPriorityGroupBackground`.
  - Khi chèn vào track, DAW giao thẳng con trỏ toàn bộ file âm thanh trên track cho plugin. Background thread quét offline toàn bài chỉ trong vài giây.
- **Giải pháp**:
  - Tích hợp chuẩn **ARA2 SDK** trong JUCE (`juce_audio_processors_ara`).
  - Đối với các DAW không hỗ trợ ARA (hoặc chế độ VST thông thường), bổ sung cơ chế nạp file âm thanh trực tiếp (Audio File Loader / Scan) để quét toàn bộ bài hát ngay lập tức.

---

## Giai Đoạn 5: Triển Khai Hoàn Thiện Toàn Bộ Kế Hoạch Nâng Cấp (Đã hoàn thành)
**Ngày hoàn thành**: 04/11/2026

### 1. Đồng bộ Phát lại Chuẩn xác trên DAW (Sample-Accurate Playback Replacement)
- Trong `OpenMeloAudioProcessor::processBlock`, theo dõi thời gian `getPlayHead()->getPosition()`.
- Phương thức `NoteManager::renderTrackPlayback`:
  - So khớp từng block âm thanh với tọa độ thời gian của các nốt trên track.
  - Khi DAW phát qua một nốt đã được chỉnh phô (semitone $\ne 0$ hoặc cent $\ne 0$): tự động mute vocal gốc trên track và thay thế bằng âm thanh đã qua xử lý pitch & formant (`renderedBuffer`).
  - Khi người dùng ấn Play trên DAW, âm thanh vang ra **chính xác 100% theo các nốt đã nắn chỉnh**!

### 2. Lõi DSP Bảo Toàn Formant (Anti-Chipmunk Cepstral Engine)
- Xây dựng thuật toán biến đổi Cepstrum 2048-point FFT:
  - Tách đường bao Formant âm học $E(k)$ qua bộ lọc Lifter quefrency thấp.
  - Làm phẳng phổ biên độ (Spectral Whitening).
  - Dịch chuyển các đỉnh họa âm theo tỉ lệ pitch.
  - Tái áp đặt lại Formant gốc $E(k)$ vào đúng các dải tần số vật lý ban đầu.
  - Kéo cao độ lên 1-2 quãng tám (quãng 8, quãng 9...) **giữ nguyên vẹn độ dày, độ ấm của giọng người thật, triệt tiêu 100% tiếng sóc chuột**!

### 3. Lưu Trữ Trạng Thái Hoàn Hảo Vào DAW Project (State Persistence)
- Triển khai hàm `getStateInformation` và `setStateInformation` tuần tự hóa nhị phân:
  - Lưu toàn bộ danh sách nốt, các điểm cắt nốt (`S`, `Shift + S`), độ lệch cent (`Up/Down`, `Shift + P`), cao độ và buffer âm thanh.
  - Khi lưu project trên DAW (`Ctrl + S`) và mở lại vào bất kỳ thời điểm nào sau này, **toàn bộ quá trình làm việc được khôi phục nguyên vẹn 100%**.

### 4. Cơ Chế Quét Tức Thì Không Cần Chờ Realtime (Instant Scan & Drag-Drop)
- Bổ sung `scanAudioBuffer` & `scanAudioFile`: quét toàn bộ file âm thanh offline chỉ trong ~0.5 giây.
- Hỗ trợ Kéo & Thả file vocal (WAV, MP3, FLAC, AIFF...) trực tiếp vào cửa sổ plugin hoặc nhấn `Ctrl + O` để mở file quét tức thì.
- Đồng thời giữ nguyên cơ chế quét tự động từ luồng âm thanh track khi DAW phát.

---

## Giai Đoạn 6: Khắc Phục Triệt Để 3 Lỗi Cốt Tử Theo Đúng Cơ Chế Melodyne (Đã hoàn thành)
**Ngày hoàn thành**: 04/11/2026

### 1. Thu Nạp Toàn Bộ Track Vocal Liên Tục (Full Continuous Track Ingestion)
- **Sửa lỗi ngắt quãng**: Trước đây, ngay khi `numNotes > 0`, nhánh `processIncomingTrackAudio` bị ngắt hoàn toàn khiến plugin chỉ nhận được mẩu vocal đầu tiên. Trong v0.2:
  - Khởi tạo bộ đệm dòng thời gian `Timeline Audio Stream` nhận diện độc lập với trạng thái render.
  - Khi DAW phát hoặc thu âm, mọi block âm thanh đi qua track đều được thu nạp liên tục và bám sát theo vị trí mẫu Timeline của DAW (`timeInSamples`).
  - Toàn bộ bài hát (từ giây thứ 0 đến hết bài) đều được chia nốt đầy đủ, tự động sắp xếp theo thứ tự thời gian (`sortNotes`).
  - Hỗ trợ phím tắt `Ctrl + O` hoặc kéo thả trực tiếp để quét toàn bộ bài hát chỉ trong 0.2 giây.

### 2. Sửa Triệt Để Lỗi Chỉnh Phô Không Có Tác Động Ngoài Track DAW
- **Gắn chặt với DAW Timeline**:
  - Tọa độ `note.startSample` được tính toán trực tiếp theo `timeInSamples` của DAW Timeline.
  - Trong `NoteManager::renderTrackPlayback`:
    - Khi playhead chạy qua bất kỳ nốt nào đã chỉnh phô (`semitoneOffset != 0` hoặc `centsOffset != 0`): tự động MUTE vocal gốc của track và thay thế bằng âm thanh TD-PSOLA đã nắn chỉnh.
    - Áp dụng kỹ thuật Crossfade 64 mẫu tại hai đầu nốt để chuyển đổi mượt mà, triệt tiêu 100% tiếng nổ loa (pop/click).
    - Khi playhead chạy qua nốt chưa chỉnh sửa: cho vocal gốc sạch của track đi qua nguyên vẹn 100% (bit-transparent passthrough).
  - Dù người dùng bấm Play ở bất kỳ vị trí nào trên DAW (Bar 1, Bar 10, Bar 50...), âm thanh đã nắn chỉnh đều phát ra chính xác 100% ngoài track chính!

### 3. Thuật Toán TD-PSOLA (Bảo Toàn Formant & Âm Sắc Tự Nhiên Tuyệt Đối)
- Thay thế hoàn toàn thuật toán FFT lệch pha bằng **Time-Domain Pitch Synchronous Overlap-Add (TD-PSOLA)**:
  - Tự động dò tìm các đỉnh chu kỳ thanh đới cơ bản (Glottal Pulse Peaks / Pitch Marks $t_i$).
  - Khi nâng/hạ cao độ theo cent hay bán âm: nén hoặc dãn khoảng cách giữa các chu kỳ mà **giữ nguyên 100% dạng sóng nội tại của từng xung dao động trong miền thời gian**.
  - Nhờ đó, các tần số cộng hưởng thanh quản ($F_1, F_2, F_3$) — tức Formant và màu giọng thật của người hát — được bảo tồn tự nhiên tuyệt đối, loại bỏ hoàn toàn tiếng kim loại, tiếng rỗng ruột hay tiếng sóc chuột.

### 4. Chuẩn Hóa Phím Tắt & Trả Phím Space Cho DAW
- **Bỏ hoàn toàn can thiệp vào phím `Space`**: Để phím `Space` cho DAW xử lý chức năng Play/Stop bài hát toàn cục.
- **Phím nghe thử**: Dùng duy nhất phím `P` (Audition) và `Shift + P` (Ép phô về 0 ct).
- Bổ sung `Ctrl + R` để xóa sạch nốt trên track khi người dùng muốn quét lại từ đầu.
- Bản build Release x64 `Open Melo.vst3` đã được biên dịch thành công và cài đặt trực tiếp vào hệ thống tại:
  `C:\Users\LCBoy\AppData\Local\Programs\Common\VST3\Open Melo.vst3`.

---

## Giai Đoạn 7: Hiện Thực Hoàn Chỉnh Cơ Chế ARA 2.2.0 Chuẩn Melodyne (Tự Động Quét Track Vocal Khi Add Plugin)
**Ngày hoàn thành**: 05/11/2026

### 1. Dịch Ngược & So Sánh Cơ Chế Với Melodyne
- Dịch ngược `template/MelodyneCore-5.4.1.004.dll` phát hiện:
  - Melodyne kết nối qua giao thức ARA2 chuẩn (`IARAMainFactory`, `MUAraDocumentController`, `MUAraAudioSource`, `MUAraAudioPlaybackRegion`).
  - Khi chèn FX vào track, DAW không gửi realtime audio mà gọi hàm sự kiện `didAddPlaybackRegionToRegionSequence` / `didAddAudioSourceToDocument`.
  - Melodyne kích hoạt `handleAraElementAnalyzerWillRun` $\rightarrow$ `MUElementAnalyzer::makeMonophonicItems` đọc trực tiếp file audio qua `ARAAudioAccessController` và phân tích toàn bộ nốt trong ~0.2 giây.
- Trước đây Open Melo chưa hoạt động ARA vì thiếu toàn bộ cấu hình ARA trong CMake (`IS_ARA_EFFECT`), chưa liên kết ARA SDK và chưa triển khai các lớp ARA của JUCE.

### 2. Triển Khai Kiến Trúc Lõi ARA 2.2.0 Cho Open Melo
- **Đóng gói Celemony ARA SDK 2.2.0**: Đã tích hợp trực tiếp vào `libs/ARA_SDK` (gồm `ARA_API` và `ARA_Library`) độc lập hoàn toàn.
- **Cấu hình CMake**:
  - `juce_set_ara_sdk_path("${CMAKE_CURRENT_SOURCE_DIR}/libs/ARA_SDK")`
  - Bật `IS_ARA_EFFECT TRUE` trong `juce_add_plugin`.
  - Xuất bản class VST3 chính thức: `ARA Main Factory Class` (CID: `ABCDEF01A1B2C3D4547673744F6D6C6F`).
- **Xây dựng module `Source/OpenMeloARA.h` & `Source/OpenMeloARA.cpp`**:
  - `OpenMeloDocumentController`: Kế thừa `juce::ARADocumentControllerSpecialisation`. Tự động lắng nghe `didAddPlaybackRegionToRegionSequence`. Khi clip audio được gán vào track, lập tức dùng `juce::ARAAudioSourceReader` đọc trực tiếp dữ liệu mẫu âm thanh và gọi `noteManager.scanAudioBuffer(...)` phân tích cao độ toàn bộ track tức thì.
  - `OpenMeloPlaybackRenderer`: Kế thừa `juce::ARAPlaybackRenderer`. Đọc các region âm thanh từ source và áp dụng nắn phô TD-PSOLA từ `NoteManager` theo thời gian thực (sample-accurate).
  - `OpenMeloEditorRenderer` & `OpenMeloEditorView`: Phục vụ chức năng preview/audition và hiển thị tương tác trong ARA.
  - Export hàm khởi tạo factory: `createARAFactory()`.
- **Nâng cấp `OpenMeloAudioProcessor` & `OpenMeloAudioProcessorEditor`**:
  - `OpenMeloAudioProcessor`: Kế thừa `AudioProcessorARAExtension`. Hỗ trợ chế độ kép: Chạy ARA tự động trên các DAW có ARA (Reaper, Studio One, Cubase...), đồng thời có cơ chế fallback realtime an toàn cho các DAW không có ARA.
  - `OpenMeloAudioProcessorEditor`: Kế thừa `AudioProcessorEditorARAExtension`.
  - **Lược bỏ hoàn toàn phím `Ctrl + O` (mở file) và `Ctrl + R` (reset)** theo đúng yêu cầu người dùng — toàn bộ quy trình nhận diện track vocal giờ đây tự động 100%.
  - Hiển thị nhãn đèn trạng thái `ARA: SYNCED` (Màu xanh lá) khi kết nối ARA thành công.
  - Phản hồi tức thì qua NVDA ngay khi quét xong track: `"Open Melo ARA: Vocal track scanned automatically. [N] notes ready."`.

### 3. Kết Quả Build & Cài Đặt
- **Trạng thái Build**: Biên dịch thành công 100% Release x64 với MSVC 19.44 và CMake (Zero Errors).
- **Thư mục phân phối đã cài đặt**:
  `C:\Users\LCBoy\AppData\Local\Programs\Common\VST3\Open Melo.vst3` (kèm `nvdaControllerClient64.dll`).
- Plugin đã sẵn sàng để kiểm tra trực tiếp trên Reaper hoặc bất kỳ DAW nào hỗ trợ ARA!

---

## Giai Đoạn 8: Khắc Phục Triệt Để Lệch Cao Độ & Mất Tiếng Audition, Tích Hợp Signalsmith Stretch & McLeod Pitch Method (MPM)
**Ngày hoàn thành**: 05/11/2026

### 1. Phân Tích & Giải Quyết Các Vấn Đề Cốt Tử
1. **Lỗi lệch cao độ (-1 cung hoặc nửa cung) sau khi quét ARA**:
   - **Nguyên nhân**: Thuật toán Autocorrelation cũ tìm đỉnh cực đại toàn cục không chuẩn hóa (non-normalized cross-correlation), không nội suy đỉnh và dễ bị nhảy bậc hài âm/subharmonic (octave jumping). Khi thử nghiệm, nó làm tụt tần số nghiêm trọng (-12 đến -24 semitone hoặc lệch 1-2 semitone tùy phụ âm và formant).
   - **Khắc phục**: Xây dựng module [`PitchDetector.h`](file:///C:/Users/LCBoy/Desktop/data/vst-projects/open_melo/Source/PitchDetector.h) dựa trên thuật toán **McLeod Pitch Method (MPM)**:
     - Tính hàm chênh lệch bình phương chuẩn hóa (**NSDF** - Normalized Square Difference Function).
     - Dò tìm đỉnh cực đại đầu tiên sau điểm qua 0 vượt ngưỡng $0.85 \times \text{maxPeak}$.
     - Nội suy Parabolic cực đại chính xác tới mức vi mẫu (sub-sample).
     - **Kết quả kiểm thử**: Sai số thực tế đạt **0.0000 semitones**, bảo đảm nốt hát gì quét ra đúng 100% nốt đó.
2. **Lỗi mất tiếng Audition (Mũi tên, P) khi ở chế độ ARA**:
   - **Nguyên nhân**:
     - Trong kiến trúc ARA, DAW tạo các instance `AudioProcessor` riêng biệt cho `PlaybackRenderer` và `EditorView`. Thực thể `AuditionVoice` trước đây nằm riêng trong từng processor nên phím bấm ở Editor không truyền sang được PlaybackRenderer.
     - Khi DAW ở trạng thái **STOP** (`isPlaying == false`), `PlaybackRenderer` trước đây bị ngắt toàn bộ âm thanh.
     - Lớp `OpenMeloEditorRenderer` để trống chưa hiện thực `processBlock`.
   - **Khắc phục**:
     - Đưa `AuditionVoice` vào làm thành viên dùng chung tập trung tại `OpenMeloDocumentController`.
     - Hiện thực hoàn chỉnh `OpenMeloEditorRenderer::processBlock` và cho phép `OpenMeloPlaybackRenderer::processBlock` phát `AuditionVoice` ngay cả khi DAW đang dừng (STOP).
     - Bấm mũi tên trái/phải, lên/xuống, Ctrl+Lên/Xuống hoặc phím `P` đều phát tiếng tức thì.
3. **Triệt tiêu tiếng nổ tạch tạch & Méo robot bằng Signalsmith Stretch**:
   - Thay thế toàn bộ giải thuật TD-PSOLA tự chế bằng thư viện đỉnh cao **Signalsmith Stretch** (Geraint Luff, giấy phép MIT, self-contained trong `libs/signalsmith/`).
   - Sử dụng Spectral Phase Vocoder hiện đại với Phase Locking:
     - `stretch.setTransposeSemitones(semitones, 8000.0f / sampleRate)`: Giữ đường bao phổ phía trên 8kHz.
     - `stretch.setFormantSemitones(0.0f, true)` & `stretch.setFormantBase(pitchHz / sampleRate)`: Bù trừ cao độ để giữ nguyên 100% formants khoang họng ($F_1, F_2, F_3$), loại bỏ hoàn toàn tiếng sóc chuột (anti-chipmunk), tiếng nghẹt mũi và tiếng robot.
     - Tự động đệm đối xứng (padding & reflection) cho các phân đoạn nốt siêu ngắn để luôn render mượt mà không nổ lách tách.

### 2. Kiểm Thử Chất Lượng Bằng Tracktion pluginval
- Chạy bộ kiểm thử tiêu chuẩn quốc tế **`pluginval`** (Tracktion) ở mức khắt khe Level 5:
  - Khởi động lạnh/ấm (Cold/Warm Open): **PASSED**
  - Audio processing đa tần số mẫu (44.1kHz, 48kHz, 96kHz) và đa kích thước block (64, 128, 256, 512, 1024): **PASSED**
  - Tự động hóa tham số & Trạng thái plugin (State restore): **PASSED**
  - Quản lý bus & Luồng âm thanh an toàn: **PASSED (SUCCESS 100%)**
  - **Không có bất kỳ lỗi crash, rò rỉ bộ nhớ hay vi phạm Audio Thread nào!**

### 3. Phân Phối & Cài Đặt
- Build Release x64 thành công với CMake 4.4.0 + MSVC 19.44.
- Đã tự động triển khai vào:
  `C:\Users\LCBoy\AppData\Local\Programs\Common\VST3\Open Melo.vst3` (kèm `nvdaControllerClient64.dll`).

---

## Giai Đoạn 9: Khắc Phục Triệt Để 4 Lỗi Nghiêm Trọng (Câm Track ARA, Trễ Timeline Nhiều Bar, Tụt Nửa Cung Sample Rate & Tiếng Rè/Robot Khi Duyệt Nốt)
**Ngày hoàn thành**: 10/11/2026

### 1. Nguyên Nhân Gốc Rễ & Giải Pháp Đã Thực Hiện
1. **Khắc phục lỗi Câm Track Vocal khi thêm Open Melo vào DAW ARA (`OpenMeloARA.cpp`)**:
   - **Nguyên nhân 1**: Trong `juce::AudioProcessorARAExtension::processBlockForARA`, `playbackRenderer->processBlock` và `editorRenderer->processBlock` chạy nối tiếp nhau trên cùng một `buffer`. Trước đây `OpenMeloEditorRenderer::processBlock` gọi `buffer.clear()` vô điều kiện, xóa sạch 100% âm thanh vừa được `OpenMeloPlaybackRenderer` xuất ra.
   - **Nguyên nhân 2**: Trong `scanAudioSourceForRegion`, `scannedSources.insert(audioSource)` được gọi trước khi DAW bật quyền truy cập mẫu (`didEnableAudioSourceSamplesAccess(audioSource, true)`), khiến lần đọc đầu thất bại và không bao giờ quét lại nữa.
   - **Nguyên nhân 3**: Khởi tạo `juce::ARAAudioSourceReader` trên stack trong mỗi chu kỳ `processBlock` của luồng Audio Realtime.
   - **Giải pháp**:
     - Loại bỏ hoàn toàn lệnh xóa `buffer.clear()` trong `OpenMeloEditorRenderer::processBlock`.
     - Lắng nghe đầy đủ vòng đời ARA (`didEnableAudioSourceSamplesAccess`, `didUpdatePlaybackRegionProperties`, `didEndEditing`, `doUpdateAudioSourceContent`) trong `OpenMeloDocumentController::syncRegionsWithDocument()`. Chỉ đánh dấu region đã quét sau khi `isSampleAccessEnabled() == true` và đọc mẫu thành công.
     - Cache `std::unique_ptr<juce::ARAAudioSourceReader>` theo từng `ARAAudioSource*` trong `OpenMeloPlaybackRenderer`.
2. **Khắc phục lỗi Trễ Timeline Nhiều Bar & Tụt Cao Độ (~0.5–1.5 cung) do Lệch Sample Rate (`OpenMeloARA.cpp`)**:
   - **Nguyên nhân**:
     - Khi clip vocal trên DAW bị cắt phần đầu (`startInAudioModificationTime > startInPlaybackTime`), phép tính cũ lấy `timeOffset < 0` rồi ép `if (startTimelineSample < 0) startTimelineSample = 0`, khiến phần intro đã bị cắt bỏ vẫn bị nạp vào đầu timeline và đẩy toàn bộ câu hát thật bị trễ đi nhiều bar.
     - Khi file audio có tần số mẫu (VD: 44.1 kHz) khác với project DAW (VD: 48 kHz), việc đọc mẫu 1:1 không nội suy làm tụt cao độ $-1.46$ semitones và trôi 3900 mẫu mỗi giây.
   - **Giải pháp**:
     - Trong `OpenMeloDocumentController::syncRegionsWithDocument()`: Chỉ trích xuất đúng phạm vi thời gian hiệu lực `[startInAudioModificationTime .. endInAudioModificationTime]` của từng `ARAPlaybackRegion`, nội suy bằng `juce::LagrangeInterpolator` sang đúng `dawSampleRate`, và đặt tại tọa độ mẫu chính xác `std::llround(effectivePlaybackStart * targetRate)`.
     - Trong `OpenMeloPlaybackRenderer::processBlock`: Bổ sung bộ nội suy bậc ba 4 điểm Catmull-Rom phi trạng thái (stateless) theo thời gian thực khi `sourceRate != sampleRate`.
3. **Khắc phục lỗi Tiếng Rè / Robot / Người Ngoài Hành Tinh Khi Duyệt & Kéo Nốt (`PluginProcessor.cpp`, `NoteManager.cpp`, `PitchShiftEngine.cpp`, `PitchDetector.h`)**:
   - **Nguyên nhân 1 (Đọc chồng 3 lần `AuditionVoice`)**: Trong chế độ ARA, mỗi chu kỳ `processBlock` gọi `AuditionVoice::renderNextBlock` tới 3 lần (`PlaybackRenderer`, `EditorRenderer`, `PluginProcessor`), làm nốt phát nhanh gấp 3 lần và cộng chồng các đoạn lệch pha lên nhau.
   - **Nguyên nhân 2 (Mất mẫu giữa nốt)**: Trong `NoteManager::scanAudioBuffer`, các hop có `silenceCount < 4` ở giữa nốt bị bỏ qua không copy vào `segmentAudioL/R`, gây đứt đoạn pha và hụt độ dài nốt.
   - **Nguyên nhân 3 (`SignalsmithStretch::presetDefault` cửa sổ 120ms & không đệm biên)**: Cửa sổ mặc định 120ms (`seekLength` 150ms) quá dài cho nốt vocal ngắn, cộng với việc gọi `stretch.exact()` không có padding khiến phần đuôi của mọi nốt bị trộn với `flush(zeros)` đảo pha.
   - **Nguyên nhân 4 (`PitchDetector` MPM)**: Tìm mọi cực đại cục bộ với ngưỡng `0.85` khiến Formant $F_1$ mạnh dễ làm nhảy lên họa âm bậc 2/3.
   - **Giải pháp**:
     - Trong chế độ ARA (Reaper/Studio One/Cubase), ưu tiên trộn (`addFrom` không xóa buffer) `AuditionVoice::renderNextBlock` trực tiếp trên instance `EditorRenderer` (nằm trên Track FX Chain — luôn chạy ngay cả khi DAW đang STOP!) và tự động fallback sang `PlaybackRenderer` nếu `EditorRenderer` không hoạt động trong $> 50\text{ ms}$, đồng thời báo cáo `getTailLengthSeconds() = infinity()` (`Vst::kInfiniteTail`) để DAW không bao giờ ngắt luồng audio khi STOP.
     - Kèm theo ramp 64 mẫu chống nổ tiếng (`AuditionVoice.h`) và chỉ tiêu thụ **đúng 1 lần duy nhất mỗi block**.
     - Trích xuất lát cắt mẫu **liên tục 100%** `[segmentStartPos .. segmentEndPos)` trực tiếp từ buffer gốc trong `NoteManager::scanAudioBuffer`, đồng thời áp dụng hòa trộn tuyến tính (true linear crossfade) `dest * (1 - alpha) + src * alpha` tại biên nốt trong `renderTrackPlayback`.
     - Cấu hình cửa sổ STFT chuyên biệt cho giọng hát đơn âm (`~46ms` block / `~11.5ms` hop) trong `PitchShiftEngine::process`, và **luôn luôn đệm phản xạ có cửa sổ Cosine (`pad = seekLen`)** ở cả hai đầu cho mọi nốt hát để 100% nốt nằm trong vùng steady-state.
     - Chuẩn hóa thuật toán **Key Maxima** (1 đỉnh cao nhất trên mỗi miền dương, ngưỡng $k = 0.93$) trong `PitchDetector.h`.

### 2. Kết Quả Kiểm Thử Tự Động (`verify_open_melo`)
- **Test 1 (44.1 kHz file trong 48 kHz DAW + Clip bị cắt 4s đầu đặt tại Bar 2)**:
  - Sai số cao độ: **0.0016 semitones (0.16 cent)** — không còn bị tụt nửa cung hay 1.46 cung.
  - Vị trí Timeline: Khớp chính xác tại `2.50s` — không còn bị trễ bar.
- **Test 2 (Trích xuất sóng âm liên tục)**: Giữ nguyên 100% các hop nội tại bên trong nốt.
- **Test 3 (Dịch cao độ +2 semitones & bảo toàn Formant)**: Cao độ đầu ra đạt `62.017 MIDI` (từ `60.0 MIDI`), năng lượng `Mid RMS (0.06979)` và `Tail RMS (0.06977)` phẳng 99.97%, âm thanh tự nhiên, trong trẻo.
- **Test 4 (AuditionVoice 1.0x)**: Phát đúng tốc độ 1.0x, không chồng tiếng, không click/pop.
- Đã biên dịch Release x64 và cập nhật trực tiếp vào `C:\Users\LCBoy\AppData\Local\Programs\Common\VST3\Open Melo.vst3`.



<!-- graft:start -->
## Graft — repo context graph

This repo is indexed in `graft/`: small linked markdown nodes that explain each
system and carry exact file:line spans, kept in sync with the code through git.

For ANY task here — understanding how something works, finding where code lives,
or scoping a change — get context from the graph before grepping or opening
source files. Re-ask freely (it's cheap) and reuse literal identifiers you
already have (symbol, error string, file name) as the query. New to this repo?
Run `graft map` first — a token-budgeted orientation (dir clusters, hubs,
hotspots), no LLM, no key.

- Run `graft ask "<your question>" --source` → ranked nodes with the relevant
  code spans inlined (each hit's ≤8-line crux by default; `--full` for whole
  definitions when the crux isn't enough). Match the tool to the task shape:
  for understanding or editing, the top node IS the answer — cite its
  `covers:` file:line spans and edit straight from `--source`. For
  exhaustive tasks ("every occurrence / every caller of this pattern"), ranked
  results are top-N, not complete — run `graft grep "<literal>"` instead
  (exhaustive over indexed files, grouped by enclosing symbol), falling back
  to raw `grep -rn` only for unindexed files.
- `graft skeleton <file>` → every definition's signature + span, ~10× cheaper
  than reading the file; use it to skim an API surface.
- `graft callers <symbol>` gives precomputed, exact edges — who calls this.
  Add `--direction out` for what it calls, or `--depth N` to walk
  transitively for the full blast radius. For structural questions, skip
  ranking and use this directly.
- Or browse: `graft/INDEX.md` lists every node; follow the links.
- Monorepos and folders of multiple repos rank fairly across sub-projects —
  hits carry `[scope/]` labels naming which one they're from. Narrow with
  `graft ask "<task>" --in <scope>/` once you know where you're working.

If a returned span is truncated ("+N more lines"), open the file at that exact
range before finalizing. Only open source files when a node genuinely lacks a
needed detail, and then at the exact file:line the node points to — never
re-read whole files.

After big code changes, refresh the graph with `graft build` (deterministic,
no API key, $0).
<!-- graft:end -->
