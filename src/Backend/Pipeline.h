#include "Context.h"

class Pipeline
{
public:
    Pipeline(VkDevice device);
    ~Pipeline();

private:
    VkDevice device;

    void init();
    std::vector<char> readfile(char* fileloc);
    VkShaderModule createShaderModule(std::vector<char> code);

};