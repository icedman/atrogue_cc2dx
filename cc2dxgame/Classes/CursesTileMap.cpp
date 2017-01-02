//
//  CursesTileMap.cpp
//  cc2dxgame
//
//  Created by Marvin Sanchez on 02/01/2017.
//
//

#include "CursesTileMap.hpp"
#include "pdccc2dx.h"

extern "C" {
    bool isScreenDirty();
    char *getScreenData();
    t_pdc_color *getScreenColor();
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
    t_pdc_color *colors = getScreenColor();
    
    if (_terminalSize.width == 0 || _terminalSize.height == 0) {
        setTerminalSize(Size(getmaxx(curscr), getmaxy(curscr)));
    }
    
    for(int r=0;r<_terminalSize.height;r++) {
        for(int c=0;c<_terminalSize.width;c++) {
            char ch = data[(r*80) + c];
            t_pdc_color clr = colors[(r*80) + c];
            
            // shouldn't come to here
            if (ch == -1)
                continue;

            layer->setTileGID(ch + 1, Vec2(c,r));
            auto tile = layer->getTileAt(Vec2(c,r));
            
//            CCLOG("%d %d %d", clr.r, clr.g, clr.b);
            
            if (clr.r == 0 && clr.g == 0 && clr.b == 0)
                tile->setColor(Color3B(0xff,0xff,0xff));
            else
                tile->setColor(Color3B(clr.r, clr.g, clr.b));
        }
    }
}