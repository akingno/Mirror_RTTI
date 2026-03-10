#ifndef PLAYER_H
#define PLAYER_H

#include "Core/Mirror_Core.h"

reflect_struct Player {
public:
to_reflect:
    int health;
    float speed;

    void Move(float offsetX, float offsetY);

    int getHealth();

no_reflect:
private:
};

inline void Player::Move(float offsetX, float offsetY) {
    std::cout << "Player Move: " << offsetX << ", " << offsetY << "\n";
}

inline int Player::getHealth() {
    return health;
}

#endif //PLAYER_H
