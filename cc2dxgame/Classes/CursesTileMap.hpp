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
    
    void setTerminalSize(cocos2d::Vec2 sz);
    
private:
    cocos2d::Vec2 _terminalSize;
};

#endif /* CursesTileMap_hpp */
