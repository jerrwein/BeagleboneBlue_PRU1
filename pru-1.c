#include <stdint.h>
#include <stdbool.h>
#include "linux_types.h"

/* Note: PRU number and DPRAM should be defined prior to pru specific headers */
#define PRU1
/* Note: DPRAM address is different for PRU0/PRU1 */
#define DPRAM_SHARED 0x00010000

#include "pru_defs.h"
#include "pru_pwm.h"
#include "pru_hal.h"

/* This field must also be changed in host_main.c */
#define nPruNum 1
#define PRU0_ARM_INTERRUPT 19
#define PRU1_ARM_INTERRUPT 20

#define GPIO0_START_ADDR 0x44E07000
#define GPIO1_START_ADDR 0x4804C000
#define GPIO2_START_ADDR 0x481AC000
#define GPIO3_START_ADDR 0x481ae000

#define GPIO_OE 0x134
#define GPIO_DATAIN (0x138)
#define GPIO_DATAOUT (0x13C)
#define GPIO_CLEARDATAOUT (0x190)
#define GPIO_SETDATAOUT (0x194)

#define GPIO1_CLR_DATA (GPIO1_START_ADDR | GPIO_CLEARDATAOUT)
#define GPIO1_SET_DATA (GPIO1_START_ADDR | GPIO_SETDATAOUT)

/* All 4 LEDs reside on GPIO1 */
#define BIT_USER_LED1 (1l << 0x15)  /* GPIO1.21 */
#define BIT_USER_LED2 (1l << 0x16)  /* GPIO1.22 */
#define BIT_USER_LED3 (1l << 0x17)  /* GPIO1.23 */
#define BIT_USER_LED4 (1l << 0x18)  /* GPIO1.24 */

#define MAX_PWMS_USED 8

/*
 * T1, "pr1_pru1_pru_r30[4]",  "SERVO", "pru_r30[4]"
 * T2, "pr1_pru1_pru_r30[5]",  "SERVO", "pru_r30[5]"
 * T3, "pr1_pru1_pru_r30[6]",  "SERVO", "pru_r30[6]"
 * T4, "pr1_pru1_pru_r30[7]",  "SERVO", "pru_r30[7]"
 * U5, "pr1_pru1_pru_r30[8]",  "SERVO", "pru_r30[8]"
 * R5, "pr1_pru1_pru_r30[9]",  "SERVO", "pru_r30[9]"
 * V5, "pr1_pru1_pru_r30[10]", "SERVO", "pru_r30[10]"
 * R6, "pr1_pru1_pru_r30[11]", "SERVO", "pru_r30[11]"
 */

volatile uint32_t	pwm_period[MAX_PWMS_USED];
volatile uint32_t	pwm_pulse_width[MAX_PWMS_USED];
volatile uint32_t	pwm_pulse_width_sorted[MAX_PWMS_USED];
volatile uint8_t	pwm_enabled[MAX_PWMS_USED];

/* Note: more info needed */
struct pwm_cmd_l cfg;

/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* Description:
 *
 *      pwm_period[i] = 0x003d0900;  4.0E6    50 Hz
 *      pwm_period[i] = 0x0032dcd5;  3.3E6    60 Hz
 *      pwm_period[i] = 0x00186a00;  1.6E6   125 Hz
 *      pwm_period[i] = 0x000c3500;  8.0E5   250 Hz
 *
 */
static void pwm_setup(void)
{
	uint8_t	i;

	for (i=0; i<MAX_PWMS_USED; i++)
	{
		pwm_period[i] = 0x186a00;     /* 125 Hz */
	}

	/* This is the unsorted version of the PWM data
	 * Times are given in IEP counts (200 cnts/us)
	 * eg. 1ms = 1000us = 200,000 IEP cnts
	 */
	pwm_pulse_width[0] = 200000;       /* 1000 us */
	pwm_pulse_width[1] = 200000;       /* 1000 us */
	pwm_pulse_width[2] = 200000;       /* 2000 us */
	pwm_pulse_width[3] = 200000;       /* 2000 us */
	pwm_pulse_width[4] = 200000;       /* 2000 us */
	pwm_pulse_width[5] = 200000;       /* 2000 us */
	pwm_pulse_width[6] = 200000;       /* 2000 us */
	pwm_pulse_width[7] = 200000;       /* 2000 us */

	/* Setup DPSHRAM - each PWM period is defined, however all must be the same for now */
	for (i=0; i<MAX_PWMS_USED; i++)
		PWM_CMD->periodhi[i][0] = pwm_period[i];

	/* Again, the unsorted version of the PWM data */
    	PWM_CMD->periodhi[0][1] = 200000;
    	PWM_CMD->periodhi[1][1] = 200000;
    	PWM_CMD->periodhi[2][1] = 200000;
    	PWM_CMD->periodhi[3][1] = 200000;
    	PWM_CMD->periodhi[4][1] = 200000;
    	PWM_CMD->periodhi[5][1] = 200000;
   	PWM_CMD->periodhi[6][1] = 200000;
    	PWM_CMD->periodhi[7][1] = 200000;

	/* OFF mask TBD, all 8 PWM on for now */
    	PWM_CMD->offmsk = 0;
    	PWM_CMD->enmask = 0x000000ff;
}

/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* Description: bubble_sort_with_index (uint8_t sorted_indexes[])
 *
 *
 */
static void bubble_sort_with_index (uint8_t sorted_indexes[])
{
	uint8_t     i, j;
	uint32_t    temp_val;
	uint8_t     temp_index;

	/* Populate the sorted array with the unsorted data */
	for (i=0; i<MAX_PWMS_USED; i++)
	{
		pwm_pulse_width_sorted[i] = pwm_pulse_width[i];
		sorted_indexes[i] = i;
	}

	for (i=0; i<MAX_PWMS_USED; i++)
	{
		for (j=i; j<MAX_PWMS_USED; j++)
		{
			/* Sort all values from smallest to largest */
			if (pwm_pulse_width_sorted[j] < pwm_pulse_width_sorted[i])
			{
				temp_val = pwm_pulse_width_sorted[i];
				temp_index = sorted_indexes[i];
				pwm_pulse_width_sorted[i] = pwm_pulse_width_sorted[j];
				pwm_pulse_width_sorted[j] = temp_val;
				sorted_indexes[i] = sorted_indexes[j];
				sorted_indexes[j] = temp_index;
			}
		}
	}
	/* Debug - place values in DPRAM */
#if 0
	for (i=0; i<MAX_PWMS_USED; i++)
	{
		PWM_CMD->periodhi[8+i][0] = 0;
		PWM_CMD->periodhi[8+i][1] = sorted_indexes[i];
	}
#endif
}

/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* Description: u32 read_PIEP_COUNT(void)
 *
 * Read the PIEP timer count.
 * 1ms = 1000us = 200,000 IEP cnts
 *
 */
static inline u32 read_PIEP_COUNT(void)
{
	return PIEP_COUNT;
}


/* Description: main(int argc, char *argv[])
 *
 * Read the PIEP timer count.
 * 1ms = 1000us = 200,000 IEP cnts
 *
 */
int main(int argc, char *argv[])
{
	uint8_t		i;
	uint32_t	last_read_enmask;	/* enable mask */
	uint32_t 	latest_enmask;
//	uint32_t 	latest_pulse_width;
	uint32_t 	reg_bit_mask;
	uint32_t 	staging_cnt;

	uint32_t 	cnt_ready;
	uint32_t 	next_on_cnt[MAX_PWMS_USED];
	uint32_t 	next_off_cnt[MAX_PWMS_USED];
	uint8_t  	sorted_indexes[MAX_PWMS_USED];

	volatile uint32_t	cnt_now;
	volatile uint32_t	lLedCount = 0;
	volatile bool		bLedState = false;
	volatile bool		bDone = false;

	/* Enable OCP master port */
	PRUCFG_SYSCFG &= ~SYSCFG_STANDBY_INIT;
	PRUCFG_SYSCFG = (PRUCFG_SYSCFG &
					~(SYSCFG_IDLE_MODE_M | SYSCFG_STANDBY_MODE_M)) |
					SYSCFG_IDLE_MODE_NO | SYSCFG_STANDBY_MODE_NO;

	/* Our PRU wins arbitration */
	PRUCFG_SPP |=  SPP_PRU1_PAD_HP_EN;

	/* Configure PIEP timer */
	PIEP_GLOBAL_CFG = GLOBAL_CFG_DEFAULT_INC(1) | GLOBAL_CFG_CMP_INC(1);
	PIEP_CMP_STATUS = CMD_STATUS_CMP_HIT(1); /* clear the interrupt */
	PIEP_CMP_CMP1   = 0x0;
	PIEP_CMP_CFG |= CMP_CFG_CMP_EN(1);
	PIEP_GLOBAL_CFG |= GLOBAL_CFG_CNT_ENABLE;

	/* Setup default PWM values */
	pwm_setup();

	/* Sort pulse width data in asending order */
	bubble_sort_with_index (sorted_indexes);

	/* Initialize count */
	cnt_now = read_PIEP_COUNT();

	/* Setup turn ON counts */
	next_on_cnt[0] = cnt_now + 200;
	for (i=1; i<MAX_PWMS_USED; i++)
		next_on_cnt[i] = next_on_cnt[i-1] + 200;

	/* Setup turn OFF counts */
	for (i=0; i<MAX_PWMS_USED; i++)
		next_off_cnt[i] = next_on_cnt[i] + pwm_pulse_width_sorted[i];

	/* Primary loop */
	while (!bDone)
	{
		/* Wait for turn ON counts */
		for (i=0; i<MAX_PWMS_USED; i++)
		{
			if (pwm_enabled[sorted_indexes[i]])
			{
				cnt_ready = next_on_cnt[i];
				/* Start with R30.4 */
				reg_bit_mask = (0x10 << (sorted_indexes[i]));
				while ((read_PIEP_COUNT() - cnt_ready) & 0x80000000);
				__R30 |= reg_bit_mask;
			}
		}

		/* Wait for turn OFF counts */
		for (i=0; i<MAX_PWMS_USED; i++)
		{
			if (pwm_enabled[sorted_indexes[i]])
			{
				/* Start with R30.4 */
				reg_bit_mask = ~(0x10 << (sorted_indexes[i]));
				cnt_ready = next_off_cnt[i];
				while ((read_PIEP_COUNT() - cnt_ready) & 0x80000000);
				__R30 &= reg_bit_mask;
			}
		}

#if 1
		/* Get any potentially new pulse width data and sort it */
		for (i=0; i<MAX_PWMS_USED; i++)
		{
			pwm_pulse_width[i] = PWM_CMD->periodhi[i][1];
			pwm_period[i] =  PWM_CMD->periodhi[i][0];
		}
		bubble_sort_with_index (sorted_indexes);
#endif
//		bubble_sort_with_index (sorted_indexes);

		/* Advance channel timers to next period - staggered ON times by 1 us */
		next_on_cnt[0] += pwm_period[0];
		for (i=1; i<MAX_PWMS_USED; i++)
			next_on_cnt[i] = next_on_cnt[i-1] + 200;

		/* Next turn OFF */
		for (i=0; i<MAX_PWMS_USED; i++)
			next_off_cnt[i] = next_on_cnt[i] + pwm_pulse_width_sorted[i];

		/* Update shared memory for readback of PWM settings */
		for (i=0; i<MAX_PWMS_USED; i++)
		{
			PWM_CMD->hilo_read[i][0] = pwm_period[i];
			PWM_CMD->hilo_read[i][1] = pwm_pulse_width[i];
		}

		/* Begin processing all potentially changed data */
		latest_enmask = PWM_CMD->enmask;
		if (last_read_enmask != latest_enmask)
		{
			/* Enable/Disable */
			for (i=0; i<MAX_PWMS_USED; i++)
			{
				if (latest_enmask & (1U << i))
					pwm_enabled[i] = 1;
				else
				{
					/* Turn off any disabled channel */
					/* Start with R30.4 */
					__R30 &= ~(0x10 << i);
					pwm_enabled[i] = 0;
				}
			}
			last_read_enmask = latest_enmask;
		}

#if 0
		/* Get potentially new pulse width data and sort it */
		for (i=0; i<MAX_PWMS_USED; i++)
		{
			pwm_pulse_width[i] = PWM_CMD->periodhi[i][1];
		}
		bubble_sort_with_index (sorted_indexes);
#endif

		/* Spend some time doing buzy work till just before next PW cycle */
		staging_cnt = next_on_cnt[0] - 1000;
		while ((read_PIEP_COUNT() - staging_cnt) & 0x80000000)
		{
//			__R30 |= 0x00000002;    /* PRU1.1 - P8.46 */
			for (i=0; i<10; i++);
			{
				asm("	nop");
			}
//			__R30 &= 0xfffffffd;    /* PRU1.1 - P8.46 */
			for (i=0; i<10; i++);
			{
				asm("	nop");
			}
		};

		// Exit if we receive a Host->PRU1 interrupt
		if (__R31 & 0x80000000)
		{
			bDone = true;
		}

//		if (1000000 < ++lLedCount)
		if (25 < ++lLedCount)
		{
			if (bLedState)
			{
				/* Clear LED_3 */
				*((volatile uint32_t *)GPIO1_CLR_DATA) = (uint32_t)BIT_USER_LED3;
				bLedState = false;
			}
			else
			{
				/* Set LED_3 */
				*((volatile uint32_t *)GPIO1_SET_DATA) = (uint32_t)BIT_USER_LED3;
				bLedState = true;
			}
			lLedCount = 0;
		}
	}

	__R31 = PRU1_ARM_INTERRUPT + 16; // PRUEVENT_0 on PRU_R31_VEC_VALID
	__halt();

	return 0;
}
