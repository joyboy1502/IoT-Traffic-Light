🚦 IoT Traffic Light – ESP32
Simulasi lampu lalu lintas 4 arah (Utara, Selatan, Timur, Barat) menggunakan ESP32, dengan fitur monitoring dan update jarak jauh (OTA) via MQTT Cloud.
---
📁 Struktur File
File	Tugas	Deskripsi
`2_TrafficLight.ino`	Tugas 2	Traffic light dasar, berjalan otomatis tanpa WiFi
`3_TrafficLight.ino`	Tugas 3	Tambah WiFi + dashboard web lokal + OTA update
`4_TrafficLight.ino`	Tugas 4	Tambah MQTT Cloud (HiveMQ) → kontrol dari mana saja
`4_Trafficlight_OTA_Dashboard.html`	Tugas 4	Dashboard web untuk monitoring & kontrol via browser
---
⚙️ Cara Kerja
Sistem menggunakan 4 fase yang berputar secara otomatis:
```
Fase 1 → Utara & Selatan: HIJAU  | Timur & Barat: MERAH
Fase 2 → Utara & Selatan: KUNING | Timur & Barat: MERAH
Fase 3 → Utara & Selatan: MERAH  | Timur & Barat: HIJAU
Fase 4 → Utara & Selatan: MERAH  | Timur & Barat: KUNING
```
---
🔌 Pin Mapping (ESP32)
Arah	Merah	Kuning	Hijau
Utara	GPIO 25	GPIO 26	GPIO 27
Selatan	GPIO 14	GPIO 12	GPIO 13
Timur	GPIO 33	GPIO 23	GPIO 22
Barat	GPIO 18	GPIO 19	GPIO 21
---
🌐 Arsitektur Sistem (Tugas 4)
```
ESP32 ──── MQTT/TLS ────► HiveMQ Cloud ◄──── MQTT/WSS ──── Dashboard Browser
(WiFi Lokal)                                               (Jaringan mana saja)
```
Keunggulan: ESP32 dan dashboard tidak perlu satu jaringan WiFi yang sama.  
Kontrol bisa dilakukan dari kantor, rumah, atau via data seluler.
---
🛠️ Library yang Dibutuhkan
Install via Arduino IDE → Library Manager:
`PubSubClient` by Nick O'Leary
`ArduinoJson` by Benoit Blanchon
Board: ESP32 Dev Module
---
⏱️ Durasi Default Lampu (Tugas 4)
Warna	Durasi Awal	Setelah OTA Update
Merah	4 detik	1 detik
Kuning	2 detik	1 detik
Hijau	4 detik	1 detik
Durasi dapat diubah secara real-time dari dashboard tanpa upload ulang firmware.
---
🚀 Cara Pakai
Buka file `.ino` sesuai tugas di Arduino IDE
Ganti kredensial WiFi dan MQTT (jika pakai Tugas 4)
Upload ke ESP32
Buka Serial Monitor (115200 baud) untuk melihat log
Akses dashboard di `http://trafficlight.local` (Tugas 3) atau buka file HTML (Tugas 4)
---
📡 MQTT Topics (Tugas 4)
Topic	Arah	Isi
`trafficlight/status`	ESP32 → Dashboard	Status fase & durasi (JSON)
`trafficlight/setdur`	Dashboard → ESP32	Update durasi lampu (JSON)
`trafficlight/log`	ESP32 → Dashboard	Log aktivitas (string)
`trafficlight/cmd`	Dashboard → ESP32	Perintah kontrol
---
📺 Demo Video
▶️ Klik untuk menonton
---
Proyek mata kuliah Internet of Things — PT Floatway Systems
