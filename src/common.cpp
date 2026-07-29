#include "common.h"

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;

bool gameover = false;
bool pressed = false;
bool forceQuit = false;
bool selected = false;
int selectChange = 1;

int gameMode = 0;
const std::string game_mode[] = {"Time"};
int timeMode = 0;
const int time_mode[] = {240};

Sint32* highscore = 0;