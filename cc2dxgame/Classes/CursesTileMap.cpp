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

void CursesTileMap::setTerminalSize(cocos2d::Size sz)
{
    // max of 80x25
    _terminalSize = sz;
    setMapSize(sz);
}

void CursesTileMap::update(float delta)
{
    if (!isScreenDirty())
        return;
    
    TMXLayer *layer = getLayer("Background");
    char *data = getScreenData();
    
    if (_terminalSize.width == 0 || _terminalSize.height == 0) {
        setTerminalSize(Size(getmaxx(curscr), getmaxy(curscr)));
    }
    
    for(int r=0;r<_terminalSize.height;r++) {
        for(int c=0;c<_terminalSize.width;c++) {
            char ch = data[(r*80) + c];
            
            // shouldn't come to here
            if (ch == -1)
                continue;

            layer->setTileGID(ch + 1, Vec2(c,r));
        }
    }
}