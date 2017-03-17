//
//  GameInterface.hpp
//  cc2dxgame
//
//  Created by Marvin Sanchez on 19/02/2017.
//
//

#ifndef GameInterface_hpp
#define GameInterface_hpp

#include "cocos2d.h"
#include "GraphicsTileMap.hpp"

namespace cocos2d {
namespace ui {
    class Widget;
    class Button;
}}

class GameInterface : public cocos2d::Node
{
    GraphicsTileMap *dungeon;
    GraphicsTileMap *pathMap;
    std::vector<Node*> buttons;
    
    float scaleFactor;
    
    enum ButtonCommand {
        Search = 1,
        Rest,
        Up,
        Down,
        Inventory,
        Commands,
        Fight,
        Move,
        Throw,
        Zap,
        Quaff,
        Eat,
        PickUp,
        ReadScroll,
        WieldWeapon,
        WearArmour,
        TakeoffArmour,
        PutOnRing,
        RemoveRing,
        DropObject,
        CallObject,
        RepeatCommand,
        PlayerInfo,
        Discoveries,
        InventorySelect,
        InputString,
        ToggleGraphics,
        Identify,
        IdentifyTrap,
        InventoryItem,
        Go,
        Map,
        Version,
        Help,
        Cancel,
        Zoom,
        Exit
    };
    
    struct Command {
        int cmd;
        char text[64];
        int key;
        char icon;
    };
    
    class Sleep {
    public:
        Sleep() {
            sleeping = 0;
        }
        void sleep(float t) {
            sleeping = t;
        }
        void update(float delta) {
            if (isSleeping())
                sleeping -= delta;
            
//            fprintf(stdout, "%f\n", sleeping);
        }
        bool isSleeping() {
            return sleeping > 0;
        }
    private:
        float sleeping;
    };
    
public:
    
    CursesTileMap *curses;
    
    GameInterface(int w, int h);
    void update(float delta);
    
    void press(float x, float y);
    void release(float x, float y);
    
    static void pushKey(int key, bool shiftPressed, bool controlPressed);
    static int mapKey(int key, bool shiftPressed, bool controlPressed);
    
    cocos2d::ui::Button* createButton(std::string text, int tag);
    cocos2d::Label *createLabel(std::string text);
    
    void deadMode();
    bool isDeadMode() { return _dead; }
    
protected:
    
    bool isDungeon();
    
    char getIconFromString(std::string text);
    void queryInventory(Command *cmd);
    void showPlayerInfo();
    void showInventory();
    void showCommands();
    
    void showMap();
    void showMessage(std::string message);
    void showMenu(Command *commands);
    void showInventorySelect(char key);
    void showDiscoveries();
    void showKeyboard();
    void hideKeyboard();
    void consumeStringInput(std::string s);
    
    void cancelPendingCommand();
    
    void buttonCallback(cocos2d::Ref* pSender);
    
private:
    
    bool _dead;
    
    cocos2d::Node *keyboard;
    cocos2d::Node *toolbar;
    cocos2d::ui::Button *go;
    cocos2d::Label *statsLabel;
    cocos2d::Label *messageLabel;
    cocos2d::ui::Scale9Sprite *messageFrame;
    cocos2d::ui::Scale9Sprite *menuFrame;
    cocos2d::ui::Scale9Sprite *hudFrame;
    cocos2d::ui::Scale9Sprite *messageLabelFrame;
    
    void findPath(float x, float y);
    
    float pressX;
    float pressY;
    float releaseX;
    float releaseY;
    
    bool showGraphics;
    bool waitingForString;
    bool waitingForSpace;
    bool waitingForIdentify;
    bool keyboardShift;
    
    int messageLine;
    int statsLine;
    
    cocos2d::Vec2 lastTarget;
    
    void showPath();
    void clearPath();
    void consumePath();
    
    void updateZoom();
    void scaleAndPositionPathMap(CursesTileMap *map);
    
    std::vector<void *> path;
    Command inventoryItems[32];
    
    Sleep sleep;
    int pathError;
    int zoomIndex;

};

#endif /* GameInterface_hpp */
