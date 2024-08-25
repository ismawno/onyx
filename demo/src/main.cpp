#include "onyx/app/app.hpp"

int main()
{
    ONYX::Initialize();

    ONYX::Application2D app;
    app.Run();

    ONYX::Terminate();
}