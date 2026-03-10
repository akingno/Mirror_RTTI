#include <iostream>
#include "Core/Mirror_RTTI.h"
#include "Player.h"


int main() {
    std::cout << "测试...\n";

    void *objPtr = CreateInstanceByName("Player");

    if (objPtr) {
        std::cout << "成功创建Player\n";

        SetProperty(objPtr, typeid(Player), "health", 999);
        SetProperty(objPtr, typeid(Player), "speed", 3.14f);

        int h = 0;
        float s = 0.0f;
        GetProperty(objPtr, typeid(Player), "health", h);
        GetProperty(objPtr, typeid(Player), "speed", s);

        std::cout << "通过反射读取修改后的值: health = " << h << ", speed = " << s << "\n";

        std::cout << "动态调用函数...\n";
        std::vector<std::any> moveArgs = {10.5f, -2.0f};
        InvokeMethod(objPtr, typeid(Player), "Move", moveArgs);

        std::any result = InvokeMethod(objPtr, typeid(Player), "getHealth");
        if (result.has_value()) {
            std::cout << "通过反射获取到Health: " << std::any_cast<int>(result) << "\n";
        }

        DestroyInstanceByName(objPtr, "Player");
        std::cout << "对象已销毁...\n";
    } else {
        std::cerr << "[Error] 创建失败, 未能找到 Player 类的注册信息。\n";
    }

    return 0;
}
