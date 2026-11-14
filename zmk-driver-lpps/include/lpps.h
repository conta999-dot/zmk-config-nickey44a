/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

/** Runtime accumulation threshold (1–127); defaults from CONFIG_LPPS_ACCUM_REPORT. */
int lpps_set_accum_report(int n);
int lpps_get_accum_report(void);
