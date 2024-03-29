// Copyright 2022 BotanicFields, Inc.
// BF-037 VU meters on M5Stack

#include <M5Stack.h>
#include "BF_M5StackVuMeter.h"

// for control loop
const unsigned int loop_ms(50);  // 20fps
      unsigned int last_ms(0);

// for meters
const int offset(142);  // offset 142mV at ground-level on ESP32 ADC
const int vu_0(1227);   // 0VU = 1227mV

const int time_attack_vu(1000);    // 1000ms for attacking from -20VU to 0VU
const int time_attack_mc(3000);    // 3000ms for attacking from -60VU to 0dB
const int time_return_peak(8000);  // 8000ms for recovery from 0VU to -20VU

int vu_step   = vu_0 / (time_attack_vu   / loop_ms);
int peak_step = vu_0 / (time_return_peak / loop_ms);

int last_vu_left(0);
int last_vu_right(0);
int last_peak_left(0);
int last_peak_right(0);

enum meter_style_enum { vu_meter = 0, mc_meter };
meter_style_enum meter_style(vu_meter);

enum ch_select_enum { stereo = 0, left_ch, right_ch };
ch_select_enum ch_select(stereo);

bool double_needle(true);

void setup()
{
  const bool lcd_enable(true);
  const bool sd_enable(true);
  const bool serial_enable(true);
  const bool i2c_enable(true);
  M5.begin(!lcd_enable, !sd_enable, serial_enable, i2c_enable);

  // analog to digital converter
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // lcd & meter
  lcd_init();
  switch (meter_style) {
  case mc_meter:
    make_panel_mc();
    vu_step = vu_0 / (time_attack_mc / loop_ms);
    double_needle = false;
    break;
  case vu_meter:
  default:
    make_panel_vu();
    vu_step = vu_0 / (time_attack_vu / loop_ms);
    double_needle = true;
    meter_style = vu_meter;
    break;
  }

  // loop control
  last_ms = millis();
}

void loop()
{
  // button control
  M5.update();
  if (M5.BtnA.wasReleased()) {
    switch (meter_style) {
    case vu_meter:
      meter_style = mc_meter;
      make_panel_mc();
      vu_step = vu_0 / (time_attack_mc / loop_ms);
      double_needle = false;
      break;
    case mc_meter:
    default:
      meter_style = vu_meter;
      make_panel_vu();
      vu_step = vu_0 / (time_attack_vu / loop_ms);
      double_needle = true;
      break;
    }
    last_ms = millis();  // avoid loop hangs
  }
  if (M5.BtnB.wasReleased()) {
    switch (ch_select) {
    case stereo:
      ch_select = left_ch;
      break;
    case left_ch:
      ch_select = right_ch;
      break;
    case right_ch:
    default:
      ch_select = stereo;
      break;
    }
    lcd_clear();
    last_ms = millis();  // avoid loop hangs
  }
  if (M5.BtnC.wasReleased()) {
    double_needle = !double_needle;
  }

  // read ADC
  int adc_7 = analogReadMilliVolts(35);
  int adc_0 = analogReadMilliVolts(36);

  // compensation of ESP32 ADC
  adc_7 = (adc_7 - offset) * vu_0 / (vu_0 - offset);
  adc_0 = (adc_0 - offset) * vu_0 / (vu_0 - offset);

  // response
  if (adc_7 > last_vu_left    + vu_step)   last_vu_left    += vu_step;   else last_vu_left    = adc_7;
  if (adc_0 > last_vu_right   + vu_step)   last_vu_right   += vu_step;   else last_vu_right   = adc_0;
  if (adc_7 < last_peak_left  - peak_step) last_peak_left  -= peak_step; else last_peak_left  = adc_7;
  if (adc_0 < last_peak_right - peak_step) last_peak_right -= peak_step; else last_peak_right = adc_0;

  // draw meter on LCD
  if (double_needle)
    switch (ch_select) {
    case left_ch:
      show_meter(0,   0, "L", last_vu_left, last_peak_left);
      break;
    case right_ch:
      show_meter(0,   0, "R", last_vu_right, last_peak_right);
      break;
    case stereo:
    default:
      show_meter(0,   0, "L", last_vu_left, last_peak_left);
      show_meter(0, 120, "R", last_vu_right, last_peak_right);
      break;
    }
  else
    switch (ch_select) {
    case left_ch:
      show_meter(0,   0, "L", last_vu_left);
      break;
    case right_ch:
      show_meter(0,   0, "R", last_vu_right);
      break;
    case stereo:
    default:
      show_meter(0,   0, "L", last_vu_left);
      show_meter(0, 120, "R", last_vu_right);
      break;
    }

  // loop control
  Serial.printf("elapse=%dms adc7=%d adc0=%d\n", millis() - last_ms, adc_7, adc_0);  // elapsed time
  delay(last_ms + loop_ms - millis());
  last_ms = millis();
}
