# DJC-DIY Controller — Firmware & Wiring Guide

DIY 2-deck DJ MIDI controller dùng Arduino **Pro Micro** (ATmega32U4), giao tiếp
qua USB-MIDI (thư viện `MIDIUSB`), dùng chung với mapping Mixxx `DJC-DIY.midi.xml`.

> Sơ đồ đi dây đầy đủ nằm ở [`docs/wiring-diagram.png`](docs/wiring-diagram.png)
> (ảnh `06.webp` gốc). Bảng bên dưới liệt kê chính xác từng chân theo code —
> dùng bảng này để đối chiếu khi hàn/đấu dây thực tế.

## Mục lục
- [Bill of Materials](#bill-of-materials)
- [Sơ đồ chân Pro Micro](#sơ-đồ-chân-pro-micro)
- [Hướng dẫn đi dây](#hướng-dẫn-đi-dây)
- [Nạp firmware](#nạp-firmware)
- [Hiệu chỉnh (calibration)](#hiệu-chỉnh-calibration)
- [Cài mapping Mixxx](#cài-mapping-mixxx)

---

## Bill of Materials

| Số lượng | Linh kiện |
|---|---|
| 1 | Arduino Pro Micro (ATmega32U4, 5V/16MHz) |
| 6 | Nút nhấn (tactile push button) — pad hotcue/play/cue |
| 2 | Nút nhấn đọc qua chân analog (dùng chung 1 chân ADC) |
| 4 | 10KΩ resistor (2 cho thang điện trở nút analog, 2 cho pull-down/chia áp khác nếu cần) |
| 4 | Biến trở tuyến tính (linear potentiometer) — 2 EQ low + 2 FX/filter |
| 3 | Biến trở có lò xo về giữa (center-detent pot) — 2 Tempo/Pitch + 1 Crossfader |
| 2 | Encoder xoay (rotary encoder, quadrature A/B) — jog wheel Deck 1 & Deck 2 |
| — | Dây, vỏ hộp in 3D / khoan tay tuỳ thiết kế |

## Sơ đồ chân Pro Micro

Chân bên trái board (từ trên xuống): `TXO, RXI, GND, GND, 2, 3, 4, 5, 6, 7, 8, 9`
Chân bên phải board (từ trên xuống): `RAW, GND, RST, VCC, A3, A2, A1, A0, 15, 14, 16, 10`

Bảng chân → chức năng (khớp với `firmware_optimized.ino`):

| Chân Pro Micro | Loại | Nối tới | MIDI ra |
|---|---|---|---|
| 5 | Digital, `INPUT_PULLUP` | Nút Hotcue 1 – Deck 1 | Note 60 (0x3C), ON/OFF |
| 7 | Digital, `INPUT_PULLUP` | Nút Hotcue 2 – Deck 1 | Note 61 (0x3D) |
| 10 | Digital, `INPUT_PULLUP` | Nút Hotcue 2 – Deck 2 | Note 62 (0x3E) |
| 14 | Digital, `INPUT_PULLUP` | Nút Play – Deck 2 | Note 63 (0x3F) |
| 15 | Digital, `INPUT_PULLUP` | Nút Cue – Deck 2 | Note 64 (0x40) |
| 16 | Digital, `INPUT_PULLUP` | Nút Hotcue 1 – Deck 2 | Note 65 (0x41) |
| A6 | Analog (dùng chung, thang điện trở) | Nút Play – Deck 1 | Note 66 (0x42) |
| A6 | Analog (dùng chung, thang điện trở) | Nút Cue – Deck 1 | Note 67 (0x43) |
| A0 | Analog | FX/Filter (super1) – Deck 2 | CC 10 (0x0A) |
| A1 | Analog | EQ Low – Deck 2 | CC 11 (0x0B) |
| A2 | Analog | FX/Filter (super1) – Deck 1 | CC 12 (0x0C) |
| A3 | Analog | EQ Low – Deck 1 | CC 13 (0x0D) |
| A7 | Analog, center-detent | Tempo/Pitch – Deck 1 | CC 14 (0x0E) |
| A8 | Analog, center-detent | Crossfader | CC 15 (0x0F) |
| A9 | Analog, center-detent | Tempo/Pitch – Deck 2 | CC 16 (0x10) |
| 0 (RXI) + 1 (TXO) | Digital quadrature A/B | Encoder jog wheel – Deck 1 | CC 20 (0x14) |
| 3 + 2 | Digital quadrature A/B | Encoder jog wheel – Deck 2 | CC 21 (0x15) |

## Hướng dẫn đi dây

### 1. Nút nhấn thường (6 nút digital)
Mỗi nút nối **1 chân giữa GPIO và GND** (không cần điện trở ngoài — firmware
dùng `INPUT_PULLUP`, tự kéo lên 5V nội bộ). Khi nhấn, chân về mức LOW.

```
GPIO (5/7/10/14/15/16) ──[nút nhấn]── GND
```

### 2. Hai nút Play/Cue Deck 1 (dùng chung 1 chân A6)
Để tiết kiệm chân, 2 nút này đọc qua **thang điện trở (resistor ladder)** trên
cùng một chân A6 — mỗi nút khi nhấn tạo ra một mức điện áp khác nhau:

```
5V ──10KΩ── A6 ──10KΩ── [nút PLAY] ── GND
                  └──10KΩ── [nút CUE] ── GND   (nối tiếp sau nút PLAY)
```

Vùng ADC đọc được: PLAY khi giá trị nằm trong khoảng 450–580, CUE khi trong
khoảng 630–750 (đã đo thực tế trên mạch, có thể lệch chút theo dung sai điện trở —
nếu 2 nút không nhận đúng, dùng `Serial.println(analogRead(A6))` để đo lại 2
mốc và chỉnh `analogBtnMin[]/analogBtnMax[]` trong firmware).

### 3. Biến trở tuyến tính (EQ Low, FX/Filter — 4 cái, chân A0–A3)
Đấu chuẩn 3 chân potentiometer:

```
5V ── chân 1 pot
GND ── chân 3 pot
A0/A1/A2/A3 ── chân giữa (wiper) pot
```

### 4. Biến trở có lò xo về giữa (Tempo x2, Crossfader — chân A7/A8/A9)
Đấu dây giống hệt mục 3 (5V – wiper vào chân analog – GND), khác biệt duy nhất
nằm ở **firmware**: vì tâm điện học của mỗi pot lệch nhau do dung sai linh
kiện, code map riêng theo 2 đoạn quanh điểm giữa đã đo thực tế thay vì giả
định tâm luôn là 512 — xem phần [Hiệu chỉnh](#hiệu-chỉnh-calibration).

### 5. Encoder jog wheel (2 cái)
Mỗi encoder có tối thiểu 3 chân: A, B, COM (GND chung).

```
Encoder 1 (Deck 1):  COM → GND,  A → chân 0 (RXI),  B → chân 1 (TXO)
Encoder 2 (Deck 2):  COM → GND,  A → chân 3,          B → chân 2
```

Cả 4 chân A/B đều dùng `INPUT_PULLUP` — không cần điện trở ngoài.

## Nạp firmware

1. Cài Arduino IDE + board "SparkFun Pro Micro" (hoặc board tương thích
   ATmega32U4) qua Boards Manager.
2. Cài thư viện **MIDIUSB** (Library Manager → tìm "MIDIUSB").
3. Mở `firmware_optimized.ino`, chọn đúng board + cổng COM, Upload.

## Hiệu chỉnh (calibration)

Ba pot có lò xo về giữa (A7/A8/A9) cần đo lại 3 mốc ADC (hết trái – giữa – hết
phải) rồi điền vào firmware, vì tâm điện học thực tế không phải lúc nào cũng
là 512:

1. Nạp tạm `calibrate_center_pots.ino`, mở Serial Monitor (9600 baud).
2. Gạt từng pot về hết trái / đúng vị trí giữa / hết phải, ghi lại 3 số.
3. Điền vào `firmware_optimized.ino`:

```cpp
const int centeredRawMin[3]    = {0,   273,  0};     // hết trái: A7, A8, A9
const int centeredRawCenter[3] = {869, 888,  893};   // giữa:    A7, A8, A9
const int centeredRawMax[3]    = {1023, 995, 1023};  // hết phải: A7, A8, A9
```

4. Nạp lại firmware chính. Hàm `mapCenteredPot()` map riêng 2 đoạn (trái→giữa
   = 0→64, giữa→phải = 64→127) nên vị trí giữa vật lý luôn ra đúng MIDI 64,
   bất kể 2 nửa hành trình lệch nhau bao nhiêu.

## Cài mapping Mixxx

1. Copy `DJC-DIY.midi.xml` và `DJC-DIY-scripts.js` vào thư mục controllers
   của Mixxx (`Preferences → Controllers → Open user mapping folder`).
2. Mở Mixxx → Preferences → Controllers → chọn thiết bị Pro Micro → chọn
   mapping **DJC-DIY**.

---

Dựa trên thiết kế mã nguồn mở DJC-DIY gốc của MandićLab.
