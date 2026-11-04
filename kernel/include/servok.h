/**
 * @file 
 *
 * @brief
 *
 * @date
 *
 * @author
 */

#ifndef _SERVOK_H_
#define _SERVOK_H_

/** @brief Documents pwm channel0. */
extern int pwm_channel0;
/** @brief Documents pwm channel1. */
extern int pwm_channel1;
/** @brief Documents ch0 en. */
extern int ch0_en;
/** @brief Documents ch1 en. */
extern int ch1_en;

/** @brief Documents servo init. */
void servo_init(void);

/** @brief Documents sys servo enable. */
int sys_servo_enable(uint8_t channel, uint8_t enabled);

/** @brief Documents sys servo set. */
int sys_servo_set(uint8_t channel, uint8_t angle);

#endif /* _SERVOK_H_ */