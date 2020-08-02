#pragma once
#include "ParallelUtils.h"
#include <iostream>
#include <string>

int main(){
    mutable Lock access;
    auto lease = access.acquire();
    std::out << "Finish" << std::endl;
}