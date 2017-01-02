//
//  CursesTileMap.cpp
//  cc2dxgame
//
//  Created by Marvin Sanchez on 02/01/2017.
//
//

#include "CursesTileMap.hpp"

extern "C" {
    bool isScreenDirty();
    char *getScreenData();
}

#include "pdccc2dx.h"

USING_NS_CC;

CursesTileMap::CursesTileMap() : TMXTiledMap()
{
}

void CursesTileMap::setTerminalSize(cocos2d::Vec2 sz)
{
    // max of 80x25
    
    _terminalSize = sz;
}

void CursesTileMap::update(float delta)
{
    if (!isScreenDirty())
        return;
    
    TMXLayer *layer = getLayer("Background");
    char *data = getScreenData();
    
    if (_terminalSize.y == 0 || _terminalSize.x == 0) {
        _terminalSize = Vec2(getmaxx(curscr), getmaxy(curscr));
    }
    
    for(int r=0;r<_terminalSize.y;r++) {
        for(int c=0;c<_terminalSize.x;c++) {
            char ch = data[(r*80) + c];
            
            // shouldn't come to here
            if (ch == -1)
                continue;

            layer->setTileGID(ch + 1, Vec2(c,r));
        }
    }
}