#include"../headers/random_helper.h"
#include<iostream>
#include<cstdlib>
#include<ctime>
#include<chrono>
#include<random>

int random_helper::Random(int max)
{
    //現在時刻をシードとして使用
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();

    std::srand(static_cast<unsigned int>(seed));
    std::mt19937 gen(seed);
    int random;

    if (max == 0)
        max = RAND_MAX;


    std::uniform_int_distribution<> dis(1, max);
    random = dis(gen);

    return random;
}
