#include "Bouncer.hpp"

#include <iostream>
using namespace std;

int main( int argc, char **argv ) 
{
    std::string title("Bouncer");
    std::string luaSceneFile("arena.lua");
    if (argc > 1) {
        luaSceneFile = std::string(argv[1]);
    }
    GlWindow::launch(argc, argv, new Bouncer(luaSceneFile), 1024, 768, title);

    return 0;
}
