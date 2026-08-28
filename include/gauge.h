/**
 * gauge.h — the hybrid battery gauge. See D044.
 *
 * The SY6970 measures charge current (REG 0x12) but has no discharge-current
 * ADC and no coulomb counter, so a true "count the mA" gauge is impossible
 * with this hardware alone. This is the closest honest approximation:
 *
 *   charging     integrate MEASURED charge current       (coulomb counting)
 *   charge done  snap to 100 %                           (absolute anchor)
 *   discharging  integrate a MODELLED draw, slowly       (bounded estimate)
 *                corrected toward the voltage curve
 *
 * The result is smooth and monotonic where voltage alone sagged and jumped,
 * and it can never drift far: the voltage curve pulls it back with a ~50 min
 * time constant, and every completed charge re-zeroes it exactly.
 */
#pragma once
#include <stdint.h>

void gauge_begin(uint16_t mv_now);

/* Every battery poll. dt_ms is real elapsed time since the last call;
 * charge_ma is the SY6970's measured charging current (0 when discharging);
 * brightness is the currently APPLIED panel level, which drives the model. */
void gauge_update(uint16_t mv, bool present, bool charging, bool charge_done,
                  uint16_t charge_ma, uint8_t brightness, uint32_t dt_ms);

int gauge_pct(void);        /* 0-100, or -1 when no battery       */
int gauge_ma(void);         /* signed estimate: + in, - out of the cell */
int gauge_mah_used(void);   /* since the last completed charge, or -1  */
