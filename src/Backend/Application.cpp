#include "Application.h"

Application::Application(Layer* layer) : layer(layer)
{
    std::cout << "init started" << std::endl;
    // Physical and logical device is auto created by context class cons
    context.createSurface();
    context.createSwapchain();
    
    std::cout << "init done successfully" << std::endl;
}

Application::~Application()
{
}

void Application::run()
{
    layer->run();
}