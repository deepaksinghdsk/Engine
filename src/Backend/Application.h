#include "Context.h"
#include "Pipeline.h"
#include "Layer.h"

class Application
{
private:
    Context context;
    Layer *layer;
    // Pipeline pipeline;

public:
    Application(Layer *layer);
    ~Application();

    void run();
};