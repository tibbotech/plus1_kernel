#ifndef __EYS3D_CAMERA_HW_H__
#define __EYS3D_CAMERA_HW_H__

// Video Class-Specific Request Codes
// (USB_Video_Class_1.1.pdf, A.8 Video Class-Specific Request Codes)
#define CAMERA_HW_RC_UNDEFINED                               0x00
#define CAMERA_HW_SET_CUR                                    0x01
#define CAMERA_HW_GET_CUR                                    0x81
#define CAMERA_HW_GET_MIN                                    0x82
#define CAMERA_HW_GET_MAX                                    0x83
#define CAMERA_HW_GET_RES                                    0x84
#define CAMERA_HW_GET_LEN                                    0x85
#define CAMERA_HW_GET_INFO                                   0x86
#define CAMERA_HW_GET_DEF                                    0x87

// Camera Terminal Control Selectors
// (USB_Video_Class_1.1.pdf, A.9.4 Camera Terminal Control Selectors)
#define CAMERA_HW_CT_CONTROL_UNDEFINED                       0x00
#define CAMERA_HW_CT_SCANNING_MODE_CONTROL                   0x01 // 1 Byte
#define CAMERA_HW_CT_AE_MODE_CONTROL                         0x02 // 1 Byte
#define CAMERA_HW_CT_AE_PRIORITY_CONTROL                     0x03 // 1 Byte
#define CAMERA_HW_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL          0x04 // 4 Byte
#define CAMERA_HW_CT_EXPOSURE_TIME_RELATIVE_CONTROL          0x05 // 1 Byte
#define CAMERA_HW_CT_FOCUS_ABSOLUTE_CONTROL                  0x06 // 2 Byte
#define CAMERA_HW_CT_FOCUS_RELATIVE_CONTROL                  0x07 // 2 Byte
#define CAMERA_HW_CT_FOCUS_AUTO_CONTROL                      0x08 // 1 Byte
#define CAMERA_HW_CT_IRIS_ABSOLUTE_CONTROL                   0x09 // 2 Byte
#define CAMERA_HW_CT_IRIS_RELATIVE_CONTROL                   0x0A // 1 Byte
#define CAMERA_HW_CT_ZOOM_ABSOLUTE_CONTROL                   0x0B // 2 Byte
#define CAMERA_HW_CT_ZOOM_RELATIVE_CONTROL                   0x0C // 3 Byte
#define CAMERA_HW_CT_PANTILT_ABSOLUTE_CONTROL                0x0D // 8 Byte
#define CAMERA_HW_CT_PANTILT_RELATIVE_CONTROL                0x0E // 4 Byte
#define CAMERA_HW_CT_ROLL_ABSOLUTE_CONTROL                   0x0F // 2 Byte
#define CAMERA_HW_CT_ROLL_RELATIVE_CONTROL                   0x10 // 2 Byte
#define CAMERA_HW_CT_PRIVACY_CONTROL                         0x11 // 1 Byte

// Processing Unit Control Selectors
// (USB_Video_Class_1.1.pdf, A.9.5 Processing Unit Control Selectors)
#define CAMERA_HW_PU_CONTROL_UNDEFINED                       0x00
#define CAMERA_HW_PU_BACKLIGHT_COMPENSATION_CONTROL          0x01 // 2 Byte
#define CAMERA_HW_PU_BRIGHTNESS_CONTROL                      0x02 // 2 Byte
#define CAMERA_HW_PU_CONTRAST_CONTROL                        0x03 // 2 Byte
#define CAMERA_HW_PU_GAIN_CONTROL                            0x04 // 2 Byte
#define CAMERA_HW_PU_POWER_LINE_FREQUENCY_CONTROL            0x05 // 1 Byte
#define CAMERA_HW_PU_HUE_CONTROL                             0x06 // 2 Byte
#define CAMERA_HW_PU_SATURATION_CONTROL                      0x07 // 2 Byte
#define CAMERA_HW_PU_SHARPNESS_CONTROL                       0x08 // 2 Byte
#define CAMERA_HW_PU_GAMMA_CONTROL                           0x09 // 2 Byte
#define CAMERA_HW_PU_WHITE_BALANCE_TEMPERATURE_CONTROL       0x0A // 2 Byte
#define CAMERA_HW_PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL  0x0B // 1 Byte
#define CAMERA_HW_PU_WHITE_BALANCE_COMPONENT_CONTROL         0x0C // 4 Byte
#define CAMERA_HW_PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL    0x0D // 1 Byte
#define CAMERA_HW_PU_DIGITAL_MULTIPLIER_CONTROL              0x0E // 2 Byte
#define CAMERA_HW_PU_DIGITAL_MULTIPLIER_LIMIT_CONTROL        0x0F // 2 Byte
#define CAMERA_HW_PU_HUE_AUTO_CONTROL                        0x10 // 1 Byte
#define CAMERA_HW_PU_ANALOG_VIDEO_STANDARD_CONTROL           0x11 // 1 Byte
#define CAMERA_HW_PU_ANALOG_LOCK_STATUS_CONTROL              0x12 // 1 Byte
#endif
