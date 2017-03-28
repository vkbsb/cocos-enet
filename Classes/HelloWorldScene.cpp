#include "HelloWorldScene.h"
#include "LANMultiplayer.h"
#include <unistd.h>

USING_NS_CC;

Scene* HelloWorld::createScene()
{
    // 'scene' is an autorelease object
    auto scene = Scene::create();
    
    // 'layer' is an autorelease object
    auto layer = HelloWorld::create();

    // add layer as a child to scene
    scene->addChild(layer);

    // return the scene
    return scene;
}

// on "init" you need to initialize your instance
bool HelloWorld::init()
{
    //////////////////////////////
    // 1. super init first
    if ( !Layer::init() )
    {
        return false;
    }
    
    CCLOG("%ld --hostid", gethostid());
    char buff[1024] = {0};
    gethostname(buff, sizeof buff);
    CCLOG("%s --hostname",  buff);
    
    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();

    /////////////////////////////
    // 2. add a menu item with "X" image, which is clicked to quit the program
    //    you may modify it.

    // add a "close" icon to exit the progress. it's an autorelease object
    auto closeItem = MenuItemImage::create(
                                           "CloseNormal.png",
                                           "CloseSelected.png",
                                           CC_CALLBACK_1(HelloWorld::menuCloseCallback, this));
    
	closeItem->setPosition(Vec2(origin.x + visibleSize.width - closeItem->getContentSize().width/2 ,
                                origin.y + closeItem->getContentSize().height/2));

    // create menu, it's an autorelease object
    auto menu = Menu::create(closeItem, NULL);
    menu->setPosition(Vec2::ZERO);
    menu->setName("menu");
    this->addChild(menu, 1);
    
    
    auto start_server = MenuItemFont::create("Start Server", CC_CALLBACK_1(HelloWorld::startServer, this));
    start_server->setPosition(visibleSize/2);
    menu->addChild(start_server);
    
    auto lookup = MenuItemFont::create("LookUp Servers", CC_CALLBACK_1(HelloWorld::lookupServer, this));
    lookup->setPosition(start_server->getPosition());
    lookup->setPositionY(lookup->getPositionY()-50);
    menu->addChild(lookup);
    
    
    auto join = MenuItemFont::create("Join Server", CC_CALLBACK_1(HelloWorld::joinServer, this));
    join->setPosition(lookup->getPosition());
    join->setPositionY(lookup->getPositionY()-50);
    menu->addChild(join);
    join->setName("join");
    join->setVisible(false);

    
    auto send_msg = MenuItemFont::create("Send Msg", CC_CALLBACK_1(HelloWorld::sendMessage, this));
    send_msg->setPosition(join->getPosition());
    send_msg->setPositionY(join->getPositionY()-50);
    menu->addChild(send_msg);
    send_msg->setName("send");
    send_msg->setVisible(false);
    
    /////////////////////////////
    // 3. add your codes below...

    // add a label shows "Hello World"
    // create and initialize a label
    
    auto label = Label::createWithTTF("Hello World", "fonts/Marker Felt.ttf", 24);
    
    // position the label on the center of the screen
    label->setPosition(Vec2(origin.x + visibleSize.width/2,
                            origin.y + visibleSize.height - label->getContentSize().height));

    // add the label as a child to this layer
    this->addChild(label, 1);

    // add "HelloWorld" splash screen"
    auto sprite = Sprite::create("HelloWorld.png");

    // position the sprite on the center of the screen
    sprite->setPosition(Vec2(visibleSize.width/2 + origin.x, visibleSize.height/2 + origin.y));

    // add the sprite as a child to this layer
    this->addChild(sprite, 0);
    
    return true;
}

void HelloWorld::sendMessage(Ref *ptr)
{
    LANClient::getInstance()->send_string("Hello World");
}

void HelloWorld::joinServer(Ref *ptr)
{
    if(LANClient::getInstance()->joinServer(0)){
        getChildByName("menu")->getChildByName("send")->setVisible(true);
    }
}

void HelloWorld::serverListFetched(Ref *ptr)
{
    NotificationCenter::getInstance()->removeObserver(this, SERVER_LIST_FETCHED);
    for(auto str : LANClient::getInstance()->server_list){
        CCLOG("Server: %s", str.c_str());
    }
    
    if(LANClient::getInstance()->server_list.size())
        getChildByName("menu")->getChildByName("join")->setVisible(true);
}

void HelloWorld::lookupServer(Ref *ptr)
{
    CCLOG("+HelloWorld::lookupServer");
    NotificationCenter::getInstance()->addObserver(this, callfuncO_selector(HelloWorld::serverListFetched), SERVER_LIST_FETCHED, nullptr);
    
    runAction(Sequence::create(DelayTime::create(2), CallFunc::create([](){
        LANClient::getInstance()->stopLookup();
    }), NULL));
    
    LANClient::getInstance()->startScan();
    CCLOG("-HelloWorld::lookupServer");
}

void HelloWorld::startServer(Ref *ptr)
{
    CCLOG("+HelloWorld::startServer");
    LANServer::getInstance()->startServer();
    CCLOG("-HelloWorld::startServer");
}

void HelloWorld::menuCloseCallback(Ref* pSender)
{
    Director::getInstance()->end();

#if (CC_TARGET_PLATFORM == CC_PLATFORM_IOS)
    exit(0);
#endif
}
