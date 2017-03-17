#ifndef __HELLOWORLD_SCENE_H__
#define __HELLOWORLD_SCENE_H__

#include "cocos2d.h"

class GameInterface;
class CursesTileMap;

class HelloWorld : public cocos2d::Scene
{
public:
    
    static cocos2d::Scene* createScene();

    virtual bool init();
    
    void menuCloseCallback(cocos2d::Ref* pSender);
    
    void onKeyPressed(cocos2d::EventKeyboard::KeyCode keyCode, cocos2d::Event* event);
    void onKeyReleased(cocos2d::EventKeyboard::KeyCode keyCode, cocos2d::Event* event);
//    void onMouseDown(cocos2d::EventMouse *event);
//    void onMouseUp(cocos2d::EventMouse *event);
    void onTouchBegan(cocos2d::Touch *touch, cocos2d::Event *event);
    void onTouchEnded(cocos2d::Touch *touch, cocos2d::Event *event);

    void update(float delta);
    
    CursesTileMap *dungeonMap;
    GameInterface *gameUI;
    
    int keysDown[1024];
    
    // implement the "static create()" method manually
    CREATE_FUNC(HelloWorld);
};

#endif // __HELLOWORLD_SCENE_H__
