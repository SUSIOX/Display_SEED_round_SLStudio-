#include "blink_controller.h"
#include <math.h>
#include <stdlib.h>

static double random_range(double min_val, double max_val) {
    double scale = rand() / (double) RAND_MAX;
    return min_val + scale * (max_val - min_val);
}

static double sigmoid_func(double x) {
    const double k = 12.0;
    return 1.0 / (1.0 + exp(-k * (x - 0.5)));
}

void blink_init(BlinkController* b, double now) {
    b->state = BLINK_IDLE;
    b->state_start_time = now;
    b->next_blink_delay = random_range(2.0, 7.0);
    b->alpha = 0.0;
}

void blink_update(BlinkController* b, double now) {
    double dt = now - b->state_start_time;
    
    switch(b->state) {
        case BLINK_IDLE:
            b->alpha = 0.0;
            if (dt >= b->next_blink_delay) {
                b->state = BLINK_CLOSING;
                b->state_start_time = now;
            }
            break;
            
        case BLINK_CLOSING: {
            double T_close = 0.08;
            if (dt >= T_close) {
                b->alpha = 1.0;
                b->state = BLINK_CLOSED;
                b->state_start_time = now;
            } else {
                double x = dt / T_close;
                b->alpha = sigmoid_func(x);
            }
            break;
        }
            
        case BLINK_CLOSED: {
            double T_closed = 0.06;
            b->alpha = 1.0;
            if (dt >= T_closed) {
                b->state = BLINK_OPENING;
                b->state_start_time = now;
            }
            break;
        }
            
        case BLINK_OPENING: {
            double T_open = 0.12;
            if (dt >= T_open) {
                b->alpha = 0.0;
                b->state = BLINK_IDLE;
                b->state_start_time = now;
                b->next_blink_delay = random_range(2.0, 7.0);
            } else {
                double x = dt / T_open;
                b->alpha = 1.0 - sigmoid_func(x);
            }
            break;
        }
    }
}

double blink_get_alpha(const BlinkController* b) {
    return b->alpha;
}
