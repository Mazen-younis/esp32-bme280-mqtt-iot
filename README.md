# ESP32 Environmental Sensor Monitoring with MQTT

**ESP32-based environmental telemetry using a BME280 sensor, I²C, Wi-Fi, MQTT and ESP-IDF.**

**Authors:** Mazen Younis · Mert Satici  
**Institution:** Berliner Hochschule für Technik (BHT) · Computer Engineering  
**Course:** Internet of Things  
**Year:** 2026

## Overview

This university project demonstrates a sensor-to-network telemetry pipeline. An **ESP32-WROVER-DEV** acquires environmental measurements from a **BME280** over I²C, connects to a Wi-Fi network and publishes temperature, relative humidity and atmospheric-pressure readings as **JSON messages over MQTT**.

The work combines embedded C programming, sensor interfacing, network connectivity and message-based communication. The original experiments were completed and tested on ESP32 hardware in 2026.

> **Publication scope:** The source published here is a reviewed adaptation of the original laboratory implementation. It removes hard-coded network credentials and adds error handling. This revised source has **not** been independently rebuilt or retested on the ESP32. The figures and report document the **original experiments**, not a new validation of the revised code.

## System at a glance

```text
BME280 environmental sensor
       │
       │  I²C (SDA: GPIO21, SCL: GPIO22)
       ▼
ESP32-WROVER-DEV
       │
       │  Read sensor measurements
       │  Format JSON telemetry
       │  Connect through Wi-Fi
       ▼
MQTT broker
       │
       └── Publish temperature, humidity and pressure
```

### Hardware

| Component | Purpose |
|---|---|
| ESP32-WROVER-DEV | Microcontroller, Wi-Fi client and MQTT publisher |
| BME280 | Temperature, humidity and atmospheric-pressure sensing |
| Breadboard and jumper wires | Sensor connections |
| Serial monitor | Runtime diagnostics |

**Wiring**

| BME280 | ESP32 |
|---|---|
| VIN | 3.3 V |
| GND | GND |
| SDA | GPIO21 |
| SCL | GPIO22 |

![ESP32 and BME280 hardware setup](results/esp32_bme280_hardware.jpg)

## Implementation

### 1. Sensor acquisition

The ESP32 configures an I²C bus and initialises the BME280 sensor driver. The application reads temperature, relative humidity and pressure at regular intervals.

The original laboratory investigation detected the sensor at **I²C address `0x76`** and recorded plausible measurement values.

[View I²C detection](results/i2c_sensor_detection.png) · [View sensor readings](results/bme280_sensor_readings.png)

### 2. Wi-Fi connectivity

The application operates as a Wi-Fi station. ESP-IDF event handlers manage connection events and set or clear a FreeRTOS event-group bit reflecting connectivity.

The public source does **not** include a real network name or password. Local values are entered through ESP-IDF's `menuconfig` interface and stored in a local, ignored `sdkconfig` file.

### 3. MQTT telemetry

After establishing network connectivity, the ESP32 starts an MQTT client. Measurement values are formatted as JSON and submitted to the configured MQTT topic.

A representative message has the following structure:

```json
{
  "temperature": 24.33,
  "humidity": 36.53,
  "pressure": 1011.09
}
```

The revised source checks connection state and the MQTT publish return value. A positive message ID indicates that the publication request was accepted or queued by the client; **it is not, by itself, proof of delivery to a subscriber**.

![Original MQTT publication output](results/mqtt_sensor_publication.png)

## Original experimental evidence

The submitted laboratory report and selected screenshots document:

- Detection of the BME280 on the I²C bus at `0x76`.
- Sensor initialisation and acquisition of temperature, humidity and pressure.
- Wi-Fi connection and MQTT broker communication.
- Repeated submission of JSON-formatted telemetry messages.

See the [experimental results gallery](results/README.md) and the [redacted academic report](docs/report/IoT_Labor2_Public_Redacted.pdf).

> **Measurement note:** One original sensor-test screenshot labels pressure as `Pa` while showing numerical values around `1011`, consistent with `hPa`. The original evidence is preserved, and this unit-label discrepancy is noted in the results documentation.

## Software and dependencies

- **Language:** C
- **Development framework:** Espressif ESP-IDF
- **Runtime facilities:** FreeRTOS tasks and event groups
- **Interfaces:** I²C, Wi-Fi, MQTT
- **Libraries/components:** ESP-MQTT, BME280 sensor driver, I²C bus component
- **Build system:** CMake and ESP-IDF Component Manager

The dependency manifest in [`main/idf_component.yml`](main/idf_component.yml) records the versions identified from the original project. The university environment used **ESP-IDF 6.0.1**.

## Configuration and building

These commands describe the **intended ESP-IDF workflow**. They are provided as guidance, **not as a verified reproduction procedure for the revised code**.

With a suitable ESP-IDF installation and connected ESP32:

```bash
idf.py set-target esp32
idf.py menuconfig
idf.py build
idf.py -p <SERIAL_PORT> flash monitor
```

Under **IoT Demo Configuration** in `menuconfig`, set:

- Wi-Fi SSID and password
- MQTT broker URI
- MQTT publication topic

**Do not commit `sdkconfig` or any credentials.** The repository's `.gitignore` excludes generated local configuration and build outputs.

**Compatibility warning:** The original project used ESP-IDF 6.0.1 and external I²C/BME280 components. The reviewed source and dependency declarations have **not** been compiled together here. The external sensor-driver API and compatibility with the selected ESP-IDF release require verification before these build commands can be treated as working instructions.

## Repository structure

```text
esp32-bme280-mqtt-iot/
├── README.md
├── CMakeLists.txt
├── .gitignore
├── main/
│   ├── main.c
│   ├── CMakeLists.txt
│   ├── Kconfig.projbuild
│   └── idf_component.yml
├── docs/
│   └── report/
│       ├── README.md
│       └── IoT_Labor2_Public_Redacted.pdf
└── results/
    ├── README.md
    ├── esp32_bme280_hardware.jpg
    ├── i2c_sensor_detection.png
    ├── bme280_sensor_readings.png
    └── mqtt_sensor_publication.png
```

## Key engineering takeaways

- Combining **I²C sensor acquisition** with a **network telemetry pipeline** requires coordination between peripheral drivers and connectivity state.
- MQTT decouples the device publishing measurements from downstream subscribers.
- Connection state and error handling matter: successfully formatting a message is not the same as successfully transmitting or delivering it.
- Hardware evidence, documentation and published source should be distinguished when the code has been modified after the original experiments.

## Limitations and public repository scope

This project is an **educational IoT prototype**, not a production deployment.

- The original experiments used a laboratory MQTT setup; the public source is not presented as a secure end-to-end production telemetry system.
- For deployment beyond a controlled lab, use authenticated MQTT over TLS and suitable credential management.
- The revised code has not undergone a fresh ESP-IDF build, integration test or hardware retest.
- Published console screenshots document the original experiments, not the current revised source.
- The report and selected evidence are redacted to avoid exposing student identification numbers and private network information.

## Attribution

Completed collaboratively by **Mazen Younis and Mert Satici** as part of the **Internet of Things** course at **Berliner Hochschule für Technik (BHT)** in 2026.

The original laboratory files were selectively reviewed for public portfolio presentation; generated build artifacts, downloaded third-party dependencies and private credentials are not included.
