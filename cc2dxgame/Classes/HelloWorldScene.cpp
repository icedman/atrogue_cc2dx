#include "HelloWorldScene.h"
#include "SimpleAudioEngine.h"
#include "CursesTileMap.hpp"

extern "C" {
    int atrogue_main(int argc, const char ** argv);
    void pushKey(int k);
}

#include <pthread.h>
pthread_t gameThread;

extern char *getScreenData();

void* gameThreadRoutine(void *arg)
{
    const char *argv[] = {
        "atrogue",
        "--scr-width=60",
        "--scr-height=25",
        "--sec-width=60",
        "--sec-height=25",
    };
    atrogue_main(5,argv);
    return 0;
}

int dungeon_main()
{
    pthread_create(&gameThread, 0, &gameThreadRoutine, (void*)"");
    return 0;
}


USING_NS_CC;

Scene* HelloWorld::createScene()
{
    return HelloWorld::create();
}

// on "init" you need to initialize your instance
bool HelloWorld::init()
{
    //////////////////////////////
    // 1. super init first
    if ( !Scene::init() )
    {
        return false;
    }
    
    auto visibleSize = Director::getInstance()->getVisibleSize();
    auto winSize = Director::getInstance()->getWinSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();

    auto keyListener = EventListenerKeyboard::create();
    keyListener->onKeyReleased = CC_CALLBACK_2(HelloWorld::onKeyReleased, this);
    keyListener->onKeyPressed = CC_CALLBACK_2(HelloWorld::onKeyPressed, this);
    Director::getInstance()->getEventDispatcher()->addEventListenerWithSceneGraphPriority(keyListener, this);

    dungeonMap = new CursesTileMap();
    dungeonMap->initWithTMXFile("blank.tmx");
    dungeonMap->setTerminalSize(Vec2(60,25));
    this->addChild(dungeonMap);
    
    float scaleUp = (winSize.width) / (84 * 8) * Director::getInstance()->getContentScaleFactor();
    float scaleUpY = (winSize.height) / (41 * 8) * Director::getInstance()->getContentScaleFactor();
    if (scaleUp > scaleUpY)
        scaleUp = scaleUpY;
    
    //    scaleUp = 1.0f;
    
    float offX = ((((winSize.width) / (8.0 * scaleUp)) - 80) * 8.0 * scaleUp) * 0.5f;
    
    dungeonMap->setAnchorPoint(Vec2(0,1));
    dungeonMap->setPosition(offX, origin.y + visibleSize.height - 10);
    dungeonMap->setScale(scaleUp);
    
    dungeon_main();
    
    memset(keysDown,0,sizeof(keysDown));
    
    this->scheduleUpdate();
    return true;
}


void HelloWorld::menuCloseCallback(Ref* pSender)
{
    //Close the cocos2d-x game scene and quit the application
    Director::getInstance()->end();

    #if (CC_TARGET_PLATFORM == CC_PLATFORM_IOS)
    exit(0);
#endif
    
    /*To navigate back to native iOS screen(if present) without quitting the application  ,do not use Director::getInstance()->end() and exit(0) as given above,instead trigger a custom event created in RootViewController.mm as below*/
    
    //EventCustom customEndEvent("game_scene_close_event");
    //_eventDispatcher->dispatchEvent(&customEndEvent);
    
    
}

void HelloWorld::onKeyPressed(EventKeyboard::KeyCode keyCode, Event* event)
{
    keysDown[(int)keyCode] = 0xff;
}

void HelloWorld::onKeyReleased(EventKeyboard::KeyCode keyCode, Event* event)
{
    keysDown[(int)keyCode] = 0;
    int k = (mapKey((int)keyCode));
    pushKey(k);
}

int HelloWorld::mapKey(int k) {
    
    bool isShifting = (keysDown[(int)EventKeyboard::KeyCode::KEY_LEFT_SHIFT] == 0xff ||
                       keysDown[(int)EventKeyboard::KeyCode::KEY_RIGHT_SHIFT] == 0xff);
    
    // special keys
    switch (k)
    {
        case (int)EventKeyboard::KeyCode::KEY_UP_ARROW:
            return 'k';
        case (int)EventKeyboard::KeyCode::KEY_DOWN_ARROW:
            return 'j';
        case (int)EventKeyboard::KeyCode::KEY_LEFT_ARROW:
            return 'h';
        case (int)EventKeyboard::KeyCode::KEY_RIGHT_ARROW:
            return 'l';
        case (int)EventKeyboard::KeyCode::KEY_ENTER:
            return '\015';
        case (int)EventKeyboard::KeyCode::KEY_ESCAPE:
            return '\033';
        case (int)EventKeyboard::KeyCode::KEY_SPACE:
            return ' ';
        case (int)EventKeyboard::KeyCode::KEY_COMMA:
            if (isShifting)
                return '<';
            return ',';
        case (int)EventKeyboard::KeyCode::KEY_PERIOD:
            if (isShifting)
                return '>';
            return '.';
        case (int)EventKeyboard::KeyCode::KEY_SEMICOLON:
            if (isShifting)
                return ':';
            return ';';
        case (int)EventKeyboard::KeyCode::KEY_APOSTROPHE:
            return '\'';
        case (int)EventKeyboard::KeyCode::KEY_LEFT_BRACKET:
            if (isShifting)
                return '{';
            return '[';
        case (int)EventKeyboard::KeyCode::KEY_RIGHT_BRACKET:
            if (isShifting)
                return '}';
            return ']';
        case (int)EventKeyboard::KeyCode::KEY_SLASH:
            if (isShifting)
                return '?';
            return '/';
        case (int)EventKeyboard::KeyCode::KEY_BACK_SLASH:
            if (isShifting)
                return '|';
            return '\\';
        case (int)EventKeyboard::KeyCode::KEY_MINUS:
            if (isShifting)
                return '_';
            return '-';
        case (int)EventKeyboard::KeyCode::KEY_EQUAL:
            if (isShifting)
                return '+';
            return '=';
            
    }
    
    // numeric
    if (k >= (int)EventKeyboard::KeyCode::KEY_0 && k <= (int)EventKeyboard::KeyCode::KEY_9) {
        k = k - ((int)EventKeyboard::KeyCode::KEY_1 - '1');
        if (isShifting) {
            
            switch(k) {
                case '0': k =')'; break;
                case '1': k ='!'; break;
                case '2': k ='@'; break;
                case '3': k ='#'; break;
                case '4': k ='$'; break;
                case '5': k ='%'; break;
                case '6': k ='^'; break;
                case '7': k ='&'; break;
                case '8': k ='*'; break;
                case '9': k ='('; break;
            }
        }
        return k;
    }
    
    // alpha
    if (k >= (int)EventKeyboard::KeyCode::KEY_A && k <= (int)EventKeyboard::KeyCode::KEY_Z) {
        k = k - ((int)EventKeyboard::KeyCode::KEY_A - 'a');
        if (isShifting) {
            k = k - ('a' - 'A');
        }
        return k;
    }
    
    return -1;
}

void HelloWorld::update(float delta)
{
    dungeonMap->update(delta);
}




