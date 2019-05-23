#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <basic_i2c.h>
#include <lsm9ds0.h>
#include <pca9685.h>
#include <pca9685_servo.h>

#define PCA_ADDR 0x40
#define LSM_G_ADDR 0x6b
#define LSM_AM_ADDR 0x1d
#define PCA_HERTZ 50

#define SERVO_MIN 700
#define SERVO_MAX 2500
#define SERVO_NEU 1600

#define SERVO_NUM 6

#define SERVO_R_HIP 0
#define SERVO_L_HIP 1

#define SERVO_R_KNEE 2
#define SERVO_L_KNEE 3

#define SERVO_R_ANK 4
#define SERVO_L_ANK 5

static struct lsm9ds0_settings settings =
{
	G_SCALE_2000DPS,
	A_SCALE_2G,
 	M_SCALE_2GS,
	G_ODR_95_BW_125, 
	A_ODR_50, 
	M_ODR_50
};

int center_all(struct pca9685_servo **servos)
{
	int status, i;
	
	/* iterate through servo pointer array and write neutral position */
	for (i = 0; i < SERVO_NUM; ++i) {
		if ((status = pca9685_servo_write_us(servos[i], SERVO_NEU)) < 0)
			return status;
	}	
	
	return 0;
}	

int right_step(struct pca9685_servo **servos)
{
	int status, deg;
	
	/* move left angle away from lean to balance */
	deg = pca9685_servo_deg_to_us(servos[SERVO_L_ANK], 120);
	if ((status = pca9685_servo_write_us(servos[SERVO_L_ANK], deg)) < 0)
		return status;
		
	delay(1500);	
	
	/* move right knee in */
	deg = pca9685_servo_deg_to_us(servos[SERVO_R_KNEE], 0);
	if ((status = pca9685_servo_write_us(servos[SERVO_R_KNEE], deg)) < 0)
		return status;
		
	/* move right leg out */
	deg = pca9685_servo_deg_to_us(servos[SERVO_R_HIP], 0);
	if ((status = pca9685_servo_write_us(servos[SERVO_R_HIP], deg)) < 0)
		return status;
		
	delay(1500);
		
	/* move right knee out */
	deg = pca9685_servo_deg_to_us(servos[SERVO_R_KNEE], 180);
	if ((status = pca9685_servo_write_us(servos[SERVO_R_KNEE], deg)) < 0)
		return status;
	
	delay(1500);
		
	/* move right leg and right knee to neutral */
	if ((status = pca9685_servo_write_us(servos[SERVO_R_HIP], SERVO_NEU)) < 0)
		return status;
	if ((status = pca9685_servo_write_us(servos[SERVO_R_KNEE], SERVO_NEU)) < 0)
		return status;	
	
	return 0;
}	

int left_step(struct pca9685_servo **servos)
{
	int status, deg;
	
	/* move right angle away from lean to balance */
	deg = pca9685_servo_deg_to_us(servos[SERVO_R_ANK], 120);
	if ((status = pca9685_servo_write_us(servos[SERVO_R_ANK], deg)) < 0)
		return status;
		
	delay(1500);	
	
	/* move left knee in */
	deg = pca9685_servo_deg_to_us(servos[SERVO_L_KNEE], 0);
	if ((status = pca9685_servo_write_us(servos[SERVO_L_KNEE], deg)) < 0)
		return status;
		
	/* move left leg out */
	deg = pca9685_servo_deg_to_us(servos[SERVO_L_HIP], 0);
	if ((status = pca9685_servo_write_us(servos[SERVO_L_HIP], deg)) < 0)
		return status;
		
	delay(1500);
		
	/* move left knee out */
	deg = pca9685_servo_deg_to_us(servos[SERVO_L_KNEE], 180);
	if ((status = pca9685_servo_write_us(servos[SERVO_L_KNEE], deg)) < 0)
		return status;
	
	delay(1500);
		
	/* move left leg and left knee to neutral */
	if ((status = pca9685_servo_write_us(servos[SERVO_L_HIP], SERVO_NEU)) < 0)
		return status;
	if ((status = pca9685_servo_write_us(servos[SERVO_L_KNEE], SERVO_NEU)) < 0)
		return status;	
	
	return 0;	
}	

int main()
{
	int status, i;
	
	struct pca9685 pca;
	struct lsm9ds0 lsm;
	struct pca9685_servo *servos[SERVO_NUM];
	
	struct pca9685_servo right_knee;
	struct pca9685_servo left_knee;
	
	struct pca9685_servo right_hip;
	struct pca9685_servo left_hip;
	
	struct pca9685_servo right_ankle;
	struct pca9685_servo left_ankle;
	
	servos[SERVO_R_HIP] = &right_hip;
	servos[SERVO_L_HIP] = &left_hip;
	servos[SERVO_R_KNEE] = &right_knee;
	servos[SERVO_L_KNEE] = &left_knee;
	servos[SERVO_R_ANK] = &right_ankle;
	servos[SERVO_L_ANK] = &left_ankle;
	
	float bias_tmp[3] = {0, 0, 0};
	
	/* open i2c file */
	int fd = open(DEVICE_I2C, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "Error in opening i2c file: %s\n", strerror(errno));
		return fd;
	}
	
	/* create pca */
	if ((status = pca9685_new(&pca, fd, PCA_ADDR)) < 0) {
		fprintf(stderr, "Error in pca_new %d: %s\n", status, strerror(status * -1));
		return status;
	}
	
	/* create lsm */
	if ((status = lsm9ds0_new(&lsm, &settings, fd, LSM_G_ADDR, LSM_AM_ADDR)) < 0) {
		fprintf(stderr, "Error in lsm_new %d: %s\n", status, strerror(status * -1));
		return status;
	}
	
	/* create servos */
	for (i = 0; i < SERVO_NUM; ++i) {
		if ((status = pca9685_servo_new(servos[i], &pca, i, SERVO_MIN, SERVO_MAX)) < 0) {
			fprintf(stderr, "Error in servo_new %d for servo %d: %s\n", status, i, strerror(status * -1));
			return status;
		}
	}	
	
	/* get pca ready for pwm I/O */
	if ((status = pca9685_pwm_init(&pca, PCA_HERTZ)) < 0) {
		fprintf(stderr, "Error in pwm_init %d: %s\n", status, strerror(status * -1));
		return status;
	}
	
	/* calibrate gyro twice, take the lower as the gyro can get confused during first reads */
	printf("Calibrating gyro... Do not touch!!\n");
	
	if ((status = lsm9ds0_cal(&lsm)) < 0) {
		fprintf(stderr, "Error in lsm_cal 1 %d: %s\n", status, strerror(status * -1));
		return status;		
	}
	
	for (i = 0; i < 3; ++i)
		bias_tmp[i] = lsm.g_bias[i];
		
	/* state gyro biases */
	printf("X Bias: %4.2f Y Bias: %4.2f Z Bias: %4.2f\n", lsm.g_bias[0], lsm.g_bias[1], lsm.g_bias[2]);	
	
	/* wait a second before attempting to calibrate again, this allows for the gyro to become stable */	
	delay(1000);	
	
	if ((status = lsm9ds0_cal(&lsm)) < 0) {
		fprintf(stderr, "Error in lsm_cal 2 %d: %s\n", status, strerror(status * -1));
		return status;		
	}
	
	for (i = 0; i < 3; ++i)
		lsm.g_bias[i] = (bias_tmp[i] > lsm.g_bias[i]) ? lsm.g_bias[i] : bias_tmp[i];
	
	
	/* center all servos */
	if ((status = center_all(servos)) < 0) {
		fprintf(stderr, "Error in centering servos %d: %s\n", status, strerror(status * -1));
		return status;
	}	
	
	/* take a step right */
	if ((status = right_step(servos)) < 0) {
		fprintf(stderr, "Error in right step %d: %s\n", status, strerror(status * -1));
		return status;
	}
	
	/* take a step left */
	if ((status = left_step(servos)) < 0) {
		fprintf(stderr, "Error in left step %d: %s\n", status, strerror(status * -1));
		return status;
	}
	
	/* state gyro biases */
	printf("Final:\nX Bias: %4.2f Y Bias: %4.2f Z Bias: %4.2f\n", lsm.g_bias[0], lsm.g_bias[1], lsm.g_bias[2]);
	
	/* read gyro */
	for (i = 0; i < 10; ++i) {
		if ((status = lsm9ds0_gyro_read(&lsm)) < 0) {
			fprintf(stderr, "Error in gyro_read %d: %s\n", status, strerror(status * -1));
			return status;
		}
		
		lsm_update(&lsm);
		printf("X: %4.2f Y: %4.2f Z: %4.2f\n", lsm.gx, lsm.gy, lsm.gz);
			
		delay(1000);		
	}
	
	/* clean up */
	pca9685_pwm_reset(&pca);
	
	return 0;
	
}