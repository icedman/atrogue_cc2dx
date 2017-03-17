//
//  GraphicsTileMap.cpp
//  cc2dxgame
//
//  Created by Marvin Sanchez on 19/02/2017.
//
//

#include "GraphicsTileMap.hpp"

USING_NS_CC;

extern "C" {
    int what_thing(int y, int x);
}

GraphicsTileMap::GraphicsTileMap() : CursesTileMap()
{}

void GraphicsTileMap::centerAt()
{
    char *data = getScreenData(false);
    
    int tw = _terminalSize.width;
    
    int cw = getTileSize().width;
    int ch = getTileSize().height;
    float scale = getScale();
    
    auto visibleSize = Director::getInstance()->getVisibleSize();
    auto winSize = Director::getInstance()->getWinSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();
    
    float cX = 0;
    float cY = 0;
    
    for(int r=0;r<_terminalSize.height;r++) {
        for(int c=0;c<_terminalSize.width;c++) {
            char cc = data[(r*tw) + c];
            if (cc == '@') {
                cX = (c * cw * scale);
                cY = ((r + 1) * ch * scale);
                playerLocation = Vec2(c, r);
                break;
            }
        }
    }

    float tmxH = (_terminalSize.height * ch * scale);
    
    setAnchorPoint(Vec2(0,0));
    Vec2 pos = Vec2(-cX + (winSize.width/2), -(tmxH-cY) + (winSize.height/2));
    setPosition(pos);
    
    /*
    this->stopAllActions();
    this->runAction(MoveTo::create(0.2, pos));
     */
}

void GraphicsTileMap::loadTileset()
{
    initWithTMXFile("blank_scrollo.tmx");

    memset(tilesetMap, 0, sizeof(tilesetMap));
    
    // scroll-o-sprites & rogue5.4 mapping
    
    tilesetMap['.'] = 525; // floor
    tilesetMap['#'] = 526; // floor path
    tilesetMap['-'] = 562; //522; // wall
    tilesetMap['|'] = 563; //522; // wall
    
    // modify!
    tilesetMap['0'] = 565; // corner wall
    tilesetMap['1'] = 566; // corner wall
    tilesetMap['2'] = 567; // corner wall
    tilesetMap['3'] = 568; // corner wall
    tilesetMap['4'] = 911; // bow
    tilesetMap['5'] = 912; // arrows
    tilesetMap['6'] = 908; // mace
    tilesetMap['8'] = 905; // spear
    
    tilesetMap['+'] = 534; // door
    tilesetMap['%'] = 532; // staircase
    
    tilesetMap[':'] = 822; // food
    tilesetMap['!'] = 845; // potion
    tilesetMap[']'] = 926; // armor // 913-helmet
    tilesetMap[')'] = 903; // weapon
    tilesetMap['/'] = 983; // wand or staf
    tilesetMap['='] = 929; // ring
    tilesetMap['*'] = 755; // gold
    tilesetMap['?'] = 988; // scroll
    tilesetMap['^'] = 622; // trap
    tilesetMap['$'] = 985; // magic
    tilesetMap[','] = 930; // amulet
    
    tilesetMap['@'] = 124; // knight
    
    // monsters
    tilesetMap['A'] = 287; // aquator
    tilesetMap['B'] = 283; // bat
    tilesetMap['C'] = 405; // centaur
    tilesetMap['D'] = 293; // dragon
    tilesetMap['E'] = 282; // emu
    tilesetMap['F'] = 463; // venus flytrap
    tilesetMap['G'] = 292; // griffin
    tilesetMap['H'] = 342; // hobgoblin
    tilesetMap['I'] = 462; // ice monster
    tilesetMap['J'] = 163; // jabberwock
    tilesetMap['K'] = 346; // kestrel   // colorize
    tilesetMap['L'] = 465; // leprechaun
    tilesetMap['M'] = 194; // medusa
    tilesetMap['N'] = 408; // nymph
    tilesetMap['O'] = 344; // orc
    tilesetMap['P'] = 409; // phantom
    tilesetMap['Q'] = 289; // quagga
    tilesetMap['R'] = 284; // rattlesnake // colorize!
    tilesetMap['S'] = 288; // snake
    tilesetMap['T'] = 343; // troll
    tilesetMap['U'] = 294; // black unicorn
    tilesetMap['V'] = 165; // vampire
    tilesetMap['W'] = 409; // wraith
    tilesetMap['X'] = 467; // xroc
    tilesetMap['Y'] = 291; // yeti
    tilesetMap['Z'] = 402; // zombie
    
    // color map
    colorMap['.'] = Color3B(0x40,0x40,0x40);
    colorMap['#'] = Color3B(0x60,0x40,0x00);
    
    colorMap['*'] = Color3B::YELLOW;
    
    for(int i='A';i<='Z';i++) {
        colorMap[i] = Color3B::RED;
    }
    
    colorMap[')'] = Color3B::MAGENTA;
    colorMap[']'] = Color3B::MAGENTA;
        colorMap['4'] = Color3B::MAGENTA;
        colorMap['5'] = Color3B::MAGENTA;
        colorMap['6'] = Color3B::MAGENTA;
        colorMap['8'] = Color3B::MAGENTA;
    
    colorMap['?'] = Color3B::MAGENTA;
    colorMap['!'] = Color3B::MAGENTA;
    colorMap['/'] = Color3B::MAGENTA;
    colorMap[':'] = Color3B::MAGENTA;
    colorMap['$'] = Color3B::MAGENTA;
    colorMap[','] = Color3B::YELLOW;
    
    colorMap['^'] = Color3B(0x50,0xff,0x55);
    colorMap['%'] = Color3B(0x50,0xff,0x55);
    
    colorMap['+'] = Color3B::ORANGE;
    colorMap['-'] = Color3B(0x0,0xff,0xff);
    colorMap['|'] = colorMap['-'];
    colorMap['0'] = colorMap['-'];
    colorMap['1'] = colorMap['-'];
    colorMap['2'] = colorMap['-'];
    colorMap['3'] = colorMap['-'];
    
    colorMap['@'] = Color3B(0xff,0xff,0xaa);
    
    remappedTiles = true;
    
    // atrogue
#ifdef ATROGUE
    for(int i='A'; i<='Z';i++) {
        int cc = i - 'A' + 'a';
        tilesetMap[cc] = tilesetMap[i];
    }
    tilesetMap['*'] = 755; // stone
    tilesetMap['%'] = 755; // gold
    tilesetMap['<'] = 533; // up
    tilesetMap['>'] = 532; // down
#endif
}

char GraphicsTileMap::getCharAt(char *data, int r, int c) {
    int tw = _terminalSize.width;
    if (r < 0 || r >= _terminalSize.height)
        return '?';
    if (c < 0 || c >= _terminalSize.width)
        return '?';
    return data[(r*tw) + c];
}

bool isWall(char c, char c2) {
    return c == '-' || c == '|' || c == '+' || (c == '@' && c2 == '|');
}

/*
 #define MACE		0
 #define SWORD		1
 #define BOW		2
 #define ARROW		3
 #define DAGGER		4
 #define TWOSWORD	5
 #define DART		6
 #define SHIRAKEN	7
 #define SPEAR		8
 #define FLAME		9
*/

void GraphicsTileMap::modifyWeaponTiles(char *data)
{
    int tw = _terminalSize.width;
    for(int r=0;r<_terminalSize.height;r++) {
        for(int c=0;c<_terminalSize.width;c++) {
            
            char cc = data[(r*tw) + c];
            
            if (cc != ')')
                continue;
            
            int wt = what_thing(r, c);
            switch (wt) {
                case 0: // mace
                    cc = '6';
                    break;
                case 2: // bow
                    cc = '4';
                    break;
                case 3: // arrow
                    cc = '5';
                    break;
                default: // 1 (sword
                    break;
            }
            
            data[(r*tw) + c] = cc;
            
//            fprintf(stdout, "%d\n", wt);
        }
    }
}

void GraphicsTileMap::modifyCornerTiles(char *data)
{
    int tw = _terminalSize.width;
    for(int r=0;r<_terminalSize.height;r++) {
        for(int c=0;c<_terminalSize.width;c++) {
            
            char cc = data[(r*tw) + c];
            
            if (cc != '-')
                continue;
            
            char left = getCharAt(data, r, c-1);
            char right = getCharAt(data, r, c+1);
            char up = getCharAt(data, r-1, c);
            char down = getCharAt(data, r+1, c);
            
//            char left2 = getCharAt(data, r, c-2);
//            char right2 = getCharAt(data, r, c+2);
            char up2 = getCharAt(data, r-2, c);
            char down2 = getCharAt(data, r+2, c);
            
            if (!isWall(left,'|') && isWall(right,'|') && isWall(down,down2)) {
                cc = '0';
            } else if (isWall(left,'|') && !isWall(right,'|') && isWall(down,down2)) {
                cc = '1';
            } else if (!isWall(left,'|') && isWall(right,'|') && isWall(up,up2)) {
                cc = '2';
            } else if (isWall(left,'|') && !isWall(right,'|') && isWall(up,up2)) {
                cc = '3';
            }
            
            data[(r*tw) + c] = cc;
        }
    }
}


float GraphicsTileMap::LeastCostEstimate( void* nodeStart, void* nodeEnd )
{
    int xStart, yStart, xEnd, yEnd;
    NodeToXY( nodeStart, &xStart, &yStart );
    NodeToXY( nodeEnd, &xEnd, &yEnd );
    return Vec2(xStart, yStart).distance(Vec2(xEnd,yEnd));
}

void GraphicsTileMap::AdjacentCost( void* state, std::vector< micropather::StateCost > *adjacent )
{
    char *data = getScreenData(false);
    int tw = _terminalSize.width;
    
    int x, y;
    NodeToXY( state, &x, &y );
    
    char left = getCharAt(data, y, x-1);
    char right = getCharAt(data, y, x+1);
    char up = getCharAt(data, y-1, x);
    char down = getCharAt(data, y+1, x);
    
    bool allowDiagonal = true;
    
    if (left != '.' && left != '#' && right != '.' && right != '#') {
        allowDiagonal = false;
    }
    if (up != '.' && up != '#' && down != '.' && down != '#') {
        allowDiagonal = false;
    }
    
    for(int r=0;r<3;r++) {
        for(int c=0;c<3;c++) {
            
            bool isDiagonal = ((c == 0 || c == 2) && (r == 0 || r == 2));
            
            if (!allowDiagonal && isDiagonal) {
                continue;
            }

            int xx = x-1+c;
            int yy = y-1+r;
            
            if (xx < 0 || xx >= _terminalSize.width)
                continue;
            if (yy < 0 || yy >= _terminalSize.height)
                continue;
            
            char ch = data[(yy*tw) + xx];
            if (ch == '-' || ch == '|' || ch == '@' || ch == ' ' || ch == 0  || ch == -1)
                continue;
            
            char left = getCharAt(data, yy, xx-1);
            char right = getCharAt(data, yy, xx+1);
            char up = getCharAt(data, yy-1, xx);
            char down = getCharAt(data, yy+1, xx);
            
            if ((c == 0 && r == 0) || (c == 2 && r == 0) ||
                (c == 0 && r == 2) || (c == 2 && r == 2)) {
                if (left != '.' && left != '#' && right != '.' && right != '#') {
                    continue;
                }
                if (up != '.' && up != '#' && down != '.' && down != '#') {
                    continue;
                }
            }
            
            
            micropather::StateCost nodeCost = { XYToNode( xx, yy ), 1 };
            if (isDiagonal)
                nodeCost.cost = 1.5;
            adjacent->push_back( nodeCost );
            
        }
    }
}

void GraphicsTileMap::PrintStateInfo( void* state )
{}

void GraphicsTileMap::NodeToXY( void* node, int* x, int* y )
{
    long index = (long)node;
    *y = index / _terminalSize.width;
    *x = index - *y * _terminalSize.width;
}

void* GraphicsTileMap::XYToNode( int x, int y )
{
    return (void*)(long)( y*_terminalSize.width + x );
}
