# ANPR System (ESP32-CAM + Backend + Frontend)

Bu proje, araç algılama ve plaka tanıma için uçtan uca bir ANPR sistemidir:

- **ESP32-CAM firmware**: sensör tetiklemesi, fotoğraf çekimi, şifreli payload gönderimi, röle kontrolü
- **Backend (C++)**: payload çözme, OCR entegrasyonu, yetki kararı, log + watchlist yönetimi
- **Frontend (React)**: log/watchlist ekranları, filtreleme, görsel görüntüleme

## 1) Proje yapısı

```text
backend/           C++ API + SQLite
frontend/          React + Vite arayüz
esp32-firmware/    ESP32-CAM kodu
sim.py             Root seviyesinde hızlı simülatör çalıştırıcı
database/          (boş olabilir, aktif DB burada tutulmuyor)
```

> Not: Çalışan veritabanı dosyası **`backend/anpr.db`** altında oluşturulur.

---

## 2) Gereksinimler

### Backend
- CMake (>= 3.16)
- C++17 derleyici
- OpenSSL
- libcurl
- SQLite3

macOS (Homebrew):

```bash
brew install cmake openssl curl sqlite3
```

### Frontend
- Node.js 18+ önerilir
- npm

### Simülatör (Python)
- Python 3.9+
- `cryptography`
- `pillow` (opsiyonel ama önerilir)

```bash
python3 -m pip install cryptography pillow
```

---

## 3) İlk kurulum

Repo kökünde:

```bash
cd /Users/meysasu/anpr-system
```

### 3.1 Backend ortam dosyası

```bash
cp backend/.env.example backend/.env
```

`backend/.env` içinde en az şunları düzenleyin:

- `OCR_API_TOKEN`
- `ADMIN_API_TOKEN` (frontend ile aynı olmalı)
- (opsiyonel) SMTP ayarları

### 3.2 Frontend ortam dosyası

```bash
cp frontend/.env.example frontend/.env
```

`frontend/.env`:

- `VITE_ADMIN_API_TOKEN` değerini backend’deki `ADMIN_API_TOKEN` ile aynı yapın.

---

## 4) Çalıştırma

## 4.1 Backend build

```bash
cd backend
mkdir -p build
cd build
cmake ..
cmake --build . -j4
```

## 4.2 Backend run

```bash
cd /Users/meysasu/anpr-system/backend
bash start.sh
```

Backend:
- HTTP: `http://localhost:8000`
- HTTPS (self-signed): `https://localhost:8443`

## 4.3 Frontend run

Yeni terminal:

```bash
cd /Users/meysasu/anpr-system/frontend
npm install
npm run dev
```

Frontend URL: `http://localhost:5173`

---

## 5) Hızlı test (simülatör)

Backend açıkken repo kökünde:

```bash
cd /Users/meysasu/anpr-system
python3 sim.py 34UF2409
```

Beklenen: terminalde `HTTP 200` ve plaka sonucu.

---

## 6) ESP32 firmware notları

`esp32-firmware/src/config.h` dosyasında:

- `WIFI_SSID`, `WIFI_PASSWORD`
- `SERVER_HOST` (backend çalışan makinenin LAN IP’si)
- `SERVER_USE_TLS`:
  - `0` → HTTP
  - `1` → HTTPS (`SERVER_TLS_PORT`)

Manuel buton özelliği:
- Kısa basış: manuel bariyer aç/kapat
- Uzun basış (`RESET_HOLD_MS`): cihaz restart

---

## 7) Veritabanı kullanımı

`anpr.db` dosyasını metin editörüyle açmayın (binary dosyadır).  
Sorgu için:

```bash
sqlite3 backend/anpr.db ".tables"
sqlite3 -header -column backend/anpr.db "SELECT * FROM Access_Logs ORDER BY LogID DESC LIMIT 10;"
sqlite3 -header -column backend/anpr.db "SELECT * FROM Watchlist;"
```

---

## 8) Sık görülen sorunlar

### 8.1 “Goruntule” linki açılmıyor
- Backend’in çalıştığını kontrol edin (`/api/health`)
- Frontend ile backend token eşleşmesini kontrol edin:
  - `backend/.env` → `ADMIN_API_TOKEN`
  - `frontend/.env` → `VITE_ADMIN_API_TOKEN`

### 8.2 OCR sonucu sürekli `UNKNOWN`
- `OCR_API_TOKEN` doğru mu kontrol edin.
- Ağdan `https://api.platerecognizer.com` erişimi var mı kontrol edin.
- Gerekirse farklı ağ/hotspot ile test edin.

### 8.3 `database/` klasörü boş görünüyor
- Normal. Aktif DB dosyası `backend/anpr.db` altında.
