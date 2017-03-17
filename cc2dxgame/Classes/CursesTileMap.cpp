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
    for(int i=0;i<256;i++) {
        tilesetMap[i] = i;
        colorMap[i] = Color3B::BLACK;
    }
    remappedTiles = false;
    layer = 0;
}

CursesTileMap::TileColor* CursesTileMap::getScreenColor()
{
    return (CursesTileMap::TileColor*)(::getScreenColor());
}

bool CursesTileMap::isScreenDirty()
{
    return ::isScreenDirty();
}

char* CursesTileMap::getScreenData(bool fresh)
{
    if (fresh)
        memcpy(screenData, ::getScreenData(), 2000);
    return screenData;
}

void CursesTileMap::setTerminalSize(cocos2d::Size sz)
{
    // max of 80x25
    _terminalSize = sz;
    setMapSize(sz);
}

int CursesTileMap::getGID(int id)
{
    return tilesetMap[id];
}

cocos2d::Color3B CursesTileMap::getColor(int id)
{
    Color3B clrMap = colorMap[id];
    if (clrMap != Color3B::BLACK)
        return clrMap;
    return Color3B::WHITE;
}

void CursesTileMap::draw(char *data)
{
    if (!isVisible())
        return;
    
    if (layer == 0) {
        layer = getLayer("Background");
        layer->getTexture()->setAntiAliasTexParameters();
        layer->getTexture()->setAliasTexParameters();
    }
    
    TileColor *colors = getScreenColor();
    
    if (_terminalSize.width == 0 || _terminalSize.height == 0) {
        setTerminalSize(Size(getmaxx(curscr), getmaxy(curscr)));
    }
    
    int tw = _terminalSize.width;
    
    for(int r=0;r<_terminalSize.height;r++) {
        
        for(int c=0;c<_terminalSize.width;c++) {
                        
            char ch = data[(r*tw) + c];
            int chM = tilesetMap[ch];
            TileColor clr = colors[(r*tw) + c];
            
            if (ch == -1 || ch == ' ')
                ch = 0;
            
            if (layer->getTileGIDAt(Vec2(c,r)) == chM + 1)
                continue;
            
            auto tile = layer->getTileAt(Vec2(c,r));
            if (!tile)
                continue;
            
            tile->setVisible(chM != 0);
            tile->setScale(1.008, 1.008);
            layer->setTileGID(chM + 1, Vec2(c,r));
            
            tile->setOpacity(layer->getOpacity());
            
            Color3B clrMap = colorMap[ch];
            if (clrMap != Color3B::BLACK) {
                tile->setColor(clrMap);
            } else {
                if (clr.r == 0 && clr.g == 0 && clr.b == 0)
                    tile->setColor(Color3B(0xff,0xff,0xff));
                else
                    tile->setColor(Color3B(clr.r, clr.g, clr.b));
            }
        }
    }

}

void CursesTileMap::positionAndScale(MapAlign hAlign, MapAlign vAlign, float scale)
{
    int cw = getTileSize().width;
    int ch = getTileSize().height;
    
    auto visibleSize = Director::getInstance()->getVisibleSize();
    auto winSize = Director::getInstance()->getWinSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();
    
    float scaleUp = (winSize.width) / ((_terminalSize.width + 2) * cw) * Director::getInstance()->getContentScaleFactor();
    float scaleUpY = (winSize.height) / ((_terminalSize.height + 2) * ch) * Director::getInstance()->getContentScaleFactor();
    if (scaleUp > scaleUpY)
        scaleUp = scaleUpY;
    
    if (scale != 0) //scaleUp < scale)
        scaleUp = scale;
    
//    scaleUp = floor(scaleUp);
    
    float tmxH = scaleUp * ch * (_terminalSize.height + 2);
    float tmxW = scaleUp * ch * (_terminalSize.width + 2);
    
    float offX = origin.x + 10;
    float offY = origin.y + winSize.height - 10;
    
    // alignment
    switch(hAlign) {
        case MapAlign::Center:
            offX += (visibleSize.width * 0.5f);
            offX -= (tmxW * 0.5f);
            break;
        case MapAlign::Right:
            offX = origin.y + visibleSize.width - tmxW - 10;
            break;
        default:
            break;
    }
    
    switch(vAlign) {
        case MapAlign::Middle:
            offY -= (visibleSize.height * 0.5f);
            offY += (tmxH * 0.5f);
            break;
        case MapAlign::Bottom:
            offY = origin.y + tmxH + 10;
            break;
        default:
            break;
    }
    
    setAnchorPoint(Vec2(0,1));
    setScale(scaleUp);
    setPosition(offX, offY);
}

std::string CursesTileMap::getStringAtLine(int l)
{
    int tw  =_terminalSize.width;
    char *data = getScreenData();
    data += (l * tw);
    
    int end = tw;
    while(end > 0) {
        if (data[end-1] != ' ')
            break;
        end--;
    }
    
    std::string str(data, end);
//    fprintf(stdout, "%s\n", str.c_str());
    return str;
}

void CursesTileMap::clearAtLine(int l)
{
    int tw  =_terminalSize.width;
    char *data = getScreenData(false);
    data += (l * tw);
    memset(data, -1, sizeof(char)*tw);
}

void CursesTileMap::update(float delta)
{
    if (!isVisible())
        return;
    
    if (isScreenDirty())
        draw(getScreenData());
}
