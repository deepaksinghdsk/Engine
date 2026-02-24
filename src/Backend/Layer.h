#ifndef LAYER_HEADER
#define LAYER_HEADER

class Layer
{

public:
    virtual void run(uint32_t imgInd) {};
    virtual void prepare(){};
    virtual void onUIRender() {};
};

#endif