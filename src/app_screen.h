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
    SCREEN_CODE,       // the free code editor
    SCREEN_HOME,       // the dashboard: learning journeys and statistics
    SCREEN_LEVEL,      // playing a level's coding steps
    SCREEN_RANKINGS,
    SCREEN_SPEED,      // a level's speed round (gold mastery) or a short challenge
} AppScreen;

#endif
