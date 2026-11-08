#ifndef APP_SCREEN_H
#define APP_SCREEN_H

// The app's top-level screens. main.c switches between them; dev_shot.c can start on any.
typedef enum {
    SCREEN_SPLASH,     // launch animation
    SCREEN_GALLERY,    // dev aid: every icon and the logo (--shot only)
    SCREEN_LOGIN,
    SCREEN_RECOVERY,   // show a new recovery code
    SCREEN_VERIFY,     // email verification
    SCREEN_SETTINGS,
    SCREEN_CODE,       // code editor
    SCREEN_HOME,       // the dashboard: learning journeys, statistics and lesson search
    SCREEN_LESSON,     // reading a lesson, its quiz and its result
    SCREEN_PRACTICE,
    SCREEN_COURSES,
    SCREEN_SPEED,      // a lesson's speed round (gold mastery)
} AppScreen;

#endif
