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
- `SMTP_USER` + `SMTP_PASS` (veya `SMTP_API_TOKEN`)
- `FCM_PROJECT_ID`
- `FCM_SERVICE_ACCOUNT_FILE` (service account JSON tam dosya yolu)

### 3.2 Frontend ortam dosyası

```bash
cp frontend/.env.example frontend/.env
```

`frontend/.env`:

- `VITE_ADMIN_API_TOKEN` değerini backend’deki `ADMIN_API_TOKEN` ile aynı yapın.
- Firebase Web Push alanlarini doldurun:
  - `VITE_FIREBASE_API_KEY`
  - `VITE_FIREBASE_AUTH_DOMAIN`
  - `VITE_FIREBASE_PROJECT_ID`
  - `VITE_FIREBASE_MESSAGING_SENDER_ID`
  - `VITE_FIREBASE_APP_ID`
  - `VITE_FIREBASE_VAPID_KEY`

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

## 4.4 Tek komutla backend + frontend + ngrok

Repo kokunde:

```bash
./start.sh
```

Bu komut:
- backend'i (`:8000`) baslatir
- frontend'i (`:5173`) baslatir
- ngrok kuruluysa `:5173` icin HTTPS tunnel acmaya calisir

> ngrok ilk kurulumdan once token tanimlamaniz gerekir:
> `ngrok config add-authtoken <TOKEN>`

---

## 5) Hızlı test (simülatör)

Backend açıkken repo kökünde:

```bash
cd /Users/meysasu/anpr-system
python3 sim.py 34UF2409
```

Beklenen: terminalde `HTTP 200` ve plaka sonucu.

Eger watchlist kaydinda araca ait `owner_email` tanimliysa ve gecis onayi verildiyse (bariyer acildiysa),
sistem ilgili adrese otomatik e-posta bildirimi gonderir.

Mobil push icin dashboard u telefonda acip sol alttaki **Mobil Bildirimleri Ac** butonuna basin.
iOS tarafinda bildirim almak icin Safari ile uygulamayi **Ana Ekrana Ekle** yapip oradan acin.

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

### 8.3 Bariyer aciliyor ama e-posta gitmiyor
- `backend/.env` dosyasında SMTP ayarlarini kontrol edin:
  - `SMTP_USER`
  - `SMTP_PASS` (veya `SMTP_API_TOKEN`)
- Watchlist kaydinda `owner_email` alani dolu mu kontrol edin.
- Arac kaydinda `auto_grant=true` degilse bariyer acilmaz ve e-posta tetiklenmez.

### 8.4 `database/` klasörü boş görünüyor
- Normal. Aktif DB dosyası `backend/anpr.db` altında.

### 8.5 Mobil push bildirimi gelmiyor
- `backend/.env` icinde `FCM_PROJECT_ID` ve `FCM_SERVICE_ACCOUNT_FILE` dolu mu kontrol edin.
- `frontend/.env` icindeki Firebase + VAPID degerleri tam mi kontrol edin.
- iOS icin: Safari > Ana Ekrana Ekle > uygulamayi ana ekrandan ac > izin ver.
- Push token kaydi icin frontend ve backend `ADMIN_API_TOKEN` eslesmeli.
