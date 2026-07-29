#include "Game.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

/*
 * Hand the final score to the page, once a run has really ended.
 *
 * A run left through EXIT is not reported - that one saves its state to be
 * continued, so it is not over. The mode check stays as a guard even though
 * there is only one mode left to be in.
 *
 * The page decides what to do with it. A leaderboard host defines
 * window.onJewelGameOver; a plain build has nobody listening.
 */
static void reportScore(int score)
{
    if(gameMode != Time)
        return;
    EM_ASM({
        if (typeof window.onJewelGameOver === 'function') {
            try { window.onJewelGameOver($0); } catch (e) {}
        }
    }, score);
}
#else
static void reportScore(int) {}
#endif

Game::Game(const int &nRows, const int &nCols) : jewel(nRows, nCols), nRows(nRows), nCols(nCols)
{
    running = true;
    startGame();

    x = y = 0;
    run();
}

void Game::startGame()
{
    while(SDL_WaitEvent(&e)) {
        if(e.type == SDL_QUIT) {
            running = false;
            break;
        }
        else {
            jewel.renderStart();
            if(e.type == SDL_MOUSEMOTION || e.type == SDL_MOUSEBUTTONDOWN) {
                SDL_GetMouseState(&mousePos.x, &mousePos.y);
                if(forceQuit) {
                    if(SDL_PointInRect(&mousePos, &jewel.continueSelect)) {
                        selectChange = ContinueSelection;
                        if(e.type == SDL_MOUSEBUTTONDOWN) {
                            start();
                            break;
                        }
                    }
                }
                if(SDL_PointInRect(&mousePos, &jewel.newGameSelect)) {
                    selectChange = NewGameSelection;
                    if(e.type == SDL_MOUSEBUTTONDOWN) {
                        forceQuit = false;
                        start();
                        break;
                    }
                }
            }
            else if(e.type == SDL_KEYDOWN) {
                switch(e.key.keysym.sym) {
                    case SDLK_s: case SDLK_DOWN:
                        selectChange = (selectChange + 1) % Total_Selection;
                        break;

                    case SDLK_w: case SDLK_UP:
                        selectChange = (Total_Selection + (selectChange - 1)) % Total_Selection;
                        break;

 
                    case SDLK_RETURN:
                        start();
                        break;
                }
            }
        }
    }
}

void Game::endGame()
{
    jewel.engine.music.stopMusic();
    if(forceQuit) {
        startGame();
        return;
    }
    else {
        /* Read before rendering: renderEnd draws the final score and then clears
         * it, ready for the next run. Taken afterwards it is always zero. */
        const int finalScore = jewel.score;
        jewel.renderEnd();
        jewel.engine.endSFX.playSFX();
        reportScore(finalScore);
    }
    while(SDL_WaitEvent(&e)) {
        if(e.type == SDL_QUIT) {
            running = false;;
            break;
        }
        if(e.type == SDL_KEYDOWN) {
            if(e.key.keysym.sym == SDLK_ESCAPE) {
                startGame();
                break;
            }
            else if(e.key.keysym.sym == SDLK_RETURN) {
                start();
                break;
            }
        }
    }
}

void Game::start()
{
    gameover = false;
    if(gameMode == Time)
        highscore = &jewel.engine.savedHighscore[Time][timeMode];
    else highscore = &jewel.engine.savedHighscore[gameMode][0];

    jewel.engine.startSFX.playSFX();
    jewel.startNotice();
    jewel.engine.music.playMusic();
    /* Der Zeitgeber ist kein Uhrwerk - die Runde laeuft ueber SDL_GetTicks -,
       sondern der einzige Impuls, der die Schleife ohne Eingabe weiterdreht.
       Bei 1000 ms stand zwischen zwei Ereignissen bis zu eine Sekunde still:
       Animationen ruckelten und Klicks kamen traege an, weil SDL_WaitEvent
       so lange blockiert. 16 ms ergeben rund 60 Durchlaeufe je Sekunde. */
    timerID = SDL_AddTimer(16, callback, NULL);
    jewel.randomize();
    jewel.updateJewel();
}

void Game::run()
{
    while(running && SDL_WaitEvent(&e)) {
        if(e.type == SDL_QUIT) {
            running = false;
            forceQuit = true;
            jewel.saveState();
        }
        if(gameover) {
            jewel.hint.stop();
            jewel.needHint = false;
            SDL_RemoveTimer(timerID);
            if(!jewel.existMatch()) {
                SDL_Delay(400);
            }
            endGame();
        }
        else if(!jewel.existHint()) {
            /* No moves left: shuffle rather than end the round. The clock is
               what ends it. */
            jewel.randomize();
        }
        else {
            if(e.type == SDL_KEYDOWN) {
                if(e.key.keysym.sym == SDLK_ESCAPE)
                    gameover = true;
                else if(!pressed) {
                    pressed = true;
                }
                else keyControl();
                jewel.renderSelector(selectedX, selectedY, x, y);
                jewel.updateGame();
            }
            if(e.type == SDL_MOUSEMOTION || e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) {
                pressed = true;
                SDL_GetMouseState(&mousePos.x, &mousePos.y);
                if(e.type == SDL_MOUSEBUTTONDOWN && SDL_PointInRect(&mousePos, &jewel.exit)) {
                    gameover = forceQuit = true;
                    jewel.saveState();
                }
                for(int x_ = 0; x_ < nRows; x_++) {
                    for(int y_ = 0; y_ < nCols; y_++) {
                        if(SDL_PointInRect(&mousePos, &jewel.square[x_][y_])) {
                            x = x_;
                            y = y_;
                            mouseControl();
                            jewel.renderSelector(selectedX, selectedY, x, y);
                            jewel.updateGame();
                        }
                    }
                }
            }
            //TimerID event
            else jewel.renderSelector(selectedX, selectedY, x, y);
        }
    }
    SDL_RemoveTimer(timerID);
}

void Game::keyControl()
{
    switch(e.key.keysym.sym) {
        case SDLK_UP: case SDLK_w:
            x--;
            if(selected) {
                y = selectedY;
                if(x < 0)
                    x = selectedX;
                if(x != selectedX)
                    x = selectedX - 1;
            }
            else x = (nRows + x) % nRows;
            break;

        case SDLK_DOWN: case SDLK_s:
            x++;
            if(selected) {
                y = selectedY;
                if(x < 0)
                    x = selectedX;
                if(x != selectedX)
                    x = selectedX + 1;
            }
            else x = x % nRows;
            break;

        case SDLK_LEFT: case SDLK_a:
            y--;
            if(selected) {
                x = selectedX;
                if(y < 0)
                    y = selectedY;
                if(y != selectedY)
                    y = selectedY - 1;
            }
            else y = (nCols + y) % nCols;
            break;

        case SDLK_RIGHT: case SDLK_d:
            y++;
            if(selected) {
                x = selectedX;
                if(y < 0)
                    y = selectedY;
                if(y != selectedY)
                    y = selectedY + 1;
            }
            else y = y % nCols;
            break;
            
        case SDLK_RETURN: case SDLK_SPACE:
            swapJewels();
            break;
        
    }
}

void Game::mouseControl()
{
    switch(e.type) {
        case SDL_MOUSEMOTION:
            if(selected) {
                if(!swapCheck())
                    pressed = false;
                if(click)
                    drag = true;
                else drag = false;
            }
            break;

        case SDL_MOUSEBUTTONDOWN:
            click = true;
            if(drag) {
                selectedX = x;
                selectedY = y;
                selected = true;
            }
            else swapJewels();
            break;

        case SDL_MOUSEBUTTONUP:
            if(drag) {
                swapJewels();
            }
            else click = false;
            drag = false;
            break;
    }
}

void Game::swapJewels()
{
    /* Jeder Klick ist Aktivitaet: die Leerlaufzeit beginnt von vorn, damit der
       Hinweis nicht mitten im Spielen auftaucht. */
    jewel.hint.start();
    jewel.needHint = false;

    if(!selected) {
        selectedX = x;
        selectedY = y;
        selected = true;
        jewel.engine.selectSFX.playSFX();
    }
    else {
        if(swapCheck()) {
            std::swap(jewel.board[selectedX][selectedY], jewel.board[x][y]);
            jewel.updateJewel();
            if(!jewel.existMatch()) {
                std::swap(jewel.board[selectedX][selectedY], jewel.board[x][y]);
                jewel.updateJewel();
            }
            pressed = false;
        }
        else {
            /* Daneben geklickt. Bisher sprang der Cursor auf die alte Auswahl
               zurueck und selected wurde geloescht - der Klick war damit
               verloren und man musste denselben Stein noch einmal anklicken.
               Jetzt gilt der angeklickte Stein als neue Auswahl: jeder Klick
               tut etwas, entweder tauschen oder waehlen. */
            selectedX = x;
            selectedY = y;
            selected = true;
            pressed = true;
            jewel.engine.selectSFX.playSFX();
            return;
        }
        selected = false;
    }
}

bool Game::swapCheck()
{
    if( x > selectedX + 1 || x < selectedX - 1 || 
        y > selectedY + 1 || y < selectedY - 1 ||
        (x > selectedX && y > selectedY) || (x < selectedX && y < selectedY) ||
        (x > selectedX && y < selectedY) || (x < selectedX && y > selectedY))
        return false;
    else return true;
}

Uint32 Game::callback(Uint32 interval, void* param)
{
    SDL_Event event;
    SDL_UserEvent userevent;

    userevent.type = SDL_USEREVENT;
    userevent.code = 0;
    userevent.data1 = NULL;
    userevent.data2 = NULL;

    event.type = SDL_USEREVENT;
    event.user = userevent;

    SDL_PushEvent(&event);
    return(interval);
}