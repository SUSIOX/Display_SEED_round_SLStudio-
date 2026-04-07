#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BLINK_IDLE,
    BLINK_CLOSING,
    BLINK_CLOSED,
    BLINK_OPENING
} BlinkState;

typedef struct {
    BlinkState state;
    double state_start_time;
    double next_blink_delay;
    double alpha;   // 0.0 = otevřené, 1.0 = zavřené
} BlinkController;

// Inicializuje controller aktuálním časem (v sekundách)
void blink_init(BlinkController* b, double now);

// Aktualizuje stav mrkání podle aktuálního času (v sekundách)
void blink_update(BlinkController* b, double now);

// Vrátí aktuální alpha (0.0 až 1.0)
double blink_get_alpha(const BlinkController* b);

#ifdef __cplusplus
}
#endif
