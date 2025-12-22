#include "Backend/Application.h"
#include "Backend/Layer.h"

class Example : public Layer
{
private:
public:
    Example()
    {
    }

    virtual void prepare() override
    {
    }

    virtual void onUIRender() override
    {
    }

    virtual void run() override
    {
        /* while(true)
        {

        } */
    }
};

void main(int argc, char **argv)
{
    std::cout << "init" << std::endl;
    Example layer{};
    //Layer* l = &layer;

    /* Example *layer = new Example(); 
    Layer* l = layer; */
    
    Application app(&layer);
    //app.run();
    // app->prepare();
    // app->run();
}