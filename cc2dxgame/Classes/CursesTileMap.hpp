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
    
    enum MapAlign {
        Left,
        Center,
        Right,
        Top,
        Middle,
        Bottom,
    };
    
    typedef struct {
        unsigned short r;
        unsigned short g;
        unsigned short b;
    } TileColor;
    
    CursesTileMap();
    virtual void update(float delta);
    virtual void draw(char *);
    
    bool isScreenDirty();
    char *getScreenData(bool fresh = true);
    TileColor *getScreenColor();
    
    void setTerminalSize(cocos2d::Size sz);
    int width() { return _terminalSize.width; }
    int height() { return _terminalSize.height; }
    void positionAndScale(MapAlign hAlign, MapAlign vAligns, float scale);
    
    std::string getStringAtLine(int l);
    void clearAtLine(int l);
    
    int getGID(int id);
    cocos2d::Color3B getColor(int id);

protected:
    
    char screenData[2000];
    int tilesetMap[256];
    cocos2d::Color3B colorMap[256];
    bool remappedTiles;
    
protected:
    cocos2d::Size _terminalSize;
    cocos2d::TMXLayer *layer;
};

#endif /* CursesTileMap_hpp */
