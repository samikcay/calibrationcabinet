# Calibration Cabinet Control System

Bu proje, STM32F407VGTX mikrodenetleyicisi üzerinde koşan, hassas sıcaklık ve nem kontrolü sağlayan bir kalibrasyon kabini otomasyon sistemidir. Sistem, **FreeRTOS** (CMSIS-RTOS V2) tabanlı çok görevli (multitasking) bir yapıda ve **State Machine** mimarisiyle tasarlanmıştır.

## 🚀 Öne Çıkan Özellikler

*   **Hassas PID Kontrol:** Sıcaklık ve nem değerleri için optimize edilmiş PID algoritmaları ile kararlı çalışma.
*   **Gelişmiş Durum Makinesi:** `INIT`, `IDLE`, `RUNNING` ve `STANDBY` modları arasında otomatik ve güvenli geçişler.
*   **Stabilite Takibi:** Sıcaklık ±0.5°C, Nem ±%3 RH bandında kaldığında sistemi otomatik olarak "Standby" (Kararlı) moduna geçirir.
*   **Zengin Kullanıcı Arayüzü:** I2C üzerinden 20x4 veya 16x2 LCD ekran desteği ve matrix keypad üzerinden veri girişi.
*   **FreeRTOS Task Yönetimi:** 
    - `supervisorTask`: Sistem durumlarını ve hata kontrolünü yönetir.
    - `controlTask`: Isıtıcı/Nemlendirici için PID hesaplamalarını yapar.
    - `sensorTask`: DHT22/AM2302 sensörlerinden veri okur.
    - `uiTask`: LCD ekranı günceller ve kullanıcı girişlerini işler.
    - `commTask`: Harici haberleşme protokollerini yönetir.

## 🛠️ Donanım Gereksinimleri

*   **MCU:** STM32F407VGT6 (Discovery Kit veya Özel Kart)
*   **Sensör:** DHT22 / AM2302 (Dijital Sıcaklık ve Nem)
*   **Ekran:** I2C LCD (PCF8574 sürücülü)
*   **Giriş:** 4x4 veya 3x4 Matrix Keypad
*   **Aktüatörler:** PWM veya röle kontrollü ısıtıcı ve nemlendirici üniteleri

## 💻 Yazılım Yığını

*   **IDE:** STM32CubeIDE
*   **Framework:** STM32Cube HAL
*   **RTOS:** FreeRTOS
*   **Dil:** C

## 📂 Proje Yapısı

```text
Core/
├── Inc/                # Başlık dosyaları (.h)
│   ├── state_machine.h # Sistem durum tanımları
│   ├── thermal.h       # Sıcaklık kontrol mantığı
│   ├── humidity.h      # Nem kontrol mantığı
│   └── ...
└── Src/                # Kaynak dosyalar (.c)
    ├── main.c          # RTOS başlatma ve donanım init
    ├── state_machine.c # Transition (geçiş) mantığı
    └── ...
```

## ⚙️ Kurulum ve Derleme

1.  Bu depoyu klonlayın: `git clone https://github.com/kullanici_adi/calibrationcabinet.git`
2.  **STM32CubeIDE** programını açın.
3.  `File -> Import -> Existing Projects into Workspace` seçeneği ile bu klasörü seçin.
4.  `calibrationcabinet.ioc` dosyasını açarak pin konfigürasyonlarını gözden geçirin.
5.  Projeyi derleyin ve mikrodenetleyicinize yükleyin.

## 📄 Lisans
Bu proje [STMicroelectronics](https://www.st.com/) tarafından sağlanan HAL sürücüleri ve standart yazılım lisansları kapsamındadır.
