/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2013 Semtech-Cycleo

Description:
	LoRa concentrator Hardware Abstraction Layer

License: Revised BSD License, see LICENSE.TXT file include in the project
Maintainer: Sylvain Miermont
*/


/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include <stdint.h>		/* C99 types */
#include <stdbool.h>	/* bool type */
#include <stdio.h>		/* printf fprintf */
#include <string.h>		/* memcpy */

//#include "loragw_reg.h"
#include "loragw_hal.h"
#include "loragw_aux.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#if DEBUG_HAL == 1
	#define DEBUG_MSG(str)				fprintf(stderr, str)
	#define DEBUG_PRINTF(fmt, args...)	fprintf(stderr,"%s:%d: "fmt, __FUNCTION__, __LINE__, args)
	#define DEBUG_ARRAY(a,b,c)			for(a=0;a<b;++a) fprintf(stderr,"%x.",c[a]);fprintf(stderr,"end\n")
	#define CHECK_NULL(a)				if(a==NULL){fprintf(stderr,"%s:%d: ERROR: NULL POINTER AS ARGUMENT\n", __FUNCTION__, __LINE__);return LGW_HAL_ERROR;}
#else
	#define DEBUG_MSG(str)
	#define DEBUG_PRINTF(fmt, args...)
	#define DEBUG_ARRAY(a,b,c)			for(a=0;a!=0;){}
	#define CHECK_NULL(a)				if(a==NULL){return LGW_HAL_ERROR;}
#endif

#define IF_HZ_TO_REG(f)		(f << 5)/15625
#define	SET_PPM_ON(bw,dr)	(((bw == BW_125KHZ) && ((dr == DR_LORA_SF11) || (dr == DR_LORA_SF12))) || ((bw == BW_250KHZ) && (dr == DR_LORA_SF12)))
#define TRACE()				fprintf(stderr, "@ %s %d\n", __FUNCTION__, __LINE__);

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS & TYPES -------------------------------------------- */

#define		MCU_ARB		0
#define		MCU_AGC		1
#define		MCU_ARB_FW_BYTE		8192 /* size of the firmware IN BYTES (= twice the number of 14b words) */
#define		MCU_AGC_FW_BYTE		8192 /* size of the firmware IN BYTES (= twice the number of 14b words) */
#define		FW_VERSION_ADDR		0x20 /* Address of firmware version in data memory */
#define		FW_VERSION_CAL		2 /* Expected version of calibration firmware */
#define		FW_VERSION_AGC		4 /* Expected version of AGC firmware */
#define		FW_VERSION_ARB		1 /* Expected version of arbiter firmware */

#define		TX_METADATA_NB		16
#define		RX_METADATA_NB		16

#define		AGC_CMD_WAIT		16
#define		AGC_CMD_ABORT		17

#define		MIN_LORA_PREAMBLE		4
#define		STD_LORA_PREAMBLE		6
#define		MIN_FSK_PREAMBLE		3
#define		STD_FSK_PREAMBLE		5
#define		PLL_LOCK_MAX_ATTEMPTS	5

#define		TX_START_DELAY		1500

/*
SX1257 frequency setting :
F_register(24bit) = F_rf (Hz) / F_step(Hz)
                  = F_rf (Hz) * 2^19 / F_xtal(Hz)
                  = F_rf (Hz) * 2^19 / 32e6
                  = F_rf (Hz) * 256/15625

SX1255 frequency setting :
F_register(24bit) = F_rf (Hz) / F_step(Hz)
                  = F_rf (Hz) * 2^20 / F_xtal(Hz)
                  = F_rf (Hz) * 2^20 / 32e6
                  = F_rf (Hz) * 512/15625
*/
#define 	SX125x_32MHz_FRAC	15625	/* irreductible fraction for PLL register caculation */

#define		SX125x_TX_DAC_CLK_SEL	1	/* 0:int, 1:ext */
#define		SX125x_TX_DAC_GAIN		2	/* 3:0, 2:-3, 1:-6, 0:-9 dBFS (default 2) */
#define		SX125x_TX_MIX_GAIN		14	/* -38 + 2*TxMixGain dB (default 14) */
#define		SX125x_TX_PLL_BW		3	/* 0:75, 1:150, 2:225, 3:300 kHz (default 3) */
#define		SX125x_TX_ANA_BW		0	/* 17.5 / 2*(41-TxAnaBw) MHz (default 0) */
#define		SX125x_TX_DAC_BW		5	/* 24 + 8*TxDacBw Nb FIR taps (default 2) */
#define		SX125x_RX_LNA_GAIN		1	/* 1 to 6, 1 highest gain */
#define		SX125x_RX_BB_GAIN		12	/* 0 to 15 , 15 highest gain */
#define 	SX125x_LNA_ZIN			1	/* 0:50, 1:200 Ohms (default 1) */
#define		SX125x_RX_ADC_BW		7	/* 0 to 7, 2:100<BW<200, 5:200<BW<400,7:400<BW kHz SSB (default 7) */
#define		SX125x_RX_ADC_TRIM		6	/* 0 to 7, 6 for 32MHz ref, 5 for 36MHz ref */
#define 	SX125x_RX_BB_BW			0	/* 0:750, 1:500, 2:375; 3:250 kHz SSB (default 1, max 3) */
#define 	SX125x_RX_PLL_BW		0	/* 0:75, 1:150, 2:225, 3:300 kHz (default 3, max 3) */
#define 	SX125x_ADC_TEMP			0	/* ADC temperature measurement mode (default 0) */
#define 	SX125x_XOSC_GM_STARTUP	13	/* (default 13) */
#define 	SX125x_XOSC_DISABLE		2	/* Disable of Xtal Oscillator blocks bit0:regulator, bit1:core(gm), bit2:amplifier */

#define		RSSI_MULTI_BIAS			-35		/* difference between "multi" modem RSSI offset and "stand-alone" modem RSSI offset */
#define		RSSI_FSK_BIAS			-37.0	/* difference between FSK modem RSSI offset and "stand-alone" modem RSSI offset */
#define		RSSI_FSK_REF			-70.0	/* linearize FSK RSSI curve around -70 dBm */
#define		RSSI_FSK_SLOPE			0.8

/* constant arrays defining hardware capability */

const uint8_t ifmod_config[LGW_IF_CHAIN_NB] = LGW_IFMODEM_CONFIG;
const uint32_t rf_rx_bandwidth[LGW_RF_CHAIN_NB] = LGW_RF_RX_BANDWIDTH;

/* Strings for version (and options) identification */

//TODO: maak een speciale simulator version.

#if (CFG_SPI_NATIVE == 1)
	#define		CFG_SPI_STR		"native"
#elif (CFG_SPI_FTDI == 1)
	#define		CFG_SPI_STR		"ftdi"
#else
	#define		CFG_SPI_STR		"spi?"
#endif

/* Version string, used to identify the library version/options once compiled */
const char lgw_version_string[] = "Version: " LIBLORAGW_VERSION "; Options: " CFG_SPI_STR ";";


void lgw_constant_adjust(void) {

	return;
}

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS DEFINITION ------------------------------------------ */

int lgw_board_setconf(struct lgw_conf_board_s conf) {

	DEBUG_MSG("INFO: empty method lgw_board_setconf called.\n");

	return LGW_HAL_SUCCESS;
}

int lgw_rxrf_setconf(uint8_t rf_chain, struct lgw_conf_rxrf_s conf) {

	DEBUG_MSG("INFO: empty method lgw_rxrf_setconf called.\n");

	return LGW_HAL_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_rxif_setconf(uint8_t if_chain, struct lgw_conf_rxif_s conf) {

	DEBUG_MSG("INFO: empty method lgw_rxif_setconf called.\n");

	return LGW_HAL_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_txgain_setconf(struct lgw_tx_gain_lut_s *conf) {
	int i;

	DEBUG_MSG("INFO: empty method lgw_txgain_setconf called.\n");


	return LGW_HAL_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_start(void) {

	DEBUG_MSG("INFO: empty method lgw_start called.\n");


	return LGW_HAL_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_stop(void) {

	DEBUG_MSG("INFO: empty method lgw_stop called.\n");

	return LGW_HAL_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_receive(uint8_t max_pkt, struct lgw_pkt_rx_s *pkt_data) {

	DEBUG_MSG("INFO: empty method lgw_receive called.\n");

	return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_send(struct lgw_pkt_tx_s pkt_data) {

	DEBUG_MSG("INFO: empty method lgw_send called.\n");

	return LGW_HAL_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_status(uint8_t select, uint8_t *code) {
	int32_t read_value;

	DEBUG_MSG("INFO: empty method lgw_status called.\n");
	*code = RX_STATUS_UNKNOWN;
	return LGW_HAL_SUCCESS;

}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_abort_tx(void) {
	int i;

	DEBUG_MSG("INFO: empty method lgw_abort_tx called.\n");

	return LGW_HAL_SUCCESS;

}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_get_trigcnt(uint32_t* trig_cnt_us) {
	int i;
	int32_t val;

	DEBUG_MSG("INFO: empty method lgw_abort_tx called.\n");

	*trig_cnt_us = 0;
    return LGW_HAL_SUCCESS;

}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

const char* lgw_version_info() {
	return lgw_version_string;
}


/* --- EOF ------------------------------------------------------------------ */
