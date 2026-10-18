#include "connection.hpp"
#include <iostream>
#include <stdexcept>
int main() {
    using namespace sk150;
    auto check=[](ConnectionState actual,ConnectionState expected){if(actual!=expected) throw std::runtime_error("Falscher Verbindungszustand");};
    check(connectionState(false,false,false,false,false,false),ConnectionState::Disconnected);
    check(connectionState(true,false,false,false,false,false),ConnectionState::Connecting);
    check(connectionState(true,false,false,true,true,true),ConnectionState::Connected);
    check(connectionState(true,false,false,true,false,true),ConnectionState::Error);
    check(connectionState(true,false,false,true,true,false),ConnectionState::Error);
    check(connectionState(true,false,true,false,false,false),ConnectionState::Error);
    check(connectionState(true,true,false,true,false,false),ConnectionState::Stopping);
    check(connectionState(true,true,true,true,false,false),ConnectionState::Disconnected);
    std::cout<<"Verbindungszustände einschließlich Fehler und Trennen: OK\n";
}
