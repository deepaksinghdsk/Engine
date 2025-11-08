#include "Context.h"

class RenderPass
{
public:
    RenderPass(Context &context);
    ~RenderPass();

private:
    VkRenderPass renderPass;
    Context &context;
    
    void init();
};