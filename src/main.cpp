// Copyright (C) 2024, cubic9com All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full
// license information.

#include <M5EPD.h>
#include <SensirionI2CScd4x.h>

const int PortA_SDA = M5EPD_PORTA_Y_PIN;
const int PortA_SCL = M5EPD_PORTA_W_PIN;
const uint16_t CO2_CONCENTRATION_IN_OUTSIDE = 420;
const int SLEEP_DURATION_IN_SEC = 120;

SensirionI2CScd4x scd4x;
M5EPD_Canvas canvas(&M5.EPD);

/**
 * Disable Automatic Self-Calibration (ASC).
 * Note: ASC ensures the highest long-term stability of the SCD4x
 *       without the need of manual action steps from the user.
 *       But for power-cycled single shot operation, ASC is not available.
 *
 * Automatic Self-Calibration（ASC）を無効にする。
 * 注：ASCは、ユーザーが手動で操作することなく、SCD4xの最高の長期安定性を保証するが、
 *    電力サイクルによるシングルショット動作では、ASCは使用できない。
 */
void disable_asc() {
    uint16_t error;
    char errorMessage[256];

    error = scd4x.setAutomaticSelfCalibration(0);
    if (error) {
        Serial.print("Error trying to execute setAutomaticSelfCalibration(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    }
}

/**
 * Perform Forced recalibration (FRC).
 * Note: FRC immediately restores high accuracy with the assistance of
 *       an external CO2 reference value.
 *
 * Forced recalibration（FRC）を実行する。
 * 注：FRCは、外部CO2基準値を用いて高精度を即座に回復する。
 */
void perform_frc() {
    uint16_t error;
    char errorMessage[256];
    uint16_t frc_correction;

    M5.EPD.Clear(true);
    canvas.clear();
    canvas.setTextSize(2);

    Serial.println("Performing Forced Recalibration (FRC).");
    canvas.drawString("Performing Forced Recalibration (FRC).", 0, 0, 4);
    Serial.println("Waiting 3 minutes for equilibration.");
    canvas.drawString("Waiting 3 minutes for equilibration.", 0, 60, 4);
    canvas.pushCanvas(0, 0, UPDATE_MODE_DU4);

    // Stop periodic measurement
    error = scd4x.stopPeriodicMeasurement();
    if (error) {
        Serial.print("Error trying to execute stopPeriodicMeasurement(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    }
    delay(500);

    // Perform a factory reset
    error = scd4x.performFactoryReset();
    if (error) {
        Serial.print("Error trying to execute performFactoryReset(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    }
    delay(1200);

    // Start periodic measurement for equilibration.
    error = scd4x.startPeriodicMeasurement();
    if (error) {
        Serial.print("Error trying to execute startPeriodicMeasurement(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    }
    for (int i = 1; i < 4; i++) {
        for (int j = 0; j < 6; j++) {
            delay(10 * 1000);
            Serial.print(".");
            canvas.drawString(".", 20 * j, 60 + 60 * i, 4);
            canvas.pushCanvas(0, 0, UPDATE_MODE_DU4);
        }
        Serial.println(".");
        Serial.println(String(i) + " minute(s) has passed.");
        canvas.drawString(String(i) + " minute(s) has passed.", 0, 60 + 60 * i, 4);
        canvas.pushCanvas(0, 0, UPDATE_MODE_DU4);
    }
    // Stop periodic measurement for equilibration.
    error = scd4x.stopPeriodicMeasurement();
    if (error) {
        Serial.print("Error trying to execute stopPeriodicMeasurement(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    }
    delay(500);

    // Perform Forced Recalibration (FRC).
    error = scd4x.performForcedRecalibration(CO2_CONCENTRATION_IN_OUTSIDE, frc_correction);
    if (error) {
        Serial.print("Error trying to execute performForcedRecalibration(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    }
    delay(400);

    Serial.println("Forced Recalibration has finised.");
    canvas.drawString("Forced Recalibration has finised.", 0, 300, 4);
    canvas.pushCanvas(0, 0, UPDATE_MODE_DU4);
}

/**
 * Measure CO2 concentration, humidity and temperature on demand.
 *
 * CO2濃度、湿度、温度をオンデマンドで測定する。
 */
uint16_t measure_co2() {
    uint16_t error;
    char errorMessage[256];

    // Measure CO2 concentration, humidity and temperature on demand.
    error = scd4x.measureSingleShot();
    if (error) {
        Serial.print("Error trying to execute measureSingleShot(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    }

    // Check whether new measurement data is available for read-out.
    bool isDataReady = false;
    error = scd4x.getDataReadyFlag(isDataReady);
    if (error) {
        Serial.print("Error trying to execute getDataReadyFlag(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
        return -1;
    }
    if (!isDataReady) {
        return -1;
    }

    // read sensor output.
    uint16_t co2 = 0;
    float temperature = 0.0f;
    float humidity = 0.0f;
    error = scd4x.readMeasurement(co2, temperature, humidity);
    if (error) {
        Serial.print("Error trying to execute readMeasurement(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    } else if (co2 == 0) {
        Serial.println("Invalid sample detected, skipping.");
    } else {
        Serial.print("CO2:");
        Serial.print(co2);
        Serial.print("\t");
        Serial.print("Temperature:");
        Serial.print(temperature);
        Serial.print("\t");
        Serial.print("Humidity:");
        Serial.println(humidity);
    }

    return co2;
}

/**
 * Draw CO2 concentration to E-ink display.
 *
 * CO2濃度をE-inkディスプレイに描画する。
 */
void draw_co2(int value) {
    M5.EPD.Clear(true);
    canvas.clear();

    // Draw the word "CO2".
    canvas.setTextSize(3);
    canvas.drawString("CO", 30, 20, 4);
    canvas.setTextSize(1);
    canvas.drawString("2", 143, 55, 6);
    canvas.fillRect(195, 45, 705, 7, 15);

    // Draw the word "ppm".
    canvas.setTextSize(3);
    canvas.drawString("ppm", 770, 450, 4);
    canvas.fillRect(60, 490, 690, 7, 15);

    // Draw CO2 concentration.
    canvas.setTextSize(4);
    canvas.drawRightString(String(value), 910, 120, 8);

    canvas.pushCanvas(0, 0, UPDATE_MODE_DU4);
    delay(300);
}

/**
 * Enter Light-Sleep mode.
 *
 * Light-Sleepモードに入る。
 */
void light_sleep() {
    // Turn off E-ink display.
    M5.disableEPDPower();

    // Start light sleep to wake up on RTC.
    esp_sleep_enable_timer_wakeup(SLEEP_DURATION_IN_SEC * 1000000ULL);
    esp_light_sleep_start();

    // Turn on E-ink display.
    M5.enableEPDPower();
}

void setup() {
    M5.begin(false, // bool touchEnable,
             false, // bool SDEnable
             true,  // bool SerialEnable
             false, // bool BatteryADCEnable
             false  // bool I2CEnable
    );

    M5.EPD.SetRotation(0);
    canvas.createCanvas(960, 540);

    // Begin RTC for light sleep.
    M5.RTC.begin();

    // Prepare communication with sensor.
    Wire1.begin(PortA_SDA, PortA_SCL);
    scd4x.begin(Wire1);

    // Disable Automatic self-calibration (ASC).
    disable_asc();

    // Detect scroll wheel presses.
    M5.update();
    delay(100);
    if (M5.BtnP.isPressed()) {
        // Perform Forced recalibration (FRC).
        perform_frc();
    }
}

void loop() {
    // Measure CO2 concentration, humidity and temperature on demand.
    uint16_t co2 = measure_co2();

    // Draw CO2 concentration to E-ink display.
    draw_co2(co2);

    // Enter Light-Sleep mode.
    light_sleep();
}