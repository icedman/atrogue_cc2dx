//
//  GameInterface.cpp
//  cc2dxgame
//
//  Created by Marvin Sanchez on 19/02/2017.
//
//

#include "GameInterface.hpp"
#include "GraphicsTileMap.hpp"
#include "ui/CocosGUI.h"

#include "micropather.h"

extern "C" {
    void pushKey(int k);
}

USING_NS_CC;

#define HELP_STRING "Movement:\nTap twice on the tile you wish to move into or\
\ntap twice on the direction you wish to explore."

#define MESSAGE_FADE_DELAY 1.5f
#define FRAME_OPACITY 220
#define PUSH_KEY_WAIT 5

GameInterface::GameInterface(int w, int h)
{
    zoomIndex = 0;
    _dead = false;
    keyboardShift = false;
    showGraphics = true;
    
    Size winSize = Director::getInstance()->getWinSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();
    
    scaleFactor = Director::getInstance()->getContentScaleFactor();
    float sf2 = scaleFactor = (winSize.width / (32 * 32));
    if (scaleFactor < sf2)
        scaleFactor = sf2;

    // setup dungeon
    dungeon = new GraphicsTileMap();
    dungeon->loadTileset();
    dungeon->setTerminalSize(Size(w,h));
    
    pathMap = new GraphicsTileMap();
    pathMap->loadTileset();
    pathMap->setTerminalSize(Size(w,h));
    
    updateZoom();
    
    this->addChild(dungeon);
    this->addChild(pathMap);
    
    statsLabel = createLabel(" ");
    statsLabel->setAnchorPoint(Vec2(0,0));
    statsLabel->setPosition(Vec2(20,4));
    
    auto hudBg = ui::Scale9Sprite::create("frame.png");
    hudBg->addChild(statsLabel);
    hudBg->setContentSize(Size(winSize.width - 20, statsLabel->getContentSize().height * 1.2f ));
    hudBg->setAnchorPoint(Vec2(0,1));
    hudBg->setPosition(Vec2(origin.x + 10,origin.y + winSize.height - 10));
    hudBg->getTexture()->setAliasTexParameters();
    hudBg->setOpacity(FRAME_OPACITY);
    hudBg->setColor(Color3B::WHITE);
    statsLabel->setTextColor(Color4B(0x0,0x0,0x0,0xff));
    this->addChild(hudBg);
    
    hudFrame = hudBg;
    
    messageLabel = createLabel("");
    messageLabel->setPosition(Vec2(16,0));
    messageLabel->setAnchorPoint(Vec2(0,0));
    
    messageLabelFrame = ui::Scale9Sprite::create("frame.png");
    messageLabelFrame->setAnchorPoint(Vec2(0,0));
    messageLabelFrame->setContentSize(Size(winSize.width - 20, statsLabel->getContentSize().height * 1.2f ));
    messageLabelFrame->setOpacity(FRAME_OPACITY);
    messageLabelFrame->setColor(Color3B(0x20,0x20,0x20));
    this->addChild(messageLabelFrame);
    messageLabelFrame->addChild(messageLabel);
    
    // ROGUE
    messageLine = 0;
    statsLine = 23;
    
#ifdef ATROGUE
    messageLine = 23;
    statsLine = 24;
#endif
    
    toolbar = ui::Scale9Sprite::create("frame.png");
    this->addChild(toolbar);
    
    int spacing = 4;
    ui::Button *prevBtn;
    ui::Button *btn;
    {
        btn = (ui::Button*)createButton("Search", (int)ButtonCommand::Search);
        btn->setPosition(Vec2(spacing + 6, spacing));
        prevBtn = btn;
        toolbar->addChild(btn);
    }
    
    Size tbs = btn->getContentSize();
    tbs.height += 8;
    tbs.width = winSize.width - 20;
    toolbar->setContentSize(tbs);
    toolbar->setAnchorPoint(Vec2(0,0));
    toolbar->setPosition(Vec2(10,4));
    toolbar->setColor(Color3B::GRAY);
    toolbar->setOpacity(FRAME_OPACITY);
    
    {
        btn = (ui::Button*)createButton(" Rest ", (int)ButtonCommand::Rest);
        btn->setPosition(prevBtn->getPosition() + Vec2( prevBtn->getContentSize().width + spacing, 0) );
        prevBtn = btn;
        toolbar->addChild(btn);
    }

    {
        btn = (ui::Button*)createButton("  Up  ", (int)ButtonCommand::Up);
        btn->setPosition(prevBtn->getPosition() + Vec2( prevBtn->getContentSize().width + spacing, 0) );
        prevBtn = btn;
        toolbar->addChild(btn);
    }
    
    {
        btn = (ui::Button*)createButton(" Down ", (int)ButtonCommand::Down);
        btn->setPosition(prevBtn->getPosition() + Vec2( prevBtn->getContentSize().width + spacing, 0) );
        prevBtn = btn;
        toolbar->addChild(btn);
    }
    
    {
        btn = (ui::Button*)createButton("Commands", (int)ButtonCommand::Commands);
        btn->setAnchorPoint(Vec2(1,0));
        btn->setPosition(Vec2(tbs.width - spacing - 6, spacing) );
        prevBtn = btn;
        toolbar->addChild(btn);
    }
    
    {
        btn = (ui::Button*)createButton("Inventory", (int)ButtonCommand::Inventory);
        btn->setAnchorPoint(Vec2(1,0));
        btn->setPosition(prevBtn->getPosition() - Vec2( prevBtn->getContentSize().width + spacing, 0) );
        prevBtn = btn;
        toolbar->addChild(btn);
    }
    

    /*
    {
        btn = (ui::Button*)createButton(" go ", (int)ButtonCommand::Go);
        btn->setAnchorPoint(Vec2(1,0));
        btn->setPosition(prevBtn->getPosition() - Vec2( prevBtn->getContentSize().width + spacing, 0) );
        prevBtn = btn;
        toolbar->addChild(btn);
        go = btn;
    }
     */
    
    
    messageFrame = ui::Scale9Sprite::create("frame.png");
    this->addChild(messageFrame);
    messageFrame->setVisible(false);
    messageFrame->setAnchorPoint(Vec2(0,1));
    messageFrame->setPosition( Vec2(10, hudBg->getPosition().y - 4 - hudBg->getContentSize().height ) );
    
    menuFrame = ui::Scale9Sprite::create("frame.png");
    this->addChild(menuFrame);
    menuFrame->setVisible(false);
    menuFrame->setPosition( messageFrame->getPosition() );
    
    keyboard = ui::Scale9Sprite::create("frame.png");
    keyboard->setAnchorPoint(Vec2(0,0));
    this->addChild(keyboard);
    
    hideKeyboard();
}

void GameInterface::updateZoom()
{
    float ff[] = {
        1.0f, 1.5f, 2.0f, 0.5f
    };
    zoomIndex = zoomIndex % (int)(sizeof(ff) / sizeof(float));
    dungeon->positionAndScale(CursesTileMap::MapAlign::Left, CursesTileMap::MapAlign::Top, 2.0f * scaleFactor * ff[zoomIndex]);
    dungeon->centerAt();
    
    scaleAndPositionPathMap(dungeon);
}

void GameInterface::scaleAndPositionPathMap(CursesTileMap *map)
{
    float ss = map->getTileSize().width / pathMap->getTileSize().width;
    pathMap->setScale(map->getScale() * ss);
    pathMap->setPosition(map->getPosition());
    pathMap->setAnchorPoint(map->getAnchorPoint());
}

cocos2d::Label* GameInterface::createLabel(std::string text)
{
    float fontSize = 16 * 2 * scaleFactor;
    auto label = Label::createWithTTF(text, "VeniceClassic.ttf", fontSize);
    label->setAnchorPoint(Vec2(0,0));
//    label->enableOutline(Color4B::BLACK, 2);
    label->getFontAtlas()->setAliasTexParameters();
    return label;
}

void GameInterface::cancelPendingCommand()
{
    std::chrono::milliseconds duration(PUSH_KEY_WAIT);
    pushKey((int)EventKeyboard::KeyCode::KEY_ESCAPE, false, false);
    std::this_thread::sleep_for( duration );
    pushKey((int)EventKeyboard::KeyCode::KEY_SPACE, false, false);
    std::this_thread::sleep_for( duration );
}

ui::Button* GameInterface::createButton(std::string text, int tag)
{
    float fontSize = 16 * 2 * scaleFactor;
    
    auto btn = ui::Button::create("button.png");
    btn->setAnchorPoint(Vec2(0, 0));
    btn->addTouchEventListener([&](Ref* sender, ui::Widget::TouchEventType type) {
        
        switch (type)
        {
            case ui::Widget::TouchEventType::BEGAN:
                break;
            case ui::Widget::TouchEventType::ENDED:
            {
                ui::Button *self = (ui::Button*)sender;
                buttonCallback(self);
            }
                break;
            default:
                break;
        }
    });
    
    btn->ignoreContentAdaptWithSize(true);
    btn->setTitleFontName("VeniceClassic.ttf");
    btn->setTitleFontSize(fontSize);
    
    btn->setScale9Enabled(true);
    btn->setTitleText(text);
    
    Size size = btn->getTitleRenderer()->getContentSize();
    size.width += 20;
    size.height *= 1.2;
    btn->setContentSize(size);
    
    auto sprite = btn->getRendererNormal()->getSprite();
    sprite->getTexture()->setAliasTexParameters();
//    sprite->setOpacity(150);
    
    auto label = btn->getTitleLabel();
    label->getFontAtlas()->setAliasTexParameters();
    label->setColor(Color3B::WHITE);
//    label->setColor(Color3B::BLACK);
    
    btn->getRendererNormal()->setColor(Color3B(0xc0,0xc0,0xc0));
    
    btn->setTag(tag);
    return btn;
}

void GameInterface::buttonCallback(cocos2d::Ref* pSender)
{
    std::chrono::milliseconds duration(PUSH_KEY_WAIT);
    ui::Button *self = (ui::Button*)pSender;

    if (messageFrame->isVisible()) {
        messageFrame->setVisible(false);
        return;
    }
    
    switch(self->getTag()) {
        case (int)ButtonCommand::Exit:
        {
            //Close the cocos2d-x game scene and quit the application
            Director::getInstance()->end();
#if (CC_TARGET_PLATFORM == CC_PLATFORM_IOS)
            exit(0);
#endif
        }
            break;
        case (int)ButtonCommand::IdentifyTrap:
            menuFrame->setVisible(false);
            ::pushKey('^');
            std::this_thread::sleep_for( duration );
            break;
        case (int)ButtonCommand::Identify:
            menuFrame->setVisible(false);
            ::pushKey('/');
            std::this_thread::sleep_for( duration );
            break;
        case (int)ButtonCommand::Help:
            menuFrame->setVisible(false);
            showMessage(HELP_STRING);
            break;
        case (int)ButtonCommand::Version:
            menuFrame->setVisible(false);
            ::pushKey('v');
            std::this_thread::sleep_for( duration );
            break;
        case (int)ButtonCommand::Cancel:
            menuFrame->setVisible(false);
            cancelPendingCommand();
            std::this_thread::sleep_for( duration );
            break;
        case (int)ButtonCommand::Zoom:
            menuFrame->setVisible(false);
            zoomIndex++;
            updateZoom();
            break;
        case (int)ButtonCommand::ToggleGraphics:
        {
            menuFrame->setVisible(false);
            cancelPendingCommand();
            showGraphics = !showGraphics;
         
            if (dungeon->isVisible() && !showGraphics) {
                curses->positionAndScale(CursesTileMap::MapAlign::Center, CursesTileMap::MapAlign::Middle, 0);
                curses->setVisible(true);
                dungeon->setVisible(false);
                messageLabelFrame->setVisible(false);
                messageFrame->setVisible(false);
                hudFrame->setVisible(false);
            }
            
            if (!dungeon->isVisible() && showGraphics) {
                curses->setVisible(false);
                dungeon->setVisible(true);
                messageLabelFrame->setVisible(true);
                hudFrame->setVisible(true);
            }
            

        }
            break;
        case (int)ButtonCommand::Search:
            cancelPendingCommand();
            ::pushKey('s');
            std::this_thread::sleep_for( duration );
            break;
        case (int)ButtonCommand::Rest:
            cancelPendingCommand();
            pushKey((int)EventKeyboard::KeyCode::KEY_PERIOD, false, false);
            std::this_thread::sleep_for( duration );
            break;
        case (int)ButtonCommand::Up:
            cancelPendingCommand();
            pushKey((int)EventKeyboard::KeyCode::KEY_COMMA, true, false);
            std::this_thread::sleep_for( duration );
            break;
        case (int)ButtonCommand::Down:
            cancelPendingCommand();
            pushKey((int)EventKeyboard::KeyCode::KEY_PERIOD, true, false);
            std::this_thread::sleep_for( duration );
            break;
        case (int)ButtonCommand::Map:
            showMap();
            break;
        case (int)ButtonCommand::Inventory:
            cancelPendingCommand();
            showInventory();
            break;
        case (int)ButtonCommand::Commands:
            cancelPendingCommand();
            showCommands();
            break;
        case (int)ButtonCommand::PlayerInfo:
            showPlayerInfo();
            break;
        case (int)ButtonCommand::Quaff:
            showInventorySelect('q');
            break;
        case (int)ButtonCommand::Eat:
            showInventorySelect('e');
            break;
        case (int)ButtonCommand::Zap:
            menuFrame->setVisible(false);
            ::pushKey('z');
            std::this_thread::sleep_for( duration );
            break;
        case (int)ButtonCommand::Throw:
            menuFrame->setVisible(false);
            ::pushKey('t');
            std::this_thread::sleep_for( duration );
            break;
        case (int)ButtonCommand::Fight:
            menuFrame->setVisible(false);
            ::pushKey('f');
            std::this_thread::sleep_for( duration );
            break;
        case (int)ButtonCommand::Move:
            menuFrame->setVisible(false);
            ::pushKey('m');
            std::this_thread::sleep_for( duration );
            break;
        case (int)ButtonCommand::PickUp:
            menuFrame->setVisible(false);
            ::pushKey(',');
            std::this_thread::sleep_for( duration );
            break;
        case (int)ButtonCommand::InputString:
        {
            std::string sz = self->getTitleText();
            consumeStringInput(sz);
        }
            break;
        case (int)ButtonCommand::ReadScroll:
            showInventorySelect('r');
            break;
        case (int)ButtonCommand::WearArmour:
            showInventorySelect('W');
            break;
        case (int)ButtonCommand::WieldWeapon:
            showInventorySelect('w');
            break;
        case (int)ButtonCommand::TakeoffArmour:
            menuFrame->setVisible(false);
            ::pushKey('T');
            std::this_thread::sleep_for( duration );
            break;
        case (int)ButtonCommand::RemoveRing:
            menuFrame->setVisible(false);
            ::pushKey('R');
            std::this_thread::sleep_for( duration );
            break;
        case (int)ButtonCommand::PutOnRing:
            showInventorySelect('P');
            break;
        case (int)ButtonCommand::Discoveries:
            showDiscoveries();
            break;
        case (int)ButtonCommand::DropObject:
            showInventorySelect('d');
            break;
        case (int)ButtonCommand::CallObject:
            showInventorySelect('c');
            break;
        case (int)Go:
            findPath(pressX, pressY);
            break;
        case (int)ButtonCommand::InventorySelect:
        {
            menuFrame->setVisible(false);
            std::string str = self->getTitleText();
            
            size_t p = str.find(")");
            char ch = ' ';
            if (p != std::string::npos) {
                ch = str.c_str()[p-1];
                if (ch >='a' && ch <= 'z') {
                } else {
                    ch = ' ';
                }
            }
            
            ::pushKey(ch);
            std::this_thread::sleep_for( duration );
        }
            break;
        default:
        {
        }
    }
}

void GameInterface::update(float delta)
{
    if (isDeadMode())
        return;
    
//    sleep.update(delta);
    consumePath();
    
    if (!dungeon->isScreenDirty())
        return;
    
    pathMap->setVisible(false);
    
    if (!isDungeon()) {
        return;
    }
    
    // draw HUD
    std::string message = dungeon->getStringAtLine(messageLine);
    std::string stats = dungeon->getStringAtLine(statsLine);
    
    waitingForSpace = (message.find("--More--") != std::string::npos);
    waitingForString = (message.find("call it?") != std::string::npos);
    waitingForIdentify = (message.find("identified?") != std::string::npos);

    if (waitingForString) {
        showKeyboard();
    }
    
    char *data = dungeon->getScreenData();
    dungeon->clearAtLine(messageLine);
    dungeon->clearAtLine(statsLine);
    
    statsLabel->setString(stats);
    
    if (message.length() > 0) {
        messageLabel->setString(message);
        messageLabel->stopAllActions();
        messageLabel->setVisible(true);
        messageLabel->setOpacity(255);
    } else if (messageLabel->getOpacity() == 255) {
        auto f = FadeOut::create(MESSAGE_FADE_DELAY);
        messageLabel->runAction(f);
    }
    
//    CursesTileMap::TileColor tc = dungeon->getScreenColor()[0];
//    statsLabel->setColor(Color3B(tc.r, tc.g, tc.b));
//    messageLabel->setColor(Color3B(tc.r, tc.g, tc.b));
    statsLabel->setColor(Color3B::WHITE);
    messageLabel->setColor(Color3B::WHITE);

    // draw the dungeon
    // modify corner tiles
    dungeon->modifyCornerTiles(data);
    dungeon->modifyWeaponTiles(data);
    dungeon->draw(data);
    dungeon->centerAt();
    dungeon->setVisible(true && showGraphics);
    
    scaleAndPositionPathMap(showGraphics ? dungeon : curses);
    
    if (message.find("Which") != std::string::npos &&
        message.find("(* for list)") != std::string::npos) {
        if (!menuFrame->isVisible()) {
            showInventorySelect(0);
            return;
        }
    }
    
    if ( message.find("hand or right") != std::string::npos ) {
        Command commands[] = {
            { InventorySelect, "Left" },
            { InventorySelect, "Right" },
            { 0, "" },
        };
        showMenu(commands);
        return;
    }
}

void GameInterface::pushKey(int key, bool shiftPressed, bool controlPressed)
{
//    fprintf(stdout,"%c\n", key);
    ::pushKey(GameInterface::mapKey(key, shiftPressed, controlPressed));
}

int GameInterface::mapKey(int k, bool shiftPressed, bool controlPressed)
{
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
            if (shiftPressed)
                return '<';
            return ',';
        case (int)EventKeyboard::KeyCode::KEY_PERIOD:
            if (shiftPressed)
                return '>';
            return '.';
        case (int)EventKeyboard::KeyCode::KEY_SEMICOLON:
            if (shiftPressed)
                return ':';
            return ';';
        case (int)EventKeyboard::KeyCode::KEY_APOSTROPHE:
            return '\'';
        case (int)EventKeyboard::KeyCode::KEY_LEFT_BRACKET:
            if (shiftPressed)
                return '{';
            return '[';
        case (int)EventKeyboard::KeyCode::KEY_RIGHT_BRACKET:
            if (shiftPressed)
                return '}';
            return ']';
        case (int)EventKeyboard::KeyCode::KEY_SLASH:
            if (shiftPressed)
                return '?';
            return '/';
        case (int)EventKeyboard::KeyCode::KEY_BACK_SLASH:
            if (shiftPressed)
                return '|';
            return '\\';
        case (int)EventKeyboard::KeyCode::KEY_MINUS:
            if (shiftPressed)
                return '_';
            return '-';
        case (int)EventKeyboard::KeyCode::KEY_EQUAL:
            if (shiftPressed)
                return '+';
            return '=';
            
    }
    
    // numeric
    if (k >= (int)EventKeyboard::KeyCode::KEY_0 && k <= (int)EventKeyboard::KeyCode::KEY_9) {
        k = k - ((int)EventKeyboard::KeyCode::KEY_1 - '1');
        if (shiftPressed) {
            
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
        if (shiftPressed) {
            k = k - ('a' - 'A');
        }
        if (controlPressed) {
            switch(k) {
                case 'd':
                    k = 4;
                    break;
                case 'w':
                    k = 23;
                    break;
                case 'x':
                    k = 24;
                    break;
                default:
                    break;
            }
        }
        return k;
    }
    
    return -1;
}

bool GameInterface::isDungeon()
{
    char *data = dungeon->getScreenData();
    for(int i=0;i<80*25;i++) {
        if (data[i] == '@') {
            std::string str(data+i+1, 40);
            if (data[i+4] == '@') {
                deadMode();
                return false;
            }
            //fprintf(stdout, "%s\n", str.c_str());
            if (str.find("print") == std::string::npos) {
                return true;
            } else {
                return false;
            }
        }
    }
    return false;
}

void GameInterface::press(float x, float y)
{
    if (menuFrame->isVisible() || waitingForSpace) {
        menuFrame->setVisible(false);
        cancelPendingCommand();
        return;
    }
    
    //    fprintf(stdout, "press: %f %f\n", x, y);
    pressX = x;
    pressY = y;
    findPath(pressX, pressY);
}

void GameInterface::release(float x, float y)
{}


void GameInterface::showPlayerInfo()
{
    menuFrame->setVisible(false);
    
    std::string player;
    
    std::chrono::milliseconds duration(PUSH_KEY_WAIT);
    
    // press ) to query weapon
    pushKey('L', true, false);
    std::this_thread::sleep_for( duration );
    
    player += dungeon->getStringAtLine(messageLine) + "\n";
//    fprintf(stdout, "%s\n", dungeon->getStringAtLine(messageLine).c_str());
    pushKey(';', false, false);
    std::this_thread::sleep_for( duration );
    
    // press ] to query armor
    pushKey('y', false, false);
    std::this_thread::sleep_for( duration );
//    fprintf(stdout, "%s\n", dungeon->getStringAtLine(messageLine).c_str());
    player += dungeon->getStringAtLine(messageLine) + "\n";
    pushKey(';', false, false);
    std::this_thread::sleep_for( duration );
    
    // press = to query rings
    pushKey('Y', false, false);
    std::this_thread::sleep_for( duration );
//    fprintf(stdout, "%s\n", dungeon->getStringAtLine(messageLine).c_str());
    player += dungeon->getStringAtLine(messageLine) + "\n";
    
    pushKey(';', false, false);
    std::this_thread::sleep_for( duration );
//    fprintf(stdout, "%s\n", dungeon->getStringAtLine(messageLine).c_str());
    player += dungeon->getStringAtLine(messageLine);
    pushKey(';', false, false);
    std::this_thread::sleep_for( duration );
    
    showMessage(player);
}

void GameInterface::showDiscoveries()
{
    std::chrono::milliseconds duration(PUSH_KEY_WAIT);
    menuFrame->setVisible(false);
    ::pushKey('D');
    std::this_thread::sleep_for( duration );
    ::pushKey('*');
    std::this_thread::sleep_for( duration );
    
    std::string message;
    for(int i=0;i<25;i++) {
        std::string str = dungeon->getStringAtLine(i);
        if (str.length() > 2) {
            if (message.length() > 0)
                message += "\n";
            message += str;
        }
    }
    
    pushKey((int)EventKeyboard::KeyCode::KEY_SPACE, false, false);
    std::this_thread::sleep_for( duration );
    
    showMessage(message);
}

void GameInterface::showInventory()
{

    queryInventory(inventoryItems);
    
    for(int i=0;;i++) {
        Command *c = &inventoryItems[i];
        if (c->cmd == 0)
            break;
        c->cmd = InventoryItem;
        /*
        std::string str(c->text);
        size_t p = str.find("[");
        if (p != std::string::npos) {
            str = std::string(c->text, p);
            strcpy(c->text, str.c_str());
        }
        p = str.find("(");
        if (p != std::string::npos) {
            str = std::string(c->text, p);
            strcpy(c->text, str.c_str());
        }
        */
    }
    
    showMenu( inventoryItems );
}

char GameInterface::getIconFromString(std::string str)
{
    if (str.find("food") != std::string::npos)
        return ':';
    else if (str.find("ration") != std::string::npos)
        return ':';
    else if (str.find("mold") != std::string::npos)
        return ':';
    else if (str.find("potion") != std::string::npos)
        return '!';
    else if (str.find("scroll") != std::string::npos)
        return '?';
    else if (str.find("wand") != std::string::npos)
        return '/';
    else if (str.find("stick") != std::string::npos)
        return '/';
    else if (str.find("staff") != std::string::npos)
        return '/';
    else if (str.find("sword") != std::string::npos)
        return ')';
    else if (str.find("mail") != std::string::npos)
        return ']';
    else if (str.find("amulet") != std::string::npos)
        return ',';
    else if (str.find("ring") != std::string::npos)
        return '=';
    else if (str.find("bow") != std::string::npos)
        return '4';
    else if (str.find("arrow") != std::string::npos)
        return '5';
    else if (str.find("mace") != std::string::npos)
        return '6';
    else if (str.find("dart") != std::string::npos)
        return '5';
    else if (str.find("spear") != std::string::npos)
        return '8';
    else if (str.find("shiraken") != std::string::npos)
        return '5';

    return 0;
}

void GameInterface::queryInventory(Command *inv)
{
    inv[0].cmd = 0;
    
    std::chrono::milliseconds duration(PUSH_KEY_WAIT);
    
    pushKey((int)EventKeyboard::KeyCode::KEY_SPACE, false, false);
    std::this_thread::sleep_for( duration );
    pushKey((int)EventKeyboard::KeyCode::KEY_ESCAPE, false, false);
    std::this_thread::sleep_for( duration );
    
    pushKey((int)EventKeyboard::KeyCode::KEY_I, false, false);
    std::this_thread::sleep_for( duration );
    
    int ii = 0;
    for(int i=0;i<25;i++) {
        std::string str = dungeon->getStringAtLine(i);
        if (str.length() > 2) {
            char ch = str.c_str()[0];
            bool hasLetter = false;
            
            if (str.find("--More") != std::string::npos) {
                pushKey((int)EventKeyboard::KeyCode::KEY_SPACE, false, false);
                std::this_thread::sleep_for( duration );
                i = 0;
                continue;
            }
            
            size_t p = str.find(")");
            
            if (p != std::string::npos) {
                ch = str.c_str()[p-1];
                if (ch >='a' && ch <= 'z') {
                    hasLetter = true;
                }
            }
            
            if (!hasLetter)
                continue;

            Command *c = &inv[ii++];
            strcpy(c->text, str.c_str());
            c->cmd = ButtonCommand::InventoryItem;
            c->key = ch;
            c->icon = getIconFromString(str);
            
            // add break
            inv[ii].cmd = 0;
            
            if (ii > 30)
                break;
        }
//        fprintf(stdout, "%s\n", dungeon->getStringAtLine(i).c_str());
    }
    
    pushKey(';', false, false);
    std::this_thread::sleep_for( duration );
}

void GameInterface::showCommands()
{
    if (menuFrame->isVisible()) {
        cancelPendingCommand();
        menuFrame->setVisible(false);
        return;
    }
    
    Command commands[32] = {
        { Fight,    "Fight", 0 },
        { Move,     "Move", 0 },
        { Throw,    "Throw", 0 },
        { Zap,      "Zap", 0 },
        { ReadScroll,    "Read scroll", 0 },
        { Quaff,    "Quaff", (int)EventKeyboard::KeyCode::KEY_Q, 0 },
        { Eat,      "Eat", 0 },
        { PickUp,   "Pickup", 0 },
        { WieldWeapon,  "Wield weapon", 0 },
        { WearArmour,   "Wear armour", 0 },
        { TakeoffArmour, "Take off armour", 0 },
        { PutOnRing,     "Put on ring", 0 },
        { RemoveRing,    "Remove ring", 0 },
        { DropObject,    "Drop object", 0 },
        { CallObject,    "Call object", 0 },
        { Discoveries, "Show discoveries", 0 },
        { PlayerInfo, "Show player info", 0 },
        { Map, "Show map", 0 },
        { ToggleGraphics, "Toggle graphics", 0 },
        { Zoom, "Zoom", 0 },
        { Help,    "Help", 0 },
        { Identify, "Identify", 0 },
        { IdentifyTrap, "Identify trap", 0 },
        { Version,    "Version", 0 },
        { Cancel,   "Cancel", 0 },
        { 0, "" }
    };

    showMenu(commands);
}

void GameInterface::showMap()
{
    if (!isDungeon())
        return;
    
    auto winSize = Director::getInstance()->getWinSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();
    
    menuFrame->setVisible(false);
    
    messageFrame->removeAllChildrenWithCleanup(true);
    messageLabel->setString("");
    
    Vec2 min(-1,-1);
    Vec2 max(-1,-1);
    
    Node *mapCentered = Node::create();
    
    int th = dungeon->height();
    int tw = dungeon->width();
    char *data = dungeon->getScreenData();
    for(int r=0;r<th;r++) {
        for(int c=0;c<tw;c++) {
            char ch = data[(r*tw) + c];
            
            // draw room
            Color3B clr = Color3B::BLACK;
            if (ch == '-' || ch == '|') {
                
            } else if (ch == '#' || ch == '+') {
                clr = Color3B(0xc0,0xc0,0xc0);
            } else if (ch == '%') {
                clr = Color3B::GREEN;
            } else if (ch == '@') {
                clr = Color3B::RED;
                
            } else {
                continue;
            }
            
            Sprite *spr = Sprite::create("button.png", Rect(10,10,8,8));
            float sw = spr->getContentSize().width;
            float sh = spr->getContentSize().height;
            Vec2 pos = Vec2(c * sw, (th - r) * sh);
            
            spr->setPosition(pos);
            spr->setColor(clr);
            mapCentered->addChild(spr);
            
            if (min.x > pos.x || min.x == -1)
                min.x = pos.x;
            if (max.x < pos.x || max.x == -1)
                max.x = pos.x;
            if (min.y > pos.y || min.y == -1)
                min.y = pos.y;
            if (max.y < pos.y || max.y == -1)
                max.y = pos.y;
        }
    }

    messageFrame->setContentSize(winSize * 0.6);
    messageFrame->setAnchorPoint(Vec2(0.5,0.5));
    messageFrame->setPosition(Vec2(winSize.width/2,winSize.height/2));
    messageFrame->setVisible(true);

    messageFrame->addChild(mapCentered);

    Vec2 mapSize = max - min;
    Vec2 center = mapSize * 0.5;

    float ss = (messageFrame->getContentSize().width * 0.95) / mapSize.x;
    if (ss > 2.0)
        ss = 2.0;
    mapCentered->setScale(ss);
    
    //mapCentered->setAnchorPoint(Vec2(0.5,0.5));
    mapCentered->setPosition(messageFrame->getContentSize().width/2 - (min.x + center.x) * ss, messageFrame->getContentSize().height/2 - (min.y + center.y) * ss);
    

}

void GameInterface::showMessage(std::string message)
{
    auto winSize = Director::getInstance()->getWinSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();
    
    menuFrame->setVisible(false);
    
    messageFrame->removeAllChildrenWithCleanup(true);
    messageLabel->setString("");
    
    auto label = createLabel(message);
    //    label->disableEffect();
    label->getFontAtlas()->setAliasTexParameters();
    
    label->setColor(Color3B::WHITE);
    messageFrame->addChild(label);
    
    Size size = label->getContentSize();
    label->setColor(Color3B::WHITE);
    size.width += 40;
    size.height += 14;
    label->setAnchorPoint(Vec2(0.5,0.5));
    label->setPosition(size.width/2, size.height/2);
    messageFrame->setContentSize(size);
    messageFrame->setColor(Color3B::GRAY);
    messageFrame->setOpacity(FRAME_OPACITY);
    
    messageFrame->setVisible(showGraphics);
    
    messageFrame->setAnchorPoint(Vec2(0.5,0.5));
    messageFrame->setPosition( Vec2( winSize.width/2, winSize.height/2) );
    
}

void GameInterface::showMenu(Command *commands)
{
    auto winSize = Director::getInstance()->getWinSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();
    
    menuFrame->removeAllChildrenWithCleanup(true);
    
    Size sz(0,0);
    int x = 8;
    int y = 8;
    
    int cmdSize = 0;
    for(int i=0;;i++) {
        Command c = commands[i];
        if (c.cmd == 0)
            break;
        cmdSize++;
    }
    
    int btnHeight = toolbar->getChildren().at(0)->getContentSize().height;
    int cmdBreak = (winSize.height * 0.70) / btnHeight;
    
    TMXLayer *layer = dungeon->getLayer("Background");
    
    int numCommands = 0;
    for(int i=0;;i++) {
        Command c = commands[i];
        if (c.cmd == 0)
            break;
        numCommands++;
    }
    
    int cols = floor(numCommands / cmdBreak) + 1;
    cmdBreak = floor(numCommands / cols) + 1;
    
    for(int i=0;;i++) {
        Command c = commands[i];
        if (c.cmd == 0)
            break;
        
        if ((i % cmdBreak) == 0 && i != 0) {
            x = sz.width;
            y = 8;
        }
        
        std::string text = c.text;
        if (c.icon != 0) {
            text = "     ";
            text += c.text;
        }
        
        auto btn = createButton(text, c.cmd);
//        btn->getTitleLabel()->setColor(Color3B::WHITE);
        menuFrame->addChild(btn);
//      fprintf(stdout, "%s\n", c.text);
        
        Size btnSize = btn->getContentSize();
        if (x + btnSize.width + 10 > sz.width)
            sz.width = x + btnSize.width + 10;
        
        btn->setPosition(Vec2(x + 2,y));
        y += btnSize.height + 8;
        
        if (c.icon != 0) {
            Rect rc = layer->getTileSet()->getRectForGID( dungeon->getGID(c.icon) + 1 );
            Sprite *spr = Sprite::create(layer->getTileSet()->_sourceImage, rc);
            spr->setAnchorPoint(Vec2(0,0.5));
            spr->setPosition(Vec2(0, btn->getTitleLabel()->getContentSize().height/2));
            spr->setScale(scaleFactor * 2);
            btn->getRendererNormal()->setColor(Color3B(0x80,0x80,0x80));
            btn->getTitleLabel()->addChild(spr);
        }
        
        if (sz.height < y)
            sz.height = y;
        
        if (c.cmd == InventoryItem)
            btn->setEnabled(false);
    }
    
    sz.height += 8;
    menuFrame->setContentSize(sz);
    menuFrame->setColor(Color3B::GRAY);
    menuFrame->setOpacity(FRAME_OPACITY);
    
    menuFrame->setVisible(true);
    
    menuFrame->setAnchorPoint(Vec2(0.5,0.5));
    menuFrame->setPosition( Vec2( winSize.width/2, winSize.height/2) );
    
    
    // reposition
    for(int i=0;i<menuFrame->getChildrenCount();i++) {
        auto btn = menuFrame->getChildren().at(i);
        Vec2 pos = btn->getPosition();
        pos.y = sz.height - pos.y - btn->getContentSize().height - 4;
        btn->setPosition(pos);
    }
    
    // rescale
    float maxWidth = winSize.width * 0.95;
    float menuWidth = menuFrame->getContentSize().width;
    if (menuWidth > maxWidth) {
        menuFrame->setScale(maxWidth / menuWidth);
    }
    
    
}

void GameInterface::showInventorySelect(char key)
{
    menuFrame->setVisible(false);
    messageFrame->setVisible(false);
    
    std::chrono::milliseconds duration(PUSH_KEY_WAIT);
    
    if (key == 0) {
        pushKey((int)EventKeyboard::KeyCode::KEY_8, true, false);
        std::this_thread::sleep_for( duration );
    } else {
        pushKey((int)EventKeyboard::KeyCode::KEY_SPACE, false, false);
        std::this_thread::sleep_for( duration );
        pushKey((int)EventKeyboard::KeyCode::KEY_ESCAPE, false, false);
        std::this_thread::sleep_for( duration );

        ::pushKey(key);
        std::this_thread::sleep_for( duration );
        
#ifdef ATROGUE
        std::string what = dungeon->getStringAtLine(messageLine);
        size_t whatIdx = what.find("\"");
        if (whatIdx == std::string::npos) {
            cancelPendingCommand();
            return;
        }
        ::pushKey((int) what.c_str()[whatIdx+1]);
        std::this_thread::sleep_for( duration );
#else
        pushKey((int)EventKeyboard::KeyCode::KEY_8, true, false);
        std::this_thread::sleep_for( duration );
#endif
        
    }
    
    std::string str = dungeon->getStringAtLine(messageLine);
    if (str.find("You") != std::string::npos)
        return;
    
    Command commands[32];
    int cc = 0;
    for(int i=0;i<25;i++) {
        std::string item = dungeon->getStringAtLine(i);
        bool hasLetter = false;
        size_t p = item.find(")");
        
        if (p != std::string::npos) {
            char ch = item.c_str()[p-1];
            if (ch >='a' && ch <= 'z') {
                hasLetter = true;
            }
        }
        if (!hasLetter)
            continue;
        
        
        if (item.length() > 2) {
            commands[cc].cmd = InventorySelect;
            commands[cc].icon = getIconFromString(item);

            /*
            p = item.find("[");
            if (p != std::string::npos) {
                item = std::string(item.c_str(), p);
            }
            p = item.find("(");
            if (p != std::string::npos) {
                item = std::string(item.c_str(), p);
            }
             */
            
            strcpy(commands[cc].text, item.c_str());
            cc++;
            commands[cc].cmd = 0;
            if (item.find("--More--") != std::string::npos)
                break;
        }
    }

    if (cc != 0) {
        pushKey((int)EventKeyboard::KeyCode::KEY_SPACE, false, false);
        std::this_thread::sleep_for( duration );
        showMenu(commands);
    }
}

void GameInterface::deadMode()
{
    if (_dead)
        return;
    
    _dead = true;
    showGraphics = false;

    for(int i=0;i<toolbar->getChildrenCount();i++) {
        toolbar->getChildren().at(i)->setVisible(false);
    }
    menuFrame->setVisible(false);
    messageLabelFrame->setVisible(false);
    hudFrame->setVisible(false);

    auto btn = createButton("  Bye  ", ButtonCommand::Exit);
    btn->setPosition( toolbar->getChildren().at(0)->getPosition() );
    toolbar->addChild(btn);
    
    curses->setVisible(true);
    dungeon->setVisible(false);
    
    curses->positionAndScale(CursesTileMap::MapAlign::Center, CursesTileMap::MapAlign::Middle, 0);
}

void GameInterface::consumePath()
{
//    if (dungeon->getNumberOfRunningActions() > 0)
//        return;
    
    if (path.size() == 0)
        return;

    std::chrono::milliseconds duration(PUSH_KEY_WAIT);
    std::this_thread::sleep_for( duration);
    
    //sleep.sleep(0.15f);
    
    void *n = path.at(0);
    int tx, ty;
    dungeon->NodeToXY(n, &tx, &ty);
    path.erase( path.begin() );
    
    // current
    int x = dungeon->playerLocation.x;
    int y = dungeon->playerLocation.y;
    
    if (x == tx && y == ty)
        return;
    
    if ((abs(x - tx) > 1 || abs(y - ty) > 1)) {
        path.clear();
        return;
    }
    
    if (y == ty) {
        if (x < tx) {
            ::pushKey('l');
        } else {
            ::pushKey('h');
        }
    } else if (x == tx) {
        if (y < ty) {
            ::pushKey('j');
        } else {
            ::pushKey('k');
        }
    } else if (x > tx) {
        if (y < ty) {
            ::pushKey('b');
        } else {
            ::pushKey('y');
        }
    } else if (x < tx) {
        if (y < ty) {
            ::pushKey('n');
        } else {
            ::pushKey('u');
        }
    }
    
    std::this_thread::sleep_for( duration);
}

void GameInterface::findPath(float x, float y)
{
    pathError = 0;
    if (messageFrame->isVisible()) {
        messageFrame->setVisible(false);
        return;
    }
    
    if (menuFrame->isVisible()) {
        menuFrame->setVisible(false);
        cancelPendingCommand();
        return;
    }
    
    if (keyboard->isVisible()) {
        return;
    }
    
    Vec2 vp = Vec2(dungeon->playerLocation.x, dungeon->playerLocation.y);
    
    CursesTileMap *map = showGraphics ? dungeon : curses;
    
    Vec2 n = map->convertToNodeSpace(Vec2(x,y));
    Size ts = map->getTileSize();
    n.x = floor(n.x / ts.width);
    n.y = floor(25 - (n.y / ts.height));
    
    float totalCost = 0;
    micropather::MicroPather pather(dungeon);

//    char message[64];
//    sprintf(message, "%f %f - %f %f\n",
//            dungeon->playerLocation.x, dungeon->playerLocation.y,
//            n.x, n.y);
//    messageLabel->setString(message);

    clearPath();
    
    int result = pather.Solve(dungeon->XYToNode(vp.x, vp.y),
                     dungeon->XYToNode(n.x, n.y), &path, &totalCost);
    
    
    if (result == micropather::MicroPather::SOLVED) {
        
        if (n.equals(lastTarget)) {
            lastTarget = Vec2::ZERO;
            
            if (waitingForIdentify) {
                char ch = dungeon->getCharAt( dungeon->getScreenData(), n.y, n.x);
                ::pushKey(ch);
                std::chrono::milliseconds duration(PUSH_KEY_WAIT);
                std::this_thread::sleep_for( duration );
                waitingForIdentify = false;
            } else {
                return;
            }
        }

        lastTarget = n;
        showPath();
        path.clear();
        
    } else {
        
        Vec2 dd = vp - n;
        path.push_back(dungeon->XYToNode(vp.x, vp.y));
        
        if ((fabs(dd.y) > 2 && fabs(dd.x) < 3) || fabs(dd.y) > 3) {
            if (dd.y > 0) {
                path.push_back( dungeon->XYToNode(vp.x, vp.y - 1));
            } else {
                path.push_back( dungeon->XYToNode(vp.x, vp.y + 1));
            }
        } else if (fabs(dd.x) > 3) {
            if (dd.x > 0) {
                path.push_back( dungeon->XYToNode(vp.x - 1, vp.y));
            } else {
                path.push_back( dungeon->XYToNode(vp.x + 1, vp.y));
            }
        }
        
        if (path.size() > 1) {
            
            int xx, yy;
            dungeon->NodeToXY(path.at(1), &xx, &yy);
            
            n.x = xx;
            n.y = yy;
            
            // don't bother
            char ch = dungeon->getCharAt( dungeon->getScreenData(), n.y, n.x);
            if (ch == '-' || ch == '|' || ch == ' ' || ch == 0 || ch == -1)
                return;
            
            if (n.equals(lastTarget)) {
                lastTarget = Vec2::ZERO;
                return;
            }
            
            lastTarget = n;
            showPath();
        }
        
        path.clear();
    }
}

void GameInterface::showPath()
{
    TMXLayer *layer = pathMap->getLayer("Background");
    
    size_t sz = path.size();
    for(int i=1;i<sz;i++) {
        void *n = path[i];
        int nx, ny;
        dungeon->NodeToXY(n, &nx, &ny);
        auto tile = layer->getTileAt(Vec2(nx, ny));
        layer->setTileGID(1384, Vec2(nx, ny));
        
        tile->setColor(Color3B::YELLOW);
        tile->setOpacity(255);
        if (i+1<sz) {
            tile->setColor(Color3B::WHITE);
            tile->setOpacity(150);
        }
        tile->setVisible(true);
    }
    
    pathMap->setVisible(true);
    scaleAndPositionPathMap(showGraphics ? dungeon : curses);
}

void GameInterface::clearPath()
{
    path.clear();
    TMXLayer *layer = pathMap->getLayer("Background");
    
    // clear
    for(int i=0;i<80;i++) {
        for(int j=0;j<25;j++) {
            if (layer->getTileGIDAt(Vec2(i,j)) != 1) {
                layer->setTileGID(1, Vec2(i,j));
//              auto tile = layer->getTileAt(Vec2(i,j));
//              tile->setVisible(false);
            }
        }
    }
    
    pathMap->setVisible(false);
}

void GameInterface::showKeyboard()
{
    toolbar->setVisible(false);
    keyboard->setVisible(true);
    
    auto winSize = Director::getInstance()->getWinSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();

    char str[] = "!1234567890-=qwertyuiop=asdfghjkl#=_zxcvbnm_= `";
    char strShift[] = "!1234567890-=QWERTYUIOP=ASDFGHJKL#=_ZXCVBNM_= `";
    if (keyboard->getChildren().size() == 0) {
        ui::Button *prevButton = 0;
        Node *n = Node::create();
        keyboard->addChild(n);
        for(int i=0;;i++) {
            char c = str[i];
            
            if (keyboardShift) {
                c = strShift[i];
            }
            
            if (c == '`')
                break;
            if (c == '=') {
                Node *p = n;
                n = Node::create();
                n->setPosition(p->getPosition() + Vec2(0, p->getContentSize().height + 8));
                keyboard->addChild(n);
                prevButton = 0;
                continue;
            }
            std::string cz = " ";
            cz += c;
            cz += " ";
            
            if (c == '#')
                cz = " enter ";
            if (c == '_')
                cz = " shift ";
            if (c == '-')
                cz = " delete ";
            if (c == '!')
                cz = " esc ";
            if (c == ' ')
                cz = "   space   ";
            
            auto btn = createButton(cz, InputString);
            
            if (prevButton != 0) {
                btn->setPosition( prevButton->getPosition() + Vec2(prevButton->getContentSize().width + 8, 0) );
            }
            
            Size sz = btn->getContentSize();
            sz.width += btn->getPosition().x;
            n->setContentSize(sz);
            
            prevButton = btn;
            n->addChild(btn);
            
            sz.height += n->getPosition().y;
            sz.width = winSize.width - 20;
            keyboard->setPosition(Vec2(10,8));
            keyboard->setContentSize(sz);
        }
    }
    
    // shift
    
    // align
    
    for(int i=0;i<keyboard->getChildren().size(); i++) {
        Node *n = keyboard->getChildren().at(i);
        float xx = (winSize.width / 2) - n->getContentSize().width / 2;
        n->setPosition( Vec2(xx, -4 + keyboard->getContentSize().height - ((i + 1) * (n->getContentSize().height + 4))) );
    }
    keyboard->setColor( Color3B::GRAY );
    keyboard->setOpacity(200);
    messageLabelFrame->setPosition(Vec2(origin.x + 10, origin.y + keyboard->getContentSize().height + 12));
}

void GameInterface::hideKeyboard()
{
    keyboard->setVisible(false);
    toolbar->setVisible(true);
    
    auto winSize = Director::getInstance()->getWinSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();
    
//    auto btn = toolbar->getChildren().at(0);
    messageLabelFrame->setPosition(Vec2(origin.x + 10, origin.y + toolbar->getContentSize().height + toolbar->getPosition().y + 6));
}

void GameInterface::consumeStringInput(std::string sz)
{
    std::chrono::milliseconds duration(PUSH_KEY_WAIT);
    sz += "  ";
    
    if (sz.find("esc") != std::string::npos) {
        pushKey((int)EventKeyboard::KeyCode::KEY_ESCAPE, false, false);
        std::this_thread::sleep_for( duration );
        hideKeyboard();
    } else if (sz.find("delete") != std::string::npos) {
        ::pushKey('\b');
        std::this_thread::sleep_for( duration );
        return;
    } else if (sz.find("shift") != std::string::npos) {
        keyboardShift = !keyboardShift;
        keyboard->removeAllChildrenWithCleanup(true);
        showKeyboard();
        return;
    } else  if (sz.find("enter") != std::string::npos) {
        pushKey((int)EventKeyboard::KeyCode::KEY_ENTER, false, false);
        std::this_thread::sleep_for( duration );
        hideKeyboard();
    } else {
        ::pushKey(sz.c_str()[1]);
        std::this_thread::sleep_for( duration );
    }
}
