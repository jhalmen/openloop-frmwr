/*
 * This file is part of the openloop hardware looper project.
 *
 * Copyright (C) 2018  Jonathan Halmen <jonathan@halmen.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "hardware.h"

/* TODO: this should probably go into wm8778.c */
void send_codec_cmd(uint16_t cmd)
{
	uint8_t data[2] = {cmd>>8, cmd};
	i2c_transfer7(I2C1, 0x34>>1, data, 2, 0, 0);
}

void led_setup(void)
{
	rcc_periph_clock_enable(RCC_GPIOA);
	gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO12);
}

void gpio_setup(void)
{
	/* i2s2 pins */
	rcc_periph_clock_enable(RCC_GPIOC);
	gpio_set_af(GPIOC, GPIO_AF5, GPIO6);
	gpio_mode_setup(GPIOC, GPIO_MODE_AF,
			GPIO_PUPD_PULLDOWN, GPIO6);
	rcc_periph_clock_enable(RCC_GPIOB);
	gpio_set_af(GPIOB, GPIO_AF5, GPIO15 | GPIO13 | GPIO12);
	gpio_set_output_options(GPIOB, GPIO_OTYPE_PP,
			GPIO_OSPEED_25MHZ, GPIO13);
	gpio_mode_setup(GPIOB, GPIO_MODE_AF,
			GPIO_PUPD_PULLDOWN, GPIO15 | GPIO13 | GPIO12);
	/* i2s2ext pin */
	gpio_set_af(GPIOC, GPIO_AF6, GPIO2);
	gpio_mode_setup(GPIOC, GPIO_MODE_AF, GPIO_PUPD_PULLDOWN, GPIO2);
}

void pll_setup(void)
{
	/* set main pll 84MHz */
	uint8_t pllm = 8;
	uint16_t plln = 336;
	uint8_t pllp = 8;
	uint8_t pllq = 14;
	uint8_t pllr = 0;
	/* set flash waitstates! */
	flash_set_ws(FLASH_ACR_PRFTEN | FLASH_ACR_DCEN |
		FLASH_ACR_ICEN | FLASH_ACR_LATENCY_2WS);
	rcc_set_main_pll_hsi(pllm, plln, pllp, pllq, pllr);
	/* set prescalers for the different domains */
	rcc_set_hpre(RCC_CFGR_HPRE_DIV_NONE);
	rcc_set_ppre1(RCC_CFGR_PPRE_DIV_2);
	rcc_set_ppre2(RCC_CFGR_PPRE_DIV_NONE);
	/* in case this is needed by the library: */
	rcc_ahb_frequency  = 84000000;
	rcc_apb1_frequency = 42000000;
	rcc_apb2_frequency = 84000000;
	/* finally enable pll */
	rcc_osc_on(RCC_PLL);
	rcc_wait_for_osc_ready(RCC_PLL);
	/* use pll as system clock */
	rcc_set_sysclk_source(RCC_CFGR_SW_PLL);
	rcc_wait_for_sysclk_status(RCC_PLL);
}

void systick_setup(uint32_t tick_frequency)
{
	/* systick_set_frequency (desired_systick_freq, current_freq_ahb) */
	/* returns false if frequency can't be set */
	if (!systick_set_frequency(tick_frequency,rcc_ahb_frequency))
		while (1);
	systick_counter_enable();
	systick_interrupt_enable();
}

void plli2s_setup(uint16_t n, uint8_t r)
{
	/* configure plli2s */
	/* void rcc_plli2s_config(uint16_t n, uint8_t r) */
	RCC_PLLI2SCFGR = (
		((n & RCC_PLLI2SCFGR_PLLI2SN_MASK) << RCC_PLLI2SCFGR_PLLI2SN_SHIFT) |
		((r & RCC_PLLI2SCFGR_PLLI2SR_MASK) << RCC_PLLI2SCFGR_PLLI2SR_SHIFT));
	/* enable pll */
	rcc_osc_on(RCC_PLLI2S);
	rcc_wait_for_osc_ready(RCC_PLLI2S);
}

/* TODO: unite these 4 functions into one? */
/* this might help? */
/* enum i2smode{ */
/* 	I2S_SLAVE_TRANSMIT, */
/* 	I2S_SLAVE_RECEIVE, */
/* 	I2S_MASTER_TRANSMIT, */
/* 	I2S_MASTER_RECEIVE */
/* }; */
void i2s_init_master_receive(uint32_t i2s, uint8_t div, uint8_t odd, uint8_t mckoe)
{
	/* set prescaler register */
	uint16_t pre = (((mckoe & 0x1) << 9) |
		((odd & 0x1) << 8) | div);
	SPI_I2SPR(i2s) = pre;
	/* set configuration register */
	uint32_t cfg = SPI_I2SCFGR_I2SMOD |
			SPI_I2SCFGR_I2SCFG_MASTER_RECEIVE |
			SPI_I2SCFGR_I2SSTD_I2S_PHILIPS |
			SPI_I2SCFGR_DATLEN_16BIT |
			SPI_I2SCFGR_CHLEN;  /*only if chlen=32bit*/
	SPI_I2SCFGR(i2s) |= cfg;
	/* finally enable the peripheral */
	SPI_I2SCFGR(i2s) |= SPI_I2SCFGR_I2SE;
}

void i2s_init_master_transmit(uint32_t i2s, uint8_t div, uint8_t odd, uint8_t mckoe)
{
	/* set prescaler register */
	uint16_t pre = (((mckoe & 0x1) << 9) |
		((odd & 0x1) << 8) | div);
	SPI_I2SPR(i2s) = pre;
	/* set configuration register */
	uint32_t cfg = SPI_I2SCFGR_I2SMOD |
			SPI_I2SCFGR_I2SCFG_MASTER_TRANSMIT |
			SPI_I2SCFGR_I2SSTD_I2S_PHILIPS |
			SPI_I2SCFGR_DATLEN_16BIT |
			SPI_I2SCFGR_CHLEN;  /*only if chlen=32bit*/
	SPI_I2SCFGR(i2s) |= cfg;
	/* finally enable the peripheral */
	SPI_I2SCFGR(i2s) |= SPI_I2SCFGR_I2SE;
}

void i2s_init_slave_transmit(uint32_t i2s)
{
	/* set configuration register */
	SPI_I2SCFGR(i2s) |= SPI_I2SCFGR_I2SMOD |
			SPI_I2SCFGR_I2SCFG_SLAVE_TRANSMIT |
			SPI_I2SCFGR_I2SSTD_I2S_PHILIPS |
			SPI_I2SCFGR_DATLEN_16BIT |
			SPI_I2SCFGR_CHLEN;  /*only if chlen=32bit*/
	/* finally enable the peripheral */
	SPI_I2SCFGR(i2s) |= SPI_I2SCFGR_I2SE;
}

void i2s_init_slave_receive(uint32_t i2s)
{
	/* set configuration register */
	SPI_I2SCFGR(i2s) |= SPI_I2SCFGR_I2SMOD |
			SPI_I2SCFGR_I2SCFG_SLAVE_RECEIVE |
			SPI_I2SCFGR_I2SSTD_I2S_PHILIPS |
			SPI_I2SCFGR_DATLEN_16BIT |
			SPI_I2SCFGR_CHLEN;  /*only if chlen=32bit*/
	/* finally enable the peripheral */
	SPI_I2SCFGR(i2s) |= SPI_I2SCFGR_I2SE;
}

uint32_t i2s_read(uint32_t i2s)
{
	if (SPI_I2SCFGR(i2s) & SPI_I2SCFGR_DATLEN_32BIT) {
		uint16_t dat = SPI_DR(i2s);
		return dat << 16 | SPI_DR(i2s);
	}
	return SPI_DR(i2s);
}

uint8_t chkside(uint32_t i2s)
{
	return SPI_SR(i2s) & BIT2;
}

void i2c_setup(void)
{
	/* i2c pins */
	/* pb6 & pb7 */
	rcc_periph_clock_enable(RCC_GPIOB);
	gpio_set_af(GPIOB, GPIO_AF4, GPIO6 | GPIO7);
	gpio_set_output_options(GPIOB, GPIO_OTYPE_OD,
			GPIO_OSPEED_50MHZ, GPIO6 | GPIO7);
	gpio_mode_setup(GPIOB, GPIO_MODE_AF,
			GPIO_PUPD_PULLUP, GPIO6 | GPIO7);
	/* i2c periph*/
	rcc_periph_clock_enable(RCC_I2C1);
	i2c_set_clock_frequency(I2C1, I2C_CR2_FREQ_42MHZ);
	i2c_set_fast_mode(I2C1);
	/* ~ 200kHz freq: */
	/* i2c_set_ccr(I2C1, 38); */
	/* 100kHz freq: */ 
	/* i2c_set_ccr(I2C1, 42); */
	/* following results in approx 40kHz freq */
	i2c_set_ccr(I2C1, 336);
	/* max rise time = 300ns */
	i2c_set_trise(I2C1, 13);
	i2c_peripheral_enable(I2C1);
}
