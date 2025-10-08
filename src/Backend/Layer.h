#ifndef LAYER_HEADER
#define LAYER_HEADER

class Layer
{

public:
    virtual void run() {};
    virtual void prepare(){};
    virtual void onUIRender() {};
};

#endif