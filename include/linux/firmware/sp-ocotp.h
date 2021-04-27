/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __SP_OCOTP_FW_H_
#define __SP_OCOTP_FW_H_
/*
 * OTP memory
 * Each bank contains 4 words (32 bits).
 * Bank 0 starts at offset 0 from the base.
 *
 */
#define OTP_WORDS_PER_BANK		4
#define OTP_WORD_SIZE			sizeof(u32)
#define OTP_BIT_ADDR_OF_BANK		(8 * OTP_WORD_SIZE * \
						OTP_WORDS_PER_BANK)

#define QAC628_OTP_NUM_BANKS            8
#define QAC628_OTP_SIZE                 (QAC628_OTP_NUM_BANKS * \
						OTP_WORDS_PER_BANK * \
						OTP_WORD_SIZE)

#define I143_OTP_NUM_BANKS		8
#define I143_OTP_SIZE			(I143_OTP_NUM_BANKS * \
						OTP_WORDS_PER_BANK * \
						OTP_WORD_SIZE)

#define QAK645_EFUSE0_NUM_BANKS		8
#define QAK645_EFUSE0_SIZE		(QAK645_EFUSE0_NUM_BANKS * \
						OTP_WORDS_PER_BANK * \
						OTP_WORD_SIZE)

#define QAK645_EFUSE1_NUM_BANKS		64
#define QAK645_EFUSE1_SIZE		(QAK645_EFUSE1_NUM_BANKS * \
						OTP_WORDS_PER_BANK * \
						OTP_WORD_SIZE)

#define QAK645_EFUSE2_NUM_BANKS		8
#define QAK645_EFUSE2_SIZE		(QAK645_EFUSE2_NUM_BANKS * \
						OTP_WORDS_PER_BANK * \
						OTP_WORD_SIZE)

#define OTP_READ_TIMEOUT                20000

/* HB_GPIO register map */
#define ADDRESS_8_DATA                  0x20
#define ADDRESS_9_DATA                  0x24
#define ADDRESS_10_DATA                 0x28
#define ADDRESS_11_DATA                 0x2C

/* OTPRX register map */
#define OTP_PROGRAM_CONTROL             0x0C
#define PIO_MODE                        0x07

#define OTP_PROGRAM_ADDRESS             0x10
#define PROG_OTP_ADDR                   0x1FFF

#define OTP_PROGRAM_PGENB               0x20
#define PIO_PGENB                       0x01

#define OTP_PROGRAM_ENABLE              0x24
#define PIO_WR                          0x01

#define OTP_PROGRAM_VDD2P5              0x28
#define PROGRAM_OTP_DATA                0xFF00
#define PROGRAM_OTP_DATA_SHIFT          8
#define REG25_PD_MODE_SEL               0x10
#define REG25_POWER_SOURCE_SEL          0x02
#define OTP2REG_PD_N_P                  0x01

#define OTP_PROGRAM_STATE               0x2C
#define OTPRSV_CMD3                     0xE0
#define OTPRSV_CMD3_SHIFT               5
#define TSMC_OTP_STATE                  0x1F

#define OTP_CONTROL                     0x44
#define PROG_OTP_PERIOD                 0xFFE0
#define PROG_OTP_PERIOD_SHIFT           5
#define OTP_EN_R                        0x01

#define OTP_CONTROL2                    0x48
#define OTP_RD_PERIOD                   0xFF00
#define OTP_RD_PERIOD_SHIFT             8
#define OTP_READ                        0x04

#define OTP_STATUS                      0x4C
#define OTP_READ_DONE                   0x10

#define OTP_READ_ADDRESS                0x50
#define RD_OTP_ADDRESS                  0x1F

typedef struct {
	int size;
} sp_otp_vX_t;

#endif /* __SP_OCOTP_FW_H_ */
