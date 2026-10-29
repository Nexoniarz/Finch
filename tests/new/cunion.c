#include "cunion.h"
int poll_event(Event *e) {
    e->motion.type = EV_MOTION;
    e->motion.x = 1.5f;
    e->motion.y = -2.0f;
    return 1;
}
