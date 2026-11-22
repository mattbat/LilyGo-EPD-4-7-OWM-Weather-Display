#include "schedule.h"
// TODO(task 6): implement per PLAN Device flow step 3.
Plan planWake(int, int, int, WakeReason) { return Plan{Action::Live, 1800}; }
