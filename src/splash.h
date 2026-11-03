#ifndef SPLASH_H
#define SPLASH_H

#include <stdbool.h>

#define SPLASH_DURATION 2.4f  // seconds

// Draws the splash animation at time t (seconds since it started).
// Returns true once it has finished or the user skipped it with a key or click.
bool SplashScreen(float t);

#endif
