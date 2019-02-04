#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include "pruss/prussdrv.h"
#include "pruss/pruss_intc_mapping.h"
#include "mio.h"
#include "AM335X_GPIO.h"

// PRU data declarations
#define PRU_NUM0 0
#define PRU_NUM1 1

#define PATH_PRU_BINS "/home/jerry/Projs/C_Makefiles/PRUs/C/DSM2_PWM_Generator_2"

/* sigint handler */
static volatile unsigned int is_sigint = 0;
static volatile unsigned int is_sigterm = 0;

static void sig_handler (int sig_num)
{
	printf ("***** on_sigint(%d) *****\n", sig_num);

	if (sig_num == SIGINT)
	{
		printf ("***** on_sigint(%d) = SIGINT *****\n", sig_num);
		is_sigint = 1;
	}
	else if (sig_num == SIGTERM)
	{
		printf ("***** on_sigint(%d) = SIGTERM *****\n", sig_num);
		is_sigterm = 1;
	}
}

/* main */
int main (int ac, char** av)
{
	int	ret;
	tpruss_intc_initdata 	pruss_intc_initdata = PRUSS_INTC_INITDATA;

	/* Setup the SIGINT signal handling */
	if (signal(SIGINT, sig_handler) == SIG_ERR)
	{
  		printf ("\n**** Can't start SIGINT handler ****\n");
	}
	if (signal(SIGTERM, sig_handler) == SIG_ERR)
	{
  		printf ("\n**** Can't start SIGTERM handler ****\n");
	}

	/* Initialize the PRU */
	/* If this segfaults, make sure you're executing as root. */
	ret = prussdrv_init();
	if (0 != ret)
	{
		printf ("prussdrv_init() failed\n");
		return (ret);
	}

	/* Open PRU event Interrupt */
	ret = prussdrv_open (PRU_EVTOUT_1);
	if (ret)
	{
		printf ("prussdrv_open failed\n");
		return (ret);
	}

	/* Get the PRU interrupt initialized */
	ret = prussdrv_pruintc_init (&pruss_intc_initdata);
	if (ret != 0)
	{
		printf ("prussdrv_pruintc_init() failed\n");
		return (ret);
	}

	/* Write program data from data.bin to pru-1 */
	ret = prussdrv_load_datafile (PRU_NUM1, PATH_PRU_BINS"/pru1_data.bin");
	if (ret < 0)
	{
		printf ("prussdrv_load_datafile(PRU-1) failed\n");
		return (ret);
	}

	/* Load/Execute code on pru-1 */
	prussdrv_exec_program_at (PRU_NUM1, PATH_PRU_BINS"/pru1_text.bin", PRU1_START_ADDR);
	if (ret < 0)
	{
		printf ("prussdrv_exec_program_at(PRU-1) failed\n");
		return (ret);
	}

	/* Wait for PRU */
	printf ("\tINFO: Waiting for Ctl-C signal to terminate...\r\n");
	while (!is_sigint && !is_sigterm)
	{
                usleep(500000);
	}

//	printf ("\tINFO: Shutting down PRU-%d.\r\n", (int)PRU_NUM0);
	prussdrv_pru_send_event (ARM_PRU1_INTERRUPT);

	/* Wait until PRU has finished execution */
	printf ("\tINFO: Waiting for HALT command from PRU.\r\n");
	ret = prussdrv_pru_wait_event (PRU_EVTOUT_1);
	printf ("\tINFO: PRU program completed, event number %d\n", ret);

	usleep(100000);

	/* Clear PRU events */
	if (0 != (ret = prussdrv_pru_clear_event (PRU_EVTOUT_0, PRU1_ARM_INTERRUPT)))
	{
		printf ("prussdrv_pru_clear_event() failed, result = %d\n", ret);
		return (ret);
	}

	/* Disable PRU-1 and close memory mapping*/
	if (0 != (ret =	prussdrv_pru_disable (PRU_NUM1)))
	{
		printf ("prussdrv_pru_disable() failed\n");
		return (ret);
	}

	if (0 != (ret = prussdrv_exit()))
	{
		printf ("prussdrv_pru_exit() failed\n");
		return (ret);
	}

	return 0;
}
