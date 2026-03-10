#include <iostream>

#include "Mirror_RTTI.h"


struct Player {
    int health;
    float speed;
};


BEGIN_CLASS(Player)
    REGISTER_FIELD(health)
    REGISTER_FIELD(speed)
END_CLASS(Player)

int main() {
    // Testing: 通过名字改field
    Player p;
    p.health = 100;

    SetProperty(&p, typeid(Player), "speed", 5.5f);

    float s = 0;
    GetProperty(&p, typeid(Player), "speed", s);

    std::cout << "通过反射读取的speed: " << s << "\n";
    std::string str("Player");

    // Testing：通过类的名字查到类信息
    const ClassDescriptor *desc = ReflectionRegistry::Instance().GetClassByName(str);
    if (desc) {
        std::cout << "成功通过字符串 " << str << "查找到类信息！\n";
        std::cout << "该类包含的字段数: " << desc->fields_map.size() << "\n";
    }

    // Testing: 动态创建对象
    void *objPtr = CreateInstanceByName(str);
    if (objPtr) {
        std::cout << "创建" << str << "成功...\n";

        SetProperty(objPtr, typeid(Player), "health", 999);
        SetProperty(objPtr, typeid(Player), "speed", 3.14f);

        float s2 = 0;
        GetProperty(objPtr, typeid(Player), "speed", s2);
        std::cout << "动态读取的speed: " << s2 << "\n";

        DestroyInstanceByName(objPtr, "Player");
        objPtr = nullptr;
    } else {
        std::cout << "创建失败，未找到该类的注册信息。\n";
    }

    return 0;
}
