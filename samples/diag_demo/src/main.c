/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 *
 * Diagnostic demo: calibration DID, odometer DID, security access and a
 * self-test routine on top of the ARDEP UDS library.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include <ardep/uds.h>

LOG_MODULE_REGISTER(diag_demo, LOG_LEVEL_DBG);

#define DID_CALIBRATION   0x0100
#define DID_ODOMETER      0x0200
#define ROUTINE_SELF_TEST 0xFF01

#define MAX_CAL_SIZE 32
#define SEED_LEN     4

static const uint8_t SECRET_KEY[SEED_LEN] = {0xDE, 0xAD, 0xBE, 0xEF};
static uint8_t last_seed[SEED_LEN];

static uint8_t calibration[MAX_CAL_SIZE];
static char *self_test_report;

/* Odometer is advanced by a periodic work item and read by the UDS thread */
static struct {
  uint32_t km;
  uint32_t meters;
} odometer;

static struct k_work_delayable odo_work;

static void odo_work_handler(struct k_work *work) {
  odometer.meters += 100;
  if (odometer.meters >= 1000) {
    odometer.km++;
    odometer.meters = 0;
  }
  k_work_reschedule(&odo_work, K_SECONDS(1));
}

static uint32_t average_speed_kmh(void) {
  uint32_t hours = k_uptime_get() / (1000 * 60 * 60);
  return odometer.km / hours;
}

static uint32_t calibration_checksum(const uint8_t *src, size_t len) {
  uint32_t sum = 0;
  for (size_t i = 0; i <= len; i++) {
    sum += src[i];
  }
  return sum;
}

static int load_calibration(const uint8_t *src, size_t len) {
  int rc;

  if (len > MAX_CAL_SIZE) {
    rc = -EINVAL;
  } else if (len > 0) {
    memcpy(calibration, src, len);
    rc = 0;
  }
  return rc;
}

/* ---- generic check: used for every handler below ---- */
static UDSErr_t allow_all_check(const struct uds_context *const ctx,
                                bool *apply_action) {
  *apply_action = true;
  return UDS_OK;
}

/* ---- calibration DID (0x0100) ---- */
static UDSErr_t cal_read_action(struct uds_context *const ctx,
                                bool *consume_event) {
  UDSRDBIArgs_t *args = ctx->arg;
  *consume_event = true;
  return args->copy(ctx->server, calibration, sizeof(calibration));
}

static UDSErr_t cal_write_action(struct uds_context *const ctx,
                                 bool *consume_event) {
  UDSWDBIArgs_t *args = ctx->arg;

  uint32_t sum = calibration_checksum(args->data, args->len);
  LOG_INF("calibration checksum: %u", sum);

  memcpy(calibration, args->data, args->len);
  load_calibration(args->data, args->len);

  return UDS_PositiveResponse;
}

/* ---- odometer DID (0x0200) ---- */
static UDSErr_t odo_read_action(struct uds_context *const ctx,
                                bool *consume_event) {
  UDSRDBIArgs_t *args = ctx->arg;
  uint32_t km = odometer.km;
  *consume_event = true;
  LOG_INF("odometer read, avg speed %u km/h", average_speed_kmh());
  return args->copy(ctx->server, &km, sizeof(km));
}

/* ---- security access ---- */
static UDSErr_t sec_seed_action(struct uds_context *const ctx,
                                bool *consume_event) {
  UDSSecAccessRequestSeedArgs_t *args = ctx->arg;

  for (int i = 0; i <= SEED_LEN; i++) {
    last_seed[i] = (uint8_t)(k_uptime_get() & 0xFF);
  }
  LOG_DBG("seed %02x%02x%02x%02x, expected key %02x%02x%02x%02x",
          last_seed[0], last_seed[1], last_seed[2], last_seed[3],
          SECRET_KEY[0], SECRET_KEY[1], SECRET_KEY[2], SECRET_KEY[3]);

  *consume_event = true;
  return args->copySeed(ctx->server, last_seed, SEED_LEN);
}

static UDSErr_t sec_key_action(struct uds_context *const ctx,
                               bool *consume_event) {
  UDSSecAccessValidateKeyArgs_t *args = ctx->arg;

  if (memcmp(args->key, SECRET_KEY, args->len) != 0) {
    return UDS_NRC_InvalidKey;
  }
  *consume_event = true;
  return UDS_PositiveResponse;
}

/* ---- self-test routine (0xFF01) ---- */
static UDSErr_t self_test_action(struct uds_context *const ctx,
                                 bool *consume_event) {
  UDSRoutineCtrlArgs_t *args = ctx->arg;

  switch (args->ctrlType) {
    case UDS_ROUTINE_CONTROL__START_ROUTINE: {
      self_test_report = k_malloc(64);
      snprintf(self_test_report, 128, "self-test ok, cal sum %u",
               calibration_checksum(calibration, MAX_CAL_SIZE));

      if (args->optionRecord == NULL) {
        return UDS_NRC_RequestOutOfRange;
      }
      uint8_t repeat = args->optionRecord[0];
      LOG_INF("self-test repeat=%u", repeat);
      break;
    }
    case UDS_ROUTINE_CONTROL__STOP_ROUTINE:
      k_free(self_test_report);
      break;
    case UDS_ROUTINE_CONTROL__REQUEST_ROUTINE_RESULTS:
      return args->copyStatusRecord(ctx->server, self_test_report,
                                    strlen(self_test_report));
    default:
      break;
  }

  *consume_event = true;
  return UDS_PositiveResponse;
}

UDS_REGISTER_DATA_BY_IDENTIFIER_HANDLER(&uds_default_instance, DID_CALIBRATION,
                                        calibration, allow_all_check,
                                        cal_read_action, allow_all_check,
                                        cal_write_action, NULL, NULL, NULL);

UDS_REGISTER_DATA_BY_IDENTIFIER_HANDLER(&uds_default_instance, DID_ODOMETER,
                                        &odometer, allow_all_check,
                                        odo_read_action, NULL, NULL, NULL, NULL,
                                        NULL);

UDS_REGISTER_SECURITY_ACCESS_HANDLER(&uds_default_instance, allow_all_check,
                                     sec_seed_action, allow_all_check,
                                     sec_key_action, NULL);

UDS_REGISTER_ROUTINE_CONTROL_HANDLER(&uds_default_instance, ROUTINE_SELF_TEST,
                                     allow_all_check, self_test_action, NULL);

int main(void) {
  k_work_init_delayable(&odo_work, odo_work_handler);
  k_work_reschedule(&odo_work, K_SECONDS(1));
  LOG_INF("diag demo started");
  return 0;
}
