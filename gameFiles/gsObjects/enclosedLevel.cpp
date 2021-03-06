#include "enclosedLevel.h"

EnclosedLevel::EnclosedLevel(double X, double Y, double W, double H, Level* l) : InstanceLev(X, Y, W, H){
    trueX = x;
    trueY = y;
    lev = l;
    shaderCheck = true;
    time = 0;
    levelUp = false;
    prevLevelUp = levelUp;
    loadedTX = false;
    if (W == 0) openHorizontally = true;
    else if (H == 0) openHorizontally = false;
    else {
        fprintf(stderr, "WARNING for EnclosedLevel: w or h expected to be 0");
        openHorizontally = true;
    }
    l->createLevel();
    l->moveRoom(x, y, false);
    l->setGlobal(true);
    trueW = openHorizontally ? l->w : l->h;
    name = "Enclosed Level";
    messWithLevel = true;
    openTime = 0;
    maxOpenTime = 1;
    open = false;
    needExtra = true;
    connected = true;
    m = nullptr;
}

EnclosedLevel::~EnclosedLevel(){
    lev->destroyLevel();
}

std::vector<int> EnclosedLevel::initLayers(){
    layers.push_back(LAYER_FRONT);
    return layers;
}

void EnclosedLevel::update(double deltaTime, bool* keyPressed, bool* keyHeld, Instance* player){
    // Check to see if the level should be opened or closed.
    if (connected) checkOpen();
    time = fmod(time+deltaTime, 1);
    prevLevelUp = levelUp;
    lastW = openHorizontally ? w : h;
    prevLevelUp = levelUp;
    if (open && openTime < maxOpenTime){
        openTime += deltaTime;
        if (openTime >= maxOpenTime){ 
            openTime = maxOpenTime;
            levelUp = true;
        }
    } else if (!open && openTime > 0){
        levelUp = false;
        openTime -= deltaTime;
        if (openTime < 0) openTime = 0;
    }
    solid = openTime > 0 && openTime < maxOpenTime;
    if (openHorizontally) w = trueW*openTime/maxOpenTime;
    else h = trueW*openTime/maxOpenTime;
    pushLevel = abs(lastW-(openHorizontally?w:h)) > 0.0001;
}

void EnclosedLevel::checkShaders(GLShaders* gls){
    if (!gls->programExists("dottedLine")){
        gls->createProgram("", "gameFiles/shaders/dotted", "dottedLine");
    }
    if (!gls->programExists("diamond")){
        gls->createProgram("", "gameFiles/shaders/enclosedDiamond", "diamond");
    }
}

void EnclosedLevel::drawEX(GLUtil* glu, int layer){
    GLDraw* gld = glu->draw;
    GLShaders* gls = glu->shade;
    // Check if the necessary shaders are loaded in.
    if (shaderCheck){
        checkShaders(gls);
        shaderCheck = false;
    }
    // If the level's visible, don't do anything.
    if (levelUp) return;
    gld->color(lev->r, lev->g, lev->b);
    if (openTime <= 0){
        // If the enclosed level's has a width of 0, let's make it a dotted line.
        int program = gls->bindShader("dottedLine");
        // TODO: Make xScale and yScale correspond to resolution to window size.
        gls->addUniform(program, "xScale", 1);
        gls->addUniform(program, "yScale", 1);
        gls->addUniform(program, "camX", gld->camX);
        gls->addUniform(program, "camY", gld->camY);
        gls->addUniform(program, "horizontal", !openHorizontally);
        gls->addUniform(program, "time", time);
        gld->begin("LINES");
        gld->vertW(x, y);
        if (openHorizontally) gld->vertW(x, y+h);
        else gld->vertW(x+w, y);
        gld->end();
        gls->unbindShader();
        return;
    }
    // Otherwise, we're going to have to draw the entire level in this...
    // Let's start with a rectangle being the background of the level.
    gld->begin("QUADS");
    gld->vertW(x,y);
    gld->vertW(x,y+h);
    gld->vertW(x+w,y+h);
    gld->vertW(x+w,y);
    gld->end();
    // Now, we need to do a translation to the corner of the screen for this.
    float transX = gld->camX-x;
    float transY = gld->camY-y;
    float scale = maxOpenTime/openTime;
    if (scale == 0) scale = 0.001;
    if (openHorizontally){
        gld->pushCameraMem(x+transX*scale, gld->camY, scale*gld->getWidth(), gld->getHeight(), false);
    } else {
        gld->pushCameraMem(gld->camX, y+transY*scale, gld->getWidth(), scale*gld->getHeight(), false);
    }
    // Now, draw the contained level!
    std::map<int, std::vector<Layer*>> levLays;
    levLays = lev->getLayers(levLays);
    std::map<int, std::vector<Layer*>>::iterator layI = levLays.begin();
    lev->drawLayer(glu, LAYER_BACK);
    for (; layI != levLays.end(); layI++){
        lev->drawLayer(glu, layI->first);
    }
    lev->drawShaderboxes(glu, nullptr, true, nullptr);
    gld->popCameraMem();
    // Now, draw the diamond.
    gld->color(lev->r*0.75, lev->g*0.75, lev->b*0.75, 0.5);
    int program = gls->bindShader("diamond");
    // TODO: Make xScale and yScale correspond to resolution to window size.
    gls->addUniform(program, "xScale", 1);
    gls->addUniform(program, "yScale", 1);
    gls->addUniform(program, "camX", gld->camX);
    gls->addUniform(program, "camY", gld->camY);
    gls->addUniform(program, "horizontal", !openHorizontally);
    gls->addUniform(program, "time", time);
    gls->addUniform(program, "open", 1-openTime/maxOpenTime);
    gld->begin("QUADS");
    gld->vertW(x,y,0.01);
    gld->vertW(x,y+h,0.01);
    gld->vertW(x+w,y+h,0.01);
    gld->vertW(x+w,y,0.01);
    gld->end();
    gls->unbindShader();
}

bool EnclosedLevel::messWithLevels(LevelList* levs, Level* lv, Map* map, Instance* player){
    bool resetLayers = false;
    m = map;
    if (!loadedTX){
        // Make trueX and trueY the x and y of that object in map coords.
        trueX = x-lv->getXOff()+lv->getMXOff();
        trueY = y-lv->getYOff()+lv->getMYOff();
        loadedTX = true;
    }
    // If we have no levels to move, then we should do nothing.
    if (levs == nullptr) return false;
    if (levelUp && !prevLevelUp){
        // Add our level if we need to.
        LevelList* ll = levs;
        while (ll->next != nullptr){
            ll = ll->next;
        }
        LevelList* ourLevel = new LevelList();
        ourLevel->lev = lev;
        ourLevel->prev = ll;
        ll->next = ourLevel;
        resetLayers = true;
    } else if (!levelUp && prevLevelUp){
        // Remove our level if we need to.
        LevelList* ll = levs;
        while (ll != nullptr){
            LevelList* next = ll->next;
            if (ll->lev == lev){
                if (ll->prev != nullptr) ll->prev->next = ll->next;
                if (ll->next != nullptr) ll->next->prev = ll->prev;
                delete ll;
                resetLayers = true;
            }
            ll = next;
        }
    }
    if (!pushLevel) return resetLayers;
    for (LevelList* ll = levs; ll != nullptr; ll = ll->next){
        Level* l = ll->lev;
        // Move/split levels to make room for the new level if they're on the right/bottom.
        // For moving the level right, let's say the midpoint of the level is where we decide if we should move it.
        if (openHorizontally && l != lev){
            if (l->getXOff() <= x && l->getXOff()+l->w >= x 
                && l->getYOff() <= y+h && l->getYOff()+l->h >= y){
                l->bisectLevel(true, x-l->getXOff(), w-lastW, this);
            } else if (l->getXOff()+l->w/2 >= x) l->moveRoom(w-lastW, 0, true);
        } else if (l != lev) {
            if (l->getYOff() <= y && l->getYOff()+l->h >= y
                && l->getXOff() <= x+w && l->getXOff()+l->w >= x){
                l->bisectLevel(false, y-l->getYOff(), h-lastW, this);
            } else if (l->getYOff()+l->h/2 >= y) l->moveRoom(0, h-lastW, true);
        }
    }
    // Add the gap to the map
    if (openHorizontally) map->addGap(trueX, w-lastW, true, true);
    else map->addGap(trueY, h-lastW, false, true);
    // Move the player if applicable.
    if (player != nullptr){
        if (openHorizontally && player->x > x){
            player->x += w-lastW;
            ((Player *)player)->cameraJump(w-lastW, 0);
        }
        else if (!openHorizontally && player->y > y){
            player->y += h-lastW;
            ((Player *)player)->cameraJump(0, h-lastW);
        }
    }
    return resetLayers;
}

bool EnclosedLevel::removeMessFromWorld(LevelList* levs, Level* lv, Instance* player){
    bool resetLayers = false;
    if (openHorizontally) m->addGap(trueX, 0, true, false);
    else m->addGap(trueY, 0, false, false);
    if (levs == nullptr) return false;
    LevelList* ll = levs;
    if (player != nullptr){
        if (openHorizontally && player->x > x){
            player->x -= w;
            ((Player *)player)->cameraJump(-w, 0);
        } else if (!openHorizontally && player->y > y){
            player->y += -h;
            ((Player *)player)->cameraJump(0, -h);
        }
    }
    while (ll != nullptr){
        LevelList* next = ll->next;
        Level* l = ll->lev;
        if (ll->lev == lev){
            if (ll->prev != nullptr) ll->prev->next = ll->next;
            if (ll->next != nullptr) ll->next->prev = ll->prev;
            delete ll;
            resetLayers = true;
        } else if (openHorizontally){
            // Undo the bisection or moving of the room horizontally.
            if (l->getXOff() <= x && l->getXOff()+l->w >= x 
                && l->getYOff() <= y+h && l->getYOff()+l->h >= y){
                l->bisectLevel(true, x-l->getXOff(), -w, this);
            } else if (l->getXOff()+l->w/2 >= x) l->moveRoom(-w, 0, true);
        } else {
            // Undo the bisection or moving of the room vertically.
            if (l->getYOff() <= y && l->getYOff()+l->h >= y
                && l->getXOff() <= x+w && l->getXOff()+l->w >= x){
                l->bisectLevel(false, y-l->getYOff(), -h, this);
            } else if (l->getYOff()+l->h/2 >= y) l->moveRoom(0, -h, true);
        }
        ll = next;
    }
    return resetLayers;
}

void EnclosedLevel::checkOpen(){
    open = false;
    // The level should open when we an arc is colliding with it.
    for (int i = 0; i < arcList.size(); i++){
        ArcInfo a = arcList[i];
        if (abs(a.r-lev->r) < 0.1 && abs(a.g-lev->g) < 0.1 && 
            abs(a.b-lev->b) < 0.1){
            open = true;
            return;
        }
    }
}

void EnclosedLevel::disconnect(){
    connected = false;
}
