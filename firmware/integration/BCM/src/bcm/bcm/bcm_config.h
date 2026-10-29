#ifndef BCM_CONFIG_H
#define BCM_CONFIG_H

#define BCM_STARTUP_LIMIT_MS            1000u
#define BCM_PCT_MAX                     100u

#define BCM_DOOR_DRIVE_MS               400u
#define BCM_DOOR_DRIVE_LIMIT_MS         600u
#define BCM_DOOR_CONFIRM_LIMIT_MS       800u
#define BCM_DOOR_CONTACT_DEBOUNCE_MS    30u
#define BCM_DOOR_RETRY_COUNT            1u

#define BCM_THERMAL_REVERSE_DELAY_MS    500u
#define BCM_THERMAL_MAX_OUTPUT_PCT      100u
#define BCM_FAN_MISMATCH_LIMIT_MS       2000u
#define BCM_OVERHEAT_CUT_DECI_C         800
#define BCM_OVERHEAT_RESUME_DECI_C      600
#define BCM_RECOVERY_CONFIRM_MS         1000u
#define BCM_FAN_MEASURE_TIMEOUT_MS      500u

#define BCM_TEMP_ADC_MAX                4095u
#define BCM_TEMP_RANGE_DECI_C           1000
#define BCM_TEMP_ADC_OPEN_BELOW         41u
#define BCM_TEMP_ADC_SHORT_ABOVE        4054u

#define BCM_PERMISSION_TIMEOUT_MS       150u
#define BCM_CAN_TRANSFER_MARGIN_MS      5u

#define BCM_DOOR_STATE_PERIOD_MS        100u
#define BCM_CLIMATE_STATE_PERIOD_MS     200u
#define BCM_LIGHT_STATE_PERIOD_MS       200u
#define BCM_STATUS_PERIOD_MS            250u

#define BCM_COMMAND_HANDOFF_SIZE        6u
#define BCM_APP_OUTBOX_SIZE             8u

#endif
