#include "HelloWorldScene.h"
#include "SimpleAudioEngine.h"

#include "CursesTileMap.hpp"
#include "GameInterface.hpp"

extern "C" {
    int rogue_main(int argc, const char ** argv);
    int is_rogue_running();
    void setUpdateConsumers(int c);
}

#include <pthread.h>
pthread_t gameThread;

extern char *getScreenData();

// don't change!
#define TERMINAL_WIDTH 80
#define TERMINAL_HEIGHT 25

void* gameThreadRoutine(void *arg)
{
    const char *argv[] = {
        "rogue",
        "--scr-width=80",
        "--scr-height=25",
        "--sec-width=80",
        "--sec-height=25",
    };
    
    rogue_main(5,(const char**)argv);
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
    
#if (CC_TARGET_PLATFORM != CC_PLATFORM_IOS)
#if (CC_TARGET_PLATFORM != CC_PLATFORM_ANDROID)
    auto keyListener = EventListenerKeyboard::create();
    keyListener->onKeyReleased = CC_CALLBACK_2(HelloWorld::onKeyReleased, this);
    keyListener->onKeyPressed = CC_CALLBACK_2(HelloWorld::onKeyPressed, this);
    Director::getInstance()
        ->getEventDispatcher()
        ->addEventListenerWithSceneGraphPriority(keyListener, this);
#endif
#endif
    
    auto touchListener = EventListenerTouchOneByOne::create();
    touchListener->setSwallowTouches(false);
    touchListener->onTouchBegan = [](Touch *touch, Event *event) {
        ((HelloWorld*)event->getCurrentTarget())->onTouchBegan(touch, event);
        return true;
    };
    touchListener->onTouchEnded = [](Touch *touch, Event *event) {
        ((HelloWorld*)event->getCurrentTarget())->onTouchEnded(touch, event);
        return true;
    };
    Director::getInstance()
        ->getEventDispatcher()
        ->addEventListenerWithSceneGraphPriority(touchListener, this);


    dungeonMap = new CursesTileMap();
    dungeonMap->initWithTMXFile("blank.tmx");
    dungeonMap->setTerminalSize(Size(TERMINAL_WIDTH,TERMINAL_HEIGHT));
    dungeonMap->positionAndScale(CursesTileMap::MapAlign::Center, CursesTileMap::MapAlign::Middle, 0);
    this->addChild(dungeonMap);
    
#if 0
    gameUI = 0;
#else
    dungeonMap->setVisible(false);
    dungeonMap->positionAndScale(CursesTileMap::MapAlign::Right, CursesTileMap::MapAlign::Top, 1.0f);
    dungeonMap->setPosition( dungeonMap->getPosition() + Vec2(0, -50));
    
    gameUI = new GameInterface(TERMINAL_WIDTH,TERMINAL_HEIGHT);
    gameUI->curses = dungeonMap;
    this->addChild(gameUI);
#endif
    
    setUpdateConsumers(4);
    dungeon_main();
    
    memset(keysDown,0,sizeof(keysDown));
    
    this->scheduleUpdate();
    
    // Director::getInstance()->getOpenGLView()->setIMEKeyboardState(true);
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
    bool shiftPressed = (keysDown[(int)EventKeyboard::KeyCode::KEY_LEFT_SHIFT] == 0xff ||
                       keysDown[(int)EventKeyboard::KeyCode::KEY_RIGHT_SHIFT] == 0xff);
    
    bool controlPressed = (keysDown[(int)EventKeyboard::KeyCode::KEY_LEFT_CTRL] == 0xff ||
                          keysDown[(int)EventKeyboard::KeyCode::KEY_RIGHT_CTRL] == 0xff);
    GameInterface::pushKey((int)keyCode, shiftPressed, controlPressed);
}
/*
void HelloWorld::onMouseUp(EventMouse *event)
{
    if (gameUI)
        gameUI->release(event->getCursorX(), event->getCursorY());
}

void HelloWorld::onMouseDown(EventMouse *event)
{
    if (gameUI)
        gameUI->press(event->getCursorX(), event->getCursorY());
}
*/
void HelloWorld::onTouchBegan(Touch *touch, Event *event)
{
    auto touchLocation = touch->getLocation();
    if (gameUI)
        gameUI->press(touchLocation.x, touchLocation.y);
}

void HelloWorld::onTouchEnded(Touch *touch, Event *event)
{
    auto touchLocation = touch->getLocation();
    if (gameUI)
        gameUI->release(touchLocation.x, touchLocation.y);
}

void HelloWorld::update(float delta)
{
    if (is_rogue_running() == 0) {
        if (!gameUI->isDeadMode()) {
            gameUI->deadMode();
            dungeonMap->setVisible(true);
            dungeonMap->positionAndScale(CursesTileMap::MapAlign::Center, CursesTileMap::MapAlign::Middle, 0);
            dungeonMap->getLayer("Background")->setOpacity(255);
            dungeonMap->update(delta);
        }
        return;
    }
    
    dungeonMap->update(delta);
    if (gameUI)
        gameUI->update(delta);
    
    // dead?
}

