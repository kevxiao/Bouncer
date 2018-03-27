#include "Bouncer.hpp"

#include <iostream>
using namespace std;

int main( int argc, char **argv ) 
{
    std::string title("Bouncer");
    std::string arenaType;
    if (argc > 1) {
        arenaType = std::string(argv[1]);
    }
    GlWindow::launch(argc, argv, new Bouncer(arenaType == "cube"), 1024, 768, title);

    return 0;
}
