//
//  CursesTileMap.hpp
//  cc2dxgame
//
//  Created by Marvin Sanchez on 02/01/2017.
//
//

#ifndef CursesTileMap_hpp
#define CursesTileMap_hpp

#include "cocos2d.h"

class CursesTileMap : public cocos2d::TMXTiledMap
{
public:
    
    CursesTileMap();
    virtual void update(float delta);
    
    void setTerminalSize(cocos2d::Size sz);
    
    bool mapping;
private:
    cocos2d::Size _terminalSize;
};

#endif /* CursesTileMap_hpp */
