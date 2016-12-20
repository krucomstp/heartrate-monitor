/* main_arc.c - main source file for ARC app */

/*
 * Copyright (c) 2016 Intel Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <zephyr.h>
#include "heartrate.h"

#include <ipm.h>
#include <ipm/ipm_quark_se.h>
#include <init.h>
#include <device.h>
#include <misc/byteorder.h>
#include <adc.h>
#include <i2c.h>
#include <stdio.h>
#include <string.h>
#include <display/grove_lcd.h>

#include <misc/printk.h>
#define DBG	printk

/* measure every 2ms */
#define INTERVAL 	2

#define ADC_DEVICE_NAME "ADC_0"
#define ADC_CHANNEL 	12
#define ADC_BUFFER_SIZE 4

/* ID of IPM channel */
#define HRS_ID		99

QUARK_SE_IPM_DEFINE(hrs_ipm, 0, QUARK_SE_IPM_OUTBOUND);

static int fadeRate = 0;
static const uint8_t color[4][3] = {{  0,   0, 255},	/* blue */
				    {  0, 255,   0},	/* green */
				    {255, 255,   0},	/* yellow */
				    {255,   0,   0}};	/* red */

/* buffer for the ADC data.
 * here only 4 bytes are used to store the analog signal
 */
static uint8_t seq_buffer[ADC_BUFFER_SIZE];

static struct adc_seq_entry sample = {
	.sampling_delay = 12,
	.channel_id = ADC_CHANNEL,
	.buffer = seq_buffer,
	.buffer_length = ADC_BUFFER_SIZE,
};

static struct adc_seq_table table = {
	.entries = &sample,
	.num_entries = 1,
};

static struct device *glcd = NULL;

/* Display the heartbeat by fading out the LCD with color. The index
 * stands for color: blue (0), green (1), yellow(2) and red(3).
 * Blue for heartrate lower than 60bpm, green for the range 60 - 79 bpm,
 * yellow for 80 - 99 bpm, and red for 100 bpm and above.
 */
static void show_heartbeat_using_fade_effect(int index)
{
	uint8_t red, green, blue;
	if (!glcd || fadeRate < 0 || index < 0 || index > 3) {
		return;
        }

	red = (color[index][0] * fadeRate / 255) & 0xff;
	green = (color[index][1] * fadeRate / 255) & 0xff;
	blue = (color[index][2] * fadeRate / 255) & 0xff;
	glcd_color_set(glcd, red, green, blue);

	fadeRate -= 15;
}

void main(void)
{
	struct device *adc, *ipm;
	uint8_t set_config;
	int count;
	int checkTime;
	int index;
	int value;
	int ret;
	bool isValid;
	char str[16];
	uint8_t ipm_value;

	/* Initialize the IPM */
        ipm = device_get_binding("hrs_ipm");
	if (!ipm) {
                DBG("IPM: Device not found.\n");
        }

	/* Initialize the ADC */
	adc = device_get_binding(ADC_DEVICE_NAME);
	if (!adc) {
		DBG("ADC Controller: Device not found.\n");
		return;
	}
	adc_enable(adc);

	/* Initialize the Grove LCD */
	glcd = device_get_binding(GROVE_LCD_NAME);
        if (!glcd) {
                DBG("Grove LCD: Device not found.\n");
        }
	else { /* Now configure the LCD the way we want it */
        	set_config = GLCD_FS_ROWS_2
                             | GLCD_FS_DOT_SIZE_LITTLE
                             | GLCD_FS_8BIT_MODE;

        	glcd_function_set(glcd, set_config);
		set_config = GLCD_DS_DISPLAY_ON;
        	glcd_display_state_set(glcd, set_config);
		glcd_color_set(glcd, 0, 0, 0);
	}

	count = 0;
	checkTime = 2000;
	index = 0;

	while (1) {
		isValid = false;
		if (adc_read(adc, &table) == 0) {
			uint32_t signal = (uint32_t) seq_buffer[0]
					| (uint32_t) seq_buffer[1] << 8
					| (uint32_t) seq_buffer[2] << 16
					| (uint32_t) seq_buffer[3] << 24;

			value = measure_heartrate(signal);
			if (value > 0) {
				/* normal heartbeat */
				if (value > 50 && value < 120) {
					isValid = true;
				}
				/* for abnormal heartbeat, check if it is stable for two seconds */
				else {
					checkTime -= INTERVAL;
					if (checkTime < 0) {
						isValid = true;
					}
				}
				if (isValid) {
					/* print to LCD screen */
					if (glcd) {
						glcd_clear(glcd);
						glcd_cursor_pos_set(glcd, 0, 0);
        					sprintf(str, "HR: %d BPM", value);
        					glcd_print(glcd, str, strlen(str));
					}

					/* send data over ipm to x86 side */
					if (ipm) {
						ipm_value = (uint8_t) value;
						ret = ipm_send(ipm, 1, HRS_ID, &ipm_value, sizeof(ipm_value));
				        	if (ret) {
                					printk("Failed to send IPM message, error (%d)\n", ret);
        					}
					}

					printk("%d\n", value);

					/* reset fading effect and check time for abnormal heart beat */
					fadeRate = 255;
					checkTime = 2000;
				}

				/* set the color code for heartbeat */
				if (value < 60)
					index = 0;
				else if (value < 80)
					index = 1;
				else if (value < 100)
					index = 2;
				else
					index = 3;
			}
		}

		/* blink the LCD to show the heartbeat */
		count ++;
		if (count >= 10) {
			show_heartbeat_using_fade_effect(index);
			count = 0;
		}

		k_sleep(INTERVAL);
	}
	adc_disable(adc);
}
