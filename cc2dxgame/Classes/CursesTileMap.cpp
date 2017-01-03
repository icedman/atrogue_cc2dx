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
    mapping = false;
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
    
    static int tilesetMap[1024];
    
    if (mapping && tilesetMap['.'] != 51) {
        memset(tilesetMap, 0, sizeof(tilesetMap));
        
        // scroll-o-sprites
        
        tilesetMap['.'] = 549; // floor
        tilesetMap['#'] = 549; // floor
        tilesetMap['-'] = 522; // wall
        tilesetMap['|'] = 522; // wall
        tilesetMap['+'] = 534; // door
        tilesetMap['>'] = 532; // stairs
        tilesetMap['<'] = 533; // stairs
        
        
        tilesetMap[':'] = 822; // food
        tilesetMap['!'] = 845; // potion
        tilesetMap['{'] = 748; // chest
        
        tilesetMap['['] = 913; // armor
        tilesetMap[')'] = 903; // weapon
        tilesetMap['/'] = 911; // long bow
        tilesetMap['='] = 929; // ring
        tilesetMap['%'] = 532; // stones << ? hole in the ground (for rogue)
        tilesetMap['*'] = 755; // gold
        tilesetMap['?'] = 988; // scroll
        
        tilesetMap['@'] = 124; // knight
        
        // monsters
        tilesetMap['e'] = 294; // emu
        tilesetMap['E'] = 294; // emu
        tilesetMap['B'] = 283; // bat
        tilesetMap['b'] = 283; // bat
        tilesetMap['K'] = 289; // kestrel
        tilesetMap['k'] = 289; // kestrel
        tilesetMap['S'] = 288; // snake
        tilesetMap['s'] = 288; // snake
        tilesetMap['H'] = 342; // hobgoblin
        tilesetMap['h'] = 342; // hobgoblin
        
#if 0 
        // jerom tiles
        
        tilesetMap['.'] = 50; // floor
        tilesetMap['#'] = 51; // floor
        tilesetMap['-'] = 12; // wall
        tilesetMap['|'] = 12; // wall
        tilesetMap['+'] =  2; // door
        tilesetMap['>'] = 13; // stairs
        tilesetMap['<'] = 14; // stairs
        
        
        tilesetMap[':'] =  96; // food
        tilesetMap['!'] =  97; // potion
        tilesetMap['{'] =  84; // chest
        
        tilesetMap['['] = 167; // armor
        tilesetMap[')'] = 132; // weapon
        tilesetMap['='] =  92; // ring
        tilesetMap['*'] =  90; // stones
        tilesetMap['%'] =  91; // bag of gold << ? hole in the ground (for rogue)
        tilesetMap['?'] = 121; // scroll
        
        tilesetMap['@'] = 195; // knight
        
        // monsters
        tilesetMap['e'] = 223; // emu
        tilesetMap['E'] = 223; // emu
        tilesetMap['B'] = 211; // bat
        tilesetMap['b'] = 211; // bat
        tilesetMap['K'] = 212; // kestrel
        tilesetMap['k'] = 212; // kestrel
        tilesetMap['S'] = 213; // snake
        tilesetMap['s'] = 213; // snake
        tilesetMap['R'] = 213; // rattle snake
        tilesetMap['r'] = 213; // rattle snake
        tilesetMap['H'] = 201; // hobgoblin
        tilesetMap['h'] = 201; // hobgoblin
#endif
    }

    
    for(int r=0;r<_terminalSize.height;r++) {
        
        if (r >= layer->getLayerSize().height)
            break;
        
        for(int c=0;c<_terminalSize.width;c++) {
            
            if (c >= layer->getLayerSize().width)
                break;
            
            char ch = data[(r*80) + c];
            t_pdc_color clr = colors[(r*80) + c];
            
            int chM = tilesetMap[ch];
            
            // shouldn't come to here
            if (ch == -1)
                continue;
            
            auto tile = layer->getTileAt(Vec2(c,r));
            
            if (mapping) {
                
                if (!tile)
                    continue;
                
                if (r == 0)
                    chM = 0;
                
                tile->setVisible(chM != 0);
//                tile->setScale(1.1f);
                layer->setTileGID(chM + 1, Vec2(c,r));
                
            } else {
                layer->setTileGID(ch + 1, Vec2(c,r));
            }
            
//            CCLOG("%d %d %d", clr.r, clr.g, clr.b);
            
            if (clr.r == 0 && clr.g == 0 && clr.b == 0)
                tile->setColor(Color3B(0xff,0xff,0xff));
            else
                tile->setColor(Color3B(clr.r, clr.g, clr.b));
        }
    }
}