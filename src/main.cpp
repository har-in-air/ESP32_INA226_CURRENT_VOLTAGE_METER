#include <Arduino.h>
#include <Wire.h>  
#include <FS.h>
#include <LittleFS.h>
#include "config.h"
#include "nv_data.h"
#include "wifi_cfg.h"
#include "tone.h"
#include "ina226.h"

const char* FwRevision = "0.90";

void setup() {
	pinMode(pinAlert, INPUT); // external pullup, active low
	pinMode(pinGate, INPUT); // external pullup, active low
	pinMode(pinBtn1, INPUT_PULLUP);
	pinMode(pinBtn2, INPUT_PULLUP);
	pinMode(pinFET1, OUTPUT); // external pulldown
	pinMode(pinFET05, OUTPUT); // external pulldown

	Wire.begin(pinSDA,pinSCL); 
	Wire.setClock(2000000);

	Serial.begin(115200);
	Serial.println();
	Serial.printf("ESP32_INA226 v%s compiled on %s at %s\n\n", FwRevision, __DATE__, __TIME__);

	nv_options_load(Options);
	nv_config_load(ConfigTbl);
	
	uint16_t val;
	ina226_read_reg(REG_ID, &val);
	if (val != 0x5449) {
		Serial.printf("INA226 Manufacturer ID read = 0x%04X, expected 0x5449\n", val);
		Serial.println("Halting...");
		while (1){}
		}

	ina226_reset();

	//nv_options_reset(Options);
	//nv_config_reset(ConfigTbl);
   	
	Measure.scale = SCALE_LO;

	Serial.println("Measuring one-shot sample times");
	for (int inx = 0; inx < NUM_CFG; inx++) {
		Measure.cfg = ConfigTbl.cfg[inx].reg | 0x0003;
		ina226_capture_oneshot(Measure);
		}

	Serial.println("Measuring triggered sample-rates");
	for (int inx = 0; inx < NUM_CFG; inx++) {
		Measure.cfg = ConfigTbl.cfg[inx].reg | 0x0003;
		Measure.periodUs = ConfigTbl.cfg[inx].periodUs;
		Measure.nSamples = 2000;
		ina226_capture_triggered(Measure, Buffer);
		}

#if 0
	Serial.println("Calibrating continuous sample rates");
	ina226_calibrate_sample_rates();
	nv_config_store(ConfigTbl);
#endif

	Serial.println("Starting web server");
	if (!LittleFS.begin(true)){
		Serial.println("LittleFS mount error");
		return;
		}  
	wifi_init();
	}


void loop() {
	ws.cleanupClients();
	if (bCapture) {
		bCapture = false;
		Measure.cfg = ConfigTbl.cfg[0].reg | 0x0003;
		Measure.scale = SCALE_LO;
		Measure.nSamples = NumSamples;
		Measure.periodUs = ConfigTbl.cfg[0].periodUs;
		Serial.printf("Capturing %d samples\n", NumSamples );
		ina226_capture_triggered(Measure, Buffer);
		if (bConnected) { 
     		Serial.println("Transmitting samples");
			ws.binary(clientID, (uint8_t*)Buffer, NumSamples*4); // needs size in bytes
			}
		}
	}


