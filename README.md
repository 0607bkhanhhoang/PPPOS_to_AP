# PPPOS_to_AP
This is Ap to PPPOS

# 📡 ESP32 4G Wi-Fi Hotspot (SoftAP + PPPoS + NAT)

This project turns your **ESP32-S3** and **4G LTE module (e.g., A7682, SIM7600, EC800K)** into a Wi-Fi hotspot that shares the mobile internet via NAT and NAPT. It randomly generates:
- SSID & Password
- MAC address
- IP address (in 192.168.X.1 range)

🔧 Built using **ESP-IDF**, `esp_modem`, and `lwIP NAT/NAPT`.

---

## 🚀 Features

- 📶 ESP32 SoftAP with dynamic SSID & password
- 🌐 Internet access via 4G (PPP over UART)
- 🔁 NAT/NAPT routing (IP + Port translation)
- 🧠 Random MAC & IP for SoftAP (privacy/randomization)
- 🧼 No external dependencies – pure ESP-IDF

---

## 🛠️ Hardware Requirements

| Component        | Example            |
|------------------|--------------------|
| ESP32 Board      | ESP32-S3, ESP32-WROOM |
| 4G Module        | A7682, EC800K, SIM7600 |
| UART Interface   | TX/RX connected from ESP32 ↔ 4G module |
| SIM Card         | With working mobile data |

---

## 🔌 UART Wiring Example

| ESP32-S3 GPIO | 4G Module Pin |
|---------------|---------------|
| GPIO17 (TXD)  | RX            |
| GPIO18 (RXD)  | TX            |
| GND           | GND           |
| (Optional) RTS/CTS if HW Flow Control is used |

Edit your pin config in `gw_modem.c`:

```c
#define MODEM_UART_PORT  UART_NUM_1
#define MODEM_TX_PIN     GPIO17
#define MODEM_RX_PIN     GPIO18

