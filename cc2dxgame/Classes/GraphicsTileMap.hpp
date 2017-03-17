//
//  GraphicsTileMap.hpp
//  cc2dxgame
//
//  Created by Marvin Sanchez on 19/02/2017.
//
//

#ifndef GraphicsTileMap_hpp
#define GraphicsTileMap_hpp

#include "CursesTileMap.hpp"
#include "micropather.h"

class GraphicsTileMap : public CursesTileMap, public micropather::Graph
{
public:
    
    GraphicsTileMap();
    void loadTileset();
    
    void centerAt();
    
    char getCharAt(char *data, int r, int c);
    void modifyCornerTiles(char *data);
    void modifyWeaponTiles(char *data);
    
    // graph
    float LeastCostEstimate( void* stateStart, void* stateEnd );
    void AdjacentCost( void* state, std::vector< micropather::StateCost > *adjacent );
    void PrintStateInfo( void* state );
    
    void NodeToXY( void* node, int* x, int* y );
    void* XYToNode( int x, int y );
    
    cocos2d::Vec2 playerLocation;
};

#endif /* GraphicsTileMap_hpp */
