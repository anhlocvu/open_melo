# Feature Specification: Khắc Phục Mất Tiếng Track Vocal, Méo Tiếng Robot/Alien & Lệch Timeline/Cao Độ Trong ARA2 / VST3

**Feature Branch**: `001-fix-ara-timeline-audio`

**Created**: 2026-11-10

**Status**: Draft (Ready for User Approval)

**Input**: User description: "lúc mà add open melo vào phần mềm, nó câm cái track vocal đó luôn. đồng thời, mình vào open melo, duyệt note, kéo note, các thứ thì không biết nó có nhận vào daw không nhưng mà tôi duyệt note kéo note thì nó bị rè và nghe như robot hay người ngoài hành tinh á. kinh lắm. chuẩn hóa time line của daw sao cho đúng đi. với lại lên mạng tìm xem có thư viện nào hữu dụng cho vst này không làm sao mà khi add open melo là time line không bị chễ hay giảm xuống nửa cung là được trong các phiên bản trước, 1. khi chưa add open melo, time line rất đúng, sau khi add open melo là nó bị trễ đi mấy bar á."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Giữ Nguyên Tiếng Track Vocal & Đồng Bộ Timeline Tuyệt Đối Khi Chèn Plugin (Priority: P1)

Khi người dùng chèn Open Melo vào track vocal trên DAW (cả chế độ ARA2 như Reaper/Studio One/Cubase lẫn chế độ VST3 thường), track vocal KHÔNG bao giờ bị câm, KHÔNG bị trễ nhịp (dù media item đặt ở đầu bài, giữa bài ở Bar 5/Bar 10 hay đã bị cắt đầu/trim), và KHÔNG bị tụt nửa cung/một cung khi tần số mẫu của file (ví dụ 44.1 kHz) khác tần số mẫu của DAW (ví dụ 48 kHz).

**Why this priority**: Đây là lỗi chặn hoàn toàn việc sử dụng plugin (blocker): chèn plugin vào làm câm track hoặc làm lệch nhịp nhiều bar và lệch cao độ so với beat nhạc.

**Independent Test**: Chèn Open Melo vào một track vocal (44.1 kHz hoặc 48 kHz, đặt ở bất kỳ bar nào hoặc đã cắt gọt clip) trên DAW và bấm Play: âm thanh phát ra ngay lập tức, khớp 100% nhịp (0 ms trễ) và đúng 100% cao độ gốc khi chưa chỉnh sửa nốt.

**Acceptance Scenarios**:

1. **Given** một track vocal trên DAW (có thể ở Bar 1 hoặc Bar 10, hoặc đã trim đầu clip), **When** người dùng chèn Open Melo vào track và bấm Play, **Then** tiếng vocal phát ra đầy đủ, không bị câm và khớp chính xác từng mẫu (sample-accurate) với timeline của DAW.
2. **Given** một file vocal 44.1 kHz đặt trong project DAW chạy ở 48 kHz (hoặc ngược lại), **When** chèn Open Melo vào track, **Then** cao độ và tốc độ phát không bị giảm nửa cung/một cung và không bị trôi timeline.

---

### User Story 2 - Âm Thanh Nghe Thử (Audition) & Kéo Nốt Mượt Mà, Tự Nhiên, Không Bị Rè Hay Robot/Alien (Priority: P1)

Khi người dùng dùng bàn phím (Mũi tên Trái/Phải, Lên/Xuống, Ctrl+Lên/Xuống, P, Shift+P, S, Shift+S) để duyệt và kéo cao độ nốt kèm phản hồi NVDA, âm thanh phát ra phải trong trẻo, tự nhiên như giọng người thật, không bị rè, không bị nhân 3 tốc độ hay chồng chéo kiểu robot/người ngoài hành tinh, và mọi thay đổi phải có hiệu lực ngay trên track của DAW khi bấm Play.

**Why this priority**: Người dùng khiếm thị sử dụng trình đọc màn hình NVDA phụ thuộc hoàn toàn vào âm thanh Audition khi duyệt và kéo nốt để chỉnh phô.

**Independent Test**: Mở cửa sổ Open Melo, bấm phím Mũi tên Trái/Phải hoặc P để nghe nốt, và bấm Lên/Xuống hoặc Ctrl+Lên/Xuống để kéo nốt: tiếng hát phát ra đúng tốc độ gốc (1x), liền mạch, giữ nguyên Formant giọng người thật, không rè, và khi bấm Space để DAW Play thì nốt tại đúng vị trí đó trên track đã được thay thế bằng âm thanh mới.

**Acceptance Scenarios**:

1. **Given** danh sách nốt đã được quét từ track vocal, **When** người dùng bấm Mũi tên Trái/Phải hoặc phím P (kể cả khi DAW đang Stop hay Play), **Then** nốt nhạc phát ra rõ ràng, đúng độ dài và tốc độ gốc, không bị cắt khúc hay chồng pha gây rè/robot, đồng thời NVDA đọc tức thì tên nốt và số cent.
2. **Given** một nốt đang được chọn, **When** người dùng kéo cao độ bằng Up/Down (±1 cent) hoặc Ctrl+Up/Down (±1 semitone), **Then** âm thanh nghe thử và âm thanh phát trên timeline DAW đều phản ánh đúng cao độ mới với âm sắc tự nhiên (bảo toàn Formant).

---

### Edge Cases

- Khi DAW thêm `ARAPlaybackRegion` trước khi bật quyền truy cập mẫu (`didEnableAudioSourceSamplesAccess(true)`), hệ thống phải tự động đợi và quét ngay khi quyền truy cập mẫu được bật thay vì bỏ qua nguồn âm thanh đó.
- Khi một nốt ngân dài có thoáng qua phụ âm hoặc khoảng sụt năng lượng ngắn (< 4 khung), bộ tách nốt không được làm rơi (drop) các mẫu âm thanh ở giữa nốt gây đứt đoạn sóng và lệch độ dài nốt.
- Khi clip âm thanh trên DAW được cắt bớt phần đầu (`startInAudioModificationTime > 0`) hoặc dời sang Bar khác (`startInPlaybackTime > 0`), tọa độ timeline của từng nốt phải ánh xạ chính xác theo từng vùng phát (`PlaybackRegion`).

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Hệ thống PHẢI giữ nguyên luồng âm thanh đầu ra của `PlaybackRenderer` trong chu trình xử lý ARA, tuyệt đối không để `EditorRenderer` xóa trắng (`clear`) bộ đệm âm thanh của track.
- **FR-002**: Hệ thống PHẢI phát âm thanh nghe thử (`AuditionVoice`) đúng 1 lần duy nhất trên mỗi chu kỳ khung âm thanh (audio block), tránh việc rút mẫu 3 lần liên tiếp trong cùng 1 block gây tua nhanh 3x và chồng pha biến dạng kiểu robot/alien.
- **FR-003**: Hệ thống PHẢI lắng nghe sự kiện cấp quyền đọc mẫu âm thanh (`didEnableAudioSourceSamplesAccess`) và cập nhật vùng phát (`didUpdatePlaybackRegionProperties`) để đảm bảo quét nốt thành công 100% ngay khi thêm plugin vào track.
- **FR-004**: Hệ thống PHẢI chuẩn hóa tần số mẫu (Sample Rate Resampling) và công thức ánh xạ thời gian giữa nguồn âm thanh (`AudioSource`), vùng sửa đổi (`AudioModification`) và dòng thời gian DAW (`PlaybackRegion` / `PlayHead`), đảm bảo không trễ bar và không lệch cao độ khi khác tần số mẫu (44.1 kHz vs 48 kHz).
- **FR-005**: Hệ thống PHẢI trích xuất dữ liệu âm thanh của từng nốt trực tiếp và liên tục từ vùng đệm gốc theo đúng khoảng mẫu `[startSample, startSample + lengthSamples]`, không được bỏ sót các khung (hop) ở giữa nốt.
- **FR-006**: Hệ thống xử lý dịch cao độ và bảo toàn Formant PHẢI được tối ưu hóa cho giọng hát đơn âm (monophonic vocal) với cửa sổ phân tích ngắn chuẩn xác, có làm mượt đầu-cuối (fade-in/fade-out/crossfade) để triệt tiêu tiếng rè và tiếng kim loại.

### Key Entities

- **OpenMeloNote**: Thực thể nốt nhạc chứa tọa độ thời gian trên nguồn/timeline, độ dài mẫu, cao độ gốc MIDI, độ lệch bán âm/cent, dữ liệu âm thanh gốc liên tục và dữ liệu âm thanh đã dịch cao độ.
- **OpenMeloDocumentController**: Bộ điều khiển tài liệu ARA quản lý việc quét nguồn âm thanh khi `isSampleAccessEnabled()` khả dụng và đồng bộ trạng thái giữa các Renderer.
- **OpenMeloPlaybackRenderer / OpenMeloEditorRenderer**: Các vai trò kết xuất âm thanh ARA chịu trách nhiệm đọc mẫu từ host (có hỗ trợ chuyển đổi tần số mẫu), thay thế các nốt đã chỉnh phô và phát tiếng nghe thử (`AuditionVoice`).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 100% trường hợp chèn Open Melo vào track vocal trên DAW phát ra tiếng bình thường ngay lập tức (không bị câm track).
- **SC-002**: Độ lệch thời gian (timeline drift/delay) khi chèn Open Melo bằng 0 mẫu (0 ms) tại mọi vị trí bar (Bar 1, Bar 10, Bar 50...) và với mọi clip đã cắt/trim.
- **SC-003**: Sai số cao độ khi chèn Open Melo giữa file 44.1 kHz và project 48 kHz (hoặc ngược lại) bằng 0.00 cent (không bị giảm nửa cung).
- **SC-004**: Khi duyệt nốt và kéo nốt bằng bàn phím, âm thanh Audition phát đúng tốc độ 1.0x, không có hiện tượng nhảy mẫu 3x hay rè robot, phản hồi giọng đọc NVDA tức thì.

## Assumptions

- Người dùng sử dụng DAW hỗ trợ VST3 và ARA2 (như Reaper) trên Windows 64-bit cùng trình đọc màn hình NVDA.
- Tất cả thư viện DSP sử dụng đều được đóng gói tự chứa (self-contained) trực tiếp trong mã nguồn dự án để đảm bảo tính độc lập khi biên dịch.
