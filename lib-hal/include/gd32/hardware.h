/**
 * @file hardware.h
 *
 */
/* Copyright (C) 2021-2024 by Arjan van Vught mailto:info@gd32-dmx.org
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef GD32_HARDWARE_H_
#define GD32_HARDWARE_H_

#include <cstdint>
#include <cstring>
#include <time.h>
#include <uuid/uuid.h>

#include "hwclock.h"

#include "gd32.h"
#include "gd32_adc.h"
#include "gd32_micros.h"

#if defined (ENABLE_USB_HOST) && defined (CONFIG_USB_HOST_MSC)
extern "C" {
#include "usbh_core.h"
#if !defined (GD32F4XX)
 extern usbh_host usb_host;
#else
 extern usbh_host usb_host_msc;
#endif
}
#endif

#if defined (DEBUG_STACK)
 void stack_debug_run();
#endif
#if defined (DEBUG_EMAC)
 void emac_debug_run();
#endif

#include "panel_led.h"

extern volatile uint32_t s_nSysTickMillis;

class Hardware {
public:
	Hardware();

	uint32_t GetReleaseId() const {
		return 0;	// FIXME GetReleaseId
	}

	void GetUuid(uuid_t out) {
		memcpy(out, m_uuid, sizeof(uuid_t));
	}

	uint32_t Millis() {
		extern volatile uint32_t s_nSysTickMillis;
		return s_nSysTickMillis;
	}

	uint32_t Micros() {
		return micros();
	}

	uint32_t GetUpTime() {
		return Millis() / 1000U;
	}

	bool SetTime(__attribute__((unused)) const struct tm *pTime) {
#if !defined(DISABLE_RTC)
		m_HwClock.Set(pTime);
		return true;
#else
		return false;
#endif
	}

	void GetTime(struct tm *pTime) {
		auto ltime = time(nullptr);
		const auto *pLocalTime = localtime(&ltime);
		memcpy(pTime, pLocalTime, sizeof(struct tm));
	}

#if !defined(DISABLE_RTC)
	bool SetAlarm(const struct tm *pTime) {
		const auto b = m_HwClock.AlarmSet(pTime);
		return b;
	}

	void GetAlarm(struct tm *pTime) {
		m_HwClock.AlarmGet(pTime);
	}
#endif

	const char *GetBoardName(uint8_t& nLength) {
		nLength = sizeof(GD32_BOARD_NAME) - 1U;
		return GD32_BOARD_NAME;
	}

	const char* GetSysName(uint8_t& nLength) {
		nLength = 8;
		return "Embedded";
	}

	const char* GetSocName(uint8_t& nLength) {
		nLength = 4;
		return "GD32";
	}

	const char *GetCpuName(uint8_t& nLength) {
		nLength = sizeof(GD32_MCU_NAME) - 1U;
		return GD32_MCU_NAME;
	}

	uint32_t GetBoardId() {
		return 0;
	}

	const char *GetWebsiteUrl() {
		return "https://gd32-dmx.org";
	}

	void WatchdogInit() {
		m_bIsWatchdog = (SUCCESS == fwdgt_config(0xFFFF, FWDGT_PSC_DIV16));

		if (m_bIsWatchdog) {
			fwdgt_enable();
		}
	}

	void WatchdogFeed() {
		fwdgt_counter_reload();
	}

	void WatchdogStop() {
		m_bIsWatchdog = false;
		fwdgt_config(0xFFFF, FWDGT_PSC_DIV64);
	}

	bool IsWatchdog() const {
		return m_bIsWatchdog;
	}

	bool Reboot();

	float GetCoreTemperature() {
		return gd32_adc_gettemp();
	}

	float GetCoreTemperatureMin() {
		return -40.0f;
	}

	float GetCoreTemperatureMax() {
		return 85.0f;
	}

	void SetModeWithLock(hardware::ledblink::Mode mode, bool doLock);
	void SetMode(hardware::ledblink::Mode mode);
	hardware::ledblink::Mode GetMode() const {
		return m_Mode;
	}

	void Run() {
#if defined (ENABLE_USB_HOST) && defined (CONFIG_USB_HOST_MSC)
# if !defined (GD32F4XX)
		usbh_core_task(&usb_host);
# else
		usbh_core_task(&usb_host_msc);
# endif
#endif
		if (__builtin_expect (m_nTicksPerSecond != 0, 1)) {
			if (__builtin_expect (!(s_nSysTickMillis - m_nMillisPrevious < m_nTicksPerSecond), 1)) {
				m_nMillisPrevious = s_nSysTickMillis;

				m_nToggleLed ^= 0x1;

				if (m_nToggleLed != 0) {
#if defined (CONFIG_LEDBLINK_USE_PANELLED)
					hal::panel_led_on(hal::panelled::ACTIVITY);
#else
					GPIO_BOP(LED_BLINK_GPIO_PORT) = LED_BLINK_PIN;
#endif
				} else {
#if defined (CONFIG_LEDBLINK_USE_PANELLED)
					hal::panel_led_off(hal::panelled::ACTIVITY);
#else
					GPIO_BC(LED_BLINK_GPIO_PORT) = LED_BLINK_PIN;
#endif
				}
			}
		}

		hal::panel_led_run();

#if defined (DEBUG_STACK)
		stack_debug_run();
#endif

#if defined (DEBUG_EMAC)
		emac_debug_run();
#endif
	}

	static Hardware *Get() {
		return s_pThis;
	}

private:
	void RebootHandler();

	void SetFrequency(const uint32_t nFreqHz) {
		switch (nFreqHz) {
		case 0:
			m_nTicksPerSecond = 0;
#if defined (CONFIG_LEDBLINK_USE_PANELLED)
			hal::panel_led_off(hal::panelled::ACTIVITY);
#else
			GPIO_BC(LED_BLINK_GPIO_PORT) = LED_BLINK_PIN;
#endif
			break;
		case 1:
			m_nTicksPerSecond = (1000 / 1);
			break;
		case 3:
			m_nTicksPerSecond = (1000 / 3);
			break;
		case 5:
			m_nTicksPerSecond = (1000 / 5);
			break;
		case 255:
			m_nTicksPerSecond = 0;
#if defined (CONFIG_LEDBLINK_USE_PANELLED)
			hal::panel_led_on(hal::panelled::ACTIVITY);
#else
			GPIO_BOP(LED_BLINK_GPIO_PORT) = LED_BLINK_PIN;
#endif
			break;
		default:
			m_nTicksPerSecond = (1000 / nFreqHz);
			break;
		}
	}

private:
#if !defined(DISABLE_RTC)
	HwClock m_HwClock;
#endif
	uuid_t m_uuid;
	bool m_bIsWatchdog { false };
	hardware::ledblink::Mode m_Mode { hardware::ledblink::Mode::UNKNOWN };
	bool m_doLock { false };
	uint32_t m_nTicksPerSecond { 1000 / 2 };
	int32_t m_nToggleLed { 0 };
	uint32_t m_nMillisPrevious { 0 };

	static Hardware *s_pThis;
};

#endif /* GD32_HARDWARE_H_ */
